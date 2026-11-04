# UI Automation providers: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the UI Automation providers
(`src/Controls/DxUi.Accessibility.cpp`, `src/Controls/DxUi.RichAccessibility.h`, `src/Controls/DxUi.RichAnnotations.h`,
`src/Controls/DxUi.RichEditorAccessibilityAccess.h`, `src/Controls/DxUi.RichAccessibilityText.cpp`,
`include/DxUi/RichAccessibilityText.h`, `src/Controls/DxUi.RichAccessibilityEpoch.h`), the accessibility-relevant parts
of `src/Controls/DxUi.WindowHost.cpp` and `src/Rendering/Embedded.cpp`, and their tests.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| A11Y-01 | P2 | Confirmed | The new RadioButtons Group element has no Name; the visible header is never exposed | `src/Controls/DxUi.Accessibility.cpp:4018` | S |
| A11Y-02 | P2 | Plausible | Semantic Expand/Collapse/Toggle actions raise no state-change event and retire the acted peer | `src/Controls/DxUi.RichAccessibility.h:758` | M |
| A11Y-03 | P3 | Confirmed | Native hosts drop the state event of an unfocused prepared-tree action when the app publishes asynchronously | `src/Controls/DxUi.Accessibility.cpp:12553` | S |
| A11Y-04 | P3 | Refuted as stated; hardening residue | Owner-thread text-range methods catch only `bad_alloc`; a throwing consumer `GetTextInputRangeRects` override terminates | `src/Controls/DxUi.Accessibility.cpp:6940` | S |
| A11Y-05 | P3 | Confirmed | Per-call whole-text and line-table copies make TextPattern navigation O(n) per call | `src/Controls/DxUi.Accessibility.cpp:8184` | M |
| A11Y-06 | P3 | Plausible | `dynamic_cast` on client-supplied provider pointers inside `noexcept` methods | `src/Controls/DxUi.Accessibility.cpp:10012` | S |
| A11Y-07 | P3 | Confirmed | Stale-versus-OOM distinction from `afe4f90a` applied at only some peer-creation sites | `src/Controls/DxUi.Accessibility.cpp:7853` | S |
| A11Y-08 | P3 | Confirmed | Embedded `Grid.GetItem` can cancel pointer capture and run `OnCaptureLost` through `InputIsCoherent` | `src/Controls/DxUi.Accessibility.cpp:11408` | S |
| A11Y-09 | P3 | Confirmed | Collapsed RadioButtons root: non-radio children unreachable, FirstChild and LastChild disagree | `src/Controls/DxUi.Accessibility.cpp:3763` | S |
| A11Y-10 | P3 | Confirmed | Embedded tree-expansion hints never expire while a declined item stays visible | `src/Controls/DxUi.Accessibility.cpp:13331` | S |
| A11Y-11 | P3 | Confirmed | `RangeFromAnnotation` reports a stale own annotation as `E_INVALIDARG`; `AnnotationTarget` leaves `VT_UNKNOWN` on failure | `src/Controls/DxUi.Accessibility.cpp:10015` | S |
| A11Y-12 | P3 | Confirmed | The accessibility contract contradicts itself on link Invoke | `Specs/UI/UI_InputAndAccessibility.md:227` | S |
| A11Y-13 | P3 | Confirmed | Caret and conversion text events still resolve the owner with a full live-tree search; a test pins it | `src/Controls/DxUi.Accessibility.cpp:13406` | S |
| A11Y-14 | P3 | Confirmed | Fixed 256-slot stack arrays in `AnnotationAttribute` depend on a limit enforced in another file | `src/Controls/DxUi.RichAnnotations.h:417` | S |
| A11Y-15 | P3 | Confirmed | No regression tests for the text-range allocation-failure containment added in the window | `src/Controls/DxUi.Accessibility.cpp:7146` | M |
| A11Y-16 | P3 | Confirmed | `MakeEmptyAccessibilitySnapshot` can throw `bad_alloc` through `noexcept` callers | `src/Controls/DxUi.Accessibility.cpp:1250` | S |

Severity counts: P0 0, P1 0, P2 2, P3 14. Verdicts: Confirmed 13, Plausible 2, Refuted 1 (A11Y-04 as stated; its
hardening residue is kept at P3), Duplicate 0.

## P2 findings

### A11Y-01 — The new RadioButtons Group element has no Name; the visible header is never exposed

- **Severity** P2 (reviewer P1, verifier P2, lead P2 with fix-before-production) · **Category** a11y · **Verdict**
  Confirmed · **Confidence** high · **Effort** S · **Introduced** `1f661c7e` (RadioButtons became a semantic Group)
- **Location**: `src/Controls/DxUi.Accessibility.cpp:4018-4075` (`GetControlAccessibleName`),
  `src/Controls/DxUi.Accessibility.cpp:2243` (record), `src/Controls/DxUi.Accessibility.cpp:9002` (Name property),
  `src/Controls/DxUi.Accessibility.cpp:1601` (semantic), `src/Controls/DxUi.Accessibility.cpp:5103` (Group type),
  `src/Controls/DxUi.Controls.cpp:5045-5049` (`RadioButtons::SetHeader`), `src/Controls/DxUi.Controls.cpp:5085-5092`
  (`Paint`), `include/DxUi/DxUi.h:2429-2451`
- **What**: `RadioButtons` is a UIA Group with Selection, and its choices are its fragment children. The snapshot stores
  `record.controlAccessibleName = GetControlAccessibleName(root, current)`, and `UIA_NamePropertyId` returns it. For a
  `RadioButtons`, that function returns `GetAccessibleName()` if set, otherwise `FindAssociatedLabelText` (a Label
  whose mnemonic target is the group). `SetHeader` stores the private `_header`, which only `Paint` reads.
  `SetHeader` calls `RequestInvalidate()` only and never republishes accessibility, unlike
  `Control::SetAccessibleName` (`src/Controls/DxUi.cpp:811-825`). `UIA_LabeledByPropertyId` (`:9007`) exposes only an
  explicit `SetAccessibleLabelledBy` label and does not supply Name.
- **Failure scenario**: `group->SetHeader(L"Paper size"); group->AddItem(L"A4"); group->AddItem(L"Letter");` →
  Inspect shows the Group with `Name=""` and `IsControlElement=true`. Narrator entering the group reads "A4, radio
  button, 1 of 2" without the question "Paper size".
- **Fix**:
  - In `GetControlAccessibleName`, after the explicit-name check and before the `FindAssociatedLabelText` fallback,
    add a `dynamic_cast<const RadioButtons*>` branch that returns the header when it is non-empty, otherwise the
    mnemonic label. Precedence: explicit name, then header, then mnemonic label.
  - Read the header through internal friend access (`ControlModelQueryAccess` in `src/Controls/DxUi.Internal.h`) or
    through a new public `[[nodiscard]] std::wstring_view GetHeader() const noexcept`.
  - Change `RadioButtons::SetHeader` to return early when the text is unchanged. Otherwise it stores the text,
    calls `RequestInvalidate()`, checks the lifetime token, and calls
    `RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host)`, exactly like `Control::SetAccessibleName`.
  - The existing `nameChanged` diff (`src/Controls/DxUi.Accessibility.cpp:14178`, raised in the block starting at
    `:14229`) then raises `UIA_NamePropertyId` natively, and queues it for embedded hosts. An embedded host
    republishes on its next `Prepare`/`UpdateAccessibility`.
  - Document the Name source in the RadioButtons entry of `docs/controls.md` and at
    `Specs/UI/UI_InputAndAccessibility.md:667`.
- **Regression test**: in `Tests/Controls/DxUi.Tests.Accessibility.cpp`, next to
  `TestAccessibilityRadioButtonsAreSelectionItems` (`:10328`):
  1. Call `group->SetHeader(L"Taille")` before `SetRoot`.
  2. Obtain the group through a choice's `ISelectionItemProvider::get_SelectionContainer`.
  3. `Require(ReadProviderStringProperty(*container, UIA_NamePropertyId, "radio group name") == L"Taille")`. This
     returns `L""` at `6df17007`.
  4. Start a `UiaTest::Client` (`Tests/Support/Support.Tests.UiaTestClient.h`) and call `group->SetHeader(L"Format")`.
  5. `WaitUntil` a property event with `id == UIA_NamePropertyId`, `controlType == UIA_GroupControlTypeId` and name
     `Format`. Use the same pattern as the tree-item Name test at `Tests/Controls/DxUi.Tests.Accessibility.cpp:10072`.
     No event arrives today.

  Add the embedded twin to `TestEmbeddedRadioGroupAccessibility`
  (`Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h:389`), with `Prepare` + `UpdateAccessibility` between steps.
- **Verifier notes**: This is not a regression. At `a0f67fd3`, `RadioButtons` was not a semantic element, so the header
  was never exposed. Before this fix, `group->SetAccessibleName(...)` works around it.
- **Lead note**: kept at P2, but it ships with the new RadioButtons Group element, so it must be fixed before
  production even though it is not a regression.

### A11Y-02 — Semantic Expand/Collapse/Toggle actions raise no state-change event and retire the acted peer

- **Severity** P2 (reviewer P2, verifier P2) · **Category** a11y · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M · **Introduced** `9acd69e9` (semantic disclosure), plus the inline-source and task actions added in the
  window
- **Location**: `src/Controls/DxUi.RichAccessibility.h:758-797` (`ChangeInlineSource`),
  `src/Controls/DxUi.RichAccessibility.h:484-516` (`Toggle`), `src/Controls/DxUi.RichTextEditor.cpp:2203-2228`
  (`SetSemanticDisclosure`), `src/Controls/DxUi.Accessibility.cpp:14176-14177` (rich diff),
  `src/Controls/DxUi.RichAccessibility.h:746-751` (`Matches`)
- **What**:
  - Expand/Collapse on a `Disclosure` calls `SetSemanticDisclosure`. That clears the semantic document
    (`_state->semantic.Clear()`) and calls `SourcePresentationChanged`. Expand/Collapse on an inline-source Link or
    Image calls `Begin/EndSemanticSourceEdit`, and Toggle on a task edits source.
  - Each action causes a new semantic publication. The diff treats a rich change only as `structureChanged = true`
    (ChildrenInvalidated on the window root).
  - No `UIA_ExpandCollapseExpandCollapseStatePropertyId` or `UIA_ToggleToggleStatePropertyId` change is raised from any
    semantic peer. A task toggle also gets `UIA_Text_TextChangedEventId` from the editor
    (`src/Controls/DxUi.Accessibility.cpp:14295-14296`).
  - The acted peer then fails `Matches` and answers `UIA_E_ELEMENTNOTAVAILABLE`.
  - `SemanticDisclosure` (`Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:2613`, asserts at `:2644-2648`)
    only checks for a fresh action provider.
  - This follows the current contract: `Specs/UI/UI_InputAndAccessibility.md:107-108` says "Presentation transitions
    retire old fragments immediately". The Button-disclosure rule at `:655-656` covers Button controls only. However,
    UIA lists the ExpandCollapseState and ToggleState property-changed events as required for providers that expose
    those patterns, so the spec has a gap.
- **Failure scenario**: in a Markdown view, a screen-reader user runs the primary action on the "More" summary. The
  document expands, but no state event arrives. The client's cached element is dead, so it says nothing or loses its
  place.
- **Fix**:
  1. Amend `Specs/UI/UI_InputAndAccessibility.md:107` to require a state event for successful semantic actions.
  2. Disclosure: after the owner publishes, map the acted element to its successor by `blockIdentity`, which
     `SetSemanticDisclosure` requires (`src/Controls/DxUi.RichTextEditor.cpp:2209`). Raise
     `UIA_ExpandCollapseExpandCollapseStatePropertyId` from a successor event peer, modelled on
     `RaiseTreeItemExpansionChange` (`src/Controls/DxUi.Accessibility.cpp:13140-13161`). Embedded hosts must use
     `QueueEmbeddedPropertyChange`.
  3. Task toggles and inline sources have no stable identity after a source edit. Raise
     `UiaRaiseNotificationEvent(NotificationKind_ActionCompleted, ...)` with a localized
     "expanded"/"collapsed"/"checked"/"unchecked" string from the editor element. Embedded hosts deliver it through
     the queued path, never by a direct raise on the owner thread. The existing notification raise at
     `src/Controls/DxUi.Accessibility.cpp:14633` shows the call shape.
- **Regression test**: in `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp`, `SemanticDisclosure` (native
  and embedded):
  - Subscribe a `UiaTest::Client` to property and notification events, call `expansion->Expand(); fixture.Publish();`,
    and `WaitUntil` an ExpandCollapseState=Expanded event named "More".
  - Match by Name, not runtime id, because the successor has a new runtime id.
  - Add a task twin in `SemanticStructureAndTasks` that expects a ToggleState change or the notification.
  - At `6df17007` only StructureChanged (plus TextChanged for tasks) arrives, so both time out.
- **Verifier notes**: The code facts are certain. Whether Narrator or NVDA stay silent on StructureChanged alone was not
  verified with physical AT, hence Plausible.

## P3 findings

### A11Y-03 — Native hosts drop the state event of an unfocused prepared-tree action when the app publishes asynchronously

- **Severity** P3 (reviewer P2, verifier P3) · **Category** a11y (native/embedded parity) · **Verdict** Confirmed ·
  **Confidence** high (code) · **Effort** S · **Introduced** `1f661c7e`
- **Location**: `src/Controls/DxUi.Accessibility.cpp:12552-12580` (hint insertion, embedded only),
  `src/Controls/DxUi.Accessibility.cpp:12610-12634` (native immediate before/after compare),
  `src/Controls/DxUi.Accessibility.cpp:13292-13351` (`RaisePreparedTreeExpansionChanges`; hints processed only under
  `if (target.embedded)` at `:13320`), `Specs/UI/UI_InputAndAccessibility.md:674-679`
- **What**:
  - A native unfocused Expand/Collapse is reported only if the delegate publishes new prepared rows synchronously
    inside `RequestExpandedState`. The owner-thread `CaptureSnapshot` flushes (`:1492-1493`), so the after
    snapshot differs from the before snapshot.
  - A later publication goes through `RaisePreparedTreeExpansionChanges`. That function considers only the focused
    item before and after, plus the embedded hints, so the native action's event is lost.
  - The spec at `:674-675` is not limited to synchronous publication. `:678` describes hints for embedded hosts only.
- **Failure scenario**: a native tree with a prepared-row source whose `OnTreeToggleExpanded` posts a lazy load and
  returns. A UIA client calls Expand on an unfocused folder, and rows publish 50 ms later → only StructureChanged; no
  ExpandCollapseState event. The same tree in an embedded host hears it.
- **Fix**:
  - Insert the bounded hint for native targets too, before `RequestExpandedState`.
  - Process hints in `RaisePreparedTreeExpansionChanges` without the `target.embedded` gate. The native drain
    (`DrainNativeAccessibilityPublication`, `:14350`) runs that diff. `RaiseTreeItemExpansionChange` already
    branches embedded/native.
  - Delete the native immediate block at `:12610-12634` so the event is not raised twice.
  - Pitfalls:
    - Native actions gain the `ERROR_BUSY` refusals (full buffer, conflicting request). Document them.
    - The non-expiry of A11Y-10 then also affects native targets, so land A11Y-10's TTL with this fix.
    - The rebind/removal retirement at `:14108-14119` is also embedded-only and needs the same widening.
- **Regression test**: in `Tests/Controls/DxUi.Tests.Accessibility.cpp`, next to the unfocused case (`:10266-10312`):
  1. Set `delegate.toggle` to a lambda that does nothing, so the delegate accepts without publishing.
  2. Call `expand->Expand()`, then publish `unfocusedRows(true)` and call `NotifyDataChanged()` from the test.
  3. `WaitUntil` the "Volets" Expanded property event. This times out at `6df17007`.
- **Verifier notes**:
  - Requires a native host, a UIA action on an unfocused item, and application-asynchronous publication together.
    StructureChanged is still raised.
  - Actions on the focused item (Narrator scan and keyboard) are covered by the focused-candidate logic.
  - The RedSalamander delegate's asynchronous shape was not verified.

### A11Y-04 — Owner-thread text-range methods catch only `bad_alloc`; a throwing consumer `GetTextInputRangeRects` override terminates

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug (COM boundary hardening) · **Verdict** Refuted as stated
  (see Refuted table); residual hardening Confirmed · **Confidence** high · **Effort** S · **Introduced** `b5869cbc`
  (function-try-blocks catching only `bad_alloc`; switch from `GetSourceVisualLineRanges` to `SourceLines`); the
  consumer-override exposure predates the window
- **Location**: function-try-blocks at `src/Controls/DxUi.Accessibility.cpp:6467` (`CompareEndpoints`), `:6617`
  (`ExpandToEnclosingUnit`), `:6940` (`GetBoundingRectangles`), `:7102` (`GetText`), `:7146` (`Move`), `:7305`
  (`MoveEndpointByUnit`). The dispatched containment is `ContainAccessibilityUiAction`, `:5718-5735`. Extension point:
  `include/DxUi/DxUi.h:1784`.
- **What**:
  - The dispatched (MTA to owner) path runs through `ContainAccessibilityUiAction`, which maps `std::exception` to
    `E_FAIL`.
  - The owner-thread path catches only `bad_alloc`, inside `noexcept` methods, so any other exception calls
    `std::terminate`. Embedded providers always take the owner path (`:7867-7871`).
  - The visual-line helper the reviewer cited cannot throw anything but `bad_alloc` (see Refuted).
  - The realistic source is a consumer subclass of a non-`final` text control (`TextField`, `ComboBox` or
    `RichTextEditor`) whose override of the non-`noexcept` virtual `GetTextInputRangeRects` throws. It is reached from
    `GetBoundingRectangles` through `TryResolveTextRangeCaretRects` (`:4235-4332`).
- **Failure scenario**: an embedded host contains `class MyField : public DxUi::TextField` whose
  `GetTextInputRangeRects` uses `THROW_IF_FAILED` on a DirectWrite call, with Arabic text (so
  `IsSimpleTextRangeCaretGeometrySupported`, `:4147-4151`, routes to range rects at `:4261-4268`). A
  `GetBoundingRectangles` call on the owner thread terminates the process. The same call dispatched from an MTA
  native client returns `E_FAIL`.
- **Fix**: add `catch (const std::exception&) { Debug::Error(L"..."); return E_FAIL; }` after each `bad_alloc` handler
  in the six function-try-blocks, matching `ContainAccessibilityUiAction`. Outputs are already nulled at entry, and
  range commits happen after the fallible work. Keep the `bad_alloc` handler first so `E_OUTOFMEMORY` survives.
  Converting inside `EmbeddedAccessibilityAccess::SourceLines` alone would not cover the rectangle path.
- **Regression test**: in `Tests/Controls/DxUi.Tests.Accessibility.cpp`:
  1. Use a test-local `TextField` subclass whose `GetTextInputRangeRects` throws `std::runtime_error`, holding Arabic
     text.
  2. Call `GetBoundingRectangles` on a range of it from the window thread.
  3. `Require(hr == E_FAIL && rectangles == nullptr)`. At `6df17007` the test process terminates.
- **Verifier notes**: This sits next to the open TSF item R28-16 (`Specs/Plans/Done/CodeReview_2026-10-28.md:230`),
  which proposes catching inside `Control::TryGetTextInputRangeRects`. Keep the `bad_alloc` rethrow there, as
  `RichTextEditor` does (`src/Controls/DxUi.RichTextEditor.cpp:4487-4497`).
- **Lead note**: The claim as stated (geometry throws reach `std::terminate` from the visual-line helper) is refuted.
  Only this P3 hardening item survives.

### A11Y-05 — Per-call whole-text and line-table copies make TextPattern navigation O(n) per call

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf · **Verdict** Confirmed (code); magnitude unmeasured ·
  **Confidence** high (code), low (magnitude) · **Effort** M · **Introduced** design predates the window; `79447470`
  added one more line-table copy per Line call in Release
- **Location**: `src/Controls/DxUi.Accessibility.cpp:8184-8201` (`ResolveText` returns `std::wstring` by value),
  `:6498` (`CompareEndpoints` uses it only for `.size()`), `:7120-7122` (`GetText` copies the whole text to return a
  slice), `:4721-4728` and `src/Controls/DxUi.RichTextEditor.cpp:4308` (line-table copies), `:6916`
  (`GetAttributeValue` scans `SourceFormats()` from run 0), `:6557` (`SourceFormatIndex`)
- **What**:
  - Every TextPattern call copies the whole source.
  - Each Line call copies the line table three times: span to vector in `SourceVisualLineRangesFallible`, vector to
    `spans`, then the `std::in_place` copy. `79447470` turned `return spans;`, which was a move, into a copy.
  - The `std::in_place, rectangles.value()` copies (`:4254`, `:4266`, `:4280`, `:4331`) are O(range), not O(document).
  - `GetAttributeValue` and `FindAttribute` start their scan at run 0 instead of at `SourceFormatIndex(record, start)`.
- **Failure scenario**: NVDA "say all" over a 1M-unit Markdown view issues about 10 TextPattern calls per line. Each
  call copies about 2 MB, so each step costs milliseconds. The total over a full read grows quadratically. "Stalls of
  seconds per screen" is not supported without a receipt.
- **Fix**:
  1. Add a borrowed `ResolveTextView()` returning `{std::shared_ptr<const AccessibilitySnapshot>, std::wstring_view}`
     for read-only callers.
  2. Start the attribute loops at `SourceFormatIndex`.
  3. Gate the copy-instead-of-move workaround on `_ITERATOR_DEBUG_LEVEL != 0`. The MSVC debug vector move allocates
     a container proxy.
  4. Optionally have the rich snapshot retain a `shared_ptr` to the prepared semantic geometry state and expose its
     `SourceLines()` span. Do not copy the table into each publication, and do not build tables for `TextField`
     (O(n) caret queries per publish).
- **Regression test**: add an allocation receipt under `Measurements/`. The global `operator new` counter exists only
  in the Embedded binary (`Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h:20-58`), so drive an embedded
  `RichTextEditor`. Compare bytes allocated per `Move(TextUnit_Line)`, `GetText` and `CompareEndpoints` at 64K and 1M
  units, and assert the ratio stays below 2. Today it scales with document size.
- **Verifier notes**: Raise to P2 if a Release receipt at the supported maximum document size shows per-step latency
  over budget.

### A11Y-06 — `dynamic_cast` on client-supplied provider pointers inside `noexcept` methods

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug (COM boundary hardening) · **Verdict** Plausible ·
  **Confidence** low (reachability) · **Effort** S · **Introduced** `e0deab0e` (`RangeFromAnnotation`); other sites
  predate the window
- **Location**: `src/Controls/DxUi.Accessibility.cpp:10012` (`RangeFromAnnotation`), `:6454` (`Compare`, no try),
  `:6487` (`CompareEndpoints`, `bad_alloc` only), `:7448` (`MoveEndpointByRange`), `:9740` (`RangeFromChild`)
- **What**:
  - For a foreign object built with RTTI, `dynamic_cast` safely returns null, which maps to `E_INVALIDARG`.
  - For a foreign object built `/GR-`, `__RTDynamicCast` reads `vfptr[-1]` as a complete-object locator. It either
    returns null or throws `std::__non_rtti_object`, a `std::bad_typeid`. Escaping `noexcept`, that calls
    `std::terminate`.
  - UIA passes server-side methods only provider objects that live in this process. A foreign pointer therefore needs
    a second in-process server-side provider framework built without RTTI (for example a hosted RichEdit or a XAML
    island) and a client that mixes ranges or elements across providers.
- **Failure scenario**: an app hosts DxUi beside an in-process `/GR-` provider. A client calls
  `IUIAutomationTextRange::Compare(dxuiRange, otherRange)` or `RangeFromAnnotation(otherElement)` on the DxUi
  document, and the host aborts.
- **Fix**: define a private `IDxUiAccessibilityPeer` IID with a method that returns a tagged identity. Use
  `QueryInterface` instead of `dynamic_cast` at all five sites, and map `E_NOINTERFACE` to `E_INVALIDARG`. This also
  enables A11Y-11's mine-but-stale distinction.
- **Regression test**: in `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:DecorationAnnotations`, pass a
  test-local COM object compiled in a `/GR-` translation unit to `RangeFromAnnotation` and `Compare`, and require
  `E_INVALIDARG`. Note that a WRL or RTTI object already returns `E_INVALIDARG` today, so only the `/GR-` object
  demonstrates the defect; at `6df17007` the test process may abort.
- **Verifier notes**: Whether RedXe or RedSalamander host such a provider is unverified. Windows Terminal
  `static_cast`s in `Compare`; Chromium uses QI with a private IID for this hazard.

### A11Y-07 — Stale-versus-OOM distinction from `afe4f90a` applied at only some peer-creation sites

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api (contract) · **Verdict** Confirmed · **Confidence**
  medium · **Effort** S · **Introduced** sites predate `afe4f90a`, which fixed only four call sites
- **Location**: `src/Controls/DxUi.Accessibility.cpp:7853-7855` (`GetChildren`, `HasCurrentRichSemantic()` heuristic),
  `:9164-9173` (root `Navigate`: `S_OK` with null on failure), `:9507-9508` (`ElementProviderFromPoint`: same),
  `src/Controls/DxUi.RichAccessibility.h:401-402` (OOM becomes `UIA_E_ELEMENTNOTAVAILABLE`),
  `src/Controls/DxUi.RichAnnotations.h:150-169` (`Navigate`: `S_OK` with null), `src/Controls/DxUi.RichAnnotations.h:422-424`
  (heuristic). Contract: `Specs/UI/UI_InputAndAccessibility.md:464-465`.
- **What**: the contract says a retired source publication reports `UIA_E_ELEMENTNOTAVAILABLE` and an actual peer
  allocation failure reports `E_OUTOFMEMORY`. At the sites above:
  - OOM is reported as "no child", "nothing at point" or "unavailable".
  - `GetChildren` reports `E_OUTOFMEMORY` when `Create` fails for a non-epoch reason. An example is
    `GuardedElementSnapshot` rejecting a moved owner whose semantic epoch is still current.
- **Failure scenario**: under memory pressure, a client walking `FirstChild` sees an empty document. Separately, a
  native off-thread `GetChildren` races a publication that shifts the editor's control path. The range's own
  `CaptureSnapshot` (`:7822-7824`) passes, peer creation fails, and the call returns `E_OUTOFMEMORY`.
- **Fix**: change `RichSemanticElementProvider::Create` and `RichAnnotationProvider::Create` to return
  `{wil::com_ptr_nothrow<...>, HRESULT}`, or thread `HRESULT* failure` through every call. Return that HRESULT at every
  site, and remove the `HasCurrentRichSemantic()` heuristics.
- **Regression test**: in `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp`, extend
  `TestRichSemanticStaleCellCreation` (`:3204`):
  1. Arm `DebugSetAccessibilityPeerCreationGateForTest`.
  2. Call `range->GetChildren` on a worker thread.
  3. While creation is held, shift the editor's control path without retiring its semantic epoch: call `TakeChild` and
     `CompactChildren` on a sibling that precedes the editor (add one to the fixture), then `Publish()`.
  4. Release the gate and require `UIA_E_ELEMENTNOTAVAILABLE`. It is `E_OUTOFMEMORY` at `6df17007`.

  Hiding the editor before the call, without the gate, does not reproduce: the range's initial snapshot check
  already returns unavailable. Add a `Navigate(FirstChild)` twin.
- **Verifier notes**: Hiding may not retire the epoch (`RetireLayout`, `src/Controls/DxUi.RichTextEditor.cpp:3276-3312`,
  does not call `RetireSemanticAccessibility`). A path shift certainly does not.

### A11Y-08 — Embedded `Grid.GetItem` can cancel pointer capture and run `OnCaptureLost` through `InputIsCoherent`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** architecture · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `6c6f0c94` (`Coherent`), `603c8517` (`InputIsCoherent(true)`)
- **Location**: `src/Controls/DxUi.Accessibility.cpp:54-59` (`EmbeddedAccessibilityAccess::Coherent`), `:11408` (first
  call), `:8593-8609` (`GetItem` cache miss on the owner thread), `src/Rendering/Embedded.cpp:767-776`
  (`InputIsCoherent`), `src/Rendering/Embedded.cpp:746-765` (`CancelPointer`)
- **What**:
  - `InputIsCoherent` is not a predicate. When `preparedInteractionRevision != _interactionRevision`, it sets
    `coherent = false` and calls `CancelPointer()`, which calls `captured->OnCaptureLost(_host)` (control or
    application code) and `PruneStaleInteractionState()`.
  - A UIA `GetItem` for an uncached row can therefore cancel an in-progress drag and run callbacks inside a query.
  - `Prepare` and the layout acknowledgement deliberately keep a drag whose captured control continues
    (`CapturedDragContinues`, `src/Rendering/Embedded.cpp:480`, `:672`), so the query cancels drags that the frame
    boundary would have kept.
  - Only the first call at `:11408` can cancel. The second call at `:11458` is always coherent for revision changes:
    `resolveLiveGrid` fails first, because `BorrowedControlModelGuard::IsCurrent` checks `GeometryRevision`, which is
    `host->_interactionRevision` (`src/Controls/DxUi.Internal.h:146-150`, `:172-176`).
- **Failure scenario**: in an embedded view, the user drags a splitter. The drag bumps the interaction revision through
  sibling `SetBounds` (`src/Controls/DxUi.cpp:368`). Before the next `Prepare`, a client calls
  `IGridProvider::GetItem(1503, 0)` for an uncached row → the drag is cancelled and `OnCaptureLost` runs inside the
  UIA call.
- **Fix**:
  - Add a side-effect-free `bool EmbeddedHost::GeometryIsCoherent() const noexcept` that returns true only when
    `_state` exists, is visible and coherent, and `preparedInteractionRevision == _interactionRevision`. It needs friend
    access for `EmbeddedAccessibilityAccess` or an internal declaration.
  - Use it in `EmbeddedAccessibilityAccess::Coherent`. The `:11458` check may stay as cheap redundancy.
- **Regression test**: in `Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h`, next to the offscreen
  `Grid.GetItem` cases (`:196-214`):
  1. Capture a Slider with pointer Down.
  2. Call `SetBounds` on a sibling so the revision changes.
  3. Call `GetItem(1503, 0)`, and accept `UIA_E_ELEMENTNOTAVAILABLE` as the result.
  4. Check the slider is still captured and that a test `OnCaptureLost` counter is unchanged.

  At `6df17007` the capture is cancelled.
- **Verifier notes**: "the next pointer event would cancel it too" holds only when the app dispatches input before
  `Prepare`.

### A11Y-09 — Collapsed RadioButtons root: non-radio children unreachable, FirstChild and LastChild disagree

- **Severity** P3 (reviewer P3, verifier P3) · **Category** a11y · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `c7144a1a`
- **Location**: `src/Controls/DxUi.Accessibility.cpp:3761-3772` (collapsed `FirstChild` = `semanticControlOrder[1]`),
  `:3793-3797` (collapsed `LastChild` = last radio), `:3859-3870` (sibling navigation), `:3838-3840` (Parent),
  `:2261-2275` (only `RadioButton` children are grouped)
- **What**:
  - `RadioButtons` is a `Panel`, so `AddChild<Label>()` is public. In a collapsed RadioButtons root, `FirstChild` is
    the first semantic descendant, while `LastChild` is the group's last choice.
  - `NextSibling` of a non-radio child skips radio-parented entries, and radios stop at the group's end.
  - The non-radio child's Parent is Root, so the element is reachable upward but not downward. UIA drops events on
    elements it cannot reach.
- **Failure scenario**: with a root RadioButtons whose children are `Label("note")`, then `RadioButton("A")`: the
  `FirstChild`/`NextSibling` walk yields `[note]`, while the `LastChild`/`PreviousSibling` walk yields `[A]`. If the
  Label comes after the radios, both walks yield `[A]` and the Label is unreachable.
- **Fix**: when collapsed with a `radioGroup`, compute FirstChild, LastChild and siblings from one ordered list: the
  radio `childPaths`, then the non-radio semantic descendants. Or collapse only when every semantic descendant is a
  grouped choice, which is the smaller change.
- **Regression test**: in `Tests/Controls/DxUi.Tests.Accessibility.cpp`, next to
  `TestAccessibilityRadioButtonsAsCollapsedRoot` (`:10434`):
  1. Build the collapsed root with `AddChild<Label>(L"note")` first, then `AddItem(L"A")`.
  2. Require that both walks visit the same set of elements.
  3. Require that every semantic descendant whose Parent is the root is reached from the root.

  This fails at `6df17007`. A Label added after the item would not fail the walk comparison.

### A11Y-10 — Embedded tree-expansion hints never expire while a declined item stays visible

- **Severity** P3 (reviewer P3, verifier P3) · **Category** architecture · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `1f661c7e`
- **Location**: `src/Controls/DxUi.Accessibility.cpp:1133-1140` (`std::array<PendingTreeExpansion, 128>` in every
  target), `:12553-12580` (insertion and `ERROR_BUSY`), `:13320-13348` (retirement when the requested state is
  published or the item leaves), `:14108-14119` (rebind/removal retirement),
  `Specs/UI/UI_InputAndAccessibility.md:678-679`
- **What**:
  - A hint is removed only when the item publishes the requested state, leaves the snapshot, or the tree is rebound
    or removed. This matches the documented contract.
  - If the delegate accepts a request but never applies it, the hint stays while the item stays visible.
  - After 128 such items, every state-changing Expand/Collapse on that target returns
    `HRESULT_FROM_WIN32(ERROR_BUSY)`.
  - Each target embeds about 5 KiB of hint storage, including native window and popup targets, which never use it.
  - Correction to the original report: an opposite request on a declined item is not `ERROR_BUSY`. It has
    `stateChanged == false` (`:12548`, `:12564`), and is busy only transiently when the live model already shows the
    hinted state before the diff runs.
- **Failure scenario**: an embedded file tree whose lazy load fails silently for unreachable folders. After 128
  distinct accepted-but-unapplied Expand requests on visible items, further UIA Expand/Collapse calls on that tree
  fail with `ERROR_BUSY`.
- **Fix**:
  - Bound each hint's lifetime by a publication-count or time TTL, after which the hint is dropped without an event.
  - Do **not** retire a hint just because a publication shows the item's state unchanged: under the asynchronous
    contract, the first publication after an accepted request normally still shows the old state.
  - Optionally allocate the hint block lazily via `std::unique_ptr`. If A11Y-03 extends hints to native targets,
    native targets will use it too.
- **Regression test**: in `Tests/Embedded/Embedded.Tests.PreparedTreeAccessibilitySemantics.h`, use a delegate that
  never publishes:
  1. Issue Expand on 128 distinct visible items, then require the 129th to return `ERROR_BUSY` (current, documented
     behaviour).
  2. Advance past the TTL (N publications) and require the next Expand to succeed. That fails at `6df17007`.

### A11Y-11 — `RangeFromAnnotation` reports a stale own annotation as `E_INVALIDARG`; `AnnotationTarget` leaves `VT_UNKNOWN` on failure

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `e0deab0e`
- **Location**: `src/Controls/DxUi.Accessibility.cpp:10003-10019`, `src/Controls/DxUi.RichAnnotations.h:297-311`
  (`Matches`/`BelongsTo`), `src/Controls/DxUi.RichAnnotations.h:132` (`AnnotationTarget`),
  `src/Controls/DxUi.RichAnnotations.h:286-295` (`get_Target`)
- **What**:
  - `BelongsTo` fails both for a foreign peer and for this editor's own peer after a retirement, and both return
    `E_INVALIDARG`. Every other stale-peer path returns `UIA_E_ELEMENTNOTAVAILABLE`.
  - `GetPropertyValue(UIA_AnnotationTargetPropertyId)` sets `vt = VT_UNKNOWN` before `get_Target`. On failure,
    `get_Target` nulls the pointer, so the call returns a typed VARIANT with a null `punkVal` and a failure HRESULT.
    That is harmless (`VariantClear` tolerates null) but untidy.
- **Failure scenario**: a client holds an annotation element. The app changes decorations, which retires annotation
  peers (`Specs/UI/UI_InputAndAccessibility.md:64-66`). `RangeFromAnnotation(retained)` then returns `E_INVALIDARG`,
  which tells the client the argument is wrong, instead of "element gone".
- **Fix**: if the peer has the same target, hwnd and path but `!Matches`, return `UIA_E_ELEMENTNOTAVAILABLE`. That needs
  a private accessor, or A11Y-06's identity interface. In `GetPropertyValue`, set `vt` only after `get_Target`
  succeeds.
- **Regression test**: in `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:DecorationAnnotations`:
  1. Retain an annotation peer.
  2. Call `SetDecorations` with changed metadata, then `Publish()`.
  3. Call `text2->RangeFromAnnotation(retained, &range)` and expect `UIA_E_ELEMENTNOTAVAILABLE`. It is `E_INVALIDARG`
     at `6df17007`.

### A11Y-12 — The accessibility contract contradicts itself on link Invoke

- **Severity** P3 (reviewer P3, verifier P3) · **Category** doc · **Verdict** Confirmed (spec); help-text half refuted ·
  **Confidence** high · **Effort** S · **Introduced** `2449ab2b` (conditional Invoke added at the top of the spec
  without updating the semantic section)
- **Location**: `Specs/UI/UI_InputAndAccessibility.md:3-4` versus `:223` ("links use inert Hyperlink") and `:227`
  ("Links advertise no Invoke/navigation action")
- **What**: lines 3-4 say local-fragment links expose Invoke and external links expose it while an activation handler
  is registered. The implementation matches lines 3-4 (`src/Controls/DxUi.RichAccessibility.h:223-225`); lines 223 and
  227 still describe inert links.
- **Failure scenario**: a consumer or reviewer following `:227` treats link Invoke as a bug, or a test oracle written
  from the spec asserts no Invoke.
- **Fix**: rewrite `:223` and `:227` to the conditional-Invoke rule of `:3-4`.
  - Do **not** restore "Invoke or press Esc" in the `ExitSource` help (`src/Controls/DxUi.RichAccessibilityText.cpp:12`).
  - Commit `30178f4d` removed it deliberately: inline-source Link/Image elements expose no Invoke
    (`src/Controls/DxUi.RichAccessibility.h:221-225`), and a test pins the help text
    (`Tests/Controls/Controls.Tests.RichTextAccessibility.cpp`, "inline source exit help does not suggest an
    unavailable Invoke action").
  - If block source leaves, which do expose Invoke (`src/Controls/DxUi.RichAccessibility.h:571`), should mention it,
    add a separate label per kind.
- **Regression test**: none (documentation). The `validate-specs` lint could flag "advertise no Invoke" in the semantic
  section.

### A11Y-13 — Caret and conversion text events still resolve the owner with a full live-tree search; a test pins it

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** left unindexed by `136897a0`
- **Location**: `src/Controls/DxUi.Accessibility.cpp:13400-13408`, `src/Controls/DxUi.Accessibility.cpp:12864-12880`
  (`FindWindowHostControlPath`), `Tests/Controls/DxUi.Tests.Accessibility.cpp:4341`
- **What**:
  - `ActiveTextPositionChanged` (every caret move) and `TextEditConversionTargetChanged` use
    `FindAccessibilityPathForTarget` over the whole live tree, which is O(controls).
  - `FindWindowHostControlPath` accepts an index hit only after confirming it on the live tree
    (`ResolveControlAtPath == control`, visible, semantic). Otherwise it falls back to the same search, and a control
    has a single path.
  - So the indexed lookup yields the same path and the same refusals. Neither the commit nor its fragment gives a
    reason for excluding the queued kinds.
  - The test asserts `>= 256u` visits, which pins the slow path.
- **Failure scenario**: in a wide window (hundreds of controls), every caret move while a UIA client listens walks the
  whole tree.
- **Fix**: for the queued kinds too, call `FindWindowHostControlPath` with the published snapshot
  (`target->snapshot.load(std::memory_order_acquire).get()`), `host->GetRoot()`, `control` and `path`.
- **Regression test**: change the assertion at `Tests/Controls/DxUi.Tests.Accessibility.cpp:4341` to `< 128u` for the
  queued conversion event, and add the same check for `ActiveTextPositionChanged`. Both fail at `6df17007`.

### A11Y-14 — Fixed 256-slot stack arrays in `AnnotationAttribute` depend on a limit enforced in another file

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (hardening) · **Verdict** Confirmed · **Confidence**
  high · **Effort** S · **Introduced** `e0deab0e`
- **Location**: `src/Controls/DxUi.RichAnnotations.h:417-428` (`owners[count] = ...; ++count;` with no bound check),
  `src/Controls/DxUi.RichTextEditor.cpp:1597-1598` (`decorations.size() > 256u` throws),
  `src/Controls/DxUi.Accessibility.cpp:2381-2400` (annotations come only from decorations)
- **What**: the array is safe today: at most 256 decorations means at most 256 overlapping annotations. Raising the
  decoration limit would make a client query write past a stack array.
- **Failure scenario**: none today. After a future limit change, `GetAttributeValue(UIA_AnnotationObjectsAttributeId)`
  over a character covered by more than 256 annotations corrupts the stack.
- **Fix**: introduce one shared `constexpr size_t kMaxRichDecorations = 256u`, used by `SetDecorations` and to size
  `owners` and `raw`, with a `static_assert`. Also add
  `if (count == owners.size()) { result->vt = VT_UNKNOWN; return UiaGetReservedMixedAttributeValue(&result->punkVal); }`.
- **Regression test**: boundary test with 256 fully overlapping annotations on one character, requiring 256 annotation
  objects. It passes today and guards the boundary; the `static_assert` is the real protection.

### A11Y-15 — No regression tests for the text-range allocation-failure containment added in the window

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test-gap · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M · **Introduced** `b5869cbc`, `79447470`, `3c40c85b`, `f7ed0578` (no test changes)
- **Location**: catch clauses at `src/Controls/DxUi.Accessibility.cpp:6507` (`CompareEndpoints`), `:6717`
  (`ExpandToEnclosingUnit`), `:7050` (`GetBoundingRectangles`), `:7140` (`GetText`), `:7299` (`Move`), `:7432`
  (`MoveEndpointByUnit`), `:7861` (`GetChildren`), and `GetSelection` (`:10350`)
- **What**: no accessibility test injects `bad_alloc` into a text-range call. The only OOM assertion
  (`Tests/Controls/DxUi.Tests.Accessibility.cpp:816`) has an application handler throw `bad_alloc` from `Toggle`. The
  claims "returns `E_OUTOFMEMORY`" and "leaves the range unchanged" are unverified. For example,
  `GetBoundingRectangles` could regress to a non-null partial SAFEARRAY, or `Move` could commit `_rangeStart` before
  the throw.
- **Failure scenario**: a later refactor commits range state before a fallible allocation, and no test notices.
- **Fix**: do not add a new countdown hook. Reuse the existing `failAllocationAfter` sweep of the Embedded test binary
  (`Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h:29-41`). It already drives many `*AllocationFailure*` tests,
  and that binary has embedded text-range fixtures (`Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h`,
  `Tests/Embedded/Embedded.Tests.RichSemanticResources.h`). The Controls binary has no such hook.
- **Regression test**: for each method above, on an embedded rich editor range:
  1. Sweep `failAllocationAfter = 0, 1, 2, ...` until the call succeeds.
  2. At each failing step, require `E_OUTOFMEMORY`, null or zero outputs, and that `GetText(-1)` afterwards returns the
     pre-call range text.

### A11Y-16 — `MakeEmptyAccessibilitySnapshot` can throw `bad_alloc` through `noexcept` callers

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S · **Introduced** predates the window (`f28f55d0`)
- **Location**: `src/Controls/DxUi.Accessibility.cpp:1250-1258` (`std::make_shared<AccessibilitySnapshot>()`), `:1501`
  (`CaptureAccessibilitySnapshot`, not `noexcept`). Its `noexcept` callers are `GuardedElementSnapshot` (`:8078-8084`),
  `CaptureProviderCreationSnapshot` (`:5895-5898`), `AccessibilityProvider::CaptureSnapshot` (`:8145`) and
  `PublishEmptyAccessibilitySnapshot` (`:1267-1270`, called at `:13512` and `:13688`).
- **What**: the allocation runs while `target->snapshot` is null (before the first publication) and on every empty
  publication. A `bad_alloc` there calls `std::terminate`.
- **Failure scenario**: under memory pressure, the first `WM_GETOBJECT`/provider query before the first publication,
  or a detach that publishes the empty snapshot, aborts the process.
- **Fix**: make `MakeEmptyAccessibilitySnapshot` `noexcept`, catching `bad_alloc` and returning `nullptr`. Callers
  already treat a null snapshot as unavailable (`UIA_E_ELEMENTNOTAVAILABLE`). `PublishEmptyAccessibilitySnapshot`
  should then store `nullptr`, which readers handle. Do not use a process-wide static empty snapshot: it cannot carry
  the per-target `hwnd`/`windowThreadId` that some paths read.
- **Regression test**: in the Embedded binary (allocation injection exists only there; see A11Y-15), arm
  `failAllocationAfter` before the first `UpdateAccessibility` and call the root's `get_FragmentRoot` or
  `GetPropertyValue`. Expect `UIA_E_ELEMENTNOTAVAILABLE`. At `6df17007` the process terminates.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| A11Y-04 | `79447470` narrowed the visual-line helper's containment, so geometry `logic_error`/`out_of_range` (for example "Semantic geometry is not current" after a reentrant retirement, or device loss) reaches `std::terminate` from `Move`/`MoveEndpointByUnit`/`ExpandToEnclosingUnit` | `EmbeddedAccessibilityAccess::SourceLines` returns early unless `LayoutIsCurrent` (`src/Controls/DxUi.Accessibility.cpp:42`); `RequireCurrentLayout` re-tests the same predicate with no intervening code; `SemanticGeometry()` is non-null only when `semantic_.Ready()` (`src/Controls/DxUi.RichEditorLayout.h:224`), the same check `Current()` throws on (`src/Controls/DxUi.RichSemanticGeometry.cpp:2909-2910`); the v0 `out_of_range` throws (`src/Controls/DxUi.RichDocumentLayout.cpp:540-576`, `:650-656`) need an internal invariant bug; device loss changes `resourceGeneration`, so `LayoutIsCurrent` falls back to logical lines; the switch was `b5869cbc`, not `79447470`. Residual hardening kept as A11Y-04 above. |
| A11Y-08 (sub-claim) | The second `Coherent()` call at `:11458` can also cancel capture between model getters | `BorrowedControlModelGuard::IsCurrent` checks `host->_interactionRevision` (`src/Controls/DxUi.Internal.h:146-150`), so `resolveLiveGrid` (`src/Controls/DxUi.Accessibility.cpp:11415`) fails first; only `:11408` can cancel |
| A11Y-10 (sub-claim) | A later opposite request on a declined item is `ERROR_BUSY` | The opposite request has `stateChanged == false` and skips the hint branch (`src/Controls/DxUi.Accessibility.cpp:12548`, `:12564`) |
| A11Y-12 (sub-claim) | `ExitSource` help must again say "Invoke or press Esc" | Inline-source elements expose no Invoke (`src/Controls/DxUi.RichAccessibility.h:221-225`); `30178f4d` added a test asserting the help omits Invoke |

## Area architecture and simplification proposals

1. **Borrowed text view instead of per-call copies.**
   - Proposal: replace `std::wstring ResolveText()` with
     `struct TextView { std::shared_ptr<const AccessibilitySnapshot> owner; std::wstring_view text; }` for read-only
     callers (`CompareEndpoints`, `GetText`, `ClampCurrentRange`, `FindText`).
   - Rationale: A11Y-05; snapshots are immutable. Keep the full `expectedText` compare for non-rich fields, which have
     no semantic revision.
   - Files: `src/Controls/DxUi.Accessibility.cpp` (about 14 call sites). Risk low, effort M. After production, unless a
     receipt raises A11Y-05.
2. **Retain the prepared line table instead of rebuilding it.**
   - Proposal: the rich snapshot holds a `shared_ptr` to the prepared semantic geometry state and exposes its
     `SourceLines()` span. Visual-line moves become binary searches, and the off-thread Line case no longer needs
     dispatch.
   - Do not copy the table into each publication: publications follow caret and selection changes while clients
     listen. Do not build tables for `TextField`.
   - Files: `src/Controls/DxUi.Accessibility.cpp:4715-4802`, `src/Controls/DxUi.RichSemanticGeometry.cpp:3200`. Risk
     medium (lifetime of geometry state), effort M. After production.
3. **One factory result type for semantic and annotation peers.**
   - Proposal: `Create` returns `{com_ptr, HRESULT}`.
   - Rationale: removes the `HasCurrentRichSemantic()` heuristics and the mix of `S_OK`, unavailable and OOM (A11Y-07).
   - Files: `src/Controls/DxUi.RichAccessibility.h`, `src/Controls/DxUi.RichAnnotations.h`, their callers. Risk low,
     effort S. Before production, together with A11Y-07.
4. **Private identity IID instead of `dynamic_cast`.**
   - Proposal: an `IDxUiAccessibilityPeer` interface returning a tagged identity.
   - Rationale: A11Y-06 hardening, and it distinguishes "mine but stale" from "not mine" (A11Y-11).
   - Files: the five `dynamic_cast` sites. Risk low, effort S. After production; reachability is low.
5. **Debug-only workaround for allocating `noexcept` moves.**
   - Proposal: centralize the `std::in_place` copy pattern in a helper such as
     `Detail::FallibleOptional(std::vector<T>&&)`, which moves when `_ITERATOR_DEBUG_LEVEL == 0` and copies otherwise.
   - Rationale: removes Release-time copies (A11Y-05) and code that reads like a bug.
   - Files: `src/Controls/DxUi.Accessibility.cpp`, `src/Controls/DxUi.RichTextEditor.cpp`. Risk low, effort S. After
     production.
6. **One radio-group model.**
   - Proposal: build a `RadioGroupSnapshot` carrying the group Name (from the header) and one ordered child list used
     for collapsed-root FirstChild, LastChild and siblings.
   - Rationale: A11Y-01 and A11Y-09 share the root cause.
   - Files: `src/Controls/DxUi.Accessibility.cpp:2261-2295`, `:3660-3870`. Risk low, effort S. Before production
     (A11Y-01).
7. **Side-effect-free coherence query and bounded hint lifetime.**
   - Proposal: `EmbeddedHost::GeometryIsCoherent() const noexcept` for accessibility (A11Y-08), and TTL-bounded hints
     (A11Y-10), optionally allocated lazily.
   - Files: `src/Rendering/Embedded.cpp:767`, `src/Controls/DxUi.Accessibility.cpp:54`, `:1133-1140`. Risk low,
     effort S. The coherence query before production; hint storage before production only if A11Y-03 extends hints to
     native targets.
8. **Annotation snapshot build complexity.**
   - The build is O(boundaries × annotations), at most 512 × 256, plus one `TryGetTextInputRangeRects` per annotation
     on every publish (`src/Controls/DxUi.Accessibility.cpp:2381-2427`). Acceptable at current limits.
   - If the limit grows, use a sweep over sorted boundaries with a running mask. After production.

## Test-quality observations

- **Grid coherence test**: "model callback invalidation aborts an in-flight uncached Grid.GetItem"
  (`Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h:209-214`) never reaches the post-callback `Coherent()` check
  at `src/Controls/DxUi.Accessibility.cpp:11458`. `resolveLiveGrid` aborts first, because
  `BorrowedControlModelGuard::IsCurrent` checks `host->_interactionRevision`. Mutating a sibling's bounds or visibility
  would be caught the same way (`src/Controls/DxUi.cpp:368`, `:432`). To exercise `:11458`, the getter must change the
  embedded host's `visible`/`coherent` state without bumping `_interactionRevision`. Otherwise treat `:11458` as
  redundant.
- **Scrolling precondition deleted rather than isolated** (`d1207e4c`): the assertion that the UIA scrolling fixture
  uses positioned geometry chunks is gone, so `ScrollingCommands` can stop covering chunked geometry without failing.
  Keep the oracle chunk-independent, and add a separate fixture assertion or a second document size that forces
  chunked geometry.
- **Oracle pins an O(controls) scan**: `Tests/Controls/DxUi.Tests.Accessibility.cpp:4341` requires `>= 256u` visits
  for a queued text event (A11Y-13).
- **Queue attempts counted as deliveries**: `NotifyPropertyEventRaisedForTest` runs unconditionally after
  `QueueEmbeddedPropertyChange`, which returns `void` (`src/Controls/DxUi.Accessibility.cpp:13156-13161`).
  `TestPreparedTreeUnfocusedExpansionAction`'s exact `changes == 1u` therefore counts queue attempts. Keep at least one
  exact-count case on the real client path, as `Tests/Embedded/Embedded.Tests.PreparedTreeEvents.h` does.
- **Missing negative and boundary tests**:
  - `ERROR_BUSY` (full hint buffer or conflicting request).
  - `RangeFromAnnotation` with a stale or foreign peer.
  - Annotation peers walked through `Navigate`: root FirstChild and LastChild with only annotations, the last semantic
    child's NextSibling, and the first annotation's PreviousSibling.
  - The RadioButtons group Name and a collapsed group root with a non-radio child.
  - Native asynchronous unfocused expansion (A11Y-03).
  - Rich disclosure, inline-source and task action events (A11Y-02).
  - Text-range allocation failures (A11Y-15).
- **Large tree-burst tests** assert `none_of` per-item events after the first Structure event. This is acceptable
  because the drop happens before queuing, but, as the spec notes, the oracle cannot rule out late delivery.
- **Good**: `DecorationAnnotations` and `HearSelectionEvents` assert what a real in-process UIA client sees.
  `TestRichSemanticStaleCellCreation` uses a deterministic gate rather than timing.

## Questions for the developer

1. **Tree model replacement**: should retained TreeItem providers survive `Tree::SetModel`?
   - `TestPreparedTreeUnfocusedExpansionAction` asserts that the old `IExpandCollapseProvider` succeeds against the
     replacement model's item 101 (`Tests/Embedded/Embedded.Tests.PreparedTreeAccessibilitySemantics.h:98-114`).
   - Grid providers are invalidated by `gridModelAssignmentGeneration`.
   - Recommended answer: retire TreeItem providers on model assignment as Grid does. An ID reused by another model is a
     different object to the user.
2. **Paragraph and Page units**: in acknowledged semantic rendering they still promote to Document, by contract.
   Recommended answer: implement Paragraph from semantic block boundaries after production, and record the deferral in
   the contract.
3. **Native unfocused expansion**: is native support intentionally limited to applications that publish synchronously
   (A11Y-03)? Recommended answer: no. Extend the hints to native targets together with the A11Y-10 TTL.
4. **Consumer impact**: radio choices moved from flat root children into a Group, and a RadioButtons root now
   collapses into the window element. Have RedXe and RedSalamander UI-automation scripts that walk root children been
   reviewed? Recommended answer: yes; review them before the consumer pin bump.
5. **Rich disclosure identity**: should a successful semantic Expand/Collapse keep the acted peer alive by mapping it
   to its successor, as tab and tree items keep identity across republish? Recommended answer: yes for disclosure (via
   `blockIdentity`). This requires amending `Specs/UI/UI_InputAndAccessibility.md:107`, which currently mandates
   immediate retirement (see A11Y-02).

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.Accessibility.cpp` | Diff (about 1k changed lines) plus touched functions, callers and callees: snapshot build (rich, annotation, radio), radio navigation, text-range `Move`/`MoveEndpointByUnit`/`ExpandToEnclosingUnit`/`GetText`/`GetBoundingRectangles`/`CompareEndpoints`/`FindAttribute`/`GetAttributeValue`/`GetChildren`/`ScrollIntoView`, line helpers, dispatch and containment, `GetItem` materialization, `ExecuteExpand` and hint lifecycle, `Raise*` event paths, `QueueEmbeddedPropertyChange`, text-event validation, `CaptureAccessibilitySnapshot`/`GuardedElementSnapshot`, `CreateControlElement`/`AcquireCanonicalRootProvider` |
| `src/Controls/DxUi.RichAccessibility.h` | Diff plus touched functions (`Create`, `GetPatternProvider`, `GetPropertyValue`, `Navigate`, `Toggle`, `Invoke`, `Expand`/`Collapse`/`ChangeInlineSource`, `CopyBounds`, `CaptureSnapshot`, grid and table items) |
| `src/Controls/DxUi.RichAnnotations.h` | Reviewed fully (new) |
| `src/Controls/DxUi.RichEditorAccessibilityAccess.h` | Reviewed fully (new) |
| `src/Controls/DxUi.RichAccessibilityText.cpp` | Reviewed fully (new) |
| `include/DxUi/RichAccessibilityText.h` | Reviewed fully (new) |
| `src/Controls/DxUi.RichAccessibilityEpoch.h` | Reviewed fully (warning pragmas only) |
| `include/DxUi/AccessibilityTextUnits.h` | Skipped (unchanged in window) |
| `include/DxUi/EmbeddedAccessibility.h` | Unchanged in window; read for the site contract |
| `include/DxUi/PreparedTreeAccessibility.h` | Skipped (unchanged in window) |
| `src/Controls/DxUi.WindowHost.cpp` | Diff scanned; no accessibility snapshot or publication hunks |
| `src/Rendering/Embedded.cpp` | Diff reviewed; `InputIsCoherent`, `CancelPointer`, `Prepare` drag preservation and detach ordering read |
| `src/Controls/DxUi.RichTextEditor.cpp` | Callees only: `SetDecorations`/`GetDecorationStatus`, `SourceVisualLineRangesFallible`/`GetSourceVisualLineRanges`, `RequireCurrentLayout`/`LayoutIsCurrent`, `GetTextInputRangeRects`, `GetVisibleSourceRange`, `SetSemanticDisclosure`, `OnHidden`/`RetireLayout` |
| `src/Controls/DxUi.RichEditorLayout.h`, `src/Controls/DxUi.RichSemanticGeometry.cpp`, `src/Controls/DxUi.RichDocumentLayout.cpp` | Verifier: throwing accessors on the visual-line path only |
| `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp` | Diff test inventory plus `DecorationAnnotations`, `SemanticDisclosure`, `TestRichSemanticStaleCellCreation`, the inline-source help assertion and the scrolling-oracle commit; other new `Semantic*` cases by name only |
| `Tests/Controls/DxUi.Tests.Accessibility.cpp` | Diff plus radio (both), prepared unfocused expansion, indexed text-event tests |
| `Tests/Embedded/Embedded.Tests.EmbeddedUia.h` | Diff reviewed |
| `Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h` | Diff reviewed fully |
| `Tests/Embedded/Embedded.Tests.PreparedTreeAccessibilitySemantics.h` | Diff reviewed |
| `Tests/Embedded/Embedded.Tests.PreparedTreeEvents.h` | Diff partially (expansion-event gate) |
| `Tests/Support/Support.Tests.UiaTestClient.h` | Diff partially (`InspectAnnotations`) |
| `Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h` | Verifier: allocation-failure injection (`failAllocationAfter`) |
