# Open Asset Import Library
aiOpts   = ""
aiIncDir = ""
aiLibs   = ""

deng_extassimp {
    ASSIMP_DIR = $$PWD/external/assimp
}

!isEmpty(ASSIMP_DIR) {
    macx {
        aiIncDir = $$ASSIMP_DIR/include
        aiLibs   = -L$$ASSIMP_DIR/lib -lassimp

        deng_sdk {
            INSTALLS += assimplib
            assimplib.files = $$ASSIMP_DIR/lib/libassimp.3.dylib
            assimplib.path  = $$DENG_SDK_LIB_DIR
        }
    }
    else:unix {
        aiIncDir = $$ASSIMP_DIR/include
        aiLibs   = -L$$ASSIMP_DIR/lib -lassimp

        deng_extassimp {
            *-g*|*-clang* {
                # Inform the dynamic linker about a custom location.
                QMAKE_LFLAGS += -Wl,-rpath,$$DENG_PLUGIN_LIB_DIR
            }

            # Install the custom-build external Assimp.
            INSTALLS += assimplib
            assimplib.files = $$ASSIMP_DIR/lib/libassimp.so.3
            assimplib.path  = $$DENG_PLUGIN_LIB_DIR
        }
        else {
            *-g*|*-clang* {
                # Inform the dynamic linker about a custom location.
                QMAKE_LFLAGS += -Wl,-rpath,$$ASSIMP_DIR/lib
            }
        }
    }
    else {
        # On Windows we assume that cmake has been run in the root of
        # the assimp source tree.
        aiIncDir = $$ASSIMP_DIR/include
        deng_msvc {
            deng_debug: aiLibs = -L$$ASSIMP_DIR/lib/debug -lassimpd
                  else: aiLibs = -L$$ASSIMP_DIR/lib/release -lassimp
        }
        else {
            aiLibs = -L$$ASSIMP_DIR/lib -lassimp
        }

        INSTALLS += assimplib
        assimplib.path = $$DENG_BIN_DIR
        deng_sdk: assimplib.path = $$DENG_SDK_LIB_DIR
        deng_debug: assimplib.files = $$ASSIMP_DIR/bin/debug/assimpd.dll
              else: assimplib.files = $$ASSIMP_DIR/bin/release/assimp.dll
    }
}
else:!macx:!win32 {
    # Are the development files installed?
    !system($$PKG_CONFIG --exists assimp) {
        error(Missing dependency: Open Asset Import Library)
    }

    aiOpts = $$system($$PKG_CONFIG --cflags assimp)
    aiLibs = $$system($$PKG_CONFIG --libs assimp)
}
else {
    error(Open Asset Import Library location not defined (ASSIMP_DIR))
}

INCLUDEPATH    += $$aiIncDir
QMAKE_CFLAGS   += $$aiOpts
QMAKE_CXXFLAGS += $$aiOpts

!libgui_headers_only {
    LIBS       += $$aiLibs
}

macx {
    defineTest(linkBinaryToBundledAssimp) {
        # 1: binary
        # 2: relative path to Frameworks
        doPostLink(install_name_tool -change "$$ASSIMP_DIR/lib/libassimp.3.dylib" \
                   "@rpath/libassimp.3.dylib" "$$1")
    }
}
