# PROGRESS — Linux SDK support

Status: Planned

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze compatibility and evidence contracts

Architecture: [platform and protocol boundaries](../../architecture/Portability.md).

- [ ] M0-T01: Record the v0.1.0 public-header hash, Windows baselines, adapter semantics, Linux package ABI ceilings, and shared-image contract.
- [ ] M0-T02: Define deterministic peer inputs, checkpoint/event evidence, repetition count, and pass/fail rules.
- [ ] M0-T03: Identify controlled Windows, Ubuntu 24.04, and SteamOS hosts for required qualification.

### M1: Implement the Linux platform layer (depends: M0)

Parallel with M2; owns Linux adapter and platform CMake files; integrates at M3.

- [ ] M1-T01: Replace the incomplete Linux adapter with the existing `platform.h` semantics.
- [ ] M1-T02: Add Linux CMake selection, libraries, visibility, and shared-library versioning without changing `ggponet.h`.
- [ ] M1-T03: Add the repository Ubuntu 24.04 Dockerfile and local container wrapper for GCC/Clang and static/shared builds.

### M2: Build the interoperability harness (depends: M0)

Parallel with M1; owns peer-runner scripts and evidence format; integrates at M4.

- [ ] M2-T01: Allow the peer runner to launch two distinct executable identities.
- [ ] M2-T02: Add controlled two-host coordination and machine-readable checkpoint/event output.
- [ ] M2-T03: Prove the harness detects mismatched state, timeout, startup, and shutdown failures.

### M3: Qualify native Linux and packages (depends: M1)

- [ ] M3-T01: Refactor CTest registration into shared, Windows-only, and Linux-specific families.
- [ ] M3-T02: Make Linux CI build and use the repository Dockerfile, then pass GCC/Clang × static/shared rollback, fake-adapter, and Linux loopback tests inside it.
- [ ] M3-T03: Audit extracted packages, consumers, exports, SONAME, dependencies, and ABI symbol ceilings.

### M4: Qualify cross-OS support and release (depends: M2, M3)

- [ ] M4-T01: Pass native Windows↔Ubuntu peer sessions with complete evidence.
- [ ] M4-T02: Pass native Windows↔SteamOS peer sessions with complete evidence.
- [ ] M4-T03: Enable release-tag Linux archives and update install, testing, and portability documentation.

## Evidence ledger

No implementation tasks are complete yet. Completed tasks require artifact identity, command or method, environment/input, and result.

| Task | Commit / artifact | Command / method | Environment / input | Result |
|---|---|---|---|---|
