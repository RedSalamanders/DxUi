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
- [ ] Run x64 Debug/Release/ASan tests and all three ARM64 builds; native support needs native receipts.
- [ ] Compare paired performance/resources; preserve every failed/noisy attempt without rebaselining.
- [x] Update controls documentation/domain contract and regenerate/review gallery; run validators/format (V11).
- [ ] Qualify publication and explicit consumer pin adoption; retain the old consumer pin until qualified.
- [ ] Move this plan to Done after its own gates pass. The previous localized-layout plan stays Done.

## Checkpoint

Current: **V11**, Debug and Release Rendering pass. The added color-emoji witness failed
against V9 (new DrawTextLayout path omitted ENABLE_COLOR_FONT), then passes with V10/V11.
V9 shares immutable ellipsis by format; V11 also shares identical derived visible layouts.
The cache stays bounded to 32 entries; visible source text and copy/UIA remain complete.
V5's broad 16-suite x64 Debug/Release/ASan and three ARM64 cross-build receipts precede the
optimizations and must not be represented as final V11 qualification.

**Resource acceptance is blocked pending developer advice and retention evidence.** The exact
Release original/candidate pair (`dxui-grid-baseline-Release-*` / `dxui-grid-v11-release-*` under
`C:/RedSalamander.Perf/evidence/i26-ui`) measures multiline dirty throughput 61.974 -> 122.243 FPS,
private bytes 33,214,464 -> 40,226,816 (+7,012,352), working set 46,895,104 -> 49,016,832.
C++ allocations remain 1,080 per 40-frame round; composition allocations remain zero;
surface/replacement storage stays 3,686,400 bytes. Private peak rises 33,267,712 -> 41,103,360.
The common fixture also flags private memory and timing: dirty FPS 550.345 -> 517.364,
private bytes 28,655,616 -> 29,360,128. Preserve these flags; no non-regression claim.
Developer advice was requested asynchronously: qualify the measured memory tradeoff with
retention/budget evidence, or keep optimizing. No answer/approval is inferred while waiting.
No publication or consumer pin adoption has occurred.

The original-production worktree `Z:/src/DxUi-worktrees/i26-grid-baseline` (detached c52a8f5)
contains only the same four benchmark-harness changes. Original Debug/Release/ASan binaries
were built and measured; all raw results remain, including older invalid harness-hash pairs.
The V6 eight-report ABBA crossover showed original multiline 59/52 FPS versus 120/122, but
35.4/35.9 MB versus 43.9/43.5 MB private bytes. V7 offscreen release and V8 skipping invisible
paragraph shaping reduced the cost; V11 still has a confirmed memory increase. Do not repeat
unchanged measurements hoping to select a favorable result or change comparison thresholds.

Next: the identical formatted harness now has `-Scenario MultilineGridRetention` (not yet built):
six passes through 1,000 rows, 200-frame resource samples, model-clear and host teardown.
Run original then V11 Release serially to distinguish warm caches from ongoing growth. This
adds a fixture fingerprint; do not compare new harness reports against old hashes. After the
resource direction is resolved, rerun final broad x64/native ARM64 CI, interaction restoration,
validators and gallery. The external focus wrapper's restoration improvement is still uncompiled;
prior Debug foreground=0 remains unaccepted. Preserve warning/lease/restore protocol.
Consumer min-row failing-witness run is in progress independently. Keep its qualified pin 78b3de3.

Earlier V5 checkpoint:

2026-09-21, V5: implementation and focused functional verification pass. All 16 non-activating
suites pass in x64 Debug and Release, zero skips. NativeTextInput passes both profiles, zero skips.
Initial Menu runs have nine desktop skips each; a corrected external harness wrapper establishes
owned foreground through the existing warning/lease and reruns Menu in both profiles with zero
skips. Debug wrapper restoration reported foreground=0; Release reported foreground=1. Preserve
that limitation; the wrapper must distinguish absent/closed prior targets and verify restoration.
The wrapper is external orchestration, not a DxUi dependency. No independent application was killed.

Original pixels: 71 ink rows both unclamped and clamped. V1/V2 initially passed a weak row-count
check, but a stronger V3 omission-marker comparison failed: DirectWrite height clipping alone
does not mark omitted explicit paragraphs. V4/V5 freeze the measured Unicode line boundaries,
append an ellipsis to the final visible line and use horizontal trimming without further wrapping.
V5 tests actual omission pixels, invariant partially clipped cells, full copy values, and retained
versus fresh layouts after text/font/width/height/clamp changes, including a 5,000-unit value.
Single-line cells retain their previous drawing path. The multiline cache has 32 slots with
4,096 UTF-16 units per retained string; larger inputs use temporary layouts. Performance acceptance
and targeted multiline retention/embedded evidence remain open.

Performance: retain every report under `C:/RedSalamander.Perf/evidence/i26-ui` (`dxui-grid-*`).
Initial baseline is `dxui-grid-baseline-debug/Performance-x64-Debug-e804afbffa99481d877b2c6ca0568ac2.json`.
V1 regressed dirty FPS 455.5 to 303.9 and private bytes 31.3 MB to 45.0 MB; rejected. Limiting the
new path to multiline cells removes that large cost. V2/V3/V5 still have investigation flags;
V5 against the separately retained original-production repeat has dirty working set 44,908,544
to 45,813,760 bytes and compose p95 0.023 to 0.030 ms. Baseline itself varies in private bytes and
FPS. No threshold or baseline has been replaced, and no non-regression pass is claimed.
Next: retain immutable baseline executables for serial diagnostic pairs, qualify all configurations,
and investigate the resource deltas rather than repeatedly rebuilding or selecting favorable runs.

All five regenerated theme sheets and the independent embedded image were directly reviewed;
the changed French grid summary shows an omission marker. Docs/domain contract updated. No new
consumer pin, publication, native ARM64 acceptance or overall I26 closeout is claimed.

V11 retention result (same new harness on both implementations): original six passes
109.66 s, private 29.65–30.42 MB; candidate 55.75 s, private 31.52–44.62 MB, median
36.41 MB versus 30.01 MB. Handles 213 throughout / 211 detached and surface 3,686,400
bytes in both. Candidate sawtooth private usage is not a proven stable long-run bound.
Reviewed records are under `Measurements/GridTextOverflow/2026-09-21/`; advice remains
pending and no memory waiver is recorded. Proceeding with final functional qualification.

Final production V11: 16/16 non-activating suites pass in each x64 Debug/Release/
ASan Debug profile, zero skips; ASan detection probe passes. All three ARM64 cross-
builds pass. `qualification/` beneath the reviewed measurement record retains
fresh per-suite/executable/performance receipts and ARM build logs. All six gallery
images regenerated and directly reviewed. No native ARM64/runtime CI claim yet.
V11 input attempt 1: Debug Menu executed assertions pass, seven desktop skips;
child focus/foreground/cursor restoration all pass. External wrapper V2 grants
foreground to its suspended owned child before resuming, with bounded job lifetime;
run that changed wrapper next, using warning/lease/restoration. Resource advice
remains pending. Consumer `e925bf7f8` closes the minimum-row functional slice (72/72)
and is independently building the save-failure/recovery/reopen placement fixture.

Local checkpoint before commit: specs/skills/dependencies/format validators pass after
qualifying the archive provenance. Raw per-suite receipts remain externally unchanged;
reviewed copies add their linked benchmark's ownership/input hashes and original receipt
hash. All five theme sheets and the embedded image are directly reviewed. Input V2 did
not launch the child (handoff exit 11), but restored focus/cursor. V3 now diagnoses each
handoff API and allows only an owned-child timeout fallback if nested jobs are unavailable;
it is uncompiled/unrun. Preserve V1's seven skips and V2's startup failure. Consumer failed-
save fixture V1 rejected a valid absent/default=true setting; V2 corrects that assertion
and includes disk failure/recovery, both placement keys and popup reopen (currently building).
