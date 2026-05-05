**TO RENDER THE DIAGRAMS, USE A MARKDOWN VIEWER THAT SUPPORTS MERMAID SYNTAX, SUCH AS VS CODE WITH THE "Markdown Preview Mermaid Support" EXTENSION.**

## 1. P2P Mode (Pure) Process Flow
This mode operates entirely without a Discovery Service, Bridge, or Offloading Manager. The VHC and MEC connect directly using statically configured IP addresses and ports. Session approvals are simulated locally based on a shared configuration file.

```mermaid
sequenceDiagram
participant User as User/ROS2 Node
participant VHC_GC as VHC Gateway Controller
participant VHC_DP as VHC Data Plane
participant MEC_DP as MEC Data Plane
participant MEC_GC as MEC Gateway Controller

    Note over VHC_GC,MEC_GC: Both nodes start with operation_mode="p2p"<br/>and load local p2p_config.json
    
    %% Phase 1: Initialization
    VHC_GC->>VHC_GC: load_local_config("p2p_config.json")
    MEC_GC->>MEC_GC: load_local_config("p2p_config.json")
    
    Note right of MEC_GC: MEC acts as TCP Server<br/>Listens on data_plane_listen_port
    MEC_GC->>MEC_DP: Start TcpServerTransport
    
    Note left of VHC_GC: VHC acts as TCP Client<br/>Target is static p2p_peer_host:p2p_peer_port
    VHC_GC->>VHC_DP: Start TcpClientTransport
    
    %% Phase 2: Connection
    VHC_DP->>+MEC_DP: TCP Connect
    MEC_DP-->>-VHC_DP: Connection Established
    
    %% Phase 3: Data Plane Confirmation & Auto-Activation
    Note left of VHC_GC: on_dp_confirmed() triggered
    VHC_GC->>VHC_GC: State: OPERATIONAL
    VHC_GC->>VHC_GC: activate_all_p2p_sessions()
    
    Note right of MEC_GC: on_dp_confirmed() triggered
    MEC_GC->>MEC_GC: State: OPERATIONAL
    MEC_GC->>MEC_GC: activate_all_p2p_sessions()
    
    %% Phase 4: Local Auto-Approval
    Note over VHC_GC,MEC_GC: For each task in local config:
    
    VHC_GC->>VHC_GC: on_session_approved(mock_payload)
    VHC_GC->>VHC_GC: Activate handlers (SUBSCRIBER/PUBLISHER)
    
    MEC_GC->>MEC_GC: on_session_approved(mock_payload)
    MEC_GC->>MEC_GC: Activate handlers (REVERSE MODE)
    
    %% Phase 5: Data Flow
    Note over User,MEC_DP: ROS Data Flow begins directly between VHC and MEC
    User->>VHC_GC: Publish ROS Message
    VHC_GC->>VHC_DP: Serialize and send
    VHC_DP->>MEC_DP: Binary message over TCP
    MEC_DP->>MEC_GC: Deserialize and publish
    Note right of MEC_GC: ROS message available on MEC
```

## 2. P2P_DS Mode Process Flow
This mode utilizes the Discovery Service for dynamic IP/Port matchmaking between the VHC and MEC, but still bypasses the Bridge and Offloading Manager. Sessions are still simulated locally based on a shared configuration file.

```mermaid
sequenceDiagram
participant VHC_GC as VHC Gateway Controller
participant VHC_DC as VHC Discovery Client
participant DS as Discovery Service
participant MEC_DC as MEC Discovery Client
participant MEC_GC as MEC Gateway Controller

    Note over VHC_GC,MEC_GC: Both nodes start with operation_mode="p2p_ds"<br/>and load local p2p_config.json
    
    %% Phase 1: Initialization
    VHC_GC->>VHC_GC: load_local_config("p2p_config.json")
    MEC_GC->>MEC_GC: load_local_config("p2p_config.json")
    
    Note right of MEC_GC: MEC acts as TCP Server<br/>Listens on data_plane_listen_port
    MEC_GC->>MEC_GC: Start TcpServerTransport
    
    %% Phase 2: MEC Registration
    MEC_GC->>MEC_DC: Start Discovery Registration
    MEC_DC->>+DS: Send DISC:REG (type=MEC, groupId=X, idInGroup=Y)
    Note right of DS: DS registers MEC and stores its IP/Port
    DS-->>-MEC_DC: Send DISC:ACK (SUCCESS)
    MEC_DC->>MEC_GC: on_discovery_success()
    
    %% Phase 3: VHC Registration & Matchmaking
    VHC_GC->>VHC_DC: Start Discovery Registration
    
    loop Matchmaking Retry
        VHC_DC->>+DS: Send DISC:REG (type=VHC, groupId=X, idInGroup=Y)
        Note right of DS: DS searches for MEC with matching<br/>groupId and idInGroup
        
        alt MEC Found
            Note right of DS: Match successful
            DS-->>-VHC_DC: Send DISC:ACK (SUCCESS, MEC IP/Port)
        else MEC Not Found
            Note right of DS: Match failed (MEC offline)
            DS-->>VHC_DC: Send DISC:ACK (WAIT)
            Note left of VHC_DC: Wait 2 seconds and retry
        end
    end
    
    %% Phase 4: Dynamic Target Assignment
    Note left of VHC_DC: Match successful
    VHC_DC->>VHC_GC: on_discovery_success(MEC_IP, MEC_PORT)
    VHC_GC->>VHC_GC: transport->set_target(MEC_IP, MEC_PORT)
    VHC_GC->>VHC_GC: State: WAITING_FOR_DP_CONNECTION
    
    %% Phase 5: P2P Connection & Data Flow
    Note over VHC_GC,MEC_GC: VHC TcpClientTransport connects to MEC TcpServerTransport
    
    VHC_GC->>MEC_GC: TCP Connect (Direct)
    Note over VHC_GC,MEC_GC: on_dp_confirmed() triggers activate_all_p2p_sessions()
    
    Note over VHC_GC,MEC_GC: Handlers activated locally
    Note over VHC_GC,MEC_GC: ROS Data Flow begins directly between VHC and MEC
```
