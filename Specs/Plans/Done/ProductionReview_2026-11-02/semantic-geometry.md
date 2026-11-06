# Semantic document and visual geometry: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the rich semantic document model and its
visual geometry (`src/Controls/DxUi.RichSemanticGeometry.cpp`, `src/Controls/DxUi.RichSemanticDocument.cpp`,
`src/Controls/DxUi.RichSemantic.cpp`, `src/Controls/DxUi.PaintIndex.h`, `src/Controls/DxUi.SemanticExtensions.h`, the
matching public headers and tests), followed into the editor, layout, script renderer and image-resource callers.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| SEM-01 | P1 | Confirmed | Script paragraphs with a color glyph are always rejected: the raster target is released before `LayoutOne` checks it | `src/Controls/DxUi.RichSemanticGeometry.cpp:1994` | S |
| SEM-02 | P2 | Confirmed | Every image completion and animation frame discards all prepared math and re-uploads every bitmap | `src/Controls/DxUi.RichSemanticGeometry.cpp:2179` | S |
| SEM-03 | P2 | Confirmed | Auto paragraph direction treats the generated `[image: ...]` placeholder as strong Latin text | `src/Controls/DxUi.RichSemanticGeometry.cpp:57` | S |
| SEM-04 | P2 | Confirmed | Definition lists and callout headers bypass `Prefix`, so a list bullet, number or footnote label is lost | `src/Controls/DxUi.RichSemanticDocument.cpp:1445` | S-M |
| SEM-05 | P2 | Confirmed | `VisitSemanticWords` and `CountWords` drop or truncate words that start with a supplementary-plane letter | `src/Controls/DxUi.RichSemanticDocument.cpp:193` | S |
| SEM-06 | P3 | Confirmed | Rejections known before layout are raised only at Publish, after a complete layout pass | `src/Controls/DxUi.RichSemanticGeometry.cpp:2832` | S |
| SEM-07 | P3 | Confirmed | PaintIndex staging spends one cooperative step per node and duplicates `PaintIndex::Build` untested | `src/Controls/DxUi.RichSemanticGeometry.cpp:2745` | S |
| SEM-08 | P3 | Confirmed | Linear per-paint and per-query scans that the paint index could serve | `src/Controls/DxUi.RichSemanticGeometry.cpp:3426` | S |
| SEM-09 | P3 | Confirmed | Callout `skipUntil` leaks into later siblings and silently disables definition lists inside callouts | `src/Controls/DxUi.RichSemanticDocument.cpp:1382` | S |
| SEM-10 | P3 | Plausible | Code copy action follows the host direction while frame, gutter and numbers follow the block direction | `src/Controls/DxUi.RichSemanticGeometry.cpp:3300` | S |

Severity counts: P1 1, P2 4, P3 5. Verdicts: Confirmed 9, Plausible 1, Refuted 0, Duplicate 0.

## P1 findings

### SEM-01 — Script paragraphs with a color glyph are always rejected: the raster target is released before `LayoutOne` checks it

- **Severity** P1 (reviewer P1, verifier P2, lead P1; see Lead note) · **Category** bug · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `309dc673` (added the `hasScripts` retention and the check; the
  conflicting `work.raster.reset()` dates from `c1a77e42`)
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:981`, `src/Controls/DxUi.RichSemanticGeometry.cpp:1003`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:1994-2001`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2452-2459`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:817`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2164`,
  `src/Controls/DxUi.RichTextLayout.cpp:610-628`, `src/Controls/DxUi.RichTextScriptRenderer.h:157-189`,
  `src/Controls/DxUi.RichTextEditor.cpp:3477-3563`
- **What**: `Preparation::raster` is deliberately kept alive when the document has superscript or subscript runs
  (`raster(state->images || state->hasScripts || ... ? target : nullptr)`, line 981). The NativeImages phase then ends
  with an unconditional `work.raster.reset(); work.phase = CodeLabels;` (2454-2459). The phase enum is strictly linear
  (946-969: NativeObjects, NativeImages, CodeLabels, MarkerLayouts, TableAllocate/Layouts, ...) and NativeObjects hands
  off only to NativeImages (2371), so every `LayoutOne` call runs with a null `raster`. `LayoutOne` is a `Preparation`
  member and has no local named `raster`; line 1994 calls
  `PreparedLayoutAccess::ScriptTargetSupported(box->layout, raster.get())` with null. For a layout with scripts whose
  staged metadata has `requiresColor`, that function returns false on `if (! target)` (`DxUi.RichTextLayout.cpp:620`).
  `requiresColor` is staged at layout creation (`DxUi.RichTextLayout.cpp:423-434`) and is set by ANY color-font glyph
  run in the paragraph, not only runs inside the script (`DxUi.RichTextScriptRenderer.h:181`). `next.Reject(
  content.element)` (2000-2001) then makes Publish throw `NativeObjectsRejected` (2832-2833); the editor records the
  leaf in `nativeObjectRejections` (`rejectNativeObjects`, `DxUi.RichTextEditor.cpp:3477-3563`, whose `hasScript`
  accepts Paragraph/Heading/TableCell leaves that overlap script runs) and rebuilds the semantic document with that
  range as literal source. The editor passes the target only at `Begin` (`host.GetDeviceContext()`,
  `DxUi.RichTextEditor.cpp:3992`); nothing re-acquires it. `State::scriptTarget` (817) exists but is used only for
  reuse identity (2164), never for validation. No test covers the success path: the only color-script test uses no
  target at all (`Tests/Support/Support.Tests.RichSemanticGeometry.h:435-450`) and the `testRichScriptTargetUnsupported`
  tests force the fallback.
- **Failure scenario**: Windows host whose device context supports `ID2D1DeviceContext7`. Document `x^2^ 😀` with the
  RichMarkdown profile, or `<p>x<sup>😀</sup></p>`, rendering enabled. Expected
  (`Specs/UI/UI_ControlsAndLayout.md` sup/sub row, "if unavailable, that HTML leaf alone falls back"): native
  superscript and color emoji. Actual: a full geometry pass runs, Publish throws `NativeObjectsRejected`, and the
  paragraph is shown as exact raw source. The documented retry "when its target changes" re-runs the same code and
  fails the same way. Every such paragraph consumes one of the 128 shared slots (`State::Reject`, 820-828); a 129th
  distinct rejection throws `NativeObjectCapacity` with `element = MarkdownNoNode` (`DxUi.RichSemanticGeometry.h:59-72`),
  which `rejectNativeObjects` turns into `nativeGeometryRejected` for the whole document (3573, 3895): the entire
  document loses rich presentation. That tail needs 129 or more such paragraphs.
- **Fix**: Make the check independent of the long-lived target.
  1. In the `Preparation` constructor, when `state->hasScripts` and `target` is non-null, query
     `ID2D1DeviceContext7` once and store a `bool colorScriptCapable` (E_NOINTERFACE → false; any other failure
     follows the existing `Check` behavior in `ScriptTargetSupported`). A null `target` (CPU-only callers) → false,
     which keeps today's behavior for them.
  2. Add an overload `PreparedLayoutAccess::ScriptTargetSupported(const PreparedLayout&, bool colorCapable)` (same
     body, with the capability bit in place of the QI) and call it at 1994. Keep the pointer overload for
     `PreparedLayout::Draw` (`DxUi.RichTextLayout.cpp:634`). Keep the `testRichScriptTargetUnsupported` seam.
  3. Change `State::scriptTarget` from a strong `wil::com_ptr` to an identity-only raw pointer (as
     `NativeImageGeometry::targetIdentity` is), compared in `same()` (2164). The strong reference currently pins the
     host device context for the lifetime of the published geometry, contradicting the "released before publication"
     comment at 1003.
  `work.raster.reset()` at 2456 can then stay where it is. Rejected alternatives: validating against
  `next.scriptTarget` (entrenches the strong reference), or moving the reset to the Publish branch (works, but holds
  the context through every later phase for no reason).
- **Regression test**: Add to `Tests/Support/Support.Tests.RichSemanticGeometry.h` next to line 435 (keep the existing
  null-target negative case), or to an Embedded fixture if the Support suite cannot create a WARP D3D11-backed
  `ID2D1DeviceContext` (its current `target` is a WIC render target, which does not expose `ID2D1DeviceContext7`).
  Steps: create the device context; `QueryInterface` for `ID2D1DeviceContext7` and skip with a log line on
  E_NOINTERFACE; otherwise `Begin(factory, doc, emojiOptions, drawing, {}, {}, deviceContext)` and `Complete` for both
  `<p>x<sup>😀</sup></p>` and the RichMarkdown paragraph `x^2^ 😀` (color glyph outside the script). Assert no
  `NativeObjectsRejected`, `geometry.Ready()`, and that `Draw` onto the same target succeeds. At `6df17007` both
  documents throw `NativeObjectsRejected` with the paragraph element.
- **Verifier notes**: Phase order, the single assignment/reset sites of `raster`, the null-target branch and the editor
  consequence were traced line by line; no guard or second acquisition exists. The reviewer's severity was lowered to
  P2 by the verifier because the documented fallback (exact source, no data loss, no crash) is still honored; the lead
  overrides that. SEM-01 and RENDER-05 (`rendering-images.md`) are one item with two causes, SEM-01 first:
  SEM-01 is the null-target lifetime bug (fires on every OS, even with `ID2D1DeviceContext7`); RENDER-05 is a real
  target that lacks `ID2D1DeviceContext7` plus `requiresColor` being set by non-script color runs. Fixing SEM-01 does
  not fix RENDER-05 (hosts without the interface still fall back) and RENDER-05's `ID2D1DeviceContext4` path does not
  fix SEM-01 (a null target still returns false). At `6df17007` SEM-01 masks RENDER-05: the older-Windows scenario is
  rejected, but so is every other one. RENDER-05's test (a wrapper whose QI returns E_NOINTERFACE) fails both before and
  after the SEM-01 fix by design. That `ID2D1DeviceContext7` is absent on older Windows is not provable from the
  repository; `Tests/Controls/Controls.Tests.RichTextLayout.cpp:1033` merely logs availability.
- **Lead note**: Severity kept at P1 against the verifier's P2. The exact-source fallback is meant for targets that
  genuinely lack color-glyph support. This bug makes it fire on every device for any superscript or subscript paragraph
  that needs a color glyph (for example `x^2^ 😀`), so a default-path feature never works anywhere. That is a
  user-visible defect, not graceful degradation.

## P2 findings

### SEM-02 — Every image completion and animation frame discards all prepared math and re-uploads every bitmap

- **Severity** P2 (reviewer P2, verifier P2) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `e5fda597` (math), `0501b802` (raster images)
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:2176-2186`, `src/Controls/DxUi.RichSemanticGeometry.cpp:268-299`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:348-363`, `src/Controls/DxUi.RichSemanticGeometry.cpp:527-551`,
  `src/Controls/DxUi.RichImageResources.h:308-335`, `src/Controls/DxUi.RichImageResources.h:340-382`,
  `src/Controls/DxUi.RichTextEditor.cpp:793-865`
- **What**: `previousImages` is passed to the new `NativeImageGeometry` only if `state_->imageResources == images`
  (shared_ptr identity, line 2179). `RichImageResources::Complete` and `ReplaceFrame` set `snapshotDirty_` and
  `Publish()` builds a new `RichImageResourceSet` (375-382, called before each Begin at `DxUi.RichTextEditor.cpp:3913`),
  so after any completion or frame replacement `previousImages == nullptr`. The constructor then re-creates every
  `PreparedMath` (with row tags and palettes) and leaves `placement.bitmap` empty, so `PrepareOne` re-uploads every
  already-decoded bitmap and re-rasterizes every SVG. The per-placement reuse inside the constructor is already keyed
  by pixel identity (`prior.pixels == placement.pixels`, plus SVG raster size) and math by element in the same
  document, so snapshot identity adds nothing to correctness; the previous state holds the `shared_ptr`s, so there is
  no address reuse. `Invalidate()` keeps `state_` (`DxUi.RichEditorLayout.h:54-58`), so the previous geometry is
  available and only the gate blocks it. Box layouts are already reused because `previous` (2187-2188) does not require
  the same snapshot. The gate is deliberate and documented: `Specs/UI/UI_ControlsAndLayout.md:1209` ("same factory,
  immutable image snapshot, target and resource epoch") and `:2199` (math reuse requires matching "image bindings"),
  both from the width-only reflow work.
- **Failure scenario**: (a) Progressive load: a note with 30 formulas and 32 local images; each published completion
  batch re-prepares all formulas and re-uploads all images decoded so far. Upper bound is about N squared over two
  image sizes: 32 images of 2 MiB give roughly 1 GiB of `CopyFromMemory` across the load (about 250 extra upload turns
  at the 4 MiB budget) plus 30 formula preparations per batch. Coalescing (`! alreadyDirty`,
  `DxUi.RichTextEditor.cpp:815`) and cancellation reduce it in practice. (b) Animation, the sustained case:
  `ReplaceImageFrame` (documented for animated media, `docs/rich-text.md:620-622`) takes the same invalidate path
  (`DxUi.RichTextEditor.cpp:849-862`) on every frame, so with one animated GIF every frame re-prepares all math and
  re-uploads every other image in the document. If that takes longer than the frame interval, the next frame cancels
  the in-flight preparation (857-858) and the published geometry can starve.
- **Fix**: Drop `state_->imageResources == images` from the `previousImages` predicate at 2179. Keep document identity,
  factory, `targetIdentity == raster`, `imageEpochs == epochs` and options-modulo-width (the target check is still
  required because bitmaps and `mathPalette` are target-bound). Bitmap reuse stays keyed by `pixels` identity, SVG reuse
  by `(pixels, svgWidth, svgHeight)`, math by element. Update the two spec sentences above and add a `Changes/`
  fragment; no other code change is needed.
- **Regression test**: Primary: `Tests/Embedded/Embedded.Tests.RichImageFailures.h` near line 582, which already counts
  `calls.uploads` and `calls.copies` through `RasterCalls`. Prepare a document with two images and complete both; then
  `Begin` again with a NEW `RichImageResourceSet` that holds the same `RichImage` pointers plus one more image and
  assert the upload counter rises by exactly one. Add a frame case: replace the pixels of image A only and assert one
  upload (A) and no re-upload of B. Secondary (CPU-only): in `Tests/Support/Support.Tests.RichSemanticGeometry.h`
  (next to the `testRichMathPreparations` checks at about line 895) use two distinct snapshot objects and assert
  `testRichMathPreparations` does not increase. Both fail at `6df17007`.
- **Verifier notes**: Mechanism traced end to end. The gate is intentional per the spec text (not a lifetime
  concern), so this needs a spec edit. The animation amplifier is the reason the severity stays P2 despite the
  spec-scoped design; the reviewer's progressive-load scenario alone would be P3.

### SEM-03 — Auto paragraph direction treats the generated `[image: ...]` placeholder as strong Latin text

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug (opt-in feature) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `0a5640e3`
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:48-58`, `src/Controls/DxUi.RichSemanticGeometry.cpp:1295`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:1396`, `src/Controls/DxUi.RichSemanticGeometry.cpp:1610`,
  `src/Controls/DxUi.RichSemanticDocument.cpp:941-948`, `src/Controls/DxUi.RichBidi.h:17-43`
- **What**: `ParagraphDirection` runs `FirstStrongDirection` over the raw display substring from `markerEnd` (or
  `display.first`). `CompleteImage` emits `[image`, `: `, the alternative and `]` as one `DisplaySpanKind::Object` span.
  `[` is neutral and `i` is bidi class L, so the first strong scalar is the English chrome. Math, diagrams and rules emit
  U+FFFC (neutral) and task boxes and bullets are neutral; only images inject Latin letters. The result also feeds
  marker layout (1295) and table measurement (1396).
- **Failure scenario**: `autoParagraphDirection = true`, document `IMG שלום עולם`, where `IMG` is the inline image `![לוגו]` immediately followed by
  `(logo.png)` (written split here only because the repository link validator scans inline code) (also `<img alt="...">`
  and unplaced images). The paragraph resolves left-to-right: leading-aligned on the left, caret order and selection
  left-to-right, list marker on the wrong side. An image-only paragraph in a right-to-left host also resolves
  left-to-right, where it should inherit the host direction. Without the image the paragraph is correctly
  right-to-left.
- **Fix**: Compute first-strong only over display ranges that are not `Generated` or `Object` spans (walk
  `document.Map().Spans()` for the paragraph range, binary-searched), or substitute U+FFFC for those spans while
  scanning. `VisitSemanticWords` already builds this exclusion (`DxUi.RichSemanticDocument.cpp:131-133`); share one
  helper. Alternative text inside the Object span is intentionally ignored, so an image-only paragraph inherits the
  host direction.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h` after line 2104, with auto direction on and a
  completed image (or the pending one). Text `IMG2 שלום`, where `IMG2` is `![x]` immediately followed by `(pending.png)`:
  assert, in the LTR host, that the image bounds lie to the RIGHT of the Hebrew word's caret and the line is
  right-aligned; add `IMG2` alone in an RTL
  host and assert it follows the host direction. Do not rely on "the Hebrew word's leading caret is right of its
  trailing caret": that holds for a right-to-left run inside a left-to-right paragraph and would pass today.
- **Verifier notes**: Display text and spans verified at the cited lines. Opt-in feature (default off), no data loss.

### SEM-04 — Definition lists and callout headers bypass `Prefix`, so a list bullet, number or footnote label is lost

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug · **Verdict** Confirmed (cases a and c; case b
  plausible) · **Confidence** high for (a) and (c), medium for (b) · **Effort** S-M · **Introduced** `bbe43469`,
  `645ef111`
- **Location**: `src/Controls/DxUi.RichSemanticDocument.cpp:1378-1444`, `src/Controls/DxUi.RichSemanticDocument.cpp:1445-1476`,
  `src/Controls/DxUi.RichSemanticDocument.cpp:1926-1953`, `src/Controls/DxUi.RichSemanticDocument.cpp:2187`,
  `src/Controls/DxUi.RichSemanticDocument.cpp:2287-2310`, `src/Controls/DxUi.RichSemanticDocument.cpp:1512-1590`
- **What**: List markers and footnote ordinals are emitted only by `Prefix`, which runs only from
  `BeginLeafParagraph` (line 1935) for the first visible leaf of a `ListItem` or footer root; `SeparateMarker` is false
  when that first child is a Paragraph (1512-1521), so the item has no stand-alone marker line either.
  `DefinitionList` (dispatched at 2187, before any leaf begins) returns true, and `DefinitionPart` creates its
  paragraphs with `Paragraph(paragraphVisit, element)` only: no `Prefix` and none of the `markerEnd`/`markerOwner`
  bookkeeping (set only at 1939-1946 and 2283-2284). The callout header (2300) is built the same way; `FindCallout`
  (661-725) has no top-level restriction. The spec places no container limit on definition lists
  (`Specs/UI/UI_ControlsAndLayout.md` "With DefinitionLists enabled ...").
- **Failure scenario**: (a) `- Term` / `  : Description` with DefinitionLists enabled: the definition renders indented
  with no bullet; with `1. Term` the number and gutter are missing. (b) A referenced `[^1]: Term` /
  `    : Desc`: the footer entry lacks its `1. ` label (that the continuation line parses with zero leading spaces is
  not proved statically). (c) `- > [!note] Title`: single-line case has no bullet anywhere (the body paragraph's
  parent quote has the skipped header as first visible child, so `Prefix` breaks at 1569); multi-line case puts the
  bullet on the body's second visual line.
- **Fix**: Extract the marker step from `BeginLeafParagraph` (1933-1949: `Prefix`, then `markerEnd` and `markerOwner`)
  into one helper and call it (1) in `DefinitionPart` for `definitionLine == 0` right after `Paragraph(...)`, valid
  because the `normalized` value computed in `DefinitionList` is still current (part 0 is pushed last at 1434-1441 and
  popped first), and (2) in the callout header path right after `Paragraph(headingVisit, header)` at 2300. For (2) mark
  the callout's child visits as "marker already emitted" so `BeginLeafParagraph` does not add a second bullet to the
  body line of the same paragraph. If geometry shows problems with a marker owner on a definition term, fall back to
  refusing `DefinitionList` when the node is the first visible child of a `ListItem` or footer root (consistent with
  "rejected forms retain normal Markdown presentation"); that fallback does not cover callouts.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticDocument.h` beside `OptionalDefinitionsAndAbbreviations`
  (line 210), using its extension-enabled parser setup (`RichAll | DefinitionLists | Abbreviations`). Sources:
  `- Term\n  : desc\n`, `1. Term\n   : desc\n`, `x[^1]\n\n[^1]: Term\n    : Desc\n`, `- > [!note] Title\n` and
  `- > [!note] Title\n  > body\n`. For each, first assert the expected `DefinitionList` or `Callout` element exists (so
  the test cannot pass vacuously), then assert the display starts with the bullet (U+2022) or number, the first
  paragraph has `markerOwner != MarkdownNoNode` and `markerEnd > display.first`, and (last source) exactly one marker
  appears. Adjust the footnote fixture if the continuation line does not parse as a definition. All fail at `6df17007`.
- **Verifier notes**: Call graph confirmed (`Prefix` has one caller path). DefinitionLists is an opt-in extension and
  callout-in-list is uncommon, hence P2 rather than higher; no data loss, source intact.

### SEM-05 — `VisitSemanticWords` and `CountWords` drop or truncate words that start with a supplementary-plane letter

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug · **Verdict** Confirmed · **Confidence**
  medium-high · **Effort** S · **Introduced** `257e5e04`
- **Location**: `src/Controls/DxUi.RichSemanticDocument.cpp:180`, `src/Controls/DxUi.RichSemanticDocument.cpp:193-201`,
  `src/Controls/DxUi.SingleLineTextEditing.cpp:24-28`, `src/Controls/DxUi.SingleLineTextEditing.cpp:145-148`,
  `src/Controls/DxUi.RichSemantic.cpp:19-27`
- **What**: `IsWordCharacter` is `GetStringTypeW(CT_CTYPE1, &ch, 1)` with `C1_ALPHA | C1_DIGIT` on one UTF-16 unit, so a
  lone lead surrogate is never classified from its pair. At line 193 a non-word start steps a whole text element
  (`StepToNextTextElement`), so a word whose first scalar is outside the BMP (Adlam, Osage, Gothic, CJK Extension B/C,
  mathematical alphanumerics) is never reached; if a BMP letter follows inside the same UIA word, `word.first = at`
  (201) truncates the word to start mid-word, so the visitor (spell checker) receives wrong text and a wrong source
  range. `FindInSource` (`DxUi.RichSemantic.cpp:21`) explicitly treats surrogate units as word characters, so the two
  word models disagree.
- **Failure scenario**: Fula text in Adlam (`𞤀𞤣𞤤𞤢𞤥` ...) → `CountWords == 0`; `𠀋a` (U+2000B then `a`) → the visitor
  receives `a` with a one-unit source range.
- **Fix**: At `if (! IsWordCharacter(segment[at]))` decode the scalar with `ReadCodePointAt(segment, at)` and classify
  the code point, using a scalar-based predicate (for example the Unicode tables already used by
  `FirstStrongDirection`, or `GetStringTypeW` over the two-unit pair after verifying on a target OS which output
  element carries the supplementary result). Apply the same decoding to the look-behind at line 180
  (`segment[at - 1u]`). Do not simply reuse the `FindInSource` lambda: it treats every surrogate, emoji included, as a
  word character, which would make emoji count as words. Keep a single shared predicate for both callers by moving the
  scalar classifier into a common helper.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticDocument.h`: `CountWords(Build(L"\U0001E900\U0001E901
  \U0001E902\n")) == 2`; a visitor over `L"\U0002000Ba b\n"` receives `L"\U0002000Ba"` first with source range
  `{0, 3}`; `L"😀 a\n"` still counts one word (guards the emoji pitfall). The first two fail at `6df17007`.
- **Verifier notes**: Static reasoning; the exact `GetStringTypeW` result for a lone surrogate is OS data (expected
  0 or `C1_DEFINED`, not `C1_ALPHA`), corroborated by the `FindInSource` special case. A three-line probe would settle
  it; if it reported `C1_ALPHA` the finding falls away. No existing test covers supplementary letters
  (`Tests/Support/Support.Tests.RichSemanticEditor.h:2148-2182` only). Severity kept at P2 given the project's explicit
  Unicode-preservation requirement; P3 if supplementary scripts are declared out of scope for word counting.

## P3 findings

### SEM-06 — Rejections known before layout are raised only at Publish, after a complete layout pass

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `ffb52976`
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:391-395`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2377-2383`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:2419-2428`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2473-2486`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:2832-2833`
- **What**: Math capacity rejections are known in the `NativeImageGeometry` constructor (`rejected.push_back`, 392-395,
  which also bypasses `State::Reject`'s dedupe and cap), font-size and palette-without-target diagram rejections in
  NativeObjects, and math unsupported/capacity rejections in NativeImages. Nothing inspects `rejected` until Publish
  (2832), so CodeLabels, MarkerLayouts, TableAllocate, all Layouts (up to 4,096 SDK layouts), CodeNumbers, SourceLines,
  bounds and PaintIndex still run before the throw. The editor then rebuilds semantics and lays everything out again.
  The CountSites comment (2315-2317) states the opposite intent.
- **Failure scenario**: a large note with one inline formula that is rejected at construction, for example
  `$\tag{1} x$` (`std::length_error("Tagged equations require display mode")`, lines 373-374). Rejections are
  remapped across edits (`DxUi.RichTextEditor.cpp:3747-3799`), but `NativeObjectRejections::Matches` includes width,
  dpi, direction, typography, resources, binding and appearance (491-502), so any width change (interactive resize)
  resets the set and re-triggers a full throw-away pass.
- **Fix**: Immediately after the NativeImages phase completes (the branch at 2454-2459, before
  `work.phase = CodeLabels`), add `if (! work.state->rejected.empty()) throw NativeObjectsRejected(work.state->rejected);`
  and keep the Publish check for rejections discovered in `LayoutOne` (script targets, diagram width). `Advance`
  already resets `next_` on any `std::exception` (2845-2849). Trade-off: when early and late rejections coexist the
  editor needs one more round (a single full pass currently collects both); the early exit wins in the realistic
  early-only case.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticGeometry.h`: inline `$\tag{1} x$` plus 50 paragraphs;
  drive `Advance` with `maximumLayouts = 1` and accumulate `layoutsPrepared` until `NativeObjectsRejected` (the
  throwing call's own count is lost, hence the budget of one). Assert the total stays below the paragraph count.
  Fails at `6df17007` (at least 50). Add a mixed early-plus-late case asserting the sequence still converges.
- **Verifier notes**: Mechanism confirmed. The reviewer's trigger "every first preparation after each syntax change"
  was overstated; the correct trigger is each rediscovery (width, dpi, typography, resource changes, full replacement).

### SEM-07 — PaintIndex staging spends one cooperative step per node and duplicates `PaintIndex::Build`; the production path is untested

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf, simplification, test gap · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `b5e5b6fd`, `6cf85f50`
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:2745-2829`, `src/Controls/DxUi.PaintIndex.h:28-53`,
  `src/Controls/DxUi.RichTextEditor.cpp:3451`, `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:8-41`
- **What**: After a one-step `reserve`, phase 1 does one `nodes.emplace_back()` per block step, phase 2 one leaf per
  step and phase 3 one parent per step, for five indexes; each iteration ends in `++result.blockSteps` (2842). That is
  about 3L plus N steps per index (L is the power of two at least N), roughly 131K steps for the 32,768-element cap
  (`include/DxUi/RichSemanticDocument.h:126`), about 32 extra turns at 4,096 steps per turn; a 1,000-element document
  costs about one turn. The node fill after `reserve` is allocation-free POD work and need not be split per element.
  `PaintIndex::Build` implements the same tree transactionally and is what production uses only for the editor's
  decoration index (`DxUi.RichTextEditor.cpp:3451`) and what `TestPaintIndexTransactionalBuild` exercises; the geometry
  staging is a second implementation with no direct test of leaf order, `{inf, -inf}` sentinels for `nullopt` bounds
  or root coverage.
- **Failure scenario**: a maximum-size document pays tens of extra cooperative turns on every edit before the new
  geometry can publish.
- **Fix**: In phase 0 replace the per-step `emplace_back` loop with `index->nodes.resize(index->leaves * 2u)` (one
  bounded allocation-free step after `reserve`, at most about 512 KiB), and process phases 2 and 3 in fixed chunks
  (for example 1,024 per step). Better: give `PaintIndex` one resumable `BuildStep(cursor, chunk)` used by both the
  geometry and the decoration index, and test that single implementation against `Build`.
- **Regression test**: Drive a 10k-element document with `maximumBlockSteps = 4096` and assert the total
  `blockSteps` of the PaintIndex tail is bounded by a small constant times N divided by the chunk size (a diagnostics
  counter or a before/after comparison makes the phase observable); unit-test the staged builder against `Build` on
  random bounds including `nullopt`.
- **Verifier notes**: Step counts verified. The 3-DIP `paintOutset` (`PaintIndex.h:59`) is by design.

### SEM-08 — Linear per-paint and per-query scans that the paint index could serve

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed (scope narrower than stated)
  · **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:3426-3428`, `src/Controls/DxUi.RichSemanticGeometry.cpp:3113-3140`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:3243-3258`, `src/Controls/DxUi.RichSemanticGeometry.cpp:3106-3112`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:908-927`, `src/Controls/DxUi.RichSemanticGeometry.cpp:668-690`
- **What**: Composition iterates all image placements to find a box's own, but only for boxes with `box->images` set
  (assigned at 1957 inside the image block), not every visible box. The worst case is the number of image-bearing
  boxes times at most 128 placements. `VisibleSourceRange`, `VisibleRange`, `SourceSiteForSyntaxNode`, `NearestBox` and
  `SemanticBox::Caret` are O(boxes) with at most 4,096 boxes and run per query or prepare, not per paint. Real impact is
  negligible.
- **Failure scenario**: none measurable; hit-test and viewport queries on a maximum-size document do a few thousand
  comparisons.
- **Fix**: Optional. Record each box's `[firstImage, lastImage)` placement range at `LayoutOne` time and use
  `paintBoxes.ForEach` in `VisibleRange` and `VisibleSourceRange`. Treat as won't-fix unless profiling shows otherwise.
- **Regression test**: microbenchmark only; no functional test.
- **Verifier notes**: The "every visible box" wording of the original finding is incorrect.

### SEM-09 — Callout `skipUntil` leaks into later siblings and silently disables definition lists inside callouts

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (opt-in feature) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.RichSemanticDocument.cpp:2327-2337`, `src/Controls/DxUi.RichSemanticDocument.cpp:2177`,
  `src/Controls/DxUi.RichSemanticDocument.cpp:1382`, `src/Controls/DxUi.RichSemanticDocument.cpp:1158`
- **What**: `skipUntil` exists to clip the callout marker line of the first paragraph, but `Visit` is copied for every
  later sibling (`auto sibling = visit`, 2177) and descendant, so it stays non-zero for the whole callout body. The only
  consumers are the inline skip at 1158 (harmless for later content, whose `source.last` exceeds the value) and
  `DefinitionList`, which refuses on any non-zero value (1382).
- **Failure scenario**: RichMarkdown with DefinitionLists enabled: `> [!note] T`, then a blank `>` line, then
  `> Term` and `> : desc`. The second paragraph never becomes a definition list, unlike the same content in an ordinary
  quote. (The single-paragraph form `> [!note]` / `> Term` / `> : desc` is a different case: its first paragraph
  contains the clipped marker line, and refusing it is correct.)
- **Fix**: Change the guard at 1382 to `visit.skipUntil > node.block.source.first` instead of non-zero. This still
  refuses the clipped first paragraph and allows later ones. Equivalent alternative: clear `skipUntil` (and
  `clipFirst`/`clipLast`) when pushing the next sibling of the skipped paragraph and when descending into a new block.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticDocument.h`: the blank-line source above with the
  extension-enabled parser; assert a `DefinitionList` element exists (fails today). Add the negative case
  `> [!note]\n> Term\n> : desc` and assert no definition list (guards over-correction).
- **Verifier notes**: Propagation confirmed. The original failure example was wrong as written; the finding itself
  stands.

### SEM-10 — Code copy action follows the host direction while the code frame, gutter and numbers follow the block's auto direction

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug or design question · **Verdict** Plausible ·
  **Confidence** medium · **Effort** S
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:3300-3312`, `src/Controls/DxUi.RichSemanticGeometry.cpp:1610-1613`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:1703`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2592-2596`,
  `src/Controls/DxUi.RichTextEditor.cpp:2861`, `src/Controls/DxUi.RichSemanticGeometry.h:14-20`
- **What**: `Draw` places the copy button and clips the language label from `state.options.readingDirection` (host);
  `GetSemanticCodeCopyBounds` uses `IsRightToLeft()` (host). The gutter, line numbers and frame use `box->rtl`, which
  is the first-strong direction when `autoParagraphDirection` is on. Draw and hit-test agree, so this is visual only.
- **Failure scenario**: `autoParagraphDirection = true`, a right-to-left code block in a left-to-right host: the gutter
  is on the right and the copy button is on the right (host); a left-to-right block in a right-to-left host puts both on
  the left. In the all-same-direction case the button sits opposite the gutter; in the mixed cases it sits on the
  gutter's side. The header strip is above the line area, so nothing overlaps.
- **Fix**: Decide the intent first. If the copy action is host UI chrome (it is the only control of its kind, and the
  spec is silent), keep the behavior and document the chrome/content split in `Specs/UI/UI_ControlsAndLayout.md`. If it
  should follow the block, store the block direction with the code element bounds (for example the first box's
  `box.rtl`) and use it in both `Draw` and `GetSemanticCodeCopyBounds`.
- **Regression test**: Only if the second option is chosen: `Tests/Embedded/Embedded.Tests.RichGeometryResources.h`,
  `TestEmbeddedCodeLineNumberPixels`: add an auto-direction right-to-left code block in a left-to-right host and assert
  the copy bounds and the gutter lie on the chosen sides.
- **Verifier notes**: Facts confirmed; whether it is a defect is a design call. File as a question to the developer
  rather than a bug.

## Refuted

No finding was refuted.

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| none | - | - |

## Area architecture and simplification proposals

1. **One staged-index implementation.** Proposal: replace the inline PaintIndex phase with a resumable
   `PaintIndex::BuildStep(cursor, chunk)` shared with the editor's decoration index (`src/Controls/DxUi.RichTextEditor.cpp:3451`
   currently calls the transactional `Build`). Rationale: two implementations of the same tree, only one tested
   (SEM-07). Files: `src/Controls/DxUi.PaintIndex.h`, `src/Controls/DxUi.RichSemanticGeometry.cpp:2745-2829`. Risk low,
   effort S. Before: two builders, per-node cooperative steps. After: one builder, chunked steps, one test oracle.
2. **Make script color capability an explicit input, not a phase-lifetime side effect.** Proposal (replaces the
   reviewer's "keep raster ownership per phase"): compute a `colorScriptCapable` bit once in the `Preparation`
   constructor and make `State::scriptTarget` an identity pointer (SEM-01). Rationale: the raster is consumed by
   palettes, math colors, SVG upload and script validation, and only the first three need the target alive; a phase-local
   reset caused SEM-01. Files: `src/Controls/DxUi.RichSemanticGeometry.cpp:817,981,1003,1994,2456`,
   `src/Controls/DxUi.RichTextLayout.cpp:610`. Risk low, effort S. Before: strong reference pinned by the published
   geometry. After: no long-lived target reference in published state.
3. **Reuse keys as named structs.** Proposal: a small `ReuseKey{document, factory, target, epochs,
   options-without-width}` with `SameExceptWidth` and `SameTypography` helpers for box reuse (1786-1875), marker reuse
   (1304-1330), native reuse (2189-2197) and image reuse (2178-2186). Rationale: makes it obvious which inputs each
   cache depends on (SEM-02 is a key stricter than its data). Risk low, effort M. Before: four hand-rolled predicates.
   After: one definition per reuse level.
4. **Generated block headers through one helper.** Proposal: `BeginGeneratedParagraph(visit, element, anchor)` that
   applies the marker step once, used by callout headers, front-matter "Properties", Details summaries and definition
   parts. Rationale: closes SEM-04 and prevents the next instance. Files:
   `src/Controls/DxUi.RichSemanticDocument.cpp:1445,1926,1989,2300`. Risk low, effort S-M. Before: hand-built
   `Paragraph` calls. After: one entry point.
5. **Display-span-aware text predicates.** Proposal: a per-document prepared "authored display" mask (bits or span list
   built once at map publication) serving direction detection (SEM-03), word traversal (the exclusion build at
   `src/Controls/DxUi.RichSemanticDocument.cpp:123-165`) and export. Risk low, effort M. Before: each caller re-derives
   authored text from the display string and spans. After: one shared structure.
6. **(Reviewer proposal withdrawn: false premise.)** The reviewer proposed declaring defaulted move operations on
   `SemanticHeading` and `SemanticElement` (`include/DxUi/RichSemanticDocument.h:50-56,83-89`) because their declared
   copy operations suppress implicit moves and vector growth copies every `name`/`slug` string, calling this "free".
   The premise is shaky: in MSVC Debug with iterator debugging a `std::wstring` move is `noexcept` yet allocates a
   container proxy, a pattern this repository works around deliberately (comments at
   `src/Controls/DxUi.RichSemanticGeometry.cpp:782` and `:1006`; the `(0u, L' ')` string constructors in these very
   structs exist for the same reason) and that the `failAllocationAfter` harness would turn into termination. Do not
   adopt without running the allocation-failure suites in Debug and ASan Debug; reserving capacity up front is the
   safer way to avoid growth copies.
7. **Named result for `HorizontalTextBounds`.** Proposal: return a small struct instead of `std::pair<float, float>`
   to remove the `{left, width}` ambiguity that caused R28-04 (the earlier review suggested this; not done). Files:
   `src/Controls/DxUi.PreparedLayoutAccess.h`, call sites at `src/Controls/DxUi.RichSemanticGeometry.cpp:1403,2014,2025`.
   Risk low, effort S. Before: positional pair. After: named fields.

## Test-quality observations

- **Color scripts** (`Tests/Support/Support.Tests.RichSemanticGeometry.h:435-450`): only the null-target rejection is
  tested. No test passes an `ID2D1DeviceContext7` target and expects acceptance, which is why SEM-01 survives. The
  `testRichScriptTargetUnsupported` tests (`Tests/Support/Support.Tests.RichSemanticEditor.h:2297-2312, 3256-3271`)
  force the fallback and cannot see the bug.
- **PaintIndex** (`Tests/Embedded/Embedded.Tests.RichGeometryResources.h:8-41`): `TestPaintIndexTransactionalBuild`
  tests `PaintIndex::Build`, which semantic geometry does not use. `count(10.f, 11.f) == 1u` ("exact-boundary stroke
  ink") passes only because of the fixed 3-DIP query outset (`PaintIndex.h:59`), by design, not because ink is
  modelled. The overhang commit `1c166856` has a test, but nothing checks that a box's paint-index node contains
  `DrawingVerticalBounds` on the staged path.
- **Snapshot interleave, `87280611`** (`Tests/Controls/Controls.Tests.RichDocument.cpp:258-285`, confirmed): the reader
  waits for `publishedRound` and the owner waits for `observedRound`, and the owner touches the document only before
  publishing, so reads and mutations never overlap in time. The test, still named "concurrent snapshot reads", can no
  longer detect a data race between immutable snapshot reads and owner edits, undo or redo; it still checks
  cross-thread visibility and that an old snapshot stays valid after mutation. Keep the deterministic rendezvous
  variant and restore a free-running reader loop (ideally under ASan or a race detector) for the race property.
- **Table staging, `64ff0e7d`** (`Tests/Support/Support.Tests.MarkdownCapacity.h:83-99`, confirmed): the parser-level
  loop dropped from 6,130 one-event turns to 128, which ends inside header-line scanning (the two header lines are
  6,002 units), so the parser-level check no longer reaches table-cell generation. That property is now covered only
  by calling `MarkdownBlocks::AdvanceTableCell` directly for 128 cells, which proves block-level granularity but not
  that the parser forwards one unit per cell event. Add back one parser-level probe that advances past 6,002 units and
  checks one cell per event.
- **Composition guard** (`Tests/Embedded/Embedded.Tests.RichSemanticResources.h:270-300`, from `47e534e8`): the old
  "fails closed with `Unavailable`" assertion became "staging `bad_alloc` propagates". Verified benign: the guard now
  reads a cached scalar without allocating, `ApplyMarkdownCommand` is not `noexcept`, and `docs/rich-text.md:1327`
  already states allocation failures change no source. No action.
- **Missing negative and edge tests**: definition list inside a list item or footnote (SEM-04), auto direction with a
  leading image (SEM-03), math and bitmap reuse across image completion and frame replacement (SEM-02),
  supplementary-plane `CountWords` (SEM-05), definition list in a later callout paragraph (SEM-09), `FindInSource` match
  straddling CRLF and a query longer than half the source.

## Questions for the developer

1. RTL Subtle tables with `tableOverflow` anchor at `x = 0` and grow rightward (`src/Controls/DxUi.RichSemanticGeometry.cpp:1653-1659`),
   so the logical first column of a wide RTL table starts off-screen to the right. Is that intended? Recommended
   answer: no; RTL overflow should start at the right edge and extend left (negative horizontal origin or scroll
   anchored right), or at minimum be documented. Premise holds.
2. Was the `imageResources == images` requirement in `previousImages` deliberate? Answered by the verifier: yes, as the
   documented width-only scope (`Specs/UI/UI_ControlsAndLayout.md:1209`, `:2199`), not a lifetime concern. Recommended:
   relax it (SEM-02) and update both spec sentences.
3. `FindInSource` calls `FindStringOrdinal` repeatedly from `first + 1` on whole-word or boundary misses
   (`src/Controls/DxUi.RichSemantic.cpp:48`); the worst case is O(n times m) on the owner thread for a 64K source and a
   query near 32K units. Recommended answer: cap the query length or make the search cooperative for hosts that pass
   user-typed queries. Premise holds.
4. The 128-slot source-fallback budget is shared by native objects and script paragraphs. Recommended answer: yes, give
   script-paragraph rejections their own budget, or raise the cap; the editor-side cap
   (`src/Controls/DxUi.RichTextEditor.cpp:3519`) must change with it. Moot once SEM-01 is fixed, because script
   rejections then occur only on targets without color support.
5. `ParagraphDirection` ignores generated and object spans only implicitly. Are localized generated labels (front-matter
   "Properties" at `src/Controls/DxUi.RichSemanticDocument.cpp:1989`, callout `! ` prefix) meant to take part in
   first-strong detection? Recommended answer: exclude generated and object spans (SEM-03). The "Properties" header is
   its own paragraph and may keep taking the direction of its localized label; the `! `, bullet and fold glyphs are
   neutral and unaffected. Premise holds.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.RichSemanticGeometry.cpp` | Diff fully walked (2,690 diff lines); current functions read in full: helpers 53-80, `NativeImageGeometry` constructor/`PrepareOne`/`PrepareMath`/`Placed` 246-653, `SemanticBox`, `SameOptions`, `SemanticDisplayInput`, `State`, `Preparation` (all members incl. `MeasureMarkerOne`, `MeasureTableOne`, `AllocateTableOne`, `LayoutOne`, `PlaceRowOne`), `Begin`, `Advance` (all phases), lifecycle, all queries, `Draw` (700-3477). Lines 80-245 (placeholder COM class, placement structs) diff only. |
| `src/Controls/DxUi.RichSemanticGeometry.h` | Reviewed fully. |
| `src/Controls/DxUi.RichSemanticDocument.cpp` | Diff fully walked (2,253 diff lines); read in full: `VisitSemanticWords`, `CountWords`, `ResolveHeadingFragment`, `BuildHeadingTableOfContents`, `SourceEditingRange`/`IsSourceEditingElement`, abbreviation preparation/emission, `FindCallout`, `RecordHeading`, `CloseElement`, `CompleteImage`, `HtmlTag`/`HtmlBlock`, `Inline`, definition-list functions, `Element`, `Prefix`/`SeparateMarker`, `BeginLeafParagraph`, `LeafBlock`, callout and front-matter paths of `Block`, `Step`, `PrepareSourceFormats`, `SetSoftBreakPolicy`, `SetAccessibilityLabels`, `BeginWithNativeFallbacks`. Diagram alternative phases (`AdvanceDiagram`): diff and touched functions. |
| `include/DxUi/RichSemanticDocument.h` | Reviewed fully. |
| `src/Controls/DxUi.RichSemantic.cpp` | Diff and touched functions (`FindInSource` in full; `Build`/`ApplySourceEdits` validation hunks). |
| `include/DxUi/RichSemantic.h` | Diff reviewed (`FindInSource` API). |
| `src/Controls/DxUi.RichSemanticValues.h` | Diff and touched functions (`NormalizeSemanticLeaf` NUL path, `ResolveSemanticValue`, `SemanticOutput` in full). |
| `src/Controls/DxUi.PaintIndex.h` | Reviewed fully. |
| `src/Controls/DxUi.SemanticExtensions.h` | Reviewed fully (diff and admission helpers). |
| `Tests/Support/Support.Tests.RichSemanticGeometry.h` | Diff skimmed; script/emoji (380-470), definition/disclosure (620-660) and table minimum/overflow (1790-1860) sections read. |
| `Tests/Controls/Controls.Tests.RichSemanticGeometry.cpp` | Diff reviewed. |
| `Tests/Support/Support.Tests.RichSemanticDocument.h` | Diff assertions listed and judged; definition/abbreviation fixture read (210-240). |
| `Tests/Controls/Controls.Tests.RichSemanticDocument.cpp` | Skipped beyond stat (time); covered indirectly through the Support header it runs. |
| `Tests/Support/Support.Tests.RichSemantic.h` | Diff assertions listed; `FindInSource` tests read (160-175). |
| `Tests/Embedded/Embedded.Tests.RichGeometryResources.h` | Outline read; PaintIndex test read fully. |
| `Tests/Embedded/Embedded.Tests.RichSemanticResources.h` | Diff reviewed fully. |
| `Tests/Support/Support.Tests.SemanticSourceBlock.h` | Diff reviewed fully. |
| `Tests/Controls/Controls.Tests.RichDocument.cpp`, `Tests/Support/Support.Tests.MarkdownCapacity.h` | Only the `87280611` and `64ff0e7d` hunks, for the snapshot-interleave and table-staging angles. |
