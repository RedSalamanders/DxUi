# Thread-state observer resolution, 2026-10-18

The four same-binary Off/On/On/Off controls complete successfully, but thread-state
attribution is inconclusive: none of the 35 acquisition brackets fits inside a
native frame. Query p95 is 17–25 ms, while the first full-frame window has p95
0.45–0.57 ms. Do not infer a Map wait reason or scheduler delay from these samples.

Build/tooling activity had stopped before the fixed 47.590-second schedule.
All four owned native children exit zero without timeout, truncation or output
drain errors. All 1,280 frames preserve zero C++ allocations, clean preparation
and hidden-resource invariants. All four populated 1280x720 captures have SHA-256
`6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD`,
matching the earlier clear/readback captures. This is not dedicated-host proof.

| Order | Observer | Snapshots | Query p95 ms | Full frame p95 ms | Map share at that frame p95 |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | Off | 0 | — | 0.4566 | 95.75% |
| 2 | On | 18 | 25.0090 | 0.4530 | 96.95% |
| 3 | On | 17 | 17.2026 | 0.5742 | 96.52% |
| 4 | Off | 0 | — | 0.5713 | 96.39% |

These are first-window values, not all eight windows pooled. Every original
window, frame and outlier remains in [the raw controls](Raw/Controls/). The On
maximum full-frame outlier is 2.4583 ms; the final Off maximum is 1.3083 ms.
The controls do not isolate stable observer overhead. A Map elapsed share is a CPU
boundary observation, not GPU duration or an algebraic component-cost estimate.

## What was observed

The sampler refreshes Process.Threads only for the native child it created and
retained. It adds no target thread, suspends no target thread and changes no
privilege, policy, affinity, priority or timer resolution. Thread/wait enums,
errors, truncation flags, query/read brackets and first-seen creation identities
are preserved. The main native PID/TID/creation is verified without converting
FILETIME through JavaScript Number. Worker-ID reuse is not attributed.

Only the full acquisition bracket may be assigned to a frame/segment; mixed and
cross-frame/outside samples remain separate. All 35 observations land in the
latter group, so the main-state/wait histograms for Map are empty. Empty histograms
are unavailable evidence, not evidence of zero waiting. Samples are unweighted
counts and cannot estimate CPU occupancy or ReadyThread latency.

Functional tests verify conditional wait-reason access, an owned wait/busy child,
disabled collection, exact creation identity, frame/Map/mixed/outside containment,
nearest-rank arithmetic, Int64 serialization and raw-input tamper rejection.
A native/managed clock check places 32 managed timestamps inside native QPC
brackets at identical 10 MHz frequencies. Installed .NET 10.0.11 assembly hashes
and read-only IL establish cached ThreadState/WaitReason behavior. The exact
product-version source lookup returned 404; that lookup and helper-development
failures remain retained rather than presented as runtime evidence.

The public [.NET wait-reason contract](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.processthread.waitreason?view=net-10.0)
permits the getter only for a waiting thread. Public runtime source is explanatory;
the packet separately identifies the actually loaded assembly. Three-byte fake
image hash fixtures in synthetic analyzer tests are aliased as receipt text and
are not native captures.

## Bindings and next experiment

This study uses the guarded geometry library source
`4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C`.
The canonical benchmark fingerprint remains
`E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E`.
[Source/build bindings](inventory.receipt.txt), the original Release attestation
and exact private executable hash are retained. The companion
[geometry packet](../2026-10-18-retained-geometry/README.md) records its full
functional qualification. No new baseline/candidate acceptance is claimed.

Windows policy still refuses kernel scheduler tracing. A next separate diagnostic
can compare ordinary WARP with its documented calling-thread rasterization flag;
[Microsoft's flag contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_create_device_flag)
explicitly describes this profiling use and cautions against general deployment.
That experiment is planned, not implemented here. Keep library/device defaults,
the canonical benchmark, policies and thresholds unchanged. The flag also changes
work placement/runtime behavior; it cannot isolate scheduler latency by itself.

Run `pwsh -NoProfile -File Verify.ps1` from this folder to verify seals, native
identities/invariants, all source bindings and pure analysis replay. No native
benchmark or privileged trace is launched. The replay is retained in a fresh
private temporary directory. The [feature plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md)
and performance acceptance remain open.

Archive paths use compact aliases within the 150-character consumer restore limit.
The inventory retains original source paths, pre-alias archive paths and every
byte hash. The first post-archive dependency check found 32 long paths; its original
failure is retained in the geometry packet's Review/Validation-First records.
Shortening aliases changes packaging only, not native data or test outcomes.
