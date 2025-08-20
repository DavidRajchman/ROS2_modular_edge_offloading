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

## Example use

To launch with the required delays (OM + DiscoveryService → 1s → Bridge → 1s → MEC → 15s → VHC) you can use the orchestrator we added.

Steps:
1. Use descriptor exp_delayed.yaml (I just created it) which encodes the exact phase order and delays.
2. Run the launcher; it will execute docker compose up -d for only the services in each phase, then sleep the specified delay_before_next (delay_after in file).

Run (from repo root or cd orchestrator first):
```
cd orchestrator
python3 launcher.py exp_delayed.yaml
```

What happens internally:
- Phase core: launches discovery_service and offloading_manager, then sleeps 1s.
- Phase bridge: launches bridge, then sleeps 1s.
- Phase mec: launches ros2_mec, then sleeps 15s.
- Phase vhc: launches ros2_vhc, then records experiment in state.json.

If you just want to preview commands:
```
python3 launcher.py exp_delayed.yaml --dry-run
```

If you wanted to tweak delays, edit `delay_after` values in `exp_delayed.yaml`.

Manual alternative (without launcher) using your current docker-compose.yml:
```
docker compose -p expDelayed up -d discovery_service offloading_manager
sleep 1
docker compose -p expDelayed up -d bridge
sleep 1
docker compose -p expDelayed up -d ros2_mec
sleep 15
docker compose -p expDelayed up -d ros2_vhc
```
(Adjust -f flags if you need overrides: add `-f docker-compose.yml -f docker-compose.expA.override.yml` before `up`.)

