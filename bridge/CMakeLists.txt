cmake_minimum_required(VERSION 3.10)
project(bridge VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add the executable
add_executable(bridge
    src/main.cpp
)

# Link libraries
target_link_libraries(bridge
    PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
)

# Install targets
install(TARGETS bridge
    RUNTIME DESTINATION bin
)
