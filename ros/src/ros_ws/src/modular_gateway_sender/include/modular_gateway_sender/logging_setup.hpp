#ifndef LOGGING_SETUP_HPP
#define LOGGING_SETUP_HPP

#include <string>

namespace gateway {

/**
 * @brief Configures the CppLogging library for a specific component.
 * 
 * This function sets up a binary file sink for the global "gateway" logger.
 * It should be the first function called in the main() of any executable.
 * 
 * @param component_type A string identifier for the component (e.g., "VHC", "MEC")
 *                       which is used to name the log file.
 */
void setup_logging(const std::string& component_type);

} // namespace gateway

#endif // LOGGING_SETUP_HPP