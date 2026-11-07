# PROGRESS — Linux SDK support

Status: Native Linux implementation complete; cross-OS qualification follows after merge

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze compatibility and evidence contracts

Architecture: [platform and protocol boundaries](../../architecture/Portability.md).

- [x] M0-T01: Record the v0.1.0 public-header hash, Windows baselines, adapter semantics, Linux package ABI ceilings, and shared-image contract.
- [x] M0-T02: Define deterministic peer inputs, checkpoint/event evidence, repetition count, and pass/fail rules.
- [x] M0-T03: Move controlled Windows↔Ubuntu/SteamOS qualification to the post-merge M4 follow-up.

### M1: Implement the Linux platform layer (depends: M0)

Parallel with M2; owns Linux adapter and platform CMake files; integrates at M3.

- [x] M1-T01: Replace the incomplete Linux adapter with the existing `platform.h` semantics.
- [x] M1-T02: Add Linux CMake selection, libraries, visibility, and shared-library versioning without changing `ggponet.h`.
- [x] M1-T03: Add the repository Ubuntu 24.04 Dockerfile and local container wrapper for GCC/Clang and static/shared builds.

### M2: Build the interoperability harness (depends: M0)

Parallel with M1; owns peer-runner scripts and evidence format; integrates at M4.

- [x] M2-T01: Allow the peer runner to launch two distinct executable identities.
- [x] M2-T02: Add controlled two-host coordination and machine-readable checkpoint/event output.
- [x] M2-T03: Prove the harness detects mismatched state, timeout, startup, and shutdown failures.

### M3: Qualify native Linux and packages (depends: M1)

- [x] M3-T01: Refactor CTest registration into shared, Windows-only, and Linux-specific families.
- [x] M3-T02: Make Linux CI build and use the repository Dockerfile, then pass GCC/Clang × static/shared rollback, fake-adapter, and Linux loopback tests inside it.
- [x] M3-T03: Generate and extract-test shared/static DEB, RPM, and raw packages; audit consumers, exports, SONAME, dependencies, and ABI symbol ceilings.

### M4: Follow-up cross-OS qualification (depends: M2, M3)

- [ ] M4-T01: Pass native Windows↔Ubuntu peer sessions with complete evidence.
- [ ] M4-T02: Pass native Windows↔SteamOS peer sessions with complete evidence.
- [x] M4-T03: Enable release-tag Linux archives and update install, testing, and portability documentation.

## Evidence ledger

Completed tasks require artifact identity, command or method, environment/input, and result.

| Task | Commit / artifact | Command / method | Environment / input | Result |
|---|---|---|---|---|
| M0-T01 | `cf821cd`; `ggponet.h` | Compare v0.1.0 and candidate SHA-256; inspect adapter/package contracts | Current branch and v0.1.0 | Header hashes match: `ea5e8681…e471d`; local shared audit requires GLIBC 2.17, below 2.39 ceiling |
| M0-T02 | `TECH_PLAN.md` | Review verification table and 180-frame smoke contract | Planning artifacts | Inputs, repetitions, checkpoints, events, checksum, lifecycle, and unavailable-evidence behavior defined |
| M0-T03 | PR #3 planning trio | Reclassify controlled native cross-OS execution | Merge scope versus qualification scope | Windows↔Ubuntu/SteamOS sessions remain explicit M4 follow-up and do not block native Linux SDK packaging |
| M1-T01 | `cf821cd` | Native configure/build and CTest | Linux; GCC shared/static and Clang shared | All four registered tests pass in each exercised lane |
| M1-T02 | `cf821cd` | Install shared SDK; `audit_linux_sdk.py` | Linux GCC shared | SONAME `libGGPO.so.0`; 16 `ggpo_*` exports; public header unchanged |
| M1-T03 | `cf821cd` | Python syntax check and source inspection | `docker/linux/Dockerfile`, local wrapper | Shared image recipe and local GCC/Clang static/shared interface added; full image matrix remains M3 |
| M2-T01 | [`linux-cross-binary-smoke.json`](evidence/linux-cross-binary-smoke.json) | `run_peer_smoke.py` with GCC peer A and Clang peer B | Linux loopback, 180 frames | Pass; both peers reached frame 180 and checksum `7db6c67b` |
| M2-T02 | `cf821cd`; Testing architecture | Remote-IP peer arguments plus result comparator | Controlled-host procedure | Native-host command/evidence contract implemented; execution remains M4 |
| M2-T03 | [`peer_runner_negative.py`](../../../tests/smoke/peer_runner_negative.py) | `ctest -R linux_peer_negative_contracts` | Linux shared build; real peer fixture with controlled faults | Pass; startup, timeout, checksum mismatch, and shutdown failures each produced the required rejection |
| M3-T01 | `cf821cd` | Configure, build and CTest | Linux native builds | Linux SyncTest, peer loopback, fake SyncTest, and fake contract registered and passing |
| M3-T02 | [Linux compatibility run 42398786860](https://github.com/irregularthanis/ggpo-next/actions/runs/42398786860) | Repository Dockerfile; GCC/Clang × shared/static matrix | GitHub Actions, Ubuntu 24.04 | Pass; all four container lanes completed their registered tests and installed-SDK checks |
| M3-T03 | Local shared/static package outputs | `build_in_container.py --compiler gcc --library <shared|static> --packages`; extract and test every format | Ubuntu 24.04 repository image; DEB, RPM, and raw tar.gz | Pass; all six artifacts built C11/C++98 consumers and passed ABI audit; shared maximum requirement is GLIBC 2.38 against the 2.39 ceiling |
| M4-T03 | `v0.2.0`; `release-packages.yml`; install/architecture docs | Push matching release tag and inspect workflow dispatch | GitHub Actions release packaging | Release-tag packaging and documentation enabled; artifact publication is tracked by the tag workflow |
