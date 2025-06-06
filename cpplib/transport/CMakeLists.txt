set(CMAKE_VERBOSE_MAKEFILE ON) 
cmake_minimum_required(VERSION 3.10)
project(TransportLib VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Important for shared libraries

add_library(transport SHARED
    src/tcp_client_transport.cpp
    src/tcp_server_transport.cpp
)

target_include_directories(transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include> # Headers will be installed to <prefix>/include
)

# Pass version definitions to the compiler for logging_utils.hpp
target_compile_definitions(transport
    PRIVATE
    APP_NAME="TransportLIB"   # Define APP_NAME as the string TransportLIB
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    ACTIVE_LOG_LEVEL=LOG_LEVEL_WARN
)



# Link against pthreads for std::thread
find_package(Threads REQUIRED)
target_link_libraries(transport PRIVATE Threads::Threads)

# Install targets and headers
include(GNUInstallDirs)
install(TARGETS transport
    EXPORT TransportLibTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Installs the 'transport' directory from 'include/' into '<prefix>/include/transport'
install(DIRECTORY include/transport/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/transport
)

# Generate and install CMake package configuration files
# This allows 'find_package(TransportLib)' to work for other projects

install(EXPORT TransportLibTargets
    FILE TransportLibTargets.cmake
    NAMESPACE TransportLib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TransportLib
)

include(CMakePackageConfigHelpers)

# Configure the package config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TransportLibConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TransportLibConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/TransportLib"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Configure the version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TransportLibConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TransportLibConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TransportLibConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TransportLib
)