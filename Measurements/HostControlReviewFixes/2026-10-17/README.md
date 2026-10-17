# Host/control review fixes: local evidence, 17 October 2026

The candidate is the focused `ej/host-control-review-fixes-2026-10-16` working tree on
`99e8b313b6ae68cc52949b113b9867001d605b24`. The [completed implementation plan](../../../Specs/Plans/Done/HostControlReviewFixes_2026-10-16.md)
owns these repairs. No application dependency pin or performance policy changes.

## Source and evidence identity

The frozen compiled-input digest from `Get-ScopedSourceIdentity -CompiledOnly` is
`1021b1b152626859095292ac020eac636218438ae24fa62773dabebc8ec83590`.
The primary checkout and isolated validation checkout have identical compiled/tooling input bytes at that digest.
The latter has the private, local snapshot commit `b96d8fde569c1ca4efc5751cd8cc6fb0680a9511`; it is not a
published consumer pin. Its independently generated build attestations and artifacts retain their own identities.
Later prose and gallery-receipt updates do not change the frozen compiled input digest.

Primary-checkout specification validation encounters pre-existing, untracked measurement downloads that do not
meet the publication contract. Those downloads remain untouched. The isolated checkout contains only the candidate
diff and explicitly enumerated new files, so its validation result must be distinguished from the primary checkout.
Native reports, logs, scoped receipts and build logs are retained in the task's local validation archive; executable
and intermediate build products are not published here.
The thirteen reviewed DxUi-only benchmark receipts/comparisons are retained byte-for-byte beside this page;
local Git attributes preserve their bytes, and [SHA256SUMS](SHA256SUMS) seals them. All five stored comparisons reproduce
exactly from their retained baseline and candidate receipts with `Tools/PerformanceComparison.psm1`.

## Validation

| Obligation | Result |
| --- | --- |
| x64 Debug | All 19 noninteractive suites pass, zero capability skips. Three exact-identity affected receipts are reused; the other 16 suites run at the same final source. |
| x64 Release | All 19 noninteractive suites pass, zero capability skips. |
| x64 ASan Debug | All 19 noninteractive suites pass, zero capability skips. The isolated detection probe catches its deliberate fault as expected. |
| ARM64 builds | Debug, Release and ASan Debug pass. Cross-builds do not qualify native execution. |
| Relocated synthetic consumer | Four runs pass: Debug, Release, ASan Debug and ASan Debug with STL annotations disabled. Each checks public headers, rendering, 12 module probes and 10 rejected pin/build mismatches. Real consumers are unchanged. |
| Gallery | `gallery.ps1 -SkipBuild -Configuration Release -Platform x64 -PublishDocs` succeeds from the candidate build. All six PNGs and both indexes are byte-identical to the retained gallery; the generation receipt is refreshed. All six sheets were inspected. |
| Format | `format.ps1 -Check` and `git diff --check` pass. |
| Validators/tooling | Full isolated `validate.ps1` passes all five validators and the complete tooling suite. Final receipt-publication/checklist edits receive an additional specification/documentation check. |
| Foreground suites | All 12 suite executions pass across x64 Debug, Release and ASan Debug with zero skips: 831 named cases complete. Four completed leases verify restoration and the approved 30-second automatic Start policy. Earlier failed/refused attempts remain separate. |

The noninteractive scope is Foundation, Embedded, Grid, Theme, Control, MenuExitLifetime, NewControls,
EditorControls, TextField, MultilineText, ReadOnly, ComboBox, Tree, Tooltip, Rendering, Animation, Accessibility,
WindowHost and InteractiveLease. The lease suite's self-tests do not constitute a foreground acceptance run.
The existing Embedded fixture checks WARP preparation/composition, clean/dirty/hidden behavior and device replacement;
the Rendering suite separately covers recovery after simulated device loss.

The canonical `Test-Changes.ps1 -Mode PrePush -Explain` selects these nineteen suites plus Tooling and identifies
the four foreground suites as a separate `INTERACTIVE_NOT_RUN` obligation. The subsequent explicitly approved
`test.ps1 -Interactive -InteractiveAutoStart` executions complete that local obligation in each x64 profile.
Together with the noninteractive results, 69 candidate-specific x64 suite executions pass. Noninteractive native
executions use `Test-Changes.ps1` with explicit scopes and each x64 configuration; it delegates to `test.ps1`. ARM64 uses
`build.ps1 -Platform ARM64` in each configuration. Relocated checks use `test-consumer.ps1` against the clean
private snapshot, with separate output roots and the default annotation policy plus the additional ASan variant.

Raw foreground reports, child logs and lease plans/results stay in the private task archive. All twelve successful
receipts independently match their executable SHA256, report zero skips and a completed zero-exit lease, and bind
both requested and native policy to automatic Start with 30 seconds. The successful lease summaries are:

| Profile/scope | Completed cases | Confirmation | Foreground / focus / pointer |
| --- | --- | --- | --- |
| Debug: all four suites | 109 Menu, 166 NativeTextInput, 1 MenuResources, 1 MenuResourceScaling | Start clicked | unchanged / unchanged / restored |
| Release: all four suites | 109, 166, 1, 1 | automatic Start | restored / restored / restored |
| ASan Debug: NativeTextInput | 166 | Start clicked | unchanged / unchanged / restored |
| ASan Debug: Menu and both resource suites | 109, 1, 1 | automatic Start | restored / restored / restored |

ASan uses two complete leases at identical compiled inputs; no partial failed lease contributes a passing result.

| x64 artifact | SHA256 |
| --- | --- |
| Debug ControlTests | `1BC64B91BEB0B88F47BC5D4CC50ABB7CAF36AFBA0E1A31124C146C1B9F7AF2A0` |
| Debug EmbeddedTests | `41F169500A9C6A2FBE98347390F62DC83336FC1C094731119B67C7F5BDAC3131` |
| Release ControlTests | `B0A7621223B194C5A069874935D02FC458AF1FFDFA7C1942939CED73EB273B77` |
| Release EmbeddedTests | `B0A1681FD2C2705DEEDE19C10D123C36A4402E109148E3C5DFA6A089FF6993EE` |
| ASan Debug ControlTests | `F2867FA637780D6A714FA94F7AE13EDFC6FF120AC615D69132A3050BF4564098` |
| ASan Debug EmbeddedTests | `932B1D46E91D8A7559FFFD7F01B69571920E4F6AACD68D97D50DFB08D1C22410` |

The ten new named regressions are registered in the active native inventory:

- `TestPageHostTabTraversalUsesCurrentLogicalChildren`
- `TestPageHostWindowUiaTraversesInvokesAndRetiresPageChildren`
- `TestPageHostEmbeddedUiaTraversesInvokesAndRetiresPageChildren`
- `TestEmbeddedTransitionsUseApplicationClockIncludingZero`
- `TestNativeCaretTicksInvalidateOnlyWhenPixelsChange`
- `TestScrollPanelForwardsSecondaryPressInContentCoordinates`
- `TestScrollPanelModelContextMenusUseScreenCoordinates`
- `TestNativeUnhandledKeysReachApplicationFallback`
- `TestExtractedRadioDoesNotUseItsFormerGroup`
- `TestEmbeddedGraphicsRetirementPreservesApplicationBindings`

They exercise actual UIA client navigation/invocation and provider retirement, forward/reverse Tab traversal,
zero/large embedded epochs, intermediate and final animation state, unchanged native caret ticks, nested logical
scroll coordinates, application key fallback, live/destroyed former radio groups, and application bindings across
replacement/detach of supplied WARP contexts. They do not establish physical input, real IME or assistive-technology
qualification. The graphics regression asserts preserved bindings; it does not measure a driver's internal flushes.

## Retained failures and measurement limits

An initial compiler attempt failed with C1088 when the system temporary volume filled. Compiler temporaries were
redirected to task-owned storage on another volume. Builds/runs whose inputs changed during execution were refused
qualification and remain superseded. The first frozen Debug run failed Tree/WindowHost assertions that required
all keys, including unhandled modifiers, to be swallowed; the updated assertions check fallback and modifier/modality
state separately. The initial isolated validator caught the gallery change's stale reviewed CI-workflow digest.
Those failures are retained separately from successful final-source receipts.

After the completed primary Release run, the original matrix was deliberately stopped during its next ASan build
to relocate remaining outputs away from the space-constrained volume. That interrupted build is not a passing
ASan result. No independently launched process or existing measurement archive was removed.

The first approved ASan foreground lease passes Menu, then NativeTextInput exits zero but records one
interactive-focused-host skip. The lease cannot grant foreground to the next child (error 5), so neither resource
suite runs. Restoration reports foreground failed, focus skipped and cursor failed. The complete failed attempt's
reports, child logs and lease plan/result remain private. A later read-only admission check reports an interactive
desktop available; a fresh retry uses the same compiled inputs. A later pass cannot establish the cause of this
earlier failure or turn it into a completed ASan foreground receipt.

The next ASan attempt is refused before any child starts because the warning cannot take the foreground. Its
state is `unwarned`; foreground, focus and cursor are all verified unchanged. This is a separate pre-child refusal,
not a resource-suite failure or a pass. Its plan/result and not-run receipts remain privately retained. Desktop
readiness is requested before another attempt; no foreground protection is waived.

After readiness is confirmed, a third attempt passes Menu but fails NativeTextInput's hidden-host initial-focus
assertion. Its resource child cannot receive foreground (error 5), so both resource suites remain unrun. Foreground
and pointer restoration succeed and focus is unchanged. This launched assertion failure and the subsequent launch
refusal are retained together, separately from the earlier skip/restoration failure and pre-child refusal.
Fresh ASan leases at the same compiled inputs subsequently complete NativeTextInput alone, then Menu and both
resource suites, with zero skips and verified restoration. Their success does not explain any earlier failure.

Pre-change Release Default, MultilineGrid and MultilineGridDistinct reports were retained before implementation.
Their originals are [Default](baseline-Default.json), [MultilineGrid](baseline-MultilineGrid.json) and
[MultilineGridDistinct](baseline-MultilineGridDistinct.json).
Candidate diagnostics run serially after all native tests and consumer builds finish, using the primary Release
artifact and each scenario's original baseline. The comparator's ordinary single-receipt results are:

| Scenario/run | Diagnostic result |
| --- | --- |
| [Default, first](candidate-Default.json) | [within-noise-budget](candidate-Default.json.comparison.json) |
| [MultilineGrid, first](candidate-MultilineGrid.json) | [advice-required](candidate-MultilineGrid.json.comparison.json): clean private +1.28 MiB, clean peak +1.31 MiB, dirty peak +1.54 MiB |
| [MultilineGridDistinct](candidate-MultilineGridDistinct.json) | [within-noise-budget](candidate-MultilineGridDistinct.json.comparison.json) |
| [MultilineGrid, repeat](repeat1-MultilineGrid.json) | [within-noise-budget](repeat1-MultilineGrid.json.comparison.json) |
| [Default, repeat](repeat1-Default.json) | [within-noise-budget](repeat1-Default.json.comparison.json) |

The first MultilineGrid memory flag remains unexplained; the passing repeat does not erase it or establish a
cause. Its clean/dirty C++ allocation counts remain 0/1080, matching the baseline. All five candidate receipts
record zero composition allocations, a 3,686,400-byte retained surface, zero hidden preparations and zero hidden
composites. These bounded-resource observations do not resolve the process-memory flag. Both original flagged
receipt/comparison and subsequent repeats are retained. No baseline or threshold is replaced.

Every ordinary test invocation also records completed WARP FPS/memory. These diagnostic/unpaired observations
do not establish non-regression. The trusted paired policy remains unqualified; stable controls and candidate-specific
paired qualification remain with the shared execution owner. There is no accepted performance verdict here.

## Plan reconciliation and remaining acceptance

All seven previously missing Done-index entries are restored. TreeReorder's completed library scope is archived;
consumer pins and current-source qualification remain separate. MenuDescriptions and GridTextOverflow retain
their actual outstanding acceptance work in WIP. CiRunScope still needs hosted observation.
The WIP index places review, CI observation and the shared qualification owner before the deferred S3-S7 proposals.

The [shared qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#shared-qualification-ownership-16-october-2026)
is the single execution owner for policy/performance, native-platform and physical-input/IME/AT qualification,
while each candidate retains its own identities. It also explicitly records the unverified wider clock audit;
the repaired PageHost/Slider cases do not establish a library-wide epoch-independent contract.
