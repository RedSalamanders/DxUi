# Rich editor presentation

Status: COMPLETED (scoped implementation and source-bound native/gallery qualification)

Completed 2026-10-23. This scoped record preserves the executed native/gallery producers and the reviewed
documentation continuation. Parent performance, physical-input, native ARM64, consumer and release gates remain
open. Publication requires the final documentation checks and clean-commit continuation described below.

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
- [x] Remove the untouched drawing-effect reset while retaining attempted-setter rollback and ordered underline restoration.
- [x] Complete six-profile builds and 62 ordinary plus 12 foreground zero-skip x64 executions at exact 38ec; retain the prior qualified 2f results separately.
- [x] Complete all three 38ec gallery drivers, retain all 18 PNGs, verify their exact prior-reviewed image identities and publish the canonical clean-38ec Release gallery.
- [x] Preserve separate private selected packets for 38ec and prior 2f without rewriting original producer bytes; prepare Markdown-only summaries with complete metadata inventories.
- [x] Apply reviewed prose/index edits and independently verify the published Markdown inventories against all 146 retained selected files and 128 producer origins; bind continuation to unchanged compiled/library inputs.
- [x] Move this scoped plan to Done and retain all separate parent acceptance holds.

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
The effects-only restoration contract is organized under
[transient raster decoration](../../Core/Core_PerformanceAndResources.md#transient-raster-decoration).

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

The 38ec repair changes only the local drawing-effect rollback guard. Plain,
dormant, disjoint, ineffective or fully excluded decoration does not reset an
untouched effect range. Each actual fallible effect-setter attempt arms cleanup
before the call, including an attempt that fails. Canonical restoration and ordered
underline replay for overlapping admitted style runs remain unchanged. Existing
native tests exercise exact plain pixels, real partial ink changes, unchanged source
geometry, shared-copy restoration and retained-copy lifetime. Static removal of a
redundant native write and these regressions do not establish a causal application
CPU/memory improvement or trusted performance-policy acceptance.

## Source-bound qualification

### Successor producer 38ec

The executed producer is `38ec36be42bf693dc649f1fe6b1dfc6cf34d4064`, compiled-input
identity `a4740ffc5463cd80880dfaf266b6c4d919fa5024f1c10fe754366c6d60aca3d3`, library-input
identity `35C47359DDC5CE8E1247D295B46FD7BDF6107D04BF7FA0BFF6E3404B85FB8714` and opaque
environment identity `21f8ef26ca9b410beb85d6bf24da6c0c0da473842befdbfa6f3ff168ea2a9082`.

The [38ec Markdown summary](../../../Measurements/RichText/2026-10-23-presentation-38ec36be.md)
records all six successful x64/ARM64 Debug, Release and ASan Debug builds.
Its 62 ordinary x64 rows comprise 57 default-suite executions (19 per profile),
three Gallery suites and the Release ButtonContrast/MenuTextLayoutResources extras,
all zero-exit and zero-skip. Menu, NativeTextInput, MenuResources and MenuResourceScaling
add twelve zero-exit, zero-skip foreground rows across the three x64 profiles.
Each lease verified foreground, focus and cursor restored or unchanged. Three real
expected ASan detector failures (two primary, one foreground) retain their nonzero
exits separately from the 74 successful suite executions.

The actual native and foreground inventories derive source/environment joins
through saved builds, matching receipt executable hashes and the byte-verified
in-place artifact manifest. Ordinary suite receipts do not embed those identities.
All 74 native artifacts were inventoried in place, totaling 2,410,927,170 logical
bytes: `physicallySnapshotted=false`, no copied native binaries. The foreground
collector joins the primary inventory without rereading binary bytes. Canonical
output paths remain replaceable; these selected records are not physical snapshots
or complete independent rebuild/replay bundles.

| Selected record | SHA-256 |
| --- | --- |
| Frozen source (retained private metadata) | `9D6C08776D0BACA25D8D2864884D4D00310DA2F4E9F389AEB12593DB49F3ADD9` |
| Native summary (retained private metadata) | `90F6BC19F0050B2C93E8537AE23210FD4EADF2E90D79C2A7328B38A561F12FFD` |
| Foreground summary (retained private metadata) | `E5913EB7186B584F384F20E8BB635E5E3DB5FFFFB658533FD7927617AC7A2F5B` |
| Gallery review (retained private metadata) | `5F81ABF3BBF19DEEC055C4DE15395E38931A51E7C352FA4C01F17E341473F58D` |
| Canonical documentation generation (recorded metadata) | `66A79EF7C79B0D1192ABF27D46BF81FFEB7DE494FB6514B16CF9D6F999260159` |
| 38ec packet manifest (retained private metadata) | `33212D278049FEBB02F6D534460896244361955824383EFB87083412D3D21912` |

All eighteen current gallery PNGs retain the eleven byte sequences already
inspected in the prior root review: six Release scenes and five differing ASan
theme sheets. Exact SHA/length comparison supports this visual-review continuation;
no new direct inspection was claimed. The unchanged ASan differences remain
unattributed. Canonical Release gallery publication ran from exact clean 38ec before
tracked public evidence or prose changes; all six published hashes match the
reviewed Release images. This establishes no physical IME/AT/touch acceptance.

The private 54-file current selected packet and unchanged 92-file prior packet contain
128 byte-exact producer origins and all 36 gallery PNGs between them. New wrapper
prose/provenance is identified as derived; raw logs, environment values, binaries
and complete payloads remain private. Only the two Markdown summaries and their complete original-path/size/SHA inventories are proposed under Measurements; no raw JSON, PNG, log or child/status payload is published there. An earlier selected-packet authoring attempt
misclassified JSON-escaped relative backslashes as UNC paths; its failed staging
and outcome remain retained. The corrected decoded-string privacy scan and later
successful packet preparation are metadata-workflow results, not native test failures
or additional native qualification.

### Prior qualified producer 2f

The prior executed producer is `2f276caa3c2f2eb58fbbcc0bbaa271c46bbd5b1b`, compiled-input
identity `ae7d1e9dec54d3c702132df7bfd3c348e610f49a4bad3d7b211a3db076383585`, library-input
identity `0E16F05CA9230B6ED13C3CAB099EF8C08E0E8D3B3B4A38DDB331D0E43D663998` and opaque
environment identity `21f8ef26ca9b410beb85d6bf24da6c0c0da473842befdbfa6f3ff168ea2a9082`.
Its [Markdown summary](../../../Measurements/RichText/2026-10-23-presentation.md) describes the unchanged private selected packet, which
retains all 92 files, 81 byte-exact producer origins, 22 historical metadata proofs
and eighteen gallery PNGs. Its manifest remains
`F402E6B6F5D77C8E0B5B6959411189D43C812BA44703FF7799DA8F5FD6902375`.

All six official x64/ARM64 Debug, Release and ASan Debug builds succeeded. The x64
ordinary rows are 57 default-suite executions (19 per profile), three Gallery suites
and the Release ButtonContrast/MenuTextLayoutResources extras: 62 zero-exit rows,
zero capability skips. Menu, NativeTextInput, MenuResources and MenuResourceScaling
add twelve zero-exit foreground rows across the three x64 profiles, also with zero skips.
Each foreground lease verified foreground, focus and cursor restored or unchanged.
The combined successful suite total is 74; three real expected ASan detector failures
(two primary, one foreground) are accounted separately and retain their nonzero exits.

The prior inventories join receipt executable hashes to saved source/build/environment
identities and their byte-verified in-place manifest. Source/environment are derived
joins, not fields embedded in ordinary suite receipts. All 74 prior artifacts were
inventoried in place: `physicallySnapshotted=false`, no copied native binaries.
Those inventories support integrity review; they are not independent rebuild/replay
bundles and do not preserve canonical output bytes against later replacement.

The private prior gallery record, inventoried in the [prior summary](../../../Measurements/RichText/2026-10-23-presentation.md),
retains 2f's three-profile results and original visual review. That review viewed
all six Release scenes plus the five byte-different ASan sheets and established
no new defect. The five ASan image differences remain observed fixture limitations,
without an attributed cause or physical-input qualification. The source-bound 2f
results are not relabelled as execution of 38ec or a later documentation commit.

Earlier e62/5a/1cff observations, the original IME Space failure, refused hidden
WM_IME fixture and mirror/same-ACP failures (consumer-owned private diagnostic context), and SDK disk-capacity attempts remain
retained under their original source and outcome. Later success does not rewrite
those receipts or retroactively establish their causes.

## Documentation closeout

The first final documentation validation failed with 102 findings because mixed
JSON/status/child reports had been proposed under Measurements. The two untracked
trees were moved privately with all 146 file sizes/hashes verified unchanged;
the original selected staging and producer manifests also remain immutable.
No validator or producer metadata was changed to bypass that contract.

The corrected publication contains two Markdown summaries with complete original-path/size/SHA inventories.
Root independently compared every selected row and origin and rehashed all 146 retained selected files.
Canonical docs/gallery remains the tracked image location. The normative transient-decoration subheading and
usage link are applied. Specification validation passes with the mixed raw reports kept private; compiled and
library input identities remain exactly those of 38ec.

Before publication, require actual successful `validate.ps1`, `format.ps1 -Check`,
`Tools/Fold-Changelog.ps1 -Check` and `Test-Changes.ps1 -BaseRef 38ec36be42bf693dc649f1fe6b1dfc6cf34d4064 -Explain`
at the clean final documentation commit. Retain their logs and the exact documentation-only continuation under
`.build/design-refresh-final/`; that proof checks producer ancestry, unchanged compiled/library inputs, unchanged
parent WIP plans, both summary hashes and the immutable private producer manifests. It runs no native code and
does not requalify the environment. Native/gallery results remain attached to executed 38ec and 2f producers;
they are never relabelled as execution of the documentation commit. Application measurements stay in their
application repository.

## Separate parent holds

The parent [owned rich-text editor](../WIP/RichTextEditor_2026-10-17.md) and
[library/consumer qualification](../WIP/ConsumerUiQualification_2026-10-11.md) remain WIP.
Physical IME/AT/touch, native ARM64 execution/restoration, exact consumer adoption,
hosted CI, release acceptance and accepted paired performance remain separate gates.
ARM64 evidence comprises three cross-builds within each six-profile matrix;
native ARM64 runtime and restoration remain unqualified.

Historical performance with 71/72 unstable controls and separately retained 72/72
observations establishes no trusted non-regression verdict. This source-bound native
and gallery record supplies no current paired SDK performance-policy acceptance,
threshold relaxation, accepted cost or rebaseline. Application measurements belong
in their consumer repository and cannot close the SDK's shared acceptance gate.

Integrate through a normal SDK merge commit so exact adopted pins remain reachable;
do not squash or rebase away the consumer's pinned source. The parent plans retain
ownership of their unclosed acceptance requirements.
