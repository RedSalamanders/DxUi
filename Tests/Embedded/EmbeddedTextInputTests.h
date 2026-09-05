#include <thread>

// Application-side composition transport; no HWND, TSF context or real IME is created by this fixture.
static void TestEmbeddedTextInput(GraphicsFixture& gpu)
{
    using Action = DxUi::EmbeddedTextInputAction;
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "text pool");
    Hr(view.Attach(pool), "text attach");
    struct TextPanel final : DxUi::Panel
    {
        void OnBoundsChanged() noexcept override
        {
            Panel::OnBoundsChanged();
            const auto bounds = GetBounds();
            if (GetLogicalChildCount())
                GetLogicalChild(0)->SetBounds(D2D1::RectF(bounds.left + 8, bounds.top + 40, bounds.right - 8, bounds.top + 88));
        }
    };
    auto panel  = std::make_unique<TextPanel>();
    auto* field = panel->AddChild<DxUi::TextField>();
    field->SetText(L"Original");
    size_t notifications = 0;
    field->SetOnTextChanged([&](std::wstring_view) { ++notifications; });
    view.Controls().SetRoot(std::move(panel));
    Hr(view.Prepare(480, 240), "text prepare");
    DxUi::EmbeddedTextInputSnapshot snapshot;
    Check(view.ReadTextInput(snapshot) == S_FALSE && snapshot.revision == 0, "unfocused text has no service snapshot");
    view.Controls().SetFocusControl(field);
    const auto read = [&]()
    {
        Check(view.ReadTextInput(snapshot) == S_OK && snapshot.revision != 0, "read current text session");
        return snapshot.state;
    };
    auto state                  = read();
    const uint64_t firstFocusId = snapshot.focusId;
    Check(firstFocusId != 0, "focused text exposes a nonzero focus-session identity");
    view.MarkDirty();
    read();
    Check(snapshot.focusId == firstFocusId, "ordinary invalidation preserves text focus identity");
    view.Controls().SetFocusControl(nullptr);
    view.Controls().SetFocusControl(field);
    read();
    Check(snapshot.focusId != firstFocusId, "focus leaving and returning between snapshots changes text focus identity");
    HRESULT otherRead = S_OK, otherWrite = S_OK;
    std::thread other([&]
    {
        DxUi::EmbeddedTextInputSnapshot foreign;
        otherRead  = view.ReadTextInput(foreign);
        otherWrite = view.ApplyTextInput(snapshot.revision, state, Action::Commit);
        view.CancelTextInput();
    });
    other.join();
    Check(otherRead == RPC_E_WRONG_THREAD && otherWrite == RPC_E_WRONG_THREAD && view.Controls().GetFocusControl() == field,
          "foreign-thread services cannot read, edit or cancel UI state");
    Hr(view.Prepare(480, 240), "prepare focused geometry");
    state = read();
    D2D1_RECT_F range{};
    bool clipped = true;
    size_t hit   = 99;
    Check(view.GetTextInputRangeBounds(snapshot.revision, 0, 4, range, clipped) == S_OK && range.right > range.left && ! clipped,
          "revisioned range geometry uses visible text layout");
    Check(view.HitTestTextInput(snapshot.revision, {range.left, range.top + 1}, hit) == S_OK && hit <= 1, "revisioned point lookup addresses current text");
    Check(view.GetTextInputRangeBounds(snapshot.revision, 0, 99, range, clipped) == E_INVALIDARG && range.left == 0 && clipped,
          "invalid range clears geometry outputs");
    const auto geometryRevision = snapshot.revision;
    view.MarkDirty();
    read();
    Check(! snapshot.caretBoundsDip && ! snapshot.viewportBoundsDip, "dirty text retains edit state but does not publish stale geometry");
    Check(view.GetTextInputRangeBounds(snapshot.revision, 0, 4, range, clipped) == S_FALSE && clipped, "dirty geometry requests wait for preparation");
    Hr(view.Prepare(480, 240, 144), "prepare text geometry at 144 DPI");
    state = read();
    Check(view.GetTextInputRangeBounds(geometryRevision, 0, 4, range, clipped) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
          "old revision cannot query a newly prepared layout");
    Check(view.GetTextInputRangeBounds(snapshot.revision, 0, 4, range, clipped) == S_OK && range.left >= 8 && range.right < 320,
          "144-DPI geometry remains in DIPs for one application-side conversion");
    Hr(view.Prepare(480, 240), "restore fixture DPI");
    state = read();
    Check(state.text == L"Original" && snapshot.caretBoundsDip && snapshot.viewportBoundsDip, "text and DIP geometry exported");
    const auto initialRevision = snapshot.revision;
    const auto preview         = [&](std::wstring text)
    {
        auto next       = read();
        next.text       = std::move(text);
        next.caretIndex = next.text.size();
        next.selectionAnchorIndex.reset();
        next.compositionStartIndex       = 0;
        next.compositionEndIndex         = next.text.size();
        next.conversionTargetStartIndex  = 0;
        next.conversionTargetEndIndex    = next.text.size();
        next.compositionCursorIndex      = next.text.size();
        next.compositionClauseBoundaries = {0, next.text.size()};
        Check(view.ApplyTextInput(snapshot.revision, next, Action::Preview) == S_OK, "preview accepted");
        Check(notifications == 0, "preview never notifies model");
    };
    preview(L"\u6771\u4eac \U0001F642");
    state = read();
    Check(state.compositionStartIndex == 0 && state.compositionEndIndex == state.text.size() && state.compositionClauseBoundaries.size() == 2,
          "composition metadata reaches shared text renderer");
    Check(view.ApplyTextInput(initialRevision, state, Action::Preview) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH), "stale text edit rejected");
    Hr(view.Prepare(480, 240), "prepare composed text");
    gpu.Bind();
    Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "draw composed text");
    Hr(gpu.Save(L".build/test-artifacts/embedded-text-composition.png"), "composition capture");
    read();
    Check(view.ApplyTextInput(snapshot.revision, {}, Action::Cancel) == S_OK && field->GetText() == L"Original" && notifications == 0,
          "composition cancellation restores base without notification");
    preview(L"\u6771\u4eac \U0001F642");
    state = read();
    state.compositionStartIndex.reset();
    state.compositionEndIndex.reset();
    state.conversionTargetStartIndex.reset();
    state.conversionTargetEndIndex.reset();
    state.compositionCursorIndex.reset();
    state.compositionClauseBoundaries.clear();
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK && notifications == 1, "unchanged preview commits once relative to original");
    read();
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK && notifications == 1, "unchanged ordinary edit does not notify");
    const auto committed = std::wstring(field->GetText());
    for (unsigned test = 0; test < 13; ++test)
    {
        auto invalid                  = read();
        auto action                   = Action::Preview;
        invalid.compositionStartIndex = 0;
        invalid.compositionEndIndex   = invalid.text.size();
        switch (test)
        {
            case 0: invalid.caretIndex = invalid.text.size() + 1; break;
            case 1: invalid.selectionAnchorIndex = invalid.text.size() + 1; break;
            case 2: invalid.compositionEndIndex.reset(); break;
            case 3: invalid.compositionStartIndex = invalid.text.size() + 1; break;
            case 4: invalid.compositionEndIndex = invalid.text.size() + 1; break;
            case 5: invalid.conversionTargetStartIndex = 0; break;
            case 6:
                invalid.conversionTargetStartIndex = 0;
                invalid.conversionTargetEndIndex   = invalid.text.size() + 1;
                break;
            case 7: invalid.compositionCursorIndex = invalid.text.size() + 1; break;
            case 8: invalid.compositionClauseBoundaries = {3, 2}; break;
            case 9: invalid.compositionClauseBoundaries.resize(257); break;
            case 10: invalid.text.assign(65537, L'x'); break;
            case 11: action = Action::Commit; break;
            case 12: action = static_cast<Action>(99); break;
        }
        Check(view.ApplyTextInput(snapshot.revision, invalid, action) == E_INVALIDARG && field->GetText() == committed && notifications == 1,
              "malformed text state is rejected without mutation");
    }
    state                      = read();
    state.caretIndex           = state.text.size();
    state.selectionAnchorIndex = state.text.size() - 2;
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK, "supplementary character selection accepted");
    read();
    Check(snapshot.state.selectionAnchorIndex == committed.size() - 2 && notifications == 1, "UTF-16 selection preserved without model edit");
    field->SetReadOnly(true);
    Hr(view.Prepare(480, 240), "readonly prepare");
    state            = read();
    state.text       = L"replace";
    state.caretIndex = 0;
    state.selectionAnchorIndex.reset();
    state.readOnly = false;
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == E_ACCESSDENIED && field->IsReadOnly(), "snapshot cannot override readonly policy");
    state            = read();
    state.caretIndex = 0;
    state.selectionAnchorIndex.reset();
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK, "readonly selection remains available");
    field->SetReadOnly(false);
    notifications = 0;
    Hr(view.Prepare(480, 240), "editable prepare");
    field->SetText(L"One");
    field->SetSelectionRange(3, 3);
    Check(view.DispatchCharacter(L'!') && field->GetText() == L"One!", "ordinary edit creates prior undo history");
    notifications = 0;
    preview(L"discarded");
    Check(view.DispatchKey(VK_ESCAPE, true) && field->GetText() == L"One!" && notifications == 0, "Escape cancels composition before application command");
    Check(view.DispatchKey('Z', true, MK_CONTROL) && field->GetText() == L"One", "canceled preview preserves earlier undo entry");
    Check(view.DispatchKey('Y', true, MK_CONTROL) && field->GetText() == L"One!", "canceled preview preserves redo");
    notifications = 0;
    preview(L"Committed phrase");
    state = read();
    state.compositionStartIndex.reset();
    state.compositionEndIndex.reset();
    state.conversionTargetStartIndex.reset();
    state.conversionTargetEndIndex.reset();
    state.compositionCursorIndex.reset();
    state.compositionClauseBoundaries.clear();
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK && notifications == 1, "composition records one edit");
    Check(view.DispatchKey('Z', true, MK_CONTROL) && field->GetText() == L"One!", "undo restores pre-composition value");
    Check(view.DispatchKey('Z', true, MK_CONTROL) && field->GetText() == L"One", "composition preserves preceding undo history");
    notifications = 0;
    preview(L"temporary");
    field->SetText(L"External update");
    view.SetVisible(false);
    Check(field->GetText() == L"External update" && notifications == 0, "external value survives composition cancellation");
    Check(view.ReadTextInput(snapshot) == S_FALSE && snapshot.state.text.empty(), "hidden text snapshot clears old data");
    view.SetVisible(true);
    Hr(view.Prepare(480, 240), "reshow text");
    view.Controls().SetFocusControl(field);
    preview(L"hidden draft");
    view.SetVisible(false);
    Check(field->GetText() == L"External update" && notifications == 0, "hide rolls back preview");
    view.SetVisible(true);
    Hr(view.Prepare(480, 240), "prepare text again");
    view.Controls().SetFocusControl(field);
    preview(L"focus draft");
    view.CancelTextInput();
    Check(field->GetText() == L"External update" && view.ReadTextInput(snapshot) == S_FALSE, "OS focus loss cancels text session");
    view.Controls().SetFocusControl(field);
    preview(L"zero draft");
    Check(view.Prepare(0, 240) == S_FALSE && field->GetText() == L"External update", "zero-size suspension rolls back preview");
    Hr(view.Prepare(480, 240), "resume nonzero text");
    view.Controls().SetFocusControl(field);
    preview(L"device draft");
    Hr(view.ReplaceDevice(pool), "text device replacement");
    Check(field->GetText() == L"External update" && view.ReadTextInput(snapshot) == S_FALSE, "device replacement cancels input before reprepare");
    Hr(view.Prepare(480, 240), "reprepare text device");
    view.Controls().SetFocusControl(field);
    state                   = read();
    const auto beforeDetach = snapshot.revision;
    view.Detach();
    Hr(view.Attach(pool), "text reattach");
    Hr(view.Prepare(480, 240), "text reattach prepare");
    // A new attachment owns a new tree; old service tokens cannot address it.
    auto owned = std::make_unique<DxUi::TextField>();
    field      = owned.get();
    field->SetText(L"New attachment");
    view.Controls().SetRoot(std::move(owned));
    Hr(view.Prepare(480, 240), "prepare new attachment tree");
    view.Controls().SetFocusControl(field);
    read();
    Check(view.ApplyTextInput(beforeDetach, state, Action::Commit) == HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH),
          "reattachment invalidates prior service revision");
    state            = read();
    state.text       = L"replacement callback";
    state.caretIndex = state.text.size();
    state.selectionAnchorIndex.reset();
    field->SetOnTextChanged([](std::wstring_view) { throw std::runtime_error("test callback failure"); });
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == E_FAIL, "throwing consumer callback is contained");
    Check(view.ReadTextInput(snapshot) == S_FALSE, "failed import suppresses interaction until prepare");
    Hr(view.Prepare(480, 240), "recover coherent text after callback failure");
    state            = read();
    state.text       = L"destroy edited root";
    state.caretIndex = state.text.size();
    field->SetOnTextChanged([&](std::wstring_view) { view.Controls().SetRoot(std::make_unique<DxUi::Panel>()); });
    Check(view.ApplyTextInput(snapshot.revision, state, Action::Commit) == S_OK, "commit callback may destroy edited tree");
    Check(view.ReadTextInput(snapshot) == S_FALSE, "removed control disconnects its text service");
    Check(view.Controls().GetHwnd() == nullptr, "text transport creates no HWND");
}
