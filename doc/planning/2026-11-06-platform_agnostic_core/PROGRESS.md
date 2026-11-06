# PROGRESS — Platform-agnostic GGPO core and Windows build layer

Status: Planned  
[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze Windows contracts

Architecture: [current build and platform status](../../architecture/Build.md#platform-status).

- [ ] M0-T01: Build current Windows x64 Debug/Release static/shared outputs.
- [ ] M0-T02: Record public DLL exports and public structure/callback layouts.
- [ ] M0-T03: Add fixed SyncTest, local-peer, and packet-layout baseline fixtures.
- [ ] M0-T04: Record whether Win32/x86 has a qualifying baseline.

### M1: Stabilize the public API and ABI (depends: M0)

Parallel with M2; owns `src/include`, API/ABI fixtures, and installed-consumer
contracts. Architecture: [public API](../../architecture/API.md).

- [ ] M1-T01: Introduce portable export/call macros and C boolean support.
- [ ] M1-T02: Preserve every M0 export, decoration, signature, enum value, callback slot, and public layout.
- [ ] M1-T03: Compile and link installed-header C11 and C++98 consumers.
- [ ] M1-T04: Define the ABI/versioning policy for later platform packages.

### M2: Extract platform services (depends: M0)

Parallel with M1; owns internal core/platform sources. Architecture:
[required adapter contracts](../../architecture/Portability.md#required-adapter-contracts).

- [ ] M2-T01: Define semantic clock, sleep, config, diagnostic, wait, and UDP contracts.
- [ ] M2-T02: Move Win32/Winsock implementations behind the Windows adapters.
- [ ] M2-T03: Remove OS and secure-CRT names from portable core sources.
- [ ] M2-T04: Re-run M0 runtime and packet fixtures against the adapters.
- [ ] M2-T05: Add deterministic fake-adapter core startup, rollback, failure, and cleanup smoke tests.

### M3: Separate the Windows build layer (depends: M2)

May overlap late M1; owns CMake platform selection and Windows target wiring.

- [ ] M3-T01: Make the Windows layer own sources, definitions, and system libraries.
- [ ] M3-T02: Keep core source composition platform-neutral and reject unsupported targets clearly.
- [ ] M3-T03: Expose only public headers and install the `GGPO::GGPO` consumer target.
- [ ] M3-T04: Build and install the complete M0 Windows matrix.
- [ ] M3-T05: Run shared platform-contract and UDP-loopback smoke tests against Windows.

### M4: Integrate and qualify (depends: M1, M2, M3)

- [ ] M4-T01: Run the compatibility matrix, agnostic smoke, Windows conformance, SyncTest, and local-peer gates in CI.
- [ ] M4-T02: Verify header-only GCC/Clang consumers without claiming a Unix SDK port.
- [ ] M4-T03: Update Build, Portability, API, and Testing architecture pages.
- [ ] M4-T04: Publish complete evidence and the contract for Linux/Apple follow-ups.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
| --- | --- | --- | --- | --- |

