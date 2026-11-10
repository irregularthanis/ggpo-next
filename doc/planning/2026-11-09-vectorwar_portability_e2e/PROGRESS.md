# PROGRESS — Portable Vector War and application E2E

Status: M0 and M3 implementation complete; M1/M4 await Windows CI evidence

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze sample behavior and host contracts

Architecture: [Vector War](../../architecture/VectorWar.md).

- [x] M0-T01: Record the current Windows CLI, controls, player/spectator modes,
  diagnostics, and shutdown behavior.
- [x] M0-T02: Add a controlled scripted-input baseline with fixed arena bounds,
  canonical encoding, checkpoints, and final digest.
- [x] M0-T03: Freeze the portable app/host/renderer/result contracts and record
  attributable material selected from upstream PR #57.

### M1: Extract the portable application core (depends: M0)

Owns game state, session integration, parser, and host interfaces.

- [x] M1-T01: Remove Win32 types and services from rollback state and shared app logic.
- [x] M1-T02: Route input, timing, rendering, diagnostics, and lifecycle through
  the frozen host contracts without changing the public GGPO API.
- [ ] M1-T03: Pass the Windows baseline and focused portable application contracts.

### M2: Build the interactive frontend on Windows and Linux (depends: M1)

Parallel with M3; owns SDL/frontend files and dependency provisioning.

- [ ] M2-T01: Implement the shared SDL2 window, input, renderer, status, and
  performance monitor.
- [ ] M2-T02: Build the interactive target in Windows x64/x86 and Linux GCC/Clang lanes.
- [ ] M2-T03: Run native SDL lifecycle smoke and document launch/topology commands.

### M3: Add application-level automated sessions (depends: M1)

Parallel with M2; owns headless host, scripted inputs, result schema, and local tests.

- [x] M3-T01: Implement the headless host using the same session/frame/callback path.
- [x] M3-T02: Validate canonical results against golden checkpoints and reject
  identity, timeout, divergence, missing rollback, and dirty shutdown.
- [ ] M3-T03: Pass two-process application loopback on Windows and Linux.

### M4: Qualify native application interoperability (depends: M2, M3, merged PR #5)

- [x] M4-T01: Add the Vector War fixture to the authenticated native E2E matrix.
- [ ] M4-T02: Pass LL, WW, LW, and WL from one source SHA with complete evidence.
- [ ] M4-T03: Update build, Vector War, testing, and portability documentation and
  record exact workflow artifacts.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
| --- | --- | --- | --- | --- |
| M0-T01 | [`53b2b3e`](https://github.com/irregularthanis/ggpo-next/commit/53b2b3e6d2587d25b519234519ea93e5ca24b528), [Vector War architecture](../../architecture/VectorWar.md) | Source and feature-boundary audit | Existing Win32 sample at `5184cbf` | Current CLI, controls, modes, diagnostics, state, and lifecycle recorded |
| M0-T03 | [Technical plan contracts](TECH_PLAN.md#contracts-and-flow), [upstream PR #57](https://github.com/pond3r/ggpo/pull/57) | Host-boundary and attribution review | C++98 app; Windows/Linux targets | Portable host/result boundary frozen; SDL2 app-only reuse scope recorded |
| M0-T02 | [`53b2b3e`](https://github.com/irregularthanis/ggpo-next/commit/53b2b3e6d2587d25b519234519ea93e5ca24b528), [Windows CI](https://github.com/irregularthanis/ggpo-next/actions/runs/42745724187) | `vectorwar_state_contract`; seven canonical checkpoints over 180 scripted frames | Linux GCC; Windows x64/x86, shared/static, Debug/Release | PASS; all variants produced the same checkpoints and final digest |
| M1-T01, M1-T02 | [`4876b2a`](https://github.com/irregularthanis/ggpo-next/commit/4876b2a) | Build `VectorWarCore`; run the full native Linux suite | Linux GCC Release, static GGPO | Portable core compiled; 14/14 tests passed; Windows verification pending CI |
| M3-T01 | `VectorWarHeadless`, [`run_headless_loopback.py`](../../../tests/vectorwar/run_headless_loopback.py) | Three independent two-process runs, 180 frames each | Linux GCC Release, static GGPO, native UDP loopback | PASS; checksum `b3381958` in every peer/run; forced rollback observed |
| M3-T02 (partial) | [`headless_negative.py`](../../../tests/vectorwar/headless_negative.py) | Inject startup, timeout, mismatch, no-rollback, and shutdown faults | Linux native loopback | PASS; all five failure families rejected; cross-host identity schema remains for M4 |
| M1-T03 (two-layer coverage) | [`gameplay_determinism_test.cpp`](../../../tests/vectorwar/gameplay_determinism_test.cpp), [`native_host_contract_test.cpp`](../../../tests/vectorwar/native_host_contract_test.cpp), [`win32_host_contract_test.cpp`](../../../tests/vectorwar/win32_host_contract_test.cpp) | Shared gameplay assertions plus selected native-host contracts | Linux locally; Windows/Linux CI | Linux PASS; Windows verification pending CI |
| M3-T02 | [`result_schema.py`](../../../tests/e2e/result_schema.py), [`test_result_schema.py`](../../../tests/e2e/test_result_schema.py) | Validate the frozen Vector War oracle plus identity/lifecycle negative cases | Python 3; schema-2 synthetic reports | PASS; wrong fixture checksum and unsupported frame count rejected |
| M4-T01 | [Native E2E workflow](../../../.github/workflows/cross-platform-e2e.yml), [`validate_manifest.py`](../../../tests/e2e/validate_manifest.py) | Add Vector War to the existing authenticated LL/WW/LW/WL matrix | Same source artifacts and control plane as generic peer smoke | Implemented; 8 fixture/scenario reports required; hosted CI evidence pending |
