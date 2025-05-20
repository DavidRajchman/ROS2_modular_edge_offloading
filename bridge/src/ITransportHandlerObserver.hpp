#ifndef ITRANSPORT_HANDLER_OBSERVER_HPP
#define ITRANSPORT_HANDLER_OBSERVER_HPP

#include <string>
#include <memory>

class ITransportHandlerObserver {
public:
    virtual ~ITransportHandlerObserver() = default;
    virtual void onHandlerConnected(const std::string& gateway_id) = 0;
    virtual void onHandlerDisconnected(const std::string& gateway_id, const std::string& reason) = 0;
    virtual void onHandlerCriticalError(const std::string& gateway_id, const std::string& error_message) = 0;
    // Add other methods as per your design if TransportHandler calls them
};

#endif // ITRANSPORT_HANDLER_OBSERVER_HPP