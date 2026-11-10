# Linux compatibility baseline

Linux runs the same frozen layout assertions as the
[Windows baseline](../windows/README.md), plus a shared-object export check.

| Test | What it does | Detects |
| --- | --- | --- |
| `linux_public_layout` | Builds [`public_layout.cpp`](../windows/public_layout.cpp) against the Linux toolchain | Public type size, offset, callback-slot, or enum drift on LP64 |
| `linux_packet_layout` | Builds [`packet_layout.cpp`](../windows/packet_layout.cpp) | Packed `UdpMsg` changes that would break Linux↔Windows peers |
| `linux_exports` (shared builds) | Runs [`check_exports.py`](check_exports.py) on `libGGPO.so` against [`exports.txt`](exports.txt) | Missing, added, or renamed `ggpo_*` dynamic exports |
| `linux_vectorwar_gui_host_contract` | Runs the SDL host and renderer under `SDL_VIDEODRIVER=dummy` | SDL window/renderer creation, bounds, neutral input, clock, and diagnostics regressions |

`exports.txt` matches the Windows `exports-x64.txt` manifest: the public API is
the same 16 functions on both platforms.
