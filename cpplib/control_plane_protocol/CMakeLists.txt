cmake_minimum_required(VERSION 3.16)
project(ControlPlaneProtocol VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Dependencies ---
# This library requires nlohmann_json. We use find_package to locate it.
# The main project will need to ensure it's available (e.g., via FetchContent).
find_package(nlohmann_json 3.2.0 REQUIRED)

# --- Add the Library ---
# Since our protocol is defined entirely in headers, we create an INTERFACE library.
# This is a target that doesn't compile any sources itself but can have properties
# like include directories and dependencies that are passed to any target that links it.
add_library(control_plane_protocol INTERFACE)
add_library(ControlPlaneProtocol::control_plane_protocol ALIAS control_plane_protocol)

# --- Set Include Directories ---
# Targets that link against control_plane_protocol will automatically get this
# include directory added to their include paths.
target_include_directories(control_plane_protocol
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include> # For installed headers
)

# --- Set Dependencies ---
# Any target that links against our library will also automatically link against
# nlohmann_json. This is called a transitive dependency.
target_link_libraries(control_plane_protocol INTERFACE nlohmann_json::nlohmann_json)


# --- Installation ---
include(GNUInstallDirs)

# Install the headers to the standard include directory.
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the INTERFACE library target. This command doesn't install any files for
# an INTERFACE library, but it crucially adds the target to the specified export set,
# making it available for the install(EXPORT ...) command below.
install(TARGETS control_plane_protocol
    EXPORT ControlPlaneProtocolTargets
)

# --- CMake Package Configuration ---
# This section generates the files that allow `find_package(ControlPlaneProtocol)` to work.
# This command now works because ControlPlaneProtocolTargets was created above.
install(EXPORT ControlPlaneProtocolTargets
    FILE ControlPlaneProtocolTargets.cmake
    NAMESPACE ControlPlaneProtocol::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ControlPlaneProtocol
)

include(CMakePackageConfigHelpers)

# Configure the package config file from a template.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ControlPlaneProtocolConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ControlPlaneProtocolConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ControlPlaneProtocol"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# Configure the version file.
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ControlPlaneProtocolConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Install the generated CMake configuration files.
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ControlPlaneProtocolConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ControlPlaneProtocolConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ControlPlaneProtocol
)