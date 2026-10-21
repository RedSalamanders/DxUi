# RichTextEditor validation summary

DxUi-owned workload. Updated: 2026-10-21. PR
[#89](https://github.com/RedSalamanders/DxUi/pull/89).

## Implemented scope

The owned Unicode C++ core provides exact source, selections, transactions,
undo/redo and cooperative preparation with cancellation and stale-result
rejection. The retained native/embedded editor adds lossless CommonMark/GFM
analysis, semantic source/display maps, source caret sites, structural/style
commands, a validated native HTML subset, TSF/IME and UIA adapters, bounded
footnote/flowchart previews and native image rendering.

Unicode file-reference paste inserts displayed Markdown images and relative
named links for other files. Unsaved or cross-root destinations use file URIs;
one mixed paste is one undo transaction. See the
[usage guide](../../docs/rich-text.md). File drag-and-drop remains a
[spec and test plan](../../Specs/Plans/WIP/RichTextFileDrop_2026-10-20.md).
Complete math/diagram/metadata support, larger-document capacity, physical
IME/assistive-technology qualification and consumer adoption remain open.

## Functional evidence

| Source and coverage | Result and limit |
| --- | --- |
| Published repair `b689bddd`, tested merge `d146ba4a`, [CI 39587588088](https://github.com/RedSalamanders/DxUi/actions/runs/39587588088) | Five native profiles pass, including x64 Release and ARM64 ASan. ARM64 Debug fails B2 sibling hover before delayed submenu close. All six original ZIPs and logs are retained and replayed: 125/126 zero-skip child exits pass; 124 scopes qualify because its shared lease failed. Eight consumer variants, both resource fixtures and all four support jobs pass separately. Six gallery PNGs match the previous run. The WIP resume checkpoint retains the unproved investigation paths; no further C++ fix exists yet. |
| Foreground repair, final local source | Complete Menu and NativeTextInput suites pass in x64 Debug, Release and ASan: six zero-skip scope receipts, recorded live keyboard focus and verified foreground/focus/pointer restoration. Original executables and receipts are sealed. Three focused cancellation/layout cases and all 81 native lease self-test checks pass separately. Earlier failed runs and the corrected cancellation-fixture race remain retained; passing repeats do not attribute historical failures. Fresh hosted native CI remains required. |
| Published head `d59b7caa`, tested merge `8f49939d`, [CI 39568060174](https://github.com/RedSalamanders/DxUi/actions/runs/39568060174) | All six original native ZIPs match GitHub sizes/digests. Five profiles pass, including x64 Release and ARM64 ASan; ARM64 Debug fails shortcut-column layout. Child scope exits pass in 125/126 cases with zero skips; both foreground children sharing its failed lease remain unqualified. Eight consumer variants, both Release resource fixtures and docs-gallery generation pass separately. Later repair results remain tied to their own source. |
| Feature head `c1a77e42`, tested merge `6110c7e0`, [CI 39555699478](https://github.com/RedSalamanders/DxUi/actions/runs/39555699478) | All six original native ZIPs match GitHub byte counts/digests. Four profiles pass; x64 Release fails foreground admission/restoration and ARM64 ASan fails a Menu probe assertion. Child scope exits pass in 124/126 cases, with zero capability skips; children of failed shared leases remain unqualified. Eight consumer variants and both Release resource fixtures pass separately. Paired collection completes, but its publisher and aggregate fail. Full qualification remains open; Release docs-gallery generation is skipped. |
| Prior feature head `335c5157`, tested merge `17969dc3`, [CI 39500910925](https://github.com/RedSalamanders/DxUi/actions/runs/39500910925) | All six original native artifacts are sealed and independently replayed: 126 native scope passes, zero capability skips, two resource fixtures and eight consumer variants. That tested merge and head have identical trees. Validation, tooling and formatting pass; performance acceptance remains on HOLD. |
| `61efca4c`, tested merge `6e2f455b`, [CI 39480512299](https://github.com/RedSalamanders/DxUi/actions/runs/39480512299) | Sealed original artifacts verify 126 native scope passes across x64/ARM64 Debug, Release and ASan, zero capability skips, two extra resource fixtures and eight consumer variants. Paired collection succeeded; its publisher and aggregate gate failed on the unqualified measured-base policy. |
| Capture-time menu repair at the same compiled source | Four sent/posted stationary root/submenu regressions and the original Right-arrow case passed focused ASan with zero skips and verified desktop restoration. Existing delivered movement history is preserved. |
| Local complete foreground runs | A newly approved exact-`c1a77e42` Release rebuild and bounded four-scope run pass with zero skips: foreground/focus unchanged and pointer restored, with live focus recorded before confirmation. Earlier Task Manager, Explorer, missing-window and geometry failures remain retained and unattributed. The current pass qualifies its own run only. |
| Earlier parser components | 1,324 pinned CommonMark/GFM semantic expectations, 2,125 independent entity values, Unicode 17 vectors and native/embedded fault/resource checks. These are bounded component results tied to their original source. |
| Earlier image/paste components | Known pixels, codecs, orientation, UIA alternatives, cancellation/device replacement, independent path roles/destinations, exact undo/selection restoration and allocation-failure sweeps. Historical passes do not qualify a later merge. |

All earlier failures, incomplete CI profiles and passing repeats are retained.
The final source cleanup replaces whole-line comments in four files. Removing
those comment lines yields identical source text; executable statements,
headers, tests and build configuration are unchanged. The prior CI receipts remain
attached to their original source; this comparison is not a new native run or
source/build attestation. Rewritten-head CI remains a separate observation.
The earlier feature-run ARM64 ASan failure is the visible-initialization Menu probe: its
healthy query answers false after 12 ms, with the popup still alive but hidden.
The x64 Release lease separately loses child foreground permission after Menu
passes; NativeTextInput does not qualify and foreground restoration fails.
Neither observation establishes a cause. No retry replaces either failure;
the original jobs/artifacts are preserved. The following log-retention repair
requires fresh-source CI qualification.

Two Menu sequences now use fresh bounded foreground attempts: the state-probe
fixture and standard shortcut-column layout. Their controlled regressions cancel
a discovered popup after activation loss, preserve its diagnostic and require
every original assertion on a fresh held attempt. A visible-initialization
regression fails before the repair with the same hidden-popup symptom; this is
not attribution of the earlier untraced ARM64 failure. Hosted warning recovery
also completes before each child launches, reusing the existing authorization,
capture, ownership, balanced-input and restoration checks. Seven added native
self-test checks cover between-child recovery and refusal paths. Local activation
behavior and probe/layout deadlines remain unchanged; final-source validation
is tracked in the implementation plan. The shortcut regression synchronizes
actual destruction before requiring an unavailable layout query; cancellation
alone may still permit a sent query. These harness changes alter no control
pixels or public runtime API, so the documentation gallery requires no new capture.

The launch-failure log wrapper now preserves existing raw bytes in unique
unqualified `.log` siblings before writing the canonical failure marker. Two
retention regressions fail before the fix; all 40 interactive-wrapper tooling
cases pass after it, including missing output, repeated refusals, literal Unicode
paths and a locked-file retention failure. This preserves future diagnostics;
it neither recovers already overwritten output nor fixes foreground admission.
The [implementation plan](../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md)
records source-specific qualification and remaining work. Layout/gallery pixels
are unchanged by the menu input-state repair.

## Performance HOLD

The initial unpaired Default clean-frame p95 flags were +5.02% and +17.05%; that
fixture does not instantiate RichTextEditor. A later 12-block investigation had
a -1.31% paired effect (Holm-adjusted p=1), with drift in all 24 same-binary
controls. Neither result establishes accepted non-regression or causal regression.

| Retained investigation | Finding |
| --- | --- |
| [Main/candidate CI 39568060174](https://github.com/RedSalamanders/DxUi/actions/runs/39568060174), seed `1327683194` | Original 138,880,407-byte ZIP, workflow logs and role executables are sealed. All 144 receipts, both frozen judges and 72 controls replay. Judges agree on metric labels, but 71/72 controls drift (23/24, 24/24, 24/24). The publisher rejects the unqualified measured-base policy. The 5,626-sample observer stream replays separately; neither result establishes accepted non-regression or attribution. |
| [Main/candidate CI 39555699478](https://github.com/RedSalamanders/DxUi/actions/runs/39555699478), seed `981054997` | Original 139,034,543-byte ZIP and both role executables are sealed. All 144 receipts, both frozen judges and 72 controls replay. Judges agree on metric labels, but 70/72 controls drift (22/24, 24/24, 24/24). The publisher rejects acceptance because the measured-base policy is unqualified; diagnostic labels do not establish non-regression. |
| [Main/candidate CI 39463272390](https://github.com/RedSalamanders/DxUi/actions/runs/39463272390) | Exact replay of 144 receipts, both frozen judges and 72 controls; 68 controls drifted. Judges disagree on clean preparation. The measured-base policy is unqualified. |
| [Previous main/candidate CI 39480512299](https://github.com/RedSalamanders/DxUi/actions/runs/39480512299), seed `1202880588` | Original role executables rehashed; all 144 receipts, both judges and 72 controls replay. Judges agree on metric labels, while 65/72 controls drift (21/24, 24/24, 20/24). Required acceptance fails on the unqualified base policy. |
| [Previous main/candidate CI 39500910925](https://github.com/RedSalamanders/DxUi/actions/runs/39500910925), seed `1568326520` | The original ZIP and both role executables are sealed; all 144 receipts, both judges and 72 controls replay. All 72 controls drift. The legacy judge flags MultilineGrid clean composition p95; the migrated judge reports within-noise. Collection succeeds, but the required publisher and aggregate fail on the unqualified base policy. |
| [Same-binary A/A 39468472800](https://github.com/RedSalamanders/DxUi/actions/runs/39468472800), `e0fa1d7a`, seed `1341918500` | One physical build; original ZIP/executable sealed. All 144 receipts and 72 controls replay. Judges agree on metric labels, but 69/72 controls drift (22/24, 24/24, 23/24 across the three scenarios). Diagnostic success is not policy approval. |
| Earlier eight-process boundary pilot, frozen library sources `f65e4a4b` / `e325158a` | All 2,560 frames replay and captures match; same-ranked full-frame Map shares are 97.75%/92.74%. Compiler/linker CPU activity overlaps coarse intervals containing timed windows, so this remains under-load diagnostics. This source pair predates the menu repair and cannot qualify the current PR. |
| Coordinated boundary attempt four, same frozen sources | Four of eight processes complete and independently replay, including 1,280 frames and matching captures. The machine-wide guard refuses process five when a compiler appears. The observer and refusal are retained; the full protocol remains incomplete. A future attempt must start all eight processes afresh. |
| Coordinated boundary attempt five, same frozen sources | All eight fresh processes complete in 327.67 s; all 2,560 frames, source/build identities and original byte bindings replay, with one matching capture hash. Across 314 observer samples there are zero compiler/linker observations and zero other DxUi native-test observations. Full-frame same-ranked Map shares are 96.69%/94.80%. Both peer pauses are released immediately after collection. This completes the bounded coordinated diagnostic, without qualifying the current candidate or performance policy. |

Full-frame medians of window p95 are 0.508/0.546 ms for main/candidate with
timers off and 0.503/0.452 ms with timers on. Each value combines four windows
from two fresh processes; the windows are not four independent trials. The
ordering reverses between modes, so these observations cannot establish candidate
speed or isolated instrumentation overhead. Attempt five's observer collection
median/p95 is 38.98/46.80 ms, maximum sample-start gap 1.081 s and sampled-span CPU
4.41% of one logical core. Missing/inaccessible observations remain explicit;
sampled absence cannot exclude unknown work or activity between samples.

Blocking Map intervals can include deferred work, waits and preemption; they do
not measure GPU duration or scheduler delay. Coarse thread observations cannot
attribute sub-ms frames, and Windows refused kernel scheduler tracing. The current
hosted observer replays 5,903 samples over 6,081.45 s, median/p95 collection
2.46/5.18 ms, sampled observer CPU 0.363% of one logical core and maximum
sample-start interval 9.24 s. The previous
hosted observer replay verifies 5,753 samples over 5,928.18 s, with median/p95
collection 2.40/5.76 ms and sampled observer CPU 0.368% of one logical core.
The maximum sample-start interval is 7.77 s. Earlier observer results remain
retained. These coarse observations establish neither a quiet host nor
physical-host visibility.

The private local-audit reader now preserves exact UTC timestamp strings and
parses them invariantly. Four regression checks cover French/US locales and
malformed child/observer timestamps. The original under-load data produces
identical audit bytes in both locales; the earlier parsing failures remain
retained. This analysis repair changes no native timing or acceptance policy.

Independent policy review in `main`, matched candidate evidence and required
acceptance remain open under the
[performance contract](../../Specs/Core/Core_PerformanceAndResources.md) and
[shared qualification ledger](../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).
No baseline, judge, threshold or exact budget is relaxed. Compiler/SDK/dependency
byte retention is not complete independent evidence closure.
The owner explicitly chose to keep PR #89 draft for independent review on
21 October 2026; no implementation-only readiness exception is approved.

## Evidence retention

Raw logs, receipts, binaries and replay packets remain private or in original CI
artifacts. They are not shipped dependencies. Before removing the PR's 19,074
raw measurement files and 43 changelog fragments, every one of the 19,117 archive
entries was verified against its original Git blob.

- Owner-retained archive: `richtext-details-8fe2d5f5.zip`, 177,483,690 bytes.
- SHA-256: `832534A2F2B448907C0AD0332E7DE853D488CC0DAC01D009D406B6E51D174B9F`.

Current code, tests, usage documentation and gallery images remain in the PR.
Future evidence updates belong in this summary and the owning plans.
