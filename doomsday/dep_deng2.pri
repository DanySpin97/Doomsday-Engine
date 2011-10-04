# Build configuration for using libdeng2.
INCLUDEPATH += $$PWD/libdeng2/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../libdeng2) {
    useLibDir($$OUT_PWD/../../libdeng2)
}

LIBS += -ldeng2

# libdeng2 requires the following Qt modules.
QT += core network

win32 {
    # Install the required Qt DLLs into the products dir.
    INSTALLS += qtlibs
    deng_debug: qtver = "d4"
    else:       qtver = "4"
    qtlibs.files += \
        $$[QT_INSTALL_LIBS]/QtCore$${qtver}.dll \
        $$[QT_INSTALL_LIBS]/QtNetwork$${qtver}.dll
    qtlibs.path = $$DENG_LIB_DIR
}
