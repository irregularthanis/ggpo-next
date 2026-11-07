# PRD — Linux SDK support

Status: Draft

Source: `2c339c056c4a210638d88b55083e641e78370ca9`

Scope: Linux x86-64 SDK, packages, and native Linux tests; follow-up: native cross-OS interoperability; excluded: a portable Vector War frontend, macOS/mobile ports, and public API changes

## Motivation

GGPO Next currently packages and qualifies only Windows builds. The goal is to let Linux and Steam Deck developers consume the same public GGPO API as a static or shared library, without duplicating portable-core changes or breaking existing Windows consumers.

This change establishes the native Linux SDK and its package contract. Native Windows↔Linux interoperability remains an explicit follow-up qualification rather than a merge or packaging gate.

## Outcomes

- Build the SDK on Ubuntu 24.04 with GCC and Clang as static and shared libraries.
- Make a repository-owned Ubuntu 24.04 Dockerfile the default Linux build environment. Local builds and CI must build and run that same image rather than maintaining separate dependency recipes.
- Install a usable `GGPO::GGPO` CMake package and publish Linux x86-64 DEB, RPM, and raw tar.gz artifacts on release tags.
- Preserve the v0.1.0 public C API/ABI surface in [`ggponet.h`](../../../src/include/ggponet.h); no callback, structure, enum, or exported-function change is in scope.
- Pass deterministic Linux↔Linux sessions with matching inputs, checkpoints, events, and clean shutdown.
- Provide the harness needed for follow-up Windows↔Linux and SteamOS qualification.

## Success and constraints

| Metric | Baseline | Target | Validation |
|---|---|---|---|
| Windows compatibility | Windows-only 8-lane matrix | No regression | Existing Windows compatibility workflow and frozen consumer tests |
| Linux build matrix | Unsupported | GCC/Clang × static/shared pass | Ubuntu 24.04 CI, CTest, installed C11/C++98 consumers |
| Build-environment parity | No Linux environment | Local and CI builds use the same repository Dockerfile | Record image source revision/digest and tool versions in both paths |
| Peer correctness | Windows loopback only | Linux loopback completes 180 frames; cross-OS execution follows after merge | Matching checkpoints, final checksum, event transcript, and exit status |
| Package compatibility ceiling | None | No required `GLIBC` symbol newer than 2.39 | `readelf`/`objdump` audit of release artifacts |
| Package formats | Windows ZIP only | Linux DEB, RPM, and raw tar.gz for shared/static linkage | Extract every artifact; compile, link, and run C11/C++98 consumers; repeat ABI audit |
| Follow-up qualification | None | Windows↔Ubuntu and Windows↔SteamOS peer smoke | Artifact SHA, OS/glibc identity, commands, transcript, and result |

Ubuntu 24.04 is the second-newest Ubuntu LTS as of this plan and supplies glibc 2.39. Current SteamOS 3.9.5 publishes glibc 2.43. The Ubuntu build therefore targets an **older glibc symbol ceiling**, not an identical SteamOS ABI. Actual SteamOS execution remains follow-up evidence.

## Clarifications

- The user selected SDK library, CMake package, tests, and release archives; Vector War portability is follow-up work.
- Windows↔Linux interoperability remains required before claiming cross-OS qualification, but does not block merging or publishing the native Linux SDK.
- The baseline is Ubuntu 24.04, GCC and Clang, x86-64, static and shared output, with Steam Deck compatibility explicitly verified.
- The primary Linux build method is the repository's container image. Its Dockerfile is the single dependency/toolchain recipe used by developers and CI.
- API and ABI compatibility are imperative; this work must not modify the public header contract.
- Linux releases provide DEB, RPM, and raw SDK archives; shared and static packages remain separate.
