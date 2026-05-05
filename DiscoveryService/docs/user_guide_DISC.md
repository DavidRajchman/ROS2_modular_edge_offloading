# Discovery Service User Guide

## 0. NAMING CONVENTION
Core terms used in the Discovery Service:
* **DISC** / **Discovery Service** – Central registration service providing component discovery and configuration distribution
* **component_id** – Unique identifier in format `group_id:id_in_group` (e.g., `60:5`)
* **VHC** – Vehicle Host Component
* **MEC** – Mirror Execution Component  
* **OM** – Offloading Manager
* **Bridge** – Bridge Control Plane
* **global_configuration** – System-wide configuration JSON provided by OM and distributed to all components
* **keepalive** – Periodic ping messages maintaining component registration
* **client_id** – Internal transport layer identifier for TCP connections

---

## 1. INTRODUCTION & SCOPE
This user guide covers the Discovery Service, a lightweight C++ application that serves as the central registration and discovery point for the entire offloading system. The Discovery Service is the **only component with a fixed IP address** that all other components must know in advance.

Discovery Service responsibilities:
* Accept registration requests from all system components
* Distribute global configuration from OM to all components
* Provide connection details for Bridge CP to VHCs and MECs
* Provide OM connection details to Bridge
* Track component liveness through keepalive messages
* Auto-detect client IP addresses for NAT traversal scenarios
* Enforce registration ordering (OM → Bridge → VHC/MEC)

The Discovery Service does not make offloading decisions, route messages, or manage sessions—it only facilitates component discovery and initial connection setup.

---

## 2. HIGH-LEVEL ARCHITECTURE
Discovery Service operational flow:

### 2.1 Startup Sequence
1. **Initialization**: Start TCP server on port 9090
2. **Wait for OM**: Block all non-OM registrations until OM connects
3. **Receive Configuration**: Store global_configuration JSON from OM registration
4. **Accept Bridge**: Allow Bridge registration, provide OM connection details
5. **Accept Gateways**: Allow VHC/MEC registration, provide Bridge connection details
6. **Operational**: Process keepalives, handle queries, monitor for stale components

### 2.2 Registration Flow by Component Type

**Offloading Manager (OM):**
1. OM connects and sends REGISTRATION_REQUEST with global_configuration in `humanReadableMessage` field
2. DISC stores configuration and responds with SUCCESS
3. System is now ready for other components

**Bridge:**
1. Bridge connects and sends REGISTRATION_REQUEST
2. DISC checks if OM is available
3. If yes: Responds with SUCCESS + OM connection details (`connectionTargetAddress`, `connectionTargetPort`)
4. If no: Responds with WAIT response code

**VHC/MEC Gateways:**
1. Gateway connects and sends REGISTRATION_REQUEST
2. DISC checks if global_configuration exists (OM registered) and Bridge is available
3. If yes: Responds with SUCCESS + Bridge connection details + global_configuration JSON
4. If no: Responds with WAIT response code

### 2.3 Component Lifecycle
```
Connect → Register → Keepalive (every 5s) → Disconnect/Timeout → Unregister
```

**Timeout handling:**
- Keepalive timeout: 15 seconds
- Stale client check: Every 7.5 seconds
- On timeout: Component is automatically unregistered and disconnected
- **Critical**: If OM disconnects, DISC immediately shuts down entire system

For detailed protocol specification, see [ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md](../../ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md).

---

## 3. COMPONENT QUERY FEATURE

### 3.1 Overview
The Discovery Service supports querying for registered components by type. This allows components to discover available peers dynamically.

### 3.2 Usage
Send COMPONENT_QUERY message with specific component type filter:

**Supported filters:**
- `V` - Query for all registered VHCs
- `M` - Query for all registered MECs
- `B` - Query for all registered Bridges
- `O` - Query for all registered OMs
- `T` - Query for test components

**Not supported:**
- `ALL` - Returns error, must specify single type
- Empty filter - Returns error

### 3.3 Response Format
COMPONENT_LIST_RESPONSE contains:
- `componentCount`: Number of matching components
- `componentDataList`: Semicolon-separated entries, each formatted as:
  ```
  component_type:group_id:id_in_group:subtype
  ```

Example response for 2 MECs:
```
componentCount: 2
componentDataList: ["M:70:1:0", "M:70:2:0"]
```

### 3.4 Use Cases
- VHC discovering available MECs
- Monitoring tools listing all registered components
- Load balancers discovering Bridge instances

---

## 4. BUILD & INSTALLATION
Prerequisites: C++17 compiler, CMake 3.16+, TransportLib, DiscoveryProtocol library

### 4.1 Manual Build
Build steps:
```bash
cd /home/ubuntu/DiscoveryService
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./DiscoveryService
```

See [DiscoveryService/CMakeLists.txt](../CMakeLists.txt) for build configuration.

Binary output location: `build/DiscoveryService`

### 4.2 Auto-Start Script
The Discovery Service includes an automated build and launch script:

```bash
cd /home/ubuntu/DiscoveryService
./autostart.sh
```

See [DiscoveryService/autostart.sh](../autostart.sh) for script details.

**What the auto-start script does:**
1. Creates build directory if it doesn't exist
2. Runs `cmake ..` to configure the project
3. Runs `make -j$(nproc)` to build with parallel compilation
4. Verifies executable was created
5. Launches `./DiscoveryService` directly
6. Exits with error code 1 if build or launch fails

**Usage in Docker:**
The script is designed to run as the container entrypoint. To run DISC in development mode without auto-start, override the container command in docker-compose.

---

## 5. RUNTIME CONFIGURATION
The Discovery Service uses compile-time constants for core configuration (defined in [DiscoveryService/src/discovery_service.hpp](../src/discovery_service.hpp) and [DiscoveryService/CMakeLists.txt](../CMakeLists.txt)):

**Fixed Configuration:**
* Listen Port: `9090` (DISCOVERY_SERVICE_PORT in CMakeLists.txt)
* Keepalive Timeout: `15 seconds` (KEEPALIVE_TIMEOUT in discovery_service.hpp)
* Stale Check Interval: `7.5 seconds` (KEEPALIVE_TIMEOUT / 2)
* Multi-client Mode: Enabled (unlimited simultaneous connections)

**Logging Configuration:**
* App Name: `DiscoverySvc` (defined via APP_NAME in CMakeLists.txt)
* Active Log Level: `LOG_LEVEL_INFO` (defined via ACTIVE_LOG_LEVEL in CMakeLists.txt)
* Version: `1.0` (PROJECT_VERSION in CMakeLists.txt)

**Dynamic Configuration (from OM):**
* Global configuration JSON (stored in `global_configuration_` member)
* Available tasks, session limits, message type definitions

**Command-Line Flags:**
* `--p2p`: Runs the Discovery Service in P2P_DS mode. In this mode, the service bypasses all OM/Bridge requirements and solely performs matchmaking between VHCs and MECs based on matching `group_id` and `id_in_group`.

To modify configuration: Edit constants in [DiscoveryService/CMakeLists.txt](../CMakeLists.txt) or [DiscoveryService/src/discovery_service.hpp](../src/discovery_service.hpp) and rebuild.

---

## 6. OPERATIONAL RESTRICTIONS

### 6.1 Registration Ordering Requirements
The Discovery Service enforces strict ordering to ensure system integrity:

**1. Offloading Manager MUST register first:**
- OM is the only component that can register before global configuration exists
- OM provides the global configuration in its registration request
- All other components receive WAIT response until OM has registered

**2. Bridge can register after OM:**
- Bridge registration requires OM to be available
- DISC provides OM connection details to Bridge in registration response
- If no OM: Bridge receives WAIT response

**3. VHC/MEC can register after both OM and Bridge:**
- VHC/MEC registration requires:
  * Global configuration exists (OM has registered)
  * At least one Bridge is available
- DISC provides Bridge connection details + global configuration to VHC/MEC
- If conditions not met: VHC/MEC receives WAIT response

**Recommended launch order:**
```
1. Discovery Service
2. Offloading Manager
3. Bridge
4. VHC and MEC components (any order)
```

**P2P_DS Mode Exception:**
When the Discovery Service is launched with the `--p2p` flag, all ordering restrictions are lifted. OM and Bridge dependencies are bypassed, and VHC/MEC can register immediately to match with each other.

### 6.2 Critical Failure Handling
**OM Disconnection:**
- If OM disconnects (graceful or timeout), DISC immediately calls `stop()`
- All active connections are terminated
- DISC shuts down completely
- **Rationale**: System cannot function without OM, clean shutdown prevents inconsistent state
- **Recovery**: Restart DISC and all components in correct order

**Bridge Disconnection:**
- Bridge can disconnect/timeout without triggering DISC shutdown
- Remaining components continue operating
- New VHC/MEC registrations will receive WAIT until Bridge returns
- **Recovery**: Restart Bridge, existing VHC/MEC connections may need to re-register

### 6.3 Component ID Management
**ID Assignment:**
- Components provide their own `group_id` and `id_in_group` in registration request
- DISC validates uniqueness—duplicate IDs are rejected with ID_CONFLICT response
- No automatic ID assignment (all IDs must be pre-configured in components)

**ID Ranges:**
- group_id: 0-255 (uint8_t)
- id_in_group: 0-255 (uint8_t)
- Combined as `group_id:id_in_group` (e.g., "60:5")

**Important Note on Component Restarts:**
- If a component crashes or loses network connection abruptly, its registry entry persists until keepalive timeout (15 seconds)
- Attempting to restart the same component with the same ID within this window will result in ID_CONFLICT error
- This is expected behavior to prevent duplicate registrations during network instability
- Workarounds:
  * Wait 15-20 seconds before restarting crashed components
  * Restart DISC service to clear all registrations (requires restarting all components)
  * Use different component IDs for parallel testing instances

### 6.4 IP Address Auto-Detection
**Behavior:**
- DISC stores the TCP source IP for each connected client
- Registration response includes `detectedClientAddress` field with auto-detected IP
- Components behind NAT can use this to discover their public IP
- DISC uses detected IP (not client-provided `listenAddress`) for internal tracking

---

## 7. PROTOCOL API SUMMARY

For complete protocol details, see [ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md](../../ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md).

### 7.1 Discovery Protocol Overview
**Transport:** TCP, text-based messages  
**Format:** Semicolon-delimited fields, prefixed with `DISC:`  
**Library:** `discovery_protocol::` namespace

### 7.2 Message Types Summary

**REGISTRATION_REQUEST (Client → DISC):**
```
DISC:REG;<component_type>;<id_request_type>;<group_id>;<id_in_group>;<component_name>;<message>
```
- `component_type`: V (VHC), M (MEC), B (Bridge), O (OM), T (Test)
- `id_request_type`: A (Automatic), S (Static) - Note: Only S is currently supported
- `message` field: OM uses this to send global_configuration JSON

**REGISTRATION_RESPONSE (DISC → Client):**
```
DISC:ACK;<response_code>;<component_id>;<connection_target_address>;<connection_target_port>;<config_json>;<detected_client_ip>;<message>
```
- `response_code`: SUCCESS, WAIT, ID_CONFLICT, INVALID_REQUEST, GENERAL_ERROR
- `connection_target_address/port`: Bridge CP address for VHC/MEC, OM address for Bridge
- `config_json`: Global configuration (empty for OM and Bridge, populated for VHC/MEC)
- `detected_client_ip`: Auto-detected TCP source IP

**KEEPALIVE_PING (Client → DISC):**
```
DISC:PNG;<component_id>;<timestamp>;<message>
```
- `component_id`: Format "group_id.id_in_group" (e.g., "60.5")
- Sent every 5 seconds by all registered components

**KEEPALIVE_RESPONSE (DISC → Client):**
```
DISC:PON;<response>;<next_interval_ms>;<message>
```
- `response`: "OK"
- `next_interval_ms`: 5000 (recommended interval)

**COMPONENT_QUERY (Client → DISC):**
```
DISC:QRY;<component_type_filter>;<message>
```
- `component_type_filter`: Single type only (V, M, B, O, T) - "ALL" is not supported
- Used to discover available components of specific type

**COMPONENT_LIST_RESPONSE (DISC → Client):**
```
DISC:LST;<component_count>;<component_data_list>;<message>
```
- `component_data_list`: Semicolon-separated list
- Format per component: `component_type:group_id:id_in_group:subtype`

**ERROR_MESSAGE (Either direction):**
```
DISC:ERR;<error_code>;<message>
```
- `error_code`: CONFIGURATION, TRANSPORT, PARSING, PROTOCOL

### 7.3 Response Codes
| Code | Meaning | Description |
|------|---------|-------------|
| SUCCESS | Registration successful | Component registered, connection details provided |
| WAIT | Dependency not ready | OM not registered, or no Bridge available |
| ID_CONFLICT | Duplicate ID | Requested component_id already in use |
| INVALID_REQUEST | Malformed request | Invalid port, missing fields, etc. |
| GENERAL_ERROR | Internal error | Registry error or other server issue |

---

## 8. LOGGING & DIAGNOSTICS

### 8.1 Logging Framework
* **Library**: TransportLib logging utilities
* **Format**: Console output with `APP_NAME="DiscoverySvc"`
* **Levels**: DEBUG, INFO, WARN, ERROR (set via ACTIVE_LOG_LEVEL in CMakeLists.txt)

### 8.2 Log Level Configuration
**Build-time configuration:**
```cmake
target_compile_definitions(DiscoveryService PRIVATE
    ACTIVE_LOG_LEVEL=LOG_LEVEL_INFO
)
```

Available levels: `LOG_LEVEL_DEBUG`, `LOG_LEVEL_INFO`, `LOG_LEVEL_WARN`, `LOG_LEVEL_ERROR`

### 8.3 Key Log Messages

**Startup:**
```
INFO: Attempting to start Discovery Service...
INFO: Discovery Service starting on port 9090...
```

**Registration Events:**
```
INFO: Client connected with transport ID: <id> from IP: <address>
INFO: Processing REG request from client <id> for component '<name>'
INFO: Offloading Manager registered. Global configuration has been set.
INFO: Successfully registered component '<name>' with ID <group>.<id>
WARN: Component '<name>' trying to register before OM. Sending WAIT.
WARN: Bridge '<name>' trying to register, but no OM is available. Sending WAIT.
ERROR: ID conflict for component '<name>'. ID <group>.<id> is already taken.
```

**Keepalive & Timeouts:**
```
DEBUG: Keepalive received from component <group>.<id>
WARN: Client '<name>' (<group>.<id>) timed out. Last seen X.XX seconds ago. Disconnecting.
```

**Critical Events:**
```
ERROR: The Offloading Manager has disconnected. This is a critical failure. Shutting down the system.
INFO: Discovery Service has shut down gracefully.
```

### 8.4 Monitoring in Production
**Check process status:**
```bash
ps aux | grep DiscoveryService
netstat -tlnp | grep 9090
```

**Monitor logs:**
```bash
# If running in Docker
docker logs -f discovery

# If running directly
./DiscoveryService 2>&1 | tee discovery.log
```

---

## 9. TROUBLESHOOTING GUIDE

### 9.1 Registration Issues

**Components receiving WAIT response:**
- Check if OM has registered: Look for "Offloading Manager registered" log message
- For Bridge: Verify OM is connected
- For VHC/MEC: Verify both OM and Bridge are registered
- Check component logs to ensure they are sending valid registration requests

**ID_CONFLICT errors:**
```
ERROR: ID conflict for component '<name>'. ID <group>.<id> is already taken.
```
- Verify component IDs are unique across all components 
- Check if a component with the same ID is already registered
- Review component configuration files to ensure no duplicates

**MOST COMMON CAUSE - Component restart race condition:**
- Component crashes or loses network connection abruptly (no graceful disconnect)
- Old registry entry remains until keepalive timeout (15 seconds) expires
- Component restarts quickly (within 15 seconds) with same ID
- New registration request arrives while old entry still in registry → ID_CONFLICT error
- **Solution 1**: Wait 15-20 seconds after component crash before restarting
- **Solution 2**: Restart DISC service to clear all registrations
- **Solution 3**: Configure component with different temporary ID for testing
- After timeout expires, component can re-register successfully with original ID

**Invalid port errors:**
```
ERROR: Invalid listen port '<port>' for component '<name>'.
```
- Ensure `listenPort` field in registration request contains valid integer
- Port must be in range 1-65535

### 9.2 Connection & Keepalive Issues

**Clients timing out:**
```
WARN: Client '<name>' timed out. Last seen X.XX seconds ago.
```
- Verify client is sending keepalives every 5 seconds
- Check network connectivity between client and DISC
- Review client logs for keepalive transmission confirmation

**OM disconnection causing shutdown:**
- Expected behavior—DISC shuts down when OM disconnects
- Restart entire system: DISC → OM → Bridge → VHC/MEC
- Investigate why OM disconnected (check OM logs)

---


## APPENDIX A: Configuration Constants

### A.1 CMakeLists.txt Constants
Located in [DiscoveryService/CMakeLists.txt](../CMakeLists.txt):
```cmake
target_compile_definitions(DiscoveryService PRIVATE
    APP_NAME="DiscoverySvc"
    VERSION_MAJOR=1
    VERSION_MINOR=0
    VERSION_PATCH=0
    DISCOVERY_SERVICE_PORT=9090
    ACTIVE_LOG_LEVEL=LOG_LEVEL_INFO
)
```

### A.2 Runtime Constants
Located in [DiscoveryService/src/discovery_service.hpp](../src/discovery_service.hpp):
```cpp
static constexpr std::chrono::seconds KEEPALIVE_TIMEOUT{15};
```

Stale check interval: `KEEPALIVE_TIMEOUT / 2` (7.5 seconds)

---

## APPENDIX B: Component Registry Internals

### B.1 Registry Data Structures
The ComponentRegistry maintains three internal data structures:

**Primary Registry:**
```cpp
std::unordered_map<uint32_t, ComponentInfo> registry_by_client_id_;
```
Maps transport client_id to full ComponentInfo struct.

**ID Lookup Map:**
```cpp
std::map<std::pair<uint8_t, uint8_t>, uint32_t> client_id_by_component_id_;
```
Maps (group_id, id_in_group) pairs to client_id for conflict detection.

**Type-Specific Lists:**
```cpp
std::vector<uint32_t> bridge_client_ids_;
std::vector<uint32_t> om_client_ids_;
```
Maintains lists of Bridge and OM client IDs for round-robin selection.

### B.2 ComponentInfo Structure
Located in [DiscoveryService/src/component_registry.hpp](../src/component_registry.hpp):
```cpp
struct ComponentInfo {
    uint32_t client_id;                  // Transport layer ID
    ComponentType component_type;         // V, M, B, O, T
    uint8_t group_id;                    // First part of component_id
    uint8_t id_in_group;                 // Second part of component_id
    uint8_t component_subtype;           // Subtype (default 0)
    std::string name;                    // Human-readable name
    std::string listen_address;          // Auto-detected IP
    uint16_t listen_port;                // Component's listening port
    std::chrono::steady_clock::time_point last_seen;  // Last keepalive
};
```

### B.3 Round-Robin Selection
When multiple Bridges or OMs are registered:
- `find_available_bridge()` and `find_available_om()` use round-robin selection
- Internal index (`next_bridge_idx_`, `next_om_idx_`) tracks next component to return
- Wraps to 0 when reaching end of list

---

## APPENDIX C: System Integration

### C.1 Discovery Service in System Architecture
The Discovery Service is the **anchor point** of the entire offloading system:

```
                    [Discovery Service]
                    Fixed IP: 192.168.50.114:9090
                           |
        +------------------+------------------+
        |                  |                  |
    [OM]              [Bridge]           [VHC/MEC]
   Provides          Gets OM addr       Gets Bridge addr
   config            + config            + config
```

### C.2 Configuration Distribution Flow
1. OM starts with local config file
2. OM registers with DISC, sends config in `humanReadableMessage` field
3. DISC stores config in `global_configuration_` member
4. Bridge registers, receives empty config (doesn't need it)
5. VHC/MEC register, receive full config in `configJson` field
6. All components now have consistent configuration

### C.3 Changing Discovery Service IP
If deploying in a different network, the Discovery Service IP must be changed in:

1. **Bridge**: [bridge/src/BridgeControlPlane.hpp](../../bridge/src/BridgeControlPlane.hpp)
   ```cpp
   const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114";
   ```

2. **OM**: [OffloadingManager/algorithm.py](../../OffloadingManager/algorithm.py)
   ```python
   DISCOVERY_SERVICE_HOST = os.getenv("DISCOVERY_SERVICE_HOST", "192.168.50.114")
   ```

3. **ROS2 Launch Files**: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py](../../ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py)
   ```python
   DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
   ```

See main [README.md](../../README.md) APPENDIX A for complete list.

---

## APPENDIX D: Testing

### D.1 Test Scripts
The Discovery Service includes test utilities:

**Component Simulator:**
[DiscoveryService/test_component_simulator.py](../test_component_simulator.py) - Simulates component registration and keepalive

**Discovery Service Tests:**
[DiscoveryService/test_discovery_service.py](../test_discovery_service.py) - Unit tests for DISC functionality

### D.2 Manual Testing Procedure
```bash
# Terminal 1: Start DISC
cd /home/ubuntu/DiscoveryService
./autostart.sh

# Terminal 2: Test OM registration
python3 test_component_simulator.py --type O --name TestOM

# Terminal 3: Test Bridge registration
python3 test_component_simulator.py --type B --name TestBridge

# Terminal 4: Test VHC registration
python3 test_component_simulator.py --type V --name TestVHC
```

Expected behavior:
- OM registers successfully
- Bridge registers successfully (receives OM connection details)
- VHC registers successfully (receives Bridge connection details + config)
