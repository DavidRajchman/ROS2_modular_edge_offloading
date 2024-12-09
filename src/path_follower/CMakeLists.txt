cmake_minimum_required(VERSION 3.8)
project(path_follower)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(services REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
find_package(OpenCV REQUIRED)

add_executable(pursuit src/PursePursuit.cpp src/PursePursuitClass.cpp ../../lib/math/ROS2_math.cpp ../../lib/topicClasses/MotorMessage.cpp ../../lib/topicClasses/PathMessage.cpp ../../lib/topicClasses/RobotPositionMessage.cpp)
ament_target_dependencies(pursuit rclcpp std_msgs nav_msgs visualization_msgs  services tf2 tf2_ros OpenCV)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
