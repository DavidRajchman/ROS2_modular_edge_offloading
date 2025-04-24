 # **TODO fixes and modifications** 
 - (0.7.1) add a parameter to change the wait time before reciever polling (current 1ms)
 - (0.8.0) TESTING NEEDED [IMPLEMENTED] implement a way where topic handler can only be used as a subcriber or a publisher to add the ability to utilise the same handler for server and reciever gw
 - (0.10.0) Rework file structure and cmakelist to make the project cleaner and adding new handlers as simple as possible (make all handlers a single cmakelist library)

# 0.11.1
 - server transpot tcp is working for message receiving also
 - added msg deserialization to laserscan handler - confirmed working
 - confirmed working, NO KNOWN issues except todo list additions

## 0.10.7
 - server transport tcp is working for message sending (server in subscriber mode)

# 0.10.0
 - added tcp server transport layer
 - aded a server node and renamed the client node
 - currently not working over localhost port 8888, needs further investigation
 - server gives error transport not connected imidiately after calling connection succesfull

# 0.9.0
 - Implemented separate togle for ros publisher and subscriber mode for topic handlers. Meaning the topic handlers are now able to function universaly.
 - After the adtition of tcp_server transport layer, modular gateway will be able to comunicate both ways

# 0.8.0
 - reciever publishing to ros2 topics functionality added
 - simple aproach - just add the can_procces_message_type to hpp of handler and implement the process_and_publish_receveid_msg() method
 - no need to modify the main file to implement the message recieving
 - tested on string handler (working), but loopback test will result in an infinite loop (expected behaviour) - need to implement server to fix this
 - need to implement only subscriber and only publisher version

## 0.7.1
 - fix, added waiting for transport connect before receiver start 

# 0.7.0 MODULAR GW
 - reciever implementation (without publishing to ros2, only transport gateway - gateway)
 - added transport_access_mutex to prevent recieve-send collisions implemented mutex protection in send_data function

  


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


  

