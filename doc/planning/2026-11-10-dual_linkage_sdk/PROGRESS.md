# PROGRESS — Dual-linkage SDK and CI consolidation

Status: In progress

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze baseline

- [ ] M0-T01: Record v0.3.0 SDK assets, package names, and installed layouts.
- [ ] M0-T02: Record current CI lanes, Actions minutes, and critical path per workflow.
- [ ] M0-T03: Confirm the frozen ABI, export, and wire-layout gates pass on master.

### M1: Build both flavors (depends: M0)

- [x] M1-T01: Add `GGPO_BUILD_SHARED` / `GGPO_BUILD_STATIC`; keep
  `BUILD_SHARED_LIBS` single-flavor behavior.
- [x] M1-T02: Build `GGPO_shared` and `GGPO_static` with distinct Windows names
  (`GGPO.lib` import library, `libGGPO.lib` static).
- [x] M1-T03: Keep in-tree `GGPO::GGPO`, tests, and Vector War working.

### M2: CMake package selection (depends: M1)

- [x] M2-T01: Install separate shared/static exports.
- [x] M2-T02: Generate `GGPO::GGPO` from `GGPO_USE_STATIC`, with `COMPONENTS` support.
- [x] M2-T03: Keep `GGPO_SHARED_LIB` on Windows shared consumers only.

### M3: Installed-consumer tests (depends: M2)

Parallel with M4.

- [x] M3-T01: Default, static, and explicit-shared cases for C11 and C++98.
- [x] M3-T02: Assert runtime dependencies (`dumpbin /dependents`, `readelf -d`).
- [ ] M3-T03: Run on Windows x64/x86 and Linux gcc/clang.

### M4: Packaging and release (depends: M2)

Parallel with M3.

- [ ] M4-T01: Single Windows zip per architecture.
- [x] M4-T02: Single Linux tar.gz, DEB, and RPM with `Provides`/`Replaces`.
- [ ] M4-T03: Release workflow reduced to 1 Linux + 2 Windows SDK jobs.
- [ ] M4-T04: Validate on an `-rc` tag: asset list, checksums, extracted consumers.

### M5: CI consolidation (depends: M3)

- [ ] M5-T01: Windows lanes reduced to x64 Release, Win32 Release, x64 Debug (all both libraries).
- [ ] M5-T02: Linux lanes reduced to gcc and clang, each testing both flavors.
- [ ] M5-T03: Add the scheduled/manual MSVC AddressSanitizer lane.
- [ ] M5-T04: Frozen Windows baseline reads the dual build unchanged.
- [ ] M5-T05: Record before/after Actions minutes and critical path.

### M6: Documentation and release (depends: M4, M5)

- [ ] M6-T01: Developer Guide: dynamic and static linking on Windows and
  Linux, with and without CMake, plus runtime deployment.
- [ ] M6-T02: Migration table from 0.3.0 package names to 0.4.0.
- [ ] M6-T03: Update Build.md and Testing.md.
- [ ] M6-T04: 0.4.0 release notes.

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| M1-T01–T03 | Native Linux build: both flavors, `BUILD_SHARED_LIBS=ON`, `=OFF` | 29/29, 27/27, 26/26 CTest; single-flavor installs contain only that flavor |
| M2-T01–T03 | [`GGPOConfig.cmake.in`](../../../src/cmake/GGPOConfig.cmake.in); negative configures | Static request against a shared-only install and a wrong expected linkage both fail at configure time |
| M3-T01–T02 | [`verify_consumers.py`](../../../tests/consumer/verify_consumers.py) on the dual install | default→shared, `GGPO_USE_STATIC=ON`→static, `GGPO::shared`→shared; `readelf` confirms `libGGPO.so` presence/absence |
| M4-T02 | `build_in_container.py --library both --packages` (gcc) | 25/25 CTest; tar.gz, DEB, RPM each pass all three consumer cases; DEB `ggpo-sdk` conflicts/replaces/provides the 0.3.0 names |
| Regression | Container `--library static --packages`, `--library shared`, clang `--library static --sanitizers` | 22/22, 23/23, 22/22; per-flavor packages unchanged; consumers clean under ASan+UBSan |

## Decisions

| Decision | Status |
|---|---|
| `GGPO::GGPO` defaults to shared when both are installed | Proposed |
| Windows static library named `libGGPO.lib` | Accepted (2026-11-10) |
| Keep exactly one Debug lane (x64, both libraries) | Accepted |
| MSVC ASan runs on a schedule, not on every PR | Proposed |
| Compile each flavor separately instead of a shared object library | Accepted (M1) |
| `--sanitizers` requires `--library static`; an instrumented `libGGPO.so` needs the C++ sanitizer runtime in every C consumer | Accepted (M3) |
