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
9. [Implementation Requirements](#9-implementation-requirements)

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
- **OM**: The offloading manager itself (component_id: `1:1`)

**Discovery Service Integration**:
- OM must register with Discovery Service using text-based protocol
- OM provides static global configuration JSON to Discovery Service for distribution
- Discovery Service has API/mechanism that returns all connected components (VHCs, MECs, Bridges)
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

**Current Challenge**: The existing discovery protocol does not have a message type to query all connected components.

**Required New Message Type** (to be implemented):
- **Query Request**: `DISC:QRY;component_type_filter;status_filter`
- **Query Response**: `DISC:LST;component_count;component1_data;component2_data;...`

**Component Data Format** (proposed):
- `component_type:group_id:id_in_group:listen_address:listen_port:status`
- Example: `M:2:5:192.168.1.100:8080:ACTIVE`

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

### 7.5 Python Implementation Requirements

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

---

## 8. Error Handling and Recovery

### 8.1 Error Handling Philosophy

**Shutdown on Critical Errors**: All critical errors should result in graceful OM shutdown with detailed logging of shutdown reasons. The system prioritizes diagnostics over automatic recovery for research purposes.

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
4. Save session state for post-mortem analysis
5. Log final shutdown completion
6. (all connections will timeout automatically)

---

## 9. Implementation Requirements

### 9.1 Core Server Components

**TCP Server**: 
- Listen on `localhost:8100`
- Accept connections from Bridge CP
- Handle multiple concurrent Bridge connections (for VHC network switching scenarios)

**JSON Message Parser**:
- Parse incoming JSON messages
- Validate message structure and required fields
- Generate appropriate error responses

**Decision Engine**:
- Implement resource allocation algorithms behind abstraction layer
- Process OFFLOAD_REQUEST messages
- Generate SESSION_APPROVED/DENIED responses
- Simple auto-approve placeholder for initial implementation

### 9.2 State Management

**Session Tracking**:
- Maintain active session registry by request_id
- Track VHC-MEC assignments
- Implement session timeout detection

**Resource Database**:
- Query Discovery Service for real-time component availability
- Task type compatibility matrix from static configuration
- System capacity monitoring

### 9.3 Integration Points

**Discovery Service Client**:
- Register OM component with Discovery Service
- Provide static global configuration JSON to Discovery Service
- Implement component discovery mechanism (new protocol messages needed)
- Maintain keepalive heartbeat

**Configuration Management**:
- Load static JSON configuration from file (path configurable via CLI)
- Validate configuration structure and required fields
- No runtime configuration changes

**Logging and Monitoring**:
- Structured logging for decision audit trails
- Performance metrics collection
- System health monitoring
- Detailed error logging for research analysis

### 9.4 Python-Specific Implementation Details

**Discovery Protocol Implementation**:
- Custom Python protocol encoder/decoder
- TCP socket management for Discovery Service connection
- Component type and response code enumerations
- Message validation and error handling

**Async/Threading Architecture**:
- Separate threads for Bridge CP server and Discovery Service client
- Thread-safe resource state management
- Graceful shutdown coordination across threads

**Error Handling Framework**:
- Centralized error logging with context
- Graceful shutdown on critical errors
- Exception handling with detailed stack traces

---

**Implementation Priority Order**:
1. Basic TCP server and JSON message handling (auto-approve algorithm)
2. Static configuration loading and validation
3. Discovery Service registration and Python protocol implementation
4. Session state management and timeouts
5. Component discovery mechanism (requires protocol extension)
6. Advanced resource optimization algorithms
7. Multiple bridge connection handling
8. Comprehensive error handling and graceful shutdown

This specification provides the foundation for implementing an OM that is fully compatible with the Bridge Control Plane and the broader modular gateway offloading system, with emphasis on research-oriented logging and diagnostics.