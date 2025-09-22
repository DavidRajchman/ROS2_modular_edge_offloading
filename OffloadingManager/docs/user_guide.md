# Offloading Manager (OM) User Guide

## Overview

The Offloading Manager (OM) is a central decision-making component in the modular gateway offloading system. It receives offloading requests from Bridge Control Planes and makes resource allocation decisions by assigning tasks to available MEC (Multi-access Edge Computing) components.

## Quick Start

### Prerequisites
- Python 3.8 or higher
- Network connectivity to Discovery Service (default: 192.168.50.114:9090)
- Access to TCP port 8100 for Bridge CP connections

### Installation
```bash
cd /home/ubuntu/OffloadingManager
pip3 install -r requirements.txt
```

### Basic Usage
```bash
# Start with default configuration
python3 main.py

# Start with custom configuration
python3 main.py --config /path/to/config.json --log-level DEBUG

# Auto-start script
./autostart_om.sh
```

## Configuration

### Configuration File (`config.json`)
The OM uses a JSON configuration file that defines available tasks and system parameters:

```json
{
  "available_tasks": [
    {
      "task_id": 1,
      "task_name": "STRING_PROCESSING",
      "input_message_types": [1],
      "output_message_types": [1]
    }
  ],
  "default_session_timeout": 300,
  "max_concurrent_sessions": 100
}
```

### Algorithm Configuration (`algorithm.py`)
The decision-making algorithm can be configured by modifying constants in `algorithm.py`:

- `AUTO_APPROVE_ALL_REQUESTS`: Enable/disable automatic approval
- `USE_RANDOM_MEC_SELECTION`: Random vs. deterministic MEC selection
- `DISCOVERY_QUERY_INTERVAL_SECONDS`: How often to check for available MECs

## System Architecture

### Core Components

1. **OM Server** (`om_server.py`): TCP server handling Bridge CP connections
2. **Decision Engine** (`decision_engine.py`): Resource allocation logic
3. **Algorithm** (`algorithm.py`): Pluggable decision-making algorithm
4. **Discovery Client** (`discovery_client.py`): Interface to Discovery Service
5. **Config Manager** (`config_manager.py`): Configuration handling

### Message Flow
```
VHC → Bridge CP → OM Server → Decision Engine → Algorithm → MEC Assignment
```

## Communication Protocols

### Bridge CP → OM Messages

**OFFLOAD_REQUEST (Code: 100)**
```json
{
  "component_id": "60:5",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "payload": {
    "request_id": 12345,
    "task_id": 67890,
    "mgwcp_component_id": "60:5",
    "vhc_data": "optional_context_data"
  }
}
```

**SESSION_KEEPALIVE (Code: 102)**
```json
{
  "component_id": "60:5",
  "message_code": 102,
  "message_type": "SESSION_KEEPALIVE",
  "payload": {
    "request_id": 12345
  }
}
```

### OM → Bridge CP Responses

**SESSION_APPROVED (Code: 200)**
```json
{
  "component_id": "1:1",
  "message_code": 200,
  "message_type": "SESSION_APPROVED",
  "payload": {
    "request_id": 12345,
    "assigned_mec_id": "50:1",
    "task_id": 67890
  }
}
```

**SESSION_DENIED (Code: 201)**
```json
{
  "component_id": "1:1",
  "message_code": 201,
  "message_type": "SESSION_DENIED",
  "payload": {
    "request_id": 12345,
    "reason_code": 4001,
    "reason_description": "No available resources"
  }
}
```

## VHC Data Feature

The OM supports optional VHC data parameters for context-aware decision making:

### Usage in Requests
VHCs can include contextual data in the `vhc_data` field:
```json
"vhc_data": "priority=high,location=37.7749,-122.4194,app=video_streaming"
```

### Algorithm Integration
The algorithm receives VHC data and can use it for enhanced decisions:
```python
def make_allocation_decision(request_id, task_id, mgwcp_component_id, vhc_data=None):
    if vhc_data and "priority=high" in vhc_data:
        # Handle high priority requests differently
        pass
```

## Monitoring and Debugging

### Log Files
- **Console Output**: Real-time structured logging
- **File Output**: `/home/ubuntu/OffloadingManager/om.log` (persistent)

### Log Levels
- `DEBUG`: Detailed message processing and algorithm decisions
- `INFO`: Request handling and resource allocation (default)
- `WARNING`: Configuration issues and recoverable errors
- `ERROR`: Connection failures and critical errors

### Key Log Messages
```
[2025-09-22 10:30:15.123] [INFO] [server] [_handle_offload_request] - Offload request 12345: task_id=67890, from=60:5
[2025-09-22 10:30:15.125] [INFO] [algorithm] [make_allocation_decision] - APPROVED request 12345: assigned MEC 50:1
[2025-09-22 10:30:15.127] [INFO] [server] [_send_session_approved] - Sent SESSION_APPROVED to connection 1
```

## Common Operations

### Starting the OM
```bash
# Standard startup
python3 main.py --config config.json --log-level INFO

# Debug mode with verbose logging
python3 main.py --log-level DEBUG

# Background execution
nohup python3 main.py > om.out 2>&1 &
```

### Monitoring Resource Allocation
Watch the logs for decision patterns:
```bash
tail -f om.log | grep "allocation_decision"
```

### Checking Discovery Service Connection
```bash
# Look for registration and keepalive messages
grep "discovery" om.log
```

## Troubleshooting

### Common Issues

**OM fails to start**
- Check configuration file syntax: `python3 -m json.tool config.json`
- Verify port 8100 is available: `netstat -tlnp | grep 8100`

**No MECs available**
- Check Discovery Service connection
- Verify MEC components are registered and active
- Review discovery client logs

**Bridge CP connection failures**
- Ensure OM is listening on port 8100
- Check network connectivity between Bridge and OM
- Verify Bridge CP configuration points to correct OM address

**Sessions not being approved**
- Check task_id exists in configuration
- Verify MEC components are available for the task
- Review algorithm decision logic in logs

### Debug Commands
```bash
# Check OM process
ps aux | grep python3 | grep main.py

# Test port connectivity
telnet <om_host> 8100

# Validate configuration
python3 -c "import json; print(json.load(open('config.json')))"

# Check algorithm configuration
python3 -c "from algorithm import *; print(f'Auto-approve: {AUTO_APPROVE_ALL_REQUESTS}')"
```

## Algorithm Customization

### Modifying Decision Logic
Edit `algorithm.py` to customize allocation behavior:

1. **Simple Auto-Approve**: Set `AUTO_APPROVE_ALL_REQUESTS = True`
2. **Custom MEC Selection**: Implement logic in `_sophisticated_strategy()`
3. **VHC Data Processing**: Parse `vhc_data` parameter for context-aware decisions

### Example Customization
```python
def make_allocation_decision(request_id, task_id, mgwcp_component_id, vhc_data=None):
    # Parse VHC context data
    if vhc_data and "priority=emergency" in vhc_data:
        # Emergency requests get dedicated MEC
        return AllocationDecision(approved=True, assigned_mec_id="emergency_mec")
    
    # Regular allocation logic
    return _auto_approve_strategy(request_id, task_id, mgwcp_component_id)
```

## Configuration Reference

### Required Configuration Fields
- `available_tasks`: Array of task definitions
- `default_session_timeout`: Session timeout in seconds
- `max_concurrent_sessions`: Maximum concurrent sessions

### Task Definition
```json
{
  "task_id": 1,
  "task_name": "TASK_NAME",
  "input_message_types": [1, 2, 3],
  "output_message_types": [4, 5, 6]
}
```

### Algorithm Constants
Located in `algorithm.py`:
- `ALGORITHM_NAME`: Algorithm identifier
- `AUTO_APPROVE_ALL_REQUESTS`: Approval behavior
- `USE_RANDOM_MEC_SELECTION`: MEC selection strategy
- `DISCOVERY_QUERY_INTERVAL_SECONDS`: Discovery update frequency

## Integration Guidelines

### Adding New MECs
1. Register MEC components with Discovery Service
2. OM will automatically discover and include them in allocation decisions
3. No OM restart required

### Adding New Tasks
1. Update `config.json` with new task definition
2. Restart OM to load new configuration
3. Verify task appears in algorithm decision logic

### Bridge CP Integration
- OM listens on port 8100 for Bridge CP connections
- Bridge CP should be configured with OM host/port
- No authentication required (trusted network assumed)

This user guide provides the essential information for deploying, configuring, and troubleshooting the Offloading Manager in the modular gateway system.