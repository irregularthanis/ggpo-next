![](doc/images/ggpo_header.png)

&nbsp; _[![Appveyor build status](https://img.shields.io/appveyor/ci/pond3r/ggpo/master.svg?logo=appveyor)](https://ci.appveyor.com/project/pond3r/ggpo/branch/master)_

(日本語ドキュメントは[こちら](README.ja.md))

## What's GGPO NEXT?

This repository is a (maybe temporary?) fork of the [original GGPO](https://github.com/pond3r/ggpo) SDK lib. Everything here stands on the shoulders of those giants, so any recognition, donations etc, belongs there, not here.

### Why make this?
The reason this fork exists as it is right now is to:
 - Document runtime, api reference and build systems.
 - Support multi-platform, beyond Windows (WIP).
 - Proper release pipeline.

### Are you merging back to upstream?
Maybe. Once this is mature enough we will reach out. It will depend on the original maintainer.

### Is this vibe-coded
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

The SDK supports Windows (MSVC and MinGW-w64, x64 and x86), Linux, and Android
(arm64-v8a, armeabi-v7a, and x86_64). Vector War runs on Windows (Win32/GDI),
Linux (SDL2), and Android (SDL2/touch). Apple and other unsupported SDK targets
fail configuration explicitly.

Choose either:

- **Release SDK:** download a tested Windows ZIP, Linux x86-64 package, or
  multi-ABI Android tar.gz from
  [Releases](https://github.com/irregularthanis/ggpo-next/releases). This is the
  shortest path for application developers.
- **Source build:** use CMake directly on Windows, the repository's Ubuntu
  24.04 Docker image on Linux, or CMake with the Android NDK.

Every SDK contains both the shared and the static library. Applications link
`GGPO::GGPO` (shared by default) and choose static with `-DGGPO_USE_STATIC=ON`.
Installation, linking each flavor with or without CMake, runtime deployment,
and source-build commands are in the
[Developer Guide](doc/DeveloperGuide.md#install-the-sdk).

## Sample Application

The Vector War application in the source directory contains a simple application which uses GGPO to synchronize the two clients.  The command line arguments are:

```
vectorwar.exe  <localport>  <num players> ('local' | <remote ip>:<remote port>) for each player
```

See the .cmd files in the bin directory for examples on how to start 2, 3, and 4 player games.

## Licensing

GGPO is available under The MIT License. This means GGPO is free for commercial and non-commercial use. Attribution is not required, but appreciated. 
