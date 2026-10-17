# Host and control review fixes

Status: **DONE**, 17 October 2026, for the approved library repairs and candidate-specific local validation.
Baseline: `99e8b313b6ae68cc52949b113b9867001d605b24`. Shared performance-policy, native-platform,
physical-input/IME/AT and consumer acceptance remain open with their owning ledger.

## Scope and checklist

- [x] Verify the reported paths and retain pre-change x64 Release Default, MultilineGrid and
  MultilineGridDistinct receipts under `.build/reports/host-control-review-2026-10-16/`.
- [x] Traverse PageHost logical children consistently for Tab, UIA navigation, paths, hits and labels.
- [x] Anchor embedded PageHost/Slider animation to the application's first tick, including epoch zero.
- [x] Keep native caret ticks clean between visual changes; preserve animation completion invalidation.
- [x] Forward ScrollPanel right-button input in content coordinates and preserve scrollbar/capture behavior.
- [x] Release embedded graphics without native swap-chain ClearState/Flush on the supplied context.
- [x] Return unhandled native keys to the application/default window procedure.
- [x] Guard RadioButton group lifetime and ownership after extraction/reparenting.
- [x] Make source-filter checks non-vacuous and regenerate the CI documentation gallery before upload; update
  the reviewed workflow digest used by PrePush accounting.
- [x] Add behavior regressions, run affected tests, and complete x64 Debug/Release/ASan and ARM64 build coverage.
- [x] Receive this candidate's foreground acceptance from the
  [shared execution owner](../WIP/ConsumerUiQualification_2026-10-11.md#hostcontrol-candidate-acceptance).
  This is a closeout dependency; native ARM64, physical-input/IME/AT and trusted policy gates remain separate.
- [x] Update owning contracts, usage documentation, changelog, gallery assessment and required validators/format.
- [x] Restore the seven missing Done-index entries; audit individual plan closure; assign shared qualification
  execution to ConsumerUiQualification while retaining candidate-specific evidence and consumer handoffs.
- [x] Compare retained baseline/candidate diagnostics and record their limits, including the first MultilineGrid
  memory flag and passing repeat; shared qualification retains that unresolved uncertainty.
- [x] Move this plan to Done after its implementation, normative updates and candidate-specific local validation,
  including the foreground dependency, are satisfied.

## Validation and evidence boundaries

Use `Test-Changes.ps1 -Explain` and affected iteration. Exercise actual UIA client navigation, native key fallback,
caret invalidation counts, nonzero scroll offsets, detached radios, epoch-zero embedded frames, and preserved
application pipeline state across replacement/detach. Existing WARP/device-loss/clean/dirty/hidden tests remain required.
New native files use `Scope.Tests.*` names and the active inventory. No consumer pin is changed.

Shared policy qualification, native ARM64 execution, physical input/IME/AT and consumer adoption are owned by
[ConsumerUiQualification](../WIP/ConsumerUiQualification_2026-10-11.md), with source-specific receipts required there.
No diagnostic comparison qualifies the currently unqualified performance policy. Focus-taking suites require
agreement to the time through the existing interactive lease; noninteractive passes cannot close that obligation.

These fixes preserve static appearance and control geometry. Review existing documentation gallery pixels;
the CI publication correction regenerates docs from its own x64 Release build. No design-system token changes
or baseline replacement are intended.

## Plan reconciliation

The seven pre-existing Done files missing from the index are now linked with their actual scope. TreeReorder's
implementation checklist is complete and its only feature adoption is already consumer-owned; its library record
is archived in Done, with historical clipping limits labelled and later qualification linked to the shared owner.
MenuDescriptions retains final-correction directed/resource/paired acceptance obligations. GridTextOverflow retains
publication/adoption and integrated resource acceptance. Those two plans stay WIP; a completed implementation is
not evidence that their remaining acceptance passed. CiRunScope's hosted observation also remains unclaimed.

## Iteration receipts

The first compiler attempt failed with C1088 because the system temporary volume filled. Compiler temporary files
were redirected to task-owned storage on a volume with space; no existing evidence or unrelated output was removed.
A subsequent compiled build was refused an attestation after a tooling input changed during compilation; that
attempt is not a qualification result. Qualification builds freeze compiled/tooling inputs throughout the build.

The first frozen x64 Debug noninteractive run passed 17 suites and all nine new regressions, while Tree and
WindowHost stopped at historical assertions that every modifier/unconsumed key-down was handled. Those assertions
are corrected to require application fallback while separately checking modifier state and keyboard modality.
The initial isolated validator run reached tooling but rejected the now-stale reviewed CI workflow digest; that
digest is refreshed only after reviewing the gallery-only workflow change. Original failures remain in the task's
private failed-iteration archive; later passes do not erase or reattribute them. New native caret test setup uses
the native epoch, and secondary release is allowed to return unhandled after a button opens its context action on press.

Final test audit gives Slider and PageHost independent attachments so each epoch-zero scenario has a monotonic
timeline. The caret test first admits the current phase, then checks identical-timestamp cleanliness and a full
blink-period flip, avoiding dependence on focus setup latency. Receipts from superseded test inputs are not reused.

The clock correction remains the requested PageHost/Slider scope. A wider source audit found additional legacy
clock callers; their independent reproduction is explicitly assigned to the shared ledger, and public guidance
does not claim universal epoch independence from these two controls' passing tests.

The restored secondary dispatch also requires content-to-screen context-anchor conversion. Base controls and
Tree/Grid delegates now subtract every logical ancestor ScrollPanel offset, including across PageHost. The
regression covers pointer/keyboard anchors, nested scrollers and both model delegates; forwarding alone is not
reported as a complete context-menu repair. The final regression inventory contains ten named tests.

Primary-checkout spec validation also reports pre-existing untracked measurement downloads as publication-invalid.
They remain untouched. A separate checkout receives only this candidate's tracked diff and explicitly enumerated
new files for complete validators/tooling; source equality and its result must be recorded at closeout.

The [local evidence packet](../../../Measurements/HostControlReviewFixes/2026-10-17/README.md) tracks final-source
results separately from superseded iterations. Final Debug and Release each pass all nineteen noninteractive
suites with zero capability skips and all ten new named regressions. Gallery regeneration succeeds; every PNG and
both indexes remain byte-identical, with a refreshed generation receipt and all six sheets reviewed. The original
matrix is intentionally stopped after Release during its next ASan build to relocate remaining outputs; the
interrupted build is not reused. At that checkpoint the foreground suites awaited agreement to the time;
the approved foreground outcomes below retain their independent acceptance boundary.

All six x64/ARM64 configuration builds pass at the frozen inputs. Four relocated x64 public-consumer fixtures
pass against the clean private snapshot: Debug, Release, ASan Debug and ASan Debug without STL annotations.
Native ARM64 execution and application pins are separate obligations; neither is inferred from these checks.
Final ASan Debug also passes all nineteen noninteractive suites with zero skips: 57 suite executions across the
three x64 profiles. Its detection probe catches the deliberate fault in isolation. The extracted-radio regression
passes with both a surviving former group and a destroyed former group under the instrumented build.

Serial Release diagnostics complete for all three retained-baseline scenarios. Initial Default, Distinct and
the Default/Multiline repeats are within the ordinary comparator's budget, while the first MultilineGrid result
flags about 1.28/1.31/1.54 MiB of clean private/clean peak/dirty peak memory. All originals remain retained;
the repeat does not explain the flag. Composition allocations, surface bytes and hidden work remain bounded in
all five receipts. Trusted non-regression stays unqualified, with execution and attribution owned by the shared ledger.

Complete isolated `validate.ps1` passes all five validators and every tooling test. Formatting checks and
`git diff --check` pass. The thirteen reviewed baseline/candidate/comparison files are published byte-for-byte in
the evidence packet with a SHA256 seal; all five comparisons reproduce exactly. Final publication/checklist edits
are checked again by the specification/documentation validators without re-running unchanged native tests.
The person subsequently approves automatic Start on 17 October. Debug and Release each pass Menu,
NativeTextInput, MenuResources and MenuResourceScaling with zero skips and verified foreground/focus/pointer
restoration or unchanged states. Their artifact-matched receipts and lease plans/results are retained privately.
ASan's first lease records a focused-host skip, cannot grant foreground to the next child and fails desktop
restoration. Its next lease is refused before any child starts because its warning cannot take foreground;
all desktop states remain unchanged. Those two attempts remain separate failures/refusals, with no source change
or cause attributed from another profile's pass. After desktop readiness is confirmed, a third attempt fails
NativeTextInput's hidden-host initial-focus assertion and cannot launch the resource child; its desktop states
are restored or unchanged. That failure is also retained without causal attribution.

With identical compiled inputs, fresh complete ASan leases pass NativeTextInput separately, then Menu and both
resource suites, with zero skips and verified restoration. Across Debug, Release and ASan, all twelve foreground
suite executions complete all 831 named cases; all four successful leases verify foreground/focus/pointer states
as restored or unchanged and retain the approved 30-second policy. This gives 69 passing candidate-specific x64
suite executions including the 57 noninteractive runs. Raw lease diagnostics remain private. The shared owner
records this candidate's local foreground dependency complete; this repair plan moves to Done. The earlier failures
remain unexplained, and shared performance-policy, native-platform and physical/product qualification stay open.
