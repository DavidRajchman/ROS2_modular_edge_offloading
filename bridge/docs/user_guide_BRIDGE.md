# Bridge Control Plane User Guide

## 0. NAMING CONVENTION
Core terms used in the Bridge codebase:
* **Bridge** / **BridgeCP** – Bridge Control Plane application that coordinates message routing between components
* **component_id** – Unique identifier for components in format `group_id:id_in_group` (e.g., `15:10`)
* **VHC** – Vehicle Host Component that connects to Bridge for offloading requests
* **MEC** – Mirror Execution Component that receives offloaded tasks
* **OM** – Offloading Manager that makes approval/denial decisions for offloading requests
* **MGWCP** – Modular Gateway Control Plane protocol for session management
* **MGWDP** – Modular Gateway Data Plane for binary message forwarding
* **DiscoveryService** – Registration service that provides OM connection details and configuration
* **session** – Active offloading session between VHC and MEC, managed by Bridge
* **routing_table** – Internal data structure mapping message routes to transport handlers
* **transport_handler** – Component managing TCP connections and message forwarding to gateways
* **keepalive** – Periodic messages maintaining connection liveness (Discovery and session-level)

---

## 1. INTRODUCTION & SCOPE
This user guide covers the Bridge, a standalone C++ application that acts as a central router and a connection point in the offloading ecosystem. The Bridge Control Plane facilitates communication between Vehicle Host Components (VHCs) requesting task offloading and Offloading Managers (OMs) that approve or deny those requests. It also manages connections to MECs that execute the offloaded tasks. The Bridge Data Plane handles the actual message forwarding between connected VHC and MEC gateways based on routing rules established during session approval.


Bridge responsibilities:
* Register with DiscoveryService to obtain OM connection details
* Connect to Offloading Manager (OM) for session approval/denial
* Manage MGWCP server for gateway connections (VHC/MEC gateways)
* Forward offloading requests from VHCs to OM for approval/denial decisions
* Establish data plane routing between approved VHC-MEC pairs


The Bridge operates as a message broker and does not make offloading decisions (that's the OM's role) or execute tasks (that's the MEC's role).

---

## 2. HIGH-LEVEL ARCHITECTURE
Bridge startup sequence:
1. **Initialization**: Configure logging system, create core components (RoutingTable, SessionManager)
2. **Discovery Registration**: Connect to DiscoveryService and obtain OM connection details
3. **OM Connection**: Establish persistent connection to Offloading Manager
4. **MGWCP Server**: Start server to accept connections from VHC/MEC gateways
5. **Operational**: Handle requests, route messages

Message flow (VHC offloading request):
1. VHC gateway connects to Bridge MGWCP server
2. VHC sends OFFLOAD_REQUEST via MGWCP → Bridge forwards to OM
3. OM responds with SESSION_APPROVED/SESSION_DENIED → Bridge forwards to VHC
4. On approval: Bridge creates routing rules for VHC↔MEC data plane communication
5. Data plane messages flow through Bridge routing table to appropriate destinations

See [ros/src/ros_ws/src/modular_gateway_sender/docs/process_flow_diagrams.md](../../ros/src/ros_ws/src/modular_gateway_sender/docs/process_flow_diagrams.md) for detailed process flow diagrams.

Key components & files:
* Main coordinator: `src/BridgeControlPlane.cpp` / `src/BridgeControlPlane.hpp`
* Entry point: `src/main.cpp`
* MGWCP connections: `src/MGWCPConnection.cpp` / `src/MGWCPConnection.hpp`
* Data plane routing: `src/RoutingTable.cpp` / `src/TransportHandler.cpp`
* Session tracking: `src/SessionManager.cpp` **CURRENTLY DISABLED** (sessions are tracked but cleanup is not performed)
* Configuration: `src/GlobalConfig.cpp`
* Logging system: `src/configure_logger.cpp`

---

## 3. BUILD & INSTALLATION
Prerequisites: C++17 compiler, CMake 3.10+, external libraries (CppLogging, fmt, nlohmann/json)

Basic build steps:
1. Navigate to bridge directory: `cd /home/ubuntu/bridge`
2. Create build directory: `mkdir -p build && cd build`
3. Configure with desired logging mode (if needed, default binary mode offers best performance): `cmake -DBRIDGE_LOG_MODE=TEXT ..`
4. Build: `make -j$(nproc)`
5. Run: `./bin/bridge`

Available logging configurations:
```bash
# Binary logging (high performance, requires decoder)
cmake -DBRIDGE_LOG_MODE=BINARY ..

# Text logging (direct console output, higher latency)
cmake -DBRIDGE_LOG_MODE=TEXT ..

# With specific log levels
cmake -DBRIDGE_LOG_MODE=TEXT -DBRIDGE_LOG_LEVEL_PRESET=DEBUG ..
cmake -DBRIDGE_LOG_MODE=BINARY -DBRIDGE_LOG_LEVEL_PRESET=INFO ..
```

Binary output location: `build/bin/bridge`

---

## 4. RUNTIME CONFIGURATION
The Bridge uses compile-time constants for core configuration (defined in [bridge/src/BridgeControlPlane.hpp](../src/BridgeControlPlane.hpp)):

**Fixed Configuration:**
* Component ID: `15:10` (BRIDGE_COMPONENT_ID)
* OM Port: `8100` (OM_PORT) 
* Discovery Service: `192.168.50.114:9090` (DISCOVERY_SERVICE_HOST/PORT)
* MGWCP Server Port: `7000` (MGWCP_SERVER_PORT)

**Runtime Behavior Constants:**
* Discovery keepalive interval: 5 seconds
* Session timeout: 20 seconds  
* Maintenance check interval: 10 seconds
* Maximum MGWCP connections: 50

**Dynamic Configuration (from DiscoveryService):**
* OM host address (retrieved during Discovery registration)
* OM port (if different from default)

To modify configuration: Edit constants in [bridge/src/BridgeControlPlane.hpp](../src/BridgeControlPlane.hpp) and rebuild.

Future enhancement: Configuration file support for runtime parameter changes.

---

## 5. PROTOCOL & MESSAGE BASICS
The Bridge handles three distinct communication protocols:

**Discovery Protocol** (Bridge ↔ DiscoveryService):
* Registration requests/responses for OM connection details
* Periodic keepalives to maintain registration

**MGWCP Protocol** (Bridge ↔ VHC/MEC Gateways):
* Connection management and session negotiation
* Request forwarding and response routing
* Data plane establishment coordination

**MGWDP Protocol** (Bridge ↔ VHC/MEC Gateways):
* Binary message forwarding for offloaded tasks
* Based on a binary coded header for low data overhead

Message routing is based on:
* Source component_id for request tracking
* Transport handler mappings in routing table


---

## 6. DATA PLANE ROUTING
The Bridge implements message routing through TransportHandler and RoutingTable:

**Routing Table Structure:**
* Key: `(source_id, message_type)` tuple
* Value: Target transport handler (connection to destination)
* Bidirectional entries for active sessions

**Transport Handler Management:**
* One handler per unique destination (VHC/MEC gateway)
* TCP connection pooling and reconnection logic
* Message queue management for reliable delivery

**Message Flow:**
1. Message arrives at Bridge from source gateway
2. Routing table lookup using `(source_id, message_type)`
3. Forward to target transport handler
4. Delivery to destination gateway

**Route Creation Process:**
When session approved:
1. Create TransportHandler for MGWDP connection to VHC gateway
2. Create TransportHandler for MEC gateway (if not exists)
3. Add routing rules: VHC input_types → MEC, MEC output_types → VHC
4. Enable bidirectional message flow

---

## 7. LOGGING & DIAGNOSTICS
The Bridge uses a modular logging system with build-time mode selection:

### 7.1 Logging Framework
* **Library**: CppLogging with AsyncWaitFreeProcessor
* **Performance**: Binary mode ~5μs/message, text mode ~200μs/message
* **Output**: Console + file (`bridge_text.log` or `bridge_binary.log`) output is located in the build directory
* **Format**: All messages include source file prefix for filtering

### 7.2 Log Levels & Configuration
**Available levels** (hex values):
* NONE (0x00), FATAL (0x1F), ERROR (0x3F), WARN (0x7F), INFO (0x9F), DEBUG (0xBF), ALL (0xFF)

**Build-time configuration:**
```bash
# Preset levels
cmake -DBRIDGE_LOG_MODE=TEXT -DBRIDGE_LOG_LEVEL_PRESET=DEBUG ..

# Custom hex level
cmake -DBRIDGE_LOG_MODE=BINARY -DBRIDGE_LOG_LEVEL=0xBF ..
```

**Default configuration**: Binary mode with INFO level (0x9F)

### 8.3 Log Message Structure
Console output format (text mode):
```
YYYY-MM-DDTHH:MM:SS.nnnZ [0xTHREAD] LEVEL logger_name - file.cpp: message content
```

Example:
```
2025-09-18T15:33:07.159Z [0x7F610180] INFO bridge - main.cpp: Bridge Control Plane starting...
```

### 8.4 Converting Binary Logs to Text
**Tool**: BinLogDecoder.py (workspace root)

See [BinLogDecoder.py](../../BinLogDecoder.py)

Usage:
```bash
cd /home/ubuntu/bridge
python3 BinLogDecoder.py build/bridge_binary.log bridge_decoded.txt
#Arguments not needed unless custom input/output files are used
python3 BinLogDecoder.py

```

**Important**: Update decoder format mappings when new logging patterns are added.

### 7.5 Key Log Messages for Debugging
**Startup sequence:**
* `Bridge Control Plane starting...`
* `Registering with Discovery Service...`
* `Successfully registered with Discovery Service`
* `Connected to OM...`
* `MGWCP server started...`

**Session management:**
* `Creating routing rules for session request_id=...`
* `Session approved: VHC=... MEC=...`
* `Routing table updated with bidirectional rules`

**Error patterns:**
* `Failed to register with Discovery Service`
* `No route found for SourceID=... MsgType=...`
* `Session timeout for request_id=...`


---

## 8. TROUBLESHOOTING GUIDE

### 8.1 Common Startup Issues
**Discovery Service connection failures:**
* Check network connectivity to `192.168.50.114:9090`
* Verify DiscoveryService is running and accepting connections
* Review logs for "Failed to connect to Discovery Service"

**OM connection failures:**
* Ensure DiscoveryService provides valid OM address
* Check OM is running and listening on specified port
* Look for "Failed to connect to OM" log messages

### 8.3 Data Plane Issues

**Connection drops:**
* Monitor TCP connection stability
* Check for network interruptions
* Review transport handler reconnection attempts

### 8.4 Diagnostic Commands
**Check process status:**
```bash
ps aux | grep bridge
netstat -tlnp | grep bridge
```

**Monitor log files:**
```bash
tail -f bridge_text.log | grep -E "(ERROR|WARN)"
```

**Decode binary logs:**
```bash
python3 BinLogDecoder.py bridge_binary.log | tail -100
```

---


---

## APPENDIX A: Configuration Constants
Located in [bridge/src/BridgeControlPlane.hpp](../src/BridgeControlPlane.hpp):
```cpp
const uint16_t OM_PORT = 8100;
const std::string BRIDGE_COMPONENT_ID = "15:10";
const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114";
const uint16_t DISCOVERY_SERVICE_PORT = 9090;
const uint16_t MGWCP_SERVER_PORT = 7000;

// Timing constants
static constexpr std::chrono::seconds DISCOVERY_KEEPALIVE_INTERVAL_{5};
static constexpr std::chrono::seconds SESSION_TIMEOUT_{20};
static constexpr std::chrono::seconds MAINTENANCE_INTERVAL_{10};
static constexpr size_t MAX_MGWCP_CONNECTIONS = 50;
```

## APPENDIX B: Build Options Reference
```bash
# Logging mode selection
-DBRIDGE_LOG_MODE=BINARY          # High performance binary output
-DBRIDGE_LOG_MODE=TEXT            # Human readable console output

# Log level presets
-DBRIDGE_LOG_LEVEL_PRESET=NONE    # No logging
-DBRIDGE_LOG_LEVEL_PRESET=FATAL   # Fatal errors only
-DBRIDGE_LOG_LEVEL_PRESET=ERROR   # Errors and above
-DBRIDGE_LOG_LEVEL_PRESET=WARN    # Warnings and above
-DBRIDGE_LOG_LEVEL_PRESET=INFO    # Info and above (default)
-DBRIDGE_LOG_LEVEL_PRESET=DEBUG   # All messages including debug
-DBRIDGE_LOG_LEVEL_PRESET=ALL     # Maximum verbosity

# Custom log level (hex mask)
-DBRIDGE_LOG_LEVEL=0x9F           # Custom level specification
```

Complete build examples:
```bash
# Production build
cmake -DBRIDGE_LOG_MODE=BINARY -DBRIDGE_LOG_LEVEL_PRESET=INFO ..

# Development build  
cmake -DBRIDGE_LOG_MODE=TEXT -DBRIDGE_LOG_LEVEL_PRESET=DEBUG ..

# Minimal logging build
cmake -DBRIDGE_LOG_MODE=BINARY -DBRIDGE_LOG_LEVEL_PRESET=WARN ..
```
