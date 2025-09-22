# VHC Data Parameter Implementation Guide

## Overview

The VHC (Virtual Host Controller) data parameter feature allows any system component to pass contextual string data along with offloading requests. This data flows through the entire request processing pipeline and is available to the algorithm for enhanced decision-making.

## System Architecture

The VHC data parameter flows through the system as follows:

```
Bridge CP → OM Server → Decision Engine → Algorithm
```

Each component extracts and passes the `vhc_data` parameter to enable context-aware offloading decisions.

## Implementation Details

### 1. Bridge CP Implementation

To send VHC data from Bridge CP, include the `vhc_data` field in the OFFLOAD_REQUEST message payload:

```json
{
  "component_id": "bridge_component_id",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 1,
  "payload": {
    "request_id": 12345,
    "task_id": 67890,
    "mgwcp_component_id": "mgw_component_id",
    "vhc_data": "your_contextual_data_here"
  }
}
```

**Key Points:**
- The `vhc_data` field is optional - omit it or set to `null` if no context data is available
- Can contain any string data (JSON strings, comma-separated values, encoded data, etc.)
- Maximum recommended length: 1024 characters to avoid message size issues
- Examples of useful VHC data:
  - User location: `"location=37.7749,-122.4194"`
  - Device capabilities: `"cpu=high,memory=8GB,network=5G"`
  - Application context: `"app=video_streaming,quality=4K"`
  - Priority information: `"priority=high,deadline=5s"`

### 2. OM Server Implementation

The OM server automatically extracts the `vhc_data` from incoming OFFLOAD_REQUEST messages:

```python
def _handle_offload_request(self, conn_id: int, message: Dict):
    payload = message['payload']
    request_id = self._coerce_int(payload.get('request_id'), 'request_id')
    task_id = self._coerce_int(payload.get('task_id'), 'task_id')
    mgwcp_component_id = payload.get('mgwcp_component_id')
    vhc_data = payload.get('vhc_data', None)  # Extract VHC data
    
    # Pass to decision engine
    decision = self.decision_engine.make_decision(request_id, task_id, mgwcp_component_id, vhc_data)
```

### 3. Decision Engine Implementation

The decision engine acts as a pass-through for the VHC data:

```python
def make_decision(self, request_id: int, task_id: int, mgwcp_component_id: str, vhc_data: Optional[str] = None) -> AllocationDecision:
    # Pass VHC data to the algorithm
    return algorithm.make_allocation_decision(request_id, task_id, mgwcp_component_id, vhc_data)
```

### 4. Algorithm Implementation

The algorithm receives the VHC data and can use it for enhanced decision-making:

```python
def make_allocation_decision(request_id: int, task_id: int, mgwcp_component_id: str, vhc_data: Optional[str] = None) -> AllocationDecision:
    """
    Make allocation decision with optional VHC context data.
    
    Args:
        request_id: Unique request identifier
        task_id: Task identifier for the offloading request
        mgwcp_component_id: ID of the requesting MGW component
        vhc_data: Optional contextual data from VHC (any string format)
    """
    
    # Example: Parse VHC data for decision-making
    if vhc_data:
        logger.info(f"Request {request_id}: Using VHC context: {vhc_data}")
        
        # Parse context data (example formats)
        if "priority=high" in vhc_data:
            # Handle high priority requests differently
            pass
        elif "location=" in vhc_data:
            # Use location data for geographic placement
            pass
        elif "app=video_streaming" in vhc_data:
            # Optimize for video streaming workloads
            pass
    
    # Your algorithm logic here...
```

## VHC Data Format Recommendations

### 1. Key-Value Pairs
```
"key1=value1,key2=value2,key3=value3"
```
Example: `"priority=high,location=37.7749:-122.4194,app=video"`

### 2. JSON String
```
"{\"priority\": \"high\", \"location\": {\"lat\": 37.7749, \"lng\": -122.4194}}"
```

### 3. Simple Tags
```
"tag1|tag2|tag3"
```
Example: `"high_priority|video_streaming|mobile_device"`

### 4. Encoded Data
```
"base64:SGVsbG8gV29ybGQ="
```

## Usage Examples

### Example 1: Location-Based Allocation

**Bridge CP sends:**
```json
{
  "payload": {
    "request_id": 1001,
    "task_id": 2001,
    "mgwcp_component_id": "mgw_001",
    "vhc_data": "location=37.7749,-122.4194,radius=5km"
  }
}
```

**Algorithm processes:**
```python
if vhc_data and "location=" in vhc_data:
    coords = extract_coordinates(vhc_data)
    # Select MEC closest to user location
    assigned_mec = find_nearest_mec(coords)
```

### Example 2: Application-Specific Optimization

**Bridge CP sends:**
```json
{
  "payload": {
    "request_id": 1002,
    "task_id": 2002,
    "mgwcp_component_id": "mgw_002",
    "vhc_data": "app=ar_gaming,bandwidth_req=100Mbps,latency_req=5ms"
  }
}
```

**Algorithm processes:**
```python
if vhc_data and "app=ar_gaming" in vhc_data:
    # Prioritize low-latency, high-bandwidth MECs
    assigned_mec = select_performance_optimized_mec()
```

### Example 3: Priority-Based Allocation

**Bridge CP sends:**
```json
{
  "payload": {
    "request_id": 1003,
    "task_id": 2003,
    "mgwcp_component_id": "mgw_003",
    "vhc_data": "priority=emergency,service=public_safety"
  }
}
```

**Algorithm processes:**
```python
if vhc_data and "priority=emergency" in vhc_data:
    # Override normal allocation rules for emergency services
    return AlwaysApproveDecision(assigned_mec="dedicated_emergency_mec")
```

## Testing VHC Data Implementation

### 1. Test Message Format

Create test messages with various VHC data formats:

```python
test_messages = [
    # No VHC data
    {"request_id": 1, "task_id": 1, "mgwcp_component_id": "test1"},
    
    # With VHC data
    {"request_id": 2, "task_id": 2, "mgwcp_component_id": "test2", 
     "vhc_data": "priority=high,app=video"},
    
    # Complex VHC data
    {"request_id": 3, "task_id": 3, "mgwcp_component_id": "test3",
     "vhc_data": "location=37.7749,-122.4194,device=mobile,network=5G"}
]
```

### 2. Verify Data Flow

Check that VHC data appears in logs at each system level:

```bash
# Check OM server logs
grep "vhc_data" /var/log/om_server.log

# Check algorithm logs
grep "VHC context" /var/log/algorithm.log
```

### 3. Algorithm Testing

Test algorithm behavior with different VHC data scenarios:

```python
# Test cases for algorithm
test_cases = [
    (1, 1, "mgw1", None),  # No VHC data
    (2, 2, "mgw2", "priority=high"),  # High priority
    (3, 3, "mgw3", "location=invalid"),  # Invalid format
    (4, 4, "mgw4", "app=unknown"),  # Unknown application
]

for request_id, task_id, mgw_id, vhc_data in test_cases:
    decision = make_allocation_decision(request_id, task_id, mgw_id, vhc_data)
    print(f"Request {request_id}: {decision}")
```

## Best Practices

### 1. Data Validation
- Always validate VHC data format in the algorithm
- Handle malformed or unexpected data gracefully
- Log warnings for invalid data formats

### 2. Security Considerations
- VHC data should not contain sensitive information
- Consider data sanitization if processing user-provided content
- Implement length limits to prevent DoS attacks

### 3. Performance Optimization
- Parse VHC data efficiently (avoid complex regex for performance-critical paths)
- Cache parsed VHC data if used multiple times in the same request
- Consider using structured formats (JSON) for complex data

### 4. Backward Compatibility
- Always make VHC data optional
- Ensure the system works correctly when VHC data is not provided
- Maintain fallback behavior for legacy components

## Troubleshooting

### Common Issues

1. **VHC data not reaching algorithm**
   - Check OM server logs for successful extraction
   - Verify decision engine passes the parameter
   - Ensure algorithm method signature includes vhc_data parameter

2. **Algorithm ignoring VHC data**
   - Verify VHC data is not None or empty string
   - Check parsing logic for expected formats
   - Add debug logging to trace VHC data processing

3. **Message format errors**
   - Validate JSON message structure
   - Check that vhc_data is a string type
   - Verify payload contains all required fields

### Debug Commands

```python
# Enable debug logging for VHC data
import logging
logging.getLogger('OM.algorithm').setLevel(logging.DEBUG)

# Test VHC data parsing
def debug_vhc_data(vhc_data):
    print(f"VHC data: {vhc_data}")
    print(f"Type: {type(vhc_data)}")
    print(f"Length: {len(vhc_data) if vhc_data else 0}")
    if vhc_data:
        print(f"Contains 'priority': {'priority' in vhc_data}")
        print(f"Contains 'location': {'location' in vhc_data}")
```

## Future Enhancements

### 1. Structured VHC Data
Consider implementing structured VHC data classes:

```python
@dataclass
class VHCContext:
    priority: Optional[str] = None
    location: Optional[tuple] = None
    application: Optional[str] = None
    device_type: Optional[str] = None
    
    @classmethod
    def parse(cls, vhc_data: str) -> 'VHCContext':
        # Parse string data into structured format
        pass
```

### 2. VHC Data Validation Schema
Implement JSON schema validation for VHC data:

```python
VHC_DATA_SCHEMA = {
    "type": "object",
    "properties": {
        "priority": {"type": "string", "enum": ["low", "normal", "high", "emergency"]},
        "location": {"type": "object", "properties": {"lat": {"type": "number"}, "lng": {"type": "number"}}},
        "application": {"type": "string"},
        "device_type": {"type": "string"}
    }
}
```

### 3. VHC Data Analytics
Add metrics and analytics for VHC data usage:

```python
# Track VHC data usage patterns
vhc_data_metrics = {
    "requests_with_vhc_data": 0,
    "requests_without_vhc_data": 0,
    "common_vhc_patterns": {},
    "vhc_data_lengths": []
}
```

This completes the VHC data parameter implementation. The feature is now available throughout the system and can be used by any component to provide contextual information for enhanced offloading decisions.