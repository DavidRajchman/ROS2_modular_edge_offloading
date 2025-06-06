Use modified transport libs

After receiving offloading allowed message for certain outputs, create a maping which will forward messages from certain vehicle topics, to the MEC server. Then it will setup a listener for the output topic from MEC and forward it to the vehicle

The response will be a json which the bridge will intercept and setup the forwarding accrodingly, it will also notify the VHC and MEC server to enable the handlers

{
  "RequestID": 0,
  "GrantType": 0,
  "MECserverLOC":"",
  "InputsForProcessing": [],
  "ProcessedOutputs": [],
}


request info
task for computes
priority (QoS)
time to complete


popis funkcionality:
VHC = vehicle
OM = offloading manager

VHC pošle request pro offloading, pro určitý typ výpočtu. OM zjistí zda má dostatečnou serverovou kapacitu, pokud ano tak spustí nový kontejner na MEC a pošle info do Bridge aby sestavil trasu. Bridge sestaví cestu a pošle info do VHC který zapne handlery. 

Pro určité stupně QoS bude designovaný jeden topic signalizující start výpočtu. V tu chvíly začne VHC výpočet jelikož nevý zda bude možný oflloading stihnut. Startovací topic se zároveň přepošle do OM, který rozhodne zda je schopen výpočet stihnout doručit do deadlinu. Pokud ano pošle potvrzení přes Bridge do VHC, který ukončí lokální výpočet a začne čekat na externí výpočet 

requesty pro offloading se neposílají s každým výpočtem ale periodicky (např 10s). První request vyžaduje nějaký čas na spuštění nového kontejneru a přiřazení cesty bridgem, následující requesty přijdu dříve než deadline uplyne a tím pádem již sestavená offloading trasa bude ponechána. Pokud nový request nepřijde než uplyne deadline, bude trasa zničena a kontejner vypnut. Trasu je také možné zničit na žádost VHC (např konec jízdy)

Navíc (Bonus), v Bridge bude FIFO fronta která bude cashovat veškeré zprávy topiců potřebných pro výpočet. Tato fronta se bude vyprazdňovat pokaždé když dojde k dokončení předchozího výpočtu. V případě že by něco selhalo, může být tato fronta použita k znovuspuštění výpočtu v jiném kontejneru


AI DISKUSE

Ok now that everything is working, lets work on the Bridge program itself

Here is how the bridge is supposed to function

Bridge will work with with a yet undeveloped component called Offloading Manager (OM) and the ROS2 gateway custom app.

The bridge should act as a middleman betwen a vehicle and a MEC server and will be directed by OM. 

When the vehicle is given an Ok to offload message, it will start sending certain ros2 topics messages to the bridge by a TCP stream. The bridge will be given instructions which MEC server should receive the messages. Bridge will send those messages to the MEC server using the transport layer. Then it will send the computed results back to VHC

One vehicle can be served by many MEC servers, but a signle result will be computed on a single MEC server. Here is an example: 

There will be 5 topics available on the vehicle MAP, SPEED, DIRECTION, GPS, PICTURES. There are 2 computations that can be ofloaded
1) Route planing, which needs MAP, SPEED, DIRECTION, GPS
2) Collision avoidance, which needs SPEED, DIRECTION, PICTURES

So the bridge must send some incoming messages to MEC designated for task 1, some to MEC for task 2 and some to both MEC servers. 

I imagine it could work like this. There will be transport handler tasks which will have FIFO Quees as an input and will handle connection to MEC servers and VEHICLES, they will send all messages from the QUEUE to the connected gateway using the transport library. They will also handle incoming messages. They will look at a map array which will store a list of recipients of messages from client X with topic Y (that will be a key to the map array) and then they will insert the message to all queues listed in  the array.

All messages have headers where the topic name and message types are given.

Please look at my proposed solution, tell me if my idea is good and find ways of improwing it. The bridge must have as low latency as possible. 

DO NOT WRITE ANY CODE YET

AT THE START OF YOUR MESSAGE WRITE IN YOUR OWN WORDS YOUR UNDERSTANDING OF WHAT I AM ASKING YOU TO DO, WHAT IS MY PROBLEM AND WHAT IS MY PROPOSED SOLUTION

DO NOT WRITE ANYTHING YOU DO NOT KNOW FOR CERTAIN

IF YOU NEED ADITIONAL INFORMATIOIN SUCH AS OTHER CODE SAMPLES, ASK ME



REPLY FROM AI:

Okay, I will review your proposed solution for the Bridge application.

**My Understanding of Your Request:**

You are asking me to evaluate your high-level design for a "Bridge" application. This Bridge application is intended to act as a crucial intermediary component. Its primary role is to manage the flow of ROS2 topic messages between vehicles (VHCs) and multiple Mobile Edge Computing (MEC) servers. The entire process is to be directed by an "Offloading Manager" (OM).

**The Problem You're Solving:**

The core problem is to efficiently and with low latency route specific ROS2 messages from a vehicle to one or more designated MEC servers for computation, and then route the computed results back to the originating vehicle. A key challenge is that different sets of topics might be needed by different MEC servers to perform different computational tasks, and some topics might need to be sent to multiple MEC servers simultaneously. For example, "Route planning" might need MAP, SPEED, DIRECTION, and GPS topics sent to MEC server A, while "Collision avoidance" might need SPEED, DIRECTION, and PICTURES sent to MEC server B. The SPEED and DIRECTION topics, in this case, would need to be fanned out to both MEC A and MEC B.

**Your Proposed Solution:**

Your proposed solution involves the following key elements:
1.  **Transport Handler Tasks:** These are dedicated concurrent tasks. Some will manage TCP connections to vehicles, and others will manage connections to the various MEC servers.
2.  **Input FIFO Queues:** Each transport handler task will have its own First-In, First-Out (FIFO) queue to receive messages.
3.  **Message Sending:** When a transport handler needs to send a message (either to a vehicle or a MEC server), it will take messages from its input queue and use the `TransportLib` to send them over the established connection.
4.  **Message Receiving and Routing Logic:** When a transport handler (e.g., one connected to a vehicle) receives an incoming message, it will:
    *   Inspect the message header to identify the source (e.g., vehicle ID) and the ROS2 topic name.
    *   Use this information (client X, topic Y) as a key to look up routing instructions in a shared "map array" (a routing table).
    *   This routing table will specify a list of destination queues. These destination queues belong to the transport handler tasks that are connected to the appropriate MEC servers.
5.  **Message Forwarding:** The receiving transport handler will then place (enqueue) the incoming message into all the output queues indicated by the routing table.

**Evaluation of Your Proposed Solution:**

Your idea provides a good foundation for a decoupled and potentially scalable system. The use of dedicated tasks for I/O and queues for inter-task communication is a common pattern.

**Strengths:**
*   **Decoupling:** Using queues between message reception, routing logic, and message transmission decouples these stages. This can help in managing bursts of messages and allows different parts of the system to operate somewhat independently.
*   **Concurrency:** Dedicated tasks for transport handling allow for concurrent I/O operations with multiple vehicles and MEC servers.
*   **Clear Routing Mechanism:** The concept of a routing map (client + topic -> list of output queues) is a straightforward way to manage where messages should go.

**Areas for Consideration and Potential Improvement (Focusing on Low Latency):**

1.  **Queueing Latency:**
    *   **Impact:** Every time a message is enqueued and then dequeued, a small amount of latency is added. For very low latency requirements, the cumulative effect of multiple queue hops can be significant.
    *   **Suggestion:** Evaluate if all messages truly need to pass through queues. For instance, if a VHC message is destined for only one MEC server, and the VHC-facing task can directly access the MEC-facing task's sending mechanism (perhaps with proper locking or a very small, bounded, single-producer/single-consumer queue), you might reduce latency. This is a trade-off between decoupling and raw speed.

2.  **Message Copying:**
    *   **Impact:** If messages are copied when placed into multiple queues (for fan-out), this consumes CPU time and memory bandwidth, adding to latency, especially for large messages (like PICTURES).
    *   **Suggestion:** Consider using smart pointers (e.g., `std::shared_ptr<YourMessageClass>`) to pass messages by reference through the queues. This way, only the pointer is copied, not the entire message data. This requires careful lifetime management of the message data. If `TransportLib` or your message serialization allows, explore zero-copy techniques where possible, though this is often more complex.

3.  **Task Management and Context Switching:**
    *   **Impact:** A very large number of tasks (e.g., one task per connection if many connections are expected) can lead to increased overhead from OS context switching.
    *   **Suggestion:** Consider using a thread pool for your transport handlers rather than spawning a new thread for every single connection. You could have a pool for VHC-facing connections and another for MEC-facing connections. The size of these pools can be tuned based on the number of CPU cores and expected load.

4.  **Routing Map ("Map Array"):**
    *   **Data Structure:** For "map array," if you mean a hash map (like `std::unordered_map` in C++), lookups can be very fast (average O(1)). Ensure the key (client X, topic Y) is efficient to construct and hash.
    *   **Update Mechanism:** How is this routing map populated and updated by the Offloading Manager (OM)? Updates need to be thread-safe and efficient to avoid blocking message processing. Consider mechanisms like reader-writer locks or strategies for atomically swapping to a new version of the map.

5.  **Backpressure:**
    *   **Impact:** If a MEC server is slow or its outgoing queue in the Bridge fills up, what happens? Without backpressure, the Bridge could run out of memory or drop messages.
    *   **Suggestion:** Implement backpressure. If an output queue to a MEC server is full, the routing logic should have a strategy (e.g., temporarily stop reading from the VHC, drop messages if allowed, or signal the OM).

6.  **Handling Results from MEC Servers:**
    *   Your description focuses on VHC -> MEC. The path for results (MEC -> VHC) also needs to be defined. How will the Bridge know which vehicle to send a result to? This might involve a similar routing mechanism or correlation IDs in messages.

7.  **FIFO Queues and Prioritization:**
    *   **Impact:** Standard FIFO queues process messages in the order they arrive. If some ROS2 topics or computational tasks are more critical or have stricter latency requirements than others, FIFO might not be optimal.
    *   **Suggestion:** If prioritization is needed, you might consider priority queues. However, this adds complexity to the queueing logic.

8.  **Minimizing Processing in I/O Tasks:**
    *   **Impact:** To keep I/O tasks responsive, they should do minimal processing.
    *   **Suggestion:** The VHC-facing task should ideally just receive the TCP data, parse the essential header for routing (client, topic), and quickly dispatch it. Complex parsing or decision-making should be offloaded to other tasks if it risks blocking I/O.

**Overall:**
Your proposed solution is a good starting point. The main challenge for achieving very low latency will be to minimize message copying, reduce the number of queueing stages where possible (or make them extremely efficient), and ensure that your tasking model doesn't introduce excessive context-switching overhead.

**Information I Might Need Later (No Need to Provide Now):**
To give more specific advice later (when you move to implementation details), I might ask about:
*   Expected message rates and sizes for different topics.
*   The exact nature of the "Offloading Manager" (OM) to Bridge communication protocol for routing instructions.
*   Specific latency targets (e.g., in milliseconds).
*   Details of the message headers (how client and topic are identified).
*   How results from MEC servers are correlated back to the originating vehicle/request.

This conceptual review should help you refine your design. Remember that for low-latency systems, profiling and testing under realistic load conditions are crucial to identify and address bottlenecks.