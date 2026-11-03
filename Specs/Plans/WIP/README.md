# Active plans

One entry per plan, separated by a blank line, so that changes to different plans merge without conflicts.
Status labels and current plan status lines follow [the specification authority](../../README.md#specification-authority).

- **ACTIVE**: [Diagram spacing and task presentation](DiagramSpacingAndTaskPresentation_2026-11-03.md). Correct label-aware arrow corridors and text insets; theme task markers and adopt an optional completed-task strike-through setting in RedTilde.

- **ACTIVE**: [Complete Update-stage metric accounting](FrameUpdateAccounting_2026-10-26.md). PR #112 and combined SDK702 pass all six hosted native profiles; FrameUpdate source/tests remain byte-identical to the AEB checkpoint. The earlier PR109 Release Menu-fixture failure remains historical and unattributed. Performance acceptance remains open; earlier SDK407 diagnostics retain their incomplete Update-field boundary.

- **ACTIVE**: [Library qualification and consumer adoption](ConsumerUiQualification_2026-10-11.md). The current delivery packet records source-specific native, SDK, compatibility and measurement results. Native ARM64 execution is user-deferred; independent performance-policy, physical input, hosted and real-consumer acceptance remain open in this ledger.

- **SPEC READY**: [Production review of the five-day delivery](ProductionReview_2026-11-02.md). Verified red-team review of `a0f67fd3..6df17007` (PR #159; `src`/`include` unchanged on `5fc4d4c3`). One P0 (embedded TextField use-after-free); P1 release blockers in the rich editor, input, Markdown, diagrams and SDK notices, plus closure gaps in earlier reviews. Wave 0 decisions D0-D2, D9 (redact and rewrite history) and D10 (qualification branches pushed) are taken; code implementation has not started.

- **HOLD**: [UI composition proposals S3-S7](ConsumerUiImprovements_S3_S7_2026-10-11.md). Overlay sessions, variable-height lists, read-only Markdown help, motion and theme authoring are deferred proposals requiring separate scope acceptance. Completed S0-S2 library implementation is recorded in Done; these later stages remain unimplemented.

Completed records are `../Done/SharedLibraryReadiness_2026-09-09.md`,
`../Done/RedSalamanderMigration.md`, `../Done/EditorConsumerControls_2026-09-20.md`,
`../Done/EmbeddedInteractionLayout_2026-10-06.md` and
`../Done/PreparedTreeAccessibility_2026-10-06.md`. Further ARM64 and ASan qualification is
user-deferred to RedSalamander's `DxUi_DeferredPlatformQualification_2026-09-13.md`.
RedXe retains its separately owned hardware, real IME and assistive-technology gates.
