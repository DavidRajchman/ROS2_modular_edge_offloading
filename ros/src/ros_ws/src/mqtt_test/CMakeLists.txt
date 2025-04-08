cmake_minimum_required(VERSION 3.8)
project(mqtt_test)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Add verbose output to debug library search
message(STATUS "Looking for Paho MQTT libraries...")

# Explicitly set the library paths based on your system
set(PAHO_MQTT_C_LIB "/usr/lib/x86_64-linux-gnu/libpaho-mqtt3as.so")
set(PAHO_MQTT_CPP_LIB "/usr/lib/x86_64-linux-gnu/libpaho-mqttpp3.so")

# Debug output to verify library paths
message(STATUS "Using PAHO_MQTT_C_LIB: ${PAHO_MQTT_C_LIB}")
message(STATUS "Using PAHO_MQTT_CPP_LIB: ${PAHO_MQTT_CPP_LIB}")

# Add the executable
add_executable(mqtt_test_node src/mqtt_test_node.cpp)

# Include directories
target_include_directories(mqtt_test_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
  ${rclcpp_INCLUDE_DIRS}
  ${std_msgs_INCLUDE_DIRS}
  /usr/include  # Explicitly include system headers
)

# Link libraries - specify exact library files
target_link_libraries(mqtt_test_node
  ${rclcpp_LIBRARIES}
  ${std_msgs_LIBRARIES}
  ${PAHO_MQTT_CPP_LIB}
  ${PAHO_MQTT_C_LIB}
  pthread
)

# Set compiler features
target_compile_features(mqtt_test_node PUBLIC c_std_99 cxx_std_17)

# Install targets
install(TARGETS mqtt_test_node
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()