cmake_minimum_required(VERSION 3.8)
project(path_planner)

set(OpenCV_DIR /usr/lib/x86_64-linux-gnu/cmake/opencv4)

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
# uncomment the following section in order to fill in
# further dependencies manually.
# find_package(<dependency> REQUIRED)
add_executable(astar src/Astar.cpp src/AstarClass.cpp ../../lib/math/ROS2_math.cpp ../../lib/topicClasses/MapMessage.cpp ../../lib/topicClasses/RobotPositionMessage.cpp)
ament_target_dependencies(astar rclcpp std_msgs nav_msgs visualization_msgs tf2_ros tf2_geometry_msgs services rosgraph_msgs tf2 OpenCV)

# target_link_libraries(astar ${OpenCV_LIBS})


install(TARGETS
  astar
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
