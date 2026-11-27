# TECH_PLAN — Disconnect before any input

Status: Draft

[Product brief](PRD.md) · [Progress](PROGRESS.md)

## Verified baseline

Facts from `e5a8b79`:

- `Peer2PeerBackend` starts every queue with
  `_local_connect_status[i].last_frame = -1` (`p2p.cpp:51`). It only rises when
  an input from that queue is received (`p2p.cpp:299`, `p2p.cpp:383`).
- Three paths call `DisconnectPlayerQueue(queue, syncto)` with a `syncto`
  derived from those values, so `-1` is reachable from each:
  - `DisconnectPlayer` (`p2p.cpp:487`) passes the queue's own `last_frame`.
    The disconnect timeout reaches it through the endpoint's `Disconnected`
    event; this is the path Frame Perfect hits.
  - `Poll2Players` (`p2p.cpp:191`) passes `total_min_confirmed`.
  - `PollNPlayers` (`p2p.cpp:236`) passes `queue_min_confirmed`.
- `DisconnectPlayerQueue` (`p2p.cpp:493`) calls
  `_sync.AdjustSimulation(syncto)` whenever `syncto < framecount`.
- `Sync::AdjustSimulation` (`sync.cpp:144`) calls `LoadFrame(seek_to)`, which
  calls `FindSavedFrameIndex(seek_to)`; for a frame that was never saved,
  `FindSavedFrameIndex` reaches `ASSERT(false)` (`sync.cpp:238`).
- Frame 0 is always saved: `Sync::AddLocalInput` calls `SaveCurrentFrame()`
  when `_framecount == 0` (`sync.cpp:66`).
- With no confirmed input from a peer, the prediction barrier in
  `AddLocalInput` stops the simulation at `_max_prediction_frames` frames, and
  the saved-state ring holds `MAX_PREDICTION_FRAMES + 2` frames (`sync.h:74`),
  so frame 0 is still resident when such a disconnect happens.
- A disconnected player's inputs are zeroed from `last_frame + 1` on, and
  inputs predicted for a player with no received input are zero too, so
  re-simulating from frame 0 changes nothing but is always valid.

## Design

### Rollback target

In `DisconnectPlayerQueue`, store `syncto` in `last_frame` unchanged (it is
the last frame received, `-1` included) but roll back to
`max(syncto, 0)`:

```cpp
const int seek_to = syncto < 0 ? 0 : syncto;
if (seek_to < framecount) {
   _sync.AdjustSimulation(seek_to);
}
```

Alternatives considered:

- Clamp inside `Sync::LoadFrame` or `FindSavedFrameIndex`: hides genuine
  bugs elsewhere that ask for an unsaved frame; the `ASSERT` stays meaningful
  for those.
- Skip the adjustment when `syncto < 0`: correct today only because predicted
  and disconnected inputs are both zero; the rollback keeps the code correct
  if prediction changes.

### Tests

- Unit (`tests/unit/sync_contract_test.cpp`, new cases): a `Sync` with
  frames 0..n simulated on prediction only, then `AdjustSimulation(0)` after
  marking the queue disconnected; checksums equal a reference run with
  disconnected inputs from the start.
- Peer negative contract (`tests/smoke/peer_runner_negative.py`, new case):
  peer B exits as soon as it sees `GGPO_EVENTCODE_RUNNING`, before its first
  `ggpo_add_local_input`. Peer A must report
  `GGPO_EVENTCODE_DISCONNECTED_FROM_PEER` after the disconnect timeout and
  keep advancing. The case is added first and must fail on the current code.
- `ggpo_disconnect_player` on a remote handle before any input: same
  expectations, in the smoke peer.
- Three-player variant for the `PollNPlayers` path.

## Rollout

| Milestone | Depends | Deliverable |
| --- | --- | --- |
| M0 | — | Failing regression tests on `master` (reproduction) |
| M1 | M0 | The fix; tests pass on every native lane |
| M2 | M1 | Release note; Frame Perfect informed through its migration plan |

## Risks

- A queue could be disconnected while the ring no longer holds frame 0 (more
  than `MAX_PREDICTION_FRAMES + 2` frames simulated with no input from it).
  The prediction barrier prevents that today; the unit test asserts the
  barrier holds so a change there is caught.
