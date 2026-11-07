# Testing architecture

## Contents

- [Purpose](#purpose)
- [Test layers](#test-layers)
- [Execution flow](#execution-flow)
- [Windows compatibility baseline](#windows-compatibility-baseline)
- [Runtime fixtures](#runtime-fixtures)
- [Build and CI integration](#build-and-ci-integration)
- [Evidence and interpretation](#evidence-and-interpretation)
- [Current limits and follow-ups](#current-limits-and-follow-ups)

## Purpose

GGPO's tests freeze Windows compatibility while qualifying the shared core and
Linux adapter. They protect public ABI, packed protocol data, deterministic
rollback, and real loopback sessions without requiring Vector War.

The fixtures live under [`tests`](../../tests). Their frozen values represent
the current compatibility contract; they should not be updated merely to make a
behavior-changing refactor pass.

## Test layers

| Layer | Contract | Source | Runs against |
| --- | --- | --- | --- |
| Public ABI | Public sizes, offsets, enum values, callback order, and DLL exports | [`public_layout.cpp`](../../tests/baseline/windows/public_layout.cpp), [`exports-x64.txt`](../../tests/baseline/windows/exports-x64.txt) | Compiled header and shared DLL |
| Installed consumers | C11 and C++98 compile/link use only installed SDK files | [`tests/consumer`](../../tests/consumer/CMakeLists.txt), [`verify_installed_consumers.ps1`](../../scripts/ci/verify_installed_consumers.ps1) | Installed header and shared/static library |
| Wire layout | Packed `UdpMsg` sizes, offsets, and representative packet lengths | [`packet_layout.cpp`](../../tests/baseline/windows/packet_layout.cpp) | Internal protocol structures |
| Platform contract | Virtual clock/wait, fallbacks, endpoint/socket results, failure, and cleanup | [`platform_fake_test.cpp`](../../tests/platform/fake/platform_fake_test.cpp) | Deterministic fake adapter |
| Platform-free rollback | Production core linked without the Windows adapter | [`platform_fake.cpp`](../../tests/platform/fake/platform_fake.cpp), [`synctest_smoke.cpp`](../../tests/smoke/synctest_smoke.cpp) | Fake adapter and production rollback core |
| Rollback behavior | Save, restore, synchronized input, replay, and deterministic final state | [`synctest_smoke.cpp`](../../tests/smoke/synctest_smoke.cpp) | SyncTest backend without sockets |
| Windows transport | Handshake, events, prediction, rollback, UDP delivery, and shutdown | [`peer_smoke.cpp`](../../tests/smoke/peer_smoke.cpp), [`run_peer_smoke.py`](../../tests/smoke/run_peer_smoke.py) | Two GGPO processes over localhost |
| Negative peer contracts | Startup, timeout, checksum mismatch, and shutdown failures are rejected | [`peer_runner_negative.py`](../../tests/smoke/peer_runner_negative.py) | Fault-injected real peer processes |
| Build compatibility | x64/Win32, shared/static, Debug/Release | [`windows-compatibility.yml`](../../.github/workflows/windows-compatibility.yml) | Eight Windows build variants |
| Linux transport | The same peer contract through POSIX UDP | [`peer_smoke.cpp`](../../tests/smoke/peer_smoke.cpp), [`platform_linux.cpp`](../../src/lib/ggpo/platform_linux.cpp) | Two native Linux processes |
| Linux package | GCC/Clang, static/shared, installed consumers and ABI ceiling | [`linux-compatibility.yml`](../../.github/workflows/linux-compatibility.yml) | Shared Ubuntu 24.04 image |
| Cross-host | Native Windows and Linux peers converge | [`compare_peer_results.py`](../../tests/smoke/compare_peer_results.py) | Controlled Windows↔Ubuntu/SteamOS hosts |

The shared/static lanes are both required. Shared builds additionally validate
DLL exports and import linkage; static builds ensure consumers do not depend on
DLL-only definitions or lifecycle behavior.

## Execution flow

```mermaid
flowchart TD
    Configure["Configure<br/>Windows<br/>build<br/>tests enabled"]
    Build["Build GGPO,<br/>Vector War<br/>and<br/>compatibility<br/>fixtures"]
    Layout["Public and<br/>packet<br/>layout checks"]
    Sync["Deterministic<br/>SyncTest smoke"]
    Fake["Fake adapter<br/>contract and<br/>core SyncTest"]
    Peers["Two-process<br/>Windows<br/>UDP loopback<br/>smoke"]
    Consumers["Install SDK;<br/>compile and link<br/>C11 and C++98<br/>consumers"]
    Capture["Capture<br/>exports,<br/>layouts,<br/>hashes<br/>and<br/>environment"]
    Noise{"Repeated<br/>timing<br/>requested?"}
    Repeat["Repeat<br/>peer pair<br/>and summarize<br/>duration"]
    Evidence["Upload<br/>compatibility<br/>evidence"]

    Configure --> Build
    Build --> Layout
    Build --> Sync
    Build --> Fake
    Build --> Peers
    Peers --> Consumers --> Capture
    Layout --> Capture
    Sync --> Capture
    Fake --> Capture
    Capture --> Noise
    Noise -->|"yes"| Repeat --> Evidence
    Noise -->|"no"| Evidence
```

The diagram shows validation order, not runtime threads. CTest may schedule
independent tests differently when parallel execution is requested.

## Windows compatibility baseline

### Public layout and exports

[`windows_public_layout`](../../tests/baseline/windows/public_layout.cpp) prints
and verifies consumer-visible sizes and offsets. Pointer-sized callback slots
adapt to x64 or Win32, while fixed public values must remain unchanged.

For shared builds, the evidence script runs MSVC `dumpbin /exports`, normalizes
the `ggpo_*` symbols, and records them. x64 additionally compares the result
with [`exports-x64.txt`](../../tests/baseline/windows/exports-x64.txt).

### Packet layout

[`windows_packet_layout`](../../tests/baseline/windows/packet_layout.cpp)
verifies the packed `UdpMsg` header, union members, input-bit offset, and packet
sizes for empty and nine-bit input payloads. This catches compiler-packing or
field changes before they silently alter peer-visible datagrams.

It does not prove safe parsing, delivery, endianness across different hosts, or
protocol-version compatibility; those require separate tests.

## Runtime fixtures

### Shared deterministic state

[`smoke_session.cpp`](../../tests/smoke/smoke_session.cpp) provides a small
game-owned state, checksum, and the callbacks GGPO needs to save, load, free,
and replay it.

```mermaid
flowchart LR
    Inputs["Two integer<br/>inputs"]
    Step["SmokeApply<br/>Inputs<br/>deterministic<br/>state step"]
    State["Frame,<br/>accumulator,<br/>disconnect<br/>flags"]
    Save["save_game_<br/>state<br/>copy and<br/>checksum"]
    Load["load_game_<br/>state<br/>restore copy"]
    Replay["advance_<br/>frame<br/>reapply<br/>synchronized<br/>inputs"]

    Inputs --> Step --> State
    State --> Save
    Load --> State
    State --> Replay --> Step
```

This is a test simulation, not a model of a real game's state or performance.

### Deterministic SyncTest

[`windows_synctest_smoke`](../../tests/smoke/synctest_smoke.cpp) creates two
local players and submits fixed inputs for 90 frames. It advances the same
inputs through a direct reference state and through GGPO, then requires:

- equal final state and checksum;
- at least one saved-state load and replayed frame;
- exactly one running event; and
- successful session shutdown.

A pass shows that the callbacks and local rollback path converge. It does not
exercise Winsock, peer negotiation, or process timing.

The same fixture also builds as `fake_platform_synctest`, linking the production
rollback sources to [`platform_fake.cpp`](../../tests/platform/fake/platform_fake.cpp)
instead of the Windows adapter. `fake_platform_contract` separately checks
virtual time, configuration fallbacks, endpoint representation, would-block
results, scripted socket-open failure, close behavior, and balanced network
lifetime calls.

### Two-process loopback

[`run_peer_smoke.py`](../../tests/smoke/run_peer_smoke.py) reserves two ephemeral
localhost ports and launches two copies of
[`windows_peer_loopback`](../../tests/smoke/peer_smoke.cpp). Each process owns
one local player and treats the other as remote.

```mermaid
sequenceDiagram
    participant Runner as Python<br/>runner
    participant A as Peer A<br/>process
    participant B as Peer B<br/>process

    Runner->>A: Start<br/>player 1<br/>on port A
    Runner->>B: Start<br/>player 2<br/>on port B
    A->>B: GGPO sync<br/>request<br/>over UDP
    B->>A: GGPO sync<br/>reply<br/>over UDP
    loop Until frame 180
        A->>B: Local input<br/>and acknow<br/>ledgements
        B->>A: Local input<br/>and acknow<br/>ledgements
        A->>A: Predict,<br/>restore and<br/>replay as<br/>needed
        B->>B: Predict,<br/>restore and<br/>replay as<br/>needed
    end
    A-->>Runner: Frame,<br/>checksum and<br/>callback<br/>counts
    B-->>Runner: Frame,<br/>checksum and<br/>callback<br/>counts
    Runner->>Runner: Require equal<br/>frame and<br/>checksum
```

A pass establishes real platform socket and two-peer behavior on one machine.
It does not establish Internet/NAT behavior, loss tolerance, spectators, more
than two players, or interoperability between operating systems.

### Negative peer contracts

[`peer_runner_negative.py`](../../tests/smoke/peer_runner_negative.py) launches
the real peer fixture with one controlled test-only fault at a time. It requires
the runner to reject startup failure, bounded timeout, divergent checksums, and
shutdown failure with the matching diagnostic. The fault variable is removed
from ordinary peer environments, so these cases cannot weaken positive smoke
tests.

### Native cross-host smoke

Run the same smoke executable on controlled Windows and Linux hosts. Start both
commands together, using each host's reachable LAN address:

```text
# Windows
ggpo_windows_peer_smoke.exe 1 7000 7001 180 windows.json LINUX_IP

# Ubuntu or SteamOS
./ggpo_linux_peer_smoke 2 7001 7000 180 linux.json WINDOWS_IP
```

Copy both result files to one machine and validate them with
[`compare_peer_results.py`](../../tests/smoke/compare_peer_results.py). Record
both artifact hashes, OS versions, commands, and the validator output. Wine is
not native Windows interoperability evidence.

## Build and CI integration

[`tests/CMakeLists.txt`](../../tests/CMakeLists.txt) registers Windows-only
layout checks plus shared smoke families. Linux registers:

```text
linux_synctest_smoke
linux_peer_loopback
linux_peer_negative_contracts
fake_platform_synctest
fake_platform_contract
```

Windows retains these entries:

```text
windows_public_layout
windows_packet_layout
windows_synctest_smoke
windows_peer_loopback
windows_peer_negative_contracts
fake_platform_synctest
fake_platform_contract
```

With `BUILD_TESTING=ON` and the default `GGPO_RUN_TESTS_ON_BUILD=ON`, the
`ggpo_check` target runs these tests as part of the normal build. Packaging or
consumer-only builds can disable execution explicitly.

[`windows_compatibility_baseline.ps1`](../../scripts/ci/windows_compatibility_baseline.ps1)
configures, builds, runs CTest, installs the SDK, compiles and links the two
consumer fixtures, and calls the evidence collector. GitHub Actions runs it
across this matrix:

| Architecture | Linkage | Configuration |
| --- | --- | --- |
| x64 | Shared and static | Debug and Release |
| Win32 | Shared and static | Debug and Release |

The final [Windows compatibility verifier](../../scripts/ci/verify_windows_compatibility.ps1)
builds the frozen Windows baseline and the current candidate. It compares
exports and layouts, then runs both cross-revision directions: the baseline
executable with the candidate DLL and the candidate executable with the
baseline DLL.

[`linux-compatibility.yml`](../../.github/workflows/linux-compatibility.yml)
builds [`docker/linux/Dockerfile`](../../docker/linux/Dockerfile), then runs the
GCC/Clang × static/shared matrix inside that image. Each lane runs CTest,
installs the SDK, builds and executes external C11/C++98 consumers, and audits
the installed Linux library.

The GCC and Clang portable-header jobs also compile the public C11 and C++98
consumers without linking an SDK. These remain header-portability gates.
The Clang C++ gate suppresses only `-Wextern-c-compat`: C requires the documented
one-byte `GGPOPlayer.local` placeholder while C++ retains the frozen empty-member
Windows ABI. All other pedantic diagnostics remain errors.

[`appveyor.yml`](../../appveyor.yml) also runs CTest and captures evidence for
its existing x64 Windows builds. Unsupported SDK platforms now fail CMake
configuration explicitly instead of selecting the incomplete legacy Unix file.

## Evidence and interpretation

[`capture_windows_compatibility_baseline.ps1`](../../scripts/ci/capture_windows_compatibility_baseline.ps1)
records:

- public and packet layout reports;
- normalized and raw DLL exports for shared builds;
- CMake configuration and source identity;
- built-artifact sizes and SHA-256 values; and
- optional repeated loopback results and elapsed-time summary.

Hashes identify the exact artifacts; different Debug/Release or shared/static
hashes are expected. Repeated duration is a noise measurement for designing
later A/B comparisons, not a performance threshold. Runtime compatibility is
established by the frame, checksum, callback, and lifecycle assertions.

## Current limits and follow-ups

- No focused unit tests yet cover `InputQueue`, `Sync`, bit vectors, ring
  buffers, or time synchronization.
- No malformed/truncated packet suite validates parser rejection.
- No spectator, disconnect/resume, simulated loss, or multi-player automated
  session exists.
- Native Windows↔Ubuntu and Windows↔SteamOS sessions require controlled hosts;
  ordinary isolated GitHub-hosted jobs cannot provide this network.
- No macOS adapter or build exists.
- No sanitizer job is configured.

The platform-extraction plan tracks those additions in the
[technical plan](../planning/2026-11-06-platform_agnostic_core/TECH_PLAN.md) and
[progress ledger](../planning/2026-11-06-platform_agnostic_core/PROGRESS.md).
Return to the [architecture overview](Overview.md).
