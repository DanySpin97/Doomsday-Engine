# Doomsday Engine - Applications

cmake_minimum_required (VERSION 3.1)
project (Apps)
include (../cmake/Config.cmake)

add_subdirectory (libdoomsday)
add_subdirectory (plugins)
if (DENG_ENABLE_SERVER AND NOT IOS)
    add_subdirectory (server)
endif ()
if (DENG_ENABLE_GUI)
    add_subdirectory (client)
endif ()
