# Grid native heap attribution and rejected experiments

**Investigation, not acceptance.** Library production remains V11 (`71d6446`,
current documentation head `d6c8bb8`). The original implementation is `c52a8f5`,
the production tree of the qualified `78b3de3` pin. No consumer pin changes.
The opt-in benchmark adds process-local `HeapWalk` samples; it changes no
production rendering. All raw reports, comparisons, experiment patches, logs
and hashes are retained in [raw](raw/summary.txt) and [provenance](provenance.txt).
Full external evidence remains under
`C:/RedSalamander.Perf/evidence/i26-ui/grid-heap-attribution-20260921/`.

## What the additional memory contains

Matched Release, 6,000 scroll frames over six passes through the same 1,000 rows:

| Unpaced diagnostic | Original | V11 |
|---|---:|---:|
| Private bytes, median of 30 scroll samples | 29,431,808 | 37,052,416 |
| Live heap bytes, median | 9,627,830 | 10,854,939 |
| Free space reported inside heaps, median | 5,443,928 | 14,900,864 |
| Private bytes, sampled peak | 29,487,104 | 45,182,976 |
| Private bytes after host detach | 29,384,704 | 40,390,656 |
| Unpaced dirty completed offscreen FPS | 57.157 | 119.204 |

The additional sampled live heap data is about 1.17 MiB at the median; the
reported free heap space increases by about 9.02 MiB. This supports allocator
retention/fragmentation as a substantial contributor, rather than growth in
the exact 3,686,400-byte render surface or per-round C++ allocation count
(1,080 in both). It does **not** identify allocation call stacks or prove all
free heap space is resident. Private process memory, heap accounting and
working set are different measures and must not be added together.
Every reported heap walk completes with error 0. Each heap is locked separately;
the samples are not a simultaneous process-wide heap snapshot. No purge or
working-set trim is performed.

Windows denied the process-scoped WPR HeapSnapshot recording setup with
`0x80070005` before the benchmark started. No tracing or persistent process
configuration was installed. The in-process diagnostic needs no elevation.
It attributes heap occupancy, not DLL ownership or the separate menu candidate's cost.

## Controlled experiments

All experiments are **reverted**; their patches remain in `raw/`. Do not repeat
them unchanged or treat them as accepted optimizations.

| Unpaced experiment | Dirty FPS | Private median | Private peak | Outcome |
|---|---:|---:|---:|---|
| V11, retained control | 119.204 | 37,052,416 | 45,182,976 | Original resource flag reproduced |
| Release layouts after every paint | 123.190 | 33,912,832 | 46,833,664 | Higher peak; no demonstrated removal of growth |
| Draw measured text through `DrawTextW` | 106.365 | 33,742,848 | 41,730,048 | Slower; growth remains |
| Release all cached text/ellipsis after paint | 101.315 | 34,859,008 | 47,403,008 | Slower; C++ allocations rise to median 1,650/round |

The final harness adds a separate paced diagnostic: retention targets 50 frames/s
using fixture-only sleeps, outside the five unpaced measured FPS rounds. It tests
whether different wall-clock allocation rates explain the result; it never paces
production or substitutes for the normal acceptance benchmark.

| Paced diagnostic | Original | V11 | Single-layout inline-tail prototype |
|---|---:|---:|---:|
| Private median | 29,743,104 | 36,831,232 | 34,004,992 |
| Live heap median | 9,599,035 | 10,845,483 | 10,686,822 |
| Free heap median | 6,023,120 | 16,356,816 | 13,396,576 |
| Private peak | 30,109,696 | 42,332,160 | 39,960,576 |
| Private end | 29,868,032 | 30,523,392 | 33,353,728 |
| Retention duration | 121.15 s | 120.02 s | 120.01 s |
| Unpaced dirty FPS rounds | 52.814 | 122.352 | 121.062 |

Growth remains when wall-clock pacing is matched. The prototype reuses one
DirectWrite layout and replaces the hidden tail, including paragraph separators,
with an inline ellipsis. Grid/Embedded/Rendering pass in Release with zero skips,
including actual French/emoji/omission/copy/clipping pixels. It removes about
2.70 MiB from V11's median in this one paced comparison, but still exceeds the
original median by about 4.06 MiB; the comparison remains advice-required.
No repeated improvement, settled bound or general non-regression is claimed.
The prototype also measures the full input initially, unlike V11's bounded
paragraph prefix; review hidden-tail cost before considering it. Its complete
patch and specific test/performance receipts are retained, not adopted.

## Qualification and reproducibility

The final **diagnostic-only** code, with V11 restored, passes Grid/Embedded/Rendering
in x64 Debug, Release and ASan Debug: nine suite passes, zero skips, including
the ASan detection probe. All three ARM64 configurations cross-build successfully.
The raw qualification folder binds suite, executable and common performance receipts.
Skills/specs/dependencies/format checks pass, as do all 10 performance-comparison
tool tests. All 77 copied raw hashes and byte-identical summary reproduction pass.
Those common results are unpaired and do not waive the original resource gate.
Native ARM64 execution, hardware presentation, consumer DPI/AT and adoption remain open.
No gallery regeneration is needed: the committed production/rendering inputs are
unchanged; the experimental rendering patches are not the current gallery implementation.

Use `performance.ps1 -Scenario MultilineGridHeap` or `MultilineGridHeapPaced`
on both implementations, with identical final harness hashes. Unpaced diagnostic
reports before adding the paced option have a different harness hash; do not
cross-compare those two sets. Each existing comparison checks its matched hash.
`python raw/summarize.py` reproduces `raw/summary.txt`; medians use only the 30
`scroll` samples, not the start/model-clear/detach records. JSON/log/patch files
are copied byte-for-byte; the SHA256 manifest covers all reviewed raw files.
Non-benchmark suite/probe JSON is named `*.receipt.txt`, and summary/provenance
JSON uses `*.txt`, so these are not classified as standalone performance reports.
The manifest maps original and reviewed names; their content bytes are unchanged.

Next: targeted Astra Extra High review of the unexplained native allocation cost,
using these results and the saved single-layout prototype. Retain the qualified
consumer pin and every failed/noisy receipt; do not infer a resource waiver.
