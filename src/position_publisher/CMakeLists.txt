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


include_directories(${OpenCV_INCLUDE_DIRS})
find_package(OpenCV REQUIRED)


add_executable(position src/PositionPublisher.cpp src/PositionPublisherClass.cpp ../../lib/math/ROS2_math.cpp ../../lib/topicClasses/RobotPositionMessage.cpp ../../lib/topicClasses/MapMessage.cpp )
ament_target_dependencies(position rclcpp std_msgs nav_msgs visualization_msgs tf2_ros tf2_geometry_msgs services rosgraph_msgs tf2 OpenCV)

install(TARGETS
  position
  DESTINATION lib/${PROJECT_NAME})



if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()



ament_package()
