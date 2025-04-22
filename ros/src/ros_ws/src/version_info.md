# 0.7.0 MODULAR GW
 - reciever implementation
 - added transport_access_mutex to prevent recieve-send collisions implemented mutex protection in send_data function

 - **TODO** - add a parameter to change the wait time before reciever polling (current 1ms) 
## 0.7.1
 - fix, added waiting for transport connect before receiver start 


# 0.6.0 MODULAR GW
- added laser scan handler with message serialization and possibility to empty intensities array
# 0.5.0 MODULAR GW
- rewrite of modular gateway structure - only a single node that is running. Node registers topic specific handlers, that are using handler base class. TCP structure is unchanged from v0.4.0
- confirmed working for string handler topic  "topic" (using helloword node)
# 0.4.0 MODULAR GW
 - extended ros_gateway to allow for setting all header parameters by topic modules
  
# 0.3.0 MODULAR GW
 - split gateway into sender and reciever packages NOTE: Reciever not implemented in this version
 - created a modular structure |topic_parser - gateway - sending protocol_handler|
 - could not connect to local python TCP server, needs further investigation
 - CONFIRMED WORKING on a remote python TCP server
# 0.2.0 SENDER AND RECEIVER
 - removed aplication level "ACK" messages for TCP


# 0.1.0
 - working TCP gateway sender and reciever proof of concept compatible with helloworld_node


  

