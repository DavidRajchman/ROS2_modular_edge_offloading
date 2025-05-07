cmake_minimum_required(VERSION 3.10)
project(bridge VERSION 0.1.2)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output binary to bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required packages
# Threads is needed for std::thread
find_package(Threads REQUIRED)
# This will find your installed TransportLib using the CMake config files
# generated during its installation.
find_package(TransportLib REQUIRED)

# Add the executable for your bridge application
add_executable(bridge
    src/main.cpp
)

target_compile_definitions(bridge
    PRIVATE
    APP_NAME="Bridge"  # Define the application name for bridge logs
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# Link libraries
# Link against the imported target TransportLib::transport.
# This target, defined by TransportLib's CMake configuration,
# will automatically handle include directories and library paths for TransportLib.
# Also link against pthreads.
target_link_libraries(bridge
    PRIVATE
    Threads::Threads
    TransportLib::transport
)

# Optional: Install rule for the bridge executable
# install(TARGETS bridge
#     RUNTIME DESTINATION bin
# )