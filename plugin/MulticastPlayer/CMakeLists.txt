# CMakeLists.txt - MulticastPlayer Thunder plugin
#
# Builds the org.rdk.MulticastPlayer WPEFramework/Thunder plugin against the
# platform GStreamer stack. Intended to be built inside an RDK/Yocto SDK where
# WPEFramework (Thunder) and GStreamer development packages are available.

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
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0 gobject-2.0)

add_library(${MODULE_NAME} SHARED
    Module.cpp
    MulticastPlayer.cpp
    GstMulticastPipeline.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    SUFFIX ".so"
    PREFIX "lib")

target_include_directories(${MODULE_NAME} PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS})

target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES})

target_compile_options(${MODULE_NAME} PRIVATE ${GSTREAMER_CFLAGS_OTHER})

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
