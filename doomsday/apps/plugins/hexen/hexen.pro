# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# Copyright (c) 2011-2013 Daniel Swanson <danij@dengine.net>

include(../config_plugin.pri)
include(../common/common.pri)
include(../../dep_lzss.pri)
include(../../dep_gui.pri)

TEMPLATE = lib
TARGET   = hexen
VERSION  = $$JHEXEN_VERSION

DEFINES += __JHEXEN__

gamedata.files = $$OUT_PWD/../../libhexen.pk3

macx {
    QMAKE_BUNDLE_DATA += gamedata
}
else {
    INSTALLS += gamedata
    gamedata.path = $$DENG_DATA_DIR/jhexen
}

INCLUDEPATH += include

HEADERS += \
    include/a_action.h \
    include/acfnlink.h \
    include/dstrings.h \
    include/g_game.h \
    include/h2def.h \
    include/hud/widgets/armoriconswidget.h \
    include/hud/widgets/bluemanaiconwidget.h \
    include/hud/widgets/bluemanavialwidget.h \
    include/hud/widgets/bluemanawidget.h \
    include/hud/widgets/bootswidget.h \
    include/hud/widgets/defensewidget.h \
    include/hud/widgets/greenmanaiconwidget.h \
    include/hud/widgets/greenmanavialwidget.h \
    include/hud/widgets/greenmanawidget.h \
    include/hud/widgets/servantwidget.h \
    include/hud/widgets/weaponpieceswidget.h \
    include/hud/widgets/worldtimewidget.h \
    include/intermission.h \
    include/info.h \
    include/jhexen.h \
    include/lightninganimator.h \
    include/m_cheat.h \
    include/m_random.h \
    include/p_enemy.h \
    include/p_inter.h \
    include/p_lights.h \
    include/p_local.h \
    include/p_maputl.h \
    include/p_mobj.h \
    include/p_pillar.h \
    include/p_pspr.h \
    include/p_setup.h \
    include/p_spec.h \
    include/p_telept.h \
    include/p_things.h \
    include/p_waggle.h \
    include/r_defs.h \
    include/r_local.h \
    include/s_sequence.h \
    include/st_stuff.h \
    include/textdefs.h \
    include/version.h \
    include/x_api.h \
    include/x_config.h \
    include/x_console.h \
    include/x_event.h \
    include/x_items.h \
    include/x_main.h \
    include/x_player.h \
    include/x_refresh.h \
    include/x_state.h \
    include/x_think.h \
    include/xddefs.h

SOURCES += \
    src/a_action.c \
    src/acfnlink.c \
    src/h2_main.cpp \
    src/hconsole.cpp \
    src/hrefresh.cpp \
    src/hud/widgets/armoriconswidget.cpp \
    src/hud/widgets/bluemanaiconwidget.cpp \
    src/hud/widgets/bluemanavialwidget.cpp \
    src/hud/widgets/bluemanawidget.cpp \
    src/hud/widgets/bootswidget.cpp \
    src/hud/widgets/defensewidget.cpp \
    src/hud/widgets/greenmanaiconwidget.cpp \
    src/hud/widgets/greenmanavialwidget.cpp \
    src/hud/widgets/greenmanawidget.cpp \
    src/hud/widgets/servantwidget.cpp \
    src/hud/widgets/weaponpieceswidget.cpp \
    src/hud/widgets/worldtimewidget.cpp \
    src/intermission.cpp \
    src/lightninganimator.cpp \
    src/m_cheat.cpp \
    src/m_random.c \
    src/p_enemy.c \
    src/p_inter.c \
    src/p_lights.cpp \
    src/p_maputl.c \
    src/p_mobj.c \
    src/p_pillar.cpp \
    src/p_pspr.c \
    src/p_setup.c \
    src/p_spec.cpp \
    src/p_telept.c \
    src/p_things.c \
    src/p_waggle.cpp \
    src/sn_sonix.cpp \
    src/st_stuff.cpp \
    src/tables.c \
    src/x_api.c

win32 {
    deng_msvc:  QMAKE_LFLAGS += /DEF:\"$$PWD/api/hexen.def\"
    deng_mingw: QMAKE_LFLAGS += --def \"$$PWD/api/hexen.def\"

    OTHER_FILES += api/hexen.def

    RC_FILE = res/hexen.rc
}

macx {
    fixPluginInstallId($$TARGET, 1)
    linkToBundledLibcore($$TARGET)
    linkToBundledLiblegacy($$TARGET)
}
