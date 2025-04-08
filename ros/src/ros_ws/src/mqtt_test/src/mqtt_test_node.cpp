#include <iostream>
#include <cstring>
#include "mqtt/async_client.h"

const std::string SERVER_ADDRESS("tcp://davidpraha.buru-palermo.ts.net:1883");
const std::string CLIENT_ID("publisher");
const std::string TOPIC("test/topic");
const int QOS = 1;
const int TIMEOUT = 10000;

class PublisherCallback : public virtual mqtt::callback
{
public:
    void connection_lost(const std::string& cause) override
    {
        std::cout << "Connection lost: " << cause << std::endl;
    }

    void delivery_complete(mqtt::delivery_token_ptr token) override
    {
        std::cout << "Message delivered" << std::endl;
    }
};

int main(int argc, char* argv[])
{
    mqtt::async_client client(SERVER_ADDRESS, CLIENT_ID);

    mqtt::connect_options connOpts;
    connOpts.set_keep_alive_interval(20);
    connOpts.set_clean_session(true);

    try
    {
        PublisherCallback callback;
        client.set_callback(callback);

        mqtt::token_ptr connectionToken = client.connect(connOpts);
        connectionToken->wait();

        while (true)
        {
            std::string message;
            std::cout << "Enter a message (or 'q' to quit): ";
            std::getline(std::cin, message);

            if (message == "q")
                break;

            mqtt::message_ptr pubMessage = mqtt::make_message(TOPIC, message, QOS, false);
            client.publish(pubMessage)->wait();
        }

        mqtt::token_ptr disconnectionToken = client.disconnect();
        disconnectionToken->wait();
    }
    catch (const mqtt::exception& ex)
    {
        std::cerr << "MQTT Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}