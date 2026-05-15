# SimulIDE 

Electronic Circuit Simulator

**SimulIDE is a simple real time electronic circuit simulator**, intended for hobbyist or students to learn and experiment with analog and digital electronic circuits and microcontrollers.
It supports PIC, AVR, Arduino and other MCUs and MPUs.

**Simplicity, speed and ease of use** are the key features of this simulator.
You can create, simulate and interact with your circuits within minutes, just drag components from the list, drop into the circuit, connect them and push the “power button” to see how it works.

Simulation speed is one of the most relevant characteristics of this simulator.
It has been deeply optimized to achieve excellent speeds and low cpu usage.

SimulIDE also features a code Editor and Debugger for Arduino, GcBasic, PIC asm, AVR asm and others. It is possible to write, compile and do basic debugging with breakpoints, watch registers and global variables.


## Building SimulIDE:

Build dependencies:

 - Qt5 dev packages
 - Qt5Core
 - Qt5Gui
 - Qt5Xml
 - Qt5Widgets
 - Qt5Concurrent
 - Qt5svg dev
 - Qt5 Multimedia dev
 - Qt5 Serialport dev
 - Qt5 qmake

 
Once installed go to build_XX folder, then:

```
$ qmake
$ make
```

In folder build_XX/executables/SimulIDE_x.x.x you will find executable and all files needed to run SimulIDE.



## Running SimulIDE:

Run time dependencies:

 - Qt5Core
 - Qt5Gui
 - Qt5Xml
 - Qt5svg
 - Qt5Widgets
 - Qt5Concurrent
 - Qt5 Multimedia
 - Qt5 Multimedia Plugins
 - Qt5 Serialport


No need for installation, place SimulIDE folder wherever you want and run the executable.


## Building SimulIDE for WebAssembly (Qt 5.15.2 + emscripten)

SimulIDE can be built as a WebAssembly application that runs in the browser.
This requires a custom-built Qt 5.15.2 toolchain configured for the
`wasm-emscripten` platform with ASYNCIFY enabled. The patch
`qt5.15.2-wasm-asyncify.patch` (in the project root) applies the Qt source
fixes required for the WASM build to run reliably (nested event loops,
runtime exit, async IndexedDB callbacks).

### Prerequisites

- Python 3 (for the dev server)
- OS-specific Qt 5 build dependencies — install per your platform
  (Windows / Linux / macOS) following:
  https://wiki.qt.io/Building_Qt_5_from_Git

### Install emsdk (1.39.8 for Qt 5.15.2)

Qt 5.15.2 targets emscripten 1.39.8. Install and activate it via the
official emsdk:

```
# Install 1.39.8 emsdk for qt5.15.2
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

./emsdk install 1.39.8
# to install latest version: ./emsdk install latest

./emsdk activate 1.39.8
# to activate latest version: ./emsdk activate latest
```

The `emsdk` directory you cloned here is what `<path-to-emsdk>` refers
to in the *Set up the build environment* section
(`source <path-to-emsdk>/emsdk_env.sh`).

### Get the Qt 5.15.2 source

```
git clone git://code.qt.io/qt/qt5.git
cd qt5
git checkout v5.15.2
./init-repository
```

### Apply the WASM/ASYNCIFY patch

The patch lives in the SimulIDE project root and targets `qtbase/`:

```
cd qtbase
git apply <path-to-simulide>/qt5.15.2-wasm-asyncify.patch
# or, outside git:
patch -p1 < <path-to-simulide>/qt5.15.2-wasm-asyncify.patch
cd ..
```

The patch covers six files under `qtbase/` (qmake.conf, two compile fixes,
the event-loop / dispatcher / settings WASM fixes). See the header inside
the patch file for the full description.

### Set up the build environment

Before configuring Qt, load the emsdk environment and export the
toolchain variables expected by `configure`:

```
# Load emsdk environment variables
source <path-to-emsdk>/emsdk_env.sh
export CXX=em++
export CC=emcc
export EM_CONFIG=$EMSDK/.emscripten
export EM_CACHE=$EMSDK/upstream/emscripten/cache
export PKG_CONFIG_PATH=$EMSDK/upstream/emscripten/system/lib/pkgconfig
export CXXFLAGS="--sysroot=$EMSDK/upstream/emscripten/cache/sysroot -O2 -std=c++17 -pthread"
export CFLAGS="--sysroot=$EMSDK/upstream/emscripten/cache/sysroot -O2 -pthread"
export NODE_JS=$EMSDK_NODE
export LLVM_ROOT=$EMSDK/upstream/emscripten
export BINARYEN_ROOT=$EMSDK/upstream
export EMCC_CFLAGS="-s USE_PTHREADS=1 -s WASM_MEM_MAX=512MB -s ALLOW_MEMORY_GROWTH=1"
export EMCC_CXXFLAGS="-s USE_PTHREADS=1 -s WASM_MEM_MAX=512MB -s ALLOW_MEMORY_GROWTH=1"
```

These need to be active in any shell that runs `configure`, `gmake`, or
the WASM-targeting `qmake` later.

### Configure and build Qt for WASM

From the top-level `qt5/` directory:

```
./configure -xplatform wasm-emscripten \
            -device-option EMSCRIPTEN_ASYNCIFY=1 \
            -nomake tests -nomake examples \
            -opensource -confirm-license \
            -prefix $HOME/qtinstall
gmake -j$(nproc)
gmake install
```

After this, the toolchain is installed at `$HOME/qtinstall` and
`$HOME/qtinstall/bin/qmake` is the WASM-targeting qmake.

### Fetch component sets (WASM only)

The dev tree no longer ships MCU / component data inline; the running
desktop app downloads it on demand from
`https://simulide.com/p/direct_downloads/components/`. A WASM build
can't make that fetch reliably from the browser (CORS / origin
restrictions), so the catalog and per-set zip archives must be
**bundled into the WASM data preload** at build time.

A helper script does this for you. Run it from the project root **before
building**:

```
# Download zips only — user clicks "Install" once per set in-app to expand them.
./scripts/fetch-components.sh

# Or, recommended for WASM: download AND extract each set in place,
# so every component is available immediately on first page load
# (no in-app Install step, no repeat after browser refresh).
./scripts/fetch-components.sh --extract
```

`fetch-components.sh` writes into `resources/data/components/`, which is
preloaded into the WASM virtual filesystem at `/data/components/`. The
runtime `Installer` is forced offline on WASM and reads exclusively
from that bundled directory; with `--extract`, it auto-detects the
expanded folders and treats each set as already installed.

Re-run the script whenever the upstream catalog changes (e.g. a new MCU
set is added) to keep the bundle in sync, then rebuild SimulIDE.

### Build SimulIDE for WASM

With the same environment from the *Set up the build environment*
section still active, put the installed Qt's `bin/` directory on
`PATH` so the WASM-targeting `qmake` is picked up without a full path:

Resetup environment from *Set up the build environment* step in case of new session

```
# Export path
export PATH=$HOME/qtinstall/bin:$PATH
```

```
# warm cache once
rm -rf "$EMSDK/upstream/emscripten/cache"
../scripts/warm-emcc-cache.sh
```

(Adjust the path if you used a different `-prefix` when configuring Qt.)

Then go into the build directory and run qmake + gmake:

```
cd <path-to-simulide>/build_XX
gmake clean
qmake -spec wasm-emscripten CONFIG+="release qtquickcompiler" SimulIDE_Build.pro
gmake -j$(nproc)
```

This produces the WASM artifacts in `build_XX/executables/simulIDE_XXX/`:

- `simulide.html`
- `simulide.js`
- `simulide.wasm`
- `simulide.data` (the bundled file system)
- `qtloader.js`

For a debug build replace `release` with `debug`:

```
qmake -spec wasm-emscripten CONFIG+="debug qtquickcompiler" SimulIDE_Build.pro
```

### Run in a browser

WASM cannot be loaded via `file://` — it must be served over HTTP. The
project ships a small Python server (`localwasmserve.py`) configured with the
correct MIME types and CORS headers. copy that script from scripts root folder 
to `build_XX/executables/simulIDE_XXX/` and from that run :

```
python3 localwasmserve.py
```

Then open the URL printed by the server (typically
`http://localhost:8000/simulide.html`) in a recent browser
(Chromium / Firefox).

### Rebuilding only SimulIDE

After the first successful build you only need to repeat the
*Build SimulIDE for WASM* section to pick up SimulIDE source changes.
You only need to rebuild Qt itself if you change the patch or update
the Qt source tree.

### Cleaning the Qt source tree before a fresh rebuild

If you want to rebuild Qt from a clean slate (for example, after pulling
new changes or re-applying the patch), run from the `qt5/` root:

```
# Deep clean: wipe all build artifacts in every submodule and the top tree
git submodule foreach --recursive "git clean -dfx" && git clean -dfx
```

Then re-apply the patch (*Apply the WASM/ASYNCIFY patch* section) and
continue from *Set up the build environment*.

## Building SimulIDE for WebAssembly (Qt 6.11.0 + emscripten)

SimulIDE can be built as a WebAssembly application that runs in the browser.
This requires a custom-built Qt 6.11.0 toolchain configured for the
`wasm-emscripten` platform with ASYNCIFY enabled.

### Prerequisites

- Python 3 (for the dev server)
- OS-specific Qt 6 build dependencies (e.g. Qt6 prebuilt host installed binaries. use Qt online installer to install on host) — install per your platform
  (Windows / Linux / macOS) following:
  https://wiki.qt.io/Building_Qt_5_from_Git

### Install emsdk (4.0.7 for Qt 6.11.0)

Qt 6.11.0 targets emscripten 4.0.7. Install and activate it via the
official emsdk:

```
# Install 4.0.7 emsdk for qt6.11.0
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

./emsdk install 4.0.7
# to install latest version: ./emsdk install latest

./emsdk activate 4.0.7
# to activate latest version: ./emsdk activate latest
```

The `emsdk` directory you cloned here is what `<path-to-emsdk>` refers
to in the *Set up the build environment* section
(`source <path-to-emsdk>/emsdk_env.sh`).

### Get the Qt 6.11.0 source

```
git clone git://code.qt.io/qt/qt5.git qt6
cd qt6
git checkout v6.11.0
./init-repository
```
### Set up the build environment

Before configuring Qt, load the emsdk environment and export the
toolchain variables expected by `configure`:

```
# Load emsdk environment variables
source <path-to-emsdk>/emsdk_env.sh
```

These need to be active in any shell that runs `configure`, `cmake`.

### Configure and build Qt for WASM

From the top-level `qt6/` directory:

```

./configure -xplatform wasm-emscripten -feature-thread \
            -qt-host-path /home/ewings/Qt/6.11.0/gcc_64 \
            -nomake tests -nomake examples \
            -opensource -confirm-license \
            -prefix $HOME/qt6install \
            -submodules qtbase,qtsvg,qtmultimedia \
            -- -DQT_QMAKE_DEVICE_OPTIONS=QT_EMSCRIPTEN_ASYNCIFY=1

cmake --build . --parallel 10
cmake --install .
```

After this, the toolchain is installed at `$HOME/qt6install` and
`$HOME/qt6install/bin/qt-cmake` is the WASM-targeting qmake.

### Cleaning the Qt source tree before a fresh rebuild

If you want to rebuild Qt from a clean slate (for example, after pulling
new changes or re-applying the patch), run from the `qt6/` root:

```
# Deep clean: wipe all build artifacts in every submodule and the top tree
git submodule foreach --recursive "git clean -dfx" && git clean -dfx
```

Now follow same step for building from *Build SimulIDE for WASM* section.
just make sure you will use qt6install path instead of qtinstall for qt6.


## Building binutils of avr for wasm

Required tools for debugging of AVR family MCU's

### Get the binutil source

Go to one folder back of this simulIDE root directoy ( path is used in pri file which is case sensitive, so follow as it is ). download and extract the binutil source

```
wget https://ftp.gnu.org/gnu/binutils/binutils-2.46.0.tar.zst
tar -I zstd -xf binutils-2.46.0.tar.zst
```

comment our few sections which may obstacle in building process of binutils in wasm

```
cd <path-to-binutils-2.46.0>/libiberty

for f in \
    strstr.c strchr.c strrchr.c strdup.c strndup.c \
    strncmp.c strncasecmp.c strcasecmp.c \
    strerror.c strsignal.c strtol.c strtoul.c strtoll.c strtoull.c \
    memcmp.c memcpy.c memmove.c memset.c memchr.c mempcpy.c \
    bcmp.c bcopy.c bzero.c \
    index.c rindex.c getcwd.c getpagesize.c \
    atexit.c setenv.c putenv.c \
    snprintf.c vsnprintf.c asprintf.c vasprintf.c \
    sigsetmask.c waitpid.c \
    pex-unix.c pex-common.c pex-one.c
do
    [ -f "$f" ] || continue
    [ -f "$f.orig" ] || cp "$f" "$f.orig"
    echo "/* stubbed for wasm build — provided by emscripten libc */" > "$f"
done



cd <path-to-binutils-2.46.0>/gas    

for f in \
    strstr.c strchr.c strrchr.c strdup.c strndup.c \
    strncmp.c strncasecmp.c strcasecmp.c messages.c \
    strerror.c strsignal.c strtol.c strtoul.c strtoll.c strtoull.c \
    memcmp.c memcpy.c memmove.c memset.c memchr.c mempcpy.c \
    bcmp.c bcopy.c bzero.c \
    index.c rindex.c getcwd.c getpagesize.c \
    atexit.c setenv.c putenv.c \
    snprintf.c vsnprintf.c asprintf.c vasprintf.c \
    sigsetmask.c waitpid.c \
    pex-unix.c pex-common.c pex-one.c
do
    [ -f "$f" ] || continue
    [ -f "$f.orig" ] || cp "$f" "$f.orig"
    echo "/* stubbed for wasm build — provided by emscripten libc */" > "$f"
done

```

then follow * Set up the build environment * section for environement and below command to configure.


```
cd <parent directory of simulIDE> && mkdir binutils-avr-wasm-build && cd binutils-avr-wasm-build

emconfigure ../binutils-2.46.0/configure \
    --target=avr \
    --host=wasm32-unknown-emscripten \
    --prefix="$PWD/binutils-install" \
    --disable-shared --enable-static --disable-werror --disable-nls \
    --disable-gdb --disable-sim --disable-readline --disable-libdecnumber \
    --without-mpfr --without-mpc --without-gmp \
    --without-zstd --without-zlib --without-isl --disable-plugins \
    CFLAGS="-O2 -fno-exceptions -pthread -D_GNU_SOURCE \
            -include limits.h -include unistd.h -include fcntl.h \
            -include string.h -include stdint.h -include sys/wait.h \
            -Wno-error=implicit-function-declaration \
            -Wno-error=int-conversion" \
    CXXFLAGS="-O2 -fno-exceptions -pthread -D_GNU_SOURCE" \
    LDFLAGS="-pthread"

```

now build it with 

```
emmake make all-bfd all-opcodes all-libiberty all-binutils
```

note that binutils need to built seperately for each emsdk different version.

