#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Any
import re

try:
    import yaml  # type: ignore
except ImportError:
    yaml = None

STATE_FILE = Path(__file__).parent / "state.json"

class StateStore:
    def __init__(self, path: Path):
        self.path = path
        if not self.path.exists():
            self._write({"experiments": []})

    def _read(self) -> Dict[str, Any]:
        with self.path.open("r", encoding="utf-8") as f:
            return json.load(f)

    def _write(self, data: Dict[str, Any]):
        tmp = self.path.with_suffix('.tmp')
        with tmp.open("w", encoding="utf-8") as f:
            json.dump(data, f, indent=2, sort_keys=True)
        tmp.replace(self.path)

    def record_start(self, exp_meta: Dict[str, Any]):
        data = self._read()
        data.setdefault("experiments", []).append(exp_meta)
        self._write(data)


def run(cmd: List[str], env: Dict[str, str]):
    print(f"[launcher] $ {' '.join(cmd)}")
    result = subprocess.run(cmd, env=env)
    if result.returncode != 0:
        print(f"[launcher] Command failed with code {result.returncode}", file=sys.stderr)
        sys.exit(result.returncode)


def load_descriptor(path: Path) -> Dict[str, Any]:
    if not path.exists():
        print(f"Descriptor file not found: {path}", file=sys.stderr)
        sys.exit(1)
    if path.suffix in (".yaml", ".yml"):
        if yaml is None:
            print("PyYAML not installed. Install with: pip install pyyaml", file=sys.stderr)
            sys.exit(1)
        with path.open("r", encoding="utf-8") as f:
            return yaml.safe_load(f)
    elif path.suffix == ".json":
        with path.open("r", encoding="utf-8") as f:
            return json.load(f)
    else:
        print("Unsupported descriptor format. Use .yaml/.yml/.json", file=sys.stderr)
        sys.exit(1)


def build_compose_command(base: str, overrides: List[str], project: str, services: List[str]) -> List[str]:
    cmd = ["docker", "compose", "-p", project, "-f", base]
    for o in overrides:
        cmd.extend(["-f", o])
    cmd.append("up")
    cmd.append("-d")
    if services:
        cmd.extend(services)
    return cmd


def merge_env(global_env: Dict[str, str]) -> Dict[str, str]:
    new_env = os.environ.copy()
    new_env.update(global_env or {})
    return new_env


def phased_launch(descriptor: Dict[str, Any], only: List[str] = None, dry_run: bool = False):
    compose_cfg = descriptor.get("compose", {})
    base = compose_cfg.get("base_file")
    overrides = compose_cfg.get("overrides", [])
    raw_project = descriptor.get("project_name") or descriptor.get("experiment_id") or "exp"
    # Sanitize project name to docker compose rules: lowercase alnum, hyphen, underscore
    sanitized = re.sub(r"[^a-z0-9_-]", "", raw_project.lower())
    if not sanitized or not re.match(r"^[a-z0-9][a-z0-9_-]*$", sanitized):
        # Fallback prefix if first char invalid or empty after cleaning
        sanitized = f"exp_{sanitized}" if sanitized else "exp_auto"
    project = sanitized
    if project != raw_project:
        print(f"[launcher] Adjusted project name '{raw_project}' -> '{project}' to satisfy docker naming rules")
    phases = descriptor.get("phases", [])
    global_env = descriptor.get("metadata", {}).get("env", {})
    env = merge_env(global_env)

    if not base:
        print("Missing compose.base_file in descriptor", file=sys.stderr)
        sys.exit(1)

    base_path = str((Path(__file__).parent / base).resolve()) if not os.path.isabs(base) else base
    override_paths = [str((Path(__file__).parent / o).resolve()) if not os.path.isabs(o) else o for o in overrides]

    print(f"[launcher] Using compose base: {base_path}")
    print(f"[launcher] Using overrides: {override_paths}")
    print(f"[launcher] Project name: {project}")

    for phase in phases:
        name = phase.get("name")
        if only and name not in only:
            continue
        services = phase.get("services", [])
        delay_after = phase.get("delay_after", 0)
        print(f"[launcher] Phase '{name}' -> services: {services} (delay_after={delay_after}s)")
        cmd = build_compose_command(base_path, override_paths, project, services)
        if dry_run:
            print("[dry-run]", " ".join(cmd))
        else:
            run(cmd, env)
        if delay_after and not dry_run:
            time.sleep(delay_after)


def launch_exp_1(descriptor_path: Path, only: List[str] = None, dry_run: bool = False):
    descriptor = load_descriptor(descriptor_path)
    phased_launch(descriptor, only=only, dry_run=dry_run)
    # Record after successful launch of last requested phase
    state = StateStore(STATE_FILE)
    meta = {
        "experiment_id": descriptor.get("experiment_id"),
        "project_name": descriptor.get("project_name"),
        "started_at": datetime.utcnow().isoformat() + "Z",
        "phases": [p.get("name") for p in descriptor.get("phases", [])],
        "ros_domain_id": descriptor.get("metadata", {}).get("ros_domain_id"),
        "env": descriptor.get("metadata", {}).get("env", {}),
    }
    state.record_start(meta)
    print(f"[launcher] Recorded experiment start: {meta['experiment_id']}")


def parse_args():
    ap = argparse.ArgumentParser(description="Experiment launcher with phased delayed startup")
    ap.add_argument("descriptor", type=str, help="Path to experiment YAML/JSON descriptor")
    ap.add_argument("--only", nargs="*", help="Launch only these phase names")
    ap.add_argument("--dry-run", action="store_true", help="Print docker compose commands without executing")
    return ap.parse_args()


def main():
    args = parse_args()
    descriptor_path = Path(args.descriptor).resolve()
    launch_exp_1(descriptor_path, only=args.only, dry_run=args.dry_run)

if __name__ == "__main__":
    main()
