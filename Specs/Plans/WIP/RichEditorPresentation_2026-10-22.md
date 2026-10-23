# Rich editor presentation

Status: NATIVE AND GALLERY QUALIFIED; DOCUMENTATION CLOSEOUT OPEN

The shared SDK owns editor framing, text viewport and typography, semantic spacing,
source syntax/highlight ink, focus dimming and link decoration. It also owns the
chrome controls and native/embedded input ownership used by these presentations.
Consumers retain document, file/network policy, commands and window composition.

- [x] Retain the original main `b69f452f` baseline before implementation.
- [x] Add validated appearance, paragraph/list spacing, source highlights and focus dimming.
- [x] Add opt-in hover/caret link underlines with default compatibility, high contrast and current UIA styles.
- [x] Prepare source syntax ink with exact parser markers, a shared 256-step budget and a 65,536-event ceiling.
- [x] Add canonical Separator, label/button/tree typography, button toggle/theme previews, single-click Tree invocation and TextField font-role presentation.
- [x] Implement committed-text-before-selection ordering and revision-aware transient preview/cancellation notifications without changing authoritative history.
- [x] Implement native pretranslation and embedded key/character ownership, including unmodified IME Return/Space ownership and ordinary post-composition typing.
- [x] Implement bounded opt-in remote image request/cancellation policy; transport remains consumer-owned.
- [x] Cover source/history, geometry, selected pixels, interaction, accessibility, lifetime, cancellation and work budgets in canonical native/embedded tests.
- [x] Persist durable behavior in domain, usage and design-system documentation, including revision-aware selection and fatal allocation-failure wording.
- [x] Complete the exact six-profile build matrix and 62 ordinary x64 suite executions, plus 12 foreground executions, at the recorded 2f producer.
- [x] Regenerate and review the current Release gallery and record the three-profile gallery results.
- [ ] Apply and validate the final prose/index edits, complete their exact docs-only continuation proof and publish/review the selected evidence references.
- [ ] Move this component plan to Done after that scoped closeout is verified; leave parent acceptance holds open.

## Durable contracts

Behavior belongs to [controls and layout](../../UI/UI_ControlsAndLayout.md),
[input and accessibility](../../UI/UI_InputAndAccessibility.md),
[theme and typography](../../UI/UI_ThemeAndTypography.md),
[performance/resources](../../Core/Core_PerformanceAndResources.md),
[native hosting](../../Rendering/Rendering_Win32Host.md) and
[embedded hosting](../../Rendering/Rendering_EmbeddedD3D11.md).
Usage belongs to [rich text](../../../docs/rich-text.md),
[controls](../../../docs/controls.md) and [hosting](../../../docs/hosting.md),
with affected design-system guides and the [gallery](../../../docs/gallery/README.md).

## Implemented behavior and meaningful regression coverage

Appearance, spacing, source ink, focus dimming and inert link decoration preserve
source bytes, UTF-16 ACP selection and authoritative history. Spacing assertions
use complete frames or equal-font caret-top advances; clipped caret extents are
not a substitute for line boxes. Pending sibling preparation cannot revoke an
editor's own source-current geometry. Cooperative ink preparation bounds work and
falls back atomically to complete ordinary Source rendering at capacity.

Committed edits synchronize native input and deliver the text callback before the
surviving selection callback. Nested edits, rebind, detach and retirement suppress
stale outer publication. The source revision participates in selection deduplication
so successive same-ACP previews and cancellation are observable without committed
text/history events; registration and identical imports remain silent. The callback
tests cover replacement, self-clear, failure, reentrancy and sender retirement.

Native pretranslation and embedded dispatch receipts prevent toolbar activation
characters from reaching a refocused successor editor. Foreground regression tests
exercise canonical TextField and RichTextEditor, active IME Return/Space ownership,
callback bypass and exactly one ordinary Space insertion after composition ends.
Standalone committed characters and modified-key policy remain independent. These
protocol tests do not establish external physical IME or assistive-technology acceptance.

## Exact final native and gallery evidence

The executed producer is `2f276caa3c2f2eb58fbbcc0bbaa271c46bbd5b1b`, compiled-input
identity `ae7d1e9dec54d3c702132df7bfd3c348e610f49a4bad3d7b211a3db076383585`, library-input
identity `0E16F05CA9230B6ED13C3CAB099EF8C08E0E8D3B3B4A38DDB331D0E43D663998` and opaque
environment identity `21f8ef26ca9b410beb85d6bf24da6c0c0da473842befdbfa6f3ff168ea2a9082`.
The saved freeze is `.build/qualification-2f276caa-r3/source.json`; the r5 integrity
inventories are `.build/design-refresh-final/native-evidence-2f276caa-r5` and
`.build/design-refresh-final/foreground-evidence-2f276caa-r5`.

All six official x64/ARM64 Debug, Release and ASan Debug builds succeeded. The x64
ordinary rows are 57 default-suite executions (19 per profile), three Gallery suites
and the Release ButtonContrast/MenuTextLayoutResources extras: 62 zero-exit rows,
zero capability skips. Menu, NativeTextInput, MenuResources and MenuResourceScaling
add 12 zero-exit foreground rows across the three x64 profiles, also with zero skips.
Each foreground lease verified foreground, focus and cursor restored or unchanged.
The combined ordinary total is 74; three real expected ASan detector failures
(two primary, one foreground) are accounted separately and retain their nonzero exits.

The inventories join receipt executable hashes to saved source/build/environment
identities and the byte-verified in-place manifest. Source/environment are derived
joins, not fields embedded in ordinary suite receipts. All 74 artifacts were
inventoried in place: `physicallySnapshotted=false`, no copied native binaries.
These inventories support integrity review; they are not independent rebuild/replay
bundles and do not preserve canonical output bytes against later replacement.

The [gallery generation](../../../docs/gallery/generation.json) records current 2f
x64 Release and six populated scenes. Debug and ASan drivers also completed.
The saved review in `.build/qualification-2f276caa-r3/gallery-visual-review.json`
viewed all six Release scenes plus the five byte-different ASan sheets and established
no new defect. The five ASan image differences are retained as observed
fixture limitations, without an attributed cause or physical-input qualification.

Earlier e62/5a/1cff observations, the original IME Space failure, refused hidden
WM_IME fixture, actual mirror/same-ACP failures and disk-capacity attempts remain
retained under their original source and outcome. Later success does not rewrite
those receipts or retroactively establish their causes.

## Remaining closeout and separate parent holds

The proposed usage corrections and this reconciled plan still require application,
the repository's prescribed prose/spec/tooling/format checks and exact documentation-
only continuation evidence. Reviewed public evidence references must be present and
valid before this plan moves to Done. This record claims the executed 2f results;
later documentation edits supply no new native execution.

The parent [owned rich-text editor](RichTextEditor_2026-10-17.md) and
[library/consumer qualification](ConsumerUiQualification_2026-10-11.md) remain WIP.
Physical IME/AT/touch, native ARM64 execution/restoration, exact consumer adoption,
hosted CI, release acceptance and accepted paired performance remain separate gates.
ARM64 evidence comprises three cross-builds within the six-profile matrix;
native ARM64 runtime and restoration remain unqualified.

Historical performance with 71/72 unstable controls and separately retained 72/72
observations establishes no trusted non-regression verdict. This source-bound native
and gallery record supplies no current paired SDK performance-policy acceptance,
threshold relaxation, accepted cost or rebaseline. Application measurements belong
in their consumer repository and cannot close the SDK's shared acceptance gate.

Integrate through a normal SDK merge commit so exact adopted pins remain reachable;
do not squash or rebase away the consumer's pinned source. The parent plans retain
ownership of their unclosed acceptance requirements.
