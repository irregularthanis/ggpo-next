# PRD — Disconnect before any input

Status: Draft

Source: `e5a8b79` (`master`)

Scope: a peer that is disconnected before any of its input was received must
produce `GGPO_EVENTCODE_DISCONNECTED_FROM_PEER` and let the session go on,
instead of stopping the process in an `ASSERT`. Covers every path that
disconnects a remote queue: the disconnect timeout, a remote disconnect
notice, and `ggpo_disconnect_player`. Excluded: any public API, ABI, export or
wire format change; changes to prediction or rollback outside this case.

[Technical plan](TECH_PLAN.md) · [Progress](PROGRESS.md)

## Motivation

Frame Perfect's ROM-free netcode suite found that its vendored GGPO crashes
when the opponent completes the sync handshake but disappears before sending
its first input. When the disconnect timeout fires, the P2P backend
disconnects that queue at its last received frame, which is still `-1`, and
asks `Sync` to roll back to frame `-1`. No state is saved for frame `-1`, so
`Sync::FindSavedFrameIndex` reaches `ASSERT(false)` and the process exits.

ggpo-next has the same code in `Peer2PeerBackend::DisconnectPlayerQueue`,
`Sync::AdjustSimulation` and `Sync::FindSavedFrameIndex`. A player whose
opponent quits, crashes or loses the network in the window between sync and
the first input loses the game process instead of seeing a disconnect.
Packet loss widens the window, because it delays the first input.

## Outcomes

- Disconnecting a remote queue that has no received input re-simulates from
  frame 0 with that player's inputs marked disconnected, raises
  `GGPO_EVENTCODE_DISCONNECTED_FROM_PEER` once, and keeps the session running.
- The local player's game continues and its checksums match a run where the
  disconnected player was never there (all-zero, disconnected inputs).
- The same holds for two-player and N-player sessions and for
  `ggpo_disconnect_player` called before any input arrived.
- A regression test reproduces the crash on the current code and passes after
  the fix, on every CI platform.

## Success and constraints

| Contract | Baseline | Target | Validation |
| --- | --- | --- | --- |
| No-input disconnect | `ASSERT` in `FindSavedFrameIndex`, process exits | Disconnect event, session continues | Peer negative contract on every native lane |
| Determinism | — | Same checksums as an all-disconnected-inputs reference | Unit contract on `Sync` |
| Other disconnects | Pass | Unchanged | Existing negative contracts and E2E |
| Public surface | — | No API, ABI, export or wire change | ABI and export checks |
| Existing tests | — | Kept; new cases added, none rewritten | Diff review |

## Decisions

- D1 (open): where the frame `-1` target becomes frame 0. Recommended: in
  `Peer2PeerBackend::DisconnectPlayerQueue`, the one place that turns a
  disconnect into a rollback target (see the technical plan).

## Downstream

Frame Perfect keeps its vendored GGPO unchanged and pins the crash with
`session_no_input_disconnect --known-crash`, which fails once the fixed
ggpo-next is in use, so the expectation is removed during its migration.
