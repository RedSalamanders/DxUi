# Window host, embedded host and public API: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the Win32 window host
(`src/Controls/DxUi.WindowHost.cpp`), the embedded D3D11 host (`src/Rendering/Embedded.cpp`, `include/DxUi/Embedded.h`),
the animation dispatcher and the public headers under `include/DxUi` changed in the window.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity. No crash, use-after-free or
hang was found in the new window-host or embedded paths. 16 findings survive (all P3), 1 was refuted, 0 duplicates.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| HOST-01 | P3 | Confirmed | Native mouse-leave skips tooltip and hover cleanup when a hover-out callback changes geometry | `src/Controls/DxUi.WindowHost.cpp:3493` | S |
| HOST-02 | P3 | Confirmed | `EmbeddedHost::ApplyClipboardCut` has no empty-selection precondition and no test coverage | `src/Rendering/Embedded.cpp:1260` | S |
| HOST-03 | P3 | Confirmed | Ctrl key edges over a hovered control dirty a clean embedded surface | `src/Rendering/Embedded.cpp:987` | S |
| HOST-04 | P3 | Plausible | Write the revision-4 consumer addendum for aggregate reorders and embedded behaviour changes | `Specs/Build/Build_ToolchainAndConsumption.md:287` | S |
| HOST-05 | P3 | Plausible | `AnimationDispatcher::OnTimerTick` is not reentrancy-safe | `src/Support/AnimationDispatcher.h:399` | S |
| HOST-06 | P3 | Plausible | Shared-pool device reset wakes only the failing native host | `src/Controls/DxUi.WindowHost.cpp:588` | S |
| HOST-07 | P3 | Confirmed | Embedded pointer release always drops capture; native keeps a capture taken during release | `src/Rendering/Embedded.cpp:924` | S |
| HOST-08 | P3 | Confirmed | Supplemental rich-title revalidation scans all semantic elements twice per live-tooltip tick | `src/Controls/DxUi.WindowHost.cpp:5882` | S |
| HOST-09 | P3 | Confirmed | Every successful embedded Prepare re-runs text-input activation for a non-text focus | `src/Rendering/Embedded.cpp:615` | S |
| HOST-10 | P3 | Confirmed | `requestPreparation` header lost its no-reentry restriction; hosting guide omits native visibility API | `include/DxUi/Embedded.h:29` | S |
| HOST-11 | P3 | Confirmed | `WM_MBUTTONUP` is swallowed during any capture; middle press skips overlay dismissal | `src/Controls/DxUi.WindowHost.cpp:3521` | S |
| HOST-12 | P3 | Confirmed | Click-sequence routing differs between `CS_DBLCLKS` and plain host windows | `src/Controls/DxUi.WindowHost.cpp:3562` | S |
| HOST-13 | P3 | Confirmed | Redundant string copy on every hover tooltip update | `src/Controls/DxUi.WindowHost.cpp:1702` | S |
| HOST-14 | P3 | Confirmed | Dead duplicated `Render` body under `#else` of a fixed macro | `src/Controls/DxUi.WindowHost.cpp:4601` | S |
| HOST-15 | P3 | Plausible | `Control::SetBounds` publishes a synchronous TSF layout change before preparation | `src/Controls/DxUi.cpp:385` | M |
| HOST-16 | P3 | Plausible | Child-window hosts get no TSF layout notification when an ancestor window moves | `src/Controls/DxUi.WindowHost.cpp:3394` | S |

## P3 findings

### HOST-01 — Native mouse-leave skips tooltip and hover cleanup when a hover-out callback changes geometry

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** medium-high ·
  **Effort** S · **Introduced** `d941ebf9`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:3484-3508` (early returns at 3493-3494 and 3498-3499),
  `src/Controls/DxUi.cpp:368`, `:432`, `:484`
- **What**: `WM_MOUSELEAVE` snapshots `originalInteraction = GetInteractionRevision()` (3486) and returns early after
  `OnHoverChanged(false)` (3493) and after `OnMouseLeave` (3498) when the root changed or the interaction revision differs.
  The early return skips `OnMouseLeave` (first site) and the host-owned cleanup at 3502-3508 (`_supplementalTooltip*`
  reset, `ClearTooltip()`, `Invalidate()`). The rule added by the same commit (`Specs/UI/UI_InputAndAccessibility.md`)
  only requires that the old leave must not clear a replacement root's tooltip; root replacement already resets tooltip
  state through `SetRoot`. `Control::SetBounds`, `SetVisible` and `SetEnabled` each `++_interactionRevision` when the
  control has a host, so any geometry, visibility or enabled change made in a hover-out callback takes the early return.
  Unlike `UpdateHover` (line 5768), a leave has no later event that would retry the cleanup.
- **Failure scenario**: a consumer-derived control hides or relayouts a hover-revealed child in `OnHoverChanged(false)`
  (common pattern); the pointer leaves the window while a delayed tooltip is pending or shown. `WM_MOUSELEAVE` returns at
  3493. The pending tooltip still appears after its show delay although the pointer is outside the window, and stays until
  the 5000 ms auto-hide in `TooltipLayer::Tick` (`src/Controls/DxUi.Controls.cpp:12097-12131`); the hovered control's own
  `OnMouseLeave` never runs (stale hover visuals in a consumer container derived from `TabControl` or `ScrollPanel`).
- **Fix**: finish host-owned cleanup unless the root was replaced; skip only the remaining callbacks.
  ```cpp
  const bool rootReplaced = originalRoot && (originalRootLifetime.expired() || _root.get() != originalRoot);
  // after OnHoverChanged(false):
  if (!rootReplaced && GetInteractionRevision() == originalInteraction)
      { /* revalidate oldHover via RevalidateDispatchedControl, then OnMouseLeave */ }
  if (rootReplaced) return 0;            // SetRoot -> ResetRootInteractionState already cleared tooltip state
  // reset _supplementalTooltipControl/Lifetime/Text/IsRichTitle/NeedsResolve; ClearTooltip(); Invalidate(); return 0;
  ```
  Keep the `RevalidateDispatchedControl` guard on the `OnMouseLeave` call. Do not widen the replacement-root rule.
- **Regression test**: `Tests/Controls/DxUi.Tests.WindowHost.cpp`, extend
  `TestWindowHostThrowingMouseLeaveStillClearsTooltip` with scenario 3: `OnHoverChanged(false)` calls `SetVisible(false)`
  on itself (or `SetBounds` on a sibling). Seed with `window.Host().SetTooltip(L"synthetic tooltip", {32, 32})` as the
  existing scenarios do, send `WM_MOUSELEAVE`, assert `window.Host().GetTooltipText().empty()`. Fails at `6df17007`
  (text remains), passes after. No tick assertion is needed.
- **Verifier notes**: no in-tree `OnHoverChanged` override (`Button` 4359, `Slider` 7102) or `OnMouseLeave` override
  mutates bounds, visibility or enabled state, so only consumer controls reach it. The tooltip does not persist until the
  pointer re-enters (delayed tooltips auto-hide after 5 s); only a direct `host.SetTooltip` persists, and its caller owns it.

### HOST-02 — `EmbeddedHost::ApplyClipboardCut` has no empty-selection precondition and no test coverage

- **Severity** P3 (reviewer P2, verifier P3) · **Category** api hardening · **Verdict** Confirmed · **Confidence** high
  (mechanism), reachability only for direct callers · **Effort** S · **Introduced** `ed1b0959`
- **Location**: `src/Rendering/Embedded.cpp:1245-1261` (call at 1260), `include/DxUi/Embedded.h:191`,
  `src/Controls/DxUi.RichTextEditor.cpp:5087`, `src/Controls/TextInputServices.cpp:834`
- **What**: the public method validates revision, focus id, text, caret and anchor equality, read-only and composition, then
  calls `editor->Erase(_host, false, false)`. `RichTextEditor::Erase` with an empty selection and `backwards=false`
  deletes the next text element. Nothing in the method or its header declaration (no comment) requires a non-empty
  selection. The method has no test: nothing under `Tests/` references `ApplyClipboardCut`, `ApplySemanticCut` or
  `TextClipboardSemanticCut`, so even the selection-cut happy path is unexercised.
- **Failure scenario**: an application calls `view.ReadTextInput(snapshot)` with a caret-only selection and then
  `view.ApplyClipboardCut(snapshot)` directly (outside `TextInputServices`) → S_OK and the element after the caret is
  deleted with nothing placed on the clipboard. The documented route cannot reach it: Ctrl+X and Shift+Delete go through
  `TextInputServices::PreTranslate` (`TextInputServices.cpp:989-1000`) → `Clipboard(Cut)`, which returns S_FALSE at 834
  (`if (first == last)`) before writing the clipboard and before calling `TextClipboardSemanticCut::ApplySemanticCut`
  (843-845). The editor's own context-menu Cut is disabled for an empty selection (`DxUi.RichTextEditor.cpp:1722`). The
  post-write TOCTOU window is closed by the snapshot equality check at `Embedded.cpp:1252-1254`.
- **Fix**: add an early S_FALSE guard inside `ApplyClipboardCut`, after the revision-equality check and before `Erase`:
  `if (! current.state.selectionAnchorIndex || *current.state.selectionAnchorIndex == current.state.caretIndex) return S_FALSE;`
  (same collapsed-selection definition as `TextInputServices.cpp:827-835`). Document the result codes at
  `Embedded.h:191`: S_FALSE empty selection or composition active, `E_ACCESSDENIED` read-only, `ERROR_REVISION_MISMATCH`
  stale snapshot, `E_NOTIMPL` focus is not a `RichTextEditor`.
- **Regression test**: add a direct test next to the embedded text-input coverage
  (`Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h`): prepare a focused editable `RichTextEditor` containing a
  multi-character text, place the caret mid-text (otherwise nothing is erased at end of text), `ReadTextInput`, call
  `ApplyClipboardCut(snapshot)`; assert S_FALSE and unchanged text (fails at `6df17007`: S_OK and one element removed).
  In the same test cover: a real selection → S_OK and the selection removed; a stale `revision` →
  `HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH)`; read-only → `E_ACCESSDENIED`; a focused `TextField` → `E_NOTIMPL`.
- **Verifier notes**: all callers were enumerated (definition, header, `Samples/EmbeddedControls/EmbeddedTextClient.h:18`,
  prose at `docs/rich-text.md:549-552`). `TextInputServices.h:137-138` documents the cut hook as "after clipboard
  publication", so the route through `TextInputServices` is the designed one. Not silent data loss on any library or
  documented path, hence P3 hardening rather than P1.

### HOST-03 — Ctrl key edges over a hovered control dirty a clean embedded surface

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high
  (mechanism), medium (frequency) · **Effort** S · **Introduced** `e294f921`
- **Location**: `src/Rendering/Embedded.cpp:987-992`, `src/Controls/DxUi.WindowHost.cpp:1892-1903` and `:5944-5949`,
  `src/Rendering/Embedded.cpp:264-272`
- **What**: `DispatchKeyImpl` handles `VK_CONTROL`, `VK_LCONTROL` and `VK_RCONTROL` before `KeyboardInputIsAvailable()`
  (993) whenever `_host._hoveredControl` is non-null: it sets `_supplementalTooltipModifiers` and
  `_supplementalTooltipNeedsResolve` and calls `RequestAnimation()`. The embedded branch of `RequestAnimation` (1896-1902),
  when animation was not already requested, calls `Invalidate()` → `EmbeddedHost::MarkDirty` (`dirty = true`,
  `++revision`, `RequestPreparation()`), so the next `Prepare` repaints the surface. An idle `AdvanceAnimation` tick
  resets the flag (386), so the key-up repeats it. The modifiers are consumed only by `HoveredTitle`
  (`DxUi.WindowHost.cpp:5788`), and `HoveredTitle` depends on Ctrl only for an editable `RichTextEditor`
  (`DxUi.RichTextEditor.cpp:6757`).
- **Failure scenario**: the pointer rests over a clean embedded view containing a `Button`; the user presses and releases
  Ctrl (Ctrl+C, Ctrl+S) → two application wakes and two full-surface prepares of an otherwise clean view.
- **Fix**: set the modifier state always (cheap), but request the resolve tick only when the hovered or supplemental
  target is an editable `RichTextEditor` (the only consumer of the Ctrl state), in both `UpdateModifierStateForKey`
  (`DxUi.WindowHost.cpp:5944`) and the embedded block. Do not move the block behind `KeyboardInputIsAvailable()`: that
  would skip recording the modifier while keyboard input is momentarily unavailable. Alternatively resolve synchronously in
  `DispatchKey` (owner thread) so no animation wake is needed.
- **Regression test**: `Tests/Embedded/Embedded.Tests.CleanPrepareTimerDiagnostic.h` or `Embedded.Tests.Embedded.cpp`:
  hover a `Button` with `DispatchPointer(Move)`, tick and prepare until `NeedsPreparation()` is false (the Button hover
  transition itself requests animation and dirties), then `DispatchKey(VK_CONTROL, true)` and `(…, false)`; assert
  `! view.NeedsPreparation()` and `view.Prepare(...) == S_FALSE`. Fails at `6df17007`.
- **Verifier notes**: cost is bounded by physical key edges (no steady state). The Rendering_EmbeddedD3D11 contract says
  `RequestAnimation` wakes the application through the preparation callback, so the dirty wake is the designed mechanism,
  just misapplied. Native only pays one 8 ms subscription per edge (`RequestAnimation` skips `Subscribe` when
  `_animationSubscriptionId != 0`) and no repaint.

### HOST-04 — Write the revision-4 consumer addendum for aggregate reorders and embedded behaviour changes

- **Severity** P3 (reviewer P2, verifier P3) · **Category** doc · **Verdict** Plausible (partly confirmed) ·
  **Confidence** medium · **Effort** S
- **Lead note (2 November, after `origin/main` moved to `5fc4d4c3`)**: PR #160 (documentation only; no `src`,
  `include`, `Tests` or `Tools` change) added `docs/upgrade.md`. Its "What changed in this delivery" table already tells
  consumers to avoid positional initialization, names `RichTextEditorAppearance`'s inserted fields and covers the
  `Prepare`/`E_ABORT` handling. The remaining work is to make that table complete against the list below: name
  `LayoutOptions::tabStopDip` (silent misassignment, the worst case), `MarkdownCommandRequest` and `LinkDrawing`, and the
  undocumented `TransformClipboardPaste` S_OK/raw-text change; then link the addendum from
  `Build_ToolchainAndConsumption.md` instead of duplicating it.
- **Location**: `Specs/Build/Build_ToolchainAndConsumption.md:271-290` (revision-4 migration list; the
  `ContextMenuSessionCallbacks::flowDirection` note at 286-287 is the precedent), `capabilities.json:3`
- **What**: `apiRevision` stays 4. That is an owner decision, not a defect: `Specs/Plans/Done/CodeReview_2026-10-31.md`
  lines 50 ("`apiRevision` stays 4. Do not publish an API revision 5 for this work."), 171, 261-266 and 280. Aggregate
  member insertions are not in the spec's increment list (`Build_ToolchainAndConsumption.md:223-235`; "Additions do not
  increment it"), and two of the three observable behaviour changes are already documented in `Embedded.h`. What is
  missing is the consumer migration note that revision 4 itself used for `flowDirection`. Do not bump the revision and do
  not edit the frozen `Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp`.
- **Failure scenario**: a consumer moves its pin to `6df17007` "without a change of its own" and (a) positionally
  initializes `RichText::LayoutOptions` so the heading scale silently lands in `tabStopDip`, (b) maps any non-`E_ABORT`
  `Prepare` failure to device loss and starts replacing devices on content out-of-memory, or (c) relies on `SetVisible(false)`
  to clear embedded text focus.
- **Fix**: append an "addendum" to the revision-4 consumer list in `Build_ToolchainAndConsumption.md` (after line 287)
  stating "initialize these aggregates by member name; positional initialization is unsupported" for every public
  aggregate whose member order changed in `a0f67fd3..6df17007` (verified by diff):
  1. `RichText::LayoutOptions` (`include/DxUi/RichTextLayout.h:18`): `tabStopDip` inserted between `lineHeightMultiplier`
     and `headingScaleStep` (`6d35a09d`); 8 appended (`wrap`, `codeCopyAction`, `tableOverflow`, `dpi`, `mathFontFamily`,
     `diagramLabelWidthDip`, `autoParagraphDirection`, `mathAbsoluteUnitScale`).
  2. `RichText::MarkdownCommandRequest` (`include/DxUi/RichTextCommands.h:113`): `quoteAction` and `mathDisplay` inserted
     before `headingLevel` (`1e88370b`); `tableCellCaretOffset` inserted before `text`; 3 appended (`tableAlignment`,
     `enterPolicy`, `tableInputFormat`).
  3. `RichTextEditorAppearance` (`include/DxUi/DxUi.h:3181`): 12 members inserted before `maximumTextWidthDip`
     (`directSourceInkPreview`, `showVerticalScrollbar`, `sourceMonospace`, `tableOverflow`, `sourceWrap`,
     `sourceLineNumbers`, `sourceWhitespaceMarks`, `pasteUrlOnSelection`, `zoom`, `tabStopDip`, `sourceTabSpaces`,
     `sourceTabUsesTabs`); 5 appended (`mathFontFamily`, `diagramLabelWidthDip`, `softBreakPolicy`, `enterPolicy`,
     `autoParagraphDirection`).
  4. `RichText::LinkDrawing` (`include/DxUi/RichTextLayout.h:69`): `directSourceInkPreview` inserted after `underline`
     (`ac7231b1`); `markedTextFill` and `outlineMarkedText` appended.

  State that these are not affected: `SemanticElement`, `MarkdownInline` and `TextClipboardContent` have user-provided
  constructors (not aggregates, so positional initialization never compiled); `RichTextEditorDebugState`,
  `MarkdownBlockStatistics`, `MarkdownSourceRegion` and `MarkdownInlineStatistics` only gained trailing members. All
  changed layouts require the exact-header rebuild already described at lines 237-242. Then list the three observable
  embedded behaviour changes:
  - **Logical focus survives surface loss.** `EmbeddedHost::SetVisible(false)`, a zero-size `Prepare` and `ReplaceDevice`
    no longer clear logical text focus: `ReleaseSurface` now does `++_textInputRevision; _host.DeactivateTextInput();`
    (`src/Rendering/Embedded.cpp:297-300`) and the next successful visible `Prepare` re-activates it (615-620), so no
    focus-lost callback fires on hide (`8c5fc0c8`). Already documented at `include/DxUi/Embedded.h:137-138` and in
    `Specs/Rendering/Rendering_EmbeddedD3D11.md`.
  - **`Prepare` reports an unchanged content failure with its own HRESULT** instead of `E_ABORT`
    (`src/Rendering/Embedded.cpp:566`; `E_ABORT` from content is reported as `E_FAIL`), `0a32ffc1`. `E_ABORT` is now
    reserved for a superseded frame. Already documented at `include/DxUi/Embedded.h:152-153`,
    `Specs/Rendering/Rendering_EmbeddedD3D11.md:131-132` and `docs/hosting.md` step 3. Consumers must not treat every
    non-`E_ABORT` failure as device loss.
  - **`TransformClipboardPaste` returns S_OK with the raw `content.text`** for a plain-text paste into a rich editor
    instead of S_FALSE (`src/Rendering/Embedded.cpp:1347-1351`; CR/LF spelling is preserved, the service's normalisation
    is replaced), `b5869cbc`. NOT documented in `Embedded.h:187-189`, `docs/` or `Specs/`: add one sentence there as well.

  Optionally add one line for the native routing changes consumers can observe (a capture taken during
  `WM_LBUTTONUP` is kept, the third click routes to `OnMouseTripleClick`, `WM_MBUTTONDOWN` and `WM_MBUTTONUP` can be
  handled by DxUi) and for the new `Control` virtuals (`OnMouseTripleClick`, `OnMouseMiddleDown`, `AcceptsTypedText`,
  `OnDescendantDetached`): a consumer member with the same signature now silently becomes an override.
- **Regression test**: documentation change, no red/green test. As a guard (it passes at `6df17007`) add a new, non-frozen
  fixture under `Tools/ConsumerApi/` that compiles designated-initializer cases for the four aggregates plus
  `static_assert(std::is_aggregate_v<…>)`; designated initialization requires declaration order, so a future reorder of
  existing members breaks the compile. Do not add to `ConsumerApi.Tests.Revision4.cpp` (frozen). A validator rule
  requiring a note on every aggregate reorder is not recommended.
- **Verifier notes**: the revision-4 text itself says it advanced because behaviour changed in observable ways (line 271),
  so the behaviour items are arguable under rule 2 of the increment list; the recorded owner decision overrides, hence
  documentation only. The reviewer's "no consumer notes" is partly wrong: items 1 and 2 of the behaviour list are in
  `Embedded.h`. Counts corrected from the reviewer: `LayoutOptions` appended 8 (not 7), `RichTextEditorAppearance`
  inserted 12 (not 13). Designated initialization is not broken by these insertions.

### HOST-05 — `AnimationDispatcher::OnTimerTick` is not reentrancy-safe

- **Severity** P3 (reviewer P2, verifier P3; escalate to P1 if a tick-reachable message pump is shown) ·
  **Category** concurrency · **Verdict** Plausible (mechanism certain, reachability not shown) · **Confidence** low ·
  **Effort** S · **Introduced** pre-existing (file unchanged in the window except a debug accessor, `7eb902cd`)
- **Location**: `src/Support/AnimationDispatcher.h:399-461` (range-for at 442, `_inTick = false` at 455, collect at 457,
  append at 458), `:374-397`, `:81-113`
- **What**: a nested `WM_TIMER` (`kTimerId`) reaching `WndProc` (266-269) while the outer `OnTimerTick` is inside a
  subscription callback re-enters the function: it sees `_subscriptions` non-empty, sets `_inTick = true`, re-invokes
  every callback including the one the outer pass is in, sets `_inTick = false`, then `GarbageCollect()`
  (`std::erase_if` on `_subscriptions`, 376) shifts elements under the outer loop's cached `end`, and
  `AppendPendingAdds()` (`reserve` at 387, `push_back` at 392) can reallocate the buffer the outer loop is iterating (the
  true use-after-free; plain erase leaves readable trivial slots). The outer pass then continues with `_inTick == false`,
  so a later `Subscribe`/`Unsubscribe` takes the non-deferred branches (104/108, 125-128) and mutates the vector
  directly. Separately, `Subscribe`/`AppendPendingAdds` are `noexcept` over `std::vector`; `bad_alloc` terminates
  (pre-existing, consistent with the repo's observer-callback policy).
- **Failure scenario**: a consumer `Control::Tick` (or a callback it reaches) runs a nested message loop (dialog,
  `DoDragDrop`, menu) or a UI Automation event raise that dispatches messages (`src/Controls/DxUi.Internal.h:677-681` says
  an outgoing UIA call in an STA dispatches messages) → nested tick → outer loop iterates a mutated or reallocated vector.
  No in-tree tick path was found that enters a message loop (`RunModalLoop`, `DxUi.cpp:32`, and the menu loop,
  `DxUi.Menu.cpp:6108`, run from input handlers).
- **Fix**: `if (_inTick) return;` as the first statement of `OnTimerTick` (the periodic timer fires again, so nothing is
  lost; animations of ticking hosts pause only while a nested loop entered from a tick runs). Do not iterate by index over
  a snapshot: that would still double-invoke callbacks. Optionally wrap the `push_back`/`reserve` calls in try/catch so
  `Subscribe` returns 0 on `bad_alloc`.
- **Regression test**: `Tests/Controls/DxUi.Tests.WindowHost.cpp`: a control whose `Tick` calls
  `SendMessageW(dispatcherHwnd, WM_TIMER, 1, 0)` once (add a diagnostics-only accessor for the dispatcher HWND next to
  `DebugGetActiveSubscriptionCountForTest`), then drive one real dispatcher tick; assert the control's tick count is
  exactly 1. Fails at `6df17007` deterministically (the callback runs twice) even without ASan; run it under ASan as well.
- **Verifier notes**: "new callers widen reach" is not accurate: `SetNativePresentationVisible(true)` and the Ctrl resolve
  use the same `RequestAnimation` → `Subscribe` as before, and a tick-time `Subscribe` already goes to `_pendingAdds`.
  `RefreshWindowHostAccessibilitySnapshot` (`DxUi.Accessibility.cpp:13544`) defers content-only changes to a posted flush
  and publishes synchronously only for focus transitions, which a tick does not cause.

### HOST-06 — Shared-pool device reset wakes only the failing native host

- **Severity** P3 (reviewer P2, verifier P3) · **Category** device-loss · **Verdict** Plausible (mechanism confirmed,
  impact unproven) · **Confidence** low-medium · **Effort** S · **Introduced** Present-path pattern is pre-existing;
  `recoverDeviceLoss` (stages 1-4) copies it in `1934c105`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:576-596` (`ResetSharedWindowHostGraphicsResources`, `++generation`),
  `:4300-4309`, `:4471-4477`, `:4803-4813`, `:5126-5136`, `:4210-4226` (`EnsureDeviceResources`)
- **What**: on device loss the failing host discards its resources, resets the per-thread pool (`++generation`) and
  invalidates only itself. Sibling hosts on the same thread notice the new generation only in `EnsureDeviceResources`
  when they next render, and nothing schedules that render (no `WM_DISPLAYCHANGE`, `WM_DWMCOMPOSITIONCHANGED`,
  `GetDeviceRemovedReason` or removed-event handling exists). The contract does not promise sibling recovery; the
  `NativeDevicePolicy.h` test renders the peer explicitly.
- **Failure scenario**: GPU removal or TDR with several DxUi windows on the UI thread; the window that presents first
  recovers, idle siblings may keep showing stale or black content until hover, resize or an animation tick invalidates
  them. Whether DWM retains the last frame after a real removal is the unproven part.
- **Fix**: after `ResetSharedWindowHostGraphicsResources()` in the loss paths, call a new `ControlHost` member that walks
  the attached hosts of the current thread and sets `_forceFullPresentAfterBufferReset = true` then `Invalidate()` on
  each. Lock order: `RegisterSharedWindowHostAttachment` takes the hosts mutex then the shared-resources mutex (612), so
  copy the host pointers under `GetAttachedWindowHostsMutex()` and act outside the shared lock; filter by
  `ownerThreadId == GetCurrentThreadId()`.
- **Regression test**: `Tests/Controls/Controls.Tests.NativeDevicePolicy.h`: with `peer` and `defaultOwner` on one pool,
  inject the stage-4 loss on `peer` (`DebugSetNextSizeResourceFailureForTest(4u, DXGI_ERROR_DEVICE_REMOVED)`) without the
  explicit `RenderInitialFrameForShow()` on `defaultOwner`; first `ValidateRect(defaultOwner.Hwnd(), nullptr)`, then assert
  `GetUpdateRect(defaultOwner.Hwnd(), nullptr, FALSE) != FALSE`. Both windows must be visible
  (`Invalidate` requires `IsHostWindowEffectivelyVisible`). Fails at `6df17007`.
- **Verifier notes**: a sibling recovers on any invalidate, i.e. the next interaction; rare event, hence P3.

### HOST-07 — Embedded pointer release always drops capture; native keeps a capture taken during release

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (parity) · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** native side fixed in `a61e1707`; embedded gap remains
- **Location**: `src/Rendering/Embedded.cpp:922-926`, `src/Controls/DxUi.WindowHost.cpp:3674` and `:3724`,
  `:2291`, `:2317`
- **What**: native `WM_LBUTTONUP` snapshots `_captureRequestRevision` before dispatch and releases capture only if it is
  unchanged. Embedded `PointerAction::Up` always runs `wil::scope_exit([&] { _host.ReleaseMouseCapture(); })`, dropping a
  replacement capture taken in `OnMouseUp`.
- **Failure scenario**: an embedded `Slider` whose `SetOnChange` Preview/Cancel callback calls
  `host.CaptureMouse(replacement)` during release (also inside a `ScrollPanel`) loses the replacement capture; native
  keeps it (`TestSliderReleaseThroughHostPreservesReplacementCapture`).
- **Fix**: in the `Up` case snapshot `_host._captureRequestRevision` before `OnMouseUp` and release only if unchanged,
  exactly as native; keep the release on the exception path through `CancelPointer`.
- **Regression test**: port `TestSliderReleaseThroughHostPreservesReplacementCapture`
  (`Tests/Controls/Controls.Tests.HostControlRegressions.cpp:464`) to `Tests/Embedded/Embedded.Tests.InteractionLayout.h`
  with `DispatchPointer` Down/Up; assert `view.Controls().GetCapturedControl() == replacement` after the release and
  that the slider recorded a cancel. Fails at `6df17007`.
- **Verifier notes**: the trigger requires a consumer callback that captures another control during release.

### HOST-08 — Supplemental rich-title revalidation scans all semantic elements twice per live-tooltip tick

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed (conditional) ·
  **Confidence** medium · **Effort** S · **Introduced** `e294f921`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:5778-5816`, `:5869-5887`, `src/Controls/DxUi.RichTextEditor.cpp:6755-6782`
- **What**: `ValidateSupplementalTooltipTarget` runs on each native animation tick and each embedded `AdvanceAnimation`,
  but returns at 5877 when `_supplementalTooltipControl` is null. While a tooltip target with non-empty text is live it
  calls `FindSupplementalTooltipTarget` (one `HoveredTitle`, 5815) and `SupplementalTooltipText` (5883): two geometry
  `HitTest` calls plus two linear scans of all semantic elements per tick. A visible tooltip keeps ticking for its 5 s life.
- **Failure scenario**: a very large read-only Markdown view with a hovered title, while the tooltip is up → two
  O(elements) scans per 8 ms tick. Roughly 2.5 M comparisons per second at 10^4 elements: negligible today.
- **Fix**: cache the last (point, modifiers, semantic-document identity, layout generation) → title and re-resolve only when
  one changes; optional.
- **Regression test**: embedded rich-text resources test with a debug counter on `HoveredTitle`; N idle
  `AdvanceAnimation` calls with an unchanged pointer must cost at most one call.
- **Verifier notes**: no cost when there is no live target.

### HOST-09 — Every successful embedded Prepare re-runs text-input activation for a non-text focus

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `8c5fc0c8` (added with a `SupportsTextInput()` predicate), `611cb648` (predicate removed)
- **Location**: `src/Rendering/Embedded.cpp:615-620`, `src/Controls/DxUi.NativeTextInput.cpp:540-566` and `:729-773`
- **What**: for any focused control other than `_nativeTextInputControl` (a `Button`, `Grid`, `Tree`) each successful
  `Prepare` calls `ActivateTextInput` → `ActivateNativeTextInputSession`, which bumps `_nativeTextInputSessionRevision`
  (551) and, because the control does not support text input (562), runs `DeactivateNativeTextInputSession`: another
  revision bump, `CancelNativeTextInputImeComposition`, `DeactivateNativeTextInputTsf` (transition revision bump,
  `InvalidateNativeTextStoreDeferredWork`) and the secure-cache clear. `deactivationCount` is not incremented (it only
  counts a non-null `_nativeTextInputControl`, 766-771). Functionally harmless today.
- **Failure scenario**: a `Grid` has focus in an embedded view; every dirty frame churns the text-input session and TSF
  transition revisions; a future side effect in deactivate would fire every frame.
- **Fix**: keep the activation but make it conditional on need:
  `focused != _host._nativeTextInputControl && (focused->SupportsTextInput() || _host._nativeTextInputControl != nullptr)`.
  The unconditional form was chosen deliberately in `611cb648` ("Delegate resumed text admission to guarded host"), and a
  stale `_nativeTextInputControl` pointing at a former control must still be retired when focus now rests on a non-text
  control; read that commit before changing the predicate.
- **Regression test**: embedded text-input test: focus a `Button`, prepare three dirty frames, assert the session revision
  (add a diagnostics accessor for `_nativeTextInputSessionRevision`; `deactivationCount` is unchanged today and cannot
  detect this) does not advance. Add the stale-session case: focus a `TextField`, hide, move focus to a `Button`, show and
  prepare → the stale session is retired exactly once.
- **Verifier notes**: do not assert on `deactivationCount` alone.

### HOST-10 — `requestPreparation` header lost its no-reentry restriction; hosting guide omits the native visibility API

- **Severity** P3 (reviewer P3, verifier P3) · **Category** doc · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `d04ab124`
- **Location**: `include/DxUi/Embedded.h:29`, `docs/hosting.md:38-39`, `Specs/Rendering/Rendering_EmbeddedD3D11.md:129-134`
- **What**: `d04ab124` changed the header comment from "must not re-enter the host" to "It may retire or replace
  retained controls." in the header, the spec and `docs/hosting.md` together. The spec still carries the restriction
  (line 133: the callback must not re-enter `EmbeddedHost` preparation, composition, input, device replacement or lifecycle
  methods; it may retire or replace controls through a borrowed `ControlHost` reference); the header and `docs/hosting.md`
  no longer do. `docs/hosting.md` also never mentions `ControlHost::SetNativePresentationVisible` (covered only in
  `docs/controls.md:3` and `Specs/Rendering/Rendering_Win32Host.md:7`). The "may retire" wording itself is an owner
  decision that supersedes the earlier Q1 of `Specs/Plans/Done/CodeReview_2026-10-31.md`.
- **Failure scenario**: a consumer reads the header, calls `SetVisible`, `DispatchPointer` or `Detach` from the callback.
- **Fix**: header: "Borrowed until detach; invoked synchronously on the UI thread. It may retire or replace retained
  controls through a borrowed `ControlHost`, but must not re-enter `EmbeddedHost` preparation, composition, input, device
  replacement or lifecycle methods." Same sentence in `docs/hosting.md` step 2. Add a short native-visibility paragraph to
  the "Native HWND host" section of `docs/hosting.md`.
- **Regression test**: none (documentation); the docs validator may grep the header for the restriction.
- **Verifier notes**: only `Prepare` has a reentrancy guard (`prepareCallActive`).

### HOST-11 — `WM_MBUTTONUP` is swallowed during any capture; middle press skips overlay dismissal

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S · **Introduced** `0e7d5f1d`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:3511-3525`, `src/Rendering/Embedded.cpp:888-889` and `:854-860`
- **What**: `WM_MBUTTONDOWN` goes to the captured control (or the hit target) and `handled` is whatever
  `OnMouseMiddleDown` returns (default false). `WM_MBUTTONUP` sets `handled = _capturedControl != nullptr` independent of
  whether the press was handled. `WM_MBUTTONDOWN` also bypasses `HandleTooltipInspectionPointer` and
  `DismissOverlayOnPointerDown`, which the left and right press path runs (3534, 3541); embedded does call tooltip
  inspection for middle actions.
- **Failure scenario**: during a left-drag of a `Slider` the user middle-clicks; the application's `WndProc` receives an
  unhandled `WM_MBUTTONDOWN` but never the matching `WM_MBUTTONUP`. A middle click outside an open `ComboBox` dropdown
  leaves it open.
- **Fix**: remember `_middlePressHandled` when `OnMouseMiddleDown` returns true and consume `WM_MBUTTONUP` only then
  (clear it on capture loss); route middle presses through overlay dismissal and tooltip inspection like the other buttons.
  Keep the embedded `MiddleUp` in step.
- **Regression test**: `Tests/Controls/DxUi.Tests.WindowHost.cpp`: capture a `Slider` with `WM_LBUTTONDOWN`, send
  `WM_MBUTTONDOWN` then `WM_MBUTTONUP`; assert the up is returned with `handled == false`. Fails at `6df17007`.
- **Verifier notes**: narrow trigger (a second button during a captured gesture).

### HOST-12 — Click-sequence routing differs between `CS_DBLCLKS` and plain host windows

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S · **Introduced** `19aaa7e9`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:3562-3569`, `:3638-3646`
- **What**: after a triple click the pending candidate is cleared (3638-3641). Windows delivers clicks pairwise (down,
  double-click, down, double-click). On a `CS_DBLCLKS` window the fourth click arrives as `WM_LBUTTONDBLCLK`, so
  `IsPointerDoubleClickMessage` makes it a double click even though no candidate remains; on a plain window it is a
  `WM_LBUTTONDOWN` and starts a new sequence. Before the window both produced down, double, down, double.
- **Failure scenario**: rapid quadruple click: `CS_DBLCLKS` host → `OnMouseDown`, `OnMouseDoubleClick`,
  `OnMouseTripleClick`, `OnMouseDoubleClick`; plain host → `OnMouseDown`, `OnMouseDoubleClick`, `OnMouseTripleClick`,
  `OnMouseDown`.
- **Fix**: when `tripleClick` is dispatched, record a "sequence consumed" marker (point and tick) so a `WM_LBUTTONDBLCLK`
  within the double-click interval and bounds is dispatched as `OnMouseDown` (start of a new sequence). Do not simply
  require a candidate for every `WM_LBUTTONDBLCLK`: the first press may have been unhandled and a lone double-click
  message must still count.
- **Regression test**: `Tests/Controls/DxUi.Tests.WindowHost.cpp`: record handler calls on a control. On a
  `CS_DBLCLKS` window send `WM_LBUTTONDOWN`, `WM_LBUTTONDBLCLK`, `WM_LBUTTONDOWN`, `WM_LBUTTONDBLCLK` within the
  double-click interval at one point; on a plain (`STATIC`) window send four `WM_LBUTTONDOWN`. Assert both record the
  same sequence (down, double, triple, down). Fails at `6df17007` (`...triple, double` versus `...triple, down`).
  No native triple-click test exists today.
- **Verifier notes**: trace relies on the documented Windows pairwise click message generation.

### HOST-13 — Redundant string copy on every hover tooltip update

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf (nit) · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `e294f921`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:1702`, `:5857`
- **What**: `ControlHost::SetTooltipDelayed(std::wstring text, ...)` takes the string by value and passes
  `std::wstring(text)` (a copy) to `_tooltipLayer.SetTooltipDelayed`; at `a0f67fd3` it was `std::move(text)`. The caller
  at 5857 also copies `_supplementalTooltipText`. Two heap allocations per hover update for text above the 7-unit SSO.
- **Failure scenario**: none functional; one extra allocation per mouse move over a tooltip target.
- **Fix**: restore `std::move(text)` at 1702 (`text` is not used afterwards).
- **Regression test**: none required; an allocation-counter test is optional.
- **Verifier notes**: confirmed against `git show a0f67fd3`.

### HOST-14 — Dead duplicated `Render` body under `#else` of a fixed macro

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** pre-existing; the window edited the dead copy by hand
- **Location**: `src/Controls/DxUi.WindowHost.cpp:4599-4839` (`#else` at 4601, new `presentationRevision` line at 4602),
  `include/DxUi/Configuration.h`
- **What**: `Configuration.h` defines `DXUI_ENABLE_DIAGNOSTICS 1` unconditionally ("not a consumer-selectable build
  option"), so the roughly 240-line `#else` body of `Render(const RECT*, bool)` never compiles, yet it received the
  presentation-visibility edits and can silently diverge.
- **Failure scenario**: none today; a future edit applied only to the live overload leaves a stale copy.
- **Fix**: delete the `#else` body and forward unconditionally to `Render(dirtyRectPx, nullptr, allowHidden)`; remove
  similar dead `#if ! DXUI_ENABLE_DIAGNOSTICS` blocks in the same change.
- **Regression test**: none; the build is the check.
- **Verifier notes**: deletion is safe.

### HOST-15 — `Control::SetBounds` publishes a synchronous TSF layout change before preparation

- **Severity** P3 (reviewer P3, verifier P3) · **Category** architecture · **Verdict** Plausible (design choice,
  hazard speculative) · **Confidence** low-medium · **Effort** M · **Introduced** `aa1a19bd`
- **Location**: `src/Controls/DxUi.cpp:385-390`, `src/Controls/DxUi.NativeTextInput.cpp:1134`,
  `src/Controls/DxUi.TextStoreACP.cpp:1986`
- **What**: any bounds change of the active text control calls `ControlHost::NotifyTextInputLayoutChanged` →
  `TextStoreACP::NotifyPreparedLayout`, which calls the TSF sink synchronously unless a lock is held (then it sets
  `_pendingLayoutNotification` and schedules the lock). `PublishPreparedContent` notifies again after preparation. The
  behaviour is specified: `Specs/UI/UI_InputAndAccessibility.md` (added by `aa1a19bd`) lists focused text-control bounds
  among the triggers and says locked notifications use the existing coalesced deferred path.
- **Failure scenario**: a TIP queries `GetTextExt` between the early notification and preparation of a `RichTextEditor`;
  geometry queries on unprepared layout return `TS_E_NOLAYOUT`, so at worst a redundant re-query. A TIP taking a
  synchronous write lock inside the notification and mutating the document during layout is theoretical.
- **Fix**: for controls with content preparation enabled (`RichTextEditor`), defer the notification to
  `PublishPreparedContent` and keep the immediate notification only for unprepared controls (`TextField`). This
  changes the specified behaviour, so update the `UI_InputAndAccessibility.md` sentence in the same change; otherwise
  leave as is.
- **Regression test**: native text-input suite with the layout observer store (`DebugSetTextStoreLayoutObserverForTest`):
  resize a focused `RichTextEditor` and assert the observer is notified only after the prepared frame.
- **Verifier notes**: speculative hazard; keep as an architecture note unless a TIP is shown to misbehave.

### HOST-16 — Child-window hosts get no TSF layout notification when an ancestor window moves

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S · **Introduced** `aa1a19bd`
- **Location**: `src/Controls/DxUi.WindowHost.cpp:3394-3397`
- **What**: `WM_MOVE` calls `NotifyTextInputLayoutChanged(_nativeTextInputControl)`. For a child window `WM_MOVE` is relative
  to the parent and is not sent when the top-level ancestor moves, but screen coordinates returned by `GetTextExt` change.
- **Failure scenario**: a DxUi host attached to a child HWND; the user drags the top-level window while an IME composition
  or candidate window is up → the candidate window can stay at the old screen position until the next layout change. How
  TSF/CTF behaves here is not provable from the repository.
- **Fix**: document in `docs/hosting.md` that an application hosting DxUi in a child HWND forwards a top-level move by
  calling the public `ControlHost::NotifyTextInputLayoutChanged(Control*)` with `GetFocusControl()` (it ignores a
  control that is not the active text session); optionally also add a host hook for the top-level
  `WM_WINDOWPOSCHANGED`.
- **Regression test**: native text-input suite: host attached to a child of a top-level test window, active text session
  with the layout observer store; move the parent with `SetWindowPos`; assert the observer was notified (fails at
  `6df17007`) once the documented forwarding or hook exists.
- **Verifier notes**: limitation of a new partial feature, not a regression.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| HOST-17 | `LayoutOptions::mathFontFamily = L"Cambria Math"` heap-allocates on every construction and copy and makes default construction throwing | `include/DxUi/RichTextLayout.h:28-29`: `fontFamily = L"Segoe UI"` and `codeFontFamily = L"Consolas"` (8 units each, above the 7-unit SSO) already allocate and were already there, so default construction was already potentially throwing; the one extra allocation (3 versus 2) sits beside DirectWrite layout creation at `src/Controls/DxUi.RichSemanticGeometry.cpp:1395`, `:1626`, `:2182`. Residual nit only (an empty default meaning "Cambria Math" would save one allocation); no action. |

## Public API review (`include/DxUi`, `a0f67fd3` → `6df17007`)

Consumers should read this section with HOST-04. Source-coordinated adoption applies
(`Specs/Build/Build_ToolchainAndConsumption.md:237-242`): layout and vtable changes are not ABI-promised, "source
compatible" is judged against consumer code compiled with the new headers. Corrections to the reviewer's table are
marked "(corrected)"; the table was otherwise verified against `git diff a0f67fd3 6df17007 -- include`. No public member
was removed (the removed `RichTextEditor` members `CanPasteMarkdownFileReferences()`, `RetireLayout()` and
`Erase(host, bool)` were private). `include/DxUi/ControlCatalog.h` and `include/DxUi/Configuration.h` are unchanged.

| Header | Symbol | Change kind | Source compatible? | ABI compatible? | Consumer impact |
| --- | --- | --- | --- | --- | --- |
| DxUi.h | `Control::OnMouseTripleClick`, `OnMouseMiddleDown` | virtual added (mid-vtable) | yes (name-collision risk) | no | default forwards to `OnMouseDown` / false; the third click now routes here |
| DxUi.h | `Control::AcceptsTypedText` (protected) | virtual added | yes | no | `WM_KEYDOWN` printable-swallow now uses it; default = `SupportsTextInput` |
| DxUi.h | `Panel::OnDescendantDetached`, `DescendantDetachPhase`, `NotifyContainersOfDetaching(…, phase = Detaching)` | virtual added; default argument added | yes | no | derived panels may override |
| DxUi.h | `Separator()` | `noexcept` removed | yes (traits change) | n/a | `is_nothrow_default_constructible` becomes false |
| DxUi.h | `ControlHost()` | `= default` → out-of-line | yes | layout change | no longer trivially or `noexcept` default-constructible |
| DxUi.h | `ControlHost::SetNativePresentationVisible`, `IsNativePresentationVisible` | added | yes | — | new native occlusion policy (HOST-10: absent from `docs/hosting.md`) |
| DxUi.h | `ControlHost::PreviewFileDrop`, `CommitFileDrop` | added | yes | — | file-drop routing |
| DxUi.h | `ControlHost::GetNativeTextCompositionActive`, `NotifyTextInputLayoutChanged` | added | yes | — | child hosts must forward ancestor moves (HOST-16) |
| DxUi.h | `ControlHost::ReadClipboardContent(content, bool)`, `(content, bool, bool)` | overloads added | yes | — | |
| DxUi.h | `ControlHost` debug: `DebugSetNextSizeResourceFailureForTest`, `DebugSetTextStoreLayoutObserverForTest`, `DebugSwapNativeTextInputAssociationForTest` | added | yes | — | diagnostics always compiled |
| DxUi.h | `ControlHost` private members (`_fileDropState`, `_captureRequestRevision`, presentation flags, tooltip flags, `_contentPreparationFailure`, `_debugTextStoreLayoutObserver`) | struct layout | yes | no | rebuild required (`EmbeddedHost` embeds `ControlHost` by value) |
| DxUi.h | `WindowHostCursorKind::IBeam` | enumerator appended | yes (exhaustive-switch C4061/C4062 under `/Wall`) | ordinal-stable | |
| DxUi.h | `MenuFlyoutItem`, `TextInputState`, `NativeTextInputState`, `WindowHostBitmapCapture`, `Control` string members | default member initializer changed to a sized constructor | yes | layout same | default constructor now potentially throwing |
| DxUi.h | `IGridModel` | explicit default/copy constructor and assignment declared | yes | yes | implicit move suppressed (abstract; no effect) |
| DxUi.h | `GridSelectionModel::AssignInOrder` | added | yes | — | |
| DxUi.h | `RichTextSourceMove` | class added | yes | — | |
| DxUi.h | `RichTextTaskTogglePolicy`, `RichTextLinkActivation`, `RichTextImageActivation`, `RichTextScrollInfo`, `RichTextHorizontalScrollInfo`, `RichTextDecoration*`, `RichTextAnnotationKind`, `RichTextContextMenuLabels/Command`, `RichTextPreparationStage` | types added | yes | — | |
| DxUi.h | `RichTextEditorAppearance` | 12 members inserted before `maximumTextWidthDip` (corrected from 13); 5 appended | **no for positional init** | no | HOST-04 item 3; designated initialization is unaffected |
| DxUi.h | `RichTextEditorDebugState` | members appended | yes | no | |
| DxUi.h | `RichTextEditor` | about 70 public methods added; `OnMouseTripleClick`/`OnMouseMiddleDown`/`OnContextMenu`/`ResolveCursorKind`/`AcceptsTypedText` overrides; constructor default argument `{}` → `std::wstring(0u, L' ')` | yes | no | `friend class ControlHost/EmbeddedHost` |
| DxUi.h | `TextField::ResolveCursorKind`, `AcceptsTypedText` | overrides added | yes | no | I-beam cursor over text fields |
| DxUi.h | `ScrollPanel::OnMouseTripleClick/MiddleDown/OnDescendantDetached`, `friend ControlCaptureAccess` | overrides added | yes | no | |
| DxUi.h | `Label` | 6 overrides added, members changed | yes | no | mnemonic layouts prepared before paint |
| DxUi.h | `Button`, `TabControl`, `Tree`, `Grid`, `Slider` | debug accessors, overrides and members added | yes | no | |
| DxUi.h | many classes | `#pragma warning(push/disable 4625 4626 5026 5027/pop)` wrappers | yes | yes | balanced; no leak |
| Embedded.h | `PointerAction::MiddleDown`, `MiddleUp` | enumerators appended | yes (switch warnings) | ordinal-stable | |
| Embedded.h | `EmbeddedHost::PreviewFileDrop/CommitFileDrop/ApplyClipboardFilePaste/ApplyClipboardCut/ApplyClipboardBitmapPaste` | added | yes | — | `ApplyClipboardCut` precondition and coverage: HOST-02 |
| Embedded.h | `EmbeddedCallbacks::requestPreparation` comment | doc changed | yes | — | HOST-10 |
| Embedded.h | `EmbeddedHost::SetVisible` / `Prepare` / `ReplaceDevice` behaviour | logical focus retained across surface loss; content-failure HRESULT instead of `E_ABORT` | yes (compiles) | — | behaviour change, documented in `Embedded.h:137-138` and `:152-153`; HOST-04 |
| Embedded.h | `EmbeddedHost::TransformClipboardPaste` behaviour | S_OK with raw `content.text` for plain pastes into a rich editor (was S_FALSE) | yes (compiles) | — | behaviour change, not documented in `Embedded.h`; HOST-04 |
| Embedded.h | `friend struct EmbeddedAccessibilityAccess` | added | yes | yes | |
| FileDrop.h | `CreateRichTextDragData/Source`, `RichFileDropGuard`, `RichFileDropReceiver` (consumer-implemented, noexcept virtuals), `RichFileDropAdapter` | new header | yes | — | defines `WIN32_LEAN_AND_MEAN`/`NOMINMAX` like `DxUi.h`; compiled independently by test-consumer |
| RichAccessibilityText.h | `AccessibilityTextId`, `AccessibilityTextOverride`, `AccessibilityLabels` | new header | yes | — | not in the test-consumer independent-compile list (included via `RichSemanticDocument.h`) |
| RichCodeTokenizer.h | `CodeTokenRole`, `CodeToken`, `CodeTokenizer`, `MaximumCodeTokens`, `TokenizeBuiltinCode` | new header | yes | — | not independently compiled |
| RichTextExport.h | `ExportContent`, `ExportSemanticDocument` (2), `ExportMarkdownSyntax`, `ExportDrawing` | new header | yes | — | independently compiled |
| MarkdownSyntax.h | `MarkdownExtensions::{Highlight…RichOptionalMask}` | enumerators appended (`All` still 31) | yes | stable | |
| MarkdownSyntax.h | `MarkdownDiagnosticCode`, `MarkdownInlineKind` | enumerators appended | yes (switch warnings) | stable | |
| MarkdownSyntax.h | `MarkdownInline::wiki`, `MarkdownBlockStatistics::reusedClosedRoots`, `MarkdownSourceRegion::kind/inlineKind` | members appended | yes | no | `MarkdownInline` has a user-provided constructor |
| MarkdownSyntax.h | `MarkdownInlineStatistics`, `MarkdownSourceRegionKind`, `MarkdownInlineSourceKind`, `MarkdownSyntax::InlineStatistics()` | added | yes | no (layout) | |
| RichDocument.h | `SourceLocation`, `IsValidSourceUtf16`, `SourceSnapshot::LocationAt` | added | yes | — | |
| RichDocument.h | `SourceSnapshot()` | `noexcept` added | yes | yes | implementation is `= default` on null `state_` (safe) |
| RichDocument.h | `RichDocument::CanUndo/CanRedo` (inline) | behaviour change: false during a transient preview | yes | ODR-safe (rebuilt together) | |
| RichDocument.h | `RichDocument` history storage `HistoryRing` | private layout | yes | no | |
| RichImage.h | `RichImageSourceKind::WikiTarget`, `Data` | enumerators appended | yes | stable | loaders branching "not `LocalFile` ⇒ HTTP" must switch exhaustively (opt-in sources) |
| RichImage.h | `RichImage::SvgBytes`, `RetainedBytes`, `CreateSvg`, SVG limits | added | yes | no (layout) | |
| RichMarkdown.h | `Style::Marked/Superscript/Subscript` | enumerators appended | yes | stable | |
| RichMath.h | `MathMaximumSourceUnits` 4096 → 16384, new limits, `MathIssue`, `MathExpression::issue` + constructor default parameter | value change / member added | yes | no | admission limits enlarged |
| RichSemantic.h | `SourceSearchOptions`, `FindInSource` | added | yes | — | |
| RichSemanticDocument.h | `SemanticRole` 6 enumerators appended; `SemanticDiagnosticKind` 2 appended | enumerators appended | yes | stable | |
| RichSemanticDocument.h | `SemanticElement` (`wiki`, `disclosureExpanded` after `nativeHtml`; `imageWidth`/`imageHeight` before `name`) | members inserted mid-struct | yes: not an aggregate (user-provided constructor at line 52), positional initialization never compiled (corrected from "positional-init risk") | no | layout only |
| RichSemanticDocument.h | `SemanticHeading`, `Headings()`, `ResolveHeadingFragment`, `SourceEditingRange`, `IsSourceEditingElement`, `AccessibilityLabelSet`, `HasCallouts/HasPropertiesBody`, `VisitSemanticWords`, `CountWords`, `BuildHeadingTableOfContents`, `SoftBreakPolicy`, `SemanticBuilder::SetAccessibilityLabels/SetSoftBreakPolicy` | added | yes | no | |
| RichTextCommands.h | `MarkdownCommand` 19 enumerators appended | appended | yes | stable | |
| RichTextCommands.h | `MarkdownCommandRequest` (`quoteAction`, `mathDisplay` before `headingLevel`; `tableCellCaretOffset` before `text`; 3 appended) | members inserted | **no for positional init** | no | HOST-04 item 2 |
| RichTextCommands.h | `MarkdownQuoteAction`, `MarkdownEnterPolicy`, `MarkdownTableInputFormat`, `MarkdownCommandState`, `IsCommandFeatureEnabled`, `Find*`, `Can*`, `CanJoinSelection`, `FindJoinLeaf` | added | yes | — | |
| RichTextLayout.h | `LayoutOptions::tabStopDip` (mid-struct) + 8 appended incl. `std::wstring mathFontFamily` (corrected from 7) | members inserted | **silent misassignment for positional init** | no | HOST-04 item 1 |
| RichTextLayout.h | `LinkDrawing::directSourceInkPreview` (mid), `markedTextFill`, `outlineMarkedText` | members added | **no for positional init** | no | HOST-04 item 4 |
| RichTextLayout.h | `LayoutWorkBudget::maximumUploadBytes`, `LayoutAdvance::uploadedBytes`, `PreparedDocumentLayout::Width()` | added | yes | no | |
| RichTextLayout.h | `PreparedLayout` private `PrepareSemanticOwned`/`SemanticInlineObject` removed, friend changed | private | yes | no | |
| TextInputServices.h | `HtmlTextClipboardWriter`, `CopyExportToClipboard`, `SelectiveStructuredTextClipboard`, `BitmapStructuredTextClipboard`, `TextClipboardHtmlPaste`, `TextClipboardFilePaste`, `TextClipboardSemanticCut`, `TextClipboardBitmapPaste` | new optional consumer interfaces (cross-cast discovery) | yes | — | existing vtables unchanged |
| TextInputServices.h | `TextClipboardContent` (`htmlFragment`, `hasHtmlFragment`, `bitmapDib`, `bitmapV5`, `MaximumBitmapBytes`, `Swap`) | members appended | yes | no | has a user-provided constructor |
| Configuration.h / ControlCatalog.h | — | unchanged | — | — | `ControlKind` ordinals unchanged; `DXUI_ENABLE_DIAGNOSTICS` fixed at 1, so the `#if` debug members create no ODR or layout skew |

No internal (`src/`) header leaks into `include/` (`Support/*.h` includes remain src-only). Each new header is
self-contained by inspection; `RichAccessibilityText.h` and `RichCodeTokenizer.h` are not in `test-consumer.ps1`'s
independent-compile list (only transitively compiled). `Samples/SdkSmoke/Main.cpp` only checks
`SetNativePresentationVisible(false) == E_UNEXPECTED` on a detached host; it does not compile-check designated
initialization of the reordered aggregates (see HOST-04 guard fixture).

## Area architecture and simplification proposals

1. **Single click-sequence state machine** (`src/Controls/DxUi.WindowHost.cpp:2338-2414` and `:3536-3663`,
   `src/Rendering/Embedded.cpp:890-920`). Native and embedded duplicate double and triple detection with different inputs
   (pixels versus DIPs, message kind versus action). A shared `PointerClickSequence` helper returning
   `{Down, Double, Triple}` plus the capture-release policy would remove the HOST-07 and HOST-12 divergence class.
   Risk low, effort M, after production (fix HOST-07 and HOST-12 locally first).
2. **Capture-release parity helper**: move the `_captureRequestRevision` snapshot-and-compare into one `ControlHost`
   method used by both hosts' pointer-up paths. Files: `DxUi.WindowHost.cpp:3674,3724`, `Embedded.cpp:922-926`. Risk low,
   effort S, can be done with HOST-07 itself.
3. **Tooltip resolution cache**: `SupplementalTooltipText`, `FindSupplementalTooltipTarget`,
   `ValidateSupplementalTooltipTarget`, `UpdateModifierStateForKey` and the embedded Ctrl block form an implicit state
   machine in five places. A small `SupplementalTooltipResolver` keyed by (point, modifiers, interaction revision, layout
   generation) would make the HOST-01 cleanup explicit. The verifier found HOST-03 solved by target-type gating and
   HOST-08 a nit, so the refactor is not justified by them. Risk low-medium, effort M, after production.
4. **Narrow access struct instead of `friend class RichTextEditor` on `ControlHost`** (`include/DxUi/DxUi.h:6396`). The host
   already has about fifteen friends; prefer an access struct like `ControlModelQueryAccess`/`ControlCaptureAccess`.
   Risk low, effort S, after production.
5. **`std::wstring(0u, L' ')` / `std::vector<T>(0u)` default-member pattern** in public structs (`MenuFlyoutItem`,
   `TextInputState`, `NativeTextInputState`, `WindowHostBitmapCapture`, `RichTextEditorAppearance`, `ExportContent`, …)
   exists to let Debug iterator-proxy allocation fail as `bad_alloc` instead of terminating in noexcept STL constructors,
   but only one site explains it. Add a single `DxUi::Detail::EmptyString()` helper or a short comment at each site. Risk
   none, effort S, after production.
6. **Delete dead `#if ! DXUI_ENABLE_DIAGNOSTICS` paths** repository-wide (HOST-14) since `Configuration.h` fixes the macro.
   Risk low, effort S, with HOST-14.

## Test-quality observations

- `TestWindowHostApplicationVisibilitySuspendsNativePresentation` exercises the reentrant visibility toggle through
  `DebugAnimationTickForTest`, which calls `OnAnimationTick` directly; the real dispatcher's `_inTick`, `_pendingAdds` and
  `pendingRemove` path is never run, and nothing exercises a nested tick (HOST-05).
- `Controls.Tests.NativeDevicePolicy.h` proves stage 1-4 injected recovery but renders the peer explicitly
  (`RenderInitialFrameForShow`, "the shared peer follows each replaced device generation"), which masks HOST-06.
  Injection is simulated; no real removed device.
- `TestWindowHostThrowingMouseLeaveStillClearsTooltip` covers throw and root replacement, not a geometry-only
  interaction-revision change (HOST-01).
- `TestSliderReleaseThroughHostPreservesReplacementCapture` is native-only and uses an unattached host
  (`HandleMessage(nullptr, …)`), so OS capture state is not asserted; there is no embedded counterpart (HOST-07).
- No native host test for `WM_MBUTTONDOWN`/`WM_MBUTTONUP` routing (HOST-11) or for triple-click routing and
  `CS_DBLCLKS` sequences (HOST-12); triple click is only covered in embedded (`TestRichTripleClickParagraph`) and through
  `TextField`/`ComboBox` internals.
- `EmbeddedHost::ApplyClipboardCut` has no test of any kind, including the happy path (HOST-02). No test covers Ctrl-key
  clean-surface behaviour (HOST-03).
- `Embedded.Tests.EmbeddedTextInput.h` focus-retention coverage (hide, zero size, device replacement, stale revision
  rejection, explicit `CancelTextInput` clearing focus) is good and asserts real revisions. It does not cover the
  non-text-focus churn of HOST-09, and `deactivationCount` cannot detect it.
- `TestPostedPayloadTypeIdentitySurvivesIdenticalIcfDeleters` meaningfully requires deleter folding in Release and checks
  Take/Reclaim/Drain destroy-once semantics.
- `Samples/SdkSmoke/Main.cpp` does not compile-check the reordered aggregates; `Tools/ConsumerApi/ConsumerApi.Tests.Revision4.cpp`
  is frozen and therefore cannot (HOST-04 proposes a new non-frozen fixture).

## Questions for the developer

1. Is `apiRevision` 4 intended to absorb the embedded focus-retention, `Prepare` HRESULT and `TransformClipboardPaste`
   semantics changes and the aggregate reorders? Recommended answer: yes, and add the revision-4 addendum of HOST-04.
   (false premise — owner already decided; see `Specs/Plans/Done/CodeReview_2026-10-31.md:50,171,261-266,280`. The open item
   is only whether to write the addendum.)
2. HOST-01: was the interaction-revision early return in `WM_MOUSELEAVE` meant to cover layout changes, or only tree
   replacement as the spec text says? Recommended answer: tree replacement only; always finish host-owned tooltip cleanup.
3. HOST-06: after a real TDR with several native hosts on one thread, have siblings repainted on their own (DWM retaining
   content), or is an explicit sibling wake expected? Recommended answer: do not rely on DWM retention; wake siblings.
4. HOST-03: should the Ctrl rich-title resolve be limited to editable `RichTextEditor` targets (the only consumer of
   `_supplementalTooltipModifiers`)? Recommended answer: yes.
5. Is `EmbeddedHost::ApplyClipboardCut` intended to be called only through `TextInputServices`? Recommended answer: yes
   (`TextInputServices.h:137-138`, `docs/rich-text.md:549-552`), but add the S_FALSE guard and documentation anyway
   (HOST-02).
6. `SetTheme` now always forces full visual preparation (`src/Controls/DxUi.WindowHost.cpp:1564-1565`: `_forceAllVisualPreparation`,
   `_pendingVisualPreparation`) even for an identical palette; do consumers call it on every `WM_SETTINGCHANGE` or
   `WM_THEMECHANGED` burst? Recommended answer: ask consumers; if so, short-circuit an identical palette. (Not checked by
   the verifier.)

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.WindowHost.cpp` | diff (875 lines) + touched functions read in full: ~ControlHost/Attach/Detach, SetTheme, tooltip functions (SetTooltipDelayed, ClearTooltip, InspectTooltip, HandleTooltipInspectionPointer), Invalidate, SetNativePresentationVisible/IsNative…, PrimeForShow, RenderInitialFrameForShow, RequestAnimation, GetAnimationTickMs, CaptureMouse/ReleaseMouseCapture, double-click helpers, PreviewFileDrop/CommitFileDropCore, GetTextFormat (hunk), ReadClipboardContent, DebugSimulateDeviceLoss/DebugSetNextSizeResourceFailureForTest, IsInitialRootChildBatchActive (hunk), OnDpiChanged (hunk), HandleMessage cases WM_PAINT/ERASEBKGND/SIZE/SHOWWINDOW/DESTROY/MOVE/DPICHANGED/SETFOCUS/KILLFOCUS/MOUSEMOVE/SETCURSOR/MOUSELEAVE/MBUTTON*/L/RBUTTON*/MOUSEWHEEL/CAPTURECHANGED tail/CANCELMODE/NCDESTROY, shared graphics registry, EnsureDeviceResources, EnsureSizeDependentResources, PrepareForSwapChainResize, DiscardSize/DeviceResources, TrimCaches, RecreateBrushCache, Render (both overloads, Present/device-loss tail), CaptureCurrentBackBuffer, OnSize, UpdateHover (hunk), SupplementalTooltipText, FindSupplementalTooltipTarget, UpdateSupplementalTooltipTarget, ValidateSupplementalTooltipTarget, InvalidateRichTitleTooltip, UpdateModifierStateForKey (hunk), OnAnimationTick. Not read: accessibility, TSF and menu helpers and keyboard dispatch bodies outside the hunks. |
| `src/Rendering/Embedded.cpp` | reviewed fully from line 125 to end (State, Attach, Detach, ReplaceDevice, MarkDirty, RequestPreparation, ReleaseSurface, SetVisible, AdvanceAnimation, Prepare, PrepareInteraction, Composite, pointer/key/char dispatch, receipts, file drop, ReadTextInput, ApplyClipboard*, TransformClipboardPaste, ApplyTextInput, HitTest/RangeBounds, CancelTextInput, GetStatistics); lines 1-124 (`GraphicsDevice::Create`, receipt `Reset`) unchanged and skimmed only |
| `include/DxUi/Embedded.h` | diff reviewed fully |
| `src/Controls/DxUi.cpp` | diff + touched functions (SetBounds, OnMouseTripleClick/MiddleDown defaults, SetMnemonic, accessible setters, InvalidateMeasure/RequestIntrinsicLayout, PropagateHost, OnFlowDirectionChanged/OnDensityChanged, AcceptsTypedText) |
| `src/Controls/DxUi.Internal.h` | diff + `ControlCaptureAccess`/`ReleaseUnchangedCaptureOnExit` and their callers in `DxUi.Controls.cpp` (2531-2560, 7015-7060) |
| `src/Support/PostedPayload.h` | reviewed fully, plus all call sites (grep) |
| `src/Support/AnimationDispatcher.h` | reviewed fully |
| `src/DxUi.vcxproj` | diff reviewed (5 new translation units) |
| `include/DxUi/DxUi.h` | diff reviewed fully (1824 diff lines) |
| `include/DxUi/FileDrop.h`, `RichAccessibilityText.h`, `RichCodeTokenizer.h`, `RichTextExport.h` | reviewed fully (new) |
| `include/DxUi/MarkdownSyntax.h`, `RichDocument.h`, `RichImage.h`, `RichMarkdown.h`, `RichMath.h`, `RichSemantic.h`, `RichSemanticDocument.h`, `RichTextCommands.h`, `RichTextLayout.h`, `TextInputServices.h` | diff reviewed fully |
| `include/DxUi/Configuration.h`, `ControlCatalog.h` | verified unchanged in the window; `Configuration.h` read |
| `src/Controls/DxUi.NativeTextInput.cpp` (ActivateTextInput, session activate/deactivate, NotifyTextInputLayoutChanged), `DxUi.TextStoreACP.cpp` (NotifyPreparedLayout), `TextInputServices.cpp` (clipboard command 805-950), `DxUi.Controls.cpp` (Panel::TakeChild/NotifyContainersOfDetaching, ScrollPanel detach hooks, PreparePendingPageLayout failure path, PublishPreparedContent), `DxUi.RichTextEditor.cpp` (Erase, OnMouseMiddleDown, HoveredTitle) | callees read only for the touched paths (outside HOST scope) |
| `Tests/Controls/DxUi.Tests.WindowHost.cpp` | diff of new tests reviewed (visibility ×2, ICF payload, throwing mouse leave); pragma-only hunks skimmed |
| `Tests/Embedded/Embedded.Tests.Embedded.cpp` | diff skimmed (new Tree reorder retirement test, separator allocation sweep, runner wiring) |
| `Tests/Embedded/Embedded.Tests.InteractionLayout.h` | added assertions skimmed (tab header preparation, no paint allocations) |
| `Tests/Embedded/Embedded.Tests.ComplexUiBoundaries.h` | diff skimmed (benchmark scenario plumbing only) |
| `Tests/Embedded/Embedded.Tests.CleanPrepareTimerDiagnostic.h` | diff reviewed fully |
| `Tests/Controls/DxUi.Tests.Rendering.cpp` | diff reviewed (pragma wrappers only) |
| `Tests/Controls/Controls.Tests.HostControlRegressions.cpp` | diff reviewed fully |
| `Tests/Controls/Controls.Tests.NativeDevicePolicy.h` | reviewed fully |
| `Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h` | focus-retention hunk (`8c5fc0c8`) reviewed |
| `Samples/SdkSmoke/Main.cpp` | reviewed fully |
| `Specs/Rendering/Rendering_Win32Host.md`, `Rendering_EmbeddedD3D11.md` | read fully |
| `Specs/Build/Build_ToolchainAndConsumption.md` | compatibility section (200-329) read |
| `Specs/Core/Core_Architecture.md`, `Core_PerformanceAndResources.md`, `docs/hosting.md` | grep-level only (visibility and reentrancy references); not read in full |
| `.agents/skills` win32-host, embedded-rendering, consumer-integration, modern-cpp-wil | read |
| Prior reviews (`CodeReview_2026-10-31`, `CodeReview_2026-10-28`, `DeliveryReview_2026-10-31`) | not used by the reviewer; the verifier used `CodeReview_2026-10-31.md` only for the apiRevision owner decision and the superseded Q1 |
