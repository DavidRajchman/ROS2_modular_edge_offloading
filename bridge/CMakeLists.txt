cmake_minimum_required(VERSION 3.10)
project(bridge VERSION 0.3.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include FetchContent module
include(FetchContent)

# Online moodycamel download version

## Declare the dependency on moodycamel::ConcurrentQueue
#FetchContent_Declare(
#    moodycamel_concurrent_queue
#    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
#    GIT_TAG v1.0.4 # Using a specific stable tag is recommended
#)
#
## Make the content available (downloads if not already present and adds to build)
#FetchContent_MakeAvailable(moodycamel_concurrent_queue)
#
## Get the source directory of the fetched content
#FetchContent_GetProperties(moodycamel_concurrent_queue)
## The source directory will be in moodycamel_concurrent_queue_SOURCE_DIR (variable name is <lcName>_SOURCE_DIR)
#if(NOT moodycamel_concurrent_queue_POPULATED) # Check if it was populated (downloaded/found)
#    message(FATAL_ERROR "FetchContent_MakeAvailable failed to populate moodycamel_concurrent_queue")
#endif()

#ofline moodycamel
set(moodycamel_concurrent_queue_SOURCE_DIR /home/ubuntu/external_libs/moodycamel_concurrent_queue) # Path inside the container

set(CPPLOGGER_DIR "/home/ubuntu/external_libs/CppLogging")
set(CPPCOMMON_DIR "/home/ubuntu/external_libs/CppCommon")

# Find required packages with debugging
message(STATUS "Finding Threads...")
find_package(Threads REQUIRED)
message(STATUS "Threads found successfully")

message(STATUS "Finding fmt...")
find_package(fmt REQUIRED)
message(STATUS "fmt found successfully")

message(STATUS "Finding DiscoveryProtocol...")
find_package(DiscoveryProtocol REQUIRED)
message(STATUS "DiscoveryProtocol found successfully")

message(STATUS "Finding TransportLib...")
find_package(TransportLib REQUIRED)
message(STATUS "TransportLib found successfully")

# Logging configuration options
set(BRIDGE_LOG_MODE "BINARY" CACHE STRING "Logging mode: BINARY or TEXT")
set_property(CACHE BRIDGE_LOG_MODE PROPERTY STRINGS "BINARY" "TEXT")

# Log level preset options (mutually exclusive with direct level)
set(BRIDGE_LOG_LEVEL_PRESET "" CACHE STRING "Log level preset (NONE, FATAL, ERROR, WARN, INFO, DEBUG, ALL)")
set(BRIDGE_LOG_LEVEL "" CACHE STRING "Direct log level hex mask (e.g., 0x9F)")

# Validate logging mode
if(NOT BRIDGE_LOG_MODE MATCHES "^(BINARY|TEXT)$")
    message(FATAL_ERROR "BRIDGE_LOG_MODE must be either 'BINARY' or 'TEXT', got: ${BRIDGE_LOG_MODE}")
endif()

# Add the executable for your bridge application
add_executable(bridge
    src/main.cpp
    src/BridgeControlPlane.cpp
    src/MGWCPConnection.cpp
    src/SessionManager.cpp
    src/GlobalConfig.cpp
    src/RoutingTable.cpp
    src/TransportHandler.cpp
    src/configure_logger.cpp
)

# Explicitly add include directories
target_include_directories(bridge
    PRIVATE
    ${moodycamel_concurrent_queue_SOURCE_DIR}
    ${CPPLOGGER_DIR}/include
    ${CPPCOMMON_DIR}/include
)

target_compile_definitions(bridge
    PRIVATE
    APP_NAME="Bridge"
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    ACTIVE_LOG_LEVEL=LOG_LEVEL_WARN
)

# Set logging mode compile definitions
if(BRIDGE_LOG_MODE STREQUAL "BINARY")
    add_compile_definitions(BRIDGE_LOG_MODE_BINARY)
elseif(BRIDGE_LOG_MODE STREQUAL "TEXT")
    add_compile_definitions(BRIDGE_LOG_MODE_TEXT)
endif()

# Configure log level compile definitions
if(BRIDGE_LOG_LEVEL_PRESET)
    string(TOUPPER ${BRIDGE_LOG_LEVEL_PRESET} PRESET_UPPER)
    target_compile_definitions(bridge PRIVATE BRIDGE_LOG_LEVEL_PRESET_${PRESET_UPPER})
    message(STATUS "Configured log level preset: ${BRIDGE_LOG_LEVEL_PRESET}")
elseif(BRIDGE_LOG_LEVEL)
    target_compile_definitions(bridge PRIVATE BRIDGE_LOG_LEVEL=${BRIDGE_LOG_LEVEL})
    message(STATUS "Configured direct log level: ${BRIDGE_LOG_LEVEL}")
else()
    message(STATUS "Using default log level: INFO (0x9F)")
endif()

# Find CppCommon library
find_library(CPPCOMMON_LIBRARY
    NAMES cppcommon # CMake searches for libcppcommon.a or .so
    HINTS ${CPPCOMMON_DIR}/bin ${CPPCOMMON_DIR}/build # Prefer build directory if .a is there
    # Add more specific paths if necessary, e.g., ${CPPCOMMON_DIR}/build/lib
)
if(NOT CPPCOMMON_LIBRARY)
    message(FATAL_ERROR "CppCommon library not found. Searched in ${CPPCOMMON_DIR}/build and ${CPPCOMMON_DIR}/bin. Ensure it's built.")
endif()

# Find CppLogging library
find_library(CPPLOGGING_LIBRARY
    NAMES cpplogging # CMake searches for libcpplogging.a or .so
    HINTS ${CPPLOGGER_DIR}/bin ${CPPLOGGER_DIR}/build # Prefer build directory
)
if(NOT CPPLOGGING_LIBRARY)
    message(FATAL_ERROR "CppLogging library not found. Searched in ${CPPLOGGER_DIR}/build and ${CPPLOGGER_DIR}/bin. Ensure it's built.")
endif()

# --- Fetch nlohmann_json via FetchContent ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Link libraries
target_link_libraries(bridge
    PRIVATE
    Threads::Threads
    DiscoveryProtocol::discovery_protocol  # Link the discovery protocol library
    TransportLib::transport
    nlohmann_json::nlohmann_json

    fmt::fmt                # Link against the fmt library found by find_package
    ${CPPLOGGING_LIBRARY}   # Link against the found CppLogging library
    ${CPPCOMMON_LIBRARY}    # Link against the found CppCommon library

    # Add system libraries required by CppCommon
    bfd                     # For libbfd (binary file descriptor library)
    uuid                    # For libuuid (UUID generation)
    dl
    z
    rt                      # For dynamic linking (dlopen, dlsym, etc.)
                            # rt might also be needed on some systems for CppCommon, but start without
)
# Optional: Install rule for the bridge executable
# install(TARGETS bridge
#     RUNTIME DESTINATION bin
# )