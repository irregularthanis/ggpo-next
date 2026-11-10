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

Download the archive matching the application's architecture and linkage from
[GGPO Next Releases](https://github.com/irregularthanis/ggpo-next/releases):

| Archive suffix | Use it when |
| --- | --- |
| `windows-x64-shared` | The application is 64-bit and deploys `GGPO.dll` |
| `windows-x64-static` | The application is 64-bit and links GGPO into its executable |
| `windows-x86-shared` | The application is 32-bit and deploys `GGPO.dll` |
| `windows-x86-static` | The application is 32-bit and links GGPO into its executable |
| `linux-x86_64-shared.deb` / `.rpm` | Install the shared SDK system-wide on a Debian- or RPM-family distribution |
| `linux-x86_64-static.deb` / `.rpm` | Install the static SDK system-wide on a Debian- or RPM-family distribution |
| [`linux-x86_64-shared.tar.gz`](#linux-raw-sdk-targz) | Extract a relocatable shared SDK without a package manager |
| [`linux-x86_64-static.tar.gz`](#linux-raw-sdk-targz) | Extract a relocatable static SDK without a package manager |

Extract the ZIP to a stable location such as `C:/ggpo-sdk`. Each release archive
is an installed CMake package rather than a source tree:

```text
C:/ggpo-sdk/
├── include/ggponet.h
├── bin/GGPO.dll                  # shared archive only
├── lib/GGPO.lib
├── lib/cmake/GGPO/
└── share/GGPO/
```

Configure the application with the extracted directory as a prefix:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/ggpo-sdk
cmake --build build --config Release --parallel
```

The release page also provides `SHA256SUMS`. Compare the archive's
`Get-FileHash -Algorithm SHA256` result before extracting it. Shared builds must
place `C:/ggpo-sdk/bin/GGPO.dll` beside the application executable or otherwise
make that directory available to the Windows loader.

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

Install the package matching the required linkage:

```bash
# Debian or Ubuntu, shared SDK
sudo apt install ./ggpo-0.3.0-linux-x86_64-shared.deb
# Debian or Ubuntu, static SDK
sudo apt install ./ggpo-0.3.0-linux-x86_64-static.deb

# Fedora or another RPM-family system, shared SDK
sudo dnf install ./ggpo-0.3.0-linux-x86_64-shared.rpm
# Fedora or another RPM-family system, static SDK
sudo dnf install ./ggpo-0.3.0-linux-x86_64-static.rpm
```

Run one command for the chosen package format and linkage, not both linkage
variants on the same system.
The DEB and RPM install under `/usr`; shared and static packages conflict by
design because they own the same headers and CMake metadata. After installation,
ordinary `find_package(GGPO CONFIG REQUIRED)` resolves the SDK without an
additional prefix.

Their installed files mirror the raw SDK below under `/usr`: the public header
in `/usr/include`, libraries and CMake metadata in `/usr/lib`, and license/readme
files in `/usr/share/GGPO`.

#### Linux raw SDK (`tar.gz`)

Use the raw archive for an unprivileged, application-local, or CI installation:

```bash
sha256sum --check SHA256SUMS --ignore-missing
tar -xzf ggpo-0.3.0-linux-x86_64-shared.tar.gz
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH="$PWD/ggpo-0.3.0-linux-x86_64-shared"
cmake --build build --parallel
```

The extracted shared SDK contains:

```text
ggpo-0.3.0-linux-x86_64-shared/
├── include/ggponet.h
├── lib/libGGPO.so -> libGGPO.so.0
├── lib/libGGPO.so.0 -> libGGPO.so.0.3.0
├── lib/libGGPO.so.0.3.0
├── lib/cmake/GGPO/
└── share/GGPO/
    ├── LICENSE
    └── README.md
```

The static archive has the same layout and consumer metadata, but replaces the
three shared-library entries with `lib/libGGPO.a`.

The shared archive contains `lib/libGGPO.so.0`; deploy that library with the
application and make its directory available to the loader, for example with
an application-relative RPATH. The static archive contains `lib/libGGPO.a` and
does not require a GGPO runtime library.

All Linux release formats contain the same public header and CMake target. The
package format controls installation; `shared` versus `static` controls how the
application links and deploys GGPO.

### Option 2: build Linux with the CI container

Install Docker, then run the same Ubuntu 24.04 image used by Linux CI:

```bash
# Build and install the shared SDK.
python3 scripts/linux/build_in_container.py \
  --compiler gcc --library shared --packages \
  --out build/linux-gcc-shared

# Build and install the static SDK.
python3 scripts/linux/build_in_container.py \
  --compiler gcc --library static --packages \
  --out build/linux-gcc-static
```

Each installed CMake package is written to its output's `install` directory;
verified DEB, RPM, and raw artifacts are written to `packages`. Select `clang`
with `--compiler clang` when testing that toolchain.

Run the extended static-library sanitizer qualification with the same image:

```bash
python3 scripts/linux/build_in_container.py \
  --compiler clang --library static --sanitizers \
  --out build/linux-sanitizers
```

This instruments production GGPO objects, all registered tests, and the
installed C11/C++98 consumers with ASan and UBSan.

### Option 3: build Windows from source

Install Visual Studio 2022 with the Desktop development with C++ workload,
CMake, and Python 3. From a Developer PowerShell prompt in the source checkout:

```powershell
# Build and install the shared SDK.
cmake -S . -B build/shared -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=ON `
  -DGGPO_BUILD_SDK=ON `
  -DGGPO_BUILD_VECTORWAR=ON
cmake --build build/shared --config Release --parallel
cmake --install build/shared --config Release --prefix C:/ggpo-sdk-shared

# Build and install the static SDK.
cmake -S . -B build/static -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_SHARED_LIBS=OFF `
  -DGGPO_BUILD_SDK=ON `
  -DGGPO_BUILD_VECTORWAR=ON
cmake --build build/static --config Release --parallel
cmake --install build/static --config Release --prefix C:/ggpo-sdk-static
```

Change `-A x64` to `-A Win32` for a 32-bit SDK. The normal build runs every
registered core, API, protocol, platform, native, and compatibility test. Use
`-DGGPO_RUN_TESTS_ON_BUILD=OFF` only when test execution must be explicit, or
`-DBUILD_TESTING=OFF` for a consumer/packaging-only build.

### Use the installed SDK from an application

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

Then consume the installed CMake package in the application's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.10)
project(application_x LANGUAGES CXX)

find_package(GGPO CONFIG REQUIRED)
add_executable(application_x main.cpp)
target_link_libraries(application_x PRIVATE GGPO::GGPO)
```

For a DEB or RPM installation, configure normally because GGPO is under `/usr`:

```bash
cmake -S . -B build
cmake --build build --parallel
```

For an extracted tarball or a source-built SDK, identify its install prefix:

```bash
# Compile the application against the extracted shared SDK.
cmake -S . -B build-shared \
  -DCMAKE_PREFIX_PATH=/path/to/ggpo-0.3.0-linux-x86_64-shared
cmake --build build-shared --parallel

# Compile the same application against the extracted static SDK.
cmake -S . -B build-static \
  -DCMAKE_PREFIX_PATH=/path/to/ggpo-0.3.0-linux-x86_64-static
cmake --build build-static --parallel
```

The application source and CMake target do not change. The selected prefix makes
`GGPO::GGPO` resolve to `libGGPO.so` or `libGGPO.a` and supplies required system
link dependencies. With the raw shared SDK during development, expose its
runtime library when launching:

```bash
LD_LIBRARY_PATH=/path/to/ggpo-0.3.0-linux-x86_64-shared/lib \
  ./build-shared/application_x
```

For distribution, ship `libGGPO.so.0` with a shared application and configure
an application-relative RPATH, or use the static SDK so GGPO is incorporated
into `application_x` and no GGPO runtime library is required.

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
