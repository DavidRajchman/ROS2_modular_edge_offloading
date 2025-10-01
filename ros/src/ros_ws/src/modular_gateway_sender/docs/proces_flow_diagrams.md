**TO RENDER THE DIAGRAMS, USE A MARKDOWN VIEWER THAT SUPPORTS MERMAID SYNTAX, SUCH AS VS CODE WITH THE "Markdown Preview Mermaid Support" EXTENSION.**

## 1. OM Discovery Process
```mermaid
sequenceDiagram
participant OM as Offloading Manager
participant DS as Discovery Service

    Note over OM: OM starts with global config ready
    
    OM->>OM: __init__(): Create DiscoveryClient<br/>host=DISCOVERY_SERVICE_HOST, port=9090
    OM->>OM: connect_and_register(): Start registration process<br/>with exponential backoff (5min timeout)
    
    loop Registration Attempts (with backoff)
        OM->>+DS: socket.connect() to Discovery Service
        Note right of DS: New client connection detected<br/>onClientConnected() called
        
        OM->>DS: Send DISC:REG message
        Note right of DS: handleRegistration():<br/>Store global_configuration_ from OM message<br/>OM registers FIRST, provides config to all others
        
        DS->>-OM: Send DISC:ACK response
        Note left of OM: _parse_registration_response():<br/>responseCode=SUCCESS (0)<br/>Extract detected_ip, set registered=True
        
        alt Registration Success
            Note over OM: Registration completed successfully
            OM->>OM: _start_background_threads():<br/>- keepalive_thread: Send DISC:PNG every 30s<br/>- query_thread: Query for V/M components every 30s
            
            loop Keepalive Loop
                OM->>DS: Send DISC:PNG keepalive
                DS->>OM: Send DISC:PON response
            end
            
            loop Query Loop
                OM->>DS: Send DISC:QRY for MECs
                DS->>OM: Send DISC:QRS with MEC list
                Note left of OM: Update discovered_mecs set<br/>Notify decision_engine via callback
                
                OM->>DS: Send DISC:QRY for VHCs
                DS->>OM: Send DISC:QRS with VHC list
                Note left of OM: Update discovered_vhcs set
            end
        else Registration Failed (Connection/Timeout)
            Note over OM: Exponential backoff delay<br/>Retry with longer delay
        end
    end
```

## 2. Bridge Discovery Process
```mermaid
sequenceDiagram
participant Bridge as Bridge Control Plane
participant DS as Discovery Service
participant OM as Offloading Manager

    Note over Bridge: Bridge starts after compilation
    
    Bridge->>Bridge: start(): Begin startup sequence
    Bridge->>Bridge: register_with_discovery_service():<br/>Create TcpClientTransport to Discovery Service
    
    Bridge->>+DS: connect() to Discovery Service
    Bridge->>Bridge: perform_discovery_registration(): Build request
    
    loop Registration with Retry (until OM available)
        Bridge->>DS: Send DISC:REG message
        
        Note right of DS: handleRegistration():<br/>Check if OM is available (find_available_om)
        
        alt OM Available
            Note right of DS: OM found, populate OM connection info
            DS->>-Bridge: Send DISC:ACK with OM details
            
            Note left of Bridge: decode_message(): responseCode=SUCCESS<br/>Extract OM connection details<br/>Store global_configuration_
            Bridge->>Bridge: discovery_connected_.store(true)<br/>Start discovery_keepalive_thread
            
            Note over Bridge: Proceed to connect_to_om()
            Bridge->>+OM: Connect to OM using discovered address
            OM->>-Bridge: OM connection established
            
            loop Discovery Keepalive
                Bridge->>DS: Send DISC:PNG keepalive
                DS->>Bridge: Send DISC:PON response
            end
            
        else OM Not Available
            DS->>Bridge: Send DISC:ACK WAIT (No OM available)
            Note left of Bridge: responseCode=WAIT (1)<br/>OM not registered yet
            Bridge->>Bridge: Wait 5 seconds, then retry registration
        end
    end
    
    Note over Bridge: Once OM connected:<br/>start_mgwcp_server() - Ready for MGW connections
```

## 3. VHC/MEC MGW Discovery Process
```mermaid
sequenceDiagram
participant User as User/ROS2 Node
participant GC as GatewayController (in MGW)
participant DC as DiscoveryClient (in MGW)
participant DS as Discovery Service

    User->>+GC: Create GatewayController node
    GC->>GC: Constructor: Load ROS parameters<br/>discovery_host, port, component_type, etc.
    GC->>GC: Start control_thread_func()<br/>Initial state: INITIALIZING
    
    Note over GC: control_thread_func() state machine
    GC->>GC: State: DISCOVERING<br/>string_to_component_type(component_type_)
    
    GC->>+DC: Create DiscoveryClient()<br/>Set success/failure callbacks
    GC->>DC: start(host, port, comp_type, name, group_id, id_in_group, data_plane_port)
    
    DC->>DC: client_thread_func(): Launch dedicated thread
    DC->>+DS: TcpClientTransport.connect() to Discovery Service
    
    DC->>DS: Send DISC:REG message
    
    Note right of DS: handleRegistration():<br/>Check global_configuration_ available (from OM)<br/>Check bridge available (find_available_bridge)
    
    alt All Dependencies Available
        Note right of DS: Bridge found, global config available
        DS->>-DC: Send DISC:ACK with Bridge details
        
        Note left of DC: decode_message(): responseCode=SUCCESS<br/>Extract bridge_host, bridge_port, configJson
        DC->>DC: registered_.store(true)<br/>Invoke success_cb_()
        
        DC->>-GC: on_discovery_success(bridge_host, bridge_port, config_json)
        Note right of GC: load_global_config_from_json():<br/>Parse task catalog, populate task_database_
        GC->>GC: State: CONNECTING_TO_BRIDGE<br/>store bridge_cp_host_, bridge_cp_port_
        
        Note over GC: Continue to Bridge CP connection
        
        loop Keepalive Loop (in DiscoveryClient)
            DC->>DS: Send DISC:PNG keepalive
            DS->>DC: Send DISC:PON response
            Note left of DC: Update keepalive_interval_ms if provided
        end
        
    else Dependencies Missing
        alt Bridge Not Available
            DS->>DC: Send DISC:ACK WAIT (No Bridge available)
        else OM Not Available (No Global Config)
            DS->>DC: Send DISC:ACK WAIT (No OM config available)
        end
        
        Note left of DC: responseCode=WAIT (1)<br/>Registration denied temporarily
        DC->>GC: on_discovery_failure("Registration denied: reason")
        Note right of GC: State remains DISCOVERING<br/>control_thread will retry after delay
        
        GC->>GC: Wait discovery_retry_delay (5s)<br/>Then retry discovery process
    end
```
## 4. MGW Startup Procedure (After Discovery)
```mermaid
sequenceDiagram
participant MGWCP as MGW Control Plane
participant MGWDP as MGW Data Plane
participant BridgeCP as Bridge Control Plane
participant BridgeDP as Bridge Data Plane

    Note over MGWCP,BridgeDP: Prerequisites: Discovery complete, Bridge address known<br/>State: CONNECTING_TO_BRIDGE

    %% Phase 1: Data Plane Listening
    MGWCP->>MGWDP: Start data_plane_connection_thread_func()
    Note right of MGWDP: TcpServerTransport listening on<br/>data_plane_listen_port_
    MGWDP->>MGWDP: Gateway listening ready<br/>transport->get_transport() available

    %% Phase 2: Control Plane Connection
    MGWCP->>MGWCP: Create BridgeCpClient()<br/>Set callbacks: on_dp_confirmed, on_session_approved/denied
    MGWCP->>+BridgeCP: TcpClientTransport.connect()<br/>to bridge_cp_host:bridge_cp_port
    BridgeCP-->>-MGWCP: TCP connection established

    %% Phase 3: Immediate DP_INFO Transmission
    Note over MGWCP: start() successful, immediately send DP_INFO
    MGWCP->>+BridgeCP: DP_INFO (code 103)<br/>component_id, dp_host="0.0.0.0", dp_port
    BridgeCP-->>-MGWCP: ACK (code 900)
    Note over MGWCP: State: WAITING_FOR_DP_CONNECTION<br/>Block waiting for confirmation

    %% Phase 4: Bridge Processing DP_INFO
    Note right of BridgeCP: MGWCPConnection.handle_dp_info():<br/>1. Extract dp_host, dp_port from payload<br/>2. Use client_ip_ if dp_host="0.0.0.0"<br/>3. Call dp_connect_callback_
    BridgeCP->>BridgeCP: establish_mgwcp_data_plane_connection()<br/>create_transport_handler_for_mgwdp()

    %% Phase 5: Bridge Data Plane Connection
    BridgeCP->>+BridgeDP: Request connection to MGW DP<br/>at resolved IP and port
    BridgeDP->>+MGWDP: TCP connect to data plane
    MGWDP-->>-BridgeDP: accept_connection() successful
    BridgeDP-->>-BridgeCP: Data plane connection established

    %% Phase 6: Connection Confirmation
    Note right of BridgeCP: Data plane success:<br/>set_state(OPERATIONAL)<br/>send_dp_connection_confirmed()
    BridgeCP->>+MGWCP: DP_CONNECTION_CONFIRMED (code 202)
    MGWCP-->>-BridgeCP: ACK (code 900)

    %% Phase 7: MGW Becomes Operational
    Note right of MGWCP: on_dp_confirmed() callback invoked<br/>by BridgeCpClient
    MGWCP->>MGWCP: State: OPERATIONAL<br/>Begin accepting offloading requests<br/>ROS services now ready

    %% Phase 8: Session Management Setup (DISABLED)
    Note over BridgeCP: SessionManager maintenance thread running<br/>BUT: Session timeout cleanup DISABLED<br/>Line 108: expired_sessions.push_back COMMENTED OUT<br/>Sessions detected as expired but never removed

    Note over MGWCP,BridgeDP: MGW fully operational<br/>Ready for offloading request processing
```

## 5. High Level Offloading Request Flow
```mermaid
sequenceDiagram
participant User as User/ROS2 node
participant MGWCP as VHC MGW CP
participant MGWDP as VHC MGW DP
participant BridgeCP as BridgeCP
participant BridgeDP as BridgeDP
participant OM as Offloading Manager
participant MECCP as MEC MGW CP
participant MECDP as MEC MGW DP

    %% Assume discovery complete, Bridge CP connections established
    Note over User,MECDP: Assume: Discovery complete, MGW↔Bridge connections operational

    %% Phase 1: ROS Service Call
    User->>+MGWCP: RequestOffloading(task_id, task_name)
    Note right of MGWCP: offloading_request_service_handler():<br/>1. Validate task_id exists in database<br/>2. Generate unique request_id<br/>3. Queue to offloading_request_queue_<br/>4. Return response immediately
    MGWCP-->>-User: Response(success=true, request_id)

    %% Phase 2: Control Thread Processing
    Note over MGWCP: control_thread_func() dequeues request
    MGWCP->>MGWCP: Create session in active_sessions_ map<br/>Store: request_id, task_id, state
    MGWCP->>+BridgeCP: OFFLOAD_REQUEST (code 100)<br/>request_id: X, task_id: Y, task_name: "..."
    BridgeCP-->>-MGWCP: ACK (code 900, ack_sequence_number)

    %% Phase 3: Bridge Forwarding
    Note right of BridgeCP: MGWCPConnection receives message<br/>Forwards to OM via message_forwarder_
    BridgeCP->>+OM: OFFLOAD_REQUEST (code 100)<br/>Forward complete message

    %% Phase 4: OM Decision
    Note right of OM: _handle_offload_request():<br/>1. Call decision_engine.make_decision()<br/>2. Select target MEC or deny
    alt IN CASE: Request Approved
        OM->>-BridgeCP: SESSION_APPROVED (code 200)<br/>request_id: X, assigned_mec_id: "10:5"
        Note right of BridgeCP: BridgeControlPlane routes by<br/>component_id to correct MGWCPConnection
        BridgeCP->>+MGWCP: SESSION_APPROVED (code 200)<br/>Forward to requesting VHC
        MGWCP-->>-BridgeCP: ACK (code 900)
        
        %% Phase 5: VHC Handler Activation
        Note right of MGWCP: on_session_approved() callback:<br/>1. Look up task_id from session<br/>2. Retrieve TaskDetails from database<br/>3. Activate handlers via HandlerFactory
        MGWCP->>MGWCP: For each input_type:<br/>  handler = factory.get_or_create(type)<br/>  configure_handler_mode(SUBSCRIBER_ONLY)<br/>  gateway->register_handler(handler)
        MGWCP->>MGWCP: For each output_type:<br/>  handler = factory.get_or_create(type)<br/>  configure_handler_mode(PUBLISHER_ONLY)<br/>  gateway->register_handler(handler)
        
        Note right of MGWCP: Handlers now active:<br/>- Subscribers listen to local ROS topics<br/>- Publishers ready to receive from network
        
        %% Phase 6: MEC Session Establishment
        Note over BridgeCP,MECCP: Bridge initiates session with assigned MEC
        BridgeCP->>+MECCP: SESSION_APPROVED (code 200)<br/>request_id: MEC_req_id, task_id: Y
        MECCP-->>-BridgeCP: ACK (code 900)
        
        %% Phase 7: MEC Handler Activation
        Note right of MECCP: on_session_approved() callback:<br/>Activate handlers in reverse mode
        MECCP->>MECCP: For each input_type:<br/>  configure_handler_mode(PUBLISHER_ONLY)<br/>For each output_type:<br/>  configure_handler_mode(SUBSCRIBER_ONLY)
        
        %% Phase 8: Data Plane Connection
        Note over BridgeDP,MECDP: Bridge DP establishes connection to MEC DP
        BridgeDP->>+MECDP: TCP connect (from DP_INFO payload)
        MECDP-->>-BridgeDP: Connection established
        
        %% Phase 9: First Data Transmission
        Note over User,MECDP: Data plane now ready for message flow
        User->>MGWCP: Publishes ROS message to offloaded topic
        Note right of MGWCP: Handler's ROS subscription callback:<br/>1. handle_message() invoked<br/>2. Calls send_message() → gateway_
        MGWCP->>MGWDP: RosGateway.send_message()<br/>Formats: magic(0xA5C3) + flags + type + IDs +<br/>payload_size + topic_len + topic + payload
        MGWDP->>BridgeDP: Send data plane message<br/>(TCP transport, complete header+payload)
        Note right of BridgeDP: Routes by (ID Group, ID in Group) or<br/>(ID, Message Type) to determine destination
        BridgeDP->>MECDP: Forward data plane message
        Note right of MECDP: receive_and_process_message():<br/>1. Parse header (magic, type, IDs, topic)<br/>2. Read payload_size bytes<br/>3. Find handler: can_process_message_type(type)<br/>4. handler.process_and_publish_received_msg()
        Note right of MECDP: ROS message now available on MEC
        
    else IN CASE: Request Denied
        OM->>BridgeCP: SESSION_DENIED (code 201)<br/>request_id: X, reason_code: 4001
        BridgeCP->>MGWCP: SESSION_DENIED (code 201)
        MGWCP-->>BridgeCP: ACK (code 900)
        Note right of MGWCP: on_session_denied() callback:<br/>Clean up session, notify application
    end
```


