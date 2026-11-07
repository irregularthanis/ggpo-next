# PROGRESS — Platform-agnostic GGPO core and Windows build layer

Status: M0–M2 complete; M3 implemented and awaiting Windows qualification; M4 planned
[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze Windows contracts

Architecture: [current build and platform status](../../architecture/Build.md#platform-status).

Implemented at [`3498899`](https://github.com/irregularthanis/ggpo-next/commit/34988996cab45c1a37a9c556fe66b192d60a547b)
and qualified by the [Windows compatibility matrix](https://github.com/irregularthanis/ggpo-next/actions/runs/42296246130).

- [x] M0-T01: Build current Windows x64 Debug/Release static/shared outputs.
- [x] M0-T02: Record public DLL exports and public structure/callback layouts.
- [x] M0-T03: Add fixed SyncTest, local-peer, and packet-layout baseline fixtures.
- [x] M0-T04: Record whether Win32/x86 has a qualifying baseline.
- [x] M0-T05: Freeze the legacy build recipe and measure repeated-run noise for A/B interpretation.

### M1: Stabilize the public API and ABI (depends: M0)

Parallel with M2; owns `src/include`, API/ABI fixtures, and installed-consumer
contracts. Architecture: [public API](../../architecture/API.md).

- [x] M1-T01: Introduce portable export/call macros and C boolean support.
- [x] M1-T02: Preserve every M0 export, decoration, signature, enum value, callback slot, and public layout.
- [x] M1-T03: Compile and link installed-header C11 and C++98 consumers.
- [x] M1-T04: Define the ABI/versioning policy for later platform packages.

### M2: Extract platform services (depends: M0)

Parallel with M1; owns internal core/platform sources. Architecture:
[required adapter contracts](../../architecture/Portability.md#required-adapter-contracts).

- [x] M2-T01: Define semantic clock, sleep, config, diagnostic, wait, and UDP contracts.
- [x] M2-T02: Move Win32/Winsock implementations behind the Windows adapters.
- [x] M2-T03: Remove OS and secure-CRT names from portable core sources.
- [x] M2-T04: Re-run M0 runtime and packet fixtures against the adapters.
- [x] M2-T05: Add deterministic fake-adapter core startup, rollback, failure, and cleanup smoke tests.

### M3: Separate the Windows build layer (depends: M2)

May overlap late M1; owns CMake platform selection and Windows target wiring.

- [ ] M3-T01: Make the Windows layer own sources, definitions, and system libraries.
- [ ] M3-T02: Keep core source composition platform-neutral and reject unsupported targets clearly.
- [ ] M3-T03: Expose only public headers and install the `GGPO::GGPO` consumer target.
- [ ] M3-T04: Build the same refactored source through legacy and layered entries and run the B-vs-C A/B gate.
- [ ] M3-T05: Build/install the M0 Windows matrix; enforce default tests, Windows conformance, and UDP loopback.

### M4: Integrate and qualify (depends: M1, M2, M3)

- [ ] M4-T01: Run the compatibility matrix, agnostic smoke, Windows conformance, SyncTest, and local-peer gates in CI.
- [ ] M4-T02: Verify header-only GCC/Clang consumers without claiming a Unix SDK port.
- [ ] M4-T03: Update Build, Portability, API, and Testing architecture pages.
- [ ] M4-T04: Publish complete evidence and the contract for Linux/Apple follow-ups.
- [ ] M4-T05: Run final A-vs-C qualification and remove the temporary legacy entry after all gates pass.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
| --- | --- | --- | --- | --- |
| M0-T01 | [`be5e746`](https://github.com/irregularthanis/ggpo-next/commit/be5e7462794629afb971c7ea5edf60a14810b409), [matrix](https://github.com/irregularthanis/ggpo-next/actions/runs/42296246130) | `windows_compatibility_baseline.ps1`, eight-job CMake matrix | Windows Server 2022; MSVC; x64/Win32; shared/static; Debug/Release | All eight builds and evidence uploads passed |
| M0-T02 | [`public_layout.cpp`](../../../tests/baseline/windows/public_layout.cpp), [`exports-x64.txt`](../../../tests/baseline/windows/exports-x64.txt) | Execute layout fixture; normalize `dumpbin /exports`; compare x64 manifest | Shared x64 and Win32 builds | 16 public exports preserved; public sizes, offsets, enums, and callback slots passed |
| M0-T03 | [`synctest_smoke.cpp`](../../../tests/smoke/synctest_smoke.cpp), [`peer_smoke.cpp`](../../../tests/smoke/peer_smoke.cpp), [`packet_layout.cpp`](../../../tests/baseline/windows/packet_layout.cpp) | CTest runs SyncTest, two loopback processes, and wire-layout assertions | Every matrix lane; 180 peer frames | 4/4 tests passed in every lane; peer checksums matched at frame 180 |
| M0-T04 | [Win32 matrix jobs](https://github.com/irregularthanis/ggpo-next/actions/runs/42296246130) | Build and test four Win32 variants | Win32 shared/static; Debug/Release | x86 is qualified for this baseline |
| M0-T05 | [`windows_compatibility_baseline.ps1`](../../../scripts/ci/windows_compatibility_baseline.ps1), [x64 shared Release evidence](https://github.com/irregularthanis/ggpo-next/actions/runs/42296246130) | Ten sequential two-peer loopback runs after the normal CTest pass | 180 frames/run; matching checksum `0x7db6c67b` | 17,484.7–33,180.3 ms; median 32,984.7 ms; variance requires interleaved repeated A/B comparisons |
| M1-T01 | [`ggponet.h`](../../../src/include/ggponet.h), [`1f31b0a`](https://github.com/irregularthanis/ggpo-next/commit/1f31b0a16a8dae7edfc271db6fd44d5670dc190f) | Strict local header compilation plus Windows matrix | GCC C11 and G++ C++98 with pedantic errors; MSVC x64/Win32 | Portable `GGPO_CALL`, C `bool`, and standard-C local-player declaration compile |
| M1-T02 | [M1 matrix](https://github.com/irregularthanis/ggpo-next/actions/runs/42321026313), [`public_layout.cpp`](../../../tests/baseline/windows/public_layout.cpp), [export manifests](../../../tests/baseline/windows/README.md) | Compare layouts/enums/callback offsets and normalized exports | x64/Win32 shared/static Debug/Release | All eight lanes passed; shared builds retained all 16 exports |
| M1-T03 | [`tests/consumer`](../../../tests/consumer/CMakeLists.txt), [consumer verifier](../../../scripts/ci/verify_installed_consumers.ps1), [M1 matrix](https://github.com/irregularthanis/ggpo-next/actions/runs/42321026313) | Install each SDK, compile and link consumers without source-tree includes | C11 and C++98; all eight Windows variants | Both installed consumers linked in every lane |
| M1-T04 | [API language and ABI contract](../../architecture/API.md#language-and-abi-contract) | Document compatibility policy and package obligations | Public header, static/shared packages, wire protocol boundary | Patch/minor/major and cross-platform ABI rules recorded |
| M2-T01 | [`platform.h`](../../../src/lib/ggpo/platform.h) | Source inspection and strict C++98 compilation | Core time, wait, config, diagnostics, filesystem, endpoint, and UDP needs | Semantic contract compiles without native types |
| M2-T02 | [`platform_windows.cpp`](../../../src/lib/ggpo/platform_windows.cpp), [`70929d1`](https://github.com/irregularthanis/ggpo-next/commit/70929d142c2cdd069b0cda9f0fdf137010c1cbd7) | Replace native core fields/calls with adapter operations | Windows clock, environment, diagnostics, filesystem, Winsock, IPv4, and DLL lifecycle | Win32/Winsock representation is confined to the Windows adapter |
| M2-T03 | [M2 source boundary](../../architecture/Portability.md#current-boundary) | Audit portable core for Win32/Winsock and secure-CRT names | `src/lib/ggpo`, excluding Windows adapter and unsupported legacy Linux adapter | No native Windows or secure-CRT name remains in portable sources |
| M2-T04 | [M2 matrix](https://github.com/irregularthanis/ggpo-next/actions/runs/42340436948) | Eight-lane build, six CTests, installed consumers, export/layout evidence | x64/Win32; shared/static; Debug/Release | All lanes passed; M0 ABI, packet, rollback, and two-peer contracts retained |
| M2-T05 | [`platform_fake_test.cpp`](../../../tests/platform/fake/platform_fake_test.cpp), [`fake_platform_synctest`](../../../tests/CMakeLists.txt) | Link production core to fake adapter; run contract and SyncTest fixtures | Virtual time/config/filesystem/UDP, scripted open failure, rollback state | Startup, rollback, would-block, failure recovery, close, and balanced lifetime checks passed |

