# Discovery Service Integration Guide

## 1. Overview

The Discovery Service is a centralized TCP server responsible for managing the registration and discovery of all components within the system (Vehicles, MECs, Bridges, etc.). Its primary functions are:

*   **Component Registration:** To maintain a real-time registry of all active components, their network addresses, and their roles.
*   **Configuration Distribution:** To receive a global system configuration from the Offloading Manager (OM) and distribute it to all other components as they register.
*   **Service Discovery:** To provide connecting components (like Vehicles and MECs) with the necessary connection details for other components (specifically, the Bridge).
*   **Health Monitoring:** To track component health via a keepalive mechanism and remove components that become unresponsive.

This guide details the protocol and procedures required for a client application to successfully interact with the Discovery Service.

## 2. Core Concepts

### Communication Protocol

*   **Transport:** Communication occurs over a standard TCP connection. Your client application must act as a TCP client.
*   **Message Format:** The protocol is text-based. All messages are single-line strings. The `TransportLib` handles the message framing; from your application's perspective, you send and receive complete strings.
*   **Structure:** Messages follow a strict, semicolon-delimited format: `DISC:<TYPE>;<field1>;<field2>;...;<last_field>`

### Component Identity

Each component is uniquely identified by a composite ID consisting of two parts:
*   `groupId` (uint8_t): A number representing the component's class (e.g., 2 for all Vehicles, 5 for all Bridges).
*   `idInGroup` (uint8_t): A number that is unique within that group (e.g., vehicle 10, vehicle 11).

### The Global Configuration String

A critical function of the Discovery Service is to act as a conduit for system-wide configuration.
*   **Source of Truth:** The **Offloading Manager (OM)** is the sole source of this configuration.
*   **Format:** The configuration is a single JSON-formatted string (e.g., `{"system_mode":"test","log_level":"debug"}`).
*   **Distribution Flow:**
    1.  The OM is the first critical component to register. It provides the global configuration string during its registration.
    2.  The Discovery Service stores this string.
    3.  As every other component subsequently registers, the Discovery Service includes this global configuration string in its successful registration response. This ensures all components operate with the same parameters.

## 3. Practical C++ Implementation with `discovery_protocol`

This section provides concrete C++ examples for using the `discovery_protocol` library.

### 3.1. Creating and Sending a Registration Request

To register your component, you must construct a `RegistrationRequest` object, encode it into a string, and send it over your TCP connection.

#### Example for a Standard Component (e.g., a Vehicle)

```cpp
#include <discovery_protocol/protocol.hpp>
#include <string>
#include <iostream>

// This function demonstrates creating and encoding a registration request for a standard component.
void send_vehicle_registration() {
    // 1. Create the RegistrationRequest object and populate its fields.
    discovery_protocol::RegistrationRequest req;
    req.componentType = discovery_protocol::ComponentType::VEHICLE;
    req.idRequestType = discovery_protocol::IdRequestType::STATIC;
    req.groupId = 2;
    req.idInGroup = 10;
    req.componentName = "TestVHC-01";
    req.listenAddress = "10.0.0.5";
    req.listenPort = "6000";
    req.humanReadableMessage = "VHC requesting bridge assignment";

    // 2. Wrap the request object in a generic Message object.
    discovery_protocol::Message message_to_send(req);

    // 3. Encode the Message object into a string.
    std::string encoded_message;
    if (discovery_protocol::encode_message(message_to_send, encoded_message) == discovery_protocol::ProtocolStatus::OK) {
        std::cout << "Encoded Vehicle REG: " << encoded_message << std::endl;
        // your_tcp_client->send(...);
    }
}
```

#### Example for the Offloading Manager (Providing Configuration)

The Offloading Manager uses the `humanReadableMessage` field to transmit the global configuration.

```cpp
#include <discovery_protocol/protocol.hpp>
#include <string>
#include <iostream>

// This function demonstrates how the Offloading Manager provides the global configuration.
void send_om_registration() {
    // 1. Define the global configuration as a JSON string.
    std::string global_config = R"({"system_mode":"test","log_level":"debug"})";

    // 2. Create the RegistrationRequest object.
    discovery_protocol::RegistrationRequest req;
    req.componentType = discovery_protocol::ComponentType::OFFLOAD_MANAGER;
    req.idRequestType = discovery_protocol::IdRequestType::STATIC;
    req.groupId = 1;
    req.idInGroup = 1;
    req.componentName = "OffloadManager";
    req.listenAddress = "127.0.0.1";
    req.listenPort = "8000";
    // 3. CRITICAL: Place the JSON config into the 'humanReadableMessage' field.
    req.humanReadableMessage = global_config;

    // 4. Encode and send the message.
    discovery_protocol::Message message_to_send(req);
    std::string encoded_message;
    if (discovery_protocol::encode_message(message_to_send, encoded_message) == discovery_protocol::ProtocolStatus::OK) {
        std::cout << "Encoded OM REG: " << encoded_message << std::endl;
        // your_tcp_client->send(...);
    }
}
```

### 3.2. Receiving and Decoding a Response

After sending a request, you will receive a string response. You must decode it and check for the `configJson` field.

```cpp
#include <discovery_protocol/protocol.hpp>
#include <string>
#include <iostream>
#include <variant>

// This function demonstrates decoding and handling a response from the service.
void handle_response(const std::string& response_string_from_network) {
    discovery_protocol::Message received_message;
    if (discovery_protocol::decode_message(response_string_from_network, received_message) != discovery_protocol::ProtocolStatus::OK) {
        std::cerr << "Failed to decode received message." << std::endl;
        return;
    }

    if (received_message.type == discovery_protocol::MessageType::REGISTRATION_RESPONSE) {
        auto& resp = std::get<discovery_protocol::RegistrationResponse>(received_message.data);

        if (resp.responseCode == discovery_protocol::ResponseCode::SUCCESS) {
            std::cout << "Registration SUCCESSFUL!" << std::endl;
            std::cout << "  Assigned ID: " << (int)resp.assignedGroupId << "." << (int)resp.assignedIdInGroup << std::endl;
            
            // CRITICAL: Check for and use the global configuration.
            if (!resp.configJson.empty()) {
                std::cout << "  Received Global Config: " << resp.configJson << std::endl;
                // Your application should now parse this JSON and configure itself.
            } else {
                std::cout << "  Warning: No global configuration was provided by the service." << std::endl;
            }

            if (!resp.connectionTargetAddress.empty()) {
                std::cout << "  Connect to Bridge at: " << resp.connectionTargetAddress << ":" << resp.connectionTargetPort << std::endl;
            }
        } else {
            std::cout << "Registration FAILED or WAIT. Reason: " << resp.humanReadableMessage << std::endl;
        }
    }
    // ... handle other message types ...
}
```

## 4. The Registration Lifecycle (Conceptual)

### Step 1: Establish a TCP Connection

Connect to the Discovery Service's host and port.

### Step 2: Send a Registration Request

Send a `DISC:REG` message.

**Field Breakdown:**

| Field        | Example Value       | Source Code Ref (`RegistrationRequest`) | Description                                                                                                                                                           |
| :----------- | :------------------ | :-------------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `CompType`   | `V`                 | `componentType`                         | A single character: `V`, `M`, `B`, `O`, `T`.                                                                                                                          |
| `IdReqType`  | `S`                 | `idRequestType`                         | `S` for Static ID.                                                                                                                                                    |
| `GroupID`    | `2`                 | `groupId`                               | Your component's group ID.                                                                                                                                            |
| `IDInGroup`  | `10`                | `idInGroup`                             | Your component's unique ID within its group.                                                                                                                          |
| `CompName`   | `TestVHC-01`        | `componentName`                         | A human-readable name.                                                                                                                                                |
| `ListenAddr` | `10.0.0.5`          | `listenAddress`                         | The IP address where *your* component listens.                                                                                                                        |
| `ListenPort` | `6000`              | `listenPort`                            | The port where *your* component listens.                                                                                                                              |
| `Message`    | `VHC requesting...` | `humanReadableMessage`                  | A descriptive message. **For the Offloading Manager, this field MUST contain the global JSON configuration string.**                                                  |

### Step 3: Handle the Registration Response

The service will reply with a `DISC:ACK` message.

**Field Breakdown:**

| Field          | Example Value                                | Source Code Ref (`RegistrationResponse`) | Description                                                                                             |
| :------------- | :------------------------------------------- | :--------------------------------------- | :------------------------------------------------------------------------------------------------------ |
| `RespCode`     | `0`                                          | `responseCode`                           | `0` (Success), `1` (Wait), `2` (ID Conflict), etc.                                                      |
| ...            | ...                                          | ...                                      | Other fields like assigned ID and connection target.                                                    |
| `ConfigJSON`   | `{"system_mode":"test","log_level":"debug"}` | `configJson`                             | **The global configuration string provided by the OM. Your application must parse this.**               |
| `Message`      | `Registration successful.`                   | `humanReadableMessage`                   | A human-readable status message.                                                                        |

## 5. Keepalives (Maintaining the Session)

Once registered, your component **must** periodically send `DISC:PNG` keepalive messages to the service to avoid being disconnected. The service will reply with `DISC:PON`. This process is unchanged by the global configuration.