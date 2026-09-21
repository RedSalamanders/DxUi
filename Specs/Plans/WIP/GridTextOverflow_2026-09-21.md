# Bounded grid text and clipping

Status: **ACTIVE**. Owner: the File Operations implementation task, independent library slice.
Base: qualified main `78b3de389a189c7f86f611787e0489fb6d474218`, plus its documentation-only
closeout cherry-picked as `c52a8f5`. Other owners' checkouts and changes remain separate.

The consumer's French Issues grid exposed a generic defect: `SetLineClamp` enables wrapping but
does not limit visible lines or produce an omission marker. Partially visible cells also lay out
against their clipped rectangle, changing text placement as they cross the viewport boundary.

## Contract

Honor the configured line limit and available complete-line height, using DirectWrite ellipsis
trimming without modifying model values, copy, tooltip or UIA text. Clip partially visible cells
without reflowing them. Preserve Unicode shaping, selection, hit testing, themes and both hosts.
Reuse bounded text-layout resources; clean/hidden composition adds no work. No filesystem semantics.

## Execution

- [x] Retain unchanged-production pixel witness and paired complex-UI performance baseline.
- [x] Implement complete-line trimming and stable full-cell layout with bounded resource reuse.
- [ ] Verify actual pixels, long French/Unicode text, copy/UIA, narrow/short cells, clipping,
  mutation/resize/font changes, cache bounds and independent WARP/device-loss/lifecycle suites.
- [x] Run x64 Debug/Release/ASan tests and all three ARM64 cross-builds.
- [ ] Obtain required native ARM64 qualification; cross-builds are not runtime receipts.
- [ ] Compare paired performance/resources; preserve every failed/noisy attempt without rebaselining.
- [x] Update controls documentation/domain contract and regenerate/review gallery; run validators/format (V11).
- [ ] Qualify publication and explicit consumer pin adoption; retain the old consumer pin until qualified.
- [ ] Move this plan to Done after its own gates pass. The previous localized-layout plan stays Done.

## Checkpoint

Latest 2026-09-21: **native heap attribution implemented and qualified; production V11 unchanged**.
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

**Next: targeted Astra Extra High review** of the unresolved native allocation cost
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
