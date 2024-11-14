cmake_minimum_required(VERSION 3.8)
project(position_publisher)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(rosgraph_msgs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(services REQUIRED)
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)

# add_executable(position src/position.cpp)
# target_include_directories(position PUBLIC
#   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#   $<INSTALL_INTERFACE:include>)
# target_compile_features(position PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17
add_executable(position src/PositionPublisher.cpp src/PositionPublisherClass.cpp)
ament_target_dependencies(position rclcpp std_msgs nav_msgs visualization_msgs tf2_ros tf2_geometry_msgs services rosgraph_msgs tf2)

# add_executable(publisher src/publisher.cpp)
# ament_target_dependencies(publisher rclcpp std_msgs nav_msgs visualization_msgs tf2_ros tf2_geometry_msgs services)

install(TARGETS
  position
  # publisher
  DESTINATION lib/${PROJECT_NAME})



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
