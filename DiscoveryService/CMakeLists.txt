cmake_minimum_required(VERSION 3.16)
project(DiscoveryService VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the 'src' directory to the include path for cleaner #includes
# Note: We will create the 'src' directory and its files next.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Find required, pre-existing packages
find_package(TransportLib REQUIRED)
find_package(DiscoveryProtocol REQUIRED)

# Define the executable and its source files
add_executable(DiscoveryService
    src/main.cpp
    src/discovery_service.cpp
    src/component_registry.cpp
)

# Add compile definitions for logging and configuration
target_compile_definitions(DiscoveryService PRIVATE
    APP_NAME="DiscoverySvc"
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    DISCOVERY_SERVICE_PORT=9090
    ACTIVE_LOG_LEVEL=LOG_LEVEL_INFO
)

# Add 'src' to the include directories for our own headers
target_include_directories(DiscoveryService PRIVATE src)

# Link the discovery service against its dependencies
target_link_libraries(DiscoveryService PRIVATE
    TransportLib::transport
    DiscoveryProtocol::discovery_protocol
)

# Optional: Install rule for the executable
install(TARGETS DiscoveryService DESTINATION bin)