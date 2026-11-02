# Archived RichText usage-guide qualification notes

Historical prose moved from `docs/rich-text.md`; these are descriptions of earlier runs, not a new run or current-source qualification. The original document is Git blob `eac26014d0ab93225eee4cb85238ddcb7699283f` at revision `89c5424dfae23520ace371df7a5e0d9ec66d4151`. Selection boundaries are the named qualification paragraphs below; only relative links to this archive directory were adjusted after relocation. Original evidence remains at its existing owner.

The standalone prototype consumes exact code pin
302f3783a5d2ef60f201ca3abf508c1158fde039 through an immutable consumer checkout.
[Core and broader qualification](README.md)
records 57 normal suite runs and all six builds. Consumer linkage/ownership and
application smoke are separate receipts. Performance comparisons remain
advice-required; the original baseline and both warning reports are retained.

The [D2c qualification](README.md)
retains the native/embedded, sanitizer, build, gallery and timing evidence for
this bounded preview. Full OS input/accessibility and conformance are separate
from these passing v0 gates.

The [D2d qualification](README.md)
retains the source-adapter protocol/client, history, retirement, geometry and
constant-size UIA range allocation evidence. It includes 57 ordinary x64 suite
passes and six builds. Physical IME/AT, rich structural UIA semantics, native
ARM64 execution and controlled performance acceptance remain open; both diagnostic
memory-warning reports are retained.

The [source range-command qualification](README.md)
records 57 ordinary x64 suite passes, six builds and 41 named rich-text scenarios
per x64 profile. Native/embedded command semantics and real-client actions/events
pass. Performance acceptance is on hold after repeated Default clean-frame p95
warnings; both failed comparisons and the original baseline are retained.

The [controlled Default investigation](README.md)
retains 48 interleaved reports and eight CPU/cache profiles. Its paired clean-p95
effect is -1.31%, with no judged regression; 13 of 24 same-binary controls cross
the clean-p95 band and all 24 flag some drift. A consistent slowdown and
non-regression acceptance are both unestablished. Policy qualification,
rich-editor workload performance and the original historical gates remain open;
no library source, threshold, baseline or consumer pin changed.

The [clear/readback follow-up](README.md)
retains eight fixed processes with timers-off/on and omitted-command controls.
Blocking Map accounts for about 96.5% of each side's same-p95 full-frame tail;
controls remain variable and Windows refused scheduler tracing. This identifies
a wait boundary, without proving GPU duration, scheduler delay or non-regression.
The canonical workload and all original acceptance gates remain unchanged.

The [indexed-source qualification](README.md)
retains its 57 ordinary x64 suite runs, six builds and both boundary-timer
modes in each x64 profile. The fragmented Unicode oracle and concurrent snapshot
reads pass; the fixed lookup-trial median moves from 6.5310 to 0.7234 ms in Release.
That observation is descriptive, with no paired performance acceptance. ARM64
runtime, full Markdown/extensions, incremental preparation and physical IME/AT
remain open. No consumer pin or gallery content changed.

The [Unicode 17 qualification](README.md)
retains the original corpus/allocation failures, 57 passing ordinary x64 suite
runs, six attested builds and all pinned generation inputs. Every profile passes
the 766-vector corpus and native/embedded behavior cases. The 49,152-unit forward
walk requests no C++ heap storage; oversized UTF-8 rejection also requests zero
C++ bytes in both tested cases. The refused ARM64 build-input mutation and its
two successful recovery builds remain separately recorded.

The [character-command qualification](README.md)
adds linear, allocation-free bulk movement and complete-grapheme deletion from
collapsed interior ACP carets. Explicit noncollapsed source selections retain
their exact semantics. All 57 ordinary x64 suite runs and six builds pass,
including native/embedded maximal commands, undo/redo and the original Unicode
corpus at every offset. Embedded records 5,167 checks per profile; maximal command
queries request zero C++ bytes. Original failures and descriptive timing trials
are retained. The performance hold, incremental preparation,
remaining engine and manual acceptance gates continue under the plan.

The [persistent-storage qualification](README.md)
retains all original resource failures, the intermediate Debug proxy failure and
the successful six-profile rerun. All 57 ordinary x64 suite runs pass, with zero
native skips. The fragmented source and capped history retain about 1.21 MB of
C++ storage in Release and 1.31 MB in Debug/ASan, below the fixed 4 MiB fixture
budget; the original vector implementation retained 40.2 MB. Same-content edits
request zero C++ allocations in every profile. All five failure sweeps preserve
complete source/selection/revision/history. Borrowed-input tests destroy or mutate
caller buffers after return and verify that document content survives. Exact observations and descriptive
edit/read/backward-query timings are in the packet. Incremental/shared
preparation, full Markdown/extensions and the performance/manual gates stay open.

The [bounded source-change qualification](README.md)
retains all 57 ordinary x64 suite successes, six cross/native build profiles,
original failures and unchanged fixed operation methods. Source-change queries
allocate zero C++ bytes; localized transient previews retain about 23 KB in the
fixed 4,096-preview fixture. Numeric metadata has an explicit 188,664-byte history
fixture cost. These are functional/resource observations. Incremental geometry,
shared preparation and frame-performance acceptance remain open in the plan.

The [projection-block qualification](README.md)
retains the original reuse and Debug noexcept failures, 57 final native suite
successes, six builds, allocation/retention observations and unchanged operation
methods. Snapshot slicing now propagates allocation failure. Valid block mapping
and unchanged preparation allocate zero C++ bytes. Exact universal retained
rendering, shared scheduling and complete syntax profiles remain plan gates; the
descriptive operation improvements do not resolve frame-performance acceptance.

The [geometry packet](README.md)
retains 57 passing ordinary x64 suite runs, six builds, complete observed
copy/geometry allocation sweeps, 162 exact WARP image pairs and fixed storage
bounds. The original Debug point-173 noexcept move failure is preserved. The
earlier native 13-DIP fixture needed whole-layout compatibility: its descriptive
128-edit median was 53,381.5 ms versus 51,102.6 ms before, with no controlled timing
acceptance. New affected Release checks avoid whole work in native and embedded
13-DIP editors. The [positioned-line packet](README.md)
adds 324 exact pixel pairs across default/fractional/large-origin profiles, 57
ordinary x64 suite successes and all six builds. The unchanged five-trial method
records medians of 145.909 ms initial preparation and 2,166.98 ms for 128 edits,
versus the original 522.474/51,102.6 ms. These descriptive shared-host observations
provide no controlled causal speedup, large-document latency or frame acceptance.
Universal incremental work and shared preparation remain active work.

The [thread-state follow-up](README.md)
preserves four same-binary observer controls and 1,280 clear/readback frames.
All captures match, but 17–25 ms query p95 cannot resolve 0.45–0.57 ms frame p95;
none of 35 acquisition brackets fits inside a frame. There is no Map wait-reason
or scheduler-delay conclusion. The Windows trace-policy refusal and performance
HOLD remain open. The separate WARP threading diagnostic is explicit:

```powershell
./performance-profile.ps1 -Configuration Release -SkipBuild -BoundaryTimers On -WarpRasterMode CallingThread -OutputPath .build/reports/warp-calling.json
```

Use `-WarpRasterMode Default` for a fresh comparison. The v2 report binds requested
and actual device flags. Omitting this option retains v1. The option requires
boundary diagnostics; it changes no library/canonical device default. CallingThread
names the requested SDK raster policy. Device-reported flags do not prove actual
application-main-thread execution or isolate scheduler latency.

The five repeated native preparation timing trials are now opt-in through
`test.ps1 -Suites EditorControls -Tests TestRichTextEditorRetainedPreparationFixedTrials`.
Ordinary reuse/retirement/geometry/input tests remain enabled; the timing body is
unchanged. Its explicit Release run passes all five trials against the preserved
binary. The first collector used the ordinary receipt name and failed after the
native run passed; the filtered receipt/log and all samples were recovered without
a native rerun. Both the collector failure and recovery remain retained.

The [WARP policy packet](README.md)
retains all 57 ordinary x64 successes, six build profiles, six isolated Debug/ASan
probes, full tooling/format checks, four native CLI refusals and legacy v1 smoke.
All 11 changed inputs match their frozen start/end hashes. Flat Project passes
21 allocation points in Release and 29 in Debug/ASan; no flat-helper defect was
reproduced under this compiler, and its value API remains unchanged.

The fixed Default/CallingThread/CallingThread/Default study preserves 1,280 frames
and four byte-identical populated captures. First full-work-window p95 is
0.7756/4.5538/4.6896/0.4622 ms, respectively. Map still accounts for about 99.6%
of the p95 frame in both CallingThread runs. Requested/actual flags are 32/32
and 40/40, but the intervention does not isolate application-main-thread work or
scheduler delay. Selected-image counters also record compiler activity during
the study; they cannot establish a quiet host or attribute individual frames.
Keep all windows/outliers and the original HOLD. This is a complex-UI diagnostic,
not a rich-editor edit-throughput or baseline/candidate acceptance result.

The earlier explicit native preparation trial recorded initial/edit medians of
683.7/66,413.6 ms on this shared host. Those observations retain the same library
fingerprint as the prior guarded geometry; no causal speed/regression conclusion
follows. The later positioned-line record qualifies exact retained placement;
cooperative preparation and the full Markdown/extension engine remain active work.
