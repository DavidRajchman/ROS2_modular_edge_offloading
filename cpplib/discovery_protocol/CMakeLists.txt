cmake_minimum_required(VERSION 3.10)
project(DiscoveryProtocol VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Important for shared libraries

# Add the library
add_library(discovery_protocol SHARED
    src/protocol.cpp
)

# Set include directories
target_include_directories(discovery_protocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include> # Headers will be installed to <prefix>/include
)

# Install targets and headers
include(GNUInstallDirs)
install(TARGETS discovery_protocol
    EXPORT DiscoveryProtocolTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Installs the headers
install(DIRECTORY include/discovery_protocol/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/discovery_protocol
)

# Generate and install CMake package configuration files
# This allows 'find_package(DiscoveryProtocol)' to work for other projects
install(EXPORT DiscoveryProtocolTargets
    FILE DiscoveryProtocolTargets.cmake
    NAMESPACE DiscoveryProtocol::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DiscoveryProtocol
)

include(CMakePackageConfigHelpers)

# Configure the package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DiscoveryProtocolConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DiscoveryProtocolConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DiscoveryProtocol"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Configure the version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DiscoveryProtocolConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DiscoveryProtocolConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DiscoveryProtocolConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DiscoveryProtocol
)