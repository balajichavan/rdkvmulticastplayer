# CMakeLists.txt - MulticastPlayer Thunder plugin

cmake_minimum_required(VERSION 3.10)

project(MulticastPlayer)

set(PLUGIN_NAME MulticastPlayer)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(WPEFramework CONFIG REQUIRED)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gobject-2.0)

add_library(${MODULE_NAME} SHARED
    Module.cpp
    MulticastPlayer.cpp
    GstMulticastPipeline.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    SUFFIX ".so"
    PREFIX "lib"
)

target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    PkgConfig::GSTREAMER
    PkgConfig::GLIB
)

target_compile_options(${MODULE_NAME} PRIVATE
    ${GSTREAMER_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${STORAGE_DIRECTORY}/plugins
)

write_config(${PLUGIN_NAME})
