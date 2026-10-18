# Active plans

One entry per plan, separated by blank lines, so that changes to different plans merge without conflicts.

- **ACTIVE**: [Menu fixture popup readiness](PopupReadiness_2026-10-17.md). Registered hover input now precedes pending popup paint; a deterministic paint-stream case fails against the old library and passes with the priority fix. The candidate has 57 noninteractive suite passes, six builds and 110/110 full Menu cases in each x64 profile, with zero skips and verified restoration. Original untraced Debug/ARM64 failures remain unattributed; fresh native ARM64 observation and shared qualification remain open.

- **ACTIVE**: [Production review and remediation](ProductionReview_2026-10-05.md). Requested ten-day review covers #28-#68, with the earlier #21-#65 review retained for context. The approved automatic-Start candidate has six builds and 69 passing x64 suite executions: 57 noninteractive and all four focus suites in Debug, Release and ASan Debug, with zero native skips and verified restoration. Earlier failures, refusals and cancellations remain separately retained; fresh passes do not prove their causes. The repaired v2 A/A repeat has all 72 controls unstable; policy/required-CI acceptance, native ARM64, physical-input and consumer qualification remain open.

- **ACTIVE**: [CI run scope](CiRunScope_2026-10-04.md). The validation workflow runs once per change (pushes validate main alone), and a documentation-only pull request skips the six native jobs; the hosted behavior remains to be observed.

- **ACTIVE**: [Library qualification and consumer adoption](ConsumerUiQualification_2026-10-11.md). Single execution owner for shared performance-policy, native ARM64/resource and physical-input/IME/AT gates; source-specific evidence remains separate, and the API-4 register owns consumer actions. PR #84 post-merge ARM64 Debug Menu fails the hover-switch assertion; the separately reproduced paint-starvation path is repaired locally, while fresh native observation remains open. Unrecorded ARM64 lease focus, performance flags/retention limits, and every remaining policy/resource/physical/product gate and threshold are preserved.

- **ACTIVE**: [Scoped testing](ScopedTesting_2026-10-05.md). Native test naming, affected iteration, exact local reuse and local/PR coverage coordination across the library and consumers.

- **ACTIVE**: [Slider touch thumb and halo](SliderTouchHalo_2026-10-04.md). The library feature merged in #62 and its hosted paired comparison passed before merge (run 37201375166); synthetic window/embedded tests prove device propagation and halo behavior. Physical-touch qualification and Q26's parent/viewport clipping plus neighboring-content/menu overlap checks remain. Consumer API-revision-4 pin and `PointerEvent::device` adoption are tracked separately in `ProductionReview_2026-10-05/api4-consumer-handoffs.md`.

- **ACTIVE**: [Bounded grid text and clipping](GridTextOverflow_2026-09-21.md). Merged to main (#22); gallery regenerated, design system republished and the paired multiline benchmark within the accepted V11 envelope; the multiline-cell verification suites are archived for x64 and passed the recorded native ARM64 Debug/Release/ASan run without capability skips. Resource advice and consumer qualification remain before explicit pin adoption.

- **ACTIVE**: [Described native menu entries](MenuDescriptions_2026-09-21.md). Library implementation is merged; later corrections still retain source-specific directed/resource/paired qualification obligations in addition to the consumer handoff. Reconcile those obligations against the shared qualification ledger before closing the plan.

- **ACTIVE**: [Fixes kept from the codex branches](CodexBranchReview_2026-10-02.md). The unmerged `codex/*` branches are reviewed against main; the native provider-lifetime gaps and the multiline caret clip are in main (#55) and the focus-callback audit is done (#57); per-entry elements for ordinary menus and the snapshot pre-sizing are in #56, whose measured cost the developer accepted on 2 October, and the wider delegate audit remains.

- **HOLD**: [UI composition proposals S3-S7](ConsumerUiImprovements_S3_S7_2026-10-11.md). Overlay sessions, variable-height lists, read-only Markdown help, motion and theme authoring are deferred proposals requiring separate scope acceptance. Completed S0-S2 library implementation is recorded in Done; these later stages remain unimplemented.

Completed records are `../Done/SharedLibraryReadiness_2026-09-09.md`,
`../Done/RedSalamanderMigration.md`, `../Done/EditorConsumerControls_2026-09-20.md`,
`../Done/EmbeddedInteractionLayout_2026-10-06.md` and
`../Done/PreparedTreeAccessibility_2026-10-06.md`. Further ARM64 and ASan qualification is
user-deferred to RedSalamander's `DxUi_DeferredPlatformQualification_2026-09-13.md`.
RedXe retains its separately owned hardware, real IME and assistive-technology gates.

Plans never override normative contracts. Completed plans move to Done after
validation and normative closeout.
