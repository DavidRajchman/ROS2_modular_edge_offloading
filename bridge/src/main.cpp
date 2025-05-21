#include "TransportHandler.hpp"
#include "RoutingTable.hpp"
#include "common_types.hpp" // For MPSCQueueType, Message, RoutingKey, parse_modular_gw_header, ModGW::Header
#include <transport/logging_utils.hpp> // Your logging

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <memory>

// Global atomic flag for graceful shutdown
std::atomic<bool> shutdown_flag(false);

void signal_handler(int signum) {
    LOG_INFO("Interrupt signal (%d) received. Initiating shutdown...", signum);
    shutdown_flag.store(true);
}

// Helper function to print message details (optional)
void print_message_info(const std::string& queue_name, const std::shared_ptr<Message>& msg) {
    if (!msg || msg->data.empty()) {
        LOG_INFO("[%s] Received an empty or null message.", queue_name.c_str());
        return;
    }
    LOG_INFO("[%s] Received message of size %zu bytes.", queue_name.c_str(), msg->data.size());

    // Attempt to parse the header for more details
    // Ensure your parse_modular_gw_header can handle raw byte vector
    std::optional<ParsedHeaderInfo> header_info = parse_message_header(msg->data.data(), msg->data.size());
    if (header_info) {
        LOG_INFO("  Parsed Header: SourceID: %u, MsgType: %u, PayloadOffset: %zu, PayloadSize: %u, TotalMsgLen: %zu",
            header_info->routing_key.source_id,
            header_info->routing_key.message_type,
            header_info->payload_offset,
            header_info->payload_size,
            header_info->total_message_length);
    } else {
        LOG_WARN("  Could not parse ModGW header from received message in %s.", queue_name.c_str());
    }
}


int main(int argc, char* argv[]) {
    // 1. Initialize Logging
    // (Assuming your logging is set up, e.g., via a static initializer or an explicit call if needed)
    LOG_INFO("Bridge Manual Test Rig Starting...");
    LOG_INFO("------------------------------------");

    // 2. Register Signal Handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 3. Configuration
    // --- VHC Modular GW Configuration ---
    const std::string VHC_GW_INSTANCE_ID_STR = "VHC1_TEST"; // Used for TH logging and potentially as part of routing key if MGW uses it
    const std::string VHC_MGW_IP = "127.0.0.1";         // IP of your VHC MGW
    const int VHC_MGW_PORT = 15001;                      // Port your VHC MGW listens on

    // --- MEC Modular GW Configuration ---
    const std::string MEC_GW_INSTANCE_ID_STR = "MEC1_TEST"; // Used for TH logging
    const std::string MEC_MGW_IP = "127.0.0.1";         // IP of your MEC MGW
    const int MEC_MGW_PORT = 16001;                      // Port your MEC MGW listens on

    // --- Routing Key Definitions (CRITICAL: These must match what your MGWs send) ---
    // These are the string identifiers that `parse_modular_gw_header` will produce
    // for the `source_id_str` and `message_type_str` fields of the RoutingKey.
    // You need to know how your MGWs are configured to populate the header fields
    // that map to these strings.

   // For VHC originated messages:
   const uint8_t VHC_ID_GROUP = 2; // VHC ID group - 2 is manualy assigned VHC group ID
   const uint8_t VHC_ID_IN_GROUP_1 = 1; // Example VHC instance 1
   const uint8_t MSG_STRING_INPUT = 201;


   // For MEC originated messages:
   const uint8_t MEC_ID_GROUP = 12; // MEC ID group - 12 is manually assigned MEC group ID
   const uint8_t MEC_ID_IN_GROUP_1 = 1; // Example MEC instance 1
   const uint8_t MSG_STRING_RESULT = 202;



    // 4. Create Core Components
    LOG_INFO("Initializing Routing Table...");
    auto routing_table = std::make_shared<RoutingTable>(20); // Capacity for ~20 routes

    LOG_INFO("Creating MPSC Queues...");
    // Input queues for Transport Handlers (messages to be sent TO the MGWs)
    auto vhc_th_input_queue = std::make_shared<MPSCQueueType>();
    auto mec_th_input_queue = std::make_shared<MPSCQueueType>();

    // Output queues for this test harness (messages received FROM MGWs, routed here for observation)
    auto main_observes_vhc_topic_B_q = std::make_shared<MPSCQueueType>();
    auto main_observes_mec_topic_X_q = std::make_shared<MPSCQueueType>();

    // 5. Configure Routing Table
    LOG_INFO("Configuring routes...");
    // Scenario 1: VHC sends Topic A, forward it to MEC MGW
    // This means messages from VHC_SENDER_ID_STR with type VHC_MSG_TYPE_A_STR
    // should go into the input queue of the MEC Transport Handler.
    
    //create routing key for VHC using construct routing key function
    //VHC ROUTES
    routing_table->add_route(
        construct_routing_key(VHC_ID_GROUP, VHC_ID_IN_GROUP_1, MSG_STRING_INPUT), // MSG_STRING_INPUT should be a uint8_t msg type
        mec_th_input_queue
    );
    routing_table->add_route( //debug route
        construct_routing_key(VHC_ID_GROUP, VHC_ID_IN_GROUP_1, MSG_STRING_INPUT), // MSG_STRING_INPUT should be a uint8_t msg type
        main_observes_vhc_topic_B_q
    );
    //MEC ROUTES
    routing_table->add_route(
        construct_routing_key(MEC_ID_GROUP, MEC_ID_IN_GROUP_1, MSG_STRING_RESULT), // MSG_STRING_INPUT should be a uint8_t msg type
        vhc_th_input_queue
    );
    routing_table->add_route( //debug route
        construct_routing_key(MEC_ID_GROUP, MEC_ID_IN_GROUP_1, MSG_STRING_RESULT), // MSG_STRING_INPUT should be a uint8_t msg type
        main_observes_mec_topic_X_q
    );
    

    // 6. Create and Start Transport Handlers
    LOG_INFO("Creating and starting Transport Handlers...");
    std::weak_ptr<ITransportHandlerObserver> null_observer; // No CP, so no observer


    auto vhc_handler = std::make_shared<TransportHandler>(
        VHC_GW_INSTANCE_ID_STR, VHC_MGW_IP, VHC_MGW_PORT,
        vhc_th_input_queue, routing_table, null_observer
    );
    vhc_handler->start();
    LOG_INFO("VHC Transport Handler for GW ID '%s' connecting to %s:%d has been started.", VHC_GW_INSTANCE_ID_STR.c_str(), VHC_MGW_IP.c_str(), VHC_MGW_PORT);

    auto mec_handler = std::make_shared<TransportHandler>(
        MEC_GW_INSTANCE_ID_STR, MEC_MGW_IP, MEC_MGW_PORT,
        mec_th_input_queue, routing_table, null_observer
    );
    mec_handler->start();
    LOG_INFO("MEC Transport Handler for GW ID '%s' connecting to %s:%d has been started.", MEC_GW_INSTANCE_ID_STR.c_str(), MEC_MGW_IP.c_str(), MEC_MGW_PORT);

    LOG_INFO("------------------------------------");
    LOG_INFO("Test environment running. Modular GWs should be sending messages on their testing topics.");
    LOG_INFO("Observe logs for received/forwarded messages. Press Ctrl+C to exit.");
    LOG_INFO("------------------------------------");

    // 7. Test Observation Loop
    // This loop primarily observes messages routed to the 'main_observes_*' queues.
    // Forwarding between MGWs happens via the THs and routing table directly.
    // You would confirm that forwarding by checking the logs/behavior of your MGW instances.
    unsigned long long loop_counter = 0;
    while (!shutdown_flag.load()) {
        std::shared_ptr<Message> received_msg;

        // Check for messages from VHC routed to main
        if (main_observes_vhc_topic_B_q->try_dequeue(received_msg)) {
            print_message_info("Main_Observe_VHC_Queue", received_msg);
        }

        // Check for messages from MEC routed to main (if configured for observation)
        if (main_observes_mec_topic_X_q->try_dequeue(received_msg)) {
            print_message_info("Main_Observe_MEC_Queue", received_msg);
        }

        // Optional: Manually enqueue a fully formed ModGW message to send TO an MGW
        // This requires your test harness to act like an MGW and create the entire message.
        // Example: Send a command to VHC MGW every 30 seconds
        // if (loop_counter > 0 && loop_counter % 300 == 0) { // approx every 30s
        //     LOG_INFO("Test harness attempting to send a message to VHC MGW...");
        //     auto msg_to_send_to_vhc = std::make_shared<Message>();
        //     // msg_to_send_to_vhc->data MUST be a complete, serialized ModGW message
        //     // that VHC_MGW is expecting.
        //     // E.g., construct_full_modgw_message_for_vhc(VHC_SENDER_ID_STR, "COMMAND_TOPIC", "payload data");
        //     // if (msg_to_send_to_vhc->data.size() > 0) {
        //     //    vhc_th_input_queue->enqueue(msg_to_send_to_vhc);
        //     // }
        // }


        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop_counter++;
    }

    // 8. Shutdown
    LOG_INFO("------------------------------------");
    LOG_INFO("Initiating shutdown of Transport Handlers...");
    if (vhc_handler) {
        LOG_INFO("Stopping VHC Handler (%s)...", VHC_GW_INSTANCE_ID_STR.c_str());
        vhc_handler->stop();
        LOG_INFO("VHC Handler (%s) stopped.", VHC_GW_INSTANCE_ID_STR.c_str());
    }
    if (mec_handler) {
        LOG_INFO("Stopping MEC Handler (%s)...", MEC_GW_INSTANCE_ID_STR.c_str());
        mec_handler->stop();
        LOG_INFO("MEC Handler (%s) stopped.", MEC_GW_INSTANCE_ID_STR.c_str());
    }

    LOG_INFO("Routing Table and queues will be cleared upon shared_ptr destruction.");
    LOG_INFO("Bridge Manual Test Rig Finished.");
    LOG_INFO("------------------------------------");

    return 0;
}