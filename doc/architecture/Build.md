# Build architecture

## Contents

- [Build outputs](#build-outputs)
- [Build graph](#build-graph)
- [Linux container](#linux-container)
- [Configuration options](#configuration-options)
- [SDK source composition](#sdk-source-composition)
- [Vector War](#vector-war)
- [Install layout](#install-layout)
- [Release packages](#release-packages)
- [Platform status](#platform-status)

## Build outputs

The root [`CMakeLists.txt`](../../CMakeLists.txt) configures two independent
products. [`src/CMakeLists.txt`](../../src/CMakeLists.txt) builds the SDK library;
[`src/apps/vectorwar/CMakeLists.txt`](../../src/apps/vectorwar/CMakeLists.txt)
builds the Windows sample.

| Product | CMake target | Inputs | Output |
| --- | --- | --- | --- |
| GGPO SDK | `GGPO` | Public header, backends, sync core, UDP protocol, platform source | Shared or static library |
| Vector War | `VectorWar` | Sample game, Win32 loop, GDI/OpenGL renderer, `GGPO` | Windows executable |

## Build graph

```mermaid
flowchart TD
    Root["Root<br/>CMakeLists.txt<br/>options and<br/>project"]
    SDKList["src/<br/>CMakeLists.txt<br/>GGPO library<br/>target"]
    Sources["src/<br/>CMakeSources<br/>.cmake<br/>portable SDK<br/>inventory"]
    Public["src/include/<br/>ggponet.h<br/>public API"]
    Core["Backends,<br/>sync, input,<br/>poll and UDP<br/>sources"]
    Platform{"Supported<br/>platform layer?"}
    Windows["platform/windows/<br/>Platform.cmake<br/>Windows source,<br/>defines and libs"]
    Linux["platform/linux/<br/>Platform.cmake<br/>POSIX source<br/>and flags"]
    Unsupported["Configuration<br/>error<br/>unsupported<br/>platform"]
    Library["GGPO library<br/>shared or<br/>static"]
    SampleList["Vector War<br/>CMakeLists.txt<br/>Windows only"]
    Sample["VectorWar<br/>executable"]

    Root -->|"GGPO_BUILD_<br/>SDK"| SDKList
    SDKList --> Sources
    Sources --> Public
    Sources --> Core
    Sources --> Platform
    Platform -->|"Windows"| Windows
    Platform -->|"Linux"| Linux
    Platform -->|"Other"| Unsupported
    Public --> Library
    Core --> Library
    Windows --> Library
    Linux --> Library
    Root -->|"GGPO_BUILD_<br/>VECTORWAR<br/>and WIN32"| SampleList
    SampleList --> Library
    SampleList --> Sample
```

The root selects exactly one supported platform layer. The incomplete legacy
Unix source was replaced by the Linux adapter. See [Portability](Portability.md).

## Linux container

[`docker/linux/Dockerfile`](../../docker/linux/Dockerfile) is the Linux build
environment source of truth for developers, CI, and release packaging.
[`build_in_container.py`](../../scripts/linux/build_in_container.py) builds that
image locally and runs configure, build, CTest, install-consumer, and ABI audits.

```bash
python3 scripts/linux/build_in_container.py \
  --compiler gcc --library shared --packages \
  --out build/linux-gcc-shared
```

CI builds the same Dockerfile with layer caching; it does not maintain a second
package/toolchain recipe.

## Configuration options

Declared in the [root build file](../../CMakeLists.txt):

| Option | Default | Effect |
| --- | ---: | --- |
| `GGPO_BUILD_SDK` | `ON` | Adds the `GGPO` library target |
| `GGPO_BUILD_VECTORWAR` | `ON` | Adds Vector War only when `WIN32` is true |
| `BUILD_SHARED_LIBS` | `ON` | Chooses a shared library instead of a static library |
| `GGPO_RUN_TESTS_ON_BUILD` | `ON` | Runs the platform's self-contained CTests in the default build |

Current Windows setup uses [`configure_windows.cmd`](../../configure_windows.cmd).
The generated solution is described in the root [README](../../README.md).

## SDK source composition

[`src/CMakeSources.cmake`](../../src/CMakeSources.cmake) groups sources by role:

| Group | What it contributes | Representative artifacts |
| --- | --- | --- |
| Core | Inputs, queues, saved state, polling, logging, and time synchronization | [`sync.cpp`](../../src/lib/ggpo/sync.cpp), [`input_queue.cpp`](../../src/lib/ggpo/input_queue.cpp), [`timesync.cpp`](../../src/lib/ggpo/timesync.cpp) |
| Network | UDP socket, wire messages, reliability, and peer state | [`udp.cpp`](../../src/lib/ggpo/network/udp.cpp), [`udp_proto.cpp`](../../src/lib/ggpo/network/udp_proto.cpp), [`udp_msg.h`](../../src/lib/ggpo/network/udp_msg.h) |
| Backends | Peer-to-peer, spectator, and deterministic sync-test sessions | [`backends`](../../src/lib/ggpo/backends) |
| Platform | Selected separately from the portable inventory | [Windows](../../src/platform/windows/Platform.cmake), [Linux](../../src/platform/linux/Platform.cmake) |
| Public | C-compatible integration contract | [`ggponet.h`](../../src/include/ggponet.h) |

Only `src/include` is public. Internal headers remain private to the SDK target.
In-tree and installed consumers link the `GGPO::GGPO` target.

## Vector War

Vector War is a reference integration, not part of the public SDK contract. Its
build is Windows-specific:

- [`main.cpp`](../../src/apps/vectorwar/main.cpp) owns the Win32 message loop,
  Winsock initialization, command-line parsing, and 60 Hz scheduling.
- [`vectorwar.cpp`](../../src/apps/vectorwar/vectorwar.cpp) integrates the API
  and separates game state from rendering.
- [`gamestate.cpp`](../../src/apps/vectorwar/gamestate.cpp) advances the sample
  simulation.
- [`gdi_renderer.cpp`](../../src/apps/vectorwar/gdi_renderer.cpp) presents the
  current state.
- [`ggpo_perfmon.cpp`](../../src/apps/vectorwar/ggpo_perfmon.cpp) displays
  network statistics.

See [Vector War](VectorWar.md) for the end-to-end build and runtime walkthrough.

## Install layout

The SDK install rules in [`src/CMakeLists.txt`](../../src/CMakeLists.txt) place:

```text
<prefix>/
├── include/ggponet.h
├── bin/GGPO.dll                  # shared build
├── lib/GGPO.lib                  # import or static library
├── lib/libGGPO.so.0              # Linux shared build
├── lib/libGGPO.a                 # Linux static build
├── lib/cmake/GGPO/               # CMake package and target export
└── share/GGPO/                   # license and README
```

The install also exports `lib/cmake/GGPO/GGPOConfig.cmake`; consumers use
`find_package(GGPO CONFIG REQUIRED)` and link `GGPO::GGPO`.

## Release packages

Pushing a final tag such as `v0.2.0`, or a prerelease tag such as
`v0.2.0-rc.1`, runs the
[`Package release SDK`](../../.github/workflows/release-packages.yml) workflow.
The tag must match the version declared by the root [`project()`](../../CMakeLists.txt).

The workflow builds Windows x64/x86 and Linux x86-64 SDKs with shared and
static linkage. It also publishes standalone interactive Vector War archives
for Windows x64/x86 and Linux x86-64. Windows publishes ZIP files; Linux
publishes the GUI as a tarball and the SDK in these formats:

| Format | Install shape | Validation |
| --- | --- | --- |
| DEB | `/usr` SDK for Debian-family systems | Extract, run C11/C++98 consumers, ABI audit |
| RPM | `/usr` SDK for RPM-family systems | Extract, run C11/C++98 consumers, ABI audit |
| tar.gz | Prefix-local raw SDK | Extract, run C11/C++98 consumers, ABI audit |

[`package_linux_sdk.py`](../../scripts/ci/package_linux_sdk.py) creates and
validates all three formats from one installed SDK. Shared and static packages
use distinct names and conflict metadata because they own the same headers and
CMake files. All artifacts and the combined `SHA256SUMS` file are attached to
the GitHub Release.

## Platform status

| Platform | Current build status | Main blockers |
| --- | --- | --- |
| Windows | Supported SDK and Vector War build | Existing reference platform |
| Linux | SDK and SDL2 Vector War GUI supported through the Ubuntu 24.04 container | Additional distribution formats remain follow-up work |
| macOS | Explicitly rejected until an Apple platform layer exists | Adapter, CI, lifecycle/frontend, and packaging remain |
| Mobile and consoles | No build target | Platform services, lifecycle, socket policy, packaging, and validation absent |

The build-port roadmap should preserve the public API while moving OS facilities
behind explicit adapters. See [Portability](Portability.md).
