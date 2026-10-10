# Active plans

One entry per plan, separated by blank lines, so that changes to different plans merge without conflicts.

- **ACTIVE**: [Remaining branch integration](BranchIntegration_2026-10-10.md). Review #69 against current lifetime and lazy-publication contracts; retain the unfinished prepared-tree branch and separate performance qualification.

- **ACTIVE**: [Production review and remediation](ProductionReview_2026-10-05.md). Requested ten-day review covers #28-#68, with the earlier #21-#65 review retained for context. Obvious lifetime, keyboard, accessibility and tooling corrections pass local x64 Debug/Release/ASan validation; ARM64 builds, paired measurements, harness gallery and validators are retained. Product/architecture decisions and platform/desktop/consumer qualification remain open.

- **ACTIVE**: [Scoped testing](ScopedTesting_2026-10-05.md). Native test naming, affected iteration, exact local reuse and local/PR coverage coordination across the library and consumers.

- **ACTIVE**: [UI composition improvements for the three consumers](ConsumerUiImprovements_2026-10-04.md). Proposed intrinsic sizing/forms, variable-height lists, overlay sessions, read-only Markdown help, reusable motion and theme authoring, grounded in RedSalamander, RedXe and RedPrism. Includes proposed APIs, staged implementation, resource evidence and separate consumer adoption gates; no new capability is implemented yet.

- **ACTIVE**: [Slider touch thumb and halo](SliderTouchHalo_2026-10-04.md). The library feature merged in #62 and its hosted paired comparison passed before merge (run 37201375166); synthetic window/embedded tests prove device propagation and halo behavior. Physical-touch qualification and Q26's parent/viewport clipping plus neighboring-content/menu overlap checks remain. Consumer API-revision-4 pin and `PointerEvent::device` adoption are tracked separately in `ProductionReview_2026-10-05/api4-consumer-handoffs.md`.

- **ACTIVE**: [Bounded grid text and clipping](GridTextOverflow_2026-09-21.md). Merged to main (#22); gallery regenerated, design system republished and the paired multiline benchmark within the accepted V11 envelope; the multiline-cell verification suites are archived for x64 and passed the recorded native ARM64 Debug/Release/ASan run without capability skips. Resource advice and consumer qualification remain before explicit pin adoption.

- **ACTIVE**: [TreeReorder_2026-09-21.md](TreeReorder_2026-09-21.md). The DxUi library implementation of drag-reorder and multi-select is complete and merged in #35; gallery/design-system evidence is recorded. RedPrism's exact-pin adoption and product verification remain consumer-owned and are tracked in `ProductionReview_2026-10-05/api4-consumer-handoffs.md`; this does not authorize a second tree implementation.

- **ACTIVE**: [Described native menu entries](MenuDescriptions_2026-09-21.md). Merged to main (#24); the memory waiver is removed (30 September) and directed input restoration passed. The consumer pin handoff remains.

- **ACTIVE**: [Fixes kept from the codex branches](CodexBranchReview_2026-10-02.md). The unmerged `codex/*` branches are reviewed against main; the native provider-lifetime gaps and the multiline caret clip are in main (#55) and the focus-callback audit is done (#57); per-entry elements for ordinary menus and the snapshot pre-sizing are in #56, whose measured cost the developer accepted on 2 October, and the wider delegate audit remains.

- **ACTIVE**: [CI run scope](CiRunScope_2026-10-04.md). The validation workflow runs once per change (pushes validate main alone), and a documentation-only pull request skips the six native jobs; the hosted behavior remains to be observed.

Completed records are `../Done/SharedLibraryReadiness_2026-09-09.md`,
`../Done/RedSalamanderMigration.md` and `../Done/EditorConsumerControls_2026-09-20.md`. Further ARM64 and ASan qualification is
user-deferred to RedSalamander's `DxUi_DeferredPlatformQualification_2026-09-13.md`.
RedXe retains its separately owned hardware, real IME and assistive-technology gates.

Plans never override normative contracts. Completed plans move to Done after
validation and normative closeout.
