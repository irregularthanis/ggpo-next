# PROGRESS — Internet play for Vector War

Status: Not started

[Product brief](PRD.md) · [Technical plan](TECH_PLAN.md)

## Milestones

### M0: Decisions and feasibility (spikes run in CI or a scratch account)

- [ ] M0-T01: Confirm the current always-free terms for D1 (VM shape,
  outbound allowance, region availability) and record them here.
- [ ] M0-T02: Decide D2 (DNS) and reserve the name.
- [ ] M0-T03: NAT mapping spike in Linux network namespaces: a mapping created
  by one socket survives closing it and GGPO binding the same port, for
  full-cone and port-restricted NAT; record the time window.
- [ ] M0-T04: Confirm `terraform test` mocks cover every resource type in the
  chosen providers.

### M1: Protocol and server (depends: M0)

- [ ] M1-T01: Protocol header, Python mirror, and shared test vectors.
- [ ] M1-T02: `asyncio` server: pairing, expiry, rate limits, no amplification.
- [ ] M1-T03: Server unit tests registered in CTest.
- [ ] M1-T04: Container image (non-root, read-only) with a build smoke test.

### M2: Infrastructure (depends: M0; parallel with M1)

- [ ] M2-T01: `host` module: VM, firewall (UDP server port public, SSH from
  `admin_cidr`), cloud-init running the image by digest.
- [ ] M2-T02: `dns` module: DNS-only A record.
- [ ] M2-T03: `envs/prod` with remote state in free object storage and a
  committed provider lock file.
- [ ] M2-T04: `terraform test` plan assertions (firewall, VM shape, image
  digest, DNS record).
- [ ] M2-T05: `terraform test` state assertions with mocked apply (outputs
  wired, DNS points at the VM IP from state).
- [ ] M2-T06: Variable validation tests with `expect_failures`.
- [ ] M2-T07: `infrastructure.yml` (fmt, validate, tflint, test, server tests,
  image build) with the **Complete infrastructure checks** aggregate.

### M3: Vector War client (depends: M1)

- [ ] M3-T01: `vw_udp`, `vw_platform_name`.
- [ ] M3-T02: `vw_matchmaking`: resolve, register, match, punch, hand off to
  GGPO on the same port.
- [ ] M3-T03: `vw_hello` for direct mode: platform exchange and nonce-based
  player order.
- [ ] M3-T04: `--matchmaking HOST[:PORT] LOCAL_PORT` on every client and the
  headless fixture; every existing argument form unchanged (tested).
- [ ] M3-T05: Client tests against a fake server on loopback, on every
  platform.

### M4: Start screen and legend on every client (depends: M3)

- [ ] M4-T01: Embedded bitmap font for SDL; shared player-color accessor.
- [ ] M4-T02: Windows start screen (Win32 dialog) with direct and
  Matchmaking.
- [ ] M4-T03: Linux start screen (SDL form) with direct and Matchmaking.
- [ ] M4-T04: Android launcher with direct and Matchmaking.
- [ ] M4-T05: Platform legend in GDI and SDL, color-matched to ships.
- [ ] M4-T06: Host contract tests for start screen mapping and legend on
  Windows, Linux and Android.

### M5: NAT traversal in CI (depends: M3, M1-T04)

- [ ] M5-T01: Namespace topology with full-cone, port-restricted and
  symmetric NAT and the server container.
- [ ] M5-T02: Full-cone and port-restricted pairs reach the frozen Vector War
  checksum; symmetric fails fast with the documented error.
- [ ] M5-T03: Scheduled impairment run with `tc netem`.

### M6: Deploy and trial (depends: M2, M4, M5)

- [ ] M6-T01: `deploy-matchmaking.yml` with a protected environment; first
  apply; record the plan and outputs.
- [ ] M6-T02: Set the compiled default server to the deployed name.
- [ ] M6-T03: Friends trial (checklist below), every platform pair in both
  player orders.
- [ ] M6-T04: Add **Complete infrastructure checks** to the `master` ruleset.

### M7: Documentation (depends: M6)

- [ ] M7-T01: Developer Guide "Play over the Internet".
- [ ] M7-T02: `doc/architecture/Matchmaking.md`.
- [ ] M7-T03: `infra/terraform/README.md`.
- [ ] M7-T04: Testing.md tables and workflow.

## Friends trial checklist

For each session, record:

| Field | Example |
| --- | --- |
| Players' platforms and player order | P1 Windows, P2 Android |
| Each player's connection | Home fiber, home cable, mobile data, carrier-grade NAT |
| Mode and result | Matchmaking: connected / punched / failed with message |
| Round-trip time and rollbacks (performance monitor) | 45 ms, occasional 2-frame rollbacks |
| Desync | None (checksums matched) |
| Notes | Router model, VPN in use, anything odd |

## Evidence ledger

| ID | Evidence | Result |
|---|---|---|
| | | |
