![](doc/images/ggpo_header.png)

&nbsp; _[![Appveyor build status](https://img.shields.io/appveyor/ci/pond3r/ggpo/master.svg?logo=appveyor)](https://ci.appveyor.com/project/pond3r/ggpo/branch/master)_

(日本語ドキュメントは[こちら](README.ja.md))

## What's GGPO?

Traditional techniques account for network transmission time by adding delay to a players input, resulting in a sluggish, laggy game-feel.  Rollback networking uses input prediction and speculative execution to send player inputs to the game immediately, providing the illusion of a zero-latency network.  Using rollback, the same timings, reactions, visual and audio queues, and muscle memory your players build up playing offline will translate directly online.  The GGPO networking SDK is designed to make incorporating rollback networking into new and existing games as easy as possible.  

For more information about the history of GGPO, check out http://ggpo.net/

This repository contains the code, documentation, and sample applications for the SDK.

Architecture and API internals are documented in
[`doc/architecture`](doc/architecture/README.md). Build, installation, and
application integration guidance is in the [Developer Guide](doc/DeveloperGuide.md).

## Install or build the SDK

The SDK currently supports Windows builds. Linux and Apple platform adapters are
documented follow-up work; unsupported SDK targets fail configuration explicitly.

Choose either:

- **Release SDK:** download a tested x64/x86 shared or static ZIP from
  [Releases](https://github.com/irregularthanis/ggpo-next/releases). This is the
  shortest path for application developers.
- **Source build:** clone the repository and use CMake when changing GGPO,
  running its tests, or selecting a configuration not distributed as a release.

Both options provide `ggponet.h` and the `GGPO::GGPO` CMake target. Selection,
installation, shared-library deployment, and source-build commands are in the
[Developer Guide](doc/DeveloperGuide.md#install-the-sdk).

## Sample Application

The Vector War application in the source directory contains a simple application which uses GGPO to synchronize the two clients.  The command line arguments are:

```
vectorwar.exe  <localport>  <num players> ('local' | <remote ip>:<remote port>) for each player
```

See the .cmd files in the bin directory for examples on how to start 2, 3, and 4 player games.

## Licensing

GGPO is available under The MIT License. This means GGPO is free for commercial and non-commercial use. Attribution is not required, but appreciated. 
