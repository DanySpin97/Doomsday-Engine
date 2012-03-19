# The Doomsday Engine Project
# Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2012 Daniel Swanson <danij@dengine.net>
#
# Do not modify this file. Custom CONFIG options can be specified on the 
# qmake command line or in config_user.pri.
#
# User-definable variables:
#   PREFIX          Install prefix for Unix
#   SCRIPT_PYTHON   Path of the Python interpreter binary to be used in
#                   generated scripts (python on path used for building)
#
# CONFIG options for Doomsday:
# - deng_aptstable          Include the stable apt repository .list
# - deng_aptunstable        Include the unstable apt repository .list
# - deng_fmod               Build the FMOD Ex sound driver
# - deng_nativesdk          (Mac) Use current OS's SDK for non-distrib use
# - deng_nofixedasm         Disable assembler fixed-point math
# - deng_nosdlmixer         Disable SDL_mixer; use dummy driver as default
# - deng_nosnowberry        (Unix) Exclude Snowberry from installation
# - deng_openal             Build the OpenAL sound driver
# - deng_nopackres          Do not package the Doomsday resources
# - deng_rangecheck         Parameter range checking/value assertions
# - deng_snowberry          Include Snowberry in installation
# - deng_writertypecheck    Enable type checking in Writer/Reader
#
# Read-only options (set automatically):
# - deng_debug              Debug build.

QT -= core gui
CONFIG *= thread

# Directories ----------------------------------------------------------------

DENG_API_DIR = $$PWD/engine/api
DENG_INCLUDE_DIR = $$PWD/engine/portable/include
DENG_UNIX_INCLUDE_DIR = $$PWD/engine/unix/include
DENG_MAC_INCLUDE_DIR = $$PWD/engine/mac/include
DENG_WIN_INCLUDE_DIR = $$PWD/engine/win32/include

# Binaries and generated files are placed here.
DENG_WIN_PRODUCTS_DIR = $$PWD/../distrib/products

# Versions -------------------------------------------------------------------

include(versions.pri)

# Macros ---------------------------------------------------------------------

defineTest(echo) {
    !win32 {
        message($$1)
    } else {
        # We don't want to get the printed messages after everything else,
        # so print to stdout.
        system(echo $$1)
    }
}

defineTest(useLibDir) {
    btype = ""
    win32 {
        deng_debug: btype = "/Debug"
              else: btype = "/Release"
    }
    exists($${1}$${btype}) {
        LIBS += -L$${1}$${btype}
        export(LIBS)
        return(true)
    }
    return(false)
}

defineTest(doPostLink) {
    isEmpty(QMAKE_POST_LINK) {
        QMAKE_POST_LINK = $$1
    } else {
        QMAKE_POST_LINK = $$QMAKE_POST_LINK && $$1
    }
    export(QMAKE_POST_LINK)
}

# Build Options --------------------------------------------------------------

# Configure for Debug/Release build.
CONFIG(debug, debug|release) {
    !win32: echo(Debug build.)
    DEFINES += _DEBUG
    CONFIG += deng_debug deng_rangecheck
} else {
    !win32: echo(Release build.)
    DEFINES += NDEBUG
}

# Check for a 64-bit compiler.
contains(QMAKE_HOST.arch, x86_64) {
    echo(64-bit architecture detected.)
    DEFINES += HOST_IS_64BIT
}

isStableRelease(): DEFINES += DENG_STABLE

# Options defined by the user (may not exist).
exists(config_user.pri): include(config_user.pri)

    win32: include(config_win32.pri)
else:macx: include(config_macx.pri)
     else: include(config_unix.pri)

# Apply deng_* Configuration -------------------------------------------------

deng_nofixedasm {
    DEFINES += DENG_NO_FIXED_ASM
}
!deng_rangecheck {
    DEFINES += DENG_NO_RANGECHECKING
}
deng_nosdlmixer {
    DEFINES += DENG_DISABLE_SDLMIXER
}
