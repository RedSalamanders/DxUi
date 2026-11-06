# Architecture, simplification and error policy: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: whole-tree structure, duplication, dead code,
build shape and error-handling policy of `src/` and `include/`, with emphasis on the window `a0f67fd3..6df17007`.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

Most entries are proposals, not defects. Only the three real defects carry a severity (all P3); every proposal shows
"-" and a "When" recommendation instead. Nothing in this area blocks production. The behavioural defects the reviewer
traced through the error paths are tracked in other appendices (FLOW-01, P1, and MATH-02, P2, are concrete instances of
ARCH-03).

| ID | Sev | When | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- | --- |
| ARCH-02 | P3 | before production | Confirmed (consequence narrowed) | SVG preparation turns device-loss HRESULTs into "unsupported image" | `src/Controls/DxUi.RichSemanticGeometry.cpp:472` | S |
| ARCH-03 | P3 | before production (logging, docs); after production (typed family) | Confirmed (fix corrected) | Rich preparation swallows invariant failures as a Warning and never logs the cause | `src/Controls/DxUi.RichTextEditor.cpp:4102` | S now, M later |
| ARCH-18 | P3 | before production (WIC factory); after production (idioms) | Confirmed | Unsynchronised static WIC factory for menu icons; COM implementation idioms diverge | `src/Controls/DxUi.Menu.cpp:805` | S (WIC), M (idioms) |
| ARCH-01 | - | before production (policy table); after production (helper) | Confirmed as facts, reframed | Consumer-callback failure policy is documented but split by control | `src/Controls/DxUi.Internal.h:208` | S |
| ARCH-04 | - | before production | Confirmed | Delete never-compiled `#else` arms of the fixed diagnostics switch | `src/Controls/DxUi.WindowHost.cpp:4601` | S |
| ARCH-05 | - | before production | Confirmed | Remove dead code and dead includes | `src/Controls/DxUi.MarkdownBlocks.h:1037` | S |
| ARCH-06 | - | before production | Confirmed (fix corrected) | Restore the CPU-only layering of the semantic builder | `src/Controls/DxUi.RichSourceWindow.h:2` | S |
| ARCH-07 | - | before production (doc note); after production (consolidation) | Confirmed (partly overstated) | Contain test seams compiled into the shipping library | `include/DxUi/DxUi.h` | L |
| ARCH-08 | - | opportunistic (step 1); after production (step 2) | Confirmed (spot-checked) | One Unicode classification service | `src/Controls/DxUi.TextElements.h` | S then M |
| ARCH-09 | - | opportunistic | Confirmed (spot-checked) | Small duplicated helpers (color, geometry, hashing, line breaks) | `src/Support/Utilities.h:19` | S-M |
| ARCH-10 | - | after production | Confirmed | Split `DxUi.Accessibility.cpp` along its existing seams | `src/Controls/DxUi.Accessibility.cpp` | L |
| ARCH-11 | - | after production | Confirmed | Split `DxUi.RichTextEditor.cpp` behind its pimpl | `src/Controls/DxUi.RichTextEditor.cpp:296` | M |
| ARCH-12 | - | after production | Confirmed | Split `DxUi.Controls.cpp`, `DxUi.Menu.cpp`, `DxUi.Grid.cpp` | `src/Controls/DxUi.Controls.cpp` | M each |
| ARCH-13 | - | after production | Confirmed | Decompose the public `DxUi.h` and the `ControlHost` god object | `include/DxUi/DxUi.h:6119` | M to L |
| ARCH-14 | - | after production | Confirmed | Decompose the complexity hotspots | `src/Controls/DxUi.NativeFlowchart.cpp:877` | M each |
| ARCH-15 | - | opportunistic (2, 4); after production (1) | Confirmed | Header-only implementations: keep, move, or rename by evidence | `src/Controls/DxUi.PreparedMath.h` | S-M |
| ARCH-16 | - | after production | Confirmed | Measure and fix library build throughput | `build.ps1:186` | M |
| ARCH-17 | - | opportunistic | Confirmed | Warning-suppression boilerplate (none hides actionable diagnostics) | `include/DxUi/DxUi.h` | S-M |
| ARCH-19 | - | opportunistic | Confirmed | Name the fallible zero-size construction idiom | `Specs/Core/Core_PerformanceAndResources.md:78` | S-M |
| ARCH-20 | - | opportunistic | Confirmed | Naming and dialect drift | `src/Controls/DxUi.cpp` | S |

Window at a glance (reviewer counts, `git diff --stat a0f67fd3 6df17007 -- src include`): 99 files, +30,508 / -3,150; 5 new
TUs (`DxUi.FileDrop.cpp`, `DxUi.MarkdownMetadata.cpp`, `DxUi.RichAccessibilityText.cpp`, `DxUi.RichTextExport.cpp`,
`DxUi.YamlMetadata.cpp`); 57% of the additions landed in seven files (`RichTextEditor.cpp` +5,437, `PreparedMath.h` +2,540,
`RichTextCommands.cpp` +2,366, `NativeMath.cpp` +2,018, `RichSemanticGeometry.cpp` +1,788, `NativeFlowchart.cpp` +1,715,
`RichSemanticDocument.cpp` +1,672). Library: 46 TUs, about 140,800 lines in `src` and `include`; a DFS over first-party
includes found no cycles, no public header includes a `src/` header, and `src/Support` never includes `src/Controls`.

## P3 findings

### ARCH-02 — SVG preparation turns device-loss HRESULTs into "unsupported image"

- **Severity** P3 (reviewer none given, verifier P3) · **Category** bug (error policy) · **Verdict** Confirmed,
  consequence narrowed · **Confidence** high for the misclassification, low for real-device reachability ·
  **Effort** S · **When** before production · **Introduced** `a1c25b00` (2026-11-01, opt-in SVG images; moved intact by
  `121f830a`)
- **Location**: `src/Controls/DxUi.RichSemanticGeometry.cpp:472` (the faulty test), `:414-419` (`reject`),
  `:2493-2494` (placement erased on `unsupported`), `include/DxUi/DxUi.h:104` and `src/Controls/DxUi.WindowHost.cpp:47-50`
  (`IsDeviceLossHResult`), `src/Controls/DxUi.RichTextEditor.cpp:4089-4100` (stated intent),
  `Specs/UI/UI_ControlsAndLayout.md:1177-1178` (normative text)
- **What**: in `NativeImageGeometry::PrepareOne` the SVG branch runs
  `if (FAILED(create) && create != E_OUTOFMEMORY && create != D2DERR_RECREATE_TARGET) return reject();` after
  `CreateSvgDocument`. It is a deny-list: every other failing HRESULT, including `DXGI_ERROR_DEVICE_REMOVED`,
  `DXGI_ERROR_DEVICE_RESET` and `DXGI_ERROR_DEVICE_HUNG`, takes `reject()`, which records the image in
  `unsupportedPixels`, logs "image upload is unsupported by this target", and makes the caller erase the placement so the
  image alternative is shown. Every neighbouring call in the function is an allow-list for the "unsupported" codes and
  throws the rest (`:449-450`, `:458-459`, `:462-463`, `:479-480`, `:546-549`, `:561-563`), and `PrepareContent` states that
  "target loss remains visible to the host" (`RichTextEditor.cpp:4089-4092`) for exactly those thrown
  `wil::ResultException` values. The spec says "device loss follows normal preparation recovery". The public classifier
  `IsDeviceLossHResult` (`D2DERR_RECREATE_TARGET`, `DXGI_ERROR_DEVICE_REMOVED`, `_RESET`, `_HUNG`) is used only in
  `WindowHost.cpp` (4302, 4471, 4803, 5126).
- **Failure scenario**: SVG images enabled (`SetSvgImagesEnabled(true)`), a document with a Markdown image whose
  destination is `shape.svg` and whose SVG was admitted; the GPU device is removed between private-context creation
  (`:449`) and `CreateSvgDocument`, which returns `DXGI_ERROR_DEVICE_REMOVED`. `PrepareOne` reports the image
  unsupported; `PrepareContent` completes without returning the loss code, and the frame is published with the image
  alternative. Not reproduced on a real device.
  Consequence bounds (verified): the loss is still detected at the next `EndDraw`, `Present` or resize
  (`WindowHost.cpp:4302`, `4471`, `4803`, `5126`), `_resourceGeneration` is bumped (`:4271`, `:4539`) and the prepared
  state is rebuilt for the new target (`RichSemanticGeometry.cpp:2162-2166`, `:2174-2181`: `targetIdentity` and epochs must
  match), so `unsupportedPixels` does not survive recovery and the image is not refused permanently. The harm is one frame
  of alternative text, a misleading warning, and a late loss report from `PrepareContent` to an embedded caller.
- **Fix**:
  1. Add a private header `src/Support/HResultClass.h` with `inline bool IsTargetLossHResult(HRESULT) noexcept`
     (`D2DERR_RECREATE_TARGET`, `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET`, `DXGI_ERROR_DEVICE_HUNG`) and make
     `DxUi::IsDeviceLossHResult` (`WindowHost.cpp:47-50`) forward to it, so the set has one definition. Do not include
     `DxUi/DxUi.h` from `RichSemanticGeometry.cpp`: that header is not among its current includes, and adding it widens the
     layering leak of ARCH-06.
  2. Change line 472 to
     `if (FAILED(create) && create != E_OUTOFMEMORY && ! IsTargetLossHResult(create)) return reject();`
     and keep the following `THROW_IF_FAILED(create)`. Malformed or unsupported SVG content still rejects only that image.
  3. Leave the other seven ad hoc HRESULT tests (`RichSemanticGeometry.cpp:449, 458, 462, 479, 546, 561`,
     `RichTextLayout.cpp:430`, `PreparedMath.h:222`) for the error-policy rollout; they already throw on loss.
- **Regression test**: `Tests/Support/Support.Tests.RichImageResources.h` next to the SVG block at `:314-326`. Add a seam,
  preferably a `createSvgDocument` entry in `RichImageRasterCalls` (`DxUi.RichSemanticGeometry.h:25-29`, the existing
  "inject driver HRESULT failures" table) or a `thread_local HRESULT testRichSvgCreateResult` under
  `DXUI_ENABLE_DIAGNOSTICS`. For each of `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_RESET`,
  `DXGI_ERROR_DEVICE_HUNG` and `D2DERR_RECREATE_TARGET`: with SVG enabled (the fixture at `:301-326`, alt text `vector`),
  arm the seam, prepare, and assert that `PrepareContent` returns that HRESULT; then clear the seam, prepare again, and
  assert
  that the SVG rasterizes (`testRichSvgRasterizations` increments) and the semantic display no longer contains
  `[image: vector]`. At `6df17007` the first assertion fails (preparation succeeds with the alternative) and, because the
  rejected key is already published, the second preparation does nothing. For `E_INVALIDARG` assert the old behaviour
  (placement erased, alternative `[image: vector]` kept).
- **Verifier notes**: re-read `PrepareOne` end to end and the caller. SVG is opt-in and new in the window.
  `CreateSvgDocument`
  is an XML parse that normally does not touch the GPU, and D2D usually reports loss as `D2DERR_RECREATE_TARGET` (already
  handled), so the DXGI codes may be unreachable in practice; the contradiction with the spec text is real either way.
  Would be P2 if a DXGI loss code from this call were demonstrated.
- **Lead note**: the brief asked for P1/P2 classification. After tracing the recovery path the verified severity is P3:
  the claimed consequences (loss unnoticed, image permanently refused) do not hold.

### ARCH-03 — Rich preparation swallows invariant failures as a Warning and never logs the cause

- **Severity** P3 (reviewer none given, verifier P3) · **Category** error policy, observability · **Verdict** Confirmed,
  fix corrected · **Confidence** high · **Effort** S now, M later · **When** before production (logging, `Throws:` docs),
  after production (typed family) · **Introduced** `53b0f5bbb` (2026-11-01)
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp:4045-4141` (the 8 handlers), `:4102` and `:4125`,
  `src/Controls/DxUi.RichSemanticGeometry.cpp:1379, 1648, 2269`, `src/Controls/DxUi.RichSemanticDocument.cpp:2469`,
  `src/Controls/DxUi.RichTextLayout.cpp:476-494`, `src/Controls/DxUi.RichSemanticGeometry.h:41-74` (typed rejections),
  `Tests/Support/Support.Tests.RichSemanticEditor.h:3105-3150`
- **What**: `RichTextEditor::PrepareContent` (noexcept, 3466-4141) ends in eight handlers: `NativeObjectsRejected`,
  `NativeObjectUnsupported`, `NativeObjectCapacity`, `std::length_error`, `std::bad_alloc`, `wil::ResultException`,
  `std::logic_error`, `std::exception`. The `logic_error` handler (`:4102`) catches every `invalid_argument` (175 throws),
  `out_of_range` (39) and plain `logic_error` (44, 35 at base) thrown during rich preparation. In rich mode it stores
  `preparationRejected` for the whole view key, clears the semantic document and shows exact source, logging only
  `Debug::Warning(L"DxUi::RichTextEditor: deterministic rich preparation failed; exact source is shown for this view key.")`
  (`:4125`). The handler has no exception variable, so `what()` is dropped; the non-rich branch (`:4128-4132`) returns
  `E_FAIL` with no log. `RichTextEditor.cpp` has 0 `Debug::Error` calls. Several of those throws are internal invariants,
  not content limits: "Invalid block ancestor order", "Missing intrinsic semantic table widths", "Missing subtle table
  staging", "Ambiguous source format ownership", "Unprepared rich-text ...". Typed rejections already exist
  (`NativeObjectsRejected : std::exception`, `NativeObjectCapacity : std::length_error`,
  `NativeObjectUnsupported : std::runtime_error`). Only two public comments document throwing (`MarkdownSyntax.h:284`,
  `RichSemanticDocument.h:225`) although the public rich API throws `invalid_argument`, `length_error` and `out_of_range`.
- **Failure scenario**: any invariant break, or any per-object condition thrown as `invalid_argument`, turns the whole
  document into source with one untitled Warning. FLOW-01 (a parser-published flowchart label span past the trimmed label)
  and MATH-02 (effective math size over 4096 DIP) are live instances; nothing in the log names them.
- **Fix** (corrected; the whole-view fallback is designed and tested, so do not fail fast on `logic_error` in general):
  1. Before production, in the `logic_error` handler: bind the exception (`const std::logic_error& failure`), log with
     `Debug::Error` including `failure.what()`, and increment a diagnostics counter such as
     `state.preparationInvariantFailures` exposed through `RichTextEditorDebugState`. Log the non-rich branch too.
  2. Add `// Throws: std::invalid_argument ... std::length_error ... std::out_of_range ...` lines to the public rich headers
     (`RichDocument.h`, `RichSemantic.h`, `RichTextCommands.h`, `RichTextExport.h`, `RichTextLayout.h`, `MarkdownSyntax.h`).
  3. After production: introduce `RichText::Detail::PreparationRejected` (a `std::exception` subclass) and re-base the three
     `NativeObject*` types on it; convert capacity/unsupported `logic_error` throws to it; add
     `RichText::Detail::InvariantViolation : std::logic_error` thrown by a `DXUI_INVARIANT(cond)` macro at the sites listed
     above. Handle `InvariantViolation` before the generic `logic_error` branch: Debug::Error, counter, and `__fastfail`
     (or `WI_ASSERT`) only under `_DEBUG`/ASan; Release keeps the source fallback. Plain `logic_error` stays a deterministic
     rejection, so the test injection at `RichTextEditor.cpp:3868` and `:4005` and option-admission `invalid_argument`
     (`PreparedFlowchart::Begin`) keep working. Narrow the handler at `:4102` once the conversions are complete.
  4. Do not map `invalid_argument` from `PreparedFlowchart::Begin` to a per-object reject blindly: it also reports per-view
     option admission (`FLOW.verify.md`).
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h` (stage block at `:3105-3150`): after the
  existing assertions add `editor->DebugGetState().preparationInvariantFailures == 1u` for stage 1 and 2, and a captured
  diagnostics-sink record at level `error` whose text contains the injected message (today: level `warning`, no message).
  After the typed family exists, add a case that throws `InvariantViolation` and asserts the fallback still happens in a
  build mode with fail-fast disabled (a seam that turns `__fastfail` into a counter). The `Throws:` documentation is
  review-only unless a validator is added.
- **Verifier notes**: confirmed the 8-handler chain, the counts (44/175/96/39/13/6 throws), the 0 `Debug::Error`, the 2
  documented-throw comments, and the cited invariant sites. Corrected the reviewer's fail-fast proposal because
  `Support.Tests.RichSemanticEditor.h:3105-3150` injects plain `std::logic_error` and asserts the fallback, retry keys and
  recovery. Cross-checked with FLOW-01 (confirmed P1) and MATH-02 (P2). The catch order matters: `NativeObjectCapacity`
  derives from `length_error`, which derives from `logic_error`.

### ARCH-18 — Unsynchronised static WIC factory for menu icons; COM implementation idioms diverge

- **Severity** P3 (reviewer none given, verifier P3) · **Category** bug (data race), simplification · **Verdict**
  Confirmed · **Confidence** high for the race, medium for field reachability · **Effort** S (WIC factory), M (idioms) ·
  **When** before production (WIC factory), after production (idioms) · **Introduced** `f28f55d0b` (2026-09-05, pre-window)
- **Location**: `src/Controls/DxUi.Menu.cpp:805-826` (`GetMenuIconWicFactory`), `:853`, `:883-885`, `:3430`;
  `src/Controls/DxUi.WindowHost.cpp:496-525` (per-thread graphics pool and its mutex); hand-rolled COM classes at
  `src/Controls/DxUi.Accessibility.cpp:1056, 6399, 8484`, `DxUi.RichAccessibility.h:177`, `DxUi.RichAnnotations.h:71`,
  `DxUi.RichTextScriptRenderer.h:77`, `DxUi.RichTextSourceInkRenderer.h:71`, `DxUi.TextStoreACP.cpp:1080`; WRL
  `RuntimeClass` users at `DxUi.FileDrop.cpp:23, 97, 159`, `DxUi.PreparedMath.h:167`, `DxUi.RichSemanticGeometry.cpp:82`,
  `DxUi.RichTextLayout.cpp:35`
- **What**: `GetMenuIconWicFactory` keeps `static wil::com_ptr<IWICImagingFactory> factory;` and does
  `if (factory) return factory.get();` followed by `CoCreateInstance(..., factory.put())` (twice, with a
  `CLSID_WICImagingFactory` fallback) and `factory.reset()` on failure. Construction of the static is thread-safe; the
  check, `put()` (which releases the held pointer) and use are not. Two threads that both observe null can interleave so
  that one returns a pointer that the other has just released through `put()`. It is the only function-local static COM
  pointer in `src`. The factory is also released during static destruction and is inconsistent with the `RichImage` API,
  which
  takes a caller-supplied WIC factory (`RichImage.cpp:451, 463`).
- **Failure scenario**: two hosts on different UI threads (a supported configuration: the shared graphics resources are
  keyed by thread id and guarded by `GetSharedWindowHostGraphicsResourcesMutex`) each open a menu containing a
  `MenuFlyoutItem::BitmapIcon` for the first time within the duration of the first `CoCreateInstance`. One of them can
  use a released or half-assigned factory (use after free) or leak one. The window is narrow, hence P3.
- **Fix**: WIC factory only, before production: replace the body with a mutex-protected or `std::call_once` creation of a
  process-wide `wil::com_ptr_nothrow<IWICImagingFactory>`, creating it on first use and returning a raw pointer to a
  factory that is never reset (the WIC factory class is free-threaded). Release it explicitly in the existing process-exit
  shutdown path (`ShutdownAllWindowHostsForProcessExit`) rather than in static destruction, or accept leaving it for process
  exit with a comment. Alternative: pass a per-host factory from the graphics resources. After production: heap COM objects
  use WRL `RuntimeClass` with `Make<>`; stack DirectWrite renderers share one `StackComObject<T>` base with documented
  non-owning reference counting (`RichTextScriptRenderer.h:77` keeps a non-atomic count that saturates at 0, which is safe
  only because it is used synchronously inside one `Draw`).
- **Regression test**: a Menu suite test that exposes the accessor through a `DebugGetMenuIconWicFactoryForTest` seam
  (the function is in an anonymous namespace today), resets the cached factory, initialises COM on N threads (for
  example 8),
  releases them together from a start barrier, and asserts that every returned pointer is non-null and identical. Repeat the
  reset-and-race cycle several hundred times under x64 Debug and ASan Debug. At `6df17007` the assertion can fail or crash
  intermittently (MSVC has no ThreadSanitizer, so the equality check and ASan are the detectors); after the fix it is
  deterministic.
- **Verifier notes**: reachability confirmed by the per-thread pool design (`WindowHost.cpp:496-525`,
  `docs/hosting.md:3-6`, `WindowHostAttachment.ownerThreadId`). Not reproduced. The remaining items of ARCH-18 are style
  divergence, not defects.

## Proposals (no severity)

### ARCH-01 — Consumer-callback failure policy is documented but split by control

- **Kind** error policy, duplication · **Verdict** Confirmed as facts; the claim that RichTextEditor "contradicts" the
  helper is refuted (see below) · **Effort** S · **When** before production (one documented table); after production
  (helper) · **Risk** low
- **Location**: `src/Controls/DxUi.Internal.h:208-219` (`TryControlCallback`), `src/Controls/DxUi.RichTextEditor.cpp`
  (callback sites below), `src/Controls/DxUi.Controls.cpp:172-190`, `src/Controls/DxUi.WindowHost.cpp:445-460`,
  `src/Controls/DxUi.Menu.cpp:225-240`, `src/Support/Diagnostics.h:24-35`
- **What**: `TryControlCallback` turns any `std::exception`, `bad_alloc` included, into `false`; it has 128 call sites (Grid
  60, Tree 37, WindowHost 22, Controls 8, Accessibility 1), mostly model getters but also observer delegates
  (`Grid.cpp:3166` `OnGridFocusedRowChanged`, `WindowHost.cpp:2121` `onFocusChanged`). `RichTextEditor.cpp` uses it 0
  times and
  has 11 `std::terminate()` calls (1179, 2092, 2159, 2940, 3007, 3090, 3178, 5812, 5836, 6361, 6997; 6 existed at base).
  This is the documented fatal observer policy, not drift: `Specs/UI/UI_InputAndAccessibility.md:76`, `:1327`, `:1361`,
  `:1541`, `:1554` and `docs/rich-text.md:499` ("`std::bad_alloc` is fatal"), implementing decision Q2 of
  `Specs/Plans/Done/CodeReview_2026-10-31.md:167` ("Decline `bad_alloc` atomically at input. Terminate only in observer
  callbacks."). The general host rule is different (`UI_InputAndAccessibility.md:815-822`, `docs/hosting.md:162-166`:
  callbacks are contained, allocation failure maps to `E_OUTOFMEMORY`), so an "observer" has two documented fates
  depending on the control.
- **Precise inventory** of the 13 `(*callback)(...)` sites: 9 terminate on `bad_alloc` (2088 link activation, 2155 image
  activation, 2935 scroll, 3003 horizontal scroll, 3085 selection, 3173 text, 5807 link-edit request, 5831 find request,
  6992 source-drag move); 2886 (UIA code copy) returns `E_OUTOFMEMORY`; 5865 (Ctrl+Shift+C code copy) has no local `try` and
  is declined by `OnKeyDown`'s outer handler (`:6312`); 6843 (consumer code tokenizer) returns `E_OUTOFMEMORY`; 7128 (mouse
  code copy) swallows `bad_alloc`. Two more terminates sit outside the 13: the bitmap-paste handler (1179) and
  `Control::OnContextMenu` (6361). The HTML paste converter (963) rethrows `bad_alloc` (declines the input).
  Diagnostics wrappers: `TraceButtonDiagnostics` and `TraceWindowHostDiagnostics` have identical bodies and terminate on
  `bad_alloc`; `WriteMenuDiagnosticsTrace` and `Debug::Write` swallow it. All pre-window and reached only when a diagnostics
  sink is installed.
- **Proposal**:
  1. Before production: add one table to `docs/hosting.md` and the input spec listing, per callback, whether allocation
     failure terminates, is contained or maps to `E_OUTOFMEMORY`, including the three code-copy outcomes. No code change.
  2. The developer decides whether the observer rule stays editor-specific (see Questions).
  3. After production: promote `TryControlCallback` to
     `InvokeConsumerCallback(Callback&&, CallbackFailure policy)` with `FatalOnAllocationFailure` or `Recoverable`,
     replace the
     12 own-`try` sites mechanically (each keeps its warning text), and collapse the four trace wrappers into one
     `Debug::Trace(...)` that never terminates.
- **Validation**: EditorControls, Embedded, Rendering, WindowHost and Accessibility suites
  (`Tests/test-scopes.json:233-283`)
  in x64 Debug and ASan Debug; existing allocation-failure fixtures.
- **Verifier notes**: counts re-run and corrected as above. No terminate-handler fixture for the editor observers was
  located by this review (`Embedded.Tests.Embedded.cpp:1360` installs a handler for another purpose).

### ARCH-04 — Delete never-compiled `#else` arms of the fixed diagnostics switch

- **Kind** dead code · **Verdict** Confirmed · **Effort** S · **When** before production · **Risk** none at runtime
- **Location**: `include/DxUi/Configuration.h:4`; `src/Controls/DxUi.WindowHost.cpp:4601-4839` (237 lines), `:3064`,
  `:4318`;
  `src/Controls/DxUi.Accessibility.cpp:440-475` (34), `:5632`; `src/Support/LiveResourceCount.h:70-83` (12);
  `src/Controls/DxUi.Menu.cpp:33, 1609`; `src/Foundation/FrameRuntime.cpp:180`
- **What**: `Configuration.h:4` hard-codes `#define DXUI_ENABLE_DIAGNOSTICS 1` ("not a consumer-selectable build option").
  No project, props, targets, script or CI file overrides it (there is no CMake). The `#else` arms hold about 292 lines
  (reviewer 282), the largest a hand copy of the native `Render` path that the window edited in parallel with the live path
  (`git diff -U0`: hunks at `WindowHost.cpp:4597, 4602, 4642, 4692`). Guarded blocks: 208 (125 at base). The same item
  was recorded as "A4b" in `Specs/Plans/Done/CodeReview_2026-10-28.md:663-667` and never closed.
- **Proposal**: remove the `#else` arms (keep the live `Render(dirtyRectPx, nullptr, allowHidden)` call). Optionally remove
  the `#if` guards entirely; if a no-hooks build is a goal, add a CI configuration for it (see Questions). Fixing this also
  removes a place where lockstep edits are required.
- **Validation**: build all 6 configurations; `validate.ps1`.

### ARCH-05 — Remove dead code and dead includes

- **Kind** dead code · **Verdict** Confirmed (one count corrected) · **Effort** S · **When** before production ·
  **Risk** none
- **Location**: `src/Controls/DxUi.MarkdownBlocks.h:1037-1063`; `src/Controls/DxUi.Win32Hooks.h`;
  `src/Controls/DxUi.FocusRestore.h`; `src/Controls/DxUi.RichTextEditor.cpp:2`; `src/Controls/DxUi.MathFontTable.h:277`;
  `src/Controls/DxUi.PreparedMath.h:84`; `Specs/Done/SourceImport/source-origin.json:44-47, 177-180`
- **What**: `MarkdownBlocks::ImportClosedRootParagraph` (27 lines, added by `7b5e4455`, superseded by `48ef3ee6`) has no
  caller anywhere. `DxUi.Win32Hooks.h` (38 lines) and the 3-line forwarder `DxUi.FocusRestore.h` have no includer in
  `src`, `Tests`, `Samples` or `include`. `RichTextEditor.cpp` includes `DxUi.MarkdownLabels.h` but uses none of
  `ValidFootnoteLabel`, `AppendLabelScalar`, `DecodeHeadingFragment`, `NormalizeMarkdownLabel`; the include pulls in
  `MarkdownUnicodeData.h` (89,379 bytes). `MathFontTable::VerticalVariant` has no `src` caller (5 test references);
  `PreparedMath::Locale()` has no caller at all (the reviewer's "1 test reference" was an unrelated test name).
- **Proposal**: delete the method, the two headers and the `Locale()` accessor; drop the include (compile to prove no
  transitive reliance); mark the two source-origin entries `"disposition": "retired"` with a `reason` and without
  `currentPath`, because `Tools/Validation.psm1:509-519` requires an existing file for owned entries. Move
  `VerticalVariant` behind the test-access pattern of ARCH-07 or keep it with a test-only comment.
- **Validation**: build all 6 configurations; `validate.ps1`; EditorControls suite.

### ARCH-06 — Restore the CPU-only layering of the semantic builder

- **Kind** architecture · **Verdict** Confirmed (fix corrected) · **Effort** S · **When** before production
  (compile-only) · **Risk** low
- **Location**: `src/Controls/DxUi.RichSemanticDocument.cpp:3, 180, 193, 195`;
  `src/Controls/DxUi.RichSourceWindow.h:2, 129-130,
  217-218`; `src/Controls/DxUi.Internal.h:758-765`; `Build/NativeBuildPolicy.targets:54, 56`;
  `Specs/Core/Core_Architecture.md:44-49`
- **What**: the CPU-only builder `RichSemanticDocument.cpp` includes `DxUi.RichSourceWindow.h` (added `f99d8265`), whose
  line 2
  includes `DxUi.Internal.h`, which includes `<DxUi/DxUi.h>`. The TU therefore parses all controls and `ControlHost`
  (about 800 KB of first-party source) although `Core_Architecture.md` says the builder owns no graphics or host.
  `RichSourceWindow.h` needs only `FindPreviousWordBoundary`/`FindNextWordBoundary`; `RichSemanticDocument.cpp` itself also
  calls `IsWordCharacter` (180, 193) and `StepToNextTextElement` (195), all declared only in `Internal.h`. Include-level
  leak only. Separately, the library project itself gets the include roots `Tests\Support`, `Tests\Embedded`,
  `Samples\EmbeddedControls` and `Samples\ComplexUi` (`NativeBuildPolicy.targets:54`); no `src` file uses them.
- **Proposal**: move the declarations of `StepToPreviousTextElement`, `StepToNextTextElement`, `CountTextElements`,
  `IsWordCharacter`, `FindPreviousWordBoundary`, `FindNextWordBoundary` (`Internal.h:758-765`, defined in
  `SingleLineTextEditing.cpp`) into a small `DxUi.TextWordBoundaries.h` (or `DxUi.TextElements.h`); include it from
  `RichSourceWindow.h` and `RichSemanticDocument.cpp`; drop `#include "DxUi.Internal.h"` from `RichSourceWindow.h`;
  restrict the Tests and Samples include roots to the projects that need them.
- **Validation**: build all 6 configurations; EditorControls and Accessibility suites.

### ARCH-07 — Contain test seams compiled into the shipping library

- **Kind** architecture, risk · **Verdict** Confirmed (partly overstated) · **Effort** L · **When** before production (a
  documentation note only); after production (consolidation, an API-revision change) · **Risk** medium
- **Location**: `include/DxUi/Configuration.h:4`; `include/DxUi/DxUi.h` (177 `Debug*(` declarations, 164 at base;
  `_animationClockForTest` at `:6786`); `src/Controls/DxUi.Internal.h:671-675`;
  `src/Controls/DxUi.Accessibility.cpp:271-437`;
  `src/Controls/DxUi.Menu.cpp:2129-2131`; `src/Controls/DxUi.RichTextEditor.cpp:3864-3869, 4001-4006`;
  `src/Controls/DxUi.NativeColorPalette.h:57`; `src/Controls/DxUi.PreparedMath.h:621-626`;
  `src/Controls/DxUi.TextElements.cpp:80`; `Specs/Plans/Done/CodeReview_2026-10-10.md:785-792`
- **What**: seams are always compiled. `thread_local` occurrences rose from 25 to 61, `ForTest` identifiers from 147 to 164.
  Accessibility has about 30 mutable globals; when armed they block UIA calls in `WaitForSingleObject` for up to 20,000 ms
  (`:329`) or the dispatch timeout plus 2,000 ms (`:423`, `:437`). Corrected: the arming functions
  (`DebugSetAccessibilityFocusResolutionGateForTest`, `...PeerCreationGateForTest`, `...PropertyDeliveryGateForTest`) are
  declared in the private `Internal.h:671-675`, not in `include/DxUi`; they have external linkage in `DxUi.lib` but are
  not public API. The public surface carries the `Debug*ForTest` members (`DxUi.h:517-524, 6326-6391`) and the data member
  `_animationClockForTest`. Seams also sit on production paths (thrown injections in `PrepareContent`, a MATH-table
  override in `PreparedMath.h:621-626`, a thread-local counter per code-point lookup in `TextElements.cpp:80`) and three
  designs coexist (system-call tables `ClipboardSystemCalls`, `RichImageFileCalls`, `RichImageRasterCalls`; injected
  objects; HRESULT/counter/exception variables). Already recorded as CR-37 and "Deferred by Q9 (2026-10-18) to the next
  API revision" because every rename is an API-revision event.
- **Proposal**: before production, add a comment block in `DxUi.h` and a note in `capabilities.json` that the `Debug*` and
  `ForTest` set is unsupported and outside `apiRevision`. After production (API revision): one `Detail::TestSeams` struct
  and one `thread_local TestSeams* activeSeams` in `DxUi.TestSeams.cpp` (one TLS load on the production path); fold the
  three
  call-table designs into it; replace public `DebugXxxForTest` members with `friend struct <Class>TestAccess` whose bodies
  live in `DxUi.TestHooks.cpp`; remove `_animationClockForTest` from the layout; move counters out of implementation
  headers so
  tests stop including parsers.
- **Validation**: all native suites, `-Interactive` Menu and NativeTextInput with agreement; paired complex-UI FPS and
  memory for the hot-path counter removals.

### ARCH-08 — One Unicode classification service (UTF-16, UTF-8, word, bidi, case)

- **Kind** duplication, risk · **Verdict** Confirmed (spot-checked) · **Effort** S then M · **When** opportunistic
  (step 1); after production (step 2) · **Risk** step 1 low, step 2 medium (visible navigation changes)
- **Location** (canonical service `src/Controls/DxUi.TextElements.h`, `Core_Architecture.md:85-91` "the single pure UTF-16
  decoder"): hand-written surrogate decode or encode at `Grid.cpp:429-434`; `MarkdownBlocks.h:170, 713`;
  `MarkdownInlines.h:146, 167-168`; `MarkdownLabels.h:44-45, 106`; `MarkdownMetadata.cpp:32-35`; `MarkdownSyntax.cpp:68-76`;
  `NativeFlowchart.cpp:24-31`; `NativeMath.cpp:202-203, 247-249, 1566`; `PreparedMath.h:1137-1142`;
  `RichImagePaths.h:428-431`;
  `RichSemantic.cpp:21`; `RichTextEditor.cpp:6866`; `TextClipboard.cpp:349-354`
- **What**: besides surrogates: `MetadataUtf8` (`MarkdownMetadata.cpp:21`) is a hand UTF-8 encoder that reads
  `source[++index]`
  after a lead surrogate with no bounds check (relying on "Markdown admission guarantees paired surrogates"), duplicating
  public `EncodeUtf8` (`RichDocument.h:40-45`); word characters are defined five ways (`SingleLineTextEditing.cpp:145`,
  `RichTextEditor.cpp:182`, `RichSemantic.cpp:19-27`, `RichCodeInk.h:37`, `NativeFlowchart.cpp:20`); direction is detected
  three ways (`Grid.cpp:409/437` OS tables plus hard-coded supplementary ranges, `SingleLineTextEditing.cpp:155`,
  `RichBidi.h:17`
  pinned Unicode 17); range-table lookup is duplicated (`MarkdownInlines::Contains`, `BidiRangeContains`); ASCII case
  folding
  and comparison appears nine times; ASCII punctuation four times. So the same text can classify differently in Grid,
  TextField and the rich layer.
- **Proposal**: step 1 (behaviour-neutral, opportunistic): add `AppendUtf16Scalar`, `AsciiLower`, `AsciiEqualsIgnoreCase`,
  `AsciiStartsWithIgnoreCase`, `IsAsciiPunctuation`, `RangeTableContains` to `DxUi.TextElements.h` (or
  `Support/Ascii.h`) and
  replace identical copies; replace `MetadataUtf8` with `EncodeUtf8` after confirming NUL rejection is the only extra rule.
  Step 2 (after production, behaviour-changing, needs a product decision): move Grid, SingleLine and the editor's
  history word
  grouping to pinned-table `IsWordScalar` and `StrongDirection`.
- **Validation**: `Tests/Controls/Controls.Tests.TextElements.cpp`; TextField, MultilineText, Grid and EditorControls
  suites;
  the Markdown corpus.

### ARCH-09 — Small duplicated helpers (color, geometry, hashing, line breaks)

- **Kind** duplication · **Verdict** Confirmed (spot-checked) · **Effort** S-M · **When** opportunistic · **Risk** low
- **Location**: contrast `src/Support/Utilities.h:19-29` versus `PreparedFlowchart.cpp:17-26`, `Theme.cpp:51`; packed RGB to
  `D2D1_COLOR_F` at `EditorControls.cpp:178`, `NativeColorPalette.h:55`, `PreparedFlowchart.cpp:12-15`,
  `PreparedMath.h:2849`,
  `Theme.cpp:217`; hex parsing at `EditorControls.cpp:115, 265`, `NativeColor.h:167, 197`, `MarkdownSyntax.cpp:102`,
  `NativeFlowchart.cpp:172`, `RichImagePaths.h:184, 239`; rect union at `RichSemanticGeometry.cpp:719` and
  `Accessibility.cpp:2387-2397`; `volatile float` rounding barriers at `RichDocumentLayout.cpp:20` and
  `RichSemanticGeometry.cpp:735`; FNV-1a64/fmix64 at `Internal.h:250-262` and `Accessibility.cpp`
  (`MixAccessibilityHash`/`HashControlPath`), FNV-1a32 at `Utilities.h:9`, Fibonacci hashing at `Grid.cpp:902`; two
  independent line-break sets at `Accessibility.cpp:4153-4180, 4575-4620`, a broader one at `Grid.cpp:306`.
  `catch (const std::bad_alloc&) { return E_OUTOFMEMORY; }`: about 54 identical handlers (43 by a strict grep); WIL
  `CATCH_RETURN` has 0 uses.
- **Proposal**: `Support/Color.h` (`ColorFromRgb`, `ContrastRatio(D2D1_COLOR_F, D2D1_COLOR_F)`, `ParseHexRgb`,
  `HexDigitValue`; `D2D1::ColorF(UINT32)` already converts packed RGB), `Support/Geometry.h` (`UnionInto`, `AddExact`
  with one
  documented C4746 scope), `Support/Hash.h` (`Fnv1a64`, `Mix64`); an `HResultFromCurrentException()` that rethrows
  inside named
  catch clauses so there is still no `catch (...)`; name the line-break variants (`IsCrOrLf` against `IsUnicodeLineBreak`)
  without changing semantics.
- **Validation**: Theme, EditorControls, Embedded and Accessibility suites.

### ARCH-10 — Split `DxUi.Accessibility.cpp` along its existing seams

- **Kind** architecture, build · **Verdict** Confirmed · **Effort** L · **When** after production · **Risk** medium
  (internal linkage becomes external; ODR care and a symbol diff)
- **Location**: `src/Controls/DxUi.Accessibility.cpp` (14,936 lines; 14,302 at base; one anonymous namespace spanning about
  128-12882), `src/Controls/DxUi.RichAccessibility.h` (942 lines), `src/Controls/DxUi.RichAnnotations.h` (432 lines),
  `Tests/test-scopes.json:630`
- **What**: the two "headers" are `.cpp` fragments (`#pragma once`, non-inline definitions at
  `RichAccessibility.h:13, 28` and
  `RichAnnotations.h:4, 16, 355, 361, 371, 377`) included at `Accessibility.cpp:6367-6368` inside the anonymous namespace.
  The TU parses about 1.35 MB of first-party source, the largest in the library. Regions: shared types 128-270 and 480-1000;
  test gates and diagnostics 271-480; snapshot model, build, index, navigation 1000-4000; control property and text helpers
  4000-5600; identity and provider factories 5600-6366; `AccessibilityTextRangeProvider` (45 methods) 6370-8251;
  `AccessibilityProvider` (110 methods) 8117-12880; event raising 12968-13370; registration 13370-13806; notification
  delivery
  13806-14426; `EmbeddedHost` accessibility (4 methods) 14428-14936.
- **Proposal** (mechanical, no logic edits): create `src/Controls/DxUi.AccessibilityInternal.h` holding the snapshot types,
  `ControlPath`, `WindowHostAccessibilityTarget` and the provider class declarations in `DxUi::Detail::Accessibility`; split
  into `DxUi.Accessibility.cpp` (registration, events, EmbeddedHost), `DxUi.AccessibilitySnapshot.cpp`,
  `DxUi.AccessibilityProperties.cpp`, `DxUi.AccessibilityTextRange.cpp`, `DxUi.AccessibilityProvider.cpp`,
  `DxUi.AccessibilityRich.cpp` (absorbing both fragments) and `DxUi.AccessibilityTestHooks.cpp` (the gates); add
  `DxUi.Accessibility*.cpp` to `Tests/test-scopes.json:630` and the vcxproj. If ARCH-07 consolidation comes first, the gates
  move out with it.
- **Validation**: Accessibility, Embedded and EditorControls suites plus interactive Menu and NativeTextInput
  (`test-scopes.json:630-636`); all 6 builds; paired UIA scheduling measurements
  (`Core_PerformanceAndResources.md:383`); Release symbol and size diff of `DxUi.lib`.

### ARCH-11 — Split `DxUi.RichTextEditor.cpp` behind its pimpl

- **Kind** architecture · **Verdict** Confirmed · **Effort** M · **When** after production, before the next large editor
  feature · **Risk** low to medium
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp` (7,338 lines, 2,885 at base; 209 methods;
  `struct RichTextEditor::State`
  at 296-595, about 130 fields, 968 `_state->` uses); public `RichTextEditor` methods in `DxUi.h` doubled from 75 to 150
- **Proposal**: move `State` to a private `DxUi.RichTextEditorState.h`, then split by existing method order: content and
  images
  607-876; clipboard and file drop 877-1306; properties and semantic access 1307-2301; editing, commands and search
  2302-2897;
  notifications and scroll 2898-3275; preparation 3276-4180; text-input and UIA queries 4181-4571; paint and ink
  4572-4948 and
  6783-6942; input 4949-6782 and 6943-7204; lifecycle 7205-7338.
- **Validation**: EditorControls, Embedded, Rendering, WindowHost, Theme and Accessibility suites
  (`test-scopes.json:233`); all 6 builds.

### ARCH-12 — Split `DxUi.Controls.cpp`, `DxUi.Menu.cpp` and `DxUi.Grid.cpp`

- **Kind** architecture · **Verdict** Confirmed · **Effort** M each · **When** after production, `Controls.cpp` first ·
  **Risk** `Controls.cpp` low (independent classes), `Menu.cpp` medium (anonymous-namespace types)
- **Location**: `src/Controls/DxUi.Controls.cpp` (12,294 lines, 25 classes), `DxUi.Menu.cpp` (6,973; one anonymous namespace
  43-6369; public `ContextMenu` at 6409-6583), `DxUi.Grid.cpp` (7,998; 139 `Grid::` methods)
- **Proposal**: `Controls.cpp` into `DxUi.Containers.cpp` (Panel 1282-1615, PageHost 1620-2689, StackPanel and ScrollPanel
  10596-11796, CardPanel), `DxUi.Buttons.cpp` (Button 3062-4483, Toggle/Checkbox/Radio 4492-5085), `DxUi.RangeControls.cpp`
  (ProgressBar, PageIndicator, ThroughputGraph, Slider 5137-7177), `DxUi.Navigation.cpp` (Toolbar, MenuBar, TabControl,
  StatusStrip 7854-10383), `DxUi.TagPicker.cpp` (7281-7829) and `DxUi.Overlays.cpp` (TooltipLayer 11820-12263), with shared
  helpers 467-887 in `DxUi.ControlPaint.h/.cpp`; the four `ControlHost` methods at 2365-2676 move with their owner file.
  `Menu.cpp` into `DxUi.MenuModel.h`, `DxUi.MenuPaint.cpp`, `DxUi.MenuWindow.cpp` (`MenuWndProc` at 2147) and
  `DxUi.MenuInput.cpp`. `Grid.cpp` into `DxUi.Grid.cpp`, `DxUi.GridPaint.cpp` and `DxUi.GridInput.cpp`. Update
  `test-scopes.json` patterns per file.
- **Validation**: Control, NewControls, Theme, Grid, Tree, Tooltip and MenuExitLifetime suites plus interactive Menu;
  `gallery.ps1` for unchanged visuals.

### ARCH-13 — Decompose the public `DxUi.h` and the `ControlHost` god object

- **Kind** architecture, build · **Verdict** Confirmed · **Effort** M (header split), L (`ControlHost` pimpl) ·
  **When** after production · **Risk** medium
- **Location**: `include/DxUi/DxUi.h` (6,831 lines, 194 top-level types; `ControlHost` at 6119-6794 with about 183 data
  members
  and 13 friends at 6395-6407; `RichTextEditor` 3181-3747, 150 public methods; theme and visual styles 887-1300)
- **What**: `DxUi.h` includes every rich public header and is parsed by 25 of the 46 library TUs (about 9.0 MB of parse
  volume
  per full build); a 504-byte TU (`DxUi.Typeahead.cpp`) produced a 4.9 MB Release object against 0.3 MB for
  `DxUi.PointerInput.obj`, which does not include it (2026-10-27 build, indicative).
- **Proposal**: keep `DxUi/DxUi.h` as an umbrella so consumers stay source-compatible; split into `DxUi/Theme.h`,
  `DxUi/Control.h` (Control, Panel, MeasureContext, layout records), per-family headers (Buttons, Grid with
  `IGridModel`, Tree,
  Menu, TextField, ComboBox, Containers), `DxUi/RichTextEditor.h` (the only one including rich headers) and
  `DxUi/ControlHost.h`; internal TUs include the narrow header; longer term, move `ControlHost` native-mode state (TSF, swap
  chain, timers) behind a pimpl as part of an API revision.
- **Validation**: `test-consumer.ps1` (compiles each public header independently,
  `Build_ToolchainAndConsumption.md:189-192`); all suites; build-time comparison (ARCH-16).

### ARCH-14 — Decompose the complexity hotspots

- **Kind** simplification · **Verdict** Confirmed (signatures and `Statement` length re-checked) · **Effort** M per
  function · **When** after production, alongside feature work (start with #1 and #3) · **Risk** medium (behaviour
  must stay
  identical)
- **What**: 19 functions exceed 300 lines and 45 exceed 200 (reviewer's brace-matched scan; base lengths from
  `git show a0f67fd3`; nesting counts control-statement indent levels). Lengths are HEAD (base).

| # | Location | Lines (base) | Nesting | Proposed decomposition |
|---|---|---|---|---|
| 1 | `NativeFlowchart.cpp:877` `Statement` | 1,185 (241) | 6, ~522 decision points | Shared prelude 877-1038, then per-family methods: Mindmap 1039, Gantt 1106, ER 1183, Pie 1240, Sequence 1317, Class 1380/1784, State 1481/1714, Flowchart (rest) |
| 2 | `RichTextEditor.cpp:3466` `PrepareContent` | 676 (362) | 7 | Stage functions (syntax, semantic, layout, overlay); one `ClassifyPreparationFailure()` replacing the 8 handlers (ties to ARCH-03) |
| 3 | `RichTextEditor.cpp:5649` `OnKeyDown` | 671 (170) | 9, ~402 decision points | Shortcut table: markdown, search, link, navigation, edit handlers returning `std::optional<bool>` |
| 4 | `RichTextCommands.cpp:3324` `MarkdownCommandPlanner::Table` | 659 (353) | 8 | One planner per table sub-command |
| 5 | `RichSemanticGeometry.cpp:2203` `PreparedSemanticGeometry::Advance` | 648 (379) | 6 | Per-phase advance (measure, place, raster staging) |
| 6 | `Grid.cpp:3958` `Grid::Paint` | 614 (614) | 7 | Header, body rows, overlays and scrollbars |
| 7 | `Accessibility.cpp:2215` `AppendAccessibilitySnapshotNavigation` | 614 (501) | 10 | Per-control record builders (grid, tree, tab, rich); reuse `Union` (ARCH-09) |
| 8 | `RichSemanticGeometry.cpp:1490` `LayoutOne` | 594 (374) | 7 | Per element kind (paragraph, table, image, diagram, math) |
| 9 | `TextInput.cpp:2220` `TextField::OnKeyDown` | 487 (486) | | Share the key table with #3 |
| 10 | `ComboBox.cpp:1245` `ComboBox::OnKeyDown` | 463 (462) | | Key table |
| 11 | `NativeTextInput.cpp:1669` `HandleNativeTextInputImeMessage` | 413 (413) | | Pre-window |
| 12 | `Accessibility.cpp:8686` `GetPropertyValue` | 407 (407) | | Property table |
| 13 | `MarkdownBlocks.h:1064` `Line` | 402 (359) | 4 | Per block-start kind |
| 14 | `Menu.cpp:5992` `RunMenuModalLoop` | 375 (375) | | Pre-window |
| 15 | `RichTextEditor.cpp:5290` `OnChar` | 358 (75) | 10 | `TryAutoPair`, `TrySmartTypography`, `TryMarkdownAutoFormat`, task-space handling |
| 16 | `Menu.cpp:3133` `MenuContentControl::Paint` | 358 (358) | | Pre-window |
| 17 | `Layout.cpp:614` `FlexPanel::BuildLayout` | 341 (341) | | Pre-window |
| 18 | `RichTextEditor.cpp:4572` `Paint` | 330 (156) | 6 | Background, source ink, semantic page, overlays |
| 19 | `Grid.cpp:5852` `Grid::OnKeyDown` | 310 (266) | 4 | Key table |
| 20 | `Menu.cpp:5476` `RouteMenuKeyboardEvent` | 296 (279) | | Pre-window |

  Window growth below 300 lines: `PreparedFlowchart::Draw` 284 (91), `SemanticBuilder::Inline` 283 (137),
  `MarkdownInlines::Scan` 282 (157), `AdvanceDiagram` 253 (107), `PreparedFlowchart::Advance` 235 (107),
  `PreparedMath::PlaceUnaryAccent` 234 (new), `MeasureBoxes` 233 (176).
- **Validation**: native flowchart corpus (`Tests/Support/Support.Tests.NativeFlowchartCorpus.h`) for #1; EditorControls and
  Embedded for the editor; paired performance for paint and preparation functions.

### ARCH-15 — Header-only implementations: keep, move to .cpp, or rename by evidence

- **Kind** build, architecture · **Verdict** Confirmed · **Effort** S-M · **When** opportunistic for 2 and 4; after
  production for 1 · **Risk** low
- **Evidence** (transitive includers over 46 lib TUs and 49 test TUs): `DxUi.PreparedMath.h` 162,120 bytes (1 lib TU, 4 test
  TUs; its test seams are declared at `:35-44` but defined in `RichSemanticGeometry.cpp:35-44` and `NativeMath.cpp:19`);
  `DxUi.MarkdownInlines.h` 94,237 (1, 5; tests include it for one counter at `:18`); `DxUi.MarkdownUnicodeData.h` 89,379
  (6, 6; pulled in by `MarkdownLabels.h`; dead in `RichTextEditor.cpp`, ARCH-05); `DxUi.MarkdownEntityData.h` 75,940 (1, 0);
  `DxUi.MarkdownBlocks.h` 67,615 (1, 3); `DxUi.Internal.h` 51,683 (23, 42; 44 lines start with `inline`);
  `DxUi.RichAccessibility.h` / `DxUi.RichAnnotations.h` 50,196 / 21,622 (`.cpp` fragments, ARCH-10);
  `DxUi.NativeFlowchart.h` 18,092 (7, 9). ODR: only `inline`, template or in-class definitions exist in headers; the two
  fragments are valid only because each is included once. Data tables are `inline constexpr` (one COMDAT). The only
  per-project
  define difference is `DXUI_POSTED_PAYLOAD_ICF_REQUIRED` (`Tests/Controls/DxUi.ControlTests.vcxproj:12`), consumed only
  by test
  code, so ODR risk today is low; the cost is test coupling and 4 to 5 test TUs recompiling parser and math code.
- **Proposal**: (1) `PreparedMath.h` becomes a declaration header (about 250 lines) plus `DxUi.PreparedMath.cpp`, which
  owns its
  seams (Release uses LTCG, `Directory.Build.targets:4-15`, so no inlining is lost); (2) a small
  `DxUi.UnicodeProperties.cpp`
  behind `MarkdownUnicodeData.h` (`IsUnicodePunctuation`, `IsUnicodeSymbol`, `FoldScalar`, `StrongDirection`) and
  `MarkdownLabels.h` bodies in `MarkdownLabels.cpp` (six TUs stop parsing 89 KB, and ARCH-08's duplicate `Contains` helpers
  disappear); (3) leave `MarkdownInlines.h` and `MarkdownBlocks.h` header-only but move their test counters into the
  seam header
  (ARCH-07); (4) rename the fragments to `.inl` or absorb them in ARCH-10.
- **Validation**: Embedded and EditorControls suites; the math corpus (`Tests/Support/Support.Tests.MathCorpus.h`); all
  6 builds.

### ARCH-16 — Measure and fix library build throughput

- **Kind** build · **Verdict** Confirmed · **Effort** M · **When** after production · **Risk** low to medium (PCH
  interacts with
  `/ZI`, ASan and receipts)
- **Evidence**: no `PrecompiledHeader`, `MultiProcessorCompilation`, `CL_MPCount` or `UseMultiToolTask` anywhere in
  `Directory.Build.*`, `Build/`, `src/DxUi.vcxproj` or the test projects; `build.ps1:186` passes `/m`, which parallelizes
  projects, not TUs inside `DxUi.vcxproj`. First-party parse volume is 19.5 MB per full library build against 6.2 MB of
  unique
  source (3.2x). The 2026-10-27 Release objects total 182 MB and `DxUi.lib` is 189 MB (LTCG IL); that build predates
  HEAD, so
  these figures are indicative.
- **Proposal**: baseline with C++ Build Insights (`vcperf`) or `/Bt+ /d1reportTime` on x64 Debug and Release; enable
  `UseMultiToolTask` or `/MP` if receipts and attestation tolerate it; add a PCH for the 25 control TUs (windows.h,
  d2d1_1.h,
  dwrite_3.h, d3d11, UIAutomation.h, wil, STL, DxUi.h) or first trim `DxUi.Internal.h` inline bodies; apply ARCH-13 and
  ARCH-15; compare cold and warm build times with paired receipts (`AGENTS.md` requires a retained baseline).
- **Validation**: all 6 builds; build receipts; zero-warning check.

### ARCH-17 — Warning-suppression boilerplate (none hides actionable diagnostics)

- **Kind** build, simplification · **Verdict** Confirmed · **Effort** S-M · **When** opportunistic · **Risk** low
- **Evidence**: `disable : 4625|4626|4623` blocks rose from 3 to 72; 65 "Preserve aggregate and overload behavior"
  comments and
  65 "intentionally delete these implicit operations" lines were added by `6ed0512c` (2026-10-28); `DxUi.h` alone has 37
  push/pop pairs. Other local suppressions: 4746 x6 (the `wrl/implements.h` include in `PreparedMath.h:25`,
  `FileDrop.cpp:14`,
  `RichSemanticGeometry.cpp:23`, `RichTextLayout.cpp:16`, plus two `volatile` helpers), 4464 x2 (ThirdParty relative
  includes
  at `MarkdownMetadata.cpp:13`, `YamlMetadata.cpp:19`), 4623 x2. The project-wide list (4514, 4710, 4711, 4820, 5045;
  4746 for
  ARM64 Release only) is documented in `Build/NativeBuildPolicy.targets:24-33`. No blanket suppression of an actionable
  warning
  was found.
- **Proposal**: for non-copyable polymorphic types replace each push/disable/pop triple and its two-line comment with an
  explicit
  `DXUI_NONCOPYABLE_NONMOVABLE(Type)` macro (`= delete` members do not trigger C4625, C4626, C5026 or C5027); or record one
  documented project-level exception.
- **Validation**: zero-warning builds in all 6 configurations; the consumer fixture compiling each public header under
  `/Wall`.

### ARCH-19 — Name the fallible zero-size construction idiom

- **Kind** simplification · **Verdict** Confirmed · **Effort** S-M · **When** opportunistic · **Risk** low
- **Evidence**: about 546 occurrences of `(0u, L' ')` or `(0u)` explicit construction (318 at base; 535 versus 306 by the
  verifier's stricter pattern), 186 of them `(0u, L' ')`. The rationale lives only in
  `Specs/Core/Core_PerformanceAndResources.md:78-84` (MSVC Debug iterator proxies allocate in noexcept constructors);
  the code
  has no named helper.
- **Proposal**: `Detail::FallibleEmptyString()` and `Detail::FallibleEmptyVector<T>()` with the rationale comment, then
  replace
  mechanically; add a format check so new code uses them.
- **Validation**: allocation-failure sweeps in x64 Debug.

### ARCH-20 — Naming and dialect drift

- **Kind** simplification · **Verdict** Confirmed (file names re-checked; counts not) · **Effort** S · **When**
  opportunistic,
  together with the ARCH-12 splits · **Risk** low
- **Evidence**: member naming `member_` in 30 files (rich and Markdown) versus `_member` in 19 (controls and host);
  `RichTextEditor.cpp` mixes both (979 against 45). Private namespaces: `DxUi::Detail` (4 blocks),
  `DxUi::RichText::Detail` (43), `DxUi::Internal` (1), `DxUi::RichText::MarkdownEntityData`, `DxUi::UnicodeGraphemeData`.
  File names: `DxUi.cpp` holds the `Control` base (115 methods); `DxUi.WindowHost.cpp` holds `ControlHost` (169 methods;
  `WindowHost` is an alias);
  `TextClipboard.cpp`, `TextInputServices.cpp`, `NativeMenuInterop.cpp` and `ControlCatalog.cpp` lack the `DxUi.` prefix;
  `DxUiNativeMenuInterop.h` has no dot.
- **Proposal**: record the two dialects as layer conventions in the contributing guide; rename `DxUi.cpp` to
  `DxUi.Control.cpp`
  and `DxUi.WindowHost.cpp` to `DxUi.ControlHost.cpp` during the ARCH-12 splits (renames touch `source-origin.json` and
  `test-scopes.json`); fold `DxUi::Internal` into `DxUi::Detail`.
- **Validation**: `validate.ps1` and builds.

## Refuted

No finding was refuted in full. Claims within findings that the verifier refuted or narrowed:

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| ARCH-01 | RichTextEditor "contradicts the existing helper"; the editor needs a policy decision as if it were drift | Fatal observer policy is documented and accepted: `Specs/UI/UI_InputAndAccessibility.md:76, 1327, 1361, 1541, 1554`; `docs/rich-text.md:499`; decision Q2 at `Specs/Plans/Done/CodeReview_2026-10-31.md:167`. The editor conforms. |
| ARCH-01 | The 13 listed callback blocks account for the 11 `std::terminate()` calls | Only 9 of the 13 do (2088, 2155, 2935, 3003, 3085, 3173, 5807, 5831, 6992); 2886 and 6843 return `E_OUTOFMEMORY`, 7128 swallows, 5865 has no own `try` (outer handler `RichTextEditor.cpp:6312` declines). The other two terminates are 1179 and 6361, which are not among the 13. |
| ARCH-02 | Device loss "goes unnoticed" and the image is refused permanently after recovery | Loss is detected at the next draw (`WindowHost.cpp:4302, 4471, 4803, 5126`); prepared state is rebuilt when target or generation changes (`RichSemanticGeometry.cpp:2162-2166, 2174-2181`; `WindowHost.cpp:4271, 4539`). |
| ARCH-03 | Fail fast (`__fastfail`/`WI_ASSERT`) at the `logic_error` handler in Debug and ASan | The tests inject a plain `std::logic_error` and assert the fallback: `Tests/Support/Support.Tests.RichSemanticEditor.h:3105-3150` with `RichTextEditor.cpp:3868, 4005`. |
| ARCH-05 | `PreparedMath::Locale()` has 1 test reference | 0 references anywhere (`src/Controls/DxUi.PreparedMath.h:84` only). |
| ARCH-06 | `RichSemanticDocument.cpp` needs only `PreparedDisplayWordRange` and can drop `Internal.h` by moving two declarations | It also calls `IsWordCharacter` (`RichSemanticDocument.cpp:180, 193`) and `StepToNextTextElement` (`:195`); both are declared only in `DxUi.Internal.h:759, 762`. |
| ARCH-07 | Test arming hooks can stall UIA threads as a consumer-reachable risk | The gate arming functions are declared in the private `src/Controls/DxUi.Internal.h:671-675`, not in `include/DxUi`. |
| ARCH-02 | The one-line fix `! IsDeviceLossHResult(create)` can be applied in place | `IsDeviceLossHResult` is declared only in `include/DxUi/DxUi.h:104`, which the rich geometry TU does not include; a shared private classifier header is required. |

## Error-handling catalogue and proposed single policy

Catalogue (HEAD, with base in parentheses; counts re-verified except where noted):

- `noexcept` appears about 5,861 times (5,226). `catch (...)` appears 0 times.
- `catch (const std::bad_alloc&)`: 170 (115). `catch (const std::exception&)`: 122 (93). `catch (const std::length_error&)`:
  18 (11). `catch (const std::logic_error&)`: 3 (1), all in `RichTextEditor.cpp` at 4102, 4539 and 6565.
  `catch (const wil::ResultException&)`: 2.
- Handler bodies: about 54 return `E_OUTOFMEMORY`; 17 `std::exception` handlers and 16 `bad_alloc` handlers are empty or
  comment-only. `std::terminate()` is called 14 times: 11 in `RichTextEditor.cpp` (ARCH-01, documented fatal observer
  policy), `Controls.cpp:185` and `WindowHost.cpp:460` (diagnostics tracing, `bad_alloc`), and `WindowHost.cpp:506` (failed
  `new (std::nothrow)` of the per-thread resource map). Seven handlers return `E_INVALIDARG` for any `std::exception`;
  `Accessibility.cpp:7608` has no preceding `bad_alloc` handler, so out-of-memory is reported to UIA as `E_INVALIDARG`
  (pre-window).
- Throws: `invalid_argument` 175 (107), `length_error` 96 (66), `logic_error` 44 (35), `out_of_range` 39 (34),
  `overflow_error` 13, `bad_alloc` 6. `THROW_IF_FAILED` 63 (24), `RETURN_IF_FAILED` 66 (50), `CATCH_RETURN` 0.
- A heuristic scan (reviewer, not re-derived) found 39 `noexcept` function bodies that allocate with no `try`; on OOM these
  terminate. Examples: `Accessibility.cpp:10561` `GetColumnHeaders` (a UIA COM method), `NativeMenuInterop.cpp:123, 253`,
  `TextInput.cpp:3298`. Almost all are pre-window.
- Device loss is classified only in `WindowHost.cpp` (ARCH-02). The only retry loop is the intentional two-pass
  `FlushNativeAccessibilitySnapshot` (`Accessibility.cpp:14331`), which logs through `OutputDebugStringW` (6 direct uses in
  `Accessibility.cpp`) rather than the diagnostics sink.

Proposed single policy (corrected where the verifier changed it):

1. **Out of memory** is recoverable at every library boundary: preserve the prior publication and return `E_OUTOFMEMORY` or
   `false`. The only exceptions are the callbacks that the contract lists as fatal observers (today: the RichTextEditor
   observer sites, per Q2); that list is documented in one table (ARCH-01).
2. **Device or target loss** always goes through the one shared classifier (`IsTargetLossHResult` in
   `Support/HResultClass.h`, with `IsDeviceLossHResult` forwarding to it) and is never cached as a content rejection
   (ARCH-02).
3. **Deterministic content rejection** uses one typed family (`PreparationRejected` and its subclasses) and is cached
   per view
   key (`preparationRejected`); capacity and unsupported conditions of one object reject only that object (ARCH-03).
4. **Caller contract violations** in public fallible APIs throw `invalid_argument` or `out_of_range` and the header says
   so with a
   `// Throws:` line; at noexcept COM or callback boundaries they map to `E_INVALIDARG`.
5. **Internal invariants** use `DXUI_INVARIANT(cond)`, which throws a distinct `InvariantViolation : std::logic_error`:
   it is
   logged at Error with `what()` and a counter, fails fast only under `_DEBUG`/ASan, and takes the existing rejection path
   in Release. Plain `logic_error` stays a rejection, so tested fault injection keeps working.
6. **Consumer callbacks** go only through `InvokeConsumerCallback` with an explicit failure policy argument.
7. **Diagnostics** never terminate and never bypass the sink.

Mechanical steps:

1. Add `src/Support/ErrorPolicy.h` with `HResultFromCurrentException()` (rethrow inside named catches),
   `InvokeConsumerCallback`,
   `DXUI_INVARIANT`, and `src/Support/HResultClass.h` with the HRESULT classifiers.
2. Replace the about 54 `bad_alloc` to `E_OUTOFMEMORY` handlers, and their trailing `std::exception` handlers in COM
   methods,
   with `catch (const std::exception&) { return HResultFromCurrentException(); }`.
3. Apply ARCH-01 (table first), ARCH-02 and ARCH-03 (log and docs first).
4. Triage the 39 heuristic `noexcept`-allocating functions: wrap each, or annotate it `// Terminates on OOM: <reason>`.
5. Add `Throws:` lines to the public rich headers.

## Recommended sequencing

1. **Before production (about 1 to 2 days, behaviour-neutral or bug-fix; no item blocks release):**
   - ARCH-02 (shared classifier header and the line-472 condition, with the new seam test)
   - ARCH-18 WIC factory (about 6 lines, plus the two-thread test)
   - ARCH-03 short part (bind and log `what()` at Error, counter, `Throws:` docs; no fail-fast)
   - ARCH-04 (delete the never-compiled `#else` arms)
   - ARCH-05 (dead code and includes, with the `source-origin.json` retirements)
   - ARCH-06 (move the five declarations; compile-only)
   - ARCH-01 documentation table and the Question 1 decision
   - ARCH-07 documentation-only note
   Each is small and reviewable. Run the scoped suites from `Tests/test-scopes.json` (`Test-Changes.ps1 -Explain`) and all 6
   builds.
2. **First post-production maintenance cycle:** ARCH-08 step 1, ARCH-09, ARCH-15 (2) and (4), ARCH-19, ARCH-17, the
   idiom part of
   ARCH-18, and the error-policy rollout (catalogue steps 1, 2, 4).
3. **Structural cycle:** splits in order ARCH-12 (`Controls.cpp`), ARCH-11 (RichTextEditor), ARCH-10 (Accessibility),
   ARCH-12
   (Menu, Grid), then ARCH-15 (1) `PreparedMath.cpp`; next ARCH-16 build measurement and PCH, then ARCH-13 (`DxUi.h`
   umbrella). Each split is a zero-logic diff: compare Release `DxUi.lib` symbols and size, run paired complex-UI FPS and
   memory, and update `test-scopes.json`, the vcxproj and `source-origin.json`.
4. **With feature work:** ARCH-14 decompositions (start with `Statement` and `OnKeyDown`/`OnChar`), ARCH-07 seam
   consolidation
   (API revision), ARCH-03 typed family and `InvariantViolation`, ARCH-08 step 2, and the `ControlHost` pimpl.

## Test-quality observations

- `Tests/Support/Support.Tests.RichImageResources.h:314-326` drives the SVG "unsupported target" path only through the
  boolean
  `testRichSvgTargetUnsupported`. No test can make `CreateSvgDocument` return a specific HRESULT, so the ARCH-02
  misclassification cannot be caught; the `RichImageRasterCalls` table covers `createBitmap`/`copyBitmap` but not SVG.
- `Tests/Support/Support.Tests.RichSemanticEditor.h:3105-3150` proves the whole-view fallback and its retry keys with an
  injected plain `std::logic_error`. It cannot tell a designed rejection from a broken invariant, and it would trip any
  naive
  fail-fast in the shared handler. It does not assert the log level or text.
- Reviewer observation, not re-verified: `Tests/Support/Support.Tests.RichSemantic.h:2` includes the whole 94 KB
  `DxUi.MarkdownInlines.h` only to reach one counter (`testMathOwnershipProbeWork`), coupling tests to parser internals.
- The terminate behaviour of the 11 editor observer sites is observable only through an in-process terminate handler
  (`Tests/Embedded/Embedded.Tests.Embedded.cpp:1360` installs one for other purposes). This review did not establish whether
  any fixture drives an editor observer to throw `bad_alloc`.
- No test exercises two UI threads reaching the menu icon WIC factory (ARCH-18).

## Questions for the developer

1. Is `std::terminate()` on `bad_alloc` from the RichTextEditor observers (11 sites) intentional, and should it be the
   library
   rule? Recommended: yes, it is intentional and documented (Q2; `UI_InputAndAccessibility.md:76, 1327, 1361, 1541, 1554`;
   `docs/rich-text.md:499`). Keep it, add one callback-failure table to `docs/hosting.md`, and decide separately whether
   Grid
   and WindowHost observer delegates should also terminate or whether the editor should contain (premise "RichTextEditor
   contradicts the helper": false, see verifier).
2. Is `DXUI_ENABLE_DIAGNOSTICS=1` permanent, so the `#if` guards and the about 290 `#else` lines can go? Recommended: yes
   (`Configuration.h:4`, API revision 2, "not a consumer-selectable build option"); delete the `#else` arms now (the
   2026-10-28
   review item A4b was never closed) and keep the guards. Add a CI configuration only if a no-hooks build becomes a goal.
3. Are the 177 public `Debug*`/`ForTest` declarations and the `*DebugState` structs part of the consumer contract under
   `apiRevision`? Recommended: no. State that they are unsupported now; move them behind friend accessors in the next API
   revision, as CR-37/Q9 already deferred.
4. Should internal invariant violations in rich preparation fail fast in Debug and ASan instead of falling back silently?
   Recommended: yes, but only for a distinct `InvariantViolation` type raised at the invariant, never at the generic
   `logic_error` handler, because the whole-view source fallback for deterministic failures is designed and tested.
5. Should Grid, TextField and the editor's history word grouping move from `GetStringTypeW` to the pinned Unicode 17 tables?
   Recommended: yes in principle for consistency, but after production and with a visible-behaviour decision recorded;
   do the
   behaviour-neutral helper consolidation (ARCH-08 step 1) first.
6. Is compiling the library's TUs without `/MP` or MultiToolTask deliberate (for example for deterministic receipts),
   and would a
   PCH be acceptable to the attestation and receipt tooling? Recommended: measure first (ARCH-16 step 1) and confirm
   with the
   receipt owner before changing build flags.
7. File splits change paths that appear in `test-scopes.json`, `source-origin.json` and historical measurement
   manifests. Should
   a split keep the original file as its first part to avoid `source-origin.json` edits? Recommended: yes for
   `Controls.cpp`,
   `Menu.cpp` and `Grid.cpp`; rename only `DxUi.cpp` and `DxUi.WindowHost.cpp` (ARCH-20) in a dedicated commit that
   updates the
   JSON files together.

## Coverage

| File or scope | Depth |
| --- | --- |
| `AGENTS.md`, `Specs/Core/Core_Architecture.md`, `Specs/Core/Core_PerformanceAndResources.md` (policy sections), `Specs/Build/Build_ToolchainAndConsumption.md` (84-223), `.agents/skills/modern-cpp-wil`, `.agents/skills/control-development` | Read in full or in the cited ranges (reviewer) |
| `src/` and `include/` tree (46 TUs, 137 files, about 140,800 lines) | Whole-tree counts by grep, `wc` and stdin scripts; include-graph DFS (reviewer); headline counts re-run by the verifier |
| `src/Controls/DxUi.RichSemanticGeometry.cpp` | Cited functions in full; `PrepareOne` 395-600, `PrepareContent` interaction 2150-2200, 2440-2500, 3265-3300 re-read by the verifier |
| `src/Controls/DxUi.RichTextEditor.cpp` | `PrepareContent` handlers 4030-4145, the 13 callback sites and 11 terminate sites, `OnKeyDown` catch 6300-6325 re-read by the verifier |
| `src/Controls/DxUi.WindowHost.cpp`, `DxUi.Controls.cpp`, `DxUi.Menu.cpp` | Cited ranges only (diagnostics wrappers, `#else` arm bounds, WIC factory, per-thread pool) |
| `src/Controls/DxUi.Accessibility.cpp` | Test-gate block 262-345 and cited sites; function inventory by scan (reviewer) |
| `src/Controls/DxUi.Internal.h`, `include/DxUi/DxUi.h`, `include/DxUi/Configuration.h` | Cited ranges |
| Build files (`Directory.Build.*`, `Build/*.props|targets`, `src/DxUi.vcxproj`, `build.ps1`) | Grepped for PCH, `/MP`, `DXUI_ENABLE_DIAGNOSTICS` (verifier) |
| `Tests/`, `Samples/`, `Specs/Plans/Done/CodeReview_*`, `Specs/UI/UI_InputAndAccessibility.md`, `docs/` | Used only for call-site, policy and test-injection evidence |
| `ThirdParty/` | Skipped (not first-party) |
