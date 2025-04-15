cmake_minimum_required(VERSION 3.8)
project(modular_gateway_sender)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Set version information
add_definitions(-DVERSION_MAJOR=0)
add_definitions(-DVERSION_MINOR=4)
add_definitions(-DVERSION_PATCH=0)

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Include directories
include_directories(include)

# Create library with common code
add_library(gateway_transport_lib
  src/tcp_transport.cpp
  src/ros_gateway.cpp
)
ament_target_dependencies(gateway_transport_lib
  rclcpp
  std_msgs
)

# String gateway executable
add_executable(string_gateway src/string_gateway.cpp)
target_link_libraries(string_gateway gateway_transport_lib)
ament_target_dependencies(string_gateway
  rclcpp
  std_msgs
)

# Install executables
install(TARGETS
  string_gateway
  DESTINATION lib/${PROJECT_NAME}
)

# Install library
install(TARGETS
  gateway_transport_lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install include directory
install(DIRECTORY include/
  DESTINATION include
)

ament_package()