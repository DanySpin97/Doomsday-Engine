# The Doomsday Engine Project
# Copyright (c) 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2012-2013 Daniel Swanson <danij@dengine.net>

TEMPLATE = app
TARGET = doomsday-server

# Build Configuration --------------------------------------------------------

include(../config.pri)

VERSION = $$DENG_VERSION
echo(Doomsday Server $${DENG_VERSION}.)

CONFIG -= app_bundle

# Some messy old code here:
*-g++* | *-gcc* | *-clang* {
    QMAKE_CXXFLAGS_WARN_ON += \
        -Wno-missing-field-initializers \
        -Wno-unused-parameter \
        -Wno-missing-braces
}

# External Dependencies ------------------------------------------------------

include(../dep_zlib.pri)
include(../dep_lzss.pri)
include(../dep_core.pri)
include(../dep_legacy.pri)
include(../dep_shell.pri)
include(../dep_doomsday.pri)

# Definitions ----------------------------------------------------------------

DEFINES += __DOOMSDAY__ __SERVER__

!isEmpty(DENG_BUILD) {
    !win32: echo(Build number: $$DENG_BUILD)
    DEFINES += DOOMSDAY_BUILD_TEXT=\\\"$$DENG_BUILD\\\"
} else {
    !win32: echo(DENG_BUILD is not defined.)
}

!win32:!macx {
    # Generic Unix.
    DEFINES += __USE_BSD _GNU_SOURCE=1
}

# Linking --------------------------------------------------------------------

win32 {
    RC_FILE = res/windows/doomsday.rc
    OTHER_FILES += $$RC_FILE

    deng_msvc: QMAKE_LFLAGS += /NODEFAULTLIB:libcmt

    LIBS += -lkernel32 -lgdi32 -lole32 -luser32 -lwsock32 -lopengl32 -lglu32
}
else:macx {
}
else {
    # Generic Unix.
    LIBS += -lX11 # TODO: Get rid of this! -jk

    !freebsd-*: LIBS += -ldl
}

# Source Files ---------------------------------------------------------------

# Prefix for source files (shared for now):
SRC = ../client

PRECOMPILED_HEADER = include/precompiled.h

HEADERS += \
    include/precompiled.h

DENG_API_HEADERS = \
    $$DENG_API_DIR/apis.h \
    $$DENG_API_DIR/api_audiod.h \
    $$DENG_API_DIR/api_audiod_mus.h \
    $$DENG_API_DIR/api_audiod_sfx.h \
    $$DENG_API_DIR/api_base.h \
    $$DENG_API_DIR/api_busy.h \
    $$DENG_API_DIR/api_console.h \
    $$DENG_API_DIR/api_def.h \
    $$DENG_API_DIR/api_event.h \
    $$DENG_API_DIR/api_gl.h \
    $$DENG_API_DIR/api_infine.h \
    $$DENG_API_DIR/api_internaldata.h \
    $$DENG_API_DIR/api_filesys.h \
    $$DENG_API_DIR/api_fontrender.h \
    $$DENG_API_DIR/api_gameexport.h \
    $$DENG_API_DIR/api_material.h \
    $$DENG_API_DIR/api_materialarchive.h \
    $$DENG_API_DIR/api_map.h \
    $$DENG_API_DIR/api_mapedit.h \
    $$DENG_API_DIR/api_player.h \
    $$DENG_API_DIR/api_plugin.h \
    $$DENG_API_DIR/api_render.h \
    $$DENG_API_DIR/api_resource.h \
    $$DENG_API_DIR/api_resourceclass.h \
    $$DENG_API_DIR/api_server.h \
    $$DENG_API_DIR/api_sound.h \
    $$DENG_API_DIR/api_svg.h \
    $$DENG_API_DIR/api_thinker.h \
    $$DENG_API_DIR/api_uri.h \
    $$DENG_API_DIR/dd_share.h \
    $$DENG_API_DIR/dd_types.h \
    $$DENG_API_DIR/dd_version.h \
    $$DENG_API_DIR/def_share.h \
    $$DENG_API_DIR/dengproject.h \
    $$DENG_API_DIR/doomsday.h \
    $$DENG_API_DIR/xgclass.h

# Convenience headers.
DENG_HEADERS += \
    $$SRC/include/EntityDatabase \
    $$SRC/include/Game \
    $$SRC/include/Games \
    $$SRC/include/Interceptor

# Private headers.
DENG_HEADERS += \
    include/remoteuser.h \
    include/server_dummies.h \
    include/shelluser.h \
    include/shellusers.h \
    include/serverapp.h \
    include/serversystem.h \
    include/server/sv_def.h \
    include/server/sv_frame.h \
    include/server/sv_infine.h \
    include/server/sv_missile.h \
    include/server/sv_pool.h \
    include/server/sv_sound.h \
    $$SRC/include/audio/s_cache.h \
    $$SRC/include/audio/s_environ.h \
    $$SRC/include/audio/s_main.h \
    $$SRC/include/busymode.h \
    $$SRC/include/color.h \
    $$SRC/include/con_config.h \
    $$SRC/include/dd_def.h \
    $$SRC/include/games.h \
    $$SRC/include/dd_loop.h \
    $$SRC/include/dd_main.h \
    $$SRC/include/dd_pinit.h \
    $$SRC/include/de_audio.h \
    $$SRC/include/de_base.h \
    $$SRC/include/de_console.h \
    $$SRC/include/de_defs.h \
    $$SRC/include/de_edit.h \
    $$SRC/include/de_filesys.h \
    $$SRC/include/de_graphics.h \
    $$SRC/include/de_infine.h \
    $$SRC/include/de_misc.h \
    $$SRC/include/de_network.h \
    $$SRC/include/de_platform.h \
    $$SRC/include/de_play.h \
    $$SRC/include/de_resource.h \
    $$SRC/include/de_render.h \
    $$SRC/include/de_system.h \
    $$SRC/include/de_ui.h \
    $$SRC/include/def_main.h \
    $$SRC/include/edit_map.h \
    $$SRC/include/face.h \
    $$SRC/include/game.h \
    $$SRC/include/hedge.h \
    $$SRC/include/library.h \
    $$SRC/include/m_decomp64.h \
    $$SRC/include/m_misc.h \
    $$SRC/include/m_nodepile.h \
    $$SRC/include/m_profiler.h \
    $$SRC/include/mesh.h \
    $$SRC/include/network/masterserver.h \
    $$SRC/include/network/monitor.h \
    $$SRC/include/network/net_buf.h \
    $$SRC/include/network/net_event.h \
    $$SRC/include/network/net_main.h \
    $$SRC/include/network/net_msg.h \
    $$SRC/include/partition.h \
    $$SRC/include/r_util.h \
    $$SRC/include/resource/animgroup.h \
    $$SRC/include/resource/colorpalette.h \
    $$SRC/include/resource/compositetexture.h \
    $$SRC/include/resource/image.h \
    $$SRC/include/resource/manifest.h \
    $$SRC/include/resource/mapdef.h \
    $$SRC/include/resource/material.h \
    $$SRC/include/resource/materialarchive.h \
    $$SRC/include/resource/materialmanifest.h \
    $$SRC/include/resource/materialscheme.h \
    $$SRC/include/resource/patch.h \
    $$SRC/include/resource/patchname.h \
    $$SRC/include/resource/rawtexture.h \
    $$SRC/include/resource/resourcesystem.h \
    $$SRC/include/resource/sprite.h \
    $$SRC/include/resource/texture.h \
    $$SRC/include/resource/texturemanifest.h \
    $$SRC/include/resource/texturescheme.h \
    $$SRC/include/sys_system.h \
    $$SRC/include/tab_anorms.h \
    $$SRC/include/ui/busyvisual.h \
    $$SRC/include/ui/infine/finale.h \
    $$SRC/include/ui/infine/finaleanimwidget.h \
    $$SRC/include/ui/infine/finaleinterpreter.h \
    $$SRC/include/ui/infine/finalepagewidget.h \
    $$SRC/include/ui/infine/finaletextwidget.h \
    $$SRC/include/ui/infine/finalewidget.h \
    $$SRC/include/ui/p_control.h \
    $$SRC/include/world/dmuargs.h \
    $$SRC/include/world/blockmap.h \
    $$SRC/include/world/bsp/bsptreenode.h \
    $$SRC/include/world/bsp/convexsubspace.h \
    $$SRC/include/world/bsp/edgetip.h \
    $$SRC/include/world/bsp/hplane.h \
    $$SRC/include/world/bsp/linesegment.h \
    $$SRC/include/world/bsp/partitioncost.h \
    $$SRC/include/world/bsp/partitioner.h \
    $$SRC/include/world/bsp/superblockmap.h \
    $$SRC/include/world/bspleaf.h \
    $$SRC/include/world/bspnode.h \
    $$SRC/include/world/entitydatabase.h \
    $$SRC/include/world/entitydef.h \
    $$SRC/include/world/interceptor.h \
    $$SRC/include/world/line.h \
    $$SRC/include/world/lineblockmap.h \
    $$SRC/include/world/lineowner.h \
    $$SRC/include/world/linesighttest.h \
    $$SRC/include/world/map.h \
    $$SRC/include/world/maputil.h \
    $$SRC/include/world/p_object.h \
    $$SRC/include/world/p_players.h \
    $$SRC/include/world/p_ticker.h \
    $$SRC/include/world/plane.h \
    $$SRC/include/world/polyobj.h \
    $$SRC/include/world/polyobjdata.h \
    $$SRC/include/world/propertyvalue.h \
    $$SRC/include/world/reject.h \
    $$SRC/include/world/sector.h \
    $$SRC/include/world/surface.h \
    $$SRC/include/world/thinkers.h \
    $$SRC/include/world/vertex.h \
    $$SRC/include/world/worldsystem.h \

INCLUDEPATH += \
    include \
    $$DENG_INCLUDE_DIR \
    $$DENG_API_DIR

HEADERS += \
    $$DENG_API_HEADERS \
    $$DENG_HEADERS

# Platform-specific sources.
win32 {
    # Windows.
    HEADERS += \
        $$DENG_WIN_INCLUDE_DIR/dd_winit.h \
        $$DENG_WIN_INCLUDE_DIR/resource.h

    INCLUDEPATH += $$DENG_WIN_INCLUDE_DIR

    SOURCES += \
        $$SRC/src/windows/dd_winit.cpp
}
else:unix {
    # Common Unix (including Mac OS X).
    HEADERS += \
        $$DENG_UNIX_INCLUDE_DIR/dd_uinit.h

    INCLUDEPATH += $$DENG_UNIX_INCLUDE_DIR

    SOURCES += \
        $$SRC/src/unix/dd_uinit.cpp
}

macx {
    # Mac OS X only.
    INCLUDEPATH += $$DENG_MAC_INCLUDE_DIR
}
else:unix {
}

# Platform-independent sources.
SOURCES += \
    src/main_server.cpp \
    src/remoteuser.cpp \
    src/server_dummies.cpp \
    src/shelluser.cpp \
    src/shellusers.cpp \
    src/serverapp.cpp \
    src/serversystem.cpp \
    src/server/sv_frame.cpp \
    src/server/sv_infine.cpp \
    src/server/sv_main.cpp \
    src/server/sv_missile.cpp \
    src/server/sv_pool.cpp \
    src/server/sv_sound.cpp \
    $$SRC/src/api_console.cpp \
    $$SRC/src/api_filesys.cpp \
    $$SRC/src/api_uri.cpp \
    $$SRC/src/audio/s_cache.cpp \
    $$SRC/src/audio/s_main.cpp \
    $$SRC/src/busymode.cpp \
    $$SRC/src/color.cpp \
    $$SRC/src/con_config.cpp \
    $$SRC/src/games.cpp \
    $$SRC/src/dd_loop.cpp \
    $$SRC/src/dd_main.cpp \
    $$SRC/src/dd_pinit.cpp \
    $$SRC/src/dd_plugin.cpp \
    $$SRC/src/def_main.cpp \
    $$SRC/src/face.cpp \
    $$SRC/src/game.cpp \
    $$SRC/src/hedge.cpp \
    $$SRC/src/library.cpp \
    $$SRC/src/m_decomp64.cpp \
    $$SRC/src/m_misc.cpp \
    $$SRC/src/m_nodepile.cpp \
    $$SRC/src/mesh.cpp \
    $$SRC/src/network/masterserver.cpp \
    $$SRC/src/network/monitor.cpp \
    $$SRC/src/network/net_buf.cpp \
    $$SRC/src/network/net_event.cpp \
    $$SRC/src/network/net_main.cpp \
    $$SRC/src/network/net_msg.cpp \
    $$SRC/src/network/net_ping.cpp \
    $$SRC/src/r_util.cpp \
    $$SRC/src/resource/animgroup.cpp \
    $$SRC/src/resource/api_material.cpp \
    $$SRC/src/resource/api_resource.cpp \
    $$SRC/src/resource/colorpalette.cpp \
    $$SRC/src/resource/compositetexture.cpp \
    $$SRC/src/resource/hq2x.cpp \
    $$SRC/src/resource/image.cpp \
    $$SRC/src/resource/manifest.cpp \
    $$SRC/src/resource/mapdef.cpp \
    $$SRC/src/resource/material.cpp \
    $$SRC/src/resource/materialarchive.cpp \
    $$SRC/src/resource/materialmanifest.cpp \
    $$SRC/src/resource/materialscheme.cpp \
    $$SRC/src/resource/patch.cpp \
    $$SRC/src/resource/patchname.cpp \
    $$SRC/src/resource/pcx.cpp \
    $$SRC/src/resource/resourcesystem.cpp \
    $$SRC/src/resource/sprite.cpp \
    $$SRC/src/resource/texture.cpp \
    $$SRC/src/resource/texturemanifest.cpp \
    $$SRC/src/resource/texturescheme.cpp \
    $$SRC/src/resource/tga.cpp \
    $$SRC/src/sys_system.cpp \
    $$SRC/src/tab_tables.c \
    $$SRC/src/ui/infine/finale.cpp \
    $$SRC/src/ui/infine/finaleanimwidget.cpp \
    $$SRC/src/ui/infine/finaleinterpreter.cpp \
    $$SRC/src/ui/infine/finalepagewidget.cpp \
    $$SRC/src/ui/infine/finaletextwidget.cpp \
    $$SRC/src/ui/infine/finalewidget.cpp \
    $$SRC/src/ui/infine/infinesystem.cpp \
    $$SRC/src/ui/p_control.cpp \
    $$SRC/src/world/api_map.cpp \
    $$SRC/src/world/api_mapedit.cpp \
    $$SRC/src/world/blockmap.cpp \
    $$SRC/src/world/bsp/convexsubspace.cpp \
    $$SRC/src/world/bsp/hplane.cpp \
    $$SRC/src/world/bsp/linesegment.cpp \
    $$SRC/src/world/bsp/partitioner.cpp \
    $$SRC/src/world/bsp/superblockmap.cpp \
    $$SRC/src/world/bspleaf.cpp \
    $$SRC/src/world/bspnode.cpp \
    $$SRC/src/world/dmuargs.cpp \
    $$SRC/src/world/entitydatabase.cpp \
    $$SRC/src/world/entitydef.cpp \
    $$SRC/src/world/interceptor.cpp \
    $$SRC/src/world/line.cpp \
    $$SRC/src/world/lineblockmap.cpp \
    $$SRC/src/world/linesighttest.cpp \
    $$SRC/src/world/map.cpp \
    $$SRC/src/world/mapelement.cpp \
    $$SRC/src/world/maputil.cpp \
    $$SRC/src/world/p_mobj.cpp \
    $$SRC/src/world/p_players.cpp \
    $$SRC/src/world/p_ticker.cpp \
    $$SRC/src/world/plane.cpp \
    $$SRC/src/world/polyobj.cpp \
    $$SRC/src/world/polyobjdata.cpp \
    $$SRC/src/world/propertyvalue.cpp \
    $$SRC/src/world/reject.cpp \
    $$SRC/src/world/sector.cpp \
    $$SRC/src/world/sectorcluster.cpp \
    $$SRC/src/world/surface.cpp \
    $$SRC/src/world/thinkers.cpp \
    $$SRC/src/world/vertex.cpp \
    $$SRC/src/world/worldsystem.cpp

OTHER_FILES += \
    include/template.h.template \
    src/template.c.template

# Resources ------------------------------------------------------------------

data.files = $$OUT_PWD/../doomsday.pk3

macx {
    res.path = Contents/Resources
    res.files = \
        $$SRC/res/macx/English.lproj \
        $$SRC/res/macx/deng.icns

    data.path = $${res.path}

    QMAKE_BUNDLE_DATA += res data

    QMAKE_INFO_PLIST = ../build/mac/Info.plist

    linkBinaryToBundledLibcore($$TARGET)
    linkBinaryToBundledLibshell($$TARGET)
}

# Installation ---------------------------------------------------------------

deployTarget()

!macx {
    # Common (non-Mac) parts of the installation.
    INSTALLS += data
    data.path = $$DENG_DATA_DIR

    win32 {
        # Windows-specific installation.
        INSTALLS += license icon

        license.files = $$SRC/doc/LICENSE
        license.path = $$DENG_DOCS_DIR

        icon.files = $$SRC/res/windows/doomsday.ico
        icon.path = $$DENG_DATA_DIR/graphics
    }
    else {
        # Generic Unix installation.
        INSTALLS += readme
        readme.files = ../doc/output/doomsday-server.6
        readme.path = $$PREFIX/share/man/man6
    }
}
