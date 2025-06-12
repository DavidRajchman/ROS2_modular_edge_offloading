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
set(PROJECT_ACTIVE_LOG_LEVEL 2) # Default to DEBUG.

include(FetchContent)
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG 11.0.2  # Use a version of fmt that CppLogging is compatible with (e.g., 11.x)
  CMAKE_ARGS -DBUILD_SHARED_LIBS=OFF -DFMT_HEADER_ONLY=OFF # Ensure fmt is built as a static, compiled library

)
# Set options to avoid building tests or installing fmt from this build
set(FMT_TEST OFF CACHE BOOL "Disable building of fmt tests" FORCE)
FetchContent_MakeAvailable(fmt) # This makes the fmt::fmt target available from the fetched source


# --- Find Dependencies ---
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(Threads REQUIRED) # Ensure Threads is found

# The FMT_SOURCE_DIR logic can be simplified or removed if you always rely on the Dockerfile-installed fmt.
# For simplicity, we'll rely on the find_package(fmt REQUIRED) above.
# If you had a specific FMT_SOURCE_DIR, you would set it here:
# set(FMT_SOURCE_DIR "/path/to/correct/fmt/source")
# if(FMT_SOURCE_DIR AND EXISTS "${FMT_SOURCE_DIR}/CMakeLists.txt")
#   message(STATUS "Using local fmt from ${FMT_SOURCE_DIR}, adding as subdirectory.")
#   set(FMT_INSTALL OFF CACHE BOOL "Disable installation of fmt" FORCE)
#   set(FMT_TEST OFF CACHE BOOL "Disable building of fmt tests" FORCE)
#   add_subdirectory(${FMT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/fmt_build EXCLUDE_FROM_ALL)
# else()
#   message(STATUS "FMT_SOURCE_DIR not set or invalid, relying on find_package(fmt) to find system fmt.")
#   # find_package(fmt REQUIRED) is already called above
# endif()

set(CPPLOGGER_DIR "/home/ubuntu/external_libs/CppLogging")
set(CPPCOMMON_DIR "/home/ubuntu/external_libs/CppCommon")

# Find CppCommon library
find_library(CPPCOMMON_LIBRARY
    NAMES cppcommon
    HINTS ${CPPCOMMON_DIR}/bin ${CPPCOMMON_DIR}/build
    REQUIRED
)
message(STATUS "Found CppCommon library: ${CPPCOMMON_LIBRARY}")

# Find CppLogging library
find_library(CPPLOGGING_LIBRARY
    NAMES cpplogging
    HINTS ${CPPLOGGER_DIR}/bin ${CPPLOGGER_DIR}/build
    REQUIRED
)
message(STATUS "Found CppLogging library: ${CPPLOGGING_LIBRARY}")

# --- Include Directories ---
include_directories(include)

# --- Library Definitions ---
# Add handlers here
add_library(handlers
  src/handlers/string_handler.cpp
  src/handlers/laserscan_handler.cpp
  src/handlers/string_test_input_handler.cpp  
  src/handlers/string_test_result_handler.cpp
)
target_link_libraries(handlers message_handler_base) # Changed from PUBLIC
ament_target_dependencies(handlers rclcpp std_msgs sensor_msgs)

# Transport library
add_library(gateway_transport_lib
  src/transport/tcp_client_transport.cpp
  src/transport/tcp_server_transport.cpp
)
ament_target_dependencies(gateway_transport_lib rclcpp)

# ROS Gateway library
add_library(ros_gateway_lib src/ros_gateway.cpp)
target_include_directories(ros_gateway_lib PUBLIC
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ros_gateway_lib gateway_transport_lib) # Changed from PUBLIC
ament_target_dependencies(ros_gateway_lib rclcpp)

# Message handler base
add_library(message_handler_base src/message_handler_base.cpp)
target_include_directories(message_handler_base PUBLIC
    $<BUILD_INTERFACE:${CPPLOGGER_DIR}/include>
    $<BUILD_INTERFACE:${CPPCOMMON_DIR}/include>
)
target_link_libraries(message_handler_base ros_gateway_lib) # Changed from PUBLIC
ament_target_dependencies(message_handler_base rclcpp)

# --- Executable Definitions ---
set(COMMON_EXEC_INCLUDE_DIRS
    PRIVATE
    # If executables directly use moodycamel, add its include dir here
    # ${MOODYCAMEL_CONCURRENTQUEUE_SOURCE_DIR}
    # CppLogger and CppCommon includes are typically brought in via linked libraries
)

# COMMON_EXEC_LIBS should be a plain list of libraries for plain signature linking
set(COMMON_EXEC_LIBS
    # PRIVATE # Keyword removed from the list itself
    ros_gateway_lib # This should bring its dependencies
    handlers
    Threads::Threads      # Link pthreads
    ${CPPLOGGING_LIBRARY}
    ${CPPCOMMON_LIBRARY}
    fmt::fmt              # Link fmt library

    bfd
    uuid
    dl
    z
    rt
)

# Client executable
add_executable(gateway_client src/gateway_client_main.cpp)
target_include_directories(gateway_client ${COMMON_EXEC_INCLUDE_DIRS})
target_link_libraries(gateway_client ${COMMON_EXEC_LIBS}) # This will now be a plain signature call
ament_target_dependencies(gateway_client rclcpp std_msgs sensor_msgs)

# Server executable
add_executable(gateway_server src/gateway_server_main.cpp)
target_include_directories(gateway_server ${COMMON_EXEC_INCLUDE_DIRS})
target_link_libraries(gateway_server ${COMMON_EXEC_LIBS}) # Plain signature call
ament_target_dependencies(gateway_server rclcpp std_msgs sensor_msgs)

# VHC executable
add_executable(gateway_VHC src/gateway_VHC_main.cpp)
target_include_directories(gateway_VHC ${COMMON_EXEC_INCLUDE_DIRS})
target_link_libraries(gateway_VHC ${COMMON_EXEC_LIBS}) # Plain signature call
ament_target_dependencies(gateway_VHC rclcpp std_msgs sensor_msgs)

# MEC executable
add_executable(gateway_MEC src/gateway_MEC_main.cpp)
target_include_directories(gateway_MEC ${COMMON_EXEC_INCLUDE_DIRS})
target_link_libraries(gateway_MEC ${COMMON_EXEC_LIBS}) # Plain signature call
ament_target_dependencies(gateway_MEC rclcpp std_msgs sensor_msgs)

# --- Installation ---
# ...existing code...
install(TARGETS
  gateway_client
  gateway_server
  gateway_VHC
  gateway_MEC
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  gateway_transport_lib
  ros_gateway_lib
  message_handler_base
  handlers
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)

ament_package()