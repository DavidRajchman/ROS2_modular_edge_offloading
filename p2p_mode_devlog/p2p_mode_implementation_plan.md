# P2P Mode Implementation Plan

This document outlines the implementation of direct Peer-to-Peer (`p2p`) and Discovery-Assisted P2P (`p2p_ds`) modes for the Modular Gateway.

## Core Requirement: Backward Compatibility
**CRITICAL:** The default mode of the Gateway (`networked` mode) must NOT be affected in any way by these new features. All old functionality, including the existing Discovery workflow, Bridge CP communication, and Offloading Manager interactions, must be fully preserved when the system is run in `networked` mode. P2P enhancements must act strictly as parallel operational branches.

## Phase 1: Configuration & Operational Modes
We introduce three distinct operation modes selectable via ROS2 parameters:

1.  **`networked`**: (Default) Discovery -> Bridge CP -> OM Approval -> Data Plane.
2.  **`p2p`**: Pure direct connection. VHC needs MEC IP statically. No Discovery or Bridge required.
3.  **`p2p_ds`**: Discovery-assisted P2P. Only DS IP is fixed. DS acts as a matchmaking service to pair a VHC and MEC based on a shared ID.

**New Parameters**:
*   `operation_mode`: `networked` | `p2p` | `p2p_ds`.
*   `p2p.peer_host`: (p2p mode only) Static IP of the MEC.
*   `p2p.peer_port`: (p2p mode only) Port of the MEC Data Plane.
*   `p2p.local_config_path`: Path to a JSON file defining tasks for the session.

## Phase 2: Asymmetric Transport Roles
In both P2P modes, the TCP roles are determined by the `component_type`:
*   **MEC ("M")**: Always acts as the **TCP Server**. Instantiates `TcpServerTransport`.
*   **VHC ("V")**: Always acts as the **TCP Client**. Instantiates `TcpClientTransport`.

## Phase 3: State Machine & Matchmaking Logic

### P2P Mode Process:
1.  **INITIALIZING**: Load local JSON config. VHC prepares `TcpClientTransport` (static IP). MEC starts `TcpServerTransport`.
2.  **CONNECTING**: VHC attempts TCP connection. MEC waits.
3.  **OPERATIONAL**: Once TCP is connected, trigger `activate_all_p2p_sessions()`.

### P2P_DS Mode Process:
1.  **INITIALIZING**: Load local JSON config.
2.  **DISCOVERING**:
    *   Both register with DS using a shared `component_id` (groupId:idInGroup).
    *   **MEC** registers. DS stores MEC IP/Port and returns `SUCCESS`. MEC transitions to **CONNECTING** (starts TCP Server).
    *   **VHC** registers.
        *   If MEC is not yet in DS registry: DS returns `WAIT`. VHC sleeps 2s and retries registration.
        *   If MEC is registered: DS returns `SUCCESS` and provides MEC's IP/Port in `connectionTargetAddress/Port`.
3.  **CONNECTING**: VHC extracts MEC IP/Port and initiates TCP connection.
4.  **OPERATIONAL**: Once TCP is connected, trigger `activate_all_p2p_sessions()`.

## Phase 4: Protocol Field Reuse

### Discovery Protocol (for `p2p_ds` Matchmaking):
No structural changes. Reusing existing fields:
*   `groupId`/`idInGroup`: Shared identifier for the 1-to-1 session.
*   `idRequestType`: Must be `STATIC`.
*   `RegistrationResponse::ResponseCode`: `WAIT` used for VHC polling.
*   `connectionTargetAddress/Port`: DS forwards MEC's detected IP and listen port to the VHC.

### Modular Gateway Control Plane (MGW_CP):
In P2P modes, the **Control Plane is entirely bypassed**.
*   No `BridgeCpClient` connection is made.
*   Session approval is handled internally by constructing a mock JSON payload from the local config and calling `on_session_approved()`.

### P2P Mode Identification:
Regardless of the operational mode, the `groupId` and `idInGroup` are **always** read directly from the existing `identity.group_id` and `identity.identifier_in_group` ROS parameters. 
*   In `p2p_ds` mode, these parameters are sent to the Discovery Service to find a peer with the exact same IDs.
*   In pure `p2p` mode, these parameters must be correctly set in the P2P launch files for both the VHC and MEC to ensure the binary headers match during data exchange.

## Phase 5: Edge Cases & Error Handling

1.  **ID Collision**: If a third device tries to register with a `groupId:idInGroup` already active in the DS, DS returns `ID_CONFLICT`.
2.  **Peer Disconnect**: If the Data Plane connection drops, the MGW transitions back to `INITIALIZING` (p2p) or `DISCOVERING` (p2p_ds).
3.  **Out-of-Order Registration**: The VHC polling logic (`WAIT` -> retry) ensures the system recovers regardless of which peer starts first.
4.  **Config Mismatch**: If VHC and MEC have different local JSON configs, the Data Plane will still connect, but handlers might not match (e.g., VHC sends type 201, MEC has no handler for 201). Handlers will log "No handler registered".
5.  **Graceful Teardown**: On SIGINT or standard node shutdown, the Gateway must cleanly close the `TcpClientTransport` or `TcpServerTransport` sockets to prevent `TIME_WAIT` hangups, ensuring rapid restarts are supported during testing.
6.  **Local Config I/O Failure**: If reading `p2p.local_config_path` fails (file not found or malformed JSON), the Gateway will log a `RCLCPP_FATAL` error and crash, as it cannot proceed without a valid task configuration in P2P mode.

## Phase 7: Technical Specifications & Edge Cases

### 1. Mock Session Activation
In P2P/P2P_DS modes, the Control Plane (Bridge CP) is bypassed. To activate handlers, the `GatewayController` will manually invoke `on_session_approved` for each task in the `task_database_` using this synthetic JSON structure:
```json
{
  "request_id": "p2p_static_session",
  "task_id": "TASK_ID_FROM_CONFIG"
}
```

### 2. Discovery Service P2P Behavior (`--p2p` flag)
When running in P2P mode, the `DiscoveryService` modifies its mandate:
*   **Ignore OM Dependency**: Registration is allowed even if `OFFLOAD_MANAGER` is absent.
*   **Persistence**: The service will NOT shut down when a client disconnects.
*   **Dynamic Re-pairing**: If a `VEHICLE` with ID `X` disconnects, its entry is purged from the registry, allowing a new `VEHICLE` with ID `X` to register and pair with the existing `MEC` (or vice versa).

### 3. VHC Connection Resilience
In `p2p` mode (no Discovery Service), the VHC may start before the MEC is listening. 
*   **Logic**: If `transport_->connect()` fails, the VHC will log: `[P2P] Peer not reached, retrying in 5s...`.
*   **State**: It remains in `WAITING_FOR_DP_CONNECTION` and retries the connection indefinitely (or until `running_` is false).

### 4. Config Path Resolution & File I/O
The `p2p.local_config_path` parameter supports:
*   **Absolute Paths**: (e.g., `/home/user/config.json`).
*   **Relative Paths**: Resolved against the directory where the MGW process was launched (usually the ROS2 workspace root).
*   **Implementation**: A C++ file reading utility will open the specified file and read its contents into a `std::string`, which is then passed to the existing `load_global_config_from_json()` method.

### 5. Task ID Normalization
The `BridgeCpClient` currently performs normalization on `task_id` (converting numbers to strings). The P2P auto-activation logic must ensure that `task_id` is consistently handled as a string when passed to `task_database_.find()`.

## Phase 8: CRITICAL HOTFIX: P2P Matchmaking ID Conflict

> [!CAUTION]
> **POTENTIAL BREAKING CHANGE / ARCHITECTURAL DEBT**
>
> During the final loopback testing, an ID conflict was discovered in `p2p_ds` mode. The Discovery Service registry was designed for globally unique IDs, but P2P matchmaking requires the VHC and MEC to share the same `groupId:idInGroup`.
>
> ### Hotfix Implemented (2026-05-06):
> To allow the test to pass, a "Short-Circuit Matchmaking" fix was applied to `discovery_service.cpp`:
> 1. When a **VEHICLE** requests registration in P2P mode, the service checks for an MEC match first.
> 2. If a match is found, the service sends the MEC details to the VHC and **IMMEDIATELY RETURNS** without registering the VHC in the registry.
> 3. This avoids the `ID_CONFLICT` error because only the MEC is ever "officially" registered for that ID.
>
> ### Future Corrective Action Required:
> *   **Registry Redesign**: The `ComponentRegistry` should be updated to use a composite key `(ComponentType, GroupID, IdInGroup)` instead of just `(GroupID, IdInGroup)`.
> *   **Matchmaking Formalization**: Matchmaking should be handled by a dedicated `Matchmaker` component rather than side-effects within the `handleRegistration` flow.
> *   **Standard Mode Verification**: Ensure this short-circuit logic never triggers in `networked` mode, as it would prevent Vehicles from being managed by the Bridge/OM.
