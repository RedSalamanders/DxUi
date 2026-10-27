# Read-only Source snapshot synchronization

## Checklist
- [x] P18 review correction: the retained native parser probe at `282da2b1` returns exit 1: reasserting the current adopted snapshot leaves a superseded request pending. The repair cancels that staging without allocating or replacing the immutable wrapper; a regression now asserts both conditions. This was outside the ordinary mirror path. The earlier Release/input results remain named `282da2b1` checkpoints; final qualification must use the repaired revision.
- [x] P18 preserve the source47a9 qualification and merge receipts; the parsed-data extension has its own exact API-4 evidence below.
- [x] P18 settle API revision 4 and admit the exact source-coordinated MarkdownSyntax header change. MarkdownSyntax grows 16 bytes; no binary-interoperability or stable-binary-ABI claim follows from this source/profile probe.
- [x] P18 implement allocation-free exact SourceSnapshot text comparison and optional adoption of completed canonical syntax with a target-local revision, fresh independent document namespace and flattened immutable data owner. Ordinary parsing remains the fallback; existing snapshot synchronization behavior stays unchanged.
- [x] P18 qualify parsed-data access/refusal, idempotent adoption and supersession, owner retirement, Cancel/Clear/Begin fallback, Unicode/CRLF boundaries and allocation failure. Local x64 Full and hosted native evidence for exact source `4077f57d` includes parser/editor allocation-failure and native/embedded accessibility regressions; publication remains atomic.
- [x] P18 implement a callback-free read-only Source editor adoption API; stage before changing state, preserve source/selection/history, and guard retirement at the final scheduling boundary. Reuse retains no publisher control, host, worker or graphics resources.
- [x] P18 qualify consumer wiring on the app owner thread before mirror preparation, using weak lifetime checks and exact source/profile validation. App `30fb0c2` at exact SDK4077 passes six canonical builds, all three x64 Full profiles and authorized keyboard/restoration checks. Shared-storage, independent local revision, incompatible-mode fallback and hidden reveal cases pass; native hosted app ARM64 remains open under its billing restriction.
- [x] P18 final API-4 qualification: exact local source `4077f57df8207eac6c39c43437feb45b19d23cf8` passes all six x64/ARM64 Debug, Release and ASan Debug builds; all 19 ordinary suites pass in each x64 Full profile, the real ASan detector is positive, and four authorized foreground suites pass with restoration verified. ARM64 local rows are cross-builds only.
- [x] P18 hosted source/native qualification: workflow `40286504840` is triggered by head `4077f57df8207eac6c39c43437feb45b19d23cf8`; measured PR #107 merge candidate `646b257a4bbf6fdcf252d137d121e2d3ac246d07` has the same tree `18be66885916f67cafd8aeb761207bf15fbd9120`. PR #107 is merged as `3bcf7ed9d0aaa6afb14495f0e0a03232b6d3bfea` with that same tree. All six hosted native profiles and eight scoped external-consumer reports pass. The overall CI gate is not green because the paired base policy is unqualified. See the [public evidence packet](../../../Measurements/RedesignCompletion_2026-10-26/ci-4077f57d/native-and-paired/README.md).
- [x] P18 complete SDK-side regressions, counterfactuals, six local builds, all three x64 Full profiles, ASan detection, four authorized foreground suites and hosted native execution on exact source tree `18be6688`. Keep paired performance acceptance open; source/build/native success is not a green overall CI or performance result.


- [x] Retain the pre-change native artifacts and exact merged source; production `src`/`include` at `619dc88b` matches the qualified SDK pin `2fed8246`.
- [x] Implement immutable source sharing with target-local revisions and the existing reset/selection notification semantics.
- [x] Pass both Release native/embedded source, history, Unicode boundary, UIA, source-ink parity, source-owner release and callback-retirement regressions; the ordinary-copy counterfactual fails exactly at the sharing assertion.
- [x] Pass six local builds and ordinary x64 Full Debug/Release/ASan, including the real ASan detector. Hosted run `40253024676` passes all six actual native profiles, including ARM64, 126 zero-skip scope reports, 738 Menu completions, 12 new API case completions and both ASan detectors. Six ZIP digests and 496 canonical manifest streams are independently replayed. The native provider archives retain no PE/PDB; ARM64 initial focus remains unavailable.
- [x] Execute final local input at exact clean `47a9d259`: Menu (123 named cases), NativeTextInput (172), MenuResources and MenuResourceScaling all pass with zero skips after the authorized 30-second default-Start countdown. Independent and lead replay check 17 payload streams, including four preserved prior receipts, and exact executed control/lease binaries. Foreground, focus and pointer are unchanged. This is not physical IME/AT or paired performance evidence.
- [x] Execute exact-pin RedTilde adoption: final native source `fc6c62d` passes six builds, all three x64 Full profiles (65 behavioral cases and 23 captures each) and nine keyboard stages. The declared 576-process comparison at unchanged native bytes uses all nine common inputs and an explicitly reviewed two-header profile against the actual PR15 base. Admission and independent arithmetic pass; the consumer retains four original-UI flags and its hosted ARM64 billing hold separately.
- [x] Push and merge SDK PR #105 as `6c3205cf`; its tree equals reviewed `47a9d259`. This integration checkpoint does not accept performance costs or inconclusive CI policy.
- [x] Publish the final native/input and paired evidence and folded changelog in PR #106, merged as `5a6ea951` with the reviewed `7e34c817` tree. All required documentation PR checks pass; native and paired jobs are correctly skipped by its scope. Later main run `40270999389` at `5a6ea951` completes with five native passes and an ARM64 ASan Menu right-arrow assertion failure; exact SDK47a9 qualification remains separate. Logs/provider artifacts are retained; the failure is not yet root-caused. See [26 October evidence](../../../Measurements/RedesignCompletion_2026-10-26/README.md).
- [ ] Close performance acceptance: the PR #107 hosted study has 78 rows with no flagged regressions, but 62/72 controls are unstable and 163/1,872 observations exceed bands. The base policy is unqualified, so the paired verdict and CI gate remain inconclusive/failed on policy. See the [native and paired evidence packet](../../../Measurements/RedesignCompletion_2026-10-26/ci-4077f57d/native-and-paired/README.md). No performance gain is claimed; RedTilde original-UI acceptance remains a separate consumer gate.
- [ ] Keep PR #108 Menu test follow-up and desktop-lease restoration failures separate from PR #107 qualification. In workflow `40309866176` (PR #108 head `6d8aa6e2` over base `3bcf7ed9`), x64 Debug fails `TestMenuHoveringSiblingClosesOpenSubmenuAfterDelay`; the trace does not establish the cause. ARM64 Release passes Menu, then its interactive lease exits 25 because cursor restoration is unverified. Preserve both outcomes and their limits; neither is repaired or qualified by the API-4 merge. The [Menu popup-readiness plan](PopupReadiness_2026-10-17.md) remains the separate owner.
- [ ] Move this plan to Done after its required gates pass; preserve parent performance/platform holds.

## Scope and contract

Focused continuation: the first Release case reached the bitmap check but its hidden fixture could not capture;
retain that failed run. The fixture now uses the existing non-activating display path and includes source-owner
release coverage. A separately built ordinary-copy counterfactual fails exactly at the source-sharing assertion;
its binary, receipt, source and failed evidence are retained. The restored implementation passes both filtered
native/embedded cases with no skips in run `20261026T010425257Z-47844-6abe4fba5b874e6186e9496e0bcf03f2`.
The negative run is `20261026T004418071Z-35148-fb86cdab43b34cef8b9db39fc656ada0`. The initial capture-setup
failure remains separately retained. The suite's unpaired frame timings do not qualify performance.

`RichTextEditor::SynchronizeReadOnlySourceSnapshot` lets a configured read-only Source control share an immutable
`SourceSnapshot`. It changes no class data layout, virtual method or API revision, and introduces no dependency.
Changed source clears only the target history, advances its own revision, retires the same derived state as
`SetText`, then publishes selection through the ordinary host path. The target parser remains independent.
Same-source selection uses the normal selection reset semantics; identical source/selection is idempotent.
Invalid snapshots or scalar/CRLF selection boundaries and incompatible target modes refuse atomically.
Like other setters, calls belong to the control owner thread; detached and embedded controls need no private HWND.

The RedTilde parent plan owns consumer policy, hidden-view deferral, callback supersession and performance
qualification. A successful library build or green hosted profile does not close the consumer's performance,
physical IME/AT or hosted ARM64 gate. The original source-snapshot API remains independent; the separately qualified optional P18 extension shares completed immutable syntax. No total-memory or performance improvement is claimed before paired qualification.

## Validation and publication

Use affected testing while editing. Final qualification uses the canonical six builds and all three x64 Full
profiles, a real ASan detection probe, ordinary authorized foreground suites, exact-head hosted ARM64 and public
consumer compilation. Source glyph pixels must match an independent `SetText`/`SetSelection` oracle; a separate
plain-ink capture proves the comparison actually includes colored syntax. UIA must report exact read-only source.
Keep all failed attempts, counterfactual artifacts and source/receipt identities. No tolerance or baseline changes.

Gallery/design-system review: this additive source-storage API adds no control or intended visual change. Existing
design tokens and gallery captures stay valid; the native glyph parity regression guards that boundary.

## P18 parsed-data reuse extension

The fixed 16-process consumer diagnostic preserves the colored mirror in its control. Removing only its
Markdown-profile/rendering configuration reduces Split owner cycles by about 9.89%, versus about 0.63% in
Compose. Both blocks have the same sign. This is descriptive feature-removal evidence, not an accepted
optimization: the disabled variant fails the existing source-color and independent-revision checks; the
restored full control passes all 46 Editor cases. All nine harness inputs and compiler profiles match after
an initial raw header newline mismatch was refused and repaired before timed execution.

Keep the visible feature. The extension shares only immutable completed grammar data. A wrapper publishes
its own target revision and fresh document identity while forwarding all data access to one flattened owner;
old readers remain immutable and no control is retained. Ordinary Begin after borrowed publication starts a
fresh identity correlation domain. Noncanonical source-editing envelopes, incompatible profiles/extensions,
wrong text and stale source snapshots refuse atomically. The existing source-snapshot API retains its current
independent parsing contract; consumers explicitly opt into the new derived-data API.

Source equality must walk existing piece-tree fragments without flattening or allocation. The editor API must
return ordinary refusal for unavailable/incompatible data and preserve the existing resource-error channel.
The app adapter owns the relationship between its primary and mirror on one UI thread; the SDK stores no
latent publisher pointer or callback. Reuse runs before mirror preparation, then normal preparation supplies
source ink, literal layout and the independent fallback. No new dependency or language family is introduced.

The public MarkdownSyntax layout changes, so exact pins, headers, consumers and native profiles require new
qualification. Resolve the API revision policy and versioned comparison profile before publication. A previous
API47a9 pass, successful compilation or 16-process diagnostic cannot qualify the extension or close performance.
### P18 focused checkpoint

The new parser adoption case and both Window/Embedded read-only parsed-snapshot accessibility cases pass in
x64 Release. They verify exact shared AST storage with local revision/document identity, preserved literal
geometry/scroll/source/selection/history, UIA/text export and byte-exact colored pixels against independent
parsing. This is three focused cases, not Full qualification. The valid-empty and final-owner Clear tests
also pass in the parser case. An exhaustive observed allocation-failure sweep now uses the existing Embedded
fault instrument; its execution and all final native/input/consumer/performance gates remain pending.

The private compact-icon v4 probe compares 64 cases at four DPIs, LTR/RTL and actual Fluent glyphs. Its clipped
layout A/A is exact, but all three replacement paths (including the same retained layout without CLIP) differ
in eight cases, each at 64 edge pixels by one channel level. No production icon/cache/clipping change is adopted.
### Renderer qualification ownership

The private source-ink renderer experiment and its qualification ledger now live in [Source ink renderer qualification](SourceInkRendererQualification_2026-10-27.md). Source snapshot sharing and parsed-data reuse remain the scope of this plan.
