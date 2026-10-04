# Active plans

One entry per plan, separated by blank lines, so that changes to different plans merge without conflicts.

- **ACTIVE**: [UI composition improvements for the three consumers](ConsumerUiImprovements_2026-10-04.md). Proposed intrinsic sizing/forms, variable-height lists, overlay sessions, read-only Markdown help, reusable motion and theme authoring, grounded in RedSalamander, RedXe and RedPrism. Includes proposed APIs, staged implementation, resource evidence and separate consumer adoption gates; no new capability is implemented yet.

- **ACTIVE**: [Slider touch thumb and halo](SliderTouchHalo_2026-10-04.md). The slider takes the visible thumb of the touch review and shows a translucent 48 DIP halo while a touch contact drags it; hosts report each pointer event's device. The design system is republished with it; the hosted paired benchmark and consumer adoption remain.

- **ACTIVE**: [Bounded grid text and clipping](GridTextOverflow_2026-09-21.md). Merged to main (#22); gallery regenerated, design system republished and the paired multiline benchmark within the accepted V11 envelope; the multiline-cell verification suites are archived for x64 and pass on CI's native ARM64 jobs. Consumer qualification remains before explicit pin adoption.

- **ACTIVE**: [TreeReorder_2026-09-21.md](TreeReorder_2026-09-21.md). Drag-reorder and multi-select are both in `Tree` (#35); the gallery is regenerated and the design system republished with them. RedPrism's pin bump remains. Not a second tree in the app.

- **ACTIVE**: [Described native menu entries](MenuDescriptions_2026-09-21.md). Merged to main (#24); the memory waiver is removed (30 September) and directed input restoration passed. The consumer pin handoff remains.

- **ACTIVE**: [Fixes kept from the codex branches](CodexBranchReview_2026-10-02.md). The unmerged `codex/*` branches are reviewed against main; the native provider-lifetime gaps and the multiline caret clip are in main (#55) and the focus-callback audit is done (#57); per-entry elements for ordinary menus and the snapshot pre-sizing are in #56, whose measured cost the developer accepted on 2 October, and the wider delegate audit remains.

- **ACTIVE**: [CI run scope](CiRunScope_2026-10-04.md). The validation workflow runs once per change (pushes validate main alone), and a documentation-only pull request skips the six native jobs; the hosted behavior remains to be observed.

Completed records are `../Done/SharedLibraryReadiness_2026-09-09.md`,
`../Done/RedSalamanderMigration.md` and `../Done/EditorConsumerControls_2026-09-20.md`. Further ARM64 and ASan qualification is
user-deferred to RedSalamander's `DxUi_DeferredPlatformQualification_2026-09-13.md`.
RedXe retains its separately owned hardware, real IME and assistive-technology gates.

Plans never override normative contracts. Completed plans move to Done after
validation and normative closeout.
