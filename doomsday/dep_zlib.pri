# Build configuration for zlib.
win32 {
    INCLUDEPATH += $$PWD/external/zlib/portable/include
    LIBS += -L$$PWD/external/zlib/win32 -lzdll

    # Installed shared libraries.
    INSTALLS += zlibs
    zlibs.files = $$PWD/external/zlib/win32/zlib1.dll
    zlibs.path = $$DENG_LIB_DIR
}
else:macx {
    LIBS += -lz
}
