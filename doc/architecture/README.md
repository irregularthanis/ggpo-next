# GGPO architecture

## Contents

- [Purpose](#purpose)
- [Documentation map](#documentation-map)
- [Source of truth](#source-of-truth)

## Purpose

This directory explains how GGPO is built, embedded, and executed. It connects
the public API to the session backends, rollback state, input prediction, UDP
protocol, and platform layer. Start with the [architecture overview](Overview.md).

## Documentation map

| Document | Scope |
| --- | --- |
| [Overview](Overview.md) | Project boundaries and the build/runtime component map |
| [Build](Build.md) | CMake targets, sources, artifacts, and current platform constraints |
| [Runtime](Runtime.md) | Session lifecycle, frame loop, callbacks, and backend selection |
| [Synchronization](Synchronization.md) | Input prediction, saved states, rollback, and confirmation |
| [Networking](Networking.md) | UDP transport, peer protocol, messages, acknowledgements, and time sync |
| [API reference](API.md) | Public types, callbacks, events, errors, and functions |
| [Testing](Testing.md) | Layered ABI, core, platform, transport, package, and CI contracts |
| [Native cross-platform qualification](CrossPlatformTesting.md) | Linux/Windows matrix, deterministic oracle, CI security, evidence, and cleanup |
| [Vector War](VectorWar.md) | End-to-end walkthrough of the sample application's GGPO integration |
| [Matchmaking, relay and hole punching](Matchmaking.md) | Vector War's internet play: lobby and relay server, protocol, path proxy, hole punching with relay fallback, deployment |
| [Portability](Portability.md) | Platform boundaries relevant to a multiplatform port |

## Source of truth

The implementation is authoritative. Documentation links use repository-relative
paths so they continue to resolve in forks and after an upstream merge. The public
contract is declared in [`src/include/ggponet.h`](../../src/include/ggponet.h).
