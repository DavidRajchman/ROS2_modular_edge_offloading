# P2P Local Configuration Schema

To ensure compatibility with the existing `GatewayController::load_global_config_from_json()` method, the local P2P configuration file must follow this exact JSON structure.

## Schema Definition

```json
{
  "default_session_timeout": 300,
  "max_concurrent_sessions": 10,
  "available_tasks": [
    {
      "task_id": 1,
      "task_name": "P2P_LIDAR_TASK",
      "input_message_types": [11, 201],
      "output_message_types": [202]
    }
  ]
}
```

### Field Descriptions:

*   **`default_session_timeout`** (Integer): Optional. Used as a placeholder for session liveness in P2P mode (though keepalives are bypassed).
*   **`max_concurrent_sessions`** (Integer): Optional. Limits how many tasks can be active simultaneously.
*   **`available_tasks`** (Array): **Required**. A list of tasks that will be automatically activated upon connection.
    *   **`task_id`** (Integer): Unique ID for the task.
    *   **`task_name`** (String): Human-readable name.
    *   **`input_message_types`** (Array of Integers): List of numeric MessageType IDs.
        *   **VHC**: Subscribes to these topics and sends data to MEC.
        *   **MEC**: Receives data and publishes to these topics.
    *   **`output_message_types`** (Array of Integers): List of numeric MessageType IDs.
        *   **VHC**: Receives data and publishes to these topics.
        *   **MEC**: Subscribes to these topics and sends data to VHC.

## Example File: `p2p_config.json`
```json
{
  "available_tasks": [
    {
      "task_id": 101,
      "task_name": "DIRECT_LIDAR_STREAM",
      "input_message_types": [11],
      "output_message_types": []
    }
  ]
}
```

## Note on Compatibility
By keeping this schema identical to the `RegistrationResponse.configJson` sent by the Discovery Service, we ensure that the `GatewayController` logic remains unified across all three operation modes.
