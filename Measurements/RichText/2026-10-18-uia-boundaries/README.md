# RichTextEditor clear/readback investigation

Diagnostic only. Performance acceptance remains **HOLD**. This record extends
the [48-process Default investigation](../2026-10-17-uia-profiling/README.md);
it changes no canonical workload, judge, threshold, policy or original receipt.
The Default scene does not instantiate RichTextEditor. This study predates the
subsequent indexed-document lookup implementation.

Eight declared fresh Release/x64 WARP processes completed in 234.312 seconds
within a 900-second budget; the guarded runner bounds each native child to
120 seconds. Timers-off order is ABBA; timers-on order is BAAB. A is
71059fb94c12c8966056e3afe626d1548ab1030b; B's library is unchanged from
8c21a634c5abd55f14e384673e7d710de572bdcb (checkout HEAD 42ec1984 at collection).
The exact final diagnostic closure was overlaid onto A, verified, built and
restored clean afterward. Both builds/runs use TEMP=TMP=
`Z:\DxUiPerfTemp\uia-ranges`, outside both trees. Full fixture, harness, toolchain,
dependency and environment identities match; the literal plan and every
before/after identity are retained in Profiles.

The separate `dxui-complex-ui-boundaries-v1` diagnostic uses 200 dirty warm frames,
a populated capture, then eight 40-frame clean windows: full, clear-only,
composite-only, readback-only, then reverse order. Every window retains clean
preparation, target binding and one-pixel copy/blocking Map/Unmap. Timers On adds
absolute QPC edges after each call; Off records frame start/end only. CPU counters
bracket windows; no phase/thread/heap inventory sampler runs between them.

| Control | A Off p95 ms | B Off p95 ms | A On p95 ms | B On p95 ms |
| --- | ---: | ---: | ---: | ---: |
| Full | 0.51680 | 0.57515 | 0.47350 | 0.53240 |
| Clear | 0.10045 | 0.11205 | 0.12100 | 0.13135 |
| Composite | 0.51340 | 0.96190 | 0.44270 | 0.55120 |
| Readback | 0.05920 | 0.05130 | 0.03290 | 0.04705 |

Each cell is the median of four window p95 values from two processes. Per-window
p95 is sorted index 37 of 40, matching the existing benchmark. These descriptive
centers are not a paired acceptance judgment. B Off's composite-only windows in
process 3 reach 2.0695 and 1.1337 ms; those observations are retained, not replaced.
Timers-on centers can be lower than timers-off despite added QPC calls. This
small pilot does not isolate timer overhead, establish convergence or demonstrate
a quiet/dedicated host.

For the **same p95-ranked full frame**, median blocking-Map shares are 96.586%
on A and 96.520% on B. Every individual boundary is retained; independent integer
QPC arithmetic reproduces the shares and telescoping sums. The interval includes
deferred backend work, completion waits and possible preemption. It is not a
separately measured GPU duration or scheduler delay. Removing commands changes
dependencies/readiness, so controls cannot be subtracted into operation costs.
No library optimization is justified by these observations alone.

All 2,560 probe frames pass native zero-C++-allocation, no clean preparation,
surface-reuse and hidden zero-work/release checks. All eight captures are byte
identical; the final populated 1280x720 capture was visually inspected. Their
SHA-256 is `6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD`.

Scheduler tracing was attempted with a unique WPR instance and a circular
64-buffer x 64-KiB custom ProcessThread/CSwitch/ReadyThread/SampledProfile profile.
The initial selector incorrectly included `.Memory` and returned `0xc5600611`.
The corrected selector reached the profile and was refused with **0xc5585011**,
“Failed to enable the policy to profile system performance.” No trace was
collected, no machine policy changed, and no unrelated recording was stopped.
Status, token privileges, exact profile, both attempts and native error codes
are retained. [Microsoft's WPR options](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options)
document unique instances; [SystemProvider](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/systemprovider)
documents the scheduler event provider configuration.

## Retention and replay

Profiles holds all stamped reports, exact native bytes, owned-child metadata,
logs, captures and the original predeclared plan/completion inventory. Raw holds
the pre-change canonical baseline, both successful initial builds, the include
formatting refusal and corrected builds, schema tests, validation/format receipts,
overlay backups/records, scheduler preflight and clean restoration. Native and
metadata JSON files use `.receipt.txt` aliases to distinguish them from stamped
benchmark reports; the bytes are unchanged. Harness freezes every measured
profiling input. Setup/Invoke-Boundaries are the actual collection drivers;
collection requires fresh output paths and matching restored dependencies.

Run `Analyze.ps1 -SourceRoot <checkout-with-the-frozen-Harness-bytes> -Reports
<packet>/Profiles`. It checks SHA256SUMS when present, helper hashes, original
native/stamped equality, process schedule, identities and raw/child/log/PNG
bindings before deriving analysis. Restore Harness files to a disposable checkout
for replay after later tooling changes. Analyzer output field ordering is not a
byte-level reproducibility claim; compare the parsed derived values.

The five validators, full tooling suite (including all 125 profiling cases) and
clang-format 22.1.3 passed before collection. Broader native/build qualification
of the diagnostic and the subsequent model change is recorded separately; this
packet does not reuse old native test evidence as current qualification. Gallery
pixels and public control capabilities are unchanged by the diagnostic. All
historical warnings, unstable controls, policy qualification, remaining scenes,
rich-editor workload and physical IME/AT/ARM64 gates stay open.

