cmake_minimum_required(VERSION 3.8)
project(modular_gateway_sender)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Set version information
add_definitions(-DVERSION_MAJOR=0)
add_definitions(-DVERSION_MINOR=20)
add_definitions(-DVERSION_PATCH=0)

# --- Find Dependencies ---
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)
# Add /usr/local to the search path for packages installed outside the workspace
list(APPEND CMAKE_PREFIX_PATH "/usr/local")
find_package(ControlPlaneProtocol REQUIRED)
find_package(DiscoveryProtocol REQUIRED)

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
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
    $<BUILD_INTERFACE:${MOODYCAMEL_DIR}>
    $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(gateway_transport_lib rclcpp)

# Message Handler Base library
add_library(message_handler_base_lib src/message_handler_base.cpp)
target_include_directories(message_handler_base_lib PUBLIC
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(message_handler_base_lib rclcpp)

# Handlers library (all concrete message handlers)
add_library(handlers_lib
  src/handlers/string_handler.cpp
  src/handlers/laserscan_handler.cpp
  src/handlers/string_test_input_handler.cpp
  src/handlers/string_test_result_handler.cpp
)
target_link_libraries(handlers_lib message_handler_base_lib)
ament_target_dependencies(handlers_lib rclcpp std_msgs sensor_msgs)

# RosGateway library (Data Plane core)
add_library(ros_gateway_lib src/ros_gateway.cpp)
target_link_libraries(ros_gateway_lib
  gateway_transport_lib
  message_handler_base_lib
)
ament_target_dependencies(ros_gateway_lib rclcpp)

# GatewayController library (Control Plane core)
add_library(gateway_controller_lib
    src/gateway_controller.cpp
    src/logging_setup.cpp
    src/discovery_client.cpp
    src/bridge_cp_client.cpp
    src/handler_factory.cpp
)
target_link_libraries(gateway_controller_lib
    ros_gateway_lib
    handlers_lib
    ${ControlPlaneProtocol_LIBRARIES}
    ${DiscoveryProtocol_LIBRARIES}
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
    fmt::fmt
    bfd
    uuid
    dl
    z
    rt
)
ament_target_dependencies(gateway_controller rclcpp)

# --- Installation ---
install(TARGETS
  gateway_controller
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  gateway_transport_lib
  ros_gateway_lib
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

ament_package()