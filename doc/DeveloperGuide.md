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

The SDK supports Windows and Linux. Choose a prebuilt release SDK for normal
application integration or use the repository container when developing Linux.

### Option 1: use a release SDK

Download the archive matching the application's platform and architecture from
[GGPO Next Releases](https://github.com/irregularthanis/ggpo-next/releases).
Every SDK contains **both** the shared and the static library; the application
chooses one when it links (see
[Link GGPO into an application](#link-ggpo-into-an-application)).

| Archive | Use it when |
| --- | --- |
| `ggpo-0.5.0-windows-x64.zip` | The application is 64-bit Windows |
| `ggpo-0.5.0-windows-x86.zip` | The application is 32-bit Windows |
| `ggpo-0.5.0-windows-x64-mingw.zip` | The application is 64-bit Windows built with MinGW-w64 (MSVCRT, MSYS2 `MINGW64`) |
| `ggpo-0.5.0-windows-x86-mingw.zip` | The application is 32-bit Windows built with MinGW-w64 (MSVCRT, MSYS2 `MINGW32`) |
| `ggpo-0.5.0-linux-x86_64.deb` / `.rpm` | Install system-wide on a Debian- or RPM-family distribution |
| [`ggpo-0.5.0-linux-x86_64.tar.gz`](#linux-raw-sdk-targz) | Extract a relocatable SDK without a package manager |

Extract the ZIP to a stable location such as `C:/ggpo-sdk`. Each release archive
is an installed CMake package rather than a source tree:

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

Configure the application with the extracted directory as a prefix:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/ggpo-sdk
cmake --build build --config Release --parallel
```

The release page also provides `SHA256SUMS`. Compare the archive's
`Get-FileHash -Algorithm SHA256` result before extracting it.

Windows SDKs are Release builds that use the dynamic C runtime (`/MD`):

- **Dynamic:** `GGPO.dll` works with Debug and Release applications alike.
- **Static:** `libGGPO.lib` links only into applications that also use `/MD`
  (Release or RelWithDebInfo). MSVC rejects it in a Debug (`/MDd`) or `/MT`
  build with a runtime-library mismatch (`LNK2038`). For a Debug static build,
  either link the DLL in Debug configurations or
  [build GGPO from source](#option-3-build-windows-from-source) with
  `--config Debug`.

#### MSVC or MinGW?

Pick the Windows SDK built with the same toolchain as the application:

| Application built with | Use | Shared library | Static library |
| --- | --- | --- | --- |
| Visual Studio (MSVC, clang-cl) | `windows-x64.zip` / `windows-x86.zip` | `GGPO.dll` + `GGPO.lib` | `libGGPO.lib` |
| MinGW-w64 (MSYS2, `x86_64/i686-w64-mingw32` cross compilers) | `windows-x64-mingw.zip` / `windows-x86-mingw.zip` | `libGGPO.dll` + `libGGPO.dll.a` | `libGGPO.a` |

Install each SDK in **its own folder** and never merge them: both contain
`lib/cmake/GGPO/`, and MinGW's linker also searches `.lib` names. CMake
enforces the match: `find_package(GGPO)` fails with a clear message if the SDK
was built by the other toolchain.

What can be mixed:

| Combination | Supported |
| --- | --- |
| MSVC application + MSVC SDK, shared or static | Yes |
| MinGW application + MinGW SDK, shared or static | Yes |
| MinGW application + MSVC `GGPO.dll` (C API only, linked without CMake) | Yes, see [Without CMake](#without-cmake) |
| Any static library across toolchains, or MSVC application + MinGW DLL through CMake | No |

Both MinGW SDKs use the MSVCRT C runtime: x64 is built in MSYS2 `MINGW64` and
x86 in `MINGW32`. This matches existing MinGW projects such as FBNeo. MSYS2's
newer `UCRT64` environment is **not** a static-library match; see below.
`libGGPO.dll` embeds the MinGW C/C++ runtime, so applications do not need to
ship `libgcc`, `libstdc++` or `libwinpthread` DLLs for GGPO.
`share/GGPO/TOOLCHAIN.txt` in each MinGW SDK names its toolchain and runtime.

#### MinGW static library compatibility

`libGGPO.dll` works with any MinGW application of the same architecture: it
embeds its runtime and exposes only a C interface. **`libGGPO.a` is different.**
It is C++ code that links against the application's own C and C++ runtime, so
the application's toolchain must match the SDK's in three ways:

1. **C runtime:** MSVCRT for both SDKs.
2. **Exception model:** SEH for x64, DWARF-2 for x86 (MSYS2's defaults).
3. **GCC version:** the same or newer than the GCC in `TOOLCHAIN.txt`; an older
   GCC can fail with missing `libstdc++` symbols.

| Toolchain | x64 `libGGPO.a` | x86 `libGGPO.a` |
| --- | --- | --- |
| MSYS2 `MINGW64` / `MINGW32`, current GCC | **Tested** | **Tested** |
| Other GCC builds with the same runtime and exception model (e.g. matching WinLibs or MinGW-builds flavors), same or newer GCC | Expected to work, untested | Expected to work, untested |
| x86 toolchains using SJLJ exceptions (some older MinGW-builds) | — | **Not supported:** link errors |
| UCRT toolchains (MSYS2 `UCRT64`, llvm-mingw `*-ucrt`) | **Not supported** | **Not supported** |
| Clang-based MinGW (llvm-mingw, MSYS2 `CLANG64`), libc++ | **Not supported** | **Not supported** |
| GCC older than the SDK's | May fail to link | May fail to link |

If your toolchain is not in a supported row, in particular **MSYS2 `UCRT64`**
(MSYS2's recommended environment for new projects):

- **Use `libGGPO.dll`** from the MinGW SDK. It embeds its own runtime and exposes
  only C, so it works in `UCRT64` and every other MinGW environment of the same
  architecture. Link it with CMake (`GGPO::shared`, the default `GGPO::GGPO`).
- **Or build GGPO from source** with your own toolchain
  ([Option 4](#option-4-build-with-mingw-w64)); the static library then
  matches by construction.
- **Or use the MSVC SDK's `GGPO.dll`**, linked directly without CMake (see
  [Without CMake](#without-cmake)). The MSVC SDK's static `libGGPO.lib` cannot
  be used from MinGW, and CMake rejects the MSVC SDK in a MinGW project.

### Run the standalone Vector War sample

Release pages also attach GUI sample archives independent of the SDK linkage:

| Archive suffix | Contents |
| --- | --- |
| `vectorwar-windows-x64.zip` | 64-bit Win32/GDI Vector War executable |
| `vectorwar-windows-x86.zip` | 32-bit Win32/GDI Vector War executable |
| `vectorwar-linux-x86_64.tar.gz` | 64-bit SDL2 Vector War executable |

On Linux, install the SDL2 runtime, extract the archive, and launch two local
players in separate terminals:

```bash
sudo apt install libsdl2-2.0-0
./vectorwar 7000 2 local 127.0.0.1:7001
./vectorwar 7001 2 127.0.0.1:7000 local
```

Windows uses the same arguments with `VectorWar.exe`. Arrow keys steer, `D`
fires, `S` bombs, function keys disconnect players, and Escape exits. The
sample archives include the license and the detailed
[Vector War architecture guide](architecture/VectorWar.md).

#### Linux DEB and RPM

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

#### Linux raw SDK (`tar.gz`)

Use the raw archive for an unprivileged, application-local, or CI installation:

```bash
sha256sum --check SHA256SUMS --ignore-missing
tar -xzf ggpo-0.5.0-linux-x86_64.tar.gz
cmake -S . -B build -DCMAKE_PREFIX_PATH="$PWD/ggpo-0.5.0-linux-x86_64"
cmake --build build --parallel
```

The extracted SDK contains:

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

### Option 2: build Linux with the CI container

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

### Option 3: build Windows from source

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
`GGPO.dll` (with its `GGPO.lib` import library) and `libGGPO.lib`.

To build only one flavor, add `-DBUILD_SHARED_LIBS=ON` (shared only) or
`-DBUILD_SHARED_LIBS=OFF` (static only), or set `GGPO_BUILD_SHARED` /
`GGPO_BUILD_STATIC` individually. The normal build runs every registered test.
Use `-DGGPO_RUN_TESTS_ON_BUILD=OFF` only when test execution must be explicit,
or `-DBUILD_TESTING=OFF` for a consumer/packaging-only build.

### Option 4: build with MinGW-w64

From an MSYS2 `MINGW64` shell (x64) or `MINGW32` shell (x86), matching the
release SDKs (any MSYS2 environment, including `UCRT64`, works for a build that
your own project then links):

```bash
pacman -S --needed mingw-w64-x86_64-{gcc,cmake,ninja,python}   # MINGW32: mingw-w64-i686-*, UCRT64: mingw-w64-ucrt-x86_64-*
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/ggpo-sdk-mingw"
cmake --build build/mingw --parallel
cmake --install build/mingw
```

To cross-compile from Linux, use a CMake toolchain file that sets
`CMAKE_SYSTEM_NAME Windows` and the `x86_64-w64-mingw32-gcc`/`g++` (or `i686-`)
compilers; the commands are otherwise the same. Tests need Windows (or Wine) to
run, so pass `-DBUILD_TESTING=OFF` when only producing the SDK.

Vector War is not built with MinGW. 32-bit MinGW builds compile with
`-msse2 -mfpmath=sse`: GCC's x87 default rounds floating point differently from
MSVC, x64 and Linux, which desynchronizes deterministic simulations. Compile a
32-bit MinGW **game** with the same flags if it uses floating point in its
simulation and plays against other platforms.

### Link GGPO into an application

Every SDK installs two libraries. The application chooses one per target:

| | Dynamic (shared) | Static |
| --- | --- | --- |
| Windows (MSVC) | `GGPO.dll` + import library `GGPO.lib` | `libGGPO.lib` |
| Windows (MinGW) | `libGGPO.dll` + import library `libGGPO.dll.a` | `libGGPO.a` |
| Linux | `libGGPO.so.0` | `libGGPO.a` |
| What you ship | The application **and** the GGPO runtime library | The application only |
| Updating GGPO | Replace the runtime library (same major version) | Relink the application |

The public header and API are identical for both.

For an application named `application_x`, include the public header in source:

```cpp
#include <ggponet.h>

int main()
{
    GGPOSession *session = 0;
    (void)session;
    return 0;
}
```

#### With CMake

```cmake
cmake_minimum_required(VERSION 3.10)
project(application_x LANGUAGES CXX)

find_package(GGPO CONFIG REQUIRED)
add_executable(application_x main.cpp)
target_link_libraries(application_x PRIVATE GGPO::GGPO)
```

`GGPO::GGPO` links the **shared** library by default. To link statically, set
`GGPO_USE_STATIC` when configuring; the application's `CMakeLists.txt` does not
change:

```bash
# Dynamic (default)
cmake -S . -B build-shared -DCMAKE_PREFIX_PATH=/path/to/ggpo-sdk
# Static
cmake -S . -B build-static -DCMAKE_PREFIX_PATH=/path/to/ggpo-sdk \
  -DGGPO_USE_STATIC=ON
```

Omit `CMAKE_PREFIX_PATH` for a DEB or RPM installation. A project can also name
a flavor directly, or require one when finding the package:

```cmake
find_package(GGPO CONFIG REQUIRED COMPONENTS static)
target_link_libraries(application_x PRIVATE GGPO::static)   # or GGPO::shared
```

The CMake targets carry everything else: the include directory, the Windows
system libraries (`ws2_32`, `winmm`), and on Windows the `GGPO_SHARED_LIB`
definition that the shared library needs (see below). Link one flavor per
executable, not both.

#### Deploy a dynamically linked application

- **Windows:** copy `bin/GGPO.dll` (MSVC) or `bin/libGGPO.dll` (MinGW) next
  to `application_x.exe`. For
  development, adding the SDK's `bin` directory to `PATH` is enough.
- **Linux:** ship `libGGPO.so.0` with the application and set an
  application-relative RPATH, for example:

  ```cmake
  set_target_properties(application_x PROPERTIES
      INSTALL_RPATH "$ORIGIN/../lib")
  ```

  During development, `LD_LIBRARY_PATH=/path/to/ggpo-sdk/lib ./application_x`
  also works. DEB and RPM installations need neither.

A statically linked application needs no GGPO file at run time.

#### Without CMake

Pass the include directory and the library directly. On Windows, the shared
library requires `GGPO_SHARED_LIB` so the header imports its functions from
`GGPO.dll`; the static library must **not** define it.

| | Dynamic | Static |
| --- | --- | --- |
| MSVC | `cl /I C:\ggpo-sdk\include /DGGPO_SHARED_LIB main.cpp /link /LIBPATH:C:\ggpo-sdk\lib GGPO.lib` | `cl /MD /I C:\ggpo-sdk\include main.cpp /link /LIBPATH:C:\ggpo-sdk\lib libGGPO.lib ws2_32.lib winmm.lib` |
| GCC/Clang (Linux) | `g++ -I/path/to/ggpo-sdk/include main.cpp -L/path/to/ggpo-sdk/lib -lGGPO -Wl,-rpath,'$ORIGIN'` | `g++ -I/path/to/ggpo-sdk/include main.cpp /path/to/ggpo-sdk/lib/libGGPO.a` |
| MinGW-w64 | `g++ -DGGPO_SHARED_LIB -I/c/ggpo-sdk-mingw/include main.cpp -L/c/ggpo-sdk-mingw/lib -lGGPO` | `g++ -I/c/ggpo-sdk-mingw/include main.cpp /c/ggpo-sdk-mingw/lib/libGGPO.a -lws2_32 -lwinmm` |

A MinGW program can also use the **MSVC** `GGPO.dll` by linking the DLL file
directly, because GGPO's interface is plain C:

```bash
gcc -DGGPO_SHARED_LIB -I/c/ggpo-sdk/include main.c /c/ggpo-sdk/bin/GGPO.dll
```

GGPO is written in C++. A C application that links `libGGPO.a` statically must
use the C++ compiler driver (`g++`/`clang++`) for its final link step, or add
`-lstdc++`. CMake does this automatically.

#### Choosing a flavor

- **Dynamic** keeps the executable smaller and lets several programs, or a
  GGPO update, share one library. You must ship and locate the runtime file.
- **Static** produces one self-contained executable with nothing to deploy and
  no version mismatch at run time. Updating GGPO means relinking.

GGPO is MIT licensed, so either flavor may be distributed in closed-source
applications; include the license text from `share/GGPO/LICENSE` either way.

#### Upgrading from 0.3.0

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
