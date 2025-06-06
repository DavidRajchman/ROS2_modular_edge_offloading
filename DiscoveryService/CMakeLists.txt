cmake_minimum_required(VERSION 3.10)
project(DiscoveryService VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include FetchContent module
include(FetchContent)

# Offline moodycamel
set(moodycamel_concurrent_queue_SOURCE_DIR /home/ubuntu/external_libs/moodycamel_concurrent_queue) # Path inside the container

# Find required packages
find_package(Threads REQUIRED)
find_package(TransportLib REQUIRED)

# Add the executable for your DiscoveryService application
add_executable(discovery_service
    src/main.cpp
    src/discovery_service.cpp
    src/component_registry.cpp
    src/protocol.cpp
)

# Explicitly add the include directory for concurrentqueue
# This ensures that the compiler knows where to find "concurrentqueue.h"
target_include_directories(discovery_service
    PRIVATE
    ${moodycamel_concurrent_queue_SOURCE_DIR}
)

target_compile_definitions(discovery_service
    PRIVATE
    APP_NAME="DiscoveryService"
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    ACTIVE_LOG_LEVEL=LOG_LEVEL_WARN
)

# Link libraries
target_link_libraries(discovery_service
    PRIVATE
    Threads::Threads
    TransportLib::transport
)

# Optional: Install rule for the DiscoveryService executable
# install(TARGETS discovery_service
#     RUNTIME DESTINATION bin
# )