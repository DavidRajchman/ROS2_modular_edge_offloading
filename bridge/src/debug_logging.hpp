#ifndef DEBUG_LOGGING_HPP
#define DEBUG_LOGGING_HPP

#include "logging/logger.h"

// Custom DEBUG macro that always logs debug messages regardless of NDEBUG
#define BRIDGE_DEBUG(logger, message, ...) \
    do { \
        (logger).Log(CppLogging::Level::DEBUG, false, message, ##__VA_ARGS__); \
    } while(0)

#endif // DEBUG_LOGGING_HPP
