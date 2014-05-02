# Build configuration for using the libdeng_shell library.
shellDir = libshell
INCLUDEPATH += $$PWD/$$shellDir/include

# Use the appropriate library path.
!useLibDir($$OUT_PWD/../$$shellDir) {
    !useLibDir($$OUT_PWD/../../$$shellDir) {
        !useLibDir($$OUT_PWD/../../../$$shellDir) {
            useLibDir($$OUT_PWD/../../builddir/$$shellDir)
        }
    }
}

LIBS += -ldeng_shell

macx {
    defineTest(linkBinaryToBundledLibdengShell) {
        fixInstallName($${1}, libdeng_shell.1.dylib, ..)
    }
    defineTest(linkToBundledLibdengShell) {
        linkBinaryToBundledLibdengShell($${1}.bundle/$$1)
    }
}
