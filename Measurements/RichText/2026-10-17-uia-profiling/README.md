# Controlled Default investigation of the UIA range-command change

Performance acceptance remains **HOLD**. This packet investigates the two retained
[unpaired clean-frame p95 warnings](../2026-10-17-uia-ranges/README.md), without
editing the library, canonical fixture, judge, assignment generator, policy,
thresholds or original reports. It covers Default only; full three-scene policy
qualification, the earlier D2b/D2c/D2d warnings and rich-editor workload acceptance
remain separate open gates.

Baseline: `71059fb94c12c8966056e3afe626d1548ab1030b`, library fingerprint
`FC6F53842E58EA9EB63181CC005E51913617C80DA6ACD194CE96649B7F6F6941`.
Candidate: `8c21a634c5abd55f14e384673e7d710de572bdcb`, library fingerprint
`E3437165FCDB1374DE2AA8A2AE0AA791CB4581AB9528180ACA91D90508ECFDCA`.
The candidate's dirty state consists of the investigation's prose/evidence; the
library and measured harness bytes are frozen. No consumer pin changes.

## Method and setup refusal

The initial runner created/restored/built a detached baseline, while reusing the
candidate's previously attested Release executable. First-pair inspection found
different toolchain identities. The inspected evaluated library-property inventories
differ only in TEMP and TMP: the shared absolute directory was inside the
candidate root and outside the baseline root. The unchanged normalization helper
therefore produced a repository-relative value on one side and an absolute value
on the other. This is an ineligible comparison.

The owned runner was stopped after two fully stamped receipts and one native-only
interrupted receipt. All original bytes and the stop/refusal metadata remain in
[Refused](Refused). No timing conclusion uses these reports. The runner had not
written its final summary or seed before the stop; only its observed A01/B01/B02
prefix is retained, without inventing the remaining assignments.

Both Release/x64 outputs were then rebuilt with `TEMP=TMP=Z:\DxUiPerfTemp\uia-ranges`,
outside both trees. Full cross-side preflight checks fixture, harness, compiler/
toolchain, installed dependencies, environment and WARP identities before the
fresh study. Source fingerprints remain the two values above. These fresh builds
belong to this diagnostic setup; the earlier functional qualification remains
bound to its own archived artifacts and profile.

The fresh fixed inventory is twelve independently randomized ABBA/BAAB blocks,
48 fresh serial processes (24 per side), seed `623802705`. The wrapper retains
the seed and complete literal schedule before starting the unchanged paired
runner and checks them against its completed summary. Each process uses the
unchanged five 40-frame clean and five 40-frame dirty rounds: 19,200 timed frames.
Both within-binary controls per block remain in the result. No process or outlier
is replaced. Its owned-runner limit is 2,400 seconds after setup/preflight;
corrective setup has its own 1,200-second build budget.

The subsequent eight fixed diagnostic processes use heap-off ABBA, then heap-on
BAAB: two reports per side/profile, 2,240 frames total. The existing helper adds
20/100/200 dirty warm checkpoints, twelve CPU/thread/cache/resource stages and
40-frame clean/dirty probes. It retains sampling costs, raw native reports,
query failures, child metadata and captures. The overall limit is 900 seconds,
with the existing 120-second native-child limit. These samples describe CPU
consumption and allowed affinity; they do not measure actual thread placement,
scheduler delay, native allocation ownership or a dedicated quiet host. Shared
sampling remains active with heap sampling off and can affect later readiness.
Heap-off/on spread cannot by itself isolate the observer effect.

## Results and interpretation

All 48 processes completed within the 2,400-second budget (1,293.565 seconds).
All cross-side fixture/tool/dependency/environment identities match, with one
stable source fingerprint and executable hash per side. The retained
[summary](Paired/summary.receipt.txt) and independent judge replay agree: all 26 slots
are `within-noise`, with no exact resource-budget or raw-peak violation. The frozen
base legacy judge and migrated judge agree on all verdicts. Runner exit zero
establishes completed collection, not performance acceptance.

| Metric | Baseline run-median center | Candidate run-median center | Paired block effect | Holm-adjusted p |
| --- | ---: | ---: | ---: | ---: |
| Clean frame p95 | 0.47560 ms | 0.47685 ms | -1.31% | 1.000 |
| Clean preparation p95 | 0.00030 ms | 0.00030 ms | +0.58% | 1.000 |
| Clean CPU composition p95 | 0.01170 ms | 0.01140 ms | -0.49% | 1.000 |
| Clean completed FPS | 2,515.58 | 2,558.88 | +2.01% | 1.000 |
| Dirty frame p95 | 1.77075 ms | 1.80620 ms | -2.60% | 1.000 |
| Dirty private bytes | 29,480,960 | 29,243,392 | -1.02% | 1.000 |

Centers are medians of 24 run medians per side; effects are geometric averages
of the twelve paired block ratios. These are different aggregations, so an effect
is not the ratio of the two displayed centers. [analysis.receipt.txt](analysis.receipt.txt)
retains every metric, run median, control and per-round tail frame.

All 24 within-binary controls flag drift in at least one metric. Thirteen cross
the clean-p95 5% investigation band, with changes from -14.33% to +14.41%.
Clean run-median p95 spans 0.4466-0.5595 ms on the unchanged baseline and
0.4165-0.5537 ms on the unchanged candidate. This study does not establish a
consistent candidate slowdown, and unstable controls prevent a non-regression
acceptance claim. It does not retroactively dismiss the original two warnings.

At the actual p95-ranked frame of each round, the median remainder after subtracting
that same frame's preparation and CPU composition is 98.14% of frame time on the
baseline and 98.06% on the candidate. This remainder includes target bind/clear,
copy, blocking one-pixel readback and other overhead. It supplies a next profiling
boundary; it is not a separately timed GPU or scheduler-delay measurement.
Clean preparation's median reported p95 is three 100-ns QPC ticks on both sides;
its percentage changes need quantization context. This does not explain the
much larger completed-frame tail by clock quantization.

All 240 clean rounds allocate zero C++ objects; every clean and dirty round has
zero composition allocations. Dirty round totals are 2,145 or 2,160, below the
2,560 Release limit. Both phases retain one 3,686,400-byte surface and the same
replacement peak. No exact resource-budget increase is flagged.

All eight [profiles](Profiles/completion.receipt.txt) completed in 275.835 seconds,
with complete telemetry and the exact executables used in the canonical study.
All eight captures are byte-identical; the lead decoded the common populated
1280-by-720 scene. Profile clean-probe completed FPS remains variable after the
fixed 200 dirty warm frames:

| Side / heap sampling | Two clean-probe FPS values |
| --- | --- |
| Baseline / off | 2,516.59; 2,584.26 |
| Candidate / off | 2,558.64; 2,343.64 |
| Baseline / on | 2,666.33; 2,490.74 |
| Candidate / on | 2,392.93; 2,548.83 |

Total native stage sampling costs 1.273-1.684 seconds per process, versus
0.401-0.421 seconds in all timed windows. Heap-on accounting contributes
4.631-4.901 ms per process, including enumeration, traversal and JSON work.
The common sampler remains active in both profiles. These two samples per cell
establish neither warm-up convergence nor a causal heap observer effect.
Observed thread count rises from four to 36; no actual placement or component
ownership is inferred. Seven of eight clean probes have zero reported main CPU
time but nonzero cycle deltas. CPU-time deltas appear in 15.625-ms increments;
cycle deltas remain raw consumption counters. Hidden stages retain zero cached
surface/layout residency and perform zero hidden preparation/composition.

The [independent arithmetic check](Raw/independent-checks.receipt.txt) reproduces the
-1.312134% paired effect directly from original per-round values, outside the
judge implementation. It also retains profile costs and CPU quantization counts.
All originals, including the refused setup, remain sealed by [SHA256SUMS](SHA256SUMS).

## Replay and validation

Metadata/raw/derived JSON uses `.receipt.txt` archive names, following the
repository convention. [The alias inventory](Raw/archive-aliases.receipt.txt)
checks identical hashes before/after every rename; embedded original paths and
bytes are unchanged. Benchmark JSON reports have adjacent explanations. The
first affected validation passed formatting and every tooling test but failed
specification validation with 165 archive-metadata findings. Its full stdout,
stderr and exit receipt remain in `Raw/first-*`; the repaired specification pass
and final replay/hash-negative checks are separate receipts. No timing result
was rerun or edited to fix packaging.

Run `./Measurements/RichText/2026-10-17-uia-profiling/Analyze.ps1` from this source
tree. It verifies the raw-byte seal, recorded harness/helper hashes, all 48 native
frame inventories, exact paired-judge replay, profile/canonical artifact identity,
all native profile fields and every raw/child/log/capture binding. It writes only
the derived `analysis.receipt.txt`. Changed helpers require the recorded `8c21a634`
versions; do not reinterpret the packet under another judge.

`Invoke-Study.ps1` and `Invoke-Profiles.ps1` retain the bounded collection drivers.
Repeating collection requires separate trees at the exact commits above,
`vcpkg-install.ps1 -Platform x64`, root Release builds with a common TEMP/TMP
outside both trees, and a fresh output directory. Preflight matching full
identities first. The study driver generates and retains a new independent seed;
it does not overwrite this study. Profiling outputs inside the candidate belong
under `.build/reports`. Machine power/priority/affinity policy was unchanged and
independent applications were never stopped. No ambient-load or kernel trace
was collected; matching environment identities do not prove a quiet host.

No library or measured harness input changed. The affected scope is Tooling;
the earlier six-build/57-suite functional packet keeps its own exact artifacts
and profile. This packet does not claim fresh Debug/ASan/ARM64 or foreground/AT
execution. Final tooling, validators, format and hash-negative/replay evidence
are retained with the closeout.

The repaired affected `Tooling` scope passes all five validators, the complete
tooling suite and sanitizer-runtime staging/rejection fixtures. Its actual
`SELECTED_PASSED; repository NOT_EVALUATED` result and exit zero are retained in
[the final validation receipt](Raw/validation-final.receipt.txt). Pinned
clang-format 22.1.3 passes; only evidence/prose changed after that check. The
original failed validation and successful repair are separately retained.
The [final hash-negative test](Raw/hash-negative-final.receipt.txt) rejects a
one-byte change in a private copy, and final analytical replay verifies all
originals. This does not substitute for native or performance qualification.

Acceptance remains **HOLD**: the base policy is `policy-review-required`, controls
are unstable, two qualification scenes are omitted, and rich-editor stress and
physical-platform gates remain open. The next attribution step is a separately
bounded clear/readback or scheduler/CPU-stack trace on a controlled runner,
with unchanged canonical measurements and a separate observer-effect check.
This investigation supplies no source optimization target with established
causality and approves no regression, waiver, rebaseline, merge or consumer pin.
