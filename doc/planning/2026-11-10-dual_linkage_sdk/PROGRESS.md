# PROGRESS — Dual-linkage SDK and CI consolidation

Status: Planning

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze baseline

- [ ] M0-T01: Record v0.3.0 SDK assets, package names, and installed layouts.
- [ ] M0-T02: Record current CI lanes, Actions minutes, and critical path per workflow.
- [ ] M0-T03: Confirm the frozen ABI, export, and wire-layout gates pass on master.

### M1: Build both flavors (depends: M0)

- [ ] M1-T01: Add `GGPO_BUILD_SHARED` / `GGPO_BUILD_STATIC`; keep
  `BUILD_SHARED_LIBS` single-flavor behavior.
- [ ] M1-T02: Build `GGPO_shared` and `GGPO_static` with distinct Windows names.
- [ ] M1-T03: Keep in-tree `GGPO::GGPO`, tests, and Vector War working.

### M2: CMake package selection (depends: M1)

- [ ] M2-T01: Install separate shared/static exports.
- [ ] M2-T02: Generate `GGPO::GGPO` from `GGPO_USE_STATIC`, with `COMPONENTS` support.
- [ ] M2-T03: Keep `GGPO_SHARED_LIB` on Windows shared consumers only.

### M3: Installed-consumer tests (depends: M2)

Parallel with M4.

- [ ] M3-T01: Default, static, and explicit-shared cases for C11 and C++98.
- [ ] M3-T02: Assert runtime dependencies (`dumpbin /dependents`, `readelf -d`).
- [ ] M3-T03: Run on Windows x64/x86 and Linux gcc/clang.

### M4: Packaging and release (depends: M2)

Parallel with M3.

- [ ] M4-T01: Single Windows zip per architecture.
- [ ] M4-T02: Single Linux tar.gz, DEB, and RPM with `Provides`/`Replaces`.
- [ ] M4-T03: Release workflow reduced to 1 Linux + 2 Windows SDK jobs.
- [ ] M4-T04: Validate on an `-rc` tag: asset list, checksums, extracted consumers.

### M5: CI consolidation (depends: M3)

- [ ] M5-T01: Windows lanes reduced to x64 Release, Win32 Release, x64 static Debug.
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
| — | — | — |

## Decisions

| Decision | Status |
|---|---|
| `GGPO::GGPO` defaults to shared when both are installed | Proposed |
| Windows static library named `GGPO_static.lib` | Proposed |
| Keep exactly one Debug lane (x64 static) | Proposed |
| MSVC ASan runs on a schedule, not on every PR | Proposed |
