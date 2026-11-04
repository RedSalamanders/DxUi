# Rich layout, rendering and images: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: v0 document layout, `PreparedLayout` and the
script renderer, source guides and ink, code ink, bidi, annotations, text elements, `RichDocument` history, and the
`RichImage` decode, path and resource pipeline.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| RENDER-01 | P1 | Confirmed | Source line-number gutter and whitespace marks re-stage the whole document on every edit and withhold the editor frame | `src/Controls/DxUi.RichSourceGuides.h:58` | M |
| RENDER-02 | P2 | Confirmed | With `lineHeightMultiplier > 0`, v0/Source preparation drops all chunk reuse | `src/Controls/DxUi.RichDocumentLayout.cpp:307` | M |
| RENDER-03 | P3 | Confirmed | Document-controlled `//host/share/x.png` and `file://host/...` become unclassified `LocalFile` UNC requests | `src/Controls/DxUi.RichImagePaths.h:333` | S-M |
| RENDER-04 | P3 | Confirmed | `RichImage::LoadFile` opens without `SECURITY_SQOS_PRESENT` | `src/Controls/DxUi.RichImage.cpp:484` | S |
| RENDER-05 | P3 | Confirmed | A color-font glyph in a script paragraph requires `ID2D1DeviceContext7`, so the paragraph falls back to source | `src/Controls/DxUi.RichTextScriptRenderer.h:181` | M |
| RENDER-06 | P3 | Plausible | A paragraph with a script gets a fixed 1.45x uniform line height | `src/Controls/DxUi.RichTextLayout.cpp:360` | S |
| RENDER-07 | P3 | Confirmed | An SVG completion refused while SVG is disabled stays pending (undocumented) | `src/Controls/DxUi.RichImageResources.h:310` | S |
| RENDER-08 | P3 | Confirmed | `RichImage::Stride()` returns `width*4` for SVG resources whose `Pixels()` is empty | `include/DxUi/RichImage.h:73` | S |
| RENDER-09 | P3 | Confirmed | Decode copies the encoded bytes and `LoadFile` adds another full encoded buffer | `src/Controls/DxUi.RichImage.cpp:249` | S |
| RENDER-10 | P3 | Plausible | Script rendering binary-searches `projection.runs` but admission never checks order | `src/Controls/DxUi.RichTextLayout.cpp:247` | S |
| RENDER-11 | P3 | Confirmed | UIA annotation attribute writes into 256-slot stack arrays guarded only by a limit in another file | `src/Controls/DxUi.RichAnnotations.h:417` | S |
| RENDER-12 | P3 | Confirmed | Data and SVG policy toggles reset `failed` on every source kind | `src/Controls/DxUi.RichImageResources.h:76` | S |
| RENDER-13 | P3 | Confirmed | No test covers header-declared oversize dimensions or a GIF logical-screen bomb | `Tests/Support/Support.Tests.RichImage.h:383` | S |

## P1 findings

### RENDER-01 — Source line-number gutter and whitespace marks re-stage the whole document on every edit and withhold the editor frame

- **Severity** P1 (reviewer P1, verifier P1) · **Category** perf, bug · **Verdict** Confirmed · **Confidence** high for
  the mechanism, medium for the magnitude · **Effort** M · **Merged** RENDER-14 · **Introduced** `5277381d`
  (follow-ups `82729d33`, `a8ada883`) · see also EDIT-01 in `editor.md`
- **Location**: `src/Controls/DxUi.RichSourceGuides.h:58-139` (`Begin` 66-78, `Advance` 80-139, layout cap 92-93,
  source copy 69), `src/Controls/DxUi.RichTextEditor.cpp:6897-6915` (`PrepareSourceGuides`),
  `src/Controls/DxUi.RichTextEditor.cpp:3672-3678` and `:4037-4043` (S_FALSE return), `:4708-4709` and `:4877-4878`
  (paint gates), `src/Controls/DxUi.Controls.cpp:2431-2441`, `:2499-2509`, `:2575`,
  `src/Controls/DxUi.WindowHost.cpp:4642-4649`, `src/Rendering/Embedded.cpp:552-563`,
  `include/DxUi/DxUi.h:3195-3196`
- **What**:
  - Scope: opt-in. `RichTextAppearance::sourceLineNumbers` and `sourceWhitespaceMarks` default to false
    (`include/DxUi/DxUi.h:3195-3196`) and only Source view uses them. No sample or gallery enables them; two test files
    do. The feature is documented and supported (`docs/rich-text.md:314-318`).
  - `RichSourceGuides::Begin` replaces its stage whenever revision, layout generation, `numbers` or `marks` change
    (`DxUi.RichSourceGuides.h:66-78`). `Revision()` is the document revision, so every text edit restarts at source
    unit 0 and line 1 and copies the whole source (`source.Slice(0, source.Length())`, line 69).
  - `Advance` consumes at most 256 source units and at most 4 `PreparedLayout` objects per call. Each number is a
    complete `Document` -> `Project` -> `PreparedLayout` with its own `CreateTextFormat` and `CreateTextLayout`
    (lines 94-101; the cap is `if (layouts == 4u) return false;`, lines 92-93). There is no viewport limit and no cap,
    so every physical line of the document is staged. The code-block gutter, by contrast, has a documented cap of 4096
    number layouts. The Source gutter is bounded only by `Document::MaximumUnits` (64 Ki units,
    `include/DxUi/RichDocument.h:107`), i.e. at most 65,537 retained layouts.
  - Marks (the merged RENDER-14) cost one work unit per source unit and two `SourceCaret` calls per space, tab or
    line ending (lines 119-129), so a marks-only editor needs up to 256 turns for a 64 Ki-unit source, with the same
    restart on every edit.
  - Paint draws numbers and marks only when `sourceGuides.Current(Revision(), layout.Generation(), ...)`
    (`DxUi.RichTextEditor.cpp:4708-4709`, `:4877-4878`). The previously published `numbers_` and `marks_` are never
    drawn after an edit.
  - While `PrepareSourceGuides` is not done, `PrepareContent` returns S_FALSE
    (`DxUi.RichTextEditor.cpp:3672-3678`, `:4037-4043`). `ControlHost::PreparePendingPageLayout` records that as
    `_contentPreparationPending` (`DxUi.Controls.cpp:2440-2441`), aggregates it into `_pendingContentPreparation`
    (`:2499`, `:2509`) and returns `layoutComplete && ! _pendingContentPreparation` (`:2575`), which is false.
    WindowHost then returns without rendering (`DxUi.WindowHost.cpp:4642-4649`), and EmbeddedHost returns S_FALSE
    before any `BeginDraw` (`src/Rendering/Embedded.cpp:552-563`). **The whole editor frame (new text, caret,
    selection) is withheld while guides are pending, not only the gutter.** The contract agrees:
    `Specs/UI/UI_ControlsAndLayout.md:2211-2213` lists source guides among the stages `preparationPending` covers and
    exempts only overlay fallback dirtiness from withholding a ready frame.
- **Failure scenario**:
  - Source view with `sourceLineNumbers=true` and a 2,000-line document of ordinary lines. Each `Advance` stages 4
    lines, so about 500 owner turns elapse before the first frame is composed. The 600-line fixture in
    `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:641-646` already needs about 150 turns and asserts only that
    preparation yields.
  - The user types one character. The revision changes, `Begin` restarts at line 1, and the previous composed surface
    stays on screen for another ~500 turns. While typing continues the restart repeats, so the typed text does not
    appear until a pause long enough to finish the restage.
  - `sourceWhitespaceMarks=true` alone on a 64 Ki-unit source needs ~256 turns per edit with the same effect.
  - A 64 Ki-unit source of newlines holds 65,537 `PreparedLayout` objects, each with an `IDWriteTextFormat` and an
    `IDWriteTextLayout` (memory not measured; the reviewer's estimate is tens to hundreds of MB).
  - Wall time per turn depends on the consumer: WindowHost re-invalidates at once, EmbeddedHost leaves the cadence to
    the consumer. The "8 s at 60 fps" figure applies only to a consumer that prepares once per vsync. The turn count
    and the restart on edit are provable; the wall time is not.
- **Fix**: the guides must never be able to withhold the frame and must not be O(document) per edit.
  1. Bound the work to the viewport. Stage only lines whose y lies in `[scrollDip - viewportHeight, scrollDip + 2 *
     viewportHeight]`. Find the first and last staged source offsets by hit-testing the editor layout at those y
     values (`RichEditorLayout`), and take the first physical line number by counting line endings in the snapshot
     prefix (CRLF is one ending; the trailing empty line stays addressable). Stage marks over the same source range
     only. Add the staged window to the `Current(...)` key so a scroll that leaves the window or a geometry generation
     change restages it.
  2. Make a number cost one `float` per line. Prepare the ten digit glyphs once per font, DPI and resource generation
     (`IDWriteFontFace::GetGlyphIndices` plus `GetDesignGlyphMetrics`; or a bounded LRU of at most 256 number layouts
     if shaping must be kept). Paint composes at most 10 digits per visible line from a stack array with
     `DrawGlyphRun`, honouring the existing trailing alignment (`options.textAlignment = TRAILING`) and the RTL gutter
     mirroring at `DxUi.RichTextEditor.cpp:4882-4883`.
  3. Take guides out of the frame-withholding path as a safety net. Call `EnableVisualContentPreparation()` for the
     editor, override `PrepareVisualContent` and `CancelVisualContentPreparation`, and stage the guides there, calling
     `RequestVisualContentPreparation()` to continue. Visual preparation does not enter the return value of
     `PreparePendingPageLayout` (`DxUi.Controls.cpp:2451-2458`, `:2575`). Pitfall: `PrepareVisualContent` is
     `noexcept`, so catch `std::bad_alloc` and keep the atomic publication that
     `TestSourceGuideAllocationAtomicity` pins (`Tests/Embedded/Embedded.Tests.RichGeometryResources.h:539-602`).
     `PrepareSourceGuides` must then no longer feed the S_FALSE result at `DxUi.RichTextEditor.cpp:3672` and `:4037`.
  4. Do not paint a previous publication after an edit: the stored y values are wrong after any inserted or deleted
     line ending. With 1 and 2 the gutter is current in the same turn, so keeping the `Current(...)` gate in paint is
     enough.
  - Keep: ACP, selection and clipboard unaffected, high-contrast brush choice, `SourceGutterWidth()` growth with the
    digit count, and the 256-unit cooperative budget if the window is still larger than one turn.
- **Regression test**: `Tests/Embedded/Embedded.Tests.RichGeometryResources.h`, extending
  `TestEmbeddedSourceGuidesPixels`.
  1. 2,000-line source, Source view, `sourceLineNumbers=true` and `sourceWhitespaceMarks=true`; prepare to completion
     with `PrepareRichEmbeddedContent`. Call `editor->ReplaceSelection(L"x")` (or `SetText` with one changed
     character) and run exactly one `view.Prepare(480u, 240u, 96.0f)`. Assert it returns S_OK, `view.NeedsPreparation()`
     is false, and the composited readback shows the edit and a gutter that matches the fully prepared reference
     capture. At `6df17007` the call returns S_FALSE and composes nothing, so the readback shows the stale surface.
  2. Add a diagnostic accessor for the retained number layout/glyph-run count and assert it is at most the visible
     line count plus a constant after preparing a 10,000-line source. At `6df17007` it equals the line count.
  3. Scroll by five viewports, run one `Prepare`, and compare the gutter pixels with a full-prepare capture.
  4. Update the existing yield assertion at lines 645-646 (`Check(view.NeedsPreparation(), "large guide preparation
     yields before publication")`), which pins today's behaviour, to assert that the frame is no longer withheld. Keep
     the supersession check at lines 647-649 and the allocation-atomicity test.
- **Verifier notes**:
  - The reviewer wrote that S_FALSE does not block the frame. That is false (trace above); the verifier corrected it.
  - The reviewer's original fix "keep painting the previous publication with old y values" and its test "assert the
    gutter is still painted after one Prepare" were dropped. Painting is not reached while the frame is withheld, and
    old y values are misaligned after an edit.
  - Checked: the per-revision restart, the 4-layout and 256-unit budgets, that Paint draws nothing unless `Current`,
    the absence of any cap or virtualisation, the opt-in defaults, and that no in-repo consumer enables the feature.
    EDIT-01 and EDIT-02 in `editor.md` cross-reference this finding.
- **Lead note**: stays P1 by lead decision. P1 holds because the whole frame is withheld in a documented, supported
  configuration whose purpose is viewing larger source. The scope is opt-in (default off), so it is not a default-path
  regression; under a rubric that reserves P1 for default-path defects it would be P2.

## P2 findings

### RENDER-02 — With `lineHeightMultiplier > 0`, v0/Source preparation drops all chunk reuse and lays out the whole document on every edit

- **Severity** P2 (reviewer P2, verifier P2) · **Category** perf · **Verdict** Confirmed · **Confidence** high for the
  mechanism, medium for the magnitude · **Effort** M (fix), S (measurement) · **Introduced** `b5869cbc` (R28-03 fix)
- **Location**: `src/Controls/DxUi.RichDocumentLayout.cpp:307-313`, `:460-498`, `:28-60` (`SameStyles`);
  `include/DxUi/DxUi.h:3207`, `include/DxUi/RichTextLayout.h:23`
- **What**:
  - `if (options.lineHeightMultiplier > 0.0f)` sets `request->exactPlacement = false` and jumps to
    `Preparation::Phase::Reference` (`:307-313`). The Reference phase reuses the old whole layout only if
    `SameStyles(blocks, state_->reference->Content())` holds, and `SameStyles` compares the joined display text
    (`:46`), so any text edit builds one `PreparedLayout` over `JoinBlocks(0, blocks.size())` (`:474-488`), up to
    64 Ki units, as a single indivisible preparation unit (the comment at `:467-468` says it is not a CPU-time
    guarantee). The previous chunked path could reuse unchanged chunks (`Phase::Reuse`, `:314-318`).
  - Reach: the multiplier defaults to 0; the gallery's source appearance sets 1.6
    (`Tests/Controls/DxUi.Tests.Gallery.cpp:1183`).
    It affects v0/Source through `PreparedDocumentLayout`; the semantic Rich path has its own handling. It is a
    normal documented appearance field, so any consumer that sets it for Source view is affected.
  - Not measured: `Tests/Embedded/Embedded.Tests.RichPreparationBenchmark.h` has no Source mode and no line-height
    setting, and the delivery receipt only says "correctness by whole-layout fallback, not positioned-chunk reuse"
    (`Measurements/WipDelivery_2026-11-02/raw/historical/historical-per-id.md.txt:93`). The AGENTS.md paired-baseline
    rule was not applied.
- **Failure scenario**: Source view, `lineHeightMultiplier=1.6`, a 60 Ki-unit document, one typed character. The full
  text is re-shaped and re-broken in one owner turn on every keystroke (estimated tens of ms; not measured).
- **Fix**:
  1. Measure first: add a Source-mode, multiplier 1.6, 1,000+ line, single-key-edit scenario to the preparation
     benchmark and record the paired receipt against the multiplier-0 chunked path.
  2. If the measurement confirms a regression, restore chunk reuse under uniform spacing: apply the uniform spacing to
     positioned chunks too and translate the draw and hit-test origin by `chunk.y` instead of the private origin line
     (the two-unit `\ufffc\n` prefix, `src/Controls/DxUi.RichTextLayout.cpp:273-303`). This is the alternative the
     R28-03 entry already listed (`Specs/Plans/Done/CodeReview_2026-10-28.md:98-106`). Touches `Prepare`, `HitTest`,
     `Caret`, `Range` and `Draw` in `DxUi.RichTextLayout.cpp` and the Reference/Placement phases. Pitfall: keep the
     float accumulation parity (`AddCoordinate`) that the private origin protected.
- **Regression test**: `Tests/Embedded/Embedded.Tests.RichPreparationBenchmark.h`: a Source-mode scenario with
  multiplier 1.6 that reports `edit_layout_advances` and `wholeLayoutSourceUnits`. In
  `Tests/Controls/Controls.Tests.RichDocumentLayout.cpp`: a one-character edit to a 3-chunk document must report
  `wholeLayoutSourceUnits < SourceLength()` (or `reusedChunks > 0`). At `6df17007` `wholeLayoutsPrepared == 1` and
  `wholeLayoutSourceUnits == SourceLength()`.
- **Verifier notes**: no false premise. The reviewer's claim that the old path was O(changed chunk) holds only while
  every chunk passes `PositionedExactly()` and the origin check (`:445-459`); otherwise the old path also fell back to
  a whole layout.

## P3 findings

### RENDER-03 — Document-controlled `//host/share/x.png` and `file://host/...` become unclassified `LocalFile` UNC requests

- **Severity** P3 (reviewer P2, verifier P3) · **Category** security (hardening), api · **Verdict** Confirmed ·
  **Confidence** high that the path is accepted and unclassified · **Effort** S-M
- **Location**: `src/Controls/DxUi.RichImagePaths.h:333-348` (raw spelling), `:94-110` (UNC parse, server checked only
  for `:`), `src/Controls/DxUi.RichImageResources.h:200-214`, `src/Controls/DxUi.RichImage.cpp:476-485`,
  `include/DxUi/RichImage.h:41-47`, `:112-114`
- **What**:
  - A destination with no colon is copied raw; `ParseLocalFilePath` turns `//evil.example/s/x.png` into
    `\\evil.example\s\x.png`. `file://evil.example/s/x.png` resolves the same way and is tested as intended
    (`Tests/Support/Support.Tests.RichImagePaths.h:60`). Server names are not validated, so `\\h@SSL\s\a.png` and
    `\\localhost\pipe\x` are accepted. The request is `RichImageSourceKind::LocalFile`; the only way to tell a UNC
    request from a drive path is to string-match `FilePath()`.
  - The library never calls `RichImage::LoadFile` itself (grep: only the sample loader and tests do), and the shipped
    sample loader rejects UNC by default (`Samples/EmbeddedControls/RichImageLoader.h:235`). The contract delegates the
    policy to the caller: "UNC access follows caller policy" (`include/DxUi/RichImage.h:113-114`), "The caller
    authorizes file access, including any UNC policy" (`Specs/UI/UI_ControlsAndLayout.md:1187`), and
    `docs/rich-text.md:578-590` tells applications to inspect `SourceKind()` and authorize each source.
  - The spec sentence "malformed/remote URI schemes MUST NOT become file requests"
    (`Specs/UI/UI_ControlsAndLayout.md:1110-1112`) is about schemes; `//host` has no scheme and the same paragraph
    makes server roots part of the local-path contract. So the contract is not violated.
- **Failure scenario**: a consumer such as RedSalamander sets its loader's `allowNetworkFiles` so that documents on
  shares can use relative UNC images. A downloaded Markdown file contains an image whose destination is
  `//attacker.example/s/a.png`. The loader calls `LoadFile`, the SMB redirector authenticates to the attacker, and the
  user's NTLM credentials leak. The destinations `\\attacker\s\a.png` and `file://attacker/s/a.png` expose the same
  thing, so the `//` spelling only disguises it. Requires the consumer to opt in to network files.
- **Fix**:
  1. Add typed classification, non-breaking: `RichImageRequest::IsNetworkPath()` and `NetworkServer()`
     (`std::wstring_view`, empty for drive paths), carried on `RichImageRequestState` from `ParsedLocalFilePath::unc`
     and `server` (`DxUi.RichImagePaths.h:24-40`). Consumers then write a same-server/same-share policy without
     parsing.
  2. Validate the server component in `ParseLocalFilePath` (`:94-110`): reject `@`, `;`, and `%` after URI decoding
     (WebDAV and redirector decorations), as for the share.
  3. Optional, product decision: refuse the bare scheme-relative `//host/...` spelling unless the server and share
     equal the document's own. It would be inconsistent with accepting `file://host/share/...`, so decide both
     together.
  4. Do not make `LoadFile` reject UNC by default: it contradicts the documented "caller policy" and breaks consumers
     that already authorize shares.
- **Regression test**: `Tests/Support/Support.Tests.RichImagePaths.h`: for `ResolveLocalImagePath(L"C:\\docs\\a.md",
  L"//evil.example/s/a.png")`, `file://server/share/a.png` (keeps the existing expectation at line 60) and
  `\\h@SSL\s\a.png`, assert the request reports `IsNetworkPath()` with the right server (and the `@` form is rejected
  once server validation lands); `C:\docs\a.png` reports false. At `6df17007` there is no classification and the
  `@SSL` form resolves.
- **Verifier notes**: the downgrade from P2 follows from the explicit caller-owned policy and the default-deny sample.
  The reviewer's remark that a same-share policy cannot be written without re-parsing is overstated (a prefix
  comparison against the document path works). The missing typed classification is the real gap.

### RENDER-04 — `RichImage::LoadFile` opens files without `SECURITY_SQOS_PRESENT`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** security (hardening) · **Verdict** Confirmed ·
  **Confidence** high that the flag is missing, low on exploitability · **Effort** S
- **Location**: `src/Controls/DxUi.RichImage.cpp:484-489`
- **What**: `calls.create(native.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
  FILE_FLAG_SEQUENTIAL_SCAN, nullptr)` passes no SQOS flags, so a UNC path to a named pipe
  (`\\localhost\pipe\name`, accepted by `ParseLocalFilePath`) is opened at the default impersonation level. The
  `GetFileType != FILE_TYPE_DISK` check runs after the open, when the connection has already been made. Reachable only
  when the consumer calls `LoadFile` on a UNC path (see RENDER-03).
- **Failure scenario**: with network files authorized, an image destination `//localhost/pipe/somepipe` connects the
  worker to a local pipe server. Exploitation is doubtful: a pipe server cannot impersonate a client until it has read data from the
  pipe (`ImpersonateNamedPipeClient` fails otherwise; medium confidence), and this client never writes, closing the
  handle right after `GetFileType` (`:488-489`).
- **Fix**: OR `SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION` into the flags at `:485`. It is harmless for disk
  files. Optionally reject the share names `pipe` and `IPC$` in `ParseLocalFilePath`.
- **Regression test**: `Tests/Support/Support.Tests.RichImage.h`: use the `RichImageFileCalls` seam installed by
  `FileReadFixture` (lines 97-135) to capture `dwFlagsAndAttributes` passed to `create` and assert
  `SECURITY_SQOS_PRESENT` is set. Fails at `6df17007`.
- **Verifier notes**: hardening only.

### RENDER-05 — A color-font glyph in a script paragraph requires `ID2D1DeviceContext7`, so the whole paragraph falls back to source

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug (platform degradation) · **Verdict** Confirmed ·
  **Confidence** high for the code path, medium for the OS boundary · **Effort** M · **Introduced** `309dc673`,
  `f0e28b86`
- **Location**: `src/Controls/DxUi.RichTextScriptRenderer.h:157-189` (`requiresColor |= IsColorFont()` for every glyph
  run), `src/Controls/DxUi.RichTextLayout.cpp:610-628` (`ScriptTargetSupported`) and `:634-635`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:1994-2001` (`next.Reject`), `:2832`,
  `src/Controls/DxUi.RichTextEditor.cpp:3477-3510` (`rejectNativeObjects`)
- **What**:
  - Staging records `requiresColor` when any font face drawn in the layout is a color font, script run or not.
    `ScriptTargetSupported` then requires `QueryInterface(ID2D1DeviceContext7)` and returns false on E_NOINTERFACE.
    Geometry rejects the owning element, `NativeObjectsRejected` reaches `rejectNativeObjects`, and the script-bearing
    paragraph, heading or table cell shows its exact source. There is no monochrome fallback before the rejection.
  - `ID2D1DeviceContext7` is declared under `#if NTDDI_VERSION >= NTDDI_WIN10_CU` in the SDK's `d2d1_3.h` (the
    Windows 11 era after 22H2), so it is absent on Windows 10 and Windows 11 21H2.
    `Specs/Build/Build_ToolchainAndConsumption.md:35` lists Windows 10/11 as the required embedded matrix.
  - The behaviour is documented: "Color fonts require a compatible color-glyph target; unavailable support retains
    the entire affected HTML leaf as source" (`docs/rich-text.md:1756-1759`;
    `Specs/UI/UI_ControlsAndLayout.md:1378-1380`). Source is preserved, and the trigger needs a script plus a color
    glyph (emoji, some symbols) in the same block.
- **Failure scenario**: on Windows 10, `Area is m<sup>2</sup> 😀` is rejected during preparation and shows the HTML
  source; so does any `^x^` or `<sub>` paragraph that also contains an emoji.
- **Fix**: the proper fix is a color path for DC4-only targets: `IDWriteFactory4::TranslateColorGlyphRun` and per-layer
  `DrawGlyphRun`, `DrawColorBitmapGlyphRun` or `DrawSvgGlyphRun` (Windows 10 1703+), shared with the source-ink
  renderer's color drawing. Do not take the reviewer's "set `requiresColor` only for script runs" shortcut: the whole
  layout is drawn through the custom renderer, so non-script emoji would then draw monochrome or blank. Cheaper
  alternative: keep the documented source fallback and name Windows 10 and Windows 11 21H2 in the docs. Decide with
  the product owner (Question 3).
- **Regression test**: `Tests/Embedded/Embedded.Tests.RichTextResources.h`: a variant of the existing script raster
  test on a target whose `ID2D1DeviceContext7` query fails, with `<p>m<sup>2</sup> 😀</p>`; assert the paragraph is
  published natively with no reject diagnostic. A diagnostic switch like `testRichScriptTargetUnsupported`
  (`Tests/Support/Support.Tests.RichSemanticEditor.h:2297-2312`) scoped to "no color support" is simpler than wrapping
  the full device-context interface. Fails at `6df17007`.
- **Verifier notes**: downgraded from P2 because the fallback is the documented contract and the co-occurrence is
  narrow. The Windows 11 21H2 inclusion comes from the SDK header guard.
- **Lead note**: SEM-01 in `semantic-geometry.md` is the root cause that makes this fallback fire even on devices that
  do have `ID2D1DeviceContext7`. Fix SEM-01 first; this finding then covers only targets that really lack the
  interface.

### RENDER-06 — A paragraph that contains a script gets a fixed 1.45x uniform line height

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (visual) · **Verdict** Plausible · **Confidence**
  medium · **Effort** S · **Introduced** `309dc673`
- **Location**: `src/Controls/DxUi.RichTextLayout.cpp:360-390`
- **What**: `if (lineHeight > 0.0f || scripts)` sets ascent `1.05*size`, descent `0.4*size`,
  `height = max(size*lineHeight, ascent+descent)`, i.e. 1.45x, with `DWRITE_LINE_SPACING_METHOD_UNIFORM` for every line
  of a script-bearing paragraph at the default multiplier of 0. This is deliberate and pinned: the script-position
  oracle builds its parent layout with `SetLineSpacing(UNIFORM, 18.f * 1.45f, 18.f * 1.05f)`
  (`Tests/Support/Support.Tests.RichSemanticGeometry.h:349`).
- **Failure scenario**: a paragraph mixing a script with a tall script such as Burmese (Myanmar Text) could need more
  than 1.45x and its stacked marks could overlap the next line; Latin paragraphs next to it use about 1.33x. Neither
  effect is provable statically. "Clipped by chunk culling" is doubtful because `drawingTop/Bottom` come from
  DirectWrite's measured overhang (`:399-402`).
- **Fix**: use `height = max(default line height of the layout, 1.45 * size)`: measure the unspaced line heights
  first (`GetLineMetrics` on the layout before `SetLineSpacing`) and keep the 1.45x floor so raised and lowered glyphs
  keep their headroom. Do not fall back to `DWRITE_LINE_SPACING_METHOD_DEFAULT` with only wider `drawingTop/Bottom`: it
  removes the room between lines and a superscript would collide with the descenders above.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextLayout.cpp`: prepare a paragraph with a `Superscript` run
  and a tall-script run in a font whose default line height exceeds 1.45x (skip when the font is absent), and assert
  each line height is at least the unspaced line height. Keep the Latin oracle at
  `Support.Tests.RichSemanticGeometry.h:349` unchanged.
- **Verifier notes**: not refuted, but the effect on real tall-script fonts is unproven and Question 4 is answered: the
  1.45x is intentional.

### RENDER-07 — An SVG completion refused while SVG is disabled stays pending (undocumented)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api (documentation gap) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `a1c25b00`
- **Location**: `src/Controls/DxUi.RichImageResources.h:310`, `:342`, `:398`; `include/DxUi/DxUi.h:3408-3410`
- **What**: `Complete` returns false when the image has SVG bytes and `! svgEnabled_`, leaving the entry pending and
  dispatched; `Statistics().pendingSources` keeps counting it. This is intentional and pinned: the test asserts "SVG
  resources are refused by default without cancelling the caller request", then enables SVG and completes the same
  `jobs.front()` request (`Tests/Support/Support.Tests.RichImageResources.h:309-318`). The comment on
  `CompleteImageRequest` documents `false` only for "stale/canceled/foreign requests".
- **Failure scenario**: a consumer whose worker always calls `CreateSvg` for `.svg`, ignores the `false` return and
  shows "loading" while `pendingSources > 0` waits forever on a document with an SVG image and SVG disabled.
- **Fix**: document it. In `include/DxUi/DxUi.h` (comment at `CompleteImageRequest`), `include/DxUi/RichImage.h` and
  `docs/rich-text.md` (SVG paragraph near line 623): a completion refused because SVG is disabled returns false and
  leaves the request pending; complete it with `nullptr` to fail it, or enable SVG and complete again. Do not change
  the behaviour to a failure; that contradicts the pinned test.
- **Regression test**: none needed for behaviour (the existing test pins it); the doc change is validated by
  `validate.ps1`.
- **Verifier notes**: the reviewer's fix "accept as a failure" and its test (`pendingSources == 0 && failedSources ==
  1`) were dropped because they conflict with the intended and tested behaviour.

### RENDER-08 — `RichImage::Stride()` returns `width*4` for SVG resources whose `Pixels()` is empty

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `include/DxUi/RichImage.h:73-79`, `src/Controls/DxUi.RichImage.cpp:44-49`
- **What**: `Stride()` is `width_ * 4u` for every image. The SVG constructor leaves `pixels_` empty with `width_ > 0`.
  Internal callers are safe: the SVG branch of `PrepareOne` returns before `Stride()` or `Pixels()` is used
  (`src/Controls/DxUi.RichSemanticGeometry.cpp:421-424`, then `:543`, `:559-560`).
- **Failure scenario**: application code that copies `Pixels().subspan(y * Stride(), Stride())` from an image created
  with `CreateSvg` violates the `std::span` precondition. Such code only receives SVG images it created itself.
- **Fix**: return 0 when `pixels_` is empty, or add `bool IsSvg() const noexcept` and document that `Stride()` and
  `Pixels()` apply to raster images only.
- **Regression test**: `Tests/Support/Support.Tests.RichImage.h`: after `RichImage::CreateSvg`, assert
  `Stride() == 0` (or `IsSvg()`). Fails at `6df17007`.
- **Verifier notes**: low impact, public API wart.

### RENDER-09 — Decode copies the encoded bytes and `LoadFile` adds another full encoded buffer

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.RichImage.cpp:249-253` (`owned` copy), `:497-511` (`LoadFile` buffer, then
  `Decode`), `:41` (zero-initialised pixel vector), `:149` (GIF canvas), `:190` (`Orient`)
- **What**: `Decode` copies the caller's span into `owned` before `InitializeFromMemory`. Every WIC object is a local
  released before `Decode` returns and the span outlives the call, so the copy is redundant. `LoadFile` reads the file
  into `bytes` and then `Decode` copies again: a 16 MiB file peaks at 32 MiB of encoded bytes plus the pixel buffer
  (`RichImage(width, height)` value-initialises up to 16 MiB that `CopyPixels` overwrites), and a rotated image or a
  sub-canvas GIF adds one more full pixel buffer.
- **Failure scenario**: a worker decoding several near-limit files concurrently holds up to 2x the encoded bytes
  transiently. Not a correctness issue.
- **Fix**: `InitializeFromMemory` directly over the caller's span (`const_cast`; decoders only read, though WIC does not
  promise it, so keep a short comment). Give `LoadFile` an internal overload that decodes from its own buffer. Avoiding
  the zero-fill needs a default-initialising allocator for `pixels_` in the public header; optional.
- **Regression test**: `Tests/Embedded/Embedded.Tests.RichTextResources.h` (identity-decode allocation test near
  `:1620-1657`): use a near-16 MiB, non-compressible PNG so the encoded size is large (the current 512x512 uniform PNG
  is a few hundred bytes and cannot expose the copy), and assert the peak counted allocation during `LoadFile` is at
  most encoded size plus decoded size plus a constant. Fails at `6df17007`.
- **Verifier notes**: the existing test `identity PNG decoding allocates one full owned C++ pixel buffer, not a second
  copy` cannot see the encoded copy.

### RENDER-10 — Script rendering binary-searches `projection.runs` but public layout admission never checks order

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (public API robustness) · **Verdict** Plausible ·
  **Confidence** medium · **Effort** S
- **Location**: `src/Controls/DxUi.RichTextLayout.cpp:247-253`, `src/Controls/DxUi.RichTextScriptRenderer.h:141-149`,
  `:260-271`
- **What**: `PreparedLayout::Prepare(factory, Projection, options)` and `Projection`/`StyleRun` are public
  (`include/DxUi/RichTextLayout.h:108-109`, `include/DxUi/RichMarkdown.h:23-45`). Admission validates each run's bounds
  and flags but not ordering or overlap, while the script renderer uses `lower_bound` and `upper_bound` over `runs`.
  Every in-tree producer emits sorted runs.
- **Failure scenario**: a hand-built projection with `{Bold 4..6}, {Superscript 0..3}` makes `Flags()` return 0 for
  the script glyphs, so no offset is applied; an overlap makes the collector report `DWRITE_E_UNSUPPORTEDOPERATION`,
  `Prepare` marks the scripts `unsupported` (`:428-433`), and `Draw` throws a `wil` HRESULT exception. There is no
  out-of-bounds access: the run loop is bounded by `end()` (`DxUi.RichTextScriptRenderer.h:144`).
- **Fix**: in `PreparedLayoutAccess::Prepare`, when any run carries `Superscript | Subscript`, require
  `run[i].start + run[i].length <= run[i+1].start` and throw `std::invalid_argument("Invalid rich-text style range")`
  otherwise; or sort a private copy.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextLayout.cpp`: a `Reject` case with an unsorted script run
  set and one with overlapping script runs; both are accepted at `6df17007`.
- **Verifier notes**: consequences are wrong flags or an HRESULT exception, not memory unsafety.

### RENDER-11 — UIA annotation attribute writes into 256-slot stack arrays guarded only by a limit in another file

- **Severity** P3 (reviewer P3, verifier P3) · **Category** architecture (latent memory safety) · **Verdict**
  Confirmed · **Confidence** high that the dependency is implicit; not reachable today · **Effort** S ·
  **Introduced** `e0deab0e`
- **Location**: `src/Controls/DxUi.RichAnnotations.h:417-428`, `src/Controls/DxUi.RichTextEditor.cpp:1597-1598`,
  `src/Controls/DxUi.Accessibility.cpp:2381-2400`
- **What**: `std::array<..., 256u> owners{}, raw{}` is filled once per annotation covering `range.start` with no
  `count < 256` check. The only source of annotations is the decoration loop (`DxUi.Accessibility.cpp:2399`), and
  `SetDecorations` rejects more than 256 decorations (`DxUi.RichTextEditor.cpp:1597-1598`), so the index cannot
  overflow today.
- **Failure scenario**: a later change that raises the decoration cap or adds another annotation source lets more than
  256 overlapping annotations write past the stack arrays on a UIA client query.
- **Fix**: add a shared `constexpr std::size_t MaximumDecorations = 256` used by `SetDecorations` and
  `static_assert(owners.size() >= MaximumDecorations)`, and a runtime `if (count == owners.size()) return
  E_OUTOFMEMORY;` before the write.
- **Regression test**: none possible today; the `static_assert` is the check.
- **Verifier notes**: latent only.

### RENDER-12 — Data and SVG policy toggles reset `failed` on every source kind

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (contract wording) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.RichImageResources.h:61-62`, `:76-78`, `:92-94`, `:243-249`;
  `Specs/UI/UI_ControlsAndLayout.md:1189-1190`
- **What**: `SetDataImagesEnabled` and `SetSvgImagesEnabled` run `for (auto& entry : entries_) entry.failed = false;`.
  The contract says failed sources are retried on "explicit reload, document location or remote-image policy changes".
  After the reset `Advance` no longer finds the entry as `held` (`:243-249`) and issues a new request.
- **Failure scenario**: toggling data images re-requests every failed local file, repeating filesystem work the
  application already saw fail (possibly on a slow UNC path). For data images the reset is useless: a data source
  entry is never created while the policy is off (`:215-220`), so there is no failed data entry to retry. For SVG the
  reset is plausibly intentional (a worker that completed null because SVG was off should retry).
- **Fix**: either narrow the data-toggle reset (drop it), keeping it for SVG, or, cheaper and safer, extend the
  contract text and `docs/rich-text.md` (the sentence "failed completions stay cached ... until reload, document
  location or remote-image policy changes") to name the data and SVG policy toggles.
- **Regression test**: `Tests/Support/Support.Tests.RichImageResources.h`: fail a local file, toggle data images, call
  `TakePendingImageRequests`, assert no new request. Fails at `6df17007`; applies only if the reset is narrowed. For
  SVG, assert the intended retry instead.
- **Verifier notes**: the data toggle is the clear case; the SVG case needs a decision.

### RENDER-13 — No test covers header-declared oversize dimensions or a GIF logical-screen bomb

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test-gap · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `Tests/Support/Support.Tests.RichImage.h:383-394`; guarded code `src/Controls/DxUi.RichImage.cpp:263-264`
  and `:127-128`
- **What**: `Decode` checks `DimensionsAllowed(width, height)` before `CopyPixels`, and `ComposeGifCanvas` checks the
  canvas. The only oversize tests are `CreatePbgra(4096, 1025)` and encoded-size limits. A search of `Tests` for
  4097-wide fixtures, IHDR patches, GIF headers and logical-screen fixtures finds nothing. A regression that moves the
  check after the allocation or drops it on the identity path would go unnoticed.
- **Failure scenario**: none today; this is a missing guard-rail on a resource-limit contract.
- **Fix**: n/a (test only).
- **Regression test**: `Tests/Support/Support.Tests.RichImage.h`: (1) a 4097x1 PNG and a 2049x2048 PNG; produce them
  with the WIC encoder (as `RichImageTests::Encode` does) or patch IHDR and recompute its CRC, since a bare patch makes
  the decoder fail on the CRC before the size check; (2) a GIF whose logical screen is 65535x65535 with a 1x1 frame
  (WIC may itself reject it, so assert the actual HRESULT and that no large allocation happens). Assert
  `ERROR_BUFFER_OVERFLOW`, `output == held`, and an allocation count below the pixel-buffer size, using the allocation
  counter from `Tests/Embedded/Embedded.Tests.RichTextResources.h`.
- **Verifier notes**: confirmed gap.

## Refuted

None. RENDER-14 was merged into RENDER-01 (same root cause) rather than refuted.

## Area architecture and simplification proposals

1. **Source guides as viewport-scoped glyph composition** (RENDER-01).
   - Proposal: prepare one digit set per font, DPI and resource generation, stage only y values for a viewport window,
     compose `DrawGlyphRun` from a stack buffer at paint, and stage in visual preparation so guides never gate the
     frame.
   - Rationale: memory goes from O(lines x DirectWrite layout) to O(window), and the frame is no longer withheld.
   - Files: `src/Controls/DxUi.RichSourceGuides.h`, `src/Controls/DxUi.RichTextEditor.cpp` (paint 4877-4893,
     `PrepareSourceGuides` 6897-6915), `src/Controls/DxUi.Controls.cpp` (no change needed).
   - Risk: RTL and trailing alignment of digit strings must match the current trailing layout; ASCII digits skip
     shaping, which is acceptable.
   - Effort: M.
   - Before/after: before, a 2,000-line Source document needs ~500 owner turns and withholds the frame per edit; after,
     a bounded window stages within one turn and the frame is never withheld.
2. **Restore chunked v0 geometry under a uniform line height** (RENDER-02).
   - Proposal: replace the private origin line (an inline object plus a 0.01-size LF) with a per-chunk draw and
     hit-test translation by `chunk.y`.
   - Rationale: uniform spacing can then apply to every chunk and `Phase::Reuse` keeps working; it also removes the
     `positionedExactly` heuristic and two hidden text units.
   - Files: `src/Controls/DxUi.RichTextLayout.cpp` (Prepare, HitTest, Caret, Range, Draw),
     `src/Controls/DxUi.RichDocumentLayout.cpp`.
   - Risk: float accumulation parity (`AddCoordinate`); the raw-line-origin oracle tests cover it.
   - Effort: M-L; do it only if the RENDER-02 measurement shows a regression.
   - Before/after: before, each edit with a multiplier re-lays out up to 64 Ki units; after, only the changed chunk.
3. **Single color-glyph drawing helper** (RENDER-05).
   - Proposal: factor one `DrawColorAwareGlyphRun(target, run, brush)` out of `RichTextSourceInkRenderer.h` and
     `RichTextScriptRenderer.h` with a DC4 `TranslateColorGlyphRun` path.
   - Rationale: the two renderers duplicate DC7-only drawing and the pixel-snapping code. The source-ink renderer
     already degrades to the canonical renderer on refusal while the script renderer cannot, so share the draw helper,
     not the refusal policy.
   - Risk: low. Effort: M.
   - Before/after: before, Windows 10 and Windows 11 21H2 lose native script rendering next to emoji; after, they render
     natively.
4. **Image source classification at admission** (RENDER-03).
   - Proposal: `ResolveLocalImagePath` already parses server and share; return `{path, network, server}` and carry it on
     `RichImageRequestState`.
   - Rationale: a typed policy hook replaces consumers' string-prefix checks.
   - Effort: S-M. Before/after: before, each consumer re-parses `FilePath()`; after, one accessor.
5. **`RichImage::Decode` without copies** (RENDER-09).
   - Proposal: delete the `owned` vector and let `LoadFile` pass its buffer to an internal overload.
   - Risk: low (`const_cast` on `InitializeFromMemory`). Effort: S.
   - Before/after: before, peak 2x encoded bytes per `LoadFile`; after, 1x.

## Test-quality observations

- The Source-guides tests (`Tests/Embedded/Embedded.Tests.RichGeometryResources.h:539-650`) mostly drain preparation
  (`finish`, `PrepareRichEmbeddedContent`). The one yield check (lines 645-646) pins today's frame-withholding
  behaviour and never samples frame content or the retained number-layout count, so neither the stall nor the O(lines)
  memory can fail a test.
- `TestReverseCjkTraversalAndContext` builds its expected boundaries with `FindNextTextElementBoundary`, which
  `FindPreviousTextElementBoundary` also uses for its forward rescan. It is independent only because
  `TestUnicode17DefaultGraphemeCorpus` validates `FindNext` separately. Add corpus-derived reverse expectations,
  including regional-indicator runs of odd and even length and GB9c conjunct sequences, from the UCD test file.
- `Tests/Support/Support.Tests.RichImagePaths.h:68-80` has no negative or classification cases for `//server/share/a.png`,
  `\\h@SSL\s\a.png` or `//localhost/pipe/x`.
- No hostile decode dimensions (RENDER-13); only `CreatePbgra` dimension rejection and encoded-size rejection are
  covered.
- No RTL coverage of script decorations: `ScriptTextRenderer::StageDecoration` hit-tests `x + .01` at the underline
  start, and no test checks underline or strikethrough offset on a right-to-left superscript run (for example
  `<p dir=rtl>...<sup><u>...</u></sup></p>`).
- `Tests/Embedded/Embedded.Tests.RichPreparationBenchmark.h` measures only Rich mode with default typography, so
  RENDER-01 and RENDER-02 cannot appear in paired results.
- The identity-decode allocation test (`Tests/Embedded/Embedded.Tests.RichTextResources.h:1632-1653`) uses a PNG of one
  colour, whose encoded size is tiny, so it cannot detect the encoded-byte copy in RENDER-09.
- Good: `TestRichImageRasterFailureAndBudgets` uses independent pixel-parity oracles across tile budgets, epochs, targets
  and failure injection; the allocation-failure sweeps for history rings and image publication are thorough.

## Questions for the developer

1. Is calling `IDWriteTextLayout::HitTestPoint` and `GetFontSize` from inside that same layout's `Draw` callback
   (`ScriptTextRenderer::StageDecoration`) known to be safe? DirectWrite does not document re-entrancy. Recommended
   answer: stage `(x, y, width)` during `Draw` and resolve sizes after `Draw` returns.
2. For RTL glyph runs, does DirectWrite pass the left edge or the run origin as `baselineOriginX` to `DrawUnderline` and
   `DrawStrikethrough`? `StageDecoration` and `DecorationOffset` assume the left edge. Recommended answer: verify with
   an RTL superscript underline test (see Test-quality observations) before relying on it.
3. Is Windows 10 (without `ID2D1DeviceContext7`) expected to render scripts natively next to emoji (RENDER-05), or is
   source fallback the accepted product behaviour there? Recommended answer: the contract already documents the source
   fallback (`docs/rich-text.md:1756-1759`); accept it, record the affected OS builds in the docs, and fix SEM-01
   first.
4. Is the 1.45x uniform line height for script paragraphs intentional when the multiplier is 0 (RENDER-06)? (false
   premise — see verifier: it is intentional and pinned at `Tests/Support/Support.Tests.RichSemanticGeometry.h:349`.)
   Recommended answer: keep it as a floor and raise it where the font's default is taller.
5. Should scheme-relative `//host/...` destinations be treated as remote URLs (gated by `SetRemoteImagesEnabled`) or as
   UNC paths (RENDER-03)? (partly false premise — see verifier: the "MUST NOT become file requests" sentence concerns
   URI schemes, and UNC servers are explicitly in the local-path contract.) Recommended answer: UNC paths, with typed
   classification so the consumer's policy can distinguish them.
6. Was the R28-03 fix (RENDER-02) measured against the prior chunked baseline? Recommended answer: no receipt exists
   (verified); measure it with the Source-mode benchmark scenario before deciding on the chunk-translation redesign.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.RichImage.cpp` | full |
| `src/Controls/DxUi.RichImage.h` | full |
| `include/DxUi/RichImage.h` | full |
| `src/Controls/DxUi.RichImageResources.h` | full, including the editor callers `CompleteImageRequest`, `ReplaceImageFrame`, `TakePendingImageRequests` and preparation `Begin`/`Advance`/`Publish` in `DxUi.RichTextEditor.cpp` |
| `src/Controls/DxUi.RichImagePaths.h` | full |
| `src/Controls/DxUi.RichTextScriptRenderer.h` | full |
| `src/Controls/DxUi.RichTextLayout.cpp` | full (Prepare, Draw, ScriptTargetSupported, MinimumTextWidth, queries) |
| `include/DxUi/RichTextLayout.h` | full |
| `src/Controls/DxUi.RichDocumentLayout.cpp` | full |
| `src/Controls/DxUi.RichSourceGuides.h` | full, plus the editor paint and preparation callers |
| `src/Controls/DxUi.RichSourceWindow.h` | full (termination and bounds of every window loop) |
| `src/Controls/DxUi.RichBidi.h` | full (AL is included in `kBidiR`) |
| `src/Controls/DxUi.RichCodeInk.h` | full (token disjointness, surrogates, unterminated constructs) |
| `src/Controls/DxUi.RichAnnotations.h` | full, plus snapshot construction in `DxUi.Accessibility.cpp:2381-2427` and the decoration cap |
| `src/Controls/DxUi.TextElements.cpp` / `.h` | full (reverse boundary and ASCII fast path checked against GB rules) |
| `src/Controls/DxUi.RichDocument.cpp` | diff + touched functions (HistoryRing, ReplaceTransient, ReplaceImpl history, Undo/Redo, LocationAt, codecs); piece-tree internals unchanged and skipped |
| `include/DxUi/RichDocument.h` | full |
| `src/Controls/DxUi.RichSourceInk.h` | diff + touched functions (StartInline math delimiters, FinishLinkLabel bare autolinks) |
| `src/Controls/DxUi.RichTextSourceInkRenderer.h` | diff only (diagnostic counter); the preview gate was checked at its call site in `DxUi.RichTextLayout.cpp` |
| `Tests/Support/Support.Tests.RichImage.h` | diff, plus the limit and decode sections |
| `Tests/Support/Support.Tests.RichImageResources.h` | diff |
| `Tests/Support/Support.Tests.RichImagePaths.h` | full (path tables) and diff |
| `Tests/Embedded/Embedded.Tests.RichImageFailures.h` | diff |
| `Tests/Embedded/Embedded.Tests.RichDocumentStorageResources.h` | diff |
| `Tests/Embedded/Embedded.Tests.RichPreparationBenchmark.h` | full (new file) |
| `Tests/Controls/Controls.Tests.RichTextLayout.cpp` | diff |
| `Tests/Controls/Controls.Tests.RichDocumentLayout.cpp` | diff |
| `Tests/Controls/Controls.Tests.RichDocument.cpp` | diff |
| `Tests/Controls/Controls.Tests.TextElements.cpp` | diff |
| Label mnemonic and hyperlink underline preparation | partial: link underline staging in `PreparedLayout::Draw` checked; the label control itself is outside the listed files and was skipped |
| Semantic SVG/raster upload in `DxUi.RichSemanticGeometry.cpp` | outside the listed files; only the script-gating lines (1994-2001) and the SVG branch (410-470) were read |
