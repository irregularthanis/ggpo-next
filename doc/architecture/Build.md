# Build architecture

## Contents

- [Build outputs](#build-outputs)
- [Build graph](#build-graph)
- [Configuration options](#configuration-options)
- [SDK source composition](#sdk-source-composition)
- [Vector War](#vector-war)
- [Install layout](#install-layout)
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
    Unsupported["Configuration error<br/>Linux and Apple<br/>follow-up required"]
    Library["GGPO library<br/>shared or<br/>static"]
    SampleList["Vector War<br/>CMakeLists.txt<br/>Windows only"]
    Sample["VectorWar<br/>executable"]

    Root -->|"GGPO_BUILD_<br/>SDK"| SDKList
    SDKList --> Sources
    Sources --> Public
    Sources --> Core
    Sources --> Platform
    Platform -->|"Windows"| Windows
    Platform -->|"Other"| Unsupported
    Public --> Library
    Core --> Library
    Windows --> Library
    Root -->|"GGPO_BUILD_<br/>VECTORWAR<br/>and WIN32"| SampleList
    SampleList --> Library
    SampleList --> Sample
```

The root selects exactly one supported platform layer. The incomplete legacy
Unix source is no longer selected implicitly. See [Portability](Portability.md).

## Configuration options

Declared in the [root build file](../../CMakeLists.txt):

| Option | Default | Effect |
| --- | ---: | --- |
| `GGPO_BUILD_SDK` | `ON` | Adds the `GGPO` library target |
| `GGPO_BUILD_VECTORWAR` | `ON` | Adds Vector War only when `WIN32` is true |
| `BUILD_SHARED_LIBS` | `ON` | Chooses a shared library instead of a static library |
| `GGPO_RUN_TESTS_ON_BUILD` | `ON` | Runs the six self-contained CTests in the default build |
| `GGPO_WINDOWS_BUILD_LAYER` | `LAYERED` | Selects the shipped Windows layer; `LEGACY` is temporary qualification-only input |

Current Windows setup uses [`configure_windows.cmd`](../../configure_windows.cmd).
The generated solution is described in the root [README](../../README.md).

## SDK source composition

[`src/CMakeSources.cmake`](../../src/CMakeSources.cmake) groups sources by role:

| Group | What it contributes | Representative artifacts |
| --- | --- | --- |
| Core | Inputs, queues, saved state, polling, logging, and time synchronization | [`sync.cpp`](../../src/lib/ggpo/sync.cpp), [`input_queue.cpp`](../../src/lib/ggpo/input_queue.cpp), [`timesync.cpp`](../../src/lib/ggpo/timesync.cpp) |
| Network | UDP socket, wire messages, reliability, and peer state | [`udp.cpp`](../../src/lib/ggpo/network/udp.cpp), [`udp_proto.cpp`](../../src/lib/ggpo/network/udp_proto.cpp), [`udp_msg.h`](../../src/lib/ggpo/network/udp_msg.h) |
| Backends | Peer-to-peer, spectator, and deterministic sync-test sessions | [`backends`](../../src/lib/ggpo/backends) |
| Platform | Selected separately from the portable inventory | [Windows platform layer](../../src/platform/windows/Platform.cmake), [`platform_windows.cpp`](../../src/lib/ggpo/platform_windows.cpp) |
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
├── bin/<shared library on Windows>
└── lib/<static library or Unix shared library>
```

The install also exports `lib/cmake/GGPO/GGPOConfig.cmake`; consumers use
`find_package(GGPO CONFIG REQUIRED)` and link `GGPO::GGPO`.

## Platform status

| Platform | Upstream build status | Main blockers |
| --- | --- | --- |
| Windows | Supported SDK and Vector War build | Existing reference platform |
| Linux | Explicitly rejected until a Linux platform layer exists | Adapter, CI, sample/frontend, and packaging remain |
| macOS | Explicitly rejected until an Apple platform layer exists | Adapter, CI, lifecycle/frontend, and packaging remain |
| Mobile and consoles | No build target | Platform services, lifecycle, socket policy, packaging, and validation absent |

The build-port roadmap should preserve the public API while moving OS facilities
behind explicit adapters. See [Portability](Portability.md).
