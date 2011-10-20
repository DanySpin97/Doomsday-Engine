include(../config.pri)

macx {
    CONFIG += lib_bundle
    QMAKE_BUNDLE_EXTENSION = .bundle
}
win32 {
    # Keep the version number out of the file name.
    TARGET_EXT = .dll

    # Link against the engine's export library.
    LIBS += -l$$OUT_PWD/../../engine/doomsday
}

INCLUDEPATH += $$DENG_API_DIR
