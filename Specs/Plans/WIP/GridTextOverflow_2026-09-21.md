# Bounded grid text and clipping

Status: **ACTIVE**. Owner: the File Operations implementation task, independent library slice.
Base: qualified main `78b3de389a189c7f86f611787e0489fb6d474218`, plus its documentation-only
closeout cherry-picked as `c52a8f5`. Other owners' checkouts and changes remain separate.

The consumer's French Issues grid exposed a generic defect: `SetLineClamp` enables wrapping but
does not limit visible lines or produce an omission marker. Partially visible cells also lay out
against their clipped rectangle, changing text placement as they cross the viewport boundary.

## September 30 verification

The verification item of the Execution list below is closed by 16 new tests in four suites. Each paints a scenario and a
fresh twin in the same window or view (one device, so the pixels compare exactly) and never compares stored pixels; a
scenario and its twin differ in the one thing the test is about. The
[x64 Debug, Release and ASan Debug runs](../../../Measurements/GridTextOverflow/2026-09-30/verification/README.md) of the
Grid, Rendering, Accessibility, Embedded and MultilineText suites are archived with their logs and receipts, zero capability
skips. Native ARM64 execution of the new tests is not claimed: they build in all three ARM64 configurations and the next
native CI run executes them.

- Grid: `TestGridCopyOfTrimmedMultilineCellsIsExact` (Ctrl+C and `OnCopy` of cells the paint trims, holding CR LF,
  U+2028/U+2029, a zero-width-joiner emoji, a 5,000-unit word, decomposed accents, Arabic and trailing separators, are exact
  unit for unit across rows and columns and in the display order of reordered columns; the tooltip of each cell equals its
  value, which proves it trimmed) and `TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` (the real
  tables at the 16,384-entry ceiling: growth stops there, a full set gives up a least recently used way, one of the previous
  paint's and none of the current one's, and a paint that uses under an eighth of a table halves it, down to 32 entries).
- Rendering: `TestGridMultilineDecomposedAccentsPaintLikePrecomposed` (e plus U+0301 paints the pixels of the precomposed
  letter at every clamp, for 420 and 100,000 units, and the tooltip keeps the decomposed text);
  `TestGridMultilineShapedPrefixCutInsideAClusterPaintsLikeItsShortTwin` (a 100,000-unit value whose shaped prefix ends
  inside a surrogate pair, a zero-width-joiner sequence or a letter and its marks, at every alignment, paints the pixels of a
  short twin; the shaped units show the surrogate guard stepping back in exactly the alignments that cut a pair);
  `TestGridMultilineRightToLeftFlowKeepsMarkerSideAndClipping` (marker side, clipping and tooltip in a grid whose flow is
  right to left); `TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue` (clamps 1, 2 and 3: ink inside
  the cell, the ellipsis after the lines DirectWrite cuts, the tooltip);
  `TestGridMultilineCellCutByTheViewportPaintsAShiftedCropOfItsWholeSelf` (whole-row, whole-DIP, scrollbar-dragged fractional
  vertical and fractional horizontal offsets: what survives of a cut cell is a shifted crop of the unscrolled capture, so
  nothing reflows); `TestGridMultilineRepaintsAtANewDpiLikeAFreshGrid` (144 and 192 dpi and back: the repaint equals a fresh
  grid's, the layouts being in DIPs and the log showing the change creating none);
  `TestGridMultilineRepaintsAfterThemeFontAndDensityChangesLikeAFreshAttach` (light, dark and a real
  high-contrast palette lay nothing out; another font family or size, or compact density, lays out again; each step equals a
  fresh attach); `TestGridMultilinePrefixSharingNeverCrossesTextThatLaysOutDifferently` (values that share a start but differ
  where shaping stops, inside the prefix or after a paragraph that ends it, never share a layout, including past the 4,096
  hashed units); `TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction` (the tables stay at a lowered
  ceiling, paint the pixels of the default ceiling after eviction, recover reuse when it is raised and halve when use drops);
  `TestGridMultilineTrimmedCellsPaintTheSameAfterDeviceLoss` (the pixels before and after each of several losses are equal
  and no layout is made again); `TestGridMultilineMovedBetweenHostsMatchesAFreshOne` (a grid moved to a host with another
  device, dpi, theme and density arranges and paints its cells like one created there, through the shared moved-control
  fixtures in `Tests/Controls/DxUiTestMovedControls.h`).
- Accessibility: `TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues` (Name, Value and ValuePattern of trimmed
  cells, a 100,000-unit one included, equal the model's units, in a left-to-right and a right-to-left grid) and
  `TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport` (a cell the viewport cuts keeps its whole Name and Value
  and its bounding rectangle is the viewport-clipped cell, which a point in it resolves to, at five scroll offsets and under
  a dragged scrollbar thumb; a cell scrolled out says it is offscreen and has no rectangle).
- Embedded: `TestEmbeddedMultilineGridFrenchCells` (long French cells at clamp 2 in 64-DIP rows on a WARP device, read back:
  the marker follows the second line and no third line paints, a wrapped cell paints fewer rows at clamp 2 than at 3; hide and show and
  replacing the device, shown or hidden, reproduce the pixels; 144, 192, 96 and 192 dpi equal a fresh view, the change
  creating no layout).

Every new test failed under at least one temporary mutation of the library: 27 mutants, 33 pairs of a mutant and a test, in
[the archived table](../../../Measurements/GridTextOverflow/2026-09-30/verification/README.md#falsification) with the assertion
that failed. The mutations were reverted and none is committed. Two limits are recorded there: a layout table is
pixel-transparent by design, so the Rendering ceiling test cannot fail on the eviction order (the Grid test does), and the
surrogate guard is visible only in shaped units, never in pixels, so its test asserts units.

Findings. The library needed no fix. The Grid does not react to its `FlowDirection` (`DxUi.Grid.cpp` never reads it, and the
test's log shows a right-to-left grid painting the pixels of the same grid in a left-to-right flow): the columns and cells of
a right-to-left grid are not mirrored, and only the text's own direction places the omission marker, which is what the test
asserts in a right-to-left grid. Mirroring a Grid would be a new feature with its own contract, not a defect of this slice.
The performance contract said an entry the current or previous paint used is never evicted; that holds only while a table can
grow, and at the ceiling a full set gives up its least recently used way (the Grid test pins it, on a 64-entry table running
the same code as at 16,384), so `Core_PerformanceAndResources.md` now says that. Layouts are in DIPs, so a dpi change creates none and a repaint equals a
fresh grid; theme changes create none; font or density changes create them again. The vertical scroll offset rests on whole
rows except while the scrollbar thumb is dragged, so the fractional-offset check uses a dragged thumb. One hook was added for
the ceiling test, `Grid::DebugSetTextLayoutEntryLimit` (and its getter), diagnostics only, with the production limit
unchanged at 16,384; the API revision stays 2. Docs and the gallery were reviewed: no behavior or pixel changes, so nothing
is regenerated. Remaining: native ARM64 execution of these tests, native assistive-technology acceptance, and the consumer
items below.

## September 27 gallery and paired benchmark

Gallery capture no longer depends on the desktop size (`9fe19cc`). The x64 Release CI job regenerated
the gallery from `217e602`, and all five sheets were reviewed and published. They show the clamped French
multiline cell with its ellipsis directly after the last visible character. In the high-contrast sheet
the Grid tile's IconText and ColorSwatch rows show no text. The previously published sheet shows the
same, so this work did not cause it. The gallery's high-contrast input supplies no alert colors, which
became transparent tone colors; the [gallery follow-ups](../Done/GalleryFollowUps_2026-09-28.md) fall back to
`text` on `windowBackground`. A second CI run at
unchanged gallery inputs reproduced five sheets byte for byte. Rainbow light differed only in the
ProgressBar / Indeterminate tile, whose animated segment depended on capture timing. The same
follow-ups make that tile static.

A [hosted paired benchmark](../../../Measurements/GridTextOverflow/2026-09-27/paired-hosted/README.md)
compares `6f769ab` with the merged grid in one A1/B1/B2/A2 set per scenario. Multiline dirty private
memory rises by 4,202,496 and 3,072,000 bytes against the V11 envelope's 7,012,352, with a dirty peak
increase of at most 7,327,744 bytes. Dirty throughput improves by 70–75%. Allocations and surfaces
are unchanged, and Default memory does not move. This meets the paired-benchmark condition for
applying the accepted V11 envelope to this revision; the record states the hosted-runner limitation.

The design system is republished with the updated Grid and MenuBar guidelines, previews and
stylesheet, and the five regenerated theme sheets are re-uploaded with new asset records.

## September 27 main merge

Main `6f769ab` (#27's disclosure UIA setup allowance and #24's described native menus) is merged
into this slice. Both branches changed the complex-UI benchmark. The merged harness keeps every
multiline scenario and main's `--benchmark-retention` mode. Every fixture now records main's
memory phases; the retention fixtures take their hidden phase after the scroll passes. Grid heap
walks call main's shared `Tests/Support/HeapDiagnostic.h`, which samples every heap before writing,
and `performance.ps1` hashes that helper as a benchmark input. Harness hashes therefore differ from
every earlier grid receipt, so the paired multiline benchmark must measure both sides on this
harness. The merge changes C++ test code, so it needs its own native CI run; the pull request
records the result. Neither branch regenerated the gallery; one regeneration after both covers them.

## September 26 review fixes

Main `c36413b` (#23, #25, #26) is merged. A static review of the pull request found three
multiline defects; each is fixed with a focused test. These commits were written without a
Windows toolchain. Native CI on `e5a9c4d` then passed all six profiles, validation and the format
check in runs `36230288334` and `36230290122`; no gallery or benchmark has run on them yet.

- A multiline cell too short for one complete line painted nothing; the 20-DIP minimum row and
  the Compact default row leave less than one Body line. The first line now paints, centred and
  clipped to its text area like a single-line cell (`TestGridMultilineShortRowsPaintClippedFirstLine`).
- The tooltip ignored height clamping and measured the viewport-clipped rectangle. Paint and the
  tooltip now share one prepared full-cell layout that records omission; painted text hidden by a
  horizontally scrolled viewport still offers it (`TestGridMultilineTooltipFollowsPaintedLines`).
  On 27 September the developer confirmed this horizontal-scroll behavior, which matches
  single-line cells.
- Trailing CR/LF, U+2028 or U+2029 produced a false ellipsis or a half-line offset, and the
  ellipsis followed trailing whitespace (`TestGridMultilineTrailingSeparatorsMatchTrimmedTwin`).
- The clamp no longer selects the cache slot, so the stale-cache test's new font-only and
  clamp-only steps fail on a missing key comparison. Oversized text is no longer copied.

The gallery's clamped French cell breaks at a space, so its ellipsis now follows the last word
directly; the post-merge gallery regeneration below must follow these commits. A paired multiline
benchmark must pass on them before the accepted V11 envelope is applied to this revision.

## September 25 main merge

Main `57ba237` (design system, editor consumer controls, Tree reorder) is merged into this slice for
review. The duplicate localized-layout closeout merged cleanly. The gallery sheets and receipt take
main's 30-control generation, so the published grid clamp pixels must be regenerated on the merged tree.
The Grid design-system guideline and static preview now describe complete-line clamping; the design
system still needs republishing. Python skills/specs/dependencies/test-port/build-matrix validators and
tool tests pass on the merged tree. No native build, test, gallery or benchmark ran for this merge; the
pull request's six native CI profiles also run the new UIA witness in x64 ASan and all ARM64 profiles.

## September 23 developer decision

The user explicitly answered **"Accept the grid memory tradeoff"** to the measured
6–7 MiB process-memory increase for readable multiline Issues rows, with roughly doubled
offscreen throughput. This accepts that recorded V11 grid cost only; it does not accept
the rejected associative cache, menu benchmark differences, or unrelated regressions.
Keep the original paired receipts and thresholds. Combined library qualification and
explicit consumer adoption remain required; no pin changes follow from this decision alone.

A focused Accessibility witness has been added after reviewing the existing Rendering
coverage: a genuinely shortened French/Unicode multiline cell must expose the complete
original text through its UIA Name, Value property and ValuePattern without changing selection.
The borrowed model detaches on normal and exceptional exit. The rebuilt V11 x64 Release
Grid, Rendering and Embedded suites pass; the corrected Accessibility suite passes without
skips. The first witness omitted `NotifyDataChanged` after direct selection mutation and
failed; that run is retained alongside the correction in the
[focused packet](../../../Measurements/GridTextOverflow/2026-09-23/uia-full-value-release/README.md).
Both executions use the same library binary. Their automatic benchmarks are explicitly
unpaired. The new test still needs the remaining final configurations and does not replace
native assistive-technology acceptance.

The [x64 Debug follow-up](../../../Measurements/GridTextOverflow/2026-09-23/uia-full-value-debug/README.md)
rebuilds V11 and passes Grid, Rendering, Embedded and Accessibility with zero capability skips.
Its exact source/driver and executable receipts are retained; the automatic benchmark is unpaired.
New-witness x64 ASan and ARM64 configuration qualification remains open; prior production receipts
are not presented as execution of a test that did not yet exist.


## September 23 cache experiment closeout

The consumer confirmation and waiting/Issues slices are committed as `26209bac` and `a3777b3f0`.
The latter has 424 captures, 48 transition/36 static geometry witnesses and fresh Release PR 824/0/0.
The 32-entry associative-cache experiment is **rejected for adoption and restored to base V11**.
It eliminates collisions in the new 32-cell warm fixture but increases matched private memory.
No original resource gate, consumer pin, threshold or six-profile qualification has changed.

Both final policies use `build.ps1 -Rebuild`, with identical test/helper/benchmark bytes and
actual Grid compilation. The direct-map fixture records 28 hits, 36 misses and 72 layout
creation attempts, then fails the expected warm-reuse assertion (exit 1). The candidate's
supplementary Grid/Embedded/Rendering tests pass, zero skips; Rendering records 64 hits,
zero misses and zero layout creations, equal repeated pixels and clean host teardown.

The mandatory paired candidate `test.ps1` remains failed: dirty private bytes rise
28,643,328 -> 29,659,136 (+1,015,808 bytes, 3.55%), dirty peak rises 28,717,056 ->
29,683,712, and clean preparation p95 0.0003 -> 0.0004 ms is also flagged. Functional
passes do not waive these results. Initial matched heap counters likewise show less
churn but increased process memory. Sol High independently audited the rebuilt receipts;
root checked its conclusions before rejecting/restoring the experimental implementation.

Earlier source restores preserved old timestamps, leaving a candidate Grid library linked
into original-header tests. The first `warm32-direct-map`, later `direct-map-regression`
and `direct-map-common` results are invalid as original-policy evidence. The first warm32
run exited 0xC0000005 during teardown; no stack attributes causality. A separate borrowed
model lifetime hazard was corrected before rebuilding both policies. The initial 14:05
direct-map compilation and 14:06 heap baseline remain valid. The candidate driver also
copied stale original suite receipts after the performance gate stopped it; those receipts
are quarantined and explicitly excluded. Every failed/invalid/redundant attempt is retained.

[Reviewed packet, exact patches and hashes](../../../Measurements/GridTextOverflow/2026-09-23/assoc-cache-rejected/README.md)
contains the complete scope and next action. Five experimental code/test files and their
provisional contract paragraph are restored to HEAD `0ae8362`; both archived patches apply
cleanly to that base. **Current source is V11; current Release binaries are the rejected
candidate and require a rebuild before reuse.** No restored-source execution is claimed.
Docs/gallery were reviewed: existing V11 public behavior and pixels remain unchanged, so
only performance documentation and evidence links need updates for this rejected variant.

Next: complete remaining menu resource advice and integrated consumer acceptance, while
continuing independent current-pin Debug validation. Do not repeat rejected variants or
misapply V11's previous six-profile passes to the rejected associative implementation.

## Contract

Honor the configured line limit and available complete-line height, using DirectWrite ellipsis
trimming without modifying model values, copy, tooltip or UIA text. Clip partially visible cells
without reflowing them. Preserve Unicode shaping, selection, hit testing, themes and both hosts.
Reuse bounded text-layout resources; clean/hidden composition adds no work. No filesystem semantics.

## Execution

- [x] Retain unchanged-production pixel witness and paired complex-UI performance baseline.
- [x] Implement complete-line trimming and stable full-cell layout with bounded resource reuse.
- [x] Verify actual pixels, long French/Unicode text, copy/UIA, narrow/short cells, clipping,
  mutation/resize/font changes, cache bounds and independent WARP/device-loss/lifecycle suites.
  The 30 September tests add what the earlier ones did not cover: copy, UIA values and clipped bounds, Unicode clusters at
  the shaped prefix, right-to-left flow, dpi, theme and font changes, prefix sharing, the ceiling, embedded French cells,
  device loss and moved hosts (the verification section above). x64 Debug, Release and ASan Debug runs are archived; native
  ARM64 execution of the new tests awaits the next CI run.
- [x] Run x64 Debug/Release/ASan tests and all three ARM64 cross-builds.
- [x] Obtain native ARM64 Grid/Embedded/Rendering/Accessibility qualification in all three profiles;
  nine unrelated Menu desktop-capability skips per profile remain explicitly unqualified.
- [x] Compare paired performance/resources; preserve every failed/noisy attempt without rebaselining.
  The V11 cost was accepted on 2026-09-23 and recorded in Core_PerformanceAndResources.md; the
  associative-cache variant stays rejected.
- [x] Update controls documentation/domain contract and regenerate/review gallery; run validators/format (V11).
- [x] After the main merge, pass the six native CI profiles and regenerate/review the gallery with
  `gallery.ps1 -PublishDocs` (27 September).
- [x] Republish the design system with the updated Grid preview and regenerated gallery (27 September,
  artifact version 6).
- [ ] Qualify publication and explicit consumer pin adoption; retain the old consumer pin until qualified.
- [ ] Move this plan to Done after its own gates pass. The previous localized-layout plan stays Done.

## Checkpoint

Latest native qualification: published `7827873` passes all six native CI profiles
after one unchanged x64 Debug Accessibility classification retry. Retain the first
three-second `ElementFromHandle` setup timeout as an intermittent failure; no
code repair is claimed. Grid/Embedded/Rendering/Accessibility have zero skips
on x64 and ARM64 Debug/Release/ASan. ARM64 Menu has nine explicit interactive
desktop skips in each profile. [Reviewed receipts and exact limitations](../../../Measurements/GridTextOverflow/2026-09-21/native-ci/README.md).
This closes the native grid runtime gate, not original/candidate resource
acceptance or product/AT/DPI qualification. The user challenged the separate
menu memory explanation and has approved neither memory tradeoff. No pin changed.

Latest 2026-09-21: **native-call isolation finished; all experiments rejected and
V11 restored**. Suppressing glyph submission and omitting `SetTrimming` separately
do not establish a safe resource fix. A bounded single-layout inline-marker
prototype passes Release Grid/Embedded/Rendering after the existing pixel test
catches an invisible marker over a separator-only range. Its matched paced
retention median is 37,914,624 private bytes, versus retained V11 36,831,232 and
original 29,743,104; peak 45,936,640. No improvement or resource acceptance is
claimed. [Exact patches, failed/passing tests and receipts](../../../Measurements/GridTextOverflow/2026-09-21/native-isolation/README.md).

Restored V11 passes a fresh Release rebuild and all three focused suites, zero skips.
Production/harness/gallery inputs remain those of `2ba0dfb`. This follow-up is
evidence/documentation only; no consumer pin changed. Do not repeat these variants
unchanged or mistake the previous unbounded inline-tail sample for a qualified fix.
Return to Astra High for independent RedSalamander Full/configuration acceptance
on its qualified pin while this library's resource advice/native-platform gates
remain open. Additional resource work needs a new measurable hypothesis or the
developer's explicit scoped tradeoff, not more arbitrary cache/layout substitutions.

Previous 2026-09-21: **native heap attribution implemented and qualified; production V11 unchanged**.
The opt-in `MultilineGridHeap` / `MultilineGridHeapPaced` diagnostics report bounded
process-local heap occupancy outside timed rounds. All heap walks in eight retained
runs finish without errors. The original/V11 unpaced median live-heap delta is
1,227,109 bytes; free heap space grows by 9,456,936 bytes. This supports substantial
allocator retention/fragmentation, not a call-stack-level attribution or resource waiver.
Equal 50-fps retention pacing does not remove the growth. WPR setup was denied with
0x80070005 before launch; no trace/configuration was installed. The independent menu
candidate's cost is not explained by this grid-only evidence.

Four additional experiments are retained as patches, all reverted: per-paint layout
release, DrawTextW, complete cache release, and a single-layout inline-tail prototype.
The last passes Release Grid/Embedded/Rendering with zero skips, but its paced private
median 34,004,992 still exceeds original 29,743,104 (V11 36,831,232). It initially
measures all input paragraphs, a hidden-tail cost requiring review. It is a candidate
for further analysis, not an accepted optimization. Do not repeat the failed variants.
[All receipts, patches, hashes and precise limits](../../../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md).

The final diagnostic-only code with V11 restored passes Grid/Embedded/Rendering in
x64 Debug/Release/ASan Debug (nine suite passes, zero skips, ASan detection probe),
and all three ARM64 cross-builds. Production/gallery inputs are unchanged. Native
ARM64 execution and original paired resource acceptance remain open.
Skills/specs/dependencies/format checks, 10 performance-comparison tool tests,
77 raw archive hashes and exact summary reproduction pass.

**Previous next action, completed above: targeted Astra Extra High review** of the unresolved native allocation cost
and saved inline-tail prototype. Use the current diagnostic packet, retained baseline
worktree and targeted DrawCellText reads. Preserve all baselines; no consumer pin or
other-owner checkout changes. Return to the plan's High/Sol routing after this difficult
failure is resolved. At most one immediate continuation, no competing writers/builds.

Previous 2026-09-21 resource experiment: **eight cache slots rejected and reverted**.
Fresh 32 -> 8 -> restored-32 Release retention measures dirty FPS 122.065 -> 95.418
-> 121.133; private medians 33,918,976 -> 34,605,056 -> 33,613,824 bytes across six
passes. Common-path timing also varies in the middle run, so preserve noise/causality
limits. Grid/Embedded/Rendering pass for the experiment, zero skips. No production
code change remains, no baseline is replaced, and the resource gate stays open.
[Complete evidence](../../../Measurements/GridTextOverflow/2026-09-21/cache8-rejected/README.md).
The companion consumer result slice is committed as `45f2c97ba` (87 focused Debug
checks, exact inline-rename/Issues results, TSV/JSONL, reviewed captures). Consumer
pin remains `78b3de3`; remaining P3 and final integrated gates proceed independently.


Current production: **V11, local commit `71d6446`**. The 32-slot bounded multiline
layout cache preserves complete Unicode model/copy/UIA values, enforces complete
visible lines with ellipsis and clips full-cell layouts without reflow. Single-line
cells retain their previous rendering path. No consumer or other-owner checkout changed.

**Functional qualification:** all 16 non-activating suites pass in each x64 Debug,
Release and ASan Debug profile (48 passes, zero skips), including the ASan detection
probe. All three ARM64 cross-builds pass. Menu and NativeTextInput additionally pass
in all three x64 profiles (six passes, zero skips), with verified original child
focus, foreground and cursor restoration through the external warning/lease harness.
Reviewed receipts: `Measurements/GridTextOverflow/2026-09-21/qualification/`.
Native ARM64 runtime CI remains open. No consumer/AT/native mixed-DPI claim is made.

**Resource acceptance remains blocked pending developer advice.** The matched
Release multiline fixture measures 61.974 -> 122.243 completed offscreen FPS,
private bytes 33,214,464 -> 40,226,816 (+7,012,352), working set 46,895,104 -> 49,016,832.
Per-round C++ allocations remain 1,080; composition allocations are zero;
surface/replacement storage remains 3,686,400 bytes. The common fixture flags
timing/memory too: dirty FPS 550.345 -> 517.364, private 28,655,616 -> 29,360,128.
No thresholds, baseline or failed attempts have been replaced.

The matched Release retention fixture is now implemented and executed: six passes
through 1,000 rows, samples every 200 frames, model clear and host teardown.
Original/candidate durations are 109.66/55.75 seconds. Original private bytes range
29,650,944–30,420,992; candidate 31,522,816–44,621,824, median 36,411,392 versus
30,005,248. Handles remain 213 and drop to 211 after detach in both. Candidate
private usage is sawtoothed and ends high; a settled long-run bound is not proven.
No heap purge or working-set trim was used. Developer advice was requested:
qualify the tradeoff with retention/budget evidence, or keep optimizing. No answer
or resource waiver is inferred. Publication and consumer pin adoption stay open.

All five gallery theme sheets and the embedded image were regenerated with
`gallery.ps1 -PublishDocs` and directly reviewed. Skills/specs/dependencies/format
validators pass. One trailing-whitespace line in the archived ASan log was noticed
after the local commit; its reviewed copy is normalized and documented, while the
external raw log remains unchanged. That correction and the interactive receipts
are committed in `725e83c`.

Next: resolve the resource direction, obtain missing native qualification, then
publish/adopt only an explicitly qualified pin and rerun consumer visual acceptance.
The matched ASan pair is now retained under `qualification/asan-paired/`: the common
fixture is within noise bands; multiline dirty FPS improves 26.639 -> 50.089, but
clean-frame memory/timing flags remain. No resource waiver is inferred.
Original-production worktree `Z:/src/DxUi-worktrees/i26-grid-baseline` is detached at
`c52a8f5` (production tree of qualified `78b3de3`), with only four identical benchmark
harness files changed. Its current Release and rebuilt ASan executables match the
current harness; Debug predates that harness and needs rebuilding before new pairs.
Do not mutate a tested checkout during a native run or reuse mismatched fingerprints.

Historical failures remain retained under `C:/RedSalamander.Perf/evidence/i26-ui`:
V1 all-cell cache regressed speed/memory; V3 exposed missing explicit-paragraph
ellipsis; V9 exposed missing color-emoji rendering; V10/V11 fix those pixel defects.
V6 ABBA, V11 paired runs and retention reports are reviewed under
`Measurements/GridTextOverflow/2026-09-21/`. Input V1 skipped seven desktop checks;
V2/V3 failed the pre-startup foreground handoff. V4 fixes its ordering and qualifies
all six input runs with restoration. These failures are not erased or counted as passes.

The consumer minimum-row slice is committed as `e925bf7f8` (72/72 Debug checks).
Its save-failure/recovery/reopen placement fixture is independently being qualified.
Its qualified DxUi pin remains `78b3de3`; the previous adaptive-layout plan stays Done.
