<!-- filepath: /home/ubuntu/OffloadingManager/overview.md -->
# Offloading Manager (OM) Implementation Guide

## Table of Contents

0. [Naming Convention](#0-naming-convention)
1. [Introduction and System Overview](#1-introduction-and-system-overview)
2. [OM Component Architecture](#2-om-component-architecture)
3. [Communication Protocols](#3-communication-protocols)
4. [Session Management and Decision Making](#4-session-management-and-decision-making)
5. [Resource Management](#5-resource-management)
6. [Global Configuration Distribution](#6-global-configuration-distribution)
7. [Discovery Service Integration](#7-discovery-service-integration)
8. [Error Handling and Recovery](#8-error-handling-and-recovery)
9. [Implementation Details](#9-implementation-details)
10. [File Structure and Responsibilities](#10-file-structure-and-responsibilities)

---

### 0. NAMING CONVENTION
*   **component** - a single part of the offloading system, such as a VHC or OM.
*   **component_id** - a unique identifier for a component, created from group_id, id_in_group. Formated as `group_id:id_in_group` (e.g., `10:12`). Group IDs have meaning in the context of the offloading system, such as a vehicle or a mobile device with either manualy asigned or automaticaly asigned ids in group.
*  **VHC** - short for Vehicle. It is a physical (or simulated) vehicle system that can request tasks to be offloaded using the Modular Gateway.
*  **MEC** - a component containing virtual copy of the VHC that processes ofloaded tasks. MEC can have only one vehicle asigned to it. VHC can have multiple MEC assigned to it, each with its own tasks
*  **OM** - short for Offloading Manager. It is a single component that manages the offloading network. It contains an algorithm that decides which tasks are offloaded to which VHCs and MECs. And can potentialy deny offloading request if there arent enough resources. Does not process or route any data.
*  **Bridge** - a component that routes data between VHC, MEC and OM. VHC and MEC maintain active connection only to Bridge and DiscoveryService. offloading requests are sent to the BridgeCP which then forwards them to the OM. The BridgeCP is a control plane of the Bridge. If offloading request is approved by the OM, the BridgeCP constructs a bridgeDP path that will forward the messages.
*  **DiscoveryService** - a service that allows VHCs and MECs to discover the Bridge and register themselves. It is a control plane of the offloading system. It is the only part of the offloading system with a fixed IP address and/or DNS name. It distributes the global configuration JSON to all components at the start of the offloading experiment. A library for encoding and decoding the DiscoveryService protocol is provided in the repository.
* **DP** - short for Data Plane. this short is used to refer to the data transfer layer of components. such as VHC, Bridge, MEC.
* **CP** - short for Control Plane. this short is used to refer to the component layers which are responsible for managing the offloading requests setting up the DP paths. Managing the lifecycle of the offloading tasks and components. Such as VHC, Bridge, MEC. It also comunicates with the DiscoveryService to register the component and discover the Bridge.
* **MGW** - short for Modular Gateway. It is a system that allows VHCs and MECs to offload tasks to the Bridge and OM. It implements a DP [finished] and a CP [to be implemented]. It is a ROS 2 node that is running on VHC and MEC. 
* **task** - a type of computation that can be offloaded it is a list of ros2 topics that act as an data input and output for the computation. It is identified by a task_id and a human readable task_name. At the start of the offloading experiment a Global configuration JSON is sent by OM to DiscoveryService which distributes it throughout the system. 
* **request** - a request to offload a single task. It is initiated by the VHC and sent to the BridgeCP. if aproved by the OM, the BridgeCP will construct a DP path that will forward the messages between VHC and MEC. The request is identified by a request_id and a component_id. The request is sent to the BridgeCP as a JSON message. It needs to be maintained by periodic keepalive messages from the VHC. If they timeout, the bridgeCP will teardown the DP path and notify the VHC that the request was denied. Timeout of the request is a valid way to end the offloading session. 
* **session** - a single offloaded task that is being processed by the VHC and MEC. It is identified by a the request_id and VHC component_id. also refered to as offloading session. There is no explicit session id used in the comunication.   


## 1. Introduction and System Overview

### 1.1 Purpose and Role Definition

The **Offloading Manager (OM)** is the central decision-making component in the modular gateway offloading system. According to the system specifications found in `/home/ubuntu/bridge/docs/bridgeCP.md:30`, the OM:

> *"is a single component that manages the offloading network. It contains an algorithm that decides which tasks are offloaded to which VHCs and MECs. And can potentially deny offloading request if there aren't enough resources. Does not process or route any data."*

Since this entire project is used for research of offloading algorithms, the application will be coded in a way to allow easy modification of the decision-making algorithms and resource management strategies. For this reason the OM will be writen in Python.

It will also gather information from other sources other than bridgeCP (TBD) to get more data to base its decisions on. Example sources could be:
- Real-time traffic data on the 5G network
- Physical MEC server usage statistics
- other sources

**IMPORTANT**: Everything that is configurable about the OM must be done in the algorithm.py file. The OM must not have any hardcoded values that are not configurable in the algorithm.py file. This is to allow easy modification of the decision-making algorithms and resource management strategies. And the algorithm.py file must be the only file that needs to be modified to change the behavior of the OM. The rest of the code must remain unchanged.

### 1.2 System Position and Communication Model

The OM operates as a centralized control authority that communicates exclusively with the Bridge Control Plane (Bridge CP). It also registers with the discovery service and provides the global configuration The communication model is:

```
[VHC/MEC MGWCP] <--> [Bridge CP] <--> [OM] <-- [Other data sources] 
                          |            / 
                    [Discovery Service]
```

**Key Design Principles** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:52`):
- **Decision Authority**: OM has sole authority over offloading approval/denial decisions
- **Resource Management**: OM tracks and allocates system resources (VHCs, MECs, computational capacity)
- **No Data Processing**: OM does not handle actual data transmission or processing
- **Centralized Control**: Single point of coordination for the entire offloading network
- **Discovery Service Integration**: OM registers with the Discovery Service to provide global configuration and be discoverable by bridges

### 1.3 Network Connection Details

**Connection Configuration** (source: `/home/ubuntu/bridge/src/BridgeControlPlane.hpp:25`, `/home/ubuntu/bridge/src/BridgeControlPlane.cpp:280`):
- **Host**: `variable` (IP address is stored in discovery service and provided to Bridge CP)
- **Port**: `8100` (defined as `OM_PORT`)
- **Protocol**: TCP
- **Connection Type**: Server (OM listens, Bridge CP connects)

---

## 2. OM Component Architecture

### 2.1 Core Components

Based on analysis of Bridge CP interactions, the OM must implement:

**Message Handler**: Process JSON control plane protocol messages from Bridge CP
**Decision Engine**: Algorithm to approve/deny offload requests based on available resources, it must be possible to change this algorithm without modifying rest of the code - algorithm should be behind an abstraction layer
**Resource Tracker**: Maintain real-time state of all VHCs and MECs in the system via Discovery Service API
**Session Manager**: Track active offloading sessions and their state
**Configuration Manager**: Load and provide static global configuration JSON file
**Discovery Service Client**: Handle registration and component discovery

### 2.2 Component Registration and Discovery

**Component Types** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:26-32`):
- **VHC (Vehicle)**: Physical or simulated vehicle requesting task offloading
- **MEC**: Virtual copy of VHC that processes offloaded tasks  
- **Bridge**: Routes data between VHC, MEC, and OM
- **OM**: The offloading manager itself (component_id: `1:1`, subtype: `0`)

**Discovery Service Integration**:
- OM must register with Discovery Service using text-based protocol
- OM provides static global configuration JSON to Discovery Service for distribution
- Discovery Service has API/mechanism that returns all connected components (VHCs, MECs)
- Discovery Service distributes configuration "throughout the system" at component connection

### 2.3 Global Configuration Structure

**Configuration JSON Format** (static file, no runtime changes):
```json
{
  "available_tasks": [
    {
      "task_id": 31,
      "task_name": "MAP_ROUTING", 
      "input_message_types": [201, 203],
      "output_message_types": [202, 204]
    },
    {
      "task_id": 42,
      "task_name": "OBJECT_DETECTION",
      "input_message_types": [205], 
      "output_message_types": [206, 207]
    }
  ],
  "default_session_timeout": 300,
  "max_concurrent_sessions": 100
}
```

---

## 3. Communication Protocols

### 3.1 JSON Message Protocol Structure

**Universal Message Envelope** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:566-574`):
```json
{
  "component_id": "1:1",
  "message_code": 200,
  "message_type": "SESSION_APPROVED",
  "sequence_number": 43,
  "payload": {
    "request_id": 12345
  }
}
```

**Required Fields**:
- `component_id`: OM's component identifier (always "1:1")
- `message_code`: Numeric message type identifier
- `message_type`: Human-readable message type string
- `sequence_number`: Unique, monotonically increasing per connection
- `payload`: Message-specific data object

### 3.2 Messages FROM Bridge CP to OM

**OFFLOAD_REQUEST (100)** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:380-390`):
```json
{
  "component_id": "1:5",
  "message_code": 100, 
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 15,
  "payload": {
    "request_id": 12345,
    "task_id": 31,
    "mgwcp_component_id": "1:5"
  }
}
```

**SESSION_KEEPALIVE (102)** (source: Bridge CP forwards these from VHC/MEC):
- Used to maintain active sessions
- Must be processed to prevent session timeout
- Requires response to maintain session active state

**BRIDGE_DP_FAILURE (301)** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:486-495`):
```json
{
  "component_id": "0:1", 
  "message_code": 301,
  "message_type": "BRIDGE_DP_FAILURE",
  "sequence_number": 46,
  "payload": {
    "request_id": 12345,
    "reason": "connection_failed"
  }
}
```

### 3.3 Messages FROM OM to Bridge CP

**SESSION_APPROVED (200)** (simplified - only MEC ID needed):
```json
{
  "component_id": "1:1",
  "message_code": 200,
  "message_type": "SESSION_APPROVED", 
  "sequence_number": 43,
  "payload": {
    "request_id": 12345,
    "assigned_mec_id": "2:3"
  }
}
```

**SESSION_DENIED (201)** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:452-464`):
```json
{
  "component_id": "1:1",
  "message_code": 201,
  "message_type": "SESSION_DENIED",
  "sequence_number": 44, 
  "payload": {
    "request_id": 12345,
    "reason_code": 4001,
    "reason_description": "No available MEC resources for the requested task"
  }
}
```

### 3.4 Reliability and ACK Mechanism

**ACK Message (900)** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:502-512`):
```json
{
  "component_id": "1:1",
  "message_code": 900,
  "message_type": "ACK", 
  "sequence_number": 45,
  "payload": {
    "ack_sequence_number": 15
  }
}
```

**Reliability Requirements**:
- OM must send ACK immediately upon receiving any message
- OM must expect ACK for messages it sends
- Bridge CP implements hop-by-hop reliability (source: `/home/ubuntu/bridge/docs/bridgeCP.md:55`)

---

## 4. Session Management and Decision Making

### 4.1 Session Lifecycle

**Session States**:
1. **Request Received**: OFFLOAD_REQUEST received from Bridge CP
2. **Decision Pending**: OM processing resource allocation decision
3. **Approved**: SESSION_APPROVED sent, waiting for data plane establishment
4. **Active**: Data plane operational, session running
5. **Terminated**: Session ended (timeout, completion, or failure)

### 4.2 Decision Algorithm Requirements

**Resource Allocation Factors**:
- Available MEC capacity for requested task type (from Discovery Service API)
- Current system load and concurrent sessions
- Task compatibility between VHC and available MECs  
- Network topology and latency considerations

**Decision Outcomes** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:30`):
- **APPROVE**: Assign specific MEC to handle VHC's task
- **DENY**: Insufficient resources or incompatible task requirements

**MEC Assignment Logic** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:29`):
> *"MEC can have only one vehicle assigned to it. VHC can have multiple MEC assigned to it, each with its own tasks"*

### 4.3 Session Timeout Management

**Timeout Requirements** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:584-586`):
- **Keepalive Interval**: 15 seconds (MGWCP sends SESSION_KEEPALIVE)
- **Session Timeout**: 20 seconds (5 seconds more than keepalive interval)
- **Timeout Handling**: "Terminate session and notify OM"

**Timeout Processing** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:37`):
> *"It needs to be maintained by periodic keepalive messages from the VHC. If they timeout, the bridgeCP will teardown the DP path and notify the VHC that the request was denied. Timeout of the request is a valid way to end the offloading session."*

---

## 5. Resource Management

### 5.1 Component Tracking

**VHC Management**:
- Track active VHCs via Discovery Service component queries
- Monitor VHC connection status via Bridge CP
- Maintain VHC-to-MEC assignment mappings

**MEC Management**:
- Track available MECs via Discovery Service component queries
- Monitor MEC computational load and availability
- Enforce one-vehicle-per-MEC constraint

**Task Management** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:35`):
> *"a type of computation that can be offloaded it is a list of ros2 topics that act as an data input and output for the computation. It is identified by a task_id and a human readable task_name."*

### 5.2 Resource Allocation Algorithms

**Constraint Satisfaction**:
- MEC availability for specific task types
- One-to-one VHC-MEC assignment per session
- Maximum concurrent sessions limit (configurable)
- Resource capacity limits per MEC

**Load Balancing**:
- Distribute load across available MECs
- Consider geographic or network proximity
- Optimize for system-wide performance

**Algorithm Abstraction**:
- Simple auto-approve placeholder algorithm for initial implementation
- Decision engine behind abstraction layer for easy algorithm swapping
- All algorithm decisions must be logged for research purposes

### 5.3 Failure Handling

**Data Plane Failures** (source: `/home/ubuntu/bridge/src/BridgeControlPlane.cpp:285-290`):
- Bridge CP sends BRIDGE_DP_FAILURE when data plane connections fail
- OM must deallocate resources and update MEC availability
- May need to reassign sessions to alternative MECs

**Bridge Redundancy**:
- Support multiple concurrent Bridge connections for VHC network switching scenarios
- Maintain session state across bridge transitions
- Handle bridge failover gracefully

---

## 6. Global Configuration Distribution

### 6.1 Configuration Generation

**Static Configuration Management**:
- Load configuration from static JSON file at startup
- No runtime modifications to configuration
- Configuration remains constant during OM lifetime
- Configuration file path should be configurable via command line argument

**Distribution Mechanism** (source: `/home/ubuntu/bridge/docs/bridgeCP.md:32`):
> *"At the start of the offloading experiment a Global configuration JSON is sent by OM to DiscoveryService which distributes it throughout the system."*

### 6.2 Configuration Parameters

**System Limits**:
- `max_concurrent_sessions`: Global limit on active sessions
- `default_session_timeout`: Default timeout for sessions (300 seconds)

**Task Definitions**: 
- `available_tasks`: Array of task configurations
- Each task has `task_id`, `task_name`, `input_message_types`, `output_message_types`

---

## 7. Discovery Service Integration

### 7.1 Discovery Protocol Overview

**Protocol Format** (source: analysis of protocol.hpp and protocol.cpp):
- **Transport**: TCP connection with text-based protocol
- **Message Format**: Semicolon-delimited fields: `DISC:<TYPE>;<field1>;<field2>;...;<last_field>`
- **Component Types**: V (Vehicle), M (MEC), B (Bridge), O (Offload Manager), T (Test)

### 7.2 OM Registration Process

**Registration Request Format**:
```
DISC:REG;O;S;1;1;OffloadManager;0.0.0.0;8100;{global_config_json}
```

**Field Breakdown**:
- `DISC:REG`: Protocol header for registration
- `O`: Component type (Offload Manager)
- `S`: Static ID request type
- `1`: Group ID (always 1 for OM)
- `1`: ID in group (always 1 for OM)
- `OffloadManager`: Component name
- `0.0.0.0`: Listen address (Discovery Service will detect actual IP)
- `8100`: Listen port
- `{global_config_json}`: Global configuration in humanReadableMessage field

**Registration Response Format**:
```
DISC:ACK;0;1;1;;;;;{detected_ip};Registration successful
```

**Field Breakdown**:
- `DISC:ACK`: Acknowledgment header
- `0`: Response code (0 = Success)
- `1;1`: Assigned group ID and ID in group
- `;;;;`: Empty connection target fields (not needed for OM)
- `{detected_ip}`: IP address detected by Discovery Service
- `Registration successful`: Human readable message

### 7.3 Component Discovery Mechanism

**Required New Message Type** (to be implemented in OM first, then Discovery Service):
- **Query Request**: `DISC:QRY;component_type_filter;HumanReadableMessage`
- **Query Response**: `DISC:LST;component_count;component1_data;component2_data;...`

**Component Data Format**:
- Format: `component_type:group_id:id_in_group:component_subtype`
- Example: `V:10:12:1` (Vehicle with group ID 10, ID in group 12, subtype 1)
- Example: `M:20:5:0` (MEC with group ID 20, ID in group 5, no subtype)
- **Subtypes**: Used to differentiate between component capabilities within the same type (e.g., GPU-equipped MEC vs CPU-only MEC)
- **OM Subtype**: Always 0 for backward compatibility

**Query Implementation Details**:
- **Single Component Type**: Only one type per query (e.g., "M" for MECs, "V" for VHCs)
- **OM Queries**: Primarily for MECs, VHCs for research data (current single-bridge version only)
- **Query Timing**: Periodic queries every few seconds (configurable in algorithm.py) plus on-demand capability

### 7.4 Keepalive Protocol

**Keepalive Ping**:
```
DISC:PNG;1.1;OK;Keepalive from OM
```

**Keepalive Response**:
```
DISC:PON;OK;5000;Continue operation
```

**Keepalive Requirements**:
- OM must send keepalive every 5 seconds
- Discovery Service timeout is 15 seconds
- Failed keepalives result in component deregistration

### 7.5 Connection and Startup Sequence

**Startup Order**:
1. Logging system initialization
2. Configuration loading and validation
3. **Discovery Service registration** (must succeed)
4. TCP server startup (only after successful registration)

**Registration Dependency**: TCP server exposes OM only after successful Discovery Service registration to ensure Bridge CP can discover OM location.

### 7.6 Python Implementation Requirements

Since Python cannot use the C++ discovery protocol library, the OM must implement:

**Protocol Encoder/Decoder**:
- Parse semicolon-delimited messages
- Validate message structure and field counts
- Handle component type and response code enumerations

**TCP Client for Discovery Service**:
- Connect to Discovery Service (default: 192.168.65.5:9090)
- Handle connection failures and retries
- Maintain persistent connection for keepalives

**Component State Tracking**:
- Cache component information from queries
- Update component status based on Discovery Service responses
- Trigger resource allocation updates when components change
- Periodic refresh mechanism with configurable intervals
- On-demand query capability for future MEC creation events

---

## 8. Error Handling and Recovery

### 8.1 Error Handling Philosophy

The OM is research-oriented and prioritizes simplicity, thus logs are required for all error conditions, and the system should not attempt to recover from critical errors automatically. Instead, it should log detailed diagnostics and shut down gracefully (if possible) to allow researchers to analyze the failure.

**Shutdown on Critical Errors**: All critical errors should result in OM shutdown with detailed logging of shutdown reasons. The system prioritizes diagnostics over automatic recovery for research purposes.

**Error Categories**:
- **Configuration Errors**: Invalid config file, missing required fields
- **Network Errors**: Connection failures, protocol errors
- **Discovery Service Errors**: Registration failures, component query failures
- **Bridge Communication Errors**: Message parsing failures, sequence errors
- **Resource Management Errors**: Algorithm failures, constraint violations

### 8.2 Critical Error Scenarios

**Discovery Service Connection Failure**:
- Retry connection attempts with exponential backoff
- If all retries fail, log detailed error and shutdown OM
- Cannot operate without Discovery Service registration

**Bridge CP Communication Failure**:
- Log connection attempts and failures
- If Bridge disconnects, continue operation but log degraded state
- If message parsing fails, log message content and shutdown

**Configuration Loading Failure**:
- Log detailed file path and parsing errors
- Cannot start without valid configuration
- Shutdown immediately with clear error message

### 8.3 Logging Requirements

**Structured Logging Format**:
```
[TIMESTAMP] [LEVEL] [COMPONENT] [FUNCTION] - MESSAGE
```

**Required Log Information**:
- Startup sequence with configuration details
- All Discovery Service interactions (registration, queries, keepalives)
- All Bridge CP message exchanges with full message content
- Decision algorithm choices and reasoning
- Resource allocation changes
- Error details with full context and stack traces
- Shutdown reasons with detailed error analysis

### 8.4 Graceful Shutdown Sequence

**Shutdown Triggers**:
- Critical error conditions
- SIGINT/SIGTERM signals
- Discovery Service connection loss
- Unrecoverable Bridge communication errors

**Shutdown Process**:
1. Log shutdown initiation with reason
2. Stop accepting new offload requests
3. Complete or deny pending requests
4. Log final shutdown completion
5. (all connections timeout)

---

## 9. Implementation Details

### 9.1 Architecture Overview

The OM is implemented as a multi-threaded Python application designed for research flexibility and comprehensive logging. The architecture separates concerns to allow easy modification of decision algorithms without affecting core infrastructure.

### 9.2 Threading Model

**Main Thread**: Application lifecycle management, signal handling, and shutdown coordination
**TCP Server Thread**: Handles Bridge CP connections and message processing
**Discovery Service Thread**: Manages Discovery Service communication and keepalives
**Decision Engine Thread**: Processes offload requests and makes allocation decisions (optional async processing)

### 9.3 Algorithm Research Framework

**Pluggable Design**: The decision algorithm is completely separated from the core OM infrastructure. Researchers can modify algorithm behavior by editing a single file containing configuration variables and decision logic.

**Algorithm Configuration Variables**: All algorithm parameters are defined as module-level constants that can be easily modified:
- Auto-approval mode toggle
- MEC selection strategies (random, first-available, least-loaded)
- Load balancing parameters
- Discovery Service query intervals
- Future research extensions (latency optimization, task affinity)

**Algorithm Statistics**: The algorithm tracks comprehensive statistics for research analysis including decision counts, approval rates, assignment patterns, and timing metrics.

### 9.4 Resource State Management

**Thread-Safe Operations**: All resource state (available MECs, active sessions, MEC assignments) is protected by threading locks to ensure consistency across concurrent operations.

**Component Discovery Integration**: The OM maintains real-time awareness of system components through Discovery Service queries and keepalive monitoring. Component state changes trigger resource allocation updates.

**Session Lifecycle Tracking**: Each offloading session is tracked from initial request through completion or failure, with full audit trails for research analysis.

### 9.5 Reliability Mechanisms

**Message Acknowledgments**: All control plane messages implement hop-by-hop reliability with immediate ACK responses and sequence number tracking.

**Connection Recovery**: The OM handles Bridge CP connection failures gracefully, maintaining session state during brief disconnections and logging degraded operations.

**Discovery Service Resilience**: Connection to the Discovery Service is maintained through automatic reconnection attempts and exponential backoff on failures.

---

## 10. File Structure and Responsibilities

### 10.1 Core Application Files

**main.py**: Application entry point and lifecycle management
- Command-line argument parsing (config file path, log level)
- Signal handler registration for graceful shutdown
- Main application coordination and shutdown sequencing
- Logging system initialization and configuration

**om_server.py**: TCP server for Bridge CP connections
- Multi-threaded TCP server listening on port 8100
- JSON message parsing and validation
- Bridge CP connection state management
- Message routing between Bridge CP and decision engine
- ACK mechanism implementation for hop-by-hop reliability

**decision_engine.py**: Core resource allocation logic
- Session state management and tracking
- Resource constraint validation (task compatibility, session limits)
- Integration with pluggable algorithm framework
- MEC assignment and session lifecycle management
- Thread-safe operations for concurrent access

**algorithm.py**: Pluggable decision algorithm (research modification point)
- Algorithm configuration constants (auto-approval, MEC selection strategy)
- Discovery Service query interval configuration
- Decision-making logic implementation
- Statistics collection for research analysis
- Easy modification interface for algorithm research

### 10.2 Configuration and Management

**config_manager.py**: Configuration loading and validation
- JSON configuration file parsing and validation
- Task configuration structure validation
- Configuration parameter access methods
- Global configuration JSON string generation for Discovery Service

**logger_config.py**: Logging system configuration
- Structured logging format for research diagnostics
- Multi-level logging (console and file output)
- Component-based logger naming (OM.server, OM.decision, etc.)
- Timestamp formatting and log level management

### 10.3 Discovery Service Integration

**discovery_client.py**: Discovery Service communication
- Text-based protocol encoder/decoder for Discovery Service messages
- Component registration and keepalive management
- Component discovery and state tracking (MECs, VHCs)
- Connection failure handling and retry logic
- Periodic and on-demand component queries
- Thread-safe resource state updates

### 10.4 Configuration Files

**config.json**: Global system configuration
- Available task definitions with input/output message types
- System limits (max concurrent sessions, default timeouts)
- Task compatibility information for routing decisions
- Static configuration that doesn't change during runtime

**om.log**: Runtime log file
- Comprehensive operational logging for debugging and research
- All Discovery Service interactions and Bridge CP communications
- Decision algorithm choices and resource allocation changes
- Error conditions and shutdown sequences

### 10.5 Implementation Characteristics

**Research-Oriented Design**: The entire implementation prioritizes ease of modification for algorithm research over complex optimization. The algorithm file is designed to be the only file researchers need to modify.

**Comprehensive Logging**: Every significant operation is logged with structured format for post-experiment analysis. All message exchanges are logged with full content for debugging.

**Error Transparency**: Rather than attempting automatic recovery, the system provides detailed diagnostic information and graceful shutdown on errors to aid in debugging and system understanding.

**Thread Safety**: All shared state is properly protected for multi-threaded operation while maintaining simple, readable code structure.

**Configuration Driven**: All operational parameters are externalized to configuration files or algorithm constants, avoiding hard-coded values in the core infrastructure.

This specification provides the foundation for implementing an OM that is fully compatible with the Bridge Control Plane and the broader modular gateway offloading system, with emphasis on research-oriented logging and diagnostics.