# Shared-core testing baseline

Source: `d427210dc4b68f30dc6747de1a912dd6af4a3397`

This inventory freezes the evidence available before implementing the
[shared-core testing plan](../planning/2026-11-07-shared_core_testing_ci/TECH_PLAN.md).
“Called” means an existing smoke test reaches the API; it does not imply that
invalid inputs, lifecycle transitions, cleanup, or backend differences are
covered.

## Public API

| Export | Existing direct test | Status | First missing contract |
|---|---|---|---|
| `ggpo_start_session` | `peer_smoke.cpp` | Called | Invalid sizes/counts and startup cleanup |
| `ggpo_add_player` | Peer and SyncTest smoke | Called | Capacity, duplicate and invalid player cases |
| `ggpo_start_synctest` | `synctest_smoke.cpp` | Called | Invalid frames/sizes and callback failure |
| `ggpo_start_spectating` | None | Missing | Spectator creation and startup failure |
| `ggpo_close_session` | Peer and SyncTest smoke | Called | Every backend and partial initialization |
| `ggpo_set_frame_delay` | None | Missing | Handle, delay and lifecycle boundaries |
| `ggpo_idle` | Peer and SyncTest smoke | Called | Backend and shutdown behavior |
| `ggpo_add_local_input` | Peer and SyncTest smoke | Called | Size, handle and prediction-window errors |
| `ggpo_synchronize_input` | Peer and SyncTest smoke | Called | Buffer size, flags and backend behavior |
| `ggpo_disconnect_player` | None | Missing | Local/remote/invalid and repeated disconnect |
| `ggpo_advance_frame` | Peer and SyncTest smoke | Called | Ordering and invalid lifecycle |
| `ggpo_get_network_stats` | None | Missing | Connected/disconnected/backend applicability |
| `ggpo_set_disconnect_timeout` | None | Missing | Ordering and value boundaries |
| `ggpo_set_disconnect_notify_start` | None | Missing | Ordering and relation to disconnect timeout |
| `ggpo_log` | None | Missing | Null/closed/session applicability |
| `ggpo_logv` | None | Missing | Controlled variadic wrapper contract |

Sources: [`ggponet.h`](../../src/include/ggponet.h),
[`peer_smoke.cpp`](../../tests/smoke/peer_smoke.cpp), and
[`synctest_smoke.cpp`](../../tests/smoke/synctest_smoke.cpp).

## Portable core

| Component | Existing evidence | Status | Priority |
|---|---|---|---|
| `InputQueue` | Indirectly exercised by SyncTest | Indirect | P0: prediction, mismatch, delay and wrap |
| `Sync` | Deterministic SyncTest smoke | Partial | P0: snapshots, repeated rollback and convergence |
| `TimeSync` | Native peer smoke | Indirect | P1: synthetic frame histories |
| `RingBuffer` | Production use | Indirect | P1: empty/full/capacity/wrap |
| `StaticBuffer` | Production use | Indirect | P1: capacity and reuse |
| `BitVector` | Production use | Indirect | P1: bit boundaries and reset |
| UDP message/protocol | Frozen Windows layout | Partial | P0: lengths, types and malformed datagrams |

Sources: [`input_queue.h`](../../src/lib/ggpo/input_queue.h),
[`sync.h`](../../src/lib/ggpo/sync.h),
[`timesync.h`](../../src/lib/ggpo/timesync.h), and
[`udp_msg.h`](../../src/lib/ggpo/network/udp_msg.h).

## Platform boundary

[`platform.h`](../../src/lib/ggpo/platform.h) contains 16 callable operations,
plus `UdpSocket` constructor/destructor lifecycle.

| Group | Operations | Current direct evidence | Priority |
|---|---|---|---|
| Time/process | `MonotonicTimeMs`, `WaitMs`, `ProcessId` | Fake covers time/wait; process is indirect | P0 |
| Configuration | `GetConfigInt`, `GetConfigBool` | Fake covers missing/default only | P1 |
| Diagnostics/files | `AssertFailed`, `EnsureDirectory` | Indirect | P1 |
| Endpoint | `ParseUdpEndpoint`, `SameUdpEndpoint`, `FormatUdpEndpoint` | Fake covers valid parse only | P0 |
| Network lifetime | `NetworkStartup`, `NetworkShutdown` | Fake counts one balanced pair | P0 |
| UDP socket | `OpenAndBind`, `Send`, `Receive`, `Close` | Fake covers basic open/failure/recovery | P0 |

The current fake contract is
[`platform_fake_test.cpp`](../../tests/platform/fake/platform_fake_test.cpp).
Windows and Linux exercise their adapters through separate native loopbacks;
they do not yet run one reusable conformance assertion source.

## Existing test and compatibility gates

| Evidence family | Current target or workflow | Keep during implementation |
|---|---|---|
| Public layout/exports | Windows layout targets and frozen export lists | Yes |
| Packet layout | `windows_packet_layout` | Yes |
| Rollback smoke | Windows/Linux and fake `synctest_smoke` | Yes |
| Native UDP | Windows/Linux two-process peer loopback | Yes |
| Negative runner | Windows/Linux peer negative contracts | Yes |
| Installed consumers | C11 and C++98 shared/static consumers | Yes |
| Linux SDK | GCC/Clang × shared/static build, test, install and audit | Yes |
| Windows SDK | x64/Win32 × shared/static × Debug/Release | Yes |
| Frozen compatibility | Baseline/candidate Windows comparison | Yes |
| Release packages | Release packaging and package consumption | Yes |

Sources: [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt),
[`linux-compatibility.yml`](../../.github/workflows/linux-compatibility.yml), and
[`windows-compatibility.yml`](../../.github/workflows/windows-compatibility.yml).

## CI baseline

Measured from the successful `master` runs for source `d427210` on November 7,
2026: [Linux run 42423387489](https://github.com/irregularthanis/ggpo-next/actions/runs/42423387489)
and [Windows run 42423387435](https://github.com/irregularthanis/ggpo-next/actions/runs/42423387435).

| Workflow | Jobs | Summed job time | Critical path | Relevant observation |
|---|---:|---:|---:|---|
| Linux compatibility | 4 | 3m 33s | 1m 02s | Cached container build steps took 18–27s |
| Windows compatibility | 11 | 23m 39s | 6m 51s | x64 shared Release includes ten noise runs |
| Combined required build/test jobs | 15 | 27m 12s | 6m 51s | Owner approval is separate and negligible |

This is a single successful-push baseline, not a statistical runner-cost study.
No retry or failure occurred in these two runs. M6 must collect comparable
samples before changing required checks. The current protected branch requires
four Linux checks, eight Windows matrix checks, frozen Windows compatibility,
two portable-header checks, and fork-owner approval.

## Priority order

1. P0: InputQueue/Sync rollback contracts, uncovered session creation, reusable
   clock/endpoint/UDP lifecycle assertions, and malformed protocol boundaries.
2. P1: TimeSync, containers, configuration, filesystem, diagnostics and the
   remaining public lifecycle/configuration APIs.
3. CI consolidation only after the new evidence maps to every gate proposed for
   removal or relocation.

No existing test or CI lane is removed by this baseline.
