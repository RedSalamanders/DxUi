# Active plans

One entry per plan, separated by blank lines, so that changes to different plans merge without conflicts.

- **ACTIVE**: [MSVC 14.52 and Release optimization](Msvc1452ReleaseOptimization_2026-10-23.md). Five-repository compiler/linker audit, explicit Release function elimination and full LTCG, matching pinned-archive policy, and measured before/after qualification.

- **SPEC READY**: [RichTextEditor file drag-and-drop](RichTextFileDrop_2026-10-20.md).
Displayed image references and relative links for other files reuse the paste
policy. Unicode OLE ownership, guarded caret insertion and native/embedded tests
are specified; implementation and runtime qualification have not started.

- **ACTIVE**: [Owned Unicode rich-text editor](RichTextEditor_2026-10-17.md). Core, retained preview, source
TSF/IME/UIA ranges, Unicode 17 segmentation and parser M1-M3 are functionally qualified.
M4 bounded semantic rendering, source caret sites, hanging markers, inert links,
native HTML subset, source-exact structural editing and rich style/structure UIA
are functionally qualified. M5 extensions follow the C++ only, limited Mermaid
subset decision. M5/M6, remaining D2a work and physical/platform/consumer
acceptance remain open. Performance acceptance stays on HOLD. The 21 October
resume checkpoint records five passing native profiles and the remaining ARM64
Debug B2-hover failure; its original artifacts are retained.
Leading YAML/TOML source envelopes and source-current UIA diagnostics now pass
focused Debug checks; metadata validity and remaining extensions are in progress.

- **SPEC READY**: [RichText audit remediation and completion](RichTextAudit_2026-10-21.md). A verified audit of
the rich-text engine records one memory-safety defect, contract violations in failure containment, admission,
math ownership, input and host paste, stale capability status, a non-incremental semantic pipeline, and the
Markdown, math and Mermaid completeness gaps. Waves 0 to 5, a test track and owner-scoped completion tracks
sequence the work; no library code has changed. The parent rich-text plan keeps its own gates.

- **ACTIVE**: [Menu fixture popup readiness](PopupReadiness_2026-10-17.md). Registered hover input now precedes pending popup paint; a deterministic paint-stream case fails against the old library and passes with the priority fix. PR #85's post-merge run completes 110/110 Menu cases and both hover regressions in all six native profiles, with zero skips. Original untraced Debug/ARM64 failures remain unattributed; ARM64 focus restoration and shared qualification remain open in the qualification ledger.

- **ACTIVE**: [Production review and remediation](ProductionReview_2026-10-05.md). Requested ten-day review covers #28-#68, with the earlier #21-#65 review retained for context. The approved automatic-Start candidate has six builds and 69 passing x64 suite executions: 57 noninteractive and all four focus suites in Debug, Release and ASan Debug, with zero native skips and verified restoration. Earlier failures, refusals and cancellations remain separately retained; fresh passes do not prove their causes. The repaired v2 A/A repeat has all 72 controls unstable; policy/required-CI acceptance, native ARM64, physical-input and consumer qualification remain open.

- **ACTIVE**: [CI run scope](CiRunScope_2026-10-04.md). The validation workflow runs once per change (pushes validate main alone), and a documentation-only pull request skips the six native jobs; the hosted behavior remains to be observed.

- **ACTIVE**: [Library qualification and consumer adoption](ConsumerUiQualification_2026-10-11.md). Single owner of shared qualification; the API-4 register owns consumer actions. PR #88's exact post-merge run passes six native profiles, eight enabled public-consumer checks and 128 zero-skip suite receipts (Menu 113/113 and NativeTextInput 169/169 per profile), with both intentional ASan probes detected. Three shared ARM64 leases again report foreground-minimized/none, leaving focus restoration unverified; approved ordinary-host/native ARM64 observations with recorded initial focus remain necessary. PR #87 evidence retains its original source identity. Green push CI does not qualify the skipped paired gate or unchanged qualified=false policy. Earlier failures, attribution gaps, timing/memory flags, MultilineGrid uncertainty and retention limits remain. All 20 qualification HOLDs, deadlines and thresholds stay unchanged.

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
