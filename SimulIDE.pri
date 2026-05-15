
VERSION = "2.0.0"
RELEASE = "i1"

TEMPLATE = app
TARGET = simulide

QT += svg
QT += xml
QT += widgets
QT += network
QT += concurrent
!wasm: QT += serialport

# Compile-time toggle: hide non-essential menu/toolbar actions and the
# Libraries / Files sidebar tabs. Used by the WASM-targeted distribution
# where local file browsing and component-library management aren't
# applicable. To restore the full UI, comment this out (or wrap it in a
# `wasm { ... }` block to make it WASM-only).
DEFINES += HIDE_SOME_ACTIONS
QT += multimedia widgets

SOURCES      = $$files( $$PWD/src/*.cpp, true )
HEADERS      = $$files( $$PWD/src/*.h, true )

wasm {
    SOURCES -= $$files( $$PWD/src/angel/JIT/*.cpp )

    # ASYNCIFY: required for QEventLoop::exec() / QDrag::exec() in WASM.
    # Use two-token "-s ASYNCIFY" — emscripten 1.39.x does not support "-sASYNCIFY=1".
    QMAKE_LFLAGS       += -s ASYNCIFY
    QMAKE_LFLAGS_DEBUG += -Os   # ASYNCIFY emits broken WASM without optimizations.

    # Default ASYNCIFY stack (4096 bytes) overflows on Qt's deep call chains
    # (context menu -> QFileDialog -> QDialog::exec -> processEvents -> emscripten_sleep).
    QMAKE_LFLAGS += -s ASYNCIFY_STACK_SIZE=1048576

    # Embed component data in the WASM package; applicationDirPath() returns "/" on WASM,
    # so SimulIDE looks for "/data".
    QMAKE_LFLAGS += --preload-file $$PWD/resources/data@/data

    # Enable emscripten_fetch (used by Compiler::remoteCompile to bypass
    # QNetworkAccessManager — under Qt 5.15 + ASYNCIFY, QNAM holds the
    # WASM stack suspended and the finished signal never fires even when
    # the HTTP response (200 OK) is already in hand. emscripten_fetch is
    # truly callback-driven and avoids the trap.
    QMAKE_LFLAGS += -s FETCH=1

    # Same-origin JavaScript control bridge (src/wasm/jsbridge.{h,cpp}).
    # The bridge is exposed to JS via embind so a parent web page can call
    # iframe.contentWindow.Module.SimulIDEBridge.instance().<method>().
    INCLUDEPATH  += $$PWD/src/wasm
    QMAKE_LFLAGS += --bind
}
TRANSLATIONS = $$files( $$PWD/resources/translations/*.ts )
FORMS       += $$files( $$PWD/src/*.ui, true )
RESOURCES    = $$PWD/src/application.qrc

INCLUDEPATH += $$PWD/src \
    $$PWD/src/components \
    $$PWD/src/components/active \
    $$PWD/src/components/connectors \
    $$PWD/src/components/graphical \
    $$PWD/src/components/logic \
    $$PWD/src/components/meters \
    $$PWD/src/components/micro \
    $$PWD/src/components/other \
    $$PWD/src/components/other/truthtable \
    $$PWD/src/components/outputs \
    $$PWD/src/components/outputs/displays \
    $$PWD/src/components/outputs/leds \
    $$PWD/src/components/outputs/motors \
    $$PWD/src/components/passive \
    $$PWD/src/components/passive/reactive \
    $$PWD/src/components/passive/resistors \
    $$PWD/src/components/passive/resist_sensors \
    $$PWD/src/components/sources \
    $$PWD/src/components/subcircuits \
    $$PWD/src/components/switches \
    $$PWD/src/gui \
    $$PWD/src/gui/appdialogs \
    $$PWD/src/gui/circuitwidget \
    $$PWD/src/gui/componentlist \
    $$PWD/src/gui/dataplotwidget \
    $$PWD/src/gui/editorwidget \
    $$PWD/src/gui/editorwidget/debuggers \
    $$PWD/src/gui/editorwidget/dialogs \
    $$PWD/src/gui/filebrowser \
    $$PWD/src/gui/memory \
    $$PWD/src/gui/properties \
    $$PWD/src/gui/serial \
    $$PWD/src/gui/testing \
    $$PWD/src/simulator \
    $$PWD/src/simulator/elements \
    $$PWD/src/simulator/elements/active \
    $$PWD/src/simulator/elements/outputs \
    $$PWD/src/simulator/elements/passive \
    $$PWD/src/microsim \
    $$PWD/src/microsim/cores \
    $$PWD/src/microsim/cores/avr \
    $$PWD/src/microsim/cores/i51 \
    $$PWD/src/microsim/cores/pic \
    $$PWD/src/microsim/cores/mcs65 \
    $$PWD/src/microsim/cores/z80 \
    $$PWD/src/microsim/cores/scripted \
    $$PWD/src/microsim/cores/qemu \
    $$PWD/src/microsim/cores/qemu/esp32 \
    $$PWD/src/microsim/cores/qemu/stm32 \
    $$PWD/src/microsim/modules \
    $$PWD/src/microsim/modules/memory \
    $$PWD/src/microsim/modules/usart \
    $$PWD/src/microsim/modules/onewire\
    $$PWD/src/microsim/modules/twi \
    $$PWD/src/microsim/modules/tcp\
    $$PWD/src/microsim/modules/spi\
    $$PWD/src/microsim/modules/script\
    $$PWD/src/angel/include \
    $$PWD/src/angel/JIT \
    $$PWD/src/angel/src

QMAKE_CXXFLAGS += -Wno-unused-parameter
#QMAKE_CXXFLAGS += -Wno-deprecated-declarations
QMAKE_CXXFLAGS += -Wno-implicit-fallthrough
QMAKE_CXXFLAGS += -fno-strict-aliasing      #AngelScript
QMAKE_CXXFLAGS += -Wno-cast-function-type   #AngelScript
QMAKE_CXXFLAGS += -Wno-deprecated-copy      #AngelScript
QMAKE_CXXFLAGS += -Wno-invalid-offsetof     #AngelScript
!wasm: QMAKE_CXXFLAGS += -Ofast
QMAKE_CXXFLAGS_DEBUG += -D_GLIBCXX_ASSERTIONS
QMAKE_CXXFLAGS_DEBUG -= -O
QMAKE_CXXFLAGS_DEBUG -= -O1
QMAKE_CXXFLAGS_DEBUG -= -O2
QMAKE_CXXFLAGS_DEBUG -= -O3
QMAKE_CXXFLAGS_DEBUG += -O0

!wasm: LIBS += -lz
wasm {
    # emscripten's zlib port: provides headers at compile time and links zlib.
    QMAKE_CFLAGS    += -s USE_ZLIB=1
    QMAKE_CXXFLAGS  += -s USE_ZLIB=1
    QMAKE_LFLAGS    += -s USE_ZLIB=1
}

wasm {
    BINUTILS_BUILD = $$PWD/../../dependencies/binutils-avr/binutils-avr-wasm-build
    BINUTILS_SRC   = $$PWD/../../dependencies/binutils-avr/binutils-2.46.0

    INCLUDEPATH += $$BINUTILS_BUILD/bfd \
                   $$BINUTILS_SRC/include

    LIBS += -L$$BINUTILS_BUILD/opcodes/.libs   -lopcodes \
            -L$$BINUTILS_BUILD/bfd/.libs       -lbfd     \
            -L$$BINUTILS_BUILD/libiberty       -liberty  \
            -lz

    DEFINES += SIM_HAS_LIBBFD
}

win32 {
    OS = Windows
    QMAKE_LIBS += -lwsock32
    RC_ICONS += $$PWD/resources/icons/simulide.ico
}
linux {
    OS = Linux
}
macx {
    OS = MacOs
    ICON = $$PWD/resources/icons/simulide.icns

    QMAKE_CXXFLAGS -= -stdlib=libc++
    QMAKE_LFLAGS   -= -stdlib=libc++

# To use gcc in MacOs you must force it.
# Edit to match your system:
    QMAKE_CC   = /usr/local/Cellar/gcc@7/7.5.0_4/bin/gcc-7
    QMAKE_CXX  = /usr/local/Cellar/gcc@7/7.5.0_4/bin/g++-7
    QMAKE_LINK = /usr/local/Cellar/gcc@7/7.5.0_4/bin/g++-7
}

contains( QMAKE_HOST.arch, arm64|aarch64 ) | contains( QMAKE_CC, .*aarch64.* ){
    macx {
        SOURCES += $$PWD/src/angel/src/as_callfunc_arm64_xcode.S
    } else {
        SOURCES += $$PWD/src/angel/src/as_callfunc_arm64_gcc.S
    }
}

contains( QMAKE_HOST.os, Windows ) {
    REV_NO = $$system("powershell -Command get-date -format yy-MM-dd")     # year-month-day
    BUILD_DATE = $$system("powershell -Command get-date -format dd-MM-yy") # day-month-year
}
else {
    REV_NO = $$system($(which date) +\"\\\"%y%m%d\\\"\")
    BUILD_DATE = $$system($(which date) +\"\\\"%d-%m-%y\\\"\")
}

CONFIG += qt 
CONFIG += warn_on
CONFIG += no_qml_debug
CONFIG *= c++11

DEFINES += REVNO=\\\"$$REV_NO\\\"
DEFINES += APP_VERSION=\\\"$$VERSION-$$RELEASE\\\"
DEFINES += BUILDDATE=\\\"$$BUILD_DATE\\\"

TARGET_NAME   = SimulIDE_$$VERSION-$$RELEASE
TARGET_PREFIX = $$BUILD_DIR/executables/$$TARGET_NAME

OBJECTS_DIR *= $$OUT_PWD/build/objects
MOC_DIR     *= $$OUT_PWD/build/moc
INCLUDEPATH += $$MOC_DIR

DESTDIR = $$TARGET_PREFIX

runLrelease.commands = \
    lrelease $$PWD/resources/translations/*.ts; \
    lrelease $$PWD/resources/translations/qt/*.ts; \
    $(MOVE) $$PWD/resources/translations/*.qm $$PWD/resources/qm; \
    $(MOVE) $$PWD/resources/translations/qt/*.qm $$PWD/resources/qm;

QMAKE_EXTRA_TARGETS += runLrelease
PRE_TARGETDEPS      += runLrelease

# Deploy custom WASM files (for WASM builds only)
# This ensures our custom wasm_shell.html and qtloader.js are used instead of Qt's defaults
# ALL generated files are versioned with build date/revision to enable cache-busting on deployment
wasm {
    # Create versioned filenames for cache-busting: TARGET_v2.0.0-i1_12-05-26
    WASM_VERSION_SUFFIX = _v$$VERSION-$$RELEASE
    WASM_BUILD_SUFFIX = $$system($(which date) +%y%m%d)
    WASM_FILE_SUFFIX = $$WASM_VERSION_SUFFIX$$WASM_BUILD_SUFFIX
    WASM_MODULE_NAME = $$TARGET$$WASM_FILE_SUFFIX
    QTLOADER_VERSIONED = qtloader$${WASM_FILE_SUFFIX}.js

    # Pick template based on Qt major version:
    #  - Qt5: wasm_shell.html        (uses old QtLoader API, canvas element)
    #  - Qt6: wasm_shell_qt6.html    (uses new qtLoad API, div container)
    lessThan(QT_MAJOR_VERSION, 6) {
        WASM_SHELL_TEMPLATE   = $$PWD/wasm_shell.html
        WASM_LOADER_TEMPLATE  = $$PWD/qtloader.js
    } else {
        WASM_SHELL_TEMPLATE   = $$PWD/wasm_shell_qt6.html
        WASM_LOADER_TEMPLATE  = $$PWD/qtloader_qt6.js
    }

    # emcc EXPORT_NAME used in the link step (matches -s EXPORT_NAME=...). The
    # Qt6 template references this as window.<APPEXPORTNAME> to start the wasm
    # module. Qt5 template ignores it — harmless if substituted there too.
    WASM_EXPORT_NAME = $${TARGET}_entry

    # Post-link command to deploy and version all WASM files:
    # 0. Clean any previously-deployed versioned artifacts so stale builds don't
    #    accumulate in the deploy dir. Globs are intentionally unquoted so the
    #    shell expands them; rm -f swallows the case where nothing matches.
    # 1. Substitute placeholders in the html template (writes versioned .html):
    #      @APPNAME@        -> versioned module name (e.g., simulide_v2.0.0-i1_140526)
    #      @APPEXPORTNAME@  -> emcc EXPORT_NAME (Qt6 only; ignored by Qt5 template)
    #      @QTLOADER@       -> versioned qtloader filename
    #      @PRELOAD@        -> empty (Qt6 only; ignored by Qt5 template)
    # 2. Rewrite .data and .wasm filename references inside the .js (saved as versioned .js)
    # 3. Rename .wasm and .data files to versioned names
    # 4. Copy the chosen qtloader template to the versioned name in target dir
    # 5. Remove the original (un-versioned) emcc-produced .js and Qt-installed qtloader.js
    QMAKE_POST_LINK = rm -f $$TARGET_PREFIX/$${TARGET}_v*.js \
                            $$TARGET_PREFIX/$${TARGET}_v*.wasm \
                            $$TARGET_PREFIX/$${TARGET}_v*.data \
                            $$TARGET_PREFIX/qtloader_v*.js ; \
                      sed -e s/@APPNAME@/$$WASM_MODULE_NAME/g \
                          -e s/@APPEXPORTNAME@/$$WASM_EXPORT_NAME/g \
                          -e s/@QTLOADER@/$$QTLOADER_VERSIONED/g \
                          -e s/@PRELOAD@//g \
                          $$shell_quote($$WASM_SHELL_TEMPLATE) > $$shell_quote($$TARGET_PREFIX/$${TARGET}.html) ; \
                      sed -e s/$${TARGET}\.data/$${WASM_MODULE_NAME}\.data/g \
                          -e s/$${TARGET}\.wasm/$${WASM_MODULE_NAME}\.wasm/g \
                          $$shell_quote($$TARGET_PREFIX/$${TARGET}.js) > $$shell_quote($$TARGET_PREFIX/$${WASM_MODULE_NAME}.js) ; \
                      $(MOVE) $$shell_quote($$TARGET_PREFIX/$${TARGET}.wasm) $$shell_quote($$TARGET_PREFIX/$${WASM_MODULE_NAME}.wasm) ; \
                      $(MOVE) $$shell_quote($$TARGET_PREFIX/$${TARGET}.data) $$shell_quote($$TARGET_PREFIX/$${WASM_MODULE_NAME}.data) ; \
                      $(COPY_FILE) $$shell_quote($$WASM_LOADER_TEMPLATE) $$shell_quote($$TARGET_PREFIX/$$QTLOADER_VERSIONED) ; \
                      $(DEL_FILE) $$shell_quote($$TARGET_PREFIX/$${TARGET}.js) ; \
                      $(DEL_FILE) $$shell_quote($$TARGET_PREFIX/qtloader.js) ;
}

message( "-----------------------------------")
message( "    "                               )
message( "    "$$TARGET_NAME for $$OS         )
message( "    "                               )
message( "    Host:      "$$QMAKE_HOST.os     )
message( "    Date:      "$$BUILD_DATE        )
message( "    Qt version: "$$QT_VERSION       )
message( "    "                               )
message( "    Destination Folder:"            )
message( $$TARGET_PREFIX                      )
message( "-----------------------------------")
