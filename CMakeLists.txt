cmake_minimum_required(VERSION 3.15)

project(avs_c_api_loader VERSION 1.1.0 LANGUAGES CXX)

message(STATUS "Configuring avs_c_api_loader...")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(AvisynthPlus REQUIRED)

add_library(avs_c_api_loader STATIC
    src/avs_c_api_functions.inc
    src/avs_c_api_loader.cpp
    src/avs_c_api_loader.hpp
)

add_library(avs_c_api_loader::avs_c_api_loader ALIAS avs_c_api_loader)

target_compile_features(avs_c_api_loader PUBLIC cxx_std_20)

target_link_libraries(avs_c_api_loader PUBLIC AvisynthPlus::headers)

target_include_directories(avs_c_api_loader PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# --- Installation Rules ---
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

install(FILES cmake/FindAvisynthPlus.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avs_c_api_loader
)

install(TARGETS avs_c_api_loader
    EXPORT avs_c_api_loader_targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES src/avs_c_api_loader.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(
    "cmake/avs_c_api_loader-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/avs_c_api_loader-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avs_c_api_loader
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/avs_c_api_loader-config.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avs_c_api_loader
)

install(EXPORT avs_c_api_loader_targets
    FILE        avs_c_api_loader-targets.cmake
    NAMESPACE   avs_c_api_loader::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avs_c_api_loader
)
