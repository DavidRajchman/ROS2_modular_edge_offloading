cmake_minimum_required(VERSION 3.10)
project(bridge VERSION 0.2.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include FetchContent module
include(FetchContent)

# Declare the dependency on moodycamel::ConcurrentQueue
FetchContent_Declare(
    moodycamel_concurrent_queue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG v1.0.4 # Using a specific stable tag is recommended
)

# Make the content available (downloads if not already present and adds to build)
FetchContent_MakeAvailable(moodycamel_concurrent_queue)

# Get the source directory of the fetched content
FetchContent_GetProperties(moodycamel_concurrent_queue)
# The source directory will be in moodycamel_concurrent_queue_SOURCE_DIR (variable name is <lcName>_SOURCE_DIR)
if(NOT moodycamel_concurrent_queue_POPULATED) # Check if it was populated (downloaded/found)
    message(FATAL_ERROR "FetchContent_MakeAvailable failed to populate moodycamel_concurrent_queue")
endif()

# Find required packages
find_package(Threads REQUIRED)
find_package(TransportLib REQUIRED)

# Add the executable for your bridge application
add_executable(bridge
    src/main.cpp
    src/RoutingTable.cpp
    src/TransportHandler.cpp
)

# Explicitly add the include directory for concurrentqueue
# This ensures that the compiler knows where to find "concurrentqueue.h"
target_include_directories(bridge
    PRIVATE
    ${moodycamel_concurrent_queue_SOURCE_DIR} # Add this line
)

target_compile_definitions(bridge
    PRIVATE
    APP_NAME="Bridge"
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    ACTIVE_LOG_LEVEL=LOG_LEVEL_WARN
)

# Link libraries
target_link_libraries(bridge
    PRIVATE
    Threads::Threads
    TransportLib::transport
    # moodycamel_concurrent_queue # Keep this if it's a valid target, CMake will warn if not.
                                # For header-only, often not strictly needed if includes are set.
)

# Optional: Install rule for the bridge executable
# install(TARGETS bridge
#     RUNTIME DESTINATION bin
# )