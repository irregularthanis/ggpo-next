# PRD — Native cross-platform end-to-end CI

Status: Clean native matrix qualified; impairment testing remains follow-up work

Source: inspected `3ae20afbda22f245d15ac28d67f9bd9d732100ab`; supplied Option B handoff identified `8a0b40fcbd0145837cbef7e40206eaad7c6397ff`

Qualified implementation: `9cc921b57b994d7fc5f77eda7c63a5340af709fc`

Scope: native Windows x64 and Linux x86-64 GGPO interoperability on GitHub-hosted runners; excluded: public API/ABI changes, production protocol changes, Vector War, performance benchmarking, permanent infrastructure, and self-hosted runners

## Motivation

GGPO Next now builds and tests native Windows and Linux SDKs, but each transport smoke test runs two peers on one operating system. Developers need repeatable evidence that the same production GGPO protocol, rollback callbacks, and deterministic simulation converge across native Windows and Linux—not Wine or a synthetic replacement protocol.

The selected Option B uses temporary Tailscale nodes and GitHub Actions coordination. It must leave no permanent host, public inbound port, or manually operated service.

## Outcomes

- Build one native Release x64 peer fixture per platform from the same commit.
- Run Linux→Linux, Windows→Windows, Linux→Windows, and Windows→Linux player-role assignments over real UDP.
- Validate canonical state against both the other peer and an independent expected-state calculation.
- Reject timeout, crash, missing/duplicate result, divergent state, wrong run identity, unexpected disconnect, and incomplete cleanup.
- Upload a manifest, peer results, logs, hashes, environment identity, and a concise Actions summary.
- Run the clean matrix on trusted owner-authored same-repository PRs, `master` pushes, and manual dispatch. Add deterministic latency/loss profiles as a nightly tier after clean execution is stable.
- Treat absent credentials or unavailable native evidence as incomplete, never as a pass.

## Success and constraints

| Metric | Baseline | Target | Validation |
|---|---|---|---|
| Native combinations | Windows and Linux loopback independently | LL, WW, LW, and WL all pass | Scenario manifest contains exactly four unique completed scenarios |
| Determinism | Equal native-memory checksum | Canonical checksum matches peer and independent oracle | Explicit-width serialization plus known-vector/oracle tests |
| Transport | Localhost only | Cross-platform peers exchange production UDP over Tailscale | Native process logs, endpoints, events, frames, and results |
| Lifecycle | Fixed settling delay | Bounded readiness, execution, completion, and cleanup | Stage deadlines; child/process/node cleanup evidence |
| Isolation | No concurrent cross-host runs | Run ID, attempt, SHA, session ID, and authenticated control messages match | Wrong-session and replay tests fail closed |
| Reliability | Manual procedure | Three consecutive clean workflow runs pass before enabling as required CI | Saved run URLs and artifacts |
| Security | No privileged cross-host workflow | No secrets on fork PRs; least-privileged OAuth/grants and GitHub permissions | Trigger/permission review and sanitized artifact audit |

This suite measures correctness, not latency or throughput. Direct WireGuard and relayed Tailscale paths are both valid.

## Clarifications

- Option B means GitHub-hosted native runners joined through ephemeral Tailscale nodes; no permanent coordinator.
- Privileged triggers are owner-authored same-repository PRs, `push` to `master`, and `workflow_dispatch`. Fork and non-owner PR code never receives Tailscale credentials.
- The official Tailscale Action is pinned to commit `064b10558093f38325e838ddea35486bdc6b1d5c` (v4.4.0). Its documented OAuth nodes are ephemeral and removed after logout.
- A run-scoped GitHub Deployment record bootstraps the Linux coordinator address. HMAC-authenticated control messages use a separate repository secret; deployment payloads never carry reusable credentials.
- Clean-network interoperability is the merge gate for this work. Seeded latency/loss and proven rollback execution form the nightly follow-up tier.

## Qualification result

Three consecutive trusted workflow runs completed LL, WW, LW, and WL at 180
frames. Every scenario matched canonical checksum `2109130363`, recorded one
running event, closed cleanly, and reported no unexpected disconnect. The
[progress ledger](PROGRESS.md#evidence-ledger) links the retained run evidence.
