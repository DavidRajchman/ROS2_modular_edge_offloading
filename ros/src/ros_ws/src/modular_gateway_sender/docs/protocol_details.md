## Modular Gateway Protocol Details

### Data plane protocol
This protocol is used to encaplsulate ROS 2 messages for transmission between Modular Gateway components (e.g., VHCs, MECs, Bridge CP). It is handled exclusively by MGW (VHC and MEC) and the Bridge DP can read the headers for routing purposes.
#### Message Header Structure

The header is a minimum of 11 bytes, plus the length of the topic name. All multi-byte integer fields are transmitted in **big-endian** order.

| Offset (bytes) | Length (bytes) | Field                 | Description                                                                                                |
| :------------- | :------------- | :-------------------- | :--------------------------------------------------------------------------------------------------------- |
| 0              | 2              | Magic Number          | Always `0xA5C3`. Identifies the start of a Modular GW message.                                             |
| 2              | 1              | Flags                 | A bitmask indicating various message properties and protocol version. See section 3.1.1.                   |
| 3              | 1              | Message Type          | An 8-bit unsigned integer identifying the type of the payload. See section 3.2.                            |
| 4              | 1              | ID Group              | An 8-bit identifier for the group of the sender (e.g., VHC DNS-assigned, VHC manually-assigned, MEC, etc.). |
| 5              | 1              | Identifier in Group   | An 8-bit unique identifier of the sender within its `ID Group`.                                            |
| 6              | 4              | Payload Size          | A 32-bit unsigned integer representing the size of the message payload (following the topic name) in bytes.  |
| 10             | 1              | Topic Length          | An 8-bit unsigned integer representing the length of the `Topic Name` string in bytes (max 255).           |
| 11             | `Topic Length` | Topic Name            | The ROS 2 topic name associated with the message, UTF-8 encoded.                                           |
| 11 + `Topic Length` | `Payload Size` | Payload             | The actual message data. Its interpretation depends on the `Message Type`.                                 |

**Note for Routing Intermediaries (e.g., a Bridge):** While the `Topic Name` is essential for the end-point Modular GWs to interact with their respective ROS 2 environments, an intermediary system designed for high-speed routing might primarily use a combination of `ID Group`, `Identifier in Group` (as a composite source ID), and the `Message Type` byte as its primary key for routing decisions. This assumes a system-level convention or external configuration (e.g., from an Orchestrator) that maps these key components to specific data flows, especially when a unique `(Source ID, Message Type)` pair consistently corresponds to a single logical data stream (e.g., a specific ROS 2 topic) for routing purposes. The full `Topic Name` would still need to be parsed by such an intermediary to correctly determine the total message length and forward the message intact.

##### Flags Byte (not all implemented in MGW)

The Flags byte is an 8-bit field where each bit (or group of bits) has a specific meaning:

*   **Bit 7 (MSB): Timestamp (`0x80`)**: If set, the message payload is expected to contain or be prefixed by timestamp information relevant to the message type. The exact format of this timestamp is dependent on the `Message Type` and how the corresponding handler processes it.
*   **Bit 6: Fragmented (`0x40`)**: If set, the message is part of a larger message that has been fragmented. (Note: Full fragmentation logic and reassembly might have specific handler dependencies not detailed here).
*   **Bit 5: Serialized (`0x20`)**: If set, the message payload contains data that has been serialized using ROS 2 serialization mechanisms (e.g., CDR). If not set, the payload might be a more raw or custom format. This is crucial for handlers to correctly interpret/reconstruct ROS 2 messages.
*   **Bit 4: Checksum (`0x10`)**: If set, the message includes a checksum for validation. (Note: The checksum calculation method and its location relative to the payload are not defined at this general protocol level and would be specific to implementations using this flag).
*   **Bit 3: Compressed (`0x08`)**: If set, the message payload is compressed. (Note: The compression algorithm is not defined at this general protocol level).
*   **Bits 0-2: Version (`0x07`)**: A 3-bit field representing the protocol version (values 0-7). Currently, version `0` is standard.

##### Message Payload

The content and structure of the `Payload` field are determined by the `Message Type` field in the header and the `Serialized` flag. If the `Serialized` flag is set, the payload is typically the CDR-serialized form of a ROS 2 message.

---

### Control Plane Protocol (Bridge ↔ MGW ↔ OM)

**Transport:** TCP with JSON messages (UTF-8)  
**Reliability:** Hop-by-hop ACK/retry on MGW-Bridge link only (Bridge-OM assumed stable)  
**Framing:** Implementation-specific (length-prefix or delimiter)

#### Message Envelope Structure

All control plane messages use this JSON structure:

```json
{
  "component_id": "group_id:id_in_group",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 102,
  "payload": { }
}
```

**Fields:**
- `component_id` (string, required): Format "group_id:id_in_group" (e.g., "10:12"). Range: 0-255 for both parts.
- `message_code` (int, required): Numeric identifier (100-199: MGW→Bridge, 200-299: Bridge→MGW, 300-399: Bridge→OM, 900-999: Control).
- `message_type` (string, required): Human-readable type matching message_code.
- `sequence_number` (int, required): Monotonically increasing per connection, starts at 1.
- `payload` (object, required): Message-specific JSON object (may be empty `{}`).

#### Message Codes and Payloads

| Code | Type | Direction | Payload |
|------|------|-----------|---------|
| 100 | OFFLOAD_REQUEST | MGW→Bridge→OM | `{"request_id": int\|str, "task_id": int, "task_name": str}` |
| 101 | SESSION_KEEPALIVE | MGW→Bridge→OM | `{"request_id": int\|str}` |
| 102 | SESSION_TERMINATE_REQUEST | MGW→Bridge→OM | `{"request_id": int\|str}` |
| 103 | DP_INFO | MGW→Bridge | `{"dp_host": str, "dp_port": int}` *First message after TCP connect* |
| 200 | SESSION_APPROVED | OM→Bridge→MGW | `{"request_id": int\|str, "assigned_mec_id": str}` *(optional field)* |
| 201 | SESSION_DENIED | OM→Bridge→MGW | `{"request_id": int\|str, "reason_code": int, "reason_description": str}` |
| 202 | DP_CONNECTION_CONFIRMED | Bridge→MGW | `{}` *(empty payload)* |
| 301 | BRIDGE_DP_FAILURE | Bridge→OM | `{"request_id": int\|str, "reason": str}` |
| 900 | ACK | Bidirectional | `{"ack_sequence_number": int}` |

#### Reason Codes (SESSION_DENIED)

| Code | Meaning | Description |
|------|---------|-------------|
| 4001 | INSUFFICIENT_RESOURCES | No available MEC resources |
| 4002 | TASK_NOT_FOUND | Task_id not recognized |
| 4003 | SESSION_TIMEOUT | Keepalive timeout (>20s) |
| 4004 | INVALID_REQUEST | Malformed request |
| 4005 | FINISHED_SESSION | Graceful termination |

#### ACK/Retry Mechanism

**Configuration:**
- ACK timeout: 5 seconds
- Max retries: 3 attempts
- Total max delivery: ~15 seconds

**Behavior:**
1. Sender assigns sequence_number, stores message, sends, starts timer
2. Receiver sends ACK immediately with `ack_sequence_number` matching received `sequence_number`
3. Sender removes from pending on ACK receipt
4. On timeout: retry with same sequence_number (up to 3 times)
5. ACK messages have `sequence_number: 0` and don't require acknowledgment

**Sequence Numbers:**
- Per-connection, independent spaces
- Each direction (MGW→Bridge, Bridge→MGW) has own counter
- Start at 1, increment by 1 per message
- Only meaningful within single TCP connection

#### Timing Constants

- SESSION_KEEPALIVE interval: 15 seconds
- SESSION_TIMEOUT: 20 seconds (no keepalive)
- ACK timeout: 5 seconds
- Max retry count: 3

#### Special Behaviors

**DP_INFO (103):**
- Always first message after TCP connection
- If `dp_host` is "0.0.0.0", Bridge uses TCP source IP
- Triggers Bridge data plane connection attempt

**Component ID:**
- Format: "group_id:id_in_group"
- Must remain consistent across all messages from same component
- Bridge uses for response routing

---

### Discovery Service Protocol

**Transport:** TCP, text-based messages  
**Format:** Semicolon-delimited fields, prefixed with `DISC:`  
**Library:** `discovery_protocol::` namespace (encode/decode functions provided)

#### Message Types

**Registration Request (Client → Service):**
```
DISC:REG;<component_type>;<id_request_type>;<group_id>;<id_in_group>;<component_name>;<message>
```

**Registration Response (Service → Client):**
```
DISC:ACK;<response_code>;<component_id>;<bridge_cp_host>;<bridge_cp_port>;<om_host>;<om_port>;<config_json>;<client_ip>;<message>
```

**Keepalive Ping (Client → Service):**
```
DISC:PNG;<component_id>;<timestamp>;<message>
```

**Keepalive Pong (Service → Client):**
```
DISC:PON;<response>;<timestamp>;<message>
```

**Error Message (Either direction):**
```
DISC:ERR;<error_code>;<message>
```

#### Enums and Codes

**ComponentType:** `V` (Vehicle), `M` (MEC), `B` (Bridge), `O` (OM), `T` (Test)  
**IdRequestType:** `A` (Automatic), `S` (Static)  
**ResponseCode:** `SUCCESS`, `DUPLICATE_ID`, `INVALID_REQUEST`, `SERVER_ERROR`, `GENERAL_ERROR`  
**ErrorCode:** `CONFIGURATION`, `TRANSPORT`, `PARSING`, `PROTOCOL`

#### Usage Pattern

1. Client connects to Discovery Service (fixed IP/port)
2. Client sends DISC:REG
3. Service responds with DISC:ACK containing Bridge CP address and client's public IP
4. Client sends periodic DISC:PNG (keepalive)
5. Service responds with DISC:PON

**Notes:**
- Discovery Service is the only component with fixed/known address
- Provides Bridge CP address to all components
- Auto-detects client IP from TCP connection
- Distributes global configuration JSON to all components

```


