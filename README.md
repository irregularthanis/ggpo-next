![](doc/images/ggpo_header.png)

[![Linux](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/linux-compatibility.yml)
[![Windows](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/windows-compatibility.yml)
[![MinGW](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/mingw-compatibility.yml)
[![Android](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/android-compatibility.yml)
[![macOS](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/macos-compatibility.yml)
[![Cross-platform E2E](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml/badge.svg?branch=master)](https://github.com/irregularthanis/ggpo-next/actions/workflows/cross-platform-e2e.yml)

(日本語ドキュメントは[こちら](README.ja.md))

## What's GGPO NEXT?

This repository is a (maybe temporary?) fork of the [original GGPO](https://github.com/pond3r/ggpo) SDK lib. Everything here stands on the shoulders of those giants, so any recognition, donations etc, belongs there, not here.

### Why make this?
The reason this fork exists as it is right now is to:
 - Document runtime, api reference and build systems.
 - Support multi-platform, beyond Windows.
 - Proper release pipeline.

### What's new compared to the original GGPO

**Platforms**

| Platform | SDK | Vector War sample | Tested in CI on |
| --- | --- | --- | --- |
| Windows, MSVC | x64, x86 | Win32/GDI | `windows-2022` |
| Windows, MinGW-w64 | x64, x86 | — | `windows-2022` |
| Linux | x86_64 (GCC or Clang) | SDL2 | Ubuntu 24.04 |
| Android | arm64-v8a, armeabi-v7a, x86_64; API 21+ | SDL2 with touch controls (APK) | ARM and x86_64 runners, API 21 and 35 emulators |
| macOS | 11+, universal arm64 + x86_64 | SDL2 app | macOS 15 and 26, Apple Silicon and Intel |

All platforms share one portable core behind a small OS adapter, use the same
public API and wire format, and play each other. CI plays every ordered pair
of Linux, Windows (MSVC), Android and macOS, each also against itself, across
two hosts over a real network, plus MinGW against the others; and a live
four-platform match through the deployed server.

**SDK and packaging**

- One CMake project for every platform; every SDK ships both the shared and
  the static library behind one `GGPO::GGPO` CMake package.
- Tested release packages on every tag: Windows ZIPs (MSVC and MinGW), Linux
  DEB, RPM and tar.gz, a multi-ABI Android SDK, a universal macOS SDK, and
  ready-to-run Vector War builds (Windows, Linux, Android APK, macOS `.dmg`),
  each extracted or installed and checked before upload, with `SHA256SUMS`.
- The original Windows ABI, export list and packed wire layout are frozen and
  checked on every change.

**Internet play (Vector War)**

- A start screen, and matchmaking for 2 to 4 players with optional lobby
  codes, through a small UDP [matchmaking and relay server](services/matchmaking).
- UDP hole punching with automatic relay fallback, chosen per pair of players;
  an in-game legend shows each player's platform and route.
- Cross-platform determinism: consistent floating-point settings on every
  platform and a fixed trigonometry table, so different CPUs, compilers and
  maths libraries stay in sync.
- Terraform for a free-tier OCI server with a DuckDNS name; every deployment
  is qualified by a real four-platform match through it.

  <img width="627" height="502" alt="image" src="https://github.com/user-attachments/assets/1d297a99-0047-45cd-bf93-da9d11b00347" />


**Testing and CI**

- Per-platform compatibility workflows, a native cross-platform E2E matrix,
  emulated NAT topologies, weekly sanitizer runs (ASan and UBSan), and change
  routing that runs only what a change affects. See
  [CI workflows](doc/architecture/CIWorkflows.md).

**Documentation**

- [Architecture](doc/architecture/README.md), a complete
  [API reference](doc/architecture/API.md), the
  [Developer Guide](doc/DeveloperGuide.md) for every platform, the
  [testing architecture](doc/architecture/Testing.md) and the
  [CI workflows](doc/architecture/CIWorkflows.md).

### Are you merging back to upstream?
Maybe. Once this is mature enough we will reach out. It will depend on the original maintainer.

### Is this vibe-coded?
No, but it is AI-assisted.

  
  
  
---



## What is GGPO?

Traditional techniques account for network transmission time by adding delay to a players input, resulting in a sluggish, laggy game-feel.  Rollback networking uses input prediction and speculative execution to send player inputs to the game immediately, providing the illusion of a zero-latency network.  Using rollback, the same timings, reactions, visual and audio queues, and muscle memory your players build up playing offline will translate directly online.  The GGPO networking SDK is designed to make incorporating rollback networking into new and existing games as easy as possible.  

For more information about the history of GGPO, check out http://ggpo.net/

This repository contains the code, documentation, and sample applications for the SDK.

Architecture internals are documented in
[`doc/architecture`](doc/architecture/README.md). The
[public API reference](doc/architecture/API.md) covers every exported function,
callback, type, event, and result. Build, installation, and application
integration guidance is in the [Developer Guide](doc/DeveloperGuide.md).

## Install or build the SDK

The SDK supports Windows (MSVC and MinGW-w64, x64 and x86), Linux, Android
(arm64-v8a, armeabi-v7a, and x86_64) and macOS 11 and later (one universal
arm64 + x86_64 library, tested on macOS 15 and 26). Vector War runs on Windows
(Win32/GDI), Linux (SDL2), Android (SDL2/touch), and macOS (SDL2). iOS and
other unsupported SDK targets fail
configuration explicitly.

Choose either:

- **Release SDK:** download a tested Windows ZIP, Linux x86-64 package, or
  multi-ABI Android tar.gz, or universal macOS tar.gz from
  [Releases](https://github.com/irregularthanis/ggpo-next/releases). This is the
  shortest path for application developers.
- **Source build:** use CMake directly on Windows, the repository's Ubuntu
  24.04 Docker image on Linux, CMake with the Android NDK, or CMake with Xcode
  on macOS.

Every SDK contains both the shared and the static library. Applications link
`GGPO::GGPO` (shared by default) and choose static with `-DGGPO_USE_STATIC=ON`.
Installation, linking each flavor with or without CMake, runtime deployment,
and source-build commands are in the
[Developer Guide](doc/DeveloperGuide.md#install-the-sdk).

## Sample Application

Vector War, in [`src/apps/vectorwar`](src/apps/vectorwar), is a small game that
uses GGPO to keep 2 to 4 players in sync. Run it without arguments for the
start screen, which offers matchmaking over the Internet (release builds
already fill in the public server) or direct play by address. Direct play from
the command line:

```
VectorWar <localport> <num players> ('local' | <remote ip>:<remote port>) for each player
```

The `.cmd` files in [`bin`](bin) start 2, 3 and 4 player games on one Windows
machine. Release downloads, platform notes and the matchmaking command line
are in the [Developer Guide](doc/DeveloperGuide.md#run-the-standalone-vector-war-sample).

## Licensing

GGPO is available under The MIT License. This means GGPO is free for commercial and non-commercial use. Attribution is not required, but appreciated. 
