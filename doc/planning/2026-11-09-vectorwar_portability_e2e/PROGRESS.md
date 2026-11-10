# PROGRESS — Portable Vector War and application E2E

Status: Planned

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Freeze sample behavior and host contracts

Architecture: [Vector War](../../architecture/VectorWar.md).

- [ ] M0-T01: Record the current Windows CLI, controls, player/spectator modes,
  diagnostics, and shutdown behavior.
- [ ] M0-T02: Add a controlled scripted-input baseline with fixed arena bounds,
  canonical encoding, checkpoints, and final digest.
- [ ] M0-T03: Freeze the portable app/host/renderer/result contracts and record
  attributable material selected from upstream PR #57.

### M1: Extract the portable application core (depends: M0)

Owns game state, session integration, parser, and host interfaces.

- [ ] M1-T01: Remove Win32 types and services from rollback state and shared app logic.
- [ ] M1-T02: Route input, timing, rendering, diagnostics, and lifecycle through
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

- [ ] M3-T01: Implement the headless host using the same session/frame/callback path.
- [ ] M3-T02: Validate canonical results against golden checkpoints and reject
  identity, timeout, divergence, missing rollback, and dirty shutdown.
- [ ] M3-T03: Pass two-process application loopback on Windows and Linux.

### M4: Qualify native application interoperability (depends: M2, M3, merged PR #5)

- [ ] M4-T01: Add the Vector War fixture to the authenticated native E2E matrix.
- [ ] M4-T02: Pass LL, WW, LW, and WL from one source SHA with complete evidence.
- [ ] M4-T03: Update build, Vector War, testing, and portability documentation and
  record exact workflow artifacts.

## Evidence ledger

| Task | Commit / artifact | Command / method | Environment / input | Result |
| --- | --- | --- | --- | --- |
