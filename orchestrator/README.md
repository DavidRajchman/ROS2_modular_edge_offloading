Orchestrator Prototype
======================

Purpose: Phase 1 lightweight launcher to start experiment services in timed phases using docker compose project names and override files.

Files:
- `experiment_example.yaml` descriptor with phases, compose files, env.
- `launcher.py` orchestrator script (function `launch_exp_1`).
- `state.json` simple state log (appended after successful launch).
- `../docker-compose.expA.override.yml` override for experiment A.

Descriptor Fields:
- `experiment_id`: Unique identifier.
- `project_name`: Docker compose project (-p value).
- `phases[]`: Ordered list; each phase has `name`, `services[]`, `delay_after` seconds.
- `compose.base_file`: Path to base compose file.
- `compose.overrides[]`: List of override compose files.
- `metadata.env`: Extra environment variables exported to docker compose commands.

Usage:
```
cd orchestrator
python3 launcher.py experiment_example.yaml
```

Dry run (show commands only):
```
python3 launcher.py experiment_example.yaml --dry-run
```

Launch only specific phases (e.g., just core and bridge):
```
python3 launcher.py experiment_example.yaml --only core bridge
```

State Log:
Appends an object containing experiment_id, project_name, started_at, phases, ros_domain_id, env to `state.json`.

Adding New Experiment:
1. Create new override file (e.g. `docker-compose.expB.override.yml`).
2. Copy `experiment_example.yaml` to `expB.yaml` and adjust ids, overrides, phases.
3. Launch with `python3 launcher.py expB.yaml`.

Extending:
- Add stop/destroy command: `docker compose -p <project> down -v`.
- Record per-phase timestamps.
- SQLite migration for richer state (runs, metrics).
