# Offloading Manager (OM) User Guide

## Overview

The Offloading Manager (OM) is a central decision-making component in the modular gateway offloading system. It receives offloading requests from Bridge Control Planes and makes resource allocation decisions by assigning tasks to available MEC 

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
This global configuration is distributed to the entire offloading system via Discovery Service.

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
Algorithm file is the only python source file that should be modified while doing research. It contains all configuration variables and also the algorithm logic itself. Currently a placeholder algorithm is implemented that can be modified to implement custom logic.

## System Architecture

### Core Components

1. **OM Server** (`om_server.py`): TCP server handling Bridge CP connections
2. **Decision Engine** (`decision_engine.py`): Resource allocation funcstions that algorithm calls
3. **Algorithm** (`algorithm.py`): Pluggable decision-making algorithm
4. **Discovery Client** (`discovery_client.py`): Interface to Discovery Service
5. **Config Manager** (`config_manager.py`): Global configuration handling
6. **Logger** (`logger.py`): Structured logging system
7. **External data source handler** (`external_data_sources.py`): Module for fetching data over HTTP using request - response mechanism. Can be configured to automatically refresh data periodically.
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

The OM supports optional VHC data parameters for context-aware decision making which are included in offload requests.

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


### Task Definition
```json
{
  "task_id": 1,
  "task_name": "TASK_NAME",
  "input_message_types": [1, 2, 3],
  "output_message_types": [4, 5, 6]
}
```

