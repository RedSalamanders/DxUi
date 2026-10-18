# WARP raster-policy diagnostic, 2026-10-18

The requested execution-policy intervention is implemented and functionally
qualified. It does not resolve scheduler attribution or the performance HOLD.
CallingThread remains Map dominated and is slower in these diagnostic windows.
Keep the production device, canonical fixture, thresholds and baseline unchanged.

## Actual controls

Four fresh Release processes run Default/CallingThread/CallingThread/Default in
129.766 seconds after owned build/tooling/timing work finishes. Each uses 200 dirty
warm frames followed by eight 40-frame windows, with boundary timers On and the
thread-state observer Off. These are the existing 83-control synthetic complex-UI
scene, not a rich-editor typing workload. The forward/reverse windows independently
vary clear and composition before a blocking one-pixel readback. Their costs
cannot be inferred by algebraic subtraction.

All 1,280 frames retain zero C++ allocations and clean-preparation/surface/hidden
invariants. All four native children exit 0 without timeout or output-drain errors.
Requested/device-reported flags are 32/32 for Default and 40/40 for CallingThread.
All four populated 1280x720 PNGs have SHA-256
`6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD`,
matching the earlier clear/readback captures.

| Order | Requested policy | First full-work window p95 ms | Map share at that p95 frame |
| --- | --- | ---: | ---: |
| 1 | Default | 0.7756 | 98.38% |
| 2 | CallingThread | 4.5538 | 99.60% |
| 3 | CallingThread | 4.6896 | 99.59% |
| 4 | Default | 0.4622 | 96.73% |

These are window 0 values, not pooled windows or a baseline/candidate verdict.
All 32 windows, 1,280 individual frames and original outliers are retained in
[raw controls](Raw/Controls/). Pure analysis reports nearest-rank p50/p95/max,
the Map interval of the same p95 frame and separate window CPU/cycle deltas.
CPU-time counters are quantized and queried separately. A zero CPU-time delta
does not mean zero CPU work; positive main-thread cycle deltas remain retained.
No worker ownership, CPU occupancy, ready-time, GPU-duration or scheduler-delay
claim follows.

[Microsoft documents the flag](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_create_device_flag)
as a WARP profiling/debugging policy that changes raster work placement and
parallelism, with a warning against general deployment. Device-reported flags
prove the accepted request, not actual application-main-thread execution. These
runs do not isolate that execution. Do not infer a driver defect or use the flag
as an optimization for the production control.

Selected-image counters captured outside the complete study record compiler
activity: one matching compiler gains 152.109375 CPU seconds. The snapshots contain
no command lines or paths and change no process/thread state. They cannot prove
a quiet host, identify owners or attribute individual frames. Two processes per
policy also cannot establish stability. Kernel scheduler tracing remains refused
by Windows policy, as retained in the
[earlier observer investigation](../2026-10-18-thread-state-resolution/README.md).

## Qualification and preserved failures

The fixed fourteen-operation matrix passes six build profiles, 57 ordinary x64
suite runs with zero skips, and six isolated Debug/ASan clear/geometry/flat
diagnostics. It takes 2,828.066 seconds; all eleven changed code/test/script/prose
inputs match their start/end hashes. Full five-validator/tooling qualification
and clang-format 22.1.3 pass in 332.638 seconds. All 141 portable profiling fixtures
pass. ARM64 execution, physical IME/AT and agreed focus-taking tests remain open.

Flat Project passes all 21 observed Release allocation points and 29 Debug/ASan
points, preserving source and successful retries. The original Debug probe is
also retained. No flat-helper defect is reproduced under this compiler, and no
constructor/value API change is adopted. The earlier
[guarded geometry failure/fix](../2026-10-18-retained-geometry/README.md) is separate.

Ordinary EditorControls excludes repeated timing in every profile, while retaining
independent behavior/resource tests. Explicit Release selection executes the
original five-trial source body, SHA-256
`38F507301DAB9D0A8612D20365ECD09F856209E7611575084665E33DF76DDB91`.
All five trials pass; initial/edit medians are 683.7/66,413.6 ms. They are descriptive
shared-host observations with the same library fingerprint as earlier guarded
geometry, not a causal speed/regression comparison.

The initial timing collector fails after the owned root/native run exits0:
it reads the ordinary receipt basename, while filtered tests use `.filtered`.
Its source, exception, root invocation/result and complete streams remain retained.
Evidence-only recovery validates/copies the already completed filtered receipt,
log and five samples without a native rerun. The original collector is not
promoted to a passing operation.

Four malformed native boundary requests fail before report/capture output. The
legacy public runner, without a policy argument, retains schema v1, its three
arguments, native invariants and matching capture. That smoke check overlaps the
last ARM64 build; its timings are not part of the fixed four-process comparison.
Pure arithmetic fixtures cover 64 synthetic windows/2,560 frames, unsorted
percentiles, same-frame Map attribution, CPU units and one-byte raw tamper refusal.
They use copied native PNG bytes only as hash fixtures, not new native captures.

## Bindings and replay

Library source remains
`4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C`;
canonical benchmark remains
`E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E`.
The Release diagnostic executable is
`97E3848DE9BAD623939099C8DD6EDCE036ADBD392C7FE1EB39210EACBDF58AC0`;
the explicit timing executable is
`DBEF9AFA838852F17C0880FE7D1FA1D6C2C6AC4276AA9335D8C4959AC2D1752F`.
[The inventory](inventory.receipt.txt) retains source/build/receipt/capture hashes,
original paths and private exact artifact hashes. Twenty-eight binaries/runtime
DLLs/PDBs stay private. Ordinary build products are not published in this packet.
Every file stays within the 150-character repository-relative restore limit.

Run `pwsh -NoProfile -File Verify.ps1` here to verify seals, all 57 ordinary
receipts, six builds, frozen inputs, CLI/legacy behavior, timing recovery,
policy/child/source/capture bindings and pure analysis replay. It launches no
native benchmark, build, privileged trace or desktop interaction. The pure replay
is retained in a fresh private temporary directory. This verifies integrity and
replayable calculations; it does not turn diagnostics into performance acceptance.

Both one-byte altered copies are rejected at the expected native-receipt/source
hashes, while the original files stay unchanged. The first post-archive spec check
requires an explanation inside the Performance folder; that original failure is
retained. Adding its README changes documentation only. All five final validators
then pass, including the 150-character restore-path limit. Review logs and negative
integrity receipts are retained in [Review](Review/).

The raw-baseline renderer draft is uncompiled and not adopted production support.
The [feature plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md) stays WIP.
Continue exact retained line geometry/drawing, shared preparation, full pinned
Markdown/extensions and source-preserving rich editing. No consumer pin changes.
