# Affected Files and Modifications for P2P and P2P_DS Modes

This document provides a comprehensive list of files and functions that require modification.

## 1. `gateway_controller.hpp` / `.cpp`
*   **Parameters**: Add `operation_mode`, `p2p.peer_host`, `p2p.peer_port`, `p2p.local_config_path`.
*   **`initialize()`**:
    *   `networked`: Create `TcpServerTransport`.
    *   `p2p` / `p2p_ds`:
        *   If VHC: Create `TcpClientTransport`.
        *   If MEC: Create `TcpServerTransport`.
        *   **File I/O**: Read the file at `p2p.local_config_path` into a string and pass it to `load_global_config_from_json()`. If it fails, trigger a fatal error.
*   **Graceful Shutdown**: Ensure destructors or ROS shutdown hooks cleanly close the active TCP sockets to avoid `TIME_WAIT` states.
*   **`control_thread_func()`**:
    *   Branch logic for `State::INITIALIZING` and `State::DISCOVERING`.
    *   If `p2p_ds`: Call `discovery_client_->start()` and wait for MEC IP resolution.
    *   If `p2p`: Transition directly from `INITIALIZING` to `WAITING_FOR_DP_CONNECTION`.
*   **New `activate_all_p2p_sessions()`**: Method to iterate through `task_database_` and call `on_session_approved()`.

## 2. `discovery_client.cpp`
*   **Polling Logic**: In `client_thread_func()`, if `decode_message` returns `ResponseCode::WAIT`, do not call `failure_cb_`. Instead:
    *   Sleep for 2 seconds.
    *   Re-send the `REGISTRATION_REQUEST`.
    *   Loop until `SUCCESS` or `running_` is false.

## 3. `discovery_service.cpp`
*   **New Launch Flag**: Add a boolean `--p2p` flag (via CLI argument).
*   **`handleRegistration()`**:
    *   If `--p2p` is enabled:
        *   Bypass OM requirement check.
        *   Bypass Bridge availability check.
        *   **Matchmaking**:
            *   If `ComponentType::MEC` registers: Store IP/Port and return `SUCCESS`.
            *   If `ComponentType::VEHICLE` registers:
                *   Check if a `MEC` with matching `groupId:idInGroup` is in `registry_`.
                *   If found: Return `SUCCESS` with MEC IP/Port in `connectionTarget...` fields.
                *   If NOT found: Return `WAIT`.

## 4. `protocol.hpp` (Discovery)
*   No changes needed. Reusing existing fields as planned.

## 5. `handler_factory.cpp`
*   Ensure that when P2P handlers are activated, the `id_group` and `id_in_group` are correctly passed to the `RosGateway` so the binary headers match the P2P ID.

## 5. Safeguards & Service Behavior
*   **P2P State Transitions**:
    *   **VHC**: In `control_thread_func`, call `on_dp_confirmed()` manually after successful `transport_->connect()`.
    *   **MEC**: In `data_plane_connection_thread_func`, call `on_dp_confirmed()` manually after successful `accept_connection()`.
*   **Null-Pointer Protection**: Guard all calls to `bridge_cp_client_` and `discovery_client_` in `gateway_controller.cpp`. In P2P modes, these objects will not be instantiated.
*   **Config Priority**: Modify `on_discovery_success` to skip `load_global_config_from_json` if `operation_mode_ == "p2p_ds"`, as config is loaded locally.
*   **P2P Service Logic**: 
    *   Update `offloading_request_service_handler` to return success immediately in P2P mode if the task is already auto-activated.
    *   Update `terminate_offloading_service_handler` to call `handle_session_teardown` locally without attempting to notify the Bridge.
*   **Keepalive Bypass**: In `control_thread_func`, skip the session keepalive loop if `operation_mode_ != "networked"`.

## 6. Launch & Configuration
*   **Namespacing**: `p2p_vhc_launch.py` and `p2p_mec_launch.py` must use ROS2 namespaces (`/vhc`, `/mec`) to allow loopback testing on a single host.
*   **`ros/src/ros_ws/src/modular_gateway_sender/config/p2p_config.json`**: Template configuration.

