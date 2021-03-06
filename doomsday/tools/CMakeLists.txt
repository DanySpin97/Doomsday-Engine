# Doomsday Engine - Tools

cmake_minimum_required (VERSION 3.1)
project (Tools)
include (../cmake/Config.cmake)

# Amethyst is expected to go to (e.g.) /usr/local/bin, while the rest
# goes to distrib/products/. Compile and install it manually beforehand.
#
# add_subdirectory (amethyst)

add_subdirectory (doomsdayscript)
add_subdirectory (md2tool)
add_subdirectory (savegametool)
if (DENG_ENABLE_GUI)
    add_subdirectory (shell)
endif ()
if (UNIX)
    add_subdirectory (shell-text) # requires ncurses
endif ()
add_subdirectory (texc)
add_subdirectory (wadtool)

if (APPLE AND TARGET client)
    # Tools install files into the Doomsday.app bundle, so it must now be
    # signed again.
    deng_codesign(client)
endif ()
