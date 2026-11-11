# PROGRESS — MinGW-w64 SDK support

Status: Planning

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

Depends on [PR #9](https://github.com/irregularthanis/ggpo-next/pull/9)
(dual-linkage SDK).

## Milestones

### M0: Baseline

- [ ] M0-T01: Configure with MSYS2 UCRT64 and MINGW32; record every failure.
- [ ] M0-T02: List every `WIN32` branch that assumes MSVC.

### M1: Build (depends: M0)

- [ ] M1-T01: Split Windows flags and system libraries by `MSVC`.
- [ ] M1-T02: MinGW library names (`libGGPO.dll`, `libGGPO.dll.a`, `libGGPO.a`).
- [ ] M1-T03: Replace `_snprintf_s` with portable formatting.
- [ ] M1-T04: Undecorated x86 exports; decide on static libgcc/libstdc++.
- [ ] M1-T05: Disable Vector War under MinGW with a clear message.
- [ ] M1-T06: MSVC and Linux CI unchanged.

### M2: Tests and CI (depends: M1)

- [ ] M2-T01: `mingw-compatibility.yml` for x64 (UCRT64) and x86 (MINGW32).
- [ ] M2-T02: Full CTest suite passes on both architectures.
- [ ] M2-T03: `mingw_exports` manifest test on both architectures.

### M3: Consumers (depends: M1)

Parallel with M2.

- [ ] M3-T01: `verify_consumers.py` MinGW runtime checks.
- [ ] M3-T02: Default, static, and shared cases on both architectures.
- [ ] M3-T03: MinGW program against the MSVC `GGPO.dll`.

### M4: Interoperability (depends: M2)

- [ ] M4-T01: MinGW x64 peer build in the E2E workflow.
- [ ] M4-T02: MinGW↔MSVC and MinGW↔Linux scenarios converge.

### M5: Release and docs (depends: M3, M4)

- [ ] M5-T01: MinGW x64/x86 SDK release jobs; dry run validates both zips.
- [ ] M5-T02: Developer Guide: install, build, link, deploy, compatibility table.
- [ ] M5-T03: Build.md and Testing.md.
- [ ] M5-T04: Add `MinGW x64` and `MinGW x86` to the ruleset's required checks.

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| — | — | — |

## Decisions

| Decision | Status |
|---|---|
| Support x64 and x86 | Accepted (2026-11-11) |
| No MinGW Vector War | Accepted (2026-11-11) |
| x64 uses UCRT (UCRT64); x86 uses MSVCRT (MINGW32) | Proposed |
| Statically link libgcc/libstdc++ into `libGGPO.dll` | Proposed |
| Release-only MinGW lanes, no Debug lane | Proposed |
