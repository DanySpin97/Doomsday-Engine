# The Doomsday Engine Project
# Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
# License: GPLv2 + exception to link against FMOD Ex

# This plugin uses the full libdeng2 C++ API.
CONFIG += dengplugin_libdeng2_full

include(../config_plugin.pri)
include(../../dep_fmod.pri)

TEMPLATE = lib
TARGET   = audio_fmod
VERSION  = $$FMOD_VERSION

deng_debug: DEFINES += DENG_DSFMOD_DEBUG

INCLUDEPATH += include

HEADERS += \
    include/version.h \
    include/driver_fmod.h \
    include/fmod_cd.h \
    include/fmod_music.h \
    include/fmod_sfx.h \
    include/fmod_util.h

SOURCES += \
    src/driver_fmod.cpp \
    src/fmod_cd.cpp \
    src/fmod_music.cpp \
    src/fmod_sfx.cpp \
    src/fmod_util.cpp

OTHER_FILES += doc/LICENSE

win32 {
    RC_FILE = res/fmod.rc

    QMAKE_LFLAGS += /DEF:\"$$PWD/api/dsfmod.def\"
    OTHER_FILES += api/dsfmod.def

    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}
else:macx {
    fixPluginInstallId($$TARGET, 1)
    doPostLink("install_name_tool -change ./libfmodex.dylib @executable_path/../Frameworks/libfmodex.dylib audio_fmod.bundle/audio_fmod")
}
else {
    INSTALLS += target
    target.path = $$DENG_PLUGIN_LIB_DIR
}

macx {
    linkToBundledLibdeng2(audio_fmod)
    linkToBundledLibdeng1(audio_fmod)
}
