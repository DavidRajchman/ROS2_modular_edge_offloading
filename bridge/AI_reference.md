

# Bridge System Design Specification

## TODO: High-Level Design Finalization Points
*   **[ ] 1. Interface Definitions (Abstract View):**
    *   [ ] 1.1. Bridge Control Plane - OM Interface (Key message types, ACKs).
    *   [ ] 1.2. Bridge Control Plane - VHC Control Interface (Key message types).
    *   [ ] 1.3. Bridge Control Plane - Data Plane Internal Interface (Interaction mechanism for handler/map management).
*   **[ ] 2. Routing Map - Access and Structure:**
    *   [ ] 2.1. Queue Identification (How `Source_Identifier` in map value maps to handler's MPSC queue).
    *   [ ] 2.2. Update Strategy (Preferred high-level strategy for CP updates to map, e.g., atomic pointer swaps).
*   **[ ] 3. VHC-Bridge Data Stream Reconnection Strategy:**
    *   [ ] 3.1. Finalize strategy (e.g., confirm: re-establish transport, notify CP, CP informs OM of disruption, OM decides on task restart/termination).
*   **[ ] 4. Control Plane State Management:**
    *   [ ] 4.1. Outline essential state per active offloading session.
*   **[ ] 5. Key Configuration Parameters (Conceptual):**
    *   [ ] 5.1. List critical configurable parameters for the Bridge.
*   **[ ] 6. Diagnostic Data Points (High-Level):**
    *   [ ] 6.1. Identify key categories of information for logging.


## 1. System Overview

### 1.1. Overall Goal and Context
The primary goal is to create a "Bridge" application that acts as an intelligent, low-latency intermediary. It will manage the flow of ROS2 topic messages between vehicles (VHCs) and multiple Mobile Edge Computing (MEC) servers. This entire process is orchestrated by an external component called the Offloading Manager (OM). The Bridge is crucial for enabling vehicles to offload computationally intensive tasks to nearby MEC servers.

### 1.2. Key Entities
*   **Vehicle (VHC):** A mobile entity (e.g., a car) running a ROS2 environment. It generates data on various ROS2 topics (e.g., MAP, SPEED, GPS, PICTURES) and can request to offload computations.
*   **MEC Server & Docker Instances:** A Mobile Edge Computing server, also running a ROS2 environment, typically within Docker containers. It performs computations on data received from VHCs. Each MEC Docker instance is dedicated to processing tasks for exactly one VHC.
*   **Offloading Manager (OM):** An external system responsible for high-level decision-making. It decides if and where offloading should occur, manages MEC resources (e.g., instructing the MEC Control Plane to spawn Docker containers), and instructs the Bridge on how to route data.
*   **Bridge:** The application being designed. It sits between VHCs and MECs, routing messages according to OM's directives. It comprises a Control Plane and a Data Plane.
*   **MEC Control Plane:** An entity (presumably part of the MEC infrastructure or managed by OM) that can spawn/destroy Docker containers on MEC servers based on OM directives.
*   **Task Database:** An external, static database that maps a "task ID" (used in control communications) to the specific input ROS2 topics required for that task and the output ROS2 topic(s) that will carry the results. This database does not change during runtime.

### 1.3. High-Level Communication Paths
*   **VHC <-> Bridge Control Plane <-> OM:** For control messages (e.g., offloading requests, keep-alives, OM directives). VHC-OM communication is always relayed via the Bridge Control Plane.
*   **VHC <-> Bridge Data Plane <-> MEC Docker:** For the actual ROS2 topic data streams.
*   **OM <-> MEC Control Plane:** For MEC Docker lifecycle management.

## 2. Bridge Internal Architecture

The Bridge is designed with a dual-plane architecture to separate control logic from high-speed data forwarding.

### 2.1. Control Plane
*   **Responsibilities:**
    *   Manages control communication with VHCs (via a dedicated TCP stream, separate from the data stream) and the OM.
    *   Can "snoop" on VHC-OM relayed messages to extract useful contextual information.
    *   Receives directives from the OM regarding offloading session setup and teardown.
    *   Manages the lifecycle of Data Plane transport handlers (creation, configuration, destruction).
    *   Updates and maintains the shared Routing Map used by the Data Plane.
    *   Maintains state about active offloading sessions (e.g., VHC ID, task ID, associated MEC handler).
*   **Reliability:** Aims for guaranteed message delivery for critical control messages it sends or relays (e.g., using application-level acknowledgments and retries). Can tolerate more overhead than the data plane due to lower data rates.

### 2.2. Data Plane
*   **Core Functionality:**
    *   Receives ROS2 topic messages from VHCs over TCP streams.
    *   Forwards VHC messages to one or more designated MEC Docker instances based on the Routing Map. Supports "fan-out" (max ~5 copies, using `std::shared_ptr` to avoid data duplication).
    *   Receives computed results from MEC Docker instances and routes them back to the correct VHC.
    *   **Low Latency:** A paramount design driver for all data plane operations.
*   **Universal Transport Handlers:**
    *   The core processing units are instances of a single, generic "universal transport handler" class/template.
    *   One handler instance is dedicated to managing each active TCP data connection (to a VHC or a MEC Docker).
    *   This approach promotes code reusability and consistency.
*   **Mesh-like Communication & MPSC Queues:**
    *   Handlers communicate directly with each other by placing messages onto target handlers' input queues, forming a "mesh-like" flow rather than passing through a central routing task.
    *   Each handler has its own input queue, which is MPSC (Multi-Producer, Single-Consumer): multiple handlers can produce messages for the queue, but only the owning handler consumes from it.
    *   Intention is to use high-performance, preferably lock-free, MPSC queue implementations.
*   **Shared Routing Map:**
    *   A concurrently accessible data structure (e.g., `std::unordered_map`) storing routing rules.
    *   **Key:** `(Source_Identifier_from_Header, Topic_from_Header)`
        *   `Source_Identifier_from_Header`: ID of the entity that sent the message directly to the Bridge (e.g., VHC_ID or MEC_ID).
        *   `Topic_from_Header`: ROS2 topic name from the message header.
    *   **Value:** A list of identifiers for the destination handler queues.
    *   Populated/updated by the Control Plane. Data Plane access must be thread-safe and highly efficient (e.g., reader-writer locks or atomic pointer swaps for map updates).
*   **Message Processing Flow (Data Plane Handler):**
    1.  Receives a raw message over its TCP connection.
    2.  Parses the header to extract `Source_Identifier` and `Topic`.
    3.  Performs a lookup in the shared Routing Map.
    4.  For each destination queue identified, enqueues a `std::shared_ptr<Message>`.
*   **Symmetrical Routing Logic Execution:**
    *   The Data Plane handler's code for lookup and enqueuing is identical for messages from VHCs or MECs. The intelligence to differentiate flows is encoded in the Routing Map's contents by the OM via the Control Plane.

## 3. Offloading Process Lifecycle (Dynamic Behavior)

This outlines the sequence of events for establishing, maintaining, and tearing down an offloading session.

*   **Step 1: VHC Offloading Request:** VHC sends a control message (task ID) to OM, relayed via Bridge CP.
*   **Step 2: OM Resource Check & Approval:** OM checks resources. If available, proceeds.
*   **Step 3: OM Initiates Offloading:**
    *   OM informs Bridge CP of approval (including MEC details).
    *   OM directs MEC CP to spawn the task-specific Docker.
*   **Step 4: Bridge CP Prepares Data Path:**
    *   Creates/configures a transport handler for the new MEC Docker.
    *   Updates Routing Map for VHC -> MEC (input topics from Task Database) and MEC -> VHC (result topics from Task Database).
*   **Step 5: Bridge CP Instructs VHC:** Sends control message to VHC to start sending specified topics and prepare for result topics.
*   **Step 6: Stable Data Flow:** VHC sends data, MEC Docker connects. Data flows: VHC -> Bridge DP -> MEC Docker, and MEC Docker -> Bridge DP -> VHC.
*   **Step 7: VHC Keep-Alive & OM Timeout:** VHC sends periodic keep-alives to OM (via Bridge CP). If OM doesn't receive one in time, it initiates teardown.
*   **Step 8: OM Decides to Stop Offloading:** (e.g., due to missing keep-alive) OM informs Bridge CP.
*   **Step 9: Bridge CP Instructs VHC to Stop Sending Data:** Relays OM's decision.
*   **Step 10: Bridge Graceful Data Path Shutdown:** Bridge DP waits a configurable time for in-flight MEC results to be forwarded to VHC.
*   **Step 11: Bridge CP Finalizes Teardown:**
    *   Disables/destroys the MEC transport handler.
    *   Cleans up relevant Routing Map entries.
    *   Instructs VHC to stop processing results for this task.
*   **Step 12: OM Stops MEC Docker:** OM directs MEC CP to terminate the Docker instance.

## 4. Failure Handling and System Reliability

### 4.1. Operational Environment & Philosophy
*   **Lab Environment:** The system is primarily for a lab setting.
*   **Diagnostics over Automatic Recovery:** For component crashes (Bridge, OM, etc.) or hardware resource exhaustion, the system will not attempt automatic recovery. Instead, it must provide comprehensive diagnostic information (detailed logs, error messages with context like timestamps, IDs) to enable human operators to understand the cause of failure. Structured logging is recommended.
*   **Data Plane Efficiency:** Data transmission in the data plane must be highly efficient with minimal overhead.
*   **Control Plane Robustness:** Control message transmission can tolerate more overhead to achieve higher reliability for critical operations.

### 4.2. Network Failure Handling
*   **General Internal/Backend Links (Bridge-OM, Bridge-MEC CP, Bridge DP-MEC Docker):**
    *   Reliance on TCP's inherent reliability and `TransportLib` safeguards for message framing and basic error detection.
    *   If a TCP connection on these links breaks permanently (after TCP retries fail), the Bridge component (CP or DP handler) will detect the failure, log it extensively, and the associated session/operation will likely be considered failed. The Control Plane may inform the OM. No complex application-level reconnection or session resumption will be attempted for these specific broken segments by the data plane.
*   **VHC-Bridge Link (LTE - Unreliable):**
    *   This link is considered less reliable and requires robust application-level TCP reconnection logic for both control and data streams.
    *   **Control Stream (VHC <-> Bridge CP):** Reconnection logic should be implemented. If the stream breaks, attempts to re-establish should occur. The impact of lost control messages during the outage needs consideration (potentially addressed by the guaranteed delivery mechanism).
    *   **Data Stream (VHC <-> Bridge DP):** Reconnection logic is needed. Upon reconnection, the strategy for handling data consistency (potential gaps, duplicates, state of the offloading task) must be defined. It may be simpler to flag the offloading task as encountering an error to the OM, which can then decide on restarting or terminating the task, rather than attempting complex data stream state synchronization.
*   **Bridge Control Plane Message Delivery Guarantee:**
    *   For critical control messages (e.g., OM directives to Bridge CP, Bridge CP instructions to VHC), the Bridge CP will aim for guaranteed delivery. This implies application-level mechanisms such as:
        *   Sequence numbers.
        *   Acknowledgments (ACKs).
        *   Sender-side timers and retry mechanisms.
        *   Handling of potential duplicate messages.

### 4.3. Response to Non-Network Component Failures
*   If a component (VHC, Bridge CP, Bridge DP handler, OM, MEC Docker/CP) crashes or becomes unresponsive:
    *   **Detection:** Typically through TCP connection drops, missed heartbeats/keep-alives, or explicit error messages if the component can send one before failing.
    *   **Bridge Action:** Log the failure in detail. If a data plane handler detects its peer's failure, it notifies the Bridge CP. The Bridge CP, upon detecting a failure or being notified, logs it and may inform the OM.
    *   **System State:** The affected offloading session(s) will likely be terminated or marked as failed. No automatic restart of failed components is expected.

### 4.4. Data Integrity and Timeouts
*   **Data Corruption:** Messages found to be corrupted (e.g., failing checksums if available, parsing errors) should be logged and discarded. The impact on the specific offloading task should be assessed (potentially leading to task failure).
*   **Timeouts:** Timeouts are critical for detecting unresponsive components or operations that take too long (e.g., VHC keep-alive timeout at OM, Bridge CP waiting for ACK from VHC). Timeout values should be configurable. Upon a timeout, a defined error handling path should be taken (e.g., log, notify OM, initiate teardown).

### 4.5. Failure Escalation
*   Data Plane handlers report unrecoverable errors (e.g., persistent connection loss to MEC) to the Bridge Control Plane.
*   The Bridge Control Plane reports significant failures (e.g., inability to set up a route, loss of communication with OM or VHC, critical errors from Data Plane) to the OM (if possible) and/or logs them extensively for manual intervention.

## 5. Key Design Principles and Assumptions Summary
*   **Low Latency (Data Plane):** A primary driver for data forwarding architecture.
*   **Identical Transport Layer:** VHCs and MECs utilize the same `TransportLib` and ROS2 environment.
*   **Standardized Message Headers:** Messages contain `Source_Identifier` and `Topic` for routing.
*   **Atomicity of Routing Map Updates:** Control Plane updates to the shared Routing Map must be safe for concurrent Data Plane reads.
*   **Task ID as Central Key:** Used in control communications to identify offloading instances and their associated topics via the external Task Database.
*   **Bridge Control Plane State:** Manages state for active offloading sessions.
*   **Lab Environment Constraints:** Dictates the approach to recovery (diagnostics first) and reliability (focused efforts on VHC link and CP messaging).
*   **Secure Environment:** Complex security measures are out of scope for the current phase.
*   **Separate Control/Data Planes:** For clarity, independent resource management (mostly), and differing reliability requirements. Communication between planes (e.g., CP updating routing map used by DP) must be efficient and safe.

This revised document aims to provide a comprehensive understanding of the Bridge system, its dynamic operation, and its approach to handling failures within the specified lab environment constraints.