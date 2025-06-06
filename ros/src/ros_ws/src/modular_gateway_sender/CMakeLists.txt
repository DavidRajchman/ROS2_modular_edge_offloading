cmake_minimum_required(VERSION 3.8)
project(modular_gateway_sender)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Set version information
add_definitions(-DVERSION_MAJOR=0)
add_definitions(-DVERSION_MINOR=16)
add_definitions(-DVERSION_PATCH=0)

# --- Compile-Time Log Level Control ---
# Set the desired log level here using its numeric value.
# Your logging_utils.hpp defines these levels:
#   4 = DEBUG (all custom logs enabled)
#   3 = INFO
#   2 = WARN
#   1 = ERROR
#   0 = NONE (all custom logs disabled)
# Change the '4' in the line below to the desired numeric log level.
set(PROJECT_ACTIVE_LOG_LEVEL 2) # Default to DEBUG.

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# Include directories
include_directories(include)

# Add handlers here
add_library(handlers
  src/handlers/string_handler.cpp
  src/handlers/laserscan_handler.cpp
  src/handlers/string_test_input_handler.cpp  
  src/handlers/string_test_result_handler.cpp

)

target_link_libraries(handlers
  message_handler_base
)

# Libraries needed for all handlers
ament_target_dependencies(handlers
  rclcpp
  std_msgs
  sensor_msgs
)

# Transport library
add_library(gateway_transport_lib
  src/transport/tcp_client_transport.cpp
  src/transport/tcp_server_transport.cpp
)

ament_target_dependencies(gateway_transport_lib
  rclcpp
)

# ROS Gateway library (depends on transport)
add_library(ros_gateway_lib
  src/ros_gateway.cpp
)

target_link_libraries(ros_gateway_lib
  gateway_transport_lib
)

ament_target_dependencies(ros_gateway_lib
  rclcpp
)

# Message handler base
add_library(message_handler_base
  src/message_handler_base.cpp
)

target_link_libraries(message_handler_base
  ros_gateway_lib
)

ament_target_dependencies(message_handler_base
  rclcpp
)





# Client executable
add_executable(gateway_client src/gateway_client_main.cpp)
target_link_libraries(gateway_client
  ros_gateway_lib
  handlers
)
ament_target_dependencies(gateway_client
  rclcpp
  std_msgs
  sensor_msgs
)

# Server executable
add_executable(gateway_server src/gateway_server_main.cpp)
target_link_libraries(gateway_server
  ros_gateway_lib
  handlers
)
ament_target_dependencies(gateway_server
  rclcpp
  std_msgs
  sensor_msgs
)

# VHC executable
add_executable(gateway_VHC src/gateway_VHC_main.cpp)
target_link_libraries(gateway_VHC
  ros_gateway_lib
  handlers
)
ament_target_dependencies(gateway_VHC
  rclcpp
  std_msgs
  sensor_msgs
)

# MEC executable
add_executable(gateway_MEC src/gateway_MEC_main.cpp)
target_link_libraries(gateway_MEC
  ros_gateway_lib
  handlers
)
ament_target_dependencies(gateway_MEC
  rclcpp
  std_msgs
  sensor_msgs
)

# Install executables
install(TARGETS
  gateway_client
  gateway_server
  gateway_VHC
  gateway_MEC
  DESTINATION lib/${PROJECT_NAME}
)

# Install libraries
install(TARGETS
  gateway_transport_lib
  ros_gateway_lib
  message_handler_base
  handlers
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install include directory
install(DIRECTORY include/
  DESTINATION include
)

ament_package()