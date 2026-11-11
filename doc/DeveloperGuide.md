# GGPO Developer Guide

The GGPO Network Library Developer Guide is for developers who are integrating the GGPO Network Library into their applications.

## Game State and Inputs

Your game probably has many moving parts.  GGPO only depends on these two: 

- **Game State** describes the current state of everything in your game.  In a shooter, this would include the position of the ship and all the enemies on the screen, the location of all the bullets, how much health each opponent has, the current score, etc. etc. 

- **Game Inputs** are the set of things which modify the game state.  These obviously include the joystick and button presses done by the player, but can include other non-obvious inputs as well.  For example, if your game uses the current time of day to calculate something in the game, the current time of day at the beginning of a frame is also an input.

There are many other things in your game engine that are neither game state nor inputs.  For example, your audio and video renderers are not game state since they don't have an effect on the outcome of the game.  If you have a special effects engine that's generating effects that do not have an impact on the game, they can be excluded from the game state as well.

## Using State and Inputs for Synchronization

Each player in a GGPO networked game has a complete copy of your game running.  GGPO needs to keep both copies of the game state in sync to ensure that both players are experiencing the same game.  It would be much too expensive to send an entire copy of the game state between players every frame.  Instead GGPO sends the players' inputs to each other and has each player step the game forward.  In order for this to work, your game engine must meet three criteria:

- The game simulation must be fully deterministic.  That is, for any given game state and inputs, advancing the game state by exactly 1 frame must result in identical game states for all players.
- The game state must be fully encapsulated and serializable.  
- Your game engine must be able to load, save, and execute a single simulation frame without rendering the result of that frame.  This will be used to implement rollbacks.

## Install the SDK

The SDK supports Windows (MSVC and MinGW-w64), Linux, and Android. This section
starts with what every platform shares, then has one self-contained section per
platform: download, build from source, link (dynamic or static), and deploy.

| Platform | Release download | Go to |
| --- | --- | --- |
| Windows, Visual Studio (MSVC, clang-cl) | `ggpo-0.5.0-windows-x64.zip`, `ggpo-0.5.0-windows-x86.zip` | [Windows with MSVC](#windows-with-msvc) |
| Windows, MinGW-w64 GCC (MSYS2, cross compilers) | `ggpo-0.5.0-windows-x64-mingw.zip`, `ggpo-0.5.0-windows-x86-mingw.zip` | [Windows with MinGW-w64](#windows-with-mingw-w64) |
| Linux x86_64 | `ggpo-0.5.0-linux-x86_64.deb` / `.rpm` / `.tar.gz` | [Linux](#linux) |
| Android (arm64-v8a, armeabi-v7a, x86_64) | `ggpo-0.5.0-android.tar.gz` | [Android](#android) |

Downloads are on [GGPO Next Releases](https://github.com/irregularthanis/ggpo-next/releases).

### Common to all platforms

Everything in this section applies to every platform. The platform sections
only add what differs.

#### SDK contents

- Every SDK is an installed CMake package: `include/ggponet.h`,
  `lib/cmake/GGPO/`, `share/GGPO/` (license and readme), and **both** a shared
  and a static library.
- The public header, API, ABI and wire format are the same on every platform,
  so peers on different platforms can play each other as long as the game
  itself is deterministic across them ([floating point](#floating-point-determinism)).
- Every release lists checksums in `SHA256SUMS`; verify a download before
  extracting it.
- Install each SDK in its own directory. Never merge two SDKs (for example MSVC
  and MinGW, or two Android ABIs) into one prefix.

#### Link GGPO into an application

The application picks one flavor per executable and links only that one:

| | Dynamic (shared) | Static |
| --- | --- | --- |
| What you ship | The application **and** the GGPO runtime library | The application only (on MinGW, see [fully static executables](#mingw-fully-static-executables)) |
| Updating GGPO | Replace the runtime library (same major version) | Relink the application |
| Good for | Several programs sharing one GGPO; any MinGW environment | One self-contained binary; no runtime version mismatch |

For an application named `application_x`:

```cpp
#include <ggponet.h>

int main()
{
    GGPOSession *session = 0;
    (void)session;
    return 0;
}
```

The application's `CMakeLists.txt` is the same on every platform:

```cmake
cmake_minimum_required(VERSION 3.10)
project(application_x LANGUAGES CXX)

find_package(GGPO CONFIG REQUIRED)
add_executable(application_x main.cpp)
target_link_libraries(application_x PRIVATE GGPO::GGPO)
```

- Point CMake at the SDK with `-DCMAKE_PREFIX_PATH=<sdk directory>` (not needed
  for Linux DEB/RPM installs).
- `GGPO::GGPO` is the **shared** library by default. Configure with
  `-DGGPO_USE_STATIC=ON` to link the static library instead, or name the
  flavor in the project:

  ```cmake
  find_package(GGPO CONFIG REQUIRED COMPONENTS static)
  target_link_libraries(application_x PRIVATE GGPO::static)   # or GGPO::shared
  ```

- The targets carry the include directory, any system libraries, and
  `GGPO_SHARED_LIB` when it is needed. Link one flavor per executable, not both.

#### Linking without CMake

Each platform section gives exact compiler commands. They follow the same
rules:

- Add `<sdk>/include` to the include path and link either the shared library
  or the static library file, never both.
- On Windows, define `GGPO_SHARED_LIB` for the shared library only; the static
  library must **not** define it. Other platforms ignore it.
- With GNU-style linkers, list libraries **after** the sources: they resolve
  left to right.
- GGPO is C++. A C application that links the static library must do its final
  link with the C++ driver (`g++`, `clang++`) or add `-lstdc++`. CMake does
  this automatically.

#### Build options

These apply to every source build, whatever the platform:

| Option | Effect |
| --- | --- |
| *(default)* | Build both the shared and the static library |
| `-DBUILD_SHARED_LIBS=ON` / `OFF` | Build only the shared / only the static library |
| `-DGGPO_BUILD_SHARED=OFF`, `-DGGPO_BUILD_STATIC=OFF` | Turn off one flavor individually |
| `-DGGPO_BUILD_VECTORWAR=OFF` | Skip Vector War (always off for MinGW) |
| `-DGGPO_BUILD_VECTORWAR_GUI=OFF` | Build only the portable Vector War core and headless fixture, without SDL or Win32 frontends |
| `-DGGPO_RUN_TESTS_ON_BUILD=OFF` | Build tests but run them explicitly with `ctest` |
| `-DBUILD_TESTING=OFF` | Consumer or packaging-only build, no tests |

#### Floating-point determinism

GGPO only keeps peers in sync if every peer's game simulation computes
bit-identical results. GGPO's own builds force consistent floating point where
a platform's default differs, and a game playing across platforms should use
the same flags for its simulation code:

| Platform | Flag used by GGPO | Why |
| --- | --- | --- |
| 32-bit MinGW | `-msse2 -mfpmath=sse` | GCC's x87 default rounds differently from MSVC, x64 and Linux |
| Android | `-ffp-contract=off` | Clang fuses multiply-add (FMA) on ARM, changing results |

#### Deployment and license

- **Dynamic:** ship GGPO's runtime library where the platform loader finds it
  (details per platform below).
- **Static:** no GGPO file at run time.
- GGPO is MIT licensed, so either flavor may be distributed in closed-source
  applications; include the license text from `share/GGPO/LICENSE` either way.

### Windows with MSVC

For Visual Studio and clang-cl applications.

#### Install the MSVC SDK

Download `windows-x64.zip` (64-bit) or `windows-x86.zip` (32-bit), verify it
with `Get-FileHash -Algorithm SHA256`, and extract it to its own folder such as
`C:/ggpo-sdk`:

```text
C:/ggpo-sdk/
├── include/ggponet.h
├── bin/GGPO.dll                  # shared library (runtime)
├── lib/GGPO.lib                  # import library for GGPO.dll
├── lib/libGGPO.lib               # static library
├── lib/*.pdb                     # debug symbols
├── lib/cmake/GGPO/
└── share/GGPO/
```

Never extract it into the same folder as a MinGW SDK: both contain
`lib/cmake/GGPO/`. `find_package(GGPO)` rejects a MinGW SDK in an MSVC project
with a message naming the right download.

#### Build the MSVC SDK from source

Install Visual Studio 2022 with the Desktop development with C++ workload,
CMake, and Python 3. From a Developer PowerShell prompt in the source checkout:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DGGPO_BUILD_SDK=ON `
  -DGGPO_BUILD_VECTORWAR=ON
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix C:/ggpo-sdk
```

Change `-A x64` to `-A Win32` for a 32-bit SDK. The install contains both
`GGPO.dll` (with its `GGPO.lib` import library) and `libGGPO.lib`. For an
SDL-free build of only the portable Vector War core and headless fixture, keep
`GGPO_BUILD_VECTORWAR=ON` and add `-DGGPO_BUILD_VECTORWAR_GUI=OFF`.

The [common build options](#build-options) apply.

#### Link with MSVC

Release SDKs are Release builds that use the dynamic C runtime (`/MD`):

| | Dynamic | Static |
| --- | --- | --- |
| Link against | `lib/GGPO.lib` | `lib/libGGPO.lib` + `ws2_32.lib winmm.lib` |
| `GGPO_SHARED_LIB` | Defined | **Not** defined |
| Application configurations | Debug and Release alike | `/MD` only (Release, RelWithDebInfo) |
| CMake configure | `cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/ggpo-sdk` | add `-DGGPO_USE_STATIC=ON` |
| Without CMake | `cl /MD /I C:\ggpo-sdk\include /DGGPO_SHARED_LIB main.cpp /link /LIBPATH:C:\ggpo-sdk\lib GGPO.lib` | `cl /MD /I C:\ggpo-sdk\include main.cpp /link /LIBPATH:C:\ggpo-sdk\lib libGGPO.lib ws2_32.lib winmm.lib` |

Build with `cmake --build build --config Release --parallel`.

MSVC rejects `libGGPO.lib` in a Debug (`/MDd`) or `/MT` build with a
runtime-library mismatch (`LNK2038`). For a Debug static build, either link the
DLL in Debug configurations or [build GGPO from source](#build-the-msvc-sdk-from-source)
with `--config Debug`.

#### Deploy with MSVC

- **Dynamic:** copy `bin/GGPO.dll` next to `application_x.exe`. For
  development, adding the SDK's `bin` directory to `PATH` is enough.
- **Static:** no GGPO file. Ship the usual VC++ redistributable for `/MD`.

### Windows with MinGW-w64

For GCC-based MinGW-w64 applications: MSYS2 shells, standalone toolchains, and
`x86_64-w64-mingw32` / `i686-w64-mingw32` cross compilers on Linux.

#### Choose a MinGW SDK

Download `windows-x64-mingw.zip` (64-bit) or `windows-x86-mingw.zip` (32-bit)
and extract it to its own folder, such as `C:/ggpo-sdk-mingw` (`/c/ggpo-sdk-mingw`
in MSYS2). It contains `bin/libGGPO.dll`, the import library
`lib/libGGPO.dll.a`, the static `lib/libGGPO.a`, and
`share/GGPO/TOOLCHAIN.txt`, which names the toolchain that built it. Never
merge it with an MSVC SDK: MinGW's linker also searches `.lib` names, and
`find_package(GGPO)` rejects an MSVC SDK in a MinGW project.

Both SDKs use the MSVCRT C runtime: x64 is built in MSYS2 `MINGW64` and x86 in
`MINGW32`, matching existing MinGW projects such as FBNeo.

- **`libGGPO.dll` works with any MinGW toolchain** of the same architecture,
  including MSYS2 `UCRT64`, `CLANG64` and llvm-mingw. It embeds the MinGW C/C++
  runtime and exposes only C, so applications need not ship `libgcc`,
  `libstdc++` or `libwinpthread` DLLs for GGPO.
- **`libGGPO.a` must match your toolchain.** It is C++ code linked against the
  application's own runtime, so three things must agree with the SDK:
  1. **C runtime:** MSVCRT.
  2. **Exception model:** SEH for x64, DWARF-2 for x86 (MSYS2's defaults).
  3. **GCC version:** the same or newer than the GCC in `TOOLCHAIN.txt`; an
     older GCC can fail with missing `libstdc++` symbols.

| Toolchain | Dynamic `libGGPO.dll` | Static x64 `libGGPO.a` | Static x86 `libGGPO.a` |
| --- | --- | --- | --- |
| MSYS2 `MINGW64` / `MINGW32`, current GCC | Yes | **Tested** | **Tested** |
| Other GCC builds with the same runtime and exception model (matching WinLibs or MinGW-builds flavors, distribution cross compilers), same or newer GCC | Yes | Expected to work, untested | Expected to work, untested |
| x86 toolchains using SJLJ exceptions (some older MinGW-builds) | Yes | — | **Not supported:** link errors |
| UCRT toolchains (MSYS2 `UCRT64`, llvm-mingw `*-ucrt`) | Yes | **Not supported** | **Not supported** |
| Clang-based MinGW (llvm-mingw, MSYS2 `CLANG64`), libc++ | Yes | **Not supported** | **Not supported** |
| GCC older than the SDK's | Yes | May fail to link | May fail to link |

Check a toolchain with `g++ -v` (look for `--enable-sjlj-exceptions`, or
`--disable-dw2-exceptions` on i686, which rule it out). If static linking is
not supported for your toolchain, use the DLL or
[build GGPO from source](#build-the-mingw-sdk-from-source) with that toolchain;
the static library then matches by construction.

A MinGW program can also use the **MSVC** SDK's `GGPO.dll`, linked directly
without CMake, because GGPO's interface is plain C:

```bash
gcc -DGGPO_SHARED_LIB -I/c/ggpo-sdk/include main.c /c/ggpo-sdk/bin/GGPO.dll
```

The MSVC SDK's static `libGGPO.lib` cannot be used from MinGW.

#### Build the MinGW SDK from source

From an MSYS2 `MINGW64` shell (x64) or `MINGW32` shell (x86), matching the
release SDKs. Any MSYS2 environment, including `UCRT64`, works for a build that
your own project then links:

```bash
pacman -S --needed mingw-w64-x86_64-{gcc,cmake,ninja,python}   # MINGW32: mingw-w64-i686-*, UCRT64: mingw-w64-ucrt-x86_64-*
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/ggpo-sdk-mingw"
cmake --build build/mingw --parallel
cmake --install build/mingw
```

To cross-compile from Linux, pass the
[toolchain file below](#cross-compile-for-windows-from-linux) with
`-DCMAKE_TOOLCHAIN_FILE`; the commands are otherwise the same. Tests need
Windows (or Wine) to run, so add `-DBUILD_TESTING=OFF` when only producing the
SDK. Vector War is not built with MinGW.

#### Link with MinGW-w64

| | Dynamic | Static |
| --- | --- | --- |
| Link against | `lib/libGGPO.dll.a` | `lib/libGGPO.a` + `-lws2_32 -lwinmm` |
| `GGPO_SHARED_LIB` | Defined | **Not** defined |
| Toolchains | Any MinGW of the same architecture | [Matching toolchains](#choose-a-mingw-sdk) only |
| CMake configure (MSYS2) | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/c/ggpo-sdk-mingw` | add `-DGGPO_USE_STATIC=ON` |
| CMake configure (cross from Linux) | `cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/ggpo-sdk-mingw` | add `-DGGPO_USE_STATIC=ON` |
| Without CMake (MSYS2) | `g++ -DGGPO_SHARED_LIB -I/c/ggpo-sdk-mingw/include main.cpp -L/c/ggpo-sdk-mingw/lib -lGGPO` | `g++ -I/c/ggpo-sdk-mingw/include main.cpp /c/ggpo-sdk-mingw/lib/libGGPO.a -lws2_32 -lwinmm -static` |
| Without CMake (cross from Linux) | `x86_64-w64-mingw32-g++ -DGGPO_SHARED_LIB -I$HOME/ggpo-sdk-mingw/include main.cpp -L$HOME/ggpo-sdk-mingw/lib -lGGPO` | `x86_64-w64-mingw32-g++ -I$HOME/ggpo-sdk-mingw/include main.cpp $HOME/ggpo-sdk-mingw/lib/libGGPO.a -lws2_32 -lwinmm -static` |

Libraries go **after** the sources: GNU linkers resolve left to right.

**32-bit:** GGPO's x86 MinGW build compiles with `-msse2 -mfpmath=sse`. GCC's
x87 default rounds floating point differently from MSVC, x64 and Linux, which
desynchronizes deterministic simulations. Compile the game's simulation code
with the same flags if it uses floating point and plays against other
platforms.

#### Cross-compile for Windows from Linux

Extract the MinGW SDK on the Linux host and use a toolchain file such as:

```cmake
# mingw-w64-x86_64.cmake  (use i686-w64-mingw32 for 32-bit)
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 $ENV{HOME}/ggpo-sdk-mingw)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

With `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY`, which most cross toolchain files
set, `find_package` searches only below `CMAKE_FIND_ROOT_PATH`. Add the SDK
directory there, as above, or `find_package(GGPO)` will not find it.

Distribution cross compilers (Debian/Ubuntu `g++-mingw-w64`, Fedora
`mingw64-gcc-c++`) use MSVCRT and work with the dynamic SDK. For the static
SDK, check them against the [compatibility table](#choose-a-mingw-sdk) or build
GGPO from source with the same cross compiler.

#### MinGW: fully static executables

Linking `libGGPO.a` puts GGPO inside the executable, but GCC still links its
own runtime dynamically by default. The resulting `.exe` then needs
`libstdc++-6.dll`, `libgcc_s_seh-1.dll` (x64) or `libgcc_s_dw2-1.dll` (x86),
and `libwinpthread-1.dll` at run time, like any other MinGW C++ program. To
produce a single self-contained `.exe`, link the runtime statically too:

```cmake
if(MINGW)
    target_link_options(application_x PRIVATE -static)
endif()
```

or pass `-static` on the `g++` command line, as in the table above.
`-static-libgcc -static-libstdc++` alone is not enough when `libwinpthread` is a
DLL in your toolchain. Check the result with
`objdump -p application_x.exe | grep "DLL Name"`: only Windows system DLLs
(`KERNEL32.dll`, `msvcrt.dll`, `WS2_32.dll`, `WINMM.dll`, ...) should remain.

#### Deploy with MinGW-w64

- **Dynamic:** copy `bin/libGGPO.dll` next to `application_x.exe` (or
  `GGPO.dll` when using the MSVC DLL). It needs no MinGW runtime DLLs; your
  application's own runtime is still your choice.
- **Static:** no GGPO file. Ship your toolchain's runtime DLLs unless the
  executable is [fully static](#mingw-fully-static-executables).

### Linux

For x86_64 applications built with GCC or Clang.

#### Install with DEB or RPM

```bash
# Debian or Ubuntu
sudo apt install ./ggpo-0.5.0-linux-x86_64.deb
# Fedora or another RPM-family system
sudo dnf install ./ggpo-0.5.0-linux-x86_64.rpm
```

The package is named `ggpo-sdk` and installs under `/usr`: the public header in
`/usr/include`, `libGGPO.so*`, `libGGPO.a` and the CMake metadata in `/usr/lib`,
and license/readme files in `/usr/share/GGPO`. Ordinary
`find_package(GGPO CONFIG REQUIRED)` finds it without an additional prefix.

Upgrading from 0.3.0 replaces whichever of `ggpo-sdk-shared` or
`ggpo-sdk-static` is installed.

#### Install the raw SDK (`tar.gz`)

Use the raw archive for an unprivileged, application-local, or CI installation:

```bash
sha256sum --check SHA256SUMS --ignore-missing
tar -xzf ggpo-0.5.0-linux-x86_64.tar.gz
```

```text
ggpo-0.5.0-linux-x86_64/
├── include/ggponet.h
├── lib/libGGPO.so -> libGGPO.so.0
├── lib/libGGPO.so.0 -> libGGPO.so.0.5.0
├── lib/libGGPO.so.0.5.0
├── lib/libGGPO.a
├── lib/cmake/GGPO/
└── share/GGPO/
    ├── LICENSE
    └── README.md
```

All Linux formats contain the same files. The package format only controls how
the SDK is installed.

#### Build the Linux SDK from source

Install Docker, then run the same Ubuntu 24.04 image used by Linux CI:

```bash
python3 scripts/linux/build_in_container.py \
  --compiler gcc --packages --out build/linux-gcc
```

This builds, tests and installs both libraries into `build/linux-gcc/install`;
verified DEB, RPM, and raw artifacts are written to `build/linux-gcc/packages`.
Select `clang` with `--compiler clang`. `--library shared` or `--library static`
builds a single flavor instead.

Run the extended sanitizer qualification with the same image:

```bash
python3 scripts/linux/build_in_container.py \
  --compiler clang --library static --sanitizers \
  --out build/linux-sanitizers
```

This instruments production GGPO objects, all registered tests, and the
installed C11/C++98 consumers with ASan and UBSan. Sanitizer runs use the static
library only.

#### Link on Linux

| | Dynamic | Static |
| --- | --- | --- |
| Link against | `lib/libGGPO.so` | `lib/libGGPO.a` |
| CMake configure (DEB/RPM) | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` | add `-DGGPO_USE_STATIC=ON` |
| CMake configure (`tar.gz`) | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$HOME/ggpo-0.5.0-linux-x86_64` | add `-DGGPO_USE_STATIC=ON` |
| Without CMake | `g++ -I$HOME/ggpo-0.5.0-linux-x86_64/include main.cpp -L$HOME/ggpo-0.5.0-linux-x86_64/lib -lGGPO -Wl,-rpath,'$ORIGIN'` | `g++ -I$HOME/ggpo-0.5.0-linux-x86_64/include main.cpp $HOME/ggpo-0.5.0-linux-x86_64/lib/libGGPO.a` |

`GGPO_SHARED_LIB` is not used on Linux. Build with
`cmake --build build --parallel`.

#### Deploy on Linux

- **Dynamic:** DEB and RPM installs need nothing extra. Otherwise ship
  `libGGPO.so.0` with the application and set an application-relative RPATH:

  ```cmake
  set_target_properties(application_x PROPERTIES
      INSTALL_RPATH "$ORIGIN/../lib")
  ```

  During development, `LD_LIBRARY_PATH=$HOME/ggpo-0.5.0-linux-x86_64/lib ./application_x`
  also works.
- **Static:** no GGPO file.

### Android

For NDK applications on arm64-v8a, armeabi-v7a, and x86_64, API 21 or newer.

#### Install the Android SDK

`ggpo-0.5.0-android.tar.gz` contains one complete CMake prefix per ABI:

```text
ggpo-0.5.0-android/
├── arm64-v8a/{include,lib,share}/
├── armeabi-v7a/{include,lib,share}/
└── x86_64/{include,lib,share}/
```

Always select the directory matching the application's `ANDROID_ABI`. In a
Gradle `externalNativeBuild`, pass
`-DCMAKE_PREFIX_PATH=/path/to/ggpo-0.5.0-android/${ANDROID_ABI}` so each ABI
finds its own prefix.

#### Build the Android SDK from source

Install Android Studio or the command-line SDK with the NDK, CMake, and Ninja.
Build one ABI at a time; API 21 is the supported floor:

```bash
cmake -S . -B build/android -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=21 \
  -DCMAKE_BUILD_TYPE=Release -DGGPO_BUILD_VECTORWAR=OFF \
  -DBUILD_TESTING=OFF -DCMAKE_INSTALL_PREFIX="$PWD/build/android-install"
cmake --build build/android --parallel
cmake --install build/android
```

Use `armeabi-v7a` or `x86_64` for the other release ABIs. Normal repository CI
also builds and runs the Android test suite; `BUILD_TESTING=OFF` above is only
for an application SDK build without a connected device.

#### Link on Android

| | Dynamic | Static |
| --- | --- | --- |
| Link against | `lib/libGGPO.so` | `lib/libGGPO.a` |
| CMake configure | `cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=21 -DCMAKE_PREFIX_PATH=$HOME/ggpo-0.5.0-android/arm64-v8a` | add `-DGGPO_USE_STATIC=ON` |
| Without CMake (`$triple` = `aarch64-linux-android21`, etc.) | `$triple-clang++ -I$HOME/ggpo-0.5.0-android/$abi/include main.cpp -L$HOME/ggpo-0.5.0-android/$abi/lib -lGGPO` | `$triple-clang++ -I$HOME/ggpo-0.5.0-android/$abi/include main.cpp $HOME/ggpo-0.5.0-android/$abi/lib/libGGPO.a` |

`GGPO_SHARED_LIB` is not used on Android.

#### Deploy on Android

- **Dynamic:** package the matching ABI's unversioned `libGGPO.so` under the
  APK's `lib/<abi>/` directory; Gradle does this for linked native libraries.
- **Static:** no separate GGPO file; GGPO is linked into the application's
  native library.

### Run the standalone Vector War sample

Release pages also attach Vector War builds, independent of the SDK linkage:

| Platform | Archive | Run |
| --- | --- | --- |
| Windows x64 / x86 | `vectorwar-windows-x64.zip`, `vectorwar-windows-x86.zip` (Win32/GDI) | `VectorWar.exe`, same arguments as Linux |
| Linux x86_64 | `vectorwar-linux-x86_64.tar.gz` (SDL2) | See below |
| Android, all ABIs | `vectorwar-android.apk` (SDL2) | `adb install ggpo-0.5.0-vectorwar-android.apk` |

On Linux, install the SDL2 runtime, extract the archive, and launch two local
players in separate terminals:

```bash
sudo apt install libsdl2-2.0-0
./vectorwar 7000 2 local 127.0.0.1:7001
./vectorwar 7001 2 127.0.0.1:7000 local
```

Arrow keys steer, `D` fires, `S` bombs, function keys disconnect players, and
Escape exits. The Android launcher accepts the same local port, player count,
and peer endpoints; touch controls map to the same steering, fire, and bomb
inputs. Vector War is not built with MinGW. The sample archives include the
license and the detailed [Vector War architecture guide](architecture/VectorWar.md).

### Upgrading from 0.3.0

| 0.3.0 download | 0.4.0 download |
| --- | --- |
| `windows-x64-shared.zip` / `windows-x64-static.zip` | `windows-x64.zip` |
| `windows-x86-shared.zip` / `windows-x86-static.zip` | `windows-x86.zip` |
| `linux-x86_64-shared.{deb,rpm,tar.gz}` / `linux-x86_64-static.{deb,rpm,tar.gz}` | `linux-x86_64.{deb,rpm,tar.gz}` |

Projects that linked `GGPO::GGPO` against a **static** 0.3.0 SDK should add
`-DGGPO_USE_STATIC=ON`, because the combined SDK defaults to shared.
`GGPOTargets.cmake` is now split into `GGPOSharedTargets.cmake` and
`GGPOStaticTargets.cmake`; include `GGPOConfig.cmake` (via `find_package`)
rather than either file directly.

See the [build architecture](architecture/Build.md#install-layout) for the
canonical layout and the [release workflow](architecture/Build.md#release-packages)
for its validation and publication contract.

## Programming guide

GGPO coordinates rollback by calling the application through
`GGPOSessionCallbacks`. The public declarations are in
[`ggponet.h`](../src/include/ggponet.h); [Vector War](architecture/VectorWar.md)
is the complete reference integration.

### Create a session

`GGPOSession` is opaque, so applications hold a pointer. Populate every callback
before starting the session:

```cpp
GGPOSession *ggpo = 0;
GGPOSessionCallbacks cb = { 0 };

cb.begin_game      = game_begin;
cb.save_game_state = game_save;
cb.load_game_state = game_load;
cb.log_game_state  = game_log;
cb.free_buffer     = game_free;
cb.advance_frame   = game_advance_rollback_frame;
cb.on_event        = game_on_event;

GGPOErrorCode result = ggpo_start_session(
    &ggpo, &cb, "my_game", 2, sizeof(GameInput), 7000);
if (!GGPO_SUCCEEDED(result)) {
    /* Report the error and do not use ggpo. */
}
```

The application name is diagnostic metadata. The final argument is the local
UDP port; remote endpoints are registered separately. A session represents one
match. Close it with `ggpo_close_session(ggpo)` before replacing it.

### Register players

Call `ggpo_add_player()` exactly once per player. `player_num` is one-based and
must be within the session's declared player count.

```cpp
GGPOPlayer local = { 0 };
local.size = sizeof(local);
local.type = GGPO_PLAYERTYPE_LOCAL;
local.player_num = 1;

GGPOPlayer remote = { 0 };
remote.size = sizeof(remote);
remote.type = GGPO_PLAYERTYPE_REMOTE;
remote.player_num = 2;
strcpy(remote.u.remote.ip_address, "192.168.0.100");
remote.u.remote.port = 7001;

GGPOPlayerHandle local_handle;
GGPOPlayerHandle remote_handle;
result = ggpo_add_player(ggpo, &local, &local_handle);
if (GGPO_SUCCEEDED(result)) {
    result = ggpo_add_player(ggpo, &remote, &remote_handle);
}
```

Keep the local handle: `ggpo_add_local_input()` requires it. Event callbacks
use the returned handles to identify peers.

### Implement state callbacks

The saved buffer must contain all deterministic simulation state and no
presentation-only state. The save callback owns the allocation until GGPO calls
`free_buffer`.

```cpp
GameState game_state;

bool GGPO_CALL game_save(unsigned char **buffer, int *len,
                         int *checksum, int)
{
    *len = sizeof(game_state);
    *buffer = (unsigned char *)malloc(*len);
    if (!*buffer) return false;
    memcpy(*buffer, &game_state, *len);
    *checksum = calculate_checksum(*buffer, *len);
    return true;
}

bool GGPO_CALL game_load(unsigned char *buffer, int len)
{
    if (len != sizeof(game_state)) return false;
    memcpy(&game_state, buffer, len);
    return true;
}

void GGPO_CALL game_free(void *buffer)
{
    free(buffer);
}
```

Also implement `log_game_state` for SyncTest diagnostics, `on_event` for
connection state, and the deprecated `begin_game` callback as a successful
no-op. The [API reference](architecture/API.md#required-callbacks) describes all
seven callback contracts.

### Run one frame

Submit each local player's current input, request the synchronized input array,
and advance only on success:

```cpp
GameInput local_input = read_local_input();
GameInput inputs[2] = { 0 };
int disconnect_flags = 0;

result = ggpo_add_local_input(
    ggpo, local_handle, &local_input, sizeof(local_input));
if (GGPO_SUCCEEDED(result)) {
    result = ggpo_synchronize_input(
        ggpo, inputs, sizeof(inputs), &disconnect_flags);
}
if (GGPO_SUCCEEDED(result)) {
    advance_deterministic_state(inputs, disconnect_flags);
    ggpo_advance_frame(ggpo);
    render_current_state();
}
ggpo_idle(ggpo, 0);
```

During rollback, `advance_frame` must call `ggpo_synchronize_input()`, advance
exactly one deterministic frame, and call `ggpo_advance_frame()`. It must not
sample devices, render, or emit presentation effects for replayed frames. The
[Vector War callbacks](../src/apps/vectorwar/vectorwar.cpp) provide the tested
reference flow.

## Tuning Your Application: Frame Delay vs. Speculative Execution

GGPO uses both frame delay and speculative execution to hide latency.  It does so by allowing the application developer the choice of how many frames that they'd like to delay input by.  If it takes more time to transmit a packet than the number of frames specified by the game, GGPO will use speculative execution to hide the remaining latency.  This number can be tuned by the application mid-game if you so desire.  Choosing a proper value for the frame delay depends very much on your game.  Here are some helpful hints.

In general you should try to make your frame delay as high as possible without affecting the qualitative experience of the game.  For example, a fighting game requires pixel perfect accuracy, excellent timing, and extremely tightly controlled joystick motions.  For this type of game, any frame delay larger than 1 can be noticed by most intermediate players, and expert players may even notice a single frame of delay.  On the other hand, board games or puzzle games which do not have very strict timing requirements may get away with setting the frame latency as high as 4 or 5 before users begin to notice.

Another reason to set the frame delay high is to eliminate the glitching that can occur during a rollback. The longer the rollback, the more likely the user is to notice the discontinuities caused by temporarily executing the incorrect prediction frames.  For example, suppose your game has a feature where the entire screen will flash for exactly 2 frames immediately after the user presses a button.  Suppose further that you've chosen a value of 1 for the frame latency and the time to transmit a packet is 4 frames.  In this case, a rollback is likely to be around 3 frames (4 – 1 = 3).  If the flash occurs on the first frame of the rollback, your 2-second flash will be entirely consumed by the rollback, and the remote player will never get to see it!  In this case, you're better off either specifying a higher frame latency value or redesigning your video renderer to delay the flash until after the rollback occurs.

## Sample Application

The Vector War application in the source directory contains a simple application which uses GGPO to synchronize the two clients.  The command line arguments are:

```
vectorwar.exe  <localport>  <num players> ('local' | <remote ip>:<remote port>) for each player
```

See the .cmd files in the bin directory for examples on how to start 2, 3, and 4 player games.

## Best Practices and Troubleshooting

Below is a list of recommended best practices you should consider while porting your application to GGPO.  Many of these recommendations are easy to follow even if you're not starting a game from scratch.  Most applications will already conform to most of the recommendations below.

### Isolate Game State from Non-Game State

GGPO will periodically request that you save and load the entire state of your game.  For most games the state that needs to be saved is a tiny fraction of the entire game.  Usually the video and audio renderers, look up tables, textures, sound data and your code segments are either constant from frame to frame or not involved in the calculation of game state.  These do not need to be saved or restored.

You should isolate non-game state from the game state as much as possible.  For example, you may consider encapsulating all your game state into a single C structure.  This both clearly delineates what is game state and was is not and makes it trivial to implement the save and load callbacks (see the Reference Guide for more information).

### Define a Fixed Time Quanta for Advancing Your Game State

GGPO will occasionally need to rollback and single-step your application frame by frame.  This is difficult to do if your game state advances by a variable tick rate.  You should try to make your game state advanced by a fixed time quanta per frame, even if your render loop does not.

### Separate Updating Game State from Rendering in Your Game Loop

GGPO will call your advance frame callback many times during a rollback.  Any effects or sounds which are genearted during the rollback need to be deferred until after the rollback is finished.  This is most easily accomplished by separating your game state from your render state.  When you're finished, your game loop may look something like this:

```
   Bool finished = FALSE;
   GameState state;
   Inputs inputs;

   do {
      GetControllerInputs(&inputs);
      finished = AdvanceGameState(&inputs, &state);
      if (!finished) {
         RenderCurrentFrame(&gamestate);
      }
   while (!finished);
```

In other words, your game state should be determined solely by the inputs, your rendering code should be driven by the current game state, and you should have a way to easily advance the game state forward using a set of inputs without rendering.

### Make Sure Your Game State Advances Deterministically

Once you have your game state identified, make sure the next game state is computed solely from your game inputs.  This should happen naturally if you have correctly identified all the game state and inputs, but it can be tricky sometimes.  Here are some things which are easy to overlook:

#### Beware of Random Number Generators

Many games use random numbers in the computing of the next game state.  If you use one, you must ensure that they are fully deterministic, that the seed for the random number generator is same at frame 0 for both players, and that the state of the random number generator is included in your game state.  Doing both of these will ensure that the random numbers which get generated for a particular frame are always the same, regardless of how many times GGPO needs to rollback to that frame.

#### Beware of External Time Sources (aka. Wall clock time)

Be careful if you use the current time of day in your game state calculation.  This may be used for an effect on the game or to derive other game state (e.g. using the timer as a seed to the random number generator).  The time on two computers or game consoles is almost never in sync and using time in your game state calculations can lead to synchronization issues.  You should either eliminate the use of time in your game state or include the current time for one of the players as part of the input to a frame and always use that time in your calculations.

The use of external time sources in non-gamestate calculations is fine (e.g. computing the duration of effects on screen, or the attenuation of audio samples).

### Beware of Dangling References

If your game state contains any dynamically allocated memory be very careful in your save and load functions to rebase your pointers as you save and load your data.  One way to mitigate this is to use a base and offset to reference allocated memory instead of a pointer.  This can greatly reduce the number of pointers you need to rebase.

### Beware of Static Variables or Other Hidden State

The language your game is written in may have features which make it difficult to track down all your state.   Static automatic variables in C are an example of this behavior.  You need to track down all these locations and convert them to a form which can be saved.  For example, compare:

```
   // This will totally get you into trouble.
   int get_next_counter(void) {
      static int counter = 0; /* no way to roll this back... */
      counter++;
      return counter;
   }
```

To:
```
   // If you must, this is better
   static int global_counter = 0; /* move counter to a global */

   int get_next_counter(void) {
      global_counter++;
      return global_counter; /* use the global value */
   }

   bool __cdecl
   ggpo_load_game_state_callback(unsigned char *buffer, int len)
   {
      ...
      global_counter = *((int *)buffer) /* restore it in load callback */
      ...
      return true;
   }
```

### Use the GGPO SyncTest Feature.  A Lot.

Once you've ported your application to GGPO, you can use the `ggpo_start_synctest` function to help track down synchronization issues which may be the result of leaky game state.  

The sync test session is a special, single player session which is designed to find errors in your simulation's determinism.  When running in a synctest session, GGPO will execute a 1 frame rollback for every frame of your game.  It compares the state of the frame when it was executed the first time to the state executed during the rollback, and raises an error if they differ.  If you used the `ggpo_log` function during your game's execution, you can diff the log of the initial frame vs the log of the rollback frame to track down errors.  

By running synctest on developer systems continuously when writing game code, you can identify desync causing bugs immediately after they're introduced.

## Where to Go from Here

This document describes the most basic features of GGPO.  To learn more, I recommend starting with reading the comments in the `ggponet.h` header and just diving into the code.  Good luck!
