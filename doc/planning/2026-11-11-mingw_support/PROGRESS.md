# PROGRESS — MinGW-w64 SDK support

Status: In progress

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

Depends on [PR #9](https://github.com/irregularthanis/ggpo-next/pull/9)
(dual-linkage SDK).

## Milestones

### M0: Baseline

- [x] M0-T01: Configure with MSYS2 UCRT64 and MINGW32; record every failure.
- [x] M0-T02: List every `WIN32` branch that assumes MSVC.

### M1: Build (depends: M0)

- [x] M1-T01: Split Windows flags and system libraries by `MSVC`.
- [x] M1-T02: MinGW library names (`libGGPO.dll`, `libGGPO.dll.a`, `libGGPO.a`).
- [x] M1-T03: Replace `_snprintf_s` with portable formatting.
- [x] M1-T04: Undecorated x86 exports; link libgcc/libstdc++ statically and
  verify `libGGPO.dll` imports no MinGW runtime DLLs.
- [x] M1-T05: Disable Vector War under MinGW with a clear message.
- [x] M1-T06: Record the toolchain in `GGPOConfig.cmake`; reject a mismatched
  consumer toolchain on Windows.
- [x] M1-T07: MSVC and Linux CI unchanged.

### M2: Tests and CI (depends: M1)

- [x] M2-T01: `mingw-compatibility.yml` for x64 (UCRT64) and x86 (MINGW32).
- [x] M2-T02: Full CTest suite passes on both architectures.
- [x] M2-T03: `mingw_exports` manifest test on both architectures.

### M3: Consumers (depends: M1)

Parallel with M2.

- [x] M3-T01: `verify_consumers.py` MinGW runtime checks.
- [x] M3-T02: Default, static, and shared cases on both architectures.
- [x] M3-T03: MinGW program against the MSVC `GGPO.dll`.
- [x] M3-T04: Negative tests: MinGW consumer vs MSVC SDK and MSVC consumer vs
  MinGW SDK fail at configure time.

### M4: Interoperability (depends: M2)

- [ ] M4-T01: MinGW x64 peer build in the E2E workflow.
- [ ] M4-T02: MinGW↔MSVC and MinGW↔Linux scenarios converge.

### M5: Release and docs (depends: M3, M4)

- [ ] M5-T01: MinGW x64/x86 SDK release jobs; dry run validates both zips,
  including no `*.lib` in MinGW zips and no `*.a` in MSVC zips.
- [ ] M5-T02: Developer Guide: install, build, link, deploy, compatibility
  table, and separate install folders per toolchain.
- [ ] M5-T03: Build.md and Testing.md.
- [ ] M5-T04: Add `MinGW x64` and `MinGW x86` to the ruleset's required checks.

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| M0-T01 | Unmodified tree, MinGW-w64 GCC 13 cross build | Configure passes; build fails on `/W4 /WX` for every target and on Vector War's Win32 frontend (`nullptr`, `min`/`max`, resources) |
| M0-T02 | `WIN32` audit | MSVC-only assumptions in Windows `Platform.cmake` (flags, `.lib` names), static `OUTPUT_NAME` in `src/CMakeLists.txt`, Win32 GUI host test, Vector War |
| M1, M2 | [MinGW run 42957910362](https://github.com/irregularthanis/ggpo-next/actions/runs/42957910362) (`62d05c6`) | x64 UCRT64 and x86 MINGW32: 24/24 CTest each; `libGGPO.dll`, `libGGPO.dll.a`, `libGGPO.a` installed, no `.lib`; 16 undecorated exports equal the frozen MSVC manifests; DLL imports only KERNEL32, msvcrt/UCRT, USER32, WINMM, WS2_32 |
| M3-T02 | Same run | Default, `GGPO_USE_STATIC=ON` and `GGPO::shared` consumers build and run on both architectures; static consumers run without the SDK `bin` on `PATH` |
| x86 FP | [MinGW run at `8a8e0b6`](https://github.com/irregularthanis/ggpo-next/actions/runs/42956989680) vs `3db4014` | x87 default diverged `vectorwar_state_contract` at frame 30 (2768577444 vs 3703441302); `-msse2 -mfpmath=sse` restores the frozen checksum |
| M1-T07 | MSVC Windows, Linux and E2E runs at `8a8e0b6` and `3db4014` | All pass, including the frozen Windows baseline |
| M1-T06, M3-T03, M3-T04 | [MinGW run 42958727700](https://github.com/irregularthanis/ggpo-next/actions/runs/42958727700) (`eb7c24c`), job "MinGW toolchain isolation" | MinGW project rejects the MSVC SDK and MSVC project rejects the MinGW SDK, both with the toolchain message; MinGW C program linked directly to the MSVC `GGPO.dll` imports it and runs |

## Decisions

| Decision | Status |
|---|---|
| Support x64 and x86 | Accepted (2026-11-11) |
| No MinGW Vector War | Accepted (2026-11-11) |
| x64 uses UCRT (UCRT64); x86 uses MSVCRT (MINGW32) | Proposed |
| Statically link libgcc/libstdc++ into `libGGPO.dll` | Accepted (2026-11-11) |
| Release-only MinGW lanes, no Debug lane | Accepted (implemented) |
| 32-bit MinGW compiles with `-msse2 -mfpmath=sse` (x87 breaks cross-platform determinism) | Accepted (2026-11-11) |
| Separate prefixes per toolchain; `GGPOConfig.cmake` rejects a mismatched consumer toolchain | Accepted (2026-11-11) |
