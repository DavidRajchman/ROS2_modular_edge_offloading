# Bridge Control Plane Implementation Guide

## Table of Contents

1. [Introduction and System Overview](#1-introduction-and-system-overview)
2. [MGWCP Connection and Startup Behavior](#2-mgwcp-connection-and-startup-behavior)
3. [Control Plane Protocol Specifications](#3-control-plane-protocol-specifications)
4. [Message Flow Patterns and Reliability](#4-message-flow-patterns-and-reliability)
5. [Session Lifecycle Management](#5-session-lifecycle-management)
6. [Data Plane Coordination](#6-data-plane-coordination)
7. [Error Handling and Recovery](#7-error-handling-and-recovery)
8. [Bridge Role in VHC-OM Communication](#8-bridge-role-in-vhc-om-communication)

**Appendices:**
- [Appendix A: Complete Message Reference](#appendix-a-complete-message-reference)
- [Appendix B: MGWCP State Machine Behavior](#appendix-b-mgwcp-state-machine-behavior)
- [Appendix C: Timing and Timeout Requirements](#appendix-c-timing-and-timeout-requirements)
- [Appendix D: Protocol Compliance Checklist](#appendix-d-protocol-compliance-checklist)

---

## 1. Introduction and System Overview

### 1.1 Purpose and Scope

This document provides complete specifications for implementing a Bridge Control Plane (Bridge CP) that is compatible with the Modular Gateway Control Plane (MGWCP). The Bridge CP acts as a message relay and data plane coordinator in the offloading system.

This guide focuses on the protocols, behaviors, and interaction patterns that the MGWCP expects from the Bridge CP. It does not prescribe specific implementation approaches for the Bridge but rather defines the contract that must be fulfilled.

### 1.2 Bridge CP Role Definition

The Bridge Control Plane serves two primary functions:

**Message Relay**: Provides 100% reliable message forwarding between VHC/MEC components and the Offloading Manager (OM). The Bridge does not make offloading decisions - it forwards requests to the OM and returns responses to the appropriate MGWCP.

**Data Plane Coordinator**: Establishes and manages data plane connections between VHC/MEC components and their assigned counterparts based on OM instructions.

### 1.3 System Communication Model

```
[MGWCP] <--- Control Plane ---> [Bridge CP] <--- Control Plane ---> [OM]
   |                                |                                 |
[MGW DP] <--- Data Plane -----> [Bridge DP] <--- Data Plane ---> [Assigned DP]
```

The Bridge CP coordinates both control plane message flow and data plane connection establishment.

### 1.4 Critical Design Principles

**Reliable Message Delivery**: The Bridge CP MUST implement hop-by-hop reliability between itself and MGWCP instances using the ACK mechanism.

**Transparent Forwarding**: Messages between MGWCP and OM should be forwarded with minimal modification (future header additions noted as TODO).

**Session State Tracking**: The Bridge CP MUST maintain session state to properly route responses and manage data plane connections.

**Data Plane Health Monitoring**: The Bridge CP MUST monitor data plane connections and report failures to the OM.

---

## 2. MGWCP Connection and Startup Behavior

### 2.1 MGWCP Startup Sequence

The MGWCP follows a strict state-driven startup sequence that the Bridge CP must accommodate:

**Phase 1: Discovery and Registration**
1. MGWCP registers with DiscoveryService and receives Bridge CP address
2. MGWCP starts its data plane listening on a configured port
3. MGWCP transitions to CONNECTING_TO_BRIDGE state

**Phase 2: Control Plane Connection**
1. MGWCP connects to Bridge CP on the discovered address
2. MGWCP immediately sends DP_INFO message with its data plane details
3. MGWCP transitions to WAITING_FOR_DP_CONNECTION state

**Phase 3: Data Plane Handshake**
1. Bridge DP connects to MGWCP data plane using DP_INFO details
2. Bridge CP sends DP_CONNECTION_CONFIRMED to MGWCP
3. MGWCP transitions to OPERATIONAL state and begins accepting offloading requests

### 2.2 MGWCP Connection Expectations

**Immediate DP_INFO**: The MGWCP sends DP_INFO as its first message after connecting. The Bridge CP MUST be prepared to receive this immediately.

**Blocking State Transitions**: The MGWCP will not proceed to the next state until it receives the expected response. Missing or delayed responses will cause the MGWCP to remain in a waiting state.

**Component Identification**: Each MGWCP identifies itself with a component_id in format "group_id:id_in_group" (e.g., "1:5"). This identifier is consistent across all messages from that MGWCP.

### 2.3 Connection Management Requirements

**Multi-MGWCP Support**: The Bridge CP MUST accept multiple concurrent MGWCP connections, each with independent state and message sequences.

**Connection Recovery**: If an MGWCP disconnects, the Bridge CP MUST clean up associated sessions and notify the OM of the disconnection.

**Authentication**: Currently, no authentication is required. MGWCPs connect directly using the address provided by DiscoveryService.

---

## 3. Control Plane Protocol Specifications

### 3.1 Protocol Foundation

**Transport**: TCP connections with JSON message payload
**Encoding**: UTF-8 JSON strings
**Message Termination**: Implementation-specific (length-prefixed or delimiter-based)

### 3.2 Universal Message Structure

Every control plane message MUST conform to this JSON envelope:

```json
{
  "component_id": "group_id:id_in_group",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 102,
  "payload": { /* message-specific content */ }
}
```

**Field Requirements:**
- `component_id`: MUST be present and identify the sending component
- `message_code`: MUST be a valid code from the message reference
- `message_type`: MUST correspond to the message_code for validation
- `sequence_number`: MUST be unique and monotonically increasing per connection
- `payload`: MUST be a JSON object (may be empty `{}`)

### 3.3 Message Code Categories

**MGWCP to Bridge (100-199)**:
- 100: OFFLOAD_REQUEST
- 101: SESSION_KEEPALIVE  
- 102: SESSION_TERMINATE_REQUEST
- 103: DP_INFO

**Bridge to MGWCP (200-299)**:
- 200: SESSION_APPROVED
- 201: SESSION_DENIED
- 202: DP_CONNECTION_CONFIRMED

**Bridge to OM (300-399)**:
- 301: BRIDGE_DP_FAILURE

**Reliability (900-999)**:
- 900: ACK

### 3.4 Critical Message Behaviors

**DP_INFO (103)**: Always the first message from MGWCP after connection. Contains actual IP address and port where MGWCP data plane is listening.

**OFFLOAD_REQUEST (100)**: Contains request_id, task_id, and task_name. The request_id is generated by MGWCP and must be preserved in all related communications.

**SESSION_KEEPALIVE (101)**: Sent every 15 seconds by MGWCP for active sessions. Bridge CP should forward to OM and expect a 20-second timeout.

**ACK (900)**: Must be sent immediately upon receiving any message from MGWCP. Contains ack_sequence_number matching the received message's sequence_number.

---

## 4. Message Flow Patterns and Reliability

### 4.1 Reliability Mechanism Implementation

The MGWCP-Bridge link implements hop-by-hop reliability while the Bridge-OM link is considered stable:

**MGWCP to Bridge Flow:**
1. MGWCP sends message with sequence_number
2. Bridge immediately sends ACK with ack_sequence_number
3. Bridge forwards original message to OM
4. If MGWCP doesn't receive ACK within timeout, it retries

**Bridge to MGWCP Flow:**
1. Bridge receives message from OM for specific MGWCP
2. Bridge assigns new sequence_number for that MGWCP connection
3. Bridge sends message to MGWCP and starts timeout timer
4. MGWCP sends ACK immediately upon receipt
5. Bridge removes message from pending retries

### 4.2 Sequence Number Management

**Per-Connection Sequences**: Each MGWCP connection has independent sequence number space starting from 1.

**Monotonic Increment**: Sequence numbers must increase by 1 for each new message sent.

**ACK Matching**: The ack_sequence_number in ACK payload must exactly match the sequence_number being acknowledged.

**Retry Behavior**: Unacknowledged messages should be retried with the same sequence_number.

### 4.3 Message Ordering and Delivery

**Ordering Guarantee**: Messages must be processed in the order received per connection.

**Duplicate Detection**: Bridge CP should handle duplicate messages gracefully (MGWCP may retry on timeout).

**ACK Timing**: ACKs must be sent immediately upon message receipt, before any processing or forwarding.

---

## 5. Session Lifecycle Management

### 5.1 Session Creation Flow

```
1. MGWCP generates unique request_id
2. MGWCP sends OFFLOAD_REQUEST(100) -> Bridge forwards to OM
3. OM makes offloading decision
4. OM sends SESSION_APPROVED(200) or SESSION_DENIED(201) -> Bridge forwards to MGWCP
5. If approved: Bridge initiates data plane connections
```

### 5.2 Session State Tracking Requirements

The Bridge CP MUST maintain session state containing at minimum:

**Session Identification**: request_id (from MGWCP) and component_id (from MGWCP)
**Assignment Information**: Assigned MEC component details (from OM response)
**Data Plane Status**: Connection establishment and health status
**Timing Information**: Last keepalive timestamp for timeout detection

### 5.3 Session Maintenance

**Keepalive Forwarding**: Bridge CP MUST forward SESSION_KEEPALIVE messages from MGWCP to OM and responses back to MGWCP.

**Timeout Detection**: If keepalives stop arriving from MGWCP within 20 seconds, Bridge CP should terminate the session and notify OM.

**Explicit Termination**: MGWCP can send SESSION_TERMINATE_REQUEST which Bridge CP forwards to OM.

### 5.4 Session Cleanup

**MGWCP Disconnection**: All sessions for disconnected MGWCP must be terminated and OM notified.

**Data Plane Failure**: If data plane connection fails, Bridge CP sends BRIDGE_DP_FAILURE to OM and terminates session.

**OM-Initiated Termination**: OM can send termination messages which Bridge CP forwards to appropriate MGWCP.

---

## 6. Data Plane Coordination

### 6.1 Data Plane Connection Sequence

**Initial Connection (DP_INFO Processing)**:
1. MGWCP sends DP_INFO with dp_host and dp_port
2. Bridge DP attempts connection to MGWCP DP at specified address
3. Upon successful connection, Bridge CP sends DP_CONNECTION_CONFIRMED
4. MGWCP transitions to OPERATIONAL state

**Session-Based Connections (After SESSION_APPROVED)**:
1. OM provides MEC assignment information in SESSION_APPROVED payload
2. Bridge DP connects to assigned MEC data plane
3. Bridge DP establishes bidirectional forwarding between MGWCP and MEC
4. Data begins flowing for the offloaded task

### 6.2 Data Plane Address Handling

**DP_INFO Processing**: The dp_host field will contain the actual IP address (not "0.0.0.0"). Bridge DP must connect to this exact address and port.

**Address Validation**: Bridge CP should validate that DP_INFO contains routable addresses before attempting connection.

**Connection Timeout**: Bridge DP should have reasonable timeouts for data plane connection attempts.

### 6.3 Data Plane Health Monitoring

**Connection Monitoring**: Bridge DP must continuously monitor both MGWCP-Bridge and Bridge-MEC data plane connections.

**Failure Detection**: Socket errors, unexpected disconnections, or connection timeouts must be detected promptly.

**Error Reporting**: Data plane failures must be reported to OM using BRIDGE_DP_FAILURE message with appropriate reason string.

### 6.4 Data Forwarding Requirements

**Transparent Forwarding**: Data packets should be forwarded between MGWCP and MEC without modification.

**Ordering Preservation**: Packet ordering must be maintained during forwarding.

**Performance**: Data forwarding should introduce minimal latency and maintain high throughput.

---

## 7. Error Handling and Recovery

### 7.1 Connection Error Handling

**MGWCP Connection Failures**:
- Failed connections: Log error and wait for retry
- Unexpected disconnections: Clean up sessions and notify OM
- Protocol violations: Log error and close connection

**Data Plane Connection Failures**:
- Initial connection timeout: Send error to MGWCP, do not send DP_CONNECTION_CONFIRMED
- Session connection failure: Send BRIDGE_DP_FAILURE to OM
- Runtime connection loss: Send BRIDGE_DP_FAILURE to OM and terminate session

### 7.2 Message Processing Errors

**Invalid JSON**: Log error and close connection
**Missing Required Fields**: Log error and close connection  
**Invalid Message Codes**: Log error and close connection
**Sequence Number Issues**: Log warning but continue processing

### 7.3 Recovery Strategies

**OM Connection Loss**: Queue messages for MGWCPs and attempt OM reconnection
**Partial System Failure**: Continue operating with available components
**Resource Exhaustion**: Deny new sessions gracefully with appropriate error codes

### 7.4 Error Reporting Requirements

**Structured Logging**: All errors should be logged with component identification and context
**OM Notification**: System-level errors affecting multiple sessions should be reported to OM
**MGWCP Notification**: Session-specific errors should be communicated to relevant MGWCP

---

## 8. Bridge Role in VHC-OM Communication

### 8.1 Message Forwarding Principles

**Transparent Relay**: Bridge CP acts as a transparent message relay between MGWCP and OM. The OM should receive messages that appear to come directly from the MGWCP (with future header additions for routing).

**State-Aware Routing**: Bridge CP uses component_id and request_id to route messages to the correct MGWCP connection.

**Reliability Mediation**: Bridge CP provides reliability guarantees on the unstable MGWCP-Bridge link while relying on stable Bridge-OM connectivity.

### 8.2 Request-Response Correlation

**Request Tracking**: Bridge CP must correlate OM responses with the originating MGWCP using request_id and component_id.

**Response Routing**: SESSION_APPROVED/DENIED messages from OM must be routed to the correct MGWCP connection that made the original request.

**Keepalive Routing**: SESSION_KEEPALIVE messages must be forwarded to OM and responses routed back to the originating MGWCP.

### 8.3 Session Context Preservation

**Request Context**: When forwarding OFFLOAD_REQUEST to OM, Bridge CP must preserve all original payload information.

**Response Processing**: Bridge CP may inspect OM responses for internal actions (e.g., data plane setup) but must forward complete responses to MGWCP.

**State Synchronization**: Bridge CP session state must remain synchronized with both MGWCP expectations and OM decisions.

### 8.4 Future Protocol Extensions

**Header Addition**: Future protocol versions will add routing headers to messages. Bridge CP implementation should accommodate this extension.

**TODO Note**: Message header additions for routing will be designed later and must be implemented when specified.

---

## Appendix A: Complete Message Reference

### A.1 MGWCP to Bridge Messages

**OFFLOAD_REQUEST (100)**
```json
{
  "component_id": "1:5",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 15,
  "payload": {
    "request_id": 12345,
    "task_id": 31,
    "task_name": "MAP_ROUTING"
  }
}
```

**SESSION_KEEPALIVE (101)**
```json
{
  "component_id": "1:5",
  "message_code": 101,
  "message_type": "SESSION_KEEPALIVE", 
  "sequence_number": 16,
  "payload": {
    "request_id": 12345
  }
}
```

**SESSION_TERMINATE_REQUEST (102)**
```json
{
  "component_id": "1:5",
  "message_code": 102,
  "message_type": "SESSION_TERMINATE_REQUEST",
  "sequence_number": 17,
  "payload": {
    "request_id": 12345
  }
}
```

**DP_INFO (103)**
```json
{
  "component_id": "1:5",
  "message_code": 103,
  "message_type": "DP_INFO",
  "sequence_number": 2,
  "payload": {
    "dp_host": "192.168.1.10",
    "dp_port": 7401
  }
}
```

### A.2 Bridge to MGWCP Messages

**SESSION_APPROVED (200)**
```json
{
  "component_id": "0:1",
  "message_code": 200,
  "message_type": "SESSION_APPROVED",
  "sequence_number": 43,
  "payload": {
    "request_id": 12345
  }
}
```

**SESSION_DENIED (201)**
```json
{
  "component_id": "0:1",
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

**DP_CONNECTION_CONFIRMED (202)**
```json
{
  "component_id": "0:1",
  "message_code": 202,
  "message_type": "DP_CONNECTION_CONFIRMED",
  "sequence_number": 3,
  "payload": {}
}
```

### A.3 Bridge to OM Messages

**BRIDGE_DP_FAILURE (301)**
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

### A.4 Universal Reliability Message

**ACK (900)**
```json
{
  "component_id": "sender_component_id",
  "message_code": 900,
  "message_type": "ACK",
  "sequence_number": 45,
  "payload": {
    "ack_sequence_number": 15
  }
}
```

---

## Appendix B: MGWCP State Machine Behavior

### B.1 MGWCP State Transitions

The MGWCP operates according to this state machine that the Bridge CP must accommodate:

**INITIALIZING → DISCOVERING**
- MGWCP starts data plane listening
- MGWCP contacts DiscoveryService

**DISCOVERING → CONNECTING_TO_BRIDGE** 
- Triggered by successful DiscoveryService response
- MGWCP connects to Bridge CP address

**CONNECTING_TO_BRIDGE → WAITING_FOR_DP_CONNECTION**
- MGWCP sends DP_INFO immediately after connection
- MGWCP waits for DP_CONNECTION_CONFIRMED

**WAITING_FOR_DP_CONNECTION → OPERATIONAL**
- Triggered by DP_CONNECTION_CONFIRMED from Bridge CP
- MGWCP begins accepting offloading requests

### B.2 State-Dependent Behaviors

**OPERATIONAL State**:
- Accepts ROS 2 service calls for offloading requests
- Sends OFFLOAD_REQUEST messages to Bridge CP
- Sends SESSION_KEEPALIVE every 15 seconds for active sessions
- Processes SESSION_APPROVED/DENIED responses

**Error States**:
- Connection failures cause transition back to DISCOVERING
- Protocol errors may cause MGWCP to restart connection sequence

### B.3 Blocking Behaviors

**State Transition Blocking**: MGWCP will not proceed to the next state until expected response is received.

**Service Response Blocking**: Offloading service calls are queued until MGWCP reaches OPERATIONAL state.

**Data Plane Blocking**: Data plane does not process messages until DP_CONNECTION_CONFIRMED is received.

---

## Appendix C: Timing and Timeout Requirements

### C.1 MGWCP Timing Behaviors

**Keepalive Interval**: 15 seconds for SESSION_KEEPALIVE messages
**Keepalive Timeout**: MGWCP expects response within reasonable time
**Retry Timeout**: MGWCP retries unacknowledged messages (implementation-specific interval)
**Connection Timeout**: MGWCP may retry connection on failure

### C.2 Bridge CP Timeout Requirements

**MGWCP Keepalive Timeout**: 20 seconds (5 seconds more than MGWCP sending interval)
**Data Plane Connection Timeout**: Reasonable timeout for DP connection attempts
**ACK Response Time**: ACKs should be sent immediately (sub-second response)
**Message Processing Time**: Non-blocking message processing to maintain responsiveness

### C.3 Timeout Handling

**Keepalive Timeout**: Terminate session and notify OM
**Connection Timeout**: Close connection and clean up state
**ACK Timeout**: Retry message delivery to MGWCP
**OM Response Timeout**: Queue messages and attempt OM reconnection

---

## Appendix D: Protocol Compliance Checklist

### D.1 Message Format Compliance

- [ ] All messages conform to universal JSON envelope structure
- [ ] Required fields (component_id, message_code, message_type, sequence_number, payload) present
- [ ] Message codes match specification
- [ ] Payload structures match message type requirements
- [ ] JSON is well-formed and UTF-8 encoded

### D.2 Reliability Mechanism Compliance

- [ ] ACK messages sent immediately upon message receipt
- [ ] ACK payload contains correct ack_sequence_number
- [ ] Sequence numbers are unique and monotonically increasing per connection
- [ ] Unacknowledged messages are retried with same sequence number
- [ ] Timeout detection and retry logic implemented

### D.3 Session Management Compliance

- [ ] Session state tracking per request_id and component_id
- [ ] Keepalive forwarding between MGWCP and OM
- [ ] Timeout detection for inactive sessions (20 seconds)
- [ ] Session cleanup on MGWCP disconnection
- [ ] Proper routing of responses to originating MGWCP

### D.4 Data Plane Compliance

- [ ] DP_INFO processing and data plane connection establishment
- [ ] DP_CONNECTION_CONFIRMED sent after successful data plane connection
- [ ] Data plane health monitoring and failure detection
- [ ] BRIDGE_DP_FAILURE reporting to OM on data plane failures
- [ ] Session-based data plane connections after SESSION_APPROVED

### D.5 Error Handling Compliance

- [ ] Invalid message handling (malformed JSON, missing fields)
- [ ] Connection failure detection and cleanup
- [ ] Error logging with appropriate detail level
- [ ] Graceful handling of partial system failures
- [ ] Proper error reporting to OM and MGWCP

---

**End of Bridge Control Plane Implementation Guide**

This guide provides comprehensive specifications for implementing a Bridge Control Plane that is fully compatible with the Modular Gateway Control Plane. All message formats, behavioral requirements, and interaction patterns necessary for successful