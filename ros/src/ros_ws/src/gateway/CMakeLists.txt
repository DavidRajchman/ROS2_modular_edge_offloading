cmake_minimum_required(VERSION 3.8)
project(gateway)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Common include directories for all targets
set(GATEWAY_INCLUDE_DIRS
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
  ${rclcpp_INCLUDE_DIRS}
  ${std_msgs_INCLUDE_DIRS}
)

# Common libraries for all targets
set(GATEWAY_LIBRARIES
  ${rclcpp_LIBRARIES}
  ${std_msgs_LIBRARIES}
)


# New gateway sender node
add_executable(gateway_sender_node src/gateway_sender_node.cpp)
target_include_directories(gateway_sender_node PUBLIC ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(gateway_sender_node ${GATEWAY_LIBRARIES})
target_compile_features(gateway_sender_node PUBLIC c_std_99 cxx_std_17)

# New gateway receiver node
add_executable(gateway_receiver_node src/gateway_receiver_node.cpp)
target_include_directories(gateway_receiver_node PUBLIC ${GATEWAY_INCLUDE_DIRS})
target_link_libraries(gateway_receiver_node ${GATEWAY_LIBRARIES})
target_compile_features(gateway_receiver_node PUBLIC c_std_99 cxx_std_17)

# Install all targets
install(TARGETS
  gateway_sender_node
  gateway_receiver_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install headers if you create them
install(DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()