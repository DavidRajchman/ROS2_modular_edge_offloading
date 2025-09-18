# DNS Service Component Connection Protocol - Simplified Specification

## 1. Registration Request Message Fields

### 1.1 Message Header
* **Magic String**: 
  * Value: "DNS:" (fixed)
  * Purpose: Identifies the message as part of the DNS protocol
  * Size: 4 bytes

* **Message Type**: 
  * Value: "REG" for Registration Request
  * Purpose: Identifies the message as a registration request
  * Size: 3 bytes

### 1.2 Component Information Fields

* **Component Type**: 
  * Purpose: Identifies the type of component requesting registration
  * Format: Single character code
  * Possible Values:
    * "V": Vehicle (VHC)
    * "M": Mobile Edge Computing Server (MEC)
    * "B": Bridge
    * "O": Offloading Manager (OM)
    * "T": Test/Development

* **ID Request Type**: 
  * Purpose: Specifies if component is requesting a specific ID or automatic assignment
  * Format: Single character code
  * Possible Values:
    * "A": Automatic ID assignment
    * "S": Static ID requested (specific ID provided)

* **Requested Group ID**: 
  * Purpose: Specifies the group ID part of the component's identifier
  * Format: Decimal number as text (0-255)
  * Common Values:
    * "2": Vehicle group
    * "12": MEC group
    * "5": Bridge group
  * Note: Only included when ID Request Type is "S"

* **Requested ID in Group**: 
  * Purpose: Specifies the instance ID within the group
  * Format: Decimal number as text (0-255)
  * Note: Only included when ID Request Type is "S"

* **Component Name**: 
  * Purpose: Human-readable identifier for the component
  * Format: Text string
  * Example: "VHC1_TEST", "BRIDGE_MAIN"

* **Listen Address**:
  * Format: "<IP>:<Port>"
  * Example: "192.168.1.100:8080"
  * Purpose: Where other components can reach this component

## 2. Registration Response Message Fields

* **Magic String**: "DNS:"
* **Message Type**: "ACK" (Success) or "ERR" (Error)

* **Response Code**: 
  * Purpose: Indicates success/failure of registration
  * Format: Single character code
  * Possible Values:
    * "0": Success - Registration complete
    * "W": Wait - Required component missing (e.g., waiting for Bridge or OM)
    * "C": Error - ID conflict
    * "I": Error - Invalid request
    * "E": Error - General failure

* **Assigned Group ID**: 
  * Purpose: Group ID assigned by DNS
  * Format: Decimal number as text (0-255)

* **Assigned ID in Group**: 
  * Purpose: Instance ID assigned by DNS
  * Format: Decimal number as text (0-255)

* **Connection Target** (if applicable):
  * Format: "<TYPE>;<IP>:<PORT>;<ID>"
  * Example: "B;192.168.1.15:9090;513" (Connect to Bridge with ID 513 at 192.168.1.15:9090)
  * Note: Only included when component needs to connect to another component

* **Configuration Data**: 
  * Purpose: JSON configuration string
  * Format: JSON text (can be empty)
  * Example: `{"timeout":30,"mode":"active"}`

* **Human-readable Message**:
  * Purpose: Description for logging/debugging
  * Format: Text string
  * Example: "Registration successful, connect to Bridge at 192.168.1.15:9090"

## 3. Keepalive Messages

### 3.1 Keepalive Request
* **Magic String**: "DNS:"
* **Message Type**: "PNG"
* **Component ID**: Combined group and in-group ID (e.g., "2.1" for group 2, ID 1)
* **Status**: "OK" or "ERR"

### 3.2 Keepalive Response
* **Magic String**: "DNS:"
* **Message Type**: "PON"
* **Response**: "OK" or "UPD" (update needed) or "DIS" (disconnect requested)

## 4. Error Message
* **Magic String**: "DNS:"
* **Message Type**: "ERR"
* **Error Code**: Single character:
  * "C": Configuration error
  * "N": Network error
  * "I": Internal error
  * "P": Protocol error
* **Message**: Human-readable error description

## 5. Example Messages

### Registration Request
```
DNS:REG;V;S;2;1;VHC1_TEST;192.168.1.100:8080;Registration request from Vehicle
```

### Registration Success Response
```
DNS:ACK;0;2;1;B;192.168.1.15:9090;513;{"bridge_ip":"192.168.1.15"};Registration successful, connect to Bridge
```

### Wait Response
```
DNS:ACK;W;2;1;;;;No Bridge available yet, please wait
```

### Error Response
```
DNS:ERR;C;2;1;;;;ID Conflict: Another component is already using ID 2.1
```

### Keepalive Request
```
DNS:PNG;2.1;OK;;Keepalive ping from Vehicle 2.1
```

### Keepalive Response
```
DNS:PON;OK;5000;;Continue normal operation, next ping in 5 seconds
```

## 6. Implementation Notes

1. **Field Separation**: All fields are separated by semicolons (;)
2. **Missing Fields**: Empty fields should be represented by consecutive semicolons
3. **Human Readability**: The protocol is designed to be easily readable in logs/captures
4. **Message Format**:
   - Fixed-length header ("DNS:" + message type)
   - Variable length fields separated by semicolons
   - Human-readable message as the last field
5. **ID Format**: 
   - In requests/responses: Group ID and In-Group ID as separate fields
   - In keepalives: Combined as "Group.ID" format
6. **Error Handling**: Keep error cases simple with descriptive text

This protocol provides the necessary functionality while being simple to implement and debug in a lab environment. The text-based format allows for easy manual inspection and troubleshooting.