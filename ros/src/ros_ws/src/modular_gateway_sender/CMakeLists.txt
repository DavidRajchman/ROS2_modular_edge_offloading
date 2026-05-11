cmake_minimum_required(VERSION 3.8)
project(modular_gateway_sender)

# ------------------------------------------------------------------
# Logging configuration (user-adjustable via cmake command line)
# ------------------------------------------------------------------
# MGW_LOG_MODE: BINARY or TEXT
set(MGW_LOG_MODE "BINARY" CACHE STRING "Logging output mode: BINARY or TEXT")
set_property(CACHE MGW_LOG_MODE PROPERTY STRINGS BINARY TEXT)

# MGW_LOG_LEVEL_PRESET: convenience presets (optional)
set(MGW_LOG_LEVEL_PRESET "" CACHE STRING "Log level preset: NONE FATAL ERROR WARN INFO DEBUG ALL (leave empty for direct MGW_LOG_LEVEL)")
set_property(CACHE MGW_LOG_LEVEL_PRESET PROPERTY STRINGS "" NONE FATAL ERROR WARN INFO DEBUG ALL)

# MGW_LOG_LEVEL: direct hex mask (used when no preset specified)
set(MGW_LOG_LEVEL "0x9F" CACHE STRING "Direct logging level mask (e.g. 0x9F INFO, 0xBF DEBUG, 0xFF ALL)")

# Apply logging mode
if(MGW_LOG_MODE STREQUAL "TEXT")
    add_compile_definitions(MGW_LOG_MODE_TEXT)
else()
    add_compile_definitions(MGW_LOG_MODE_BINARY)
endif()

# Apply logging level (preset takes precedence over direct)
if(NOT MGW_LOG_LEVEL_PRESET STREQUAL "")
    add_compile_definitions(MGW_LOG_LEVEL_PRESET_${MGW_LOG_LEVEL_PRESET})
    message(STATUS "MGW log level: preset ${MGW_LOG_LEVEL_PRESET}")
else()
    add_compile_definitions(MGW_LOG_LEVEL=${MGW_LOG_LEVEL})
    message(STATUS "MGW log level: direct ${MGW_LOG_LEVEL}")
endif()

# ------------------------------------------------------------------
# Existing project configuration continues below
# ------------------------------------------------------------------

# Set version information
add_definitions(-DVERSION_MAJOR=0)
add_definitions(-DVERSION_MINOR=20)
add_definitions(-DVERSION_PATCH=0)



# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()


# --- Find Dependencies ---
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(Threads REQUIRED)
# Add /usr/local to the search path for packages installed outside the workspace
list(APPEND CMAKE_PREFIX_PATH "/usr/local")
#find_package(ControlPlaneProtocol REQUIRED)
find_package(DiscoveryProtocol REQUIRED)

# --- fmt: use the version bundled inside CppCommon (same one CppLogging was compiled against) ---
set(FMT_BUNDLED_INCLUDE_DIR "${CPPCOMMON_DIR}/modules/fmt/include")
set(FMT_BUNDLED_LIB "")
# Search for pre-built libfmt from CppCommon's build tree
find_library(FMT_BUNDLED_LIBRARY
    NAMES fmt
    HINTS
        "${CPPCOMMON_DIR}/build/_deps/fmt-build"
        "${CPPCOMMON_DIR}/build/modules/fmt"
        "${CPPCOMMON_DIR}/build"
        "/usr/local/lib"
)
if(FMT_BUNDLED_LIBRARY)
    message(STATUS "Found bundled fmt library: ${FMT_BUNDLED_LIBRARY}")
else()
    # Fallback: header-only mode (fmt is mostly header-only for our usage)
    message(STATUS "fmt library not found as pre-built, using header-only from: ${FMT_BUNDLED_INCLUDE_DIR}")
endif()

# --- Generate ROS 2 Service Definitions ---
rosidl_generate_interfaces(${PROJECT_NAME}
  "srv/RequestOffloading.srv"
  "srv/TerminateOffloading.srv"
  DEPENDENCIES std_msgs
)

# --- Define paths to external libraries ---
set(CPPLOGGER_DIR "/home/ubuntu/external_libs/CppLogging")
set(CPPCOMMON_DIR "/home/ubuntu/external_libs/CppCommon")
set(MOODYCAMEL_DIR "/home/ubuntu/external_libs/readerwriterqueue")

# --- Fetch nlohmann_json via FetchContent ---
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- Find pre-built external libraries ---
find_library(CPPCOMMON_LIBRARY
    NAMES cppcommon
    HINTS ${CPPCOMMON_DIR}/bin ${CPPCOMMON_DIR}/build
    REQUIRED
)
message(STATUS "Found CppCommon library: ${CPPCOMMON_LIBRARY}")

find_library(CPPLOGGING_LIBRARY
    NAMES cpplogging
    HINTS ${CPPLOGGER_DIR}/bin ${CPPLOGGER_DIR}/build
    REQUIRED
)
message(STATUS "Found CppLogging library: ${CPPLOGGING_LIBRARY}")

# --- Global Include Directories ---
include_directories(
  include
  ${MOODYCAMEL_DIR}
)

# --- Library Definitions ---

# Transport library (TCP client/server implementations)
add_library(gateway_transport_lib
  src/transport/tcp_client_transport.cpp
  src/transport/tcp_server_transport.cpp
)
target_include_directories(gateway_transport_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
    $<BUILD_INTERFACE:${MOODYCAMEL_DIR}>
)
target_link_libraries(gateway_transport_lib
    Threads::Threads
    ${CPPLOGGING_LIBRARY}
    ${CPPCOMMON_LIBRARY}
    bfd
    uuid
    dl
    z
    rt
)
if(FMT_BUNDLED_LIBRARY)
    target_link_libraries(gateway_transport_lib ${FMT_BUNDLED_LIBRARY})
endif()
target_include_directories(gateway_transport_lib PUBLIC
    $<BUILD_INTERFACE:${FMT_BUNDLED_INCLUDE_DIR}>)

# Message Handler Base library & RosGateway library (Combined to avoid circular dependency)
add_library(message_handler_base_lib src/message_handler_base.cpp src/ros_gateway.cpp)
target_include_directories(message_handler_base_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
)
target_link_libraries(message_handler_base_lib
    gateway_transport_lib
    ${CPPLOGGING_LIBRARY}
    ${CPPCOMMON_LIBRARY}
    bfd
    uuid
    dl
    z
    rt
)
if(FMT_BUNDLED_LIBRARY)
    target_link_libraries(message_handler_base_lib ${FMT_BUNDLED_LIBRARY})
endif()
ament_target_dependencies(message_handler_base_lib rclcpp)

# Handlers library (all concrete message handlers)
add_library(handlers_lib
  src/handlers/string_handler.cpp
  src/handlers/laserscan_handler.cpp
  src/handlers/string_test_input_handler.cpp
  src/handlers/string_test_result_handler.cpp
  src/handlers/serial_string_handler.cpp
  src/handlers/joint_state_handler.cpp
  src/handlers/serial_feedback_handler.cpp
)


target_include_directories(handlers_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(handlers_lib message_handler_base_lib)
ament_target_dependencies(handlers_lib rclcpp std_msgs sensor_msgs)

# GatewayController library (Control Plane core)
add_library(gateway_controller_lib
    src/gateway_controller.cpp
    src/logging_setup.cpp
    src/discovery_client.cpp
    src/bridge_cp_client.cpp
    src/handler_factory.cpp
)
target_include_directories(gateway_controller_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
)
# Use the proper CMake targets instead of manually found library paths
target_link_libraries(gateway_controller_lib 
    message_handler_base_lib
    handlers_lib
    #ControlPlaneProtocol::control_plane_protocol
    DiscoveryProtocol::discovery_protocol
    nlohmann_json::nlohmann_json
)
ament_target_dependencies(gateway_controller_lib rclcpp)

# --- Executable Definition ---
# The single, unified executable for the new architecture.
add_executable(gateway_controller src/main.cpp)
target_link_libraries(gateway_controller
    gateway_controller_lib
    Threads::Threads
    ${CPPLOGGING_LIBRARY}
    ${CPPCOMMON_LIBRARY}
    bfd
    uuid
    dl
    z
    rt
)
if(FMT_BUNDLED_LIBRARY)
    target_link_libraries(gateway_controller ${FMT_BUNDLED_LIBRARY})
endif()
ament_target_dependencies(gateway_controller rclcpp)

# MEC Gateway executable with MEC-specific defaults
add_executable(mec_gateway src/mec_main.cpp)
target_link_libraries(mec_gateway
    gateway_controller_lib
    Threads::Threads
    ${CPPLOGGING_LIBRARY}
    ${CPPCOMMON_LIBRARY}
    bfd
    uuid
    dl
    z
    rt
)
if(FMT_BUNDLED_LIBRARY)
    target_link_libraries(mec_gateway ${FMT_BUNDLED_LIBRARY})
endif()
ament_target_dependencies(mec_gateway rclcpp)

# --- Make sure the service headers are available to the gateway_controller_lib ---
rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(gateway_controller_lib "${cpp_typesupport_target}")

# --- Installation ---
install(TARGETS
  gateway_controller
  mec_gateway
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  gateway_transport_lib
  message_handler_base_lib
  handlers_lib
  gateway_controller_lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(
  DIRECTORY include/
  DESTINATION include
)

install(
    DIRECTORY srv
    DESTINATION share/${PROJECT_NAME}
)

install(
  DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

install(
  DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

ament_package()