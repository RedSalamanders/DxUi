#include <algorithm>
#include <stdexcept>
#include <thread>
#include <utility>

// Exercise real caret pixels on a supplied WARP surface, without an HWND or OS focus.
static void TestMultilineCaretViewport(GraphicsFixture& gpu)
{
    for (const float dpi : {96.0f, 144.0f, 192.0f})
    {
        const float scale = dpi / 96.0f;
        Hr(gpu.Resize(static_cast<UINT>(480 * scale), static_cast<UINT>(240 * scale)), "caret fixture target DPI");
        for (const bool readOnly : {false, true})
        {
            DxUi::EmbeddedHost view;
            std::shared_ptr<DxUi::GraphicsDevice> pool;
            Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "caret fixture pool");
            Hr(view.Attach(pool), "caret fixture attach");
            auto root   = std::make_unique<DxUi::Panel>();
            auto* field = root->AddChild<DxUi::TextField>(L"alpha\nbravo\ncharlie\ndelta\necho\nfoxtrot\ngolf\nhotel\nindia");
            field->SetMultiline(true);
            field->SetReadOnly(readOnly);
            field->SetVerticalTextPadding(0, 0);
            field->SetCaretColor(D2D1::ColorF(1, 0, 1));
            field->SetBounds(D2D1::RectF(40, 80, 400, 105));
            view.Controls().SetRoot(std::move(root));
            Hr(view.Prepare(gpu.width, gpu.height, dpi), "prepare caret fixture");
            view.Controls().SetFocusControl(field);
            field->SetSelectionRange(0, 0);
            Check(field->OnKeyDown(view.Controls(), VK_HOME, MK_CONTROL), "reset caret and viewport to document start");
            Check(field->OnKeyDown(view.Controls(), VK_RIGHT, 0), "place visible caret inside horizontal viewport");

            const auto verify = [&](const char* scenario, bool visible, bool partial, bool above)
            {
                Hr(view.Prepare(gpu.width, gpu.height, dpi), "prepare caret viewport state");
                DxUi::EmbeddedTextInputSnapshot before;
                Check(view.ReadTextInput(before) == S_OK && before.viewportBoundsDip.has_value(), "caret viewport snapshot exists");
                const auto viewport = *before.viewportBoundsDip;
                D2D1_RECT_F caret{};
                Check(field->DebugGetCaretRect(view.Controls(), field->GetCaretIndex(), caret), "unclipped native caret geometry exists");
                if (partial)
                    Check(caret.top < viewport.bottom && caret.bottom > viewport.bottom, "fixture has a partially visible bottom-line caret");
                else if (! visible)
                {
                    Check(above ? caret.bottom <= viewport.top : caret.top >= viewport.bottom, "fixture caret lies fully outside viewport");
                    Check(caret.top >= 0 && caret.bottom < 240, "offviewport caret remains inside the capture surface");
                }
                gpu.Bind();
                Hr(view.Composite(gpu.context.get(), gpu.Viewport()), "composite caret fixture");
                std::vector<uint8_t> pixels;
                Hr(gpu.Read(pixels), "read caret fixture pixels");
                const auto imagePath = std::filesystem::path(".build/test-artifacts/caret-viewport") /
                                       (std::string(scenario) + "-" + std::to_string(static_cast<UINT>(dpi)) + (readOnly ? "-readonly.png" : "-editable.png"));
                Hr(gpu.Save(imagePath), "caret fixture capture");
                size_t inside = 0, outside = 0;
                const auto snap = [scale](float value) { return std::round(value * scale); };
                for (UINT y = 0; y < gpu.height; ++y)
                    for (UINT x = 0; x < gpu.width; ++x)
                    {
                        const size_t offset = (static_cast<size_t>(y) * gpu.width + x) * 4;
                        if (pixels[offset] > pixels[offset + 1] + 40 && pixels[offset + 2] > pixels[offset + 1] + 40)
                        {
                            const bool inViewport = static_cast<float>(x) >= snap(viewport.left) && static_cast<float>(x) < snap(viewport.right) &&
                                                    static_cast<float>(y) >= snap(viewport.top) && static_cast<float>(y) < snap(viewport.bottom);
                            if (inViewport)
                                ++inside;
                            else
                                ++outside;
                        }
                    }
                std::cout << "caret viewport " << scenario << " dpi=" << dpi << " readOnly=" << readOnly << " inside=" << inside << " outside=" << outside
                          << " caret=" << caret.left << ',' << caret.top << ',' << caret.bottom << '\n';
                Check(visible ? inside > 0 : inside == 0, "visible caret survives clipping and offscreen caret paints nothing");
                Check(outside == 0, "caret pixels never escape the text viewport");
                DxUi::EmbeddedTextInputSnapshot after;
                Check(view.ReadTextInput(after) == S_OK && after.state.text == before.state.text && after.state.caretIndex == before.state.caretIndex &&
                          after.state.selectionAnchorIndex == before.state.selectionAnchorIndex &&
                          after.state.firstVisibleLine == before.state.firstVisibleLine,
                      "painting preserves document, caret, selection and scroll");
            };
            verify("visible", true, false, false);
            Check(field->OnKeyDown(view.Controls(), VK_NEXT, 0), "PageDown positions the caret on a partial line");
            verify("partial-bottom", true, true, false);
            Check(field->OnMouseWheel(view.Controls(), {60, 90}, -WHEEL_DELTA * 1.0f, 0), "wheel scrolls caret above viewport");
            verify("offscreen-above", false, false, true);
            field->SetSelectionRange(0, field->GetText().size());
            Check(field->OnMouseWheel(view.Controls(), {60, 90}, WHEEL_DELTA * 8.0f, 0), "wheel scrolls selected caret below viewport");
            verify("offscreen-below-selected", false, false, false);
            field->SetSelectionRange(0, 0);
            verify("restored", true, false, false);
        }
    }
    Hr(gpu.Resize(480, 240), "restore shared graphics fixture size");
}

// Application-side composition transport; no HWND, TSF context or real IME is created by this fixture.
static void TestEmbeddedTextInput(GraphicsFixture& gpu)
{
    using Action = DxUi::EmbeddedTextInputAction;
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "text pool");
    Hr(view.Attach(pool), "text attach");
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)
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
    field->SetText(L"Contact alpha now");
    Hr(view.Prepare(480, 240), "prepare double-tap text");
    view.Controls().SetFocusControl(field);
    DxUi::EmbeddedTextInputSnapshot wordSnap;
    Check(view.ReadTextInput(wordSnap) == S_OK, "focused field exports state before a double-tap");
    D2D1_RECT_F wordBounds{};
    bool wordClipped = true;
    Check(view.GetTextInputRangeBounds(wordSnap.revision, 8, 13, wordBounds, wordClipped) == S_OK && ! wordClipped && wordBounds.right > wordBounds.left,
          "alpha word has visible DIP bounds for a double-tap");
    const float wordX = (wordBounds.left + wordBounds.right) * 0.5f;
    const float wordY = (wordBounds.top + wordBounds.bottom) * 0.5f;
    Check(view.DispatchPointer({DxUi::PointerAction::Down, wordX, wordY}), "first tap on a word places the caret");
    Check(view.DispatchPointer({DxUi::PointerAction::Up, wordX, wordY}), "first tap on a word releases");
    Check(view.DispatchPointer({DxUi::PointerAction::Cancel}), "capture-loss notification after up preserves the completed tap");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, wordX, wordY}), "second tap synthesizes a control double-click");
    static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, wordX, wordY}));
    Check(view.ReadTextInput(wordSnap) == S_OK && wordSnap.state.selectionAnchorIndex.has_value(), "embedded double-tap selects a word");
    const size_t wordStart = (std::min)(*wordSnap.state.selectionAnchorIndex, wordSnap.state.caretIndex);
    const size_t wordEnd   = (std::max)(*wordSnap.state.selectionAnchorIndex, wordSnap.state.caretIndex);
    Check(wordStart == 8 && wordEnd == 13, "embedded double-tap selects exactly the punctuation-delimited word");

    field->SetText(L"Contact alpha now");
    Hr(view.Prepare(480, 240), "prepare text for interrupted-press double-tap check");
    view.Controls().SetFocusControl(field);
    Check(view.DispatchPointer({DxUi::PointerAction::Down, wordX, wordY}), "start a press that will be canceled before up");
    Check(view.DispatchPointer({DxUi::PointerAction::Cancel}), "capture loss cancels the active press");
    Check(view.DispatchPointer({DxUi::PointerAction::Down, wordX, wordY}), "the next press after cancellation is an ordinary click");
    Check(view.DispatchPointer({DxUi::PointerAction::Up, wordX, wordY}), "ordinary click after cancellation releases");
    Check(view.ReadTextInput(wordSnap) == S_OK &&
              (! wordSnap.state.selectionAnchorIndex || wordSnap.state.selectionAnchorIndex.value() == wordSnap.state.caretIndex),
          "canceling an active press leaves the next ordinary click with only a collapsed caret");
    field->SetText(L"Original");
    view.Controls().SetFocusControl(field);
    Hr(view.Prepare(480, 240), "restore original text after double-tap");
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

static void TestEmbeddedTranslatedCharacterOwnership(GraphicsFixture& gpu)
{
    std::shared_ptr<DxUi::GraphicsDevice> pool;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), pool), "key correlation pool");

    const auto makeView = [&]()
    {
        auto view = std::make_unique<DxUi::EmbeddedHost>();
        Hr(view->Attach(pool), "key correlation attach");
        return view;
    };
    const auto makeInputTree = [](DxUi::EmbeddedHost& view, std::wstring value, DxUi::TextField*& field, DxUi::Button*& button)
    {
        auto root = std::make_unique<DxUi::Panel>();
        button    = root->AddChild<DxUi::Button>(L"Transfer focus");
        field     = root->AddChild<DxUi::TextField>(std::move(value));
        button->SetOnClick([&view, field] { view.Controls().SetFocusControl(field); });
        view.Controls().SetRoot(std::move(root));
        Hr(view.Prepare(480, 240), "prepare key correlation controls");
    };

    for (const UINT key : {UINT(VK_RETURN), UINT(VK_SPACE)})
    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"alpha", field, button);
        view->Controls().SetFocusControl(button);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(view->DispatchKey(key, true, 0u, receipt), "button handles activation keydown");
        Check(view->Controls().GetFocusControl() == field, "activation callback transfers focus to the editor");
        Check(! view->ShouldTranslateCharacter(receipt), "activation commands do not ask the OS to translate a text character");
        const wchar_t character = key == VK_RETURN ? L'\r' : L' ';
        Check(view->DispatchCharacter(character, 0u, receipt), "translated activation character is consumed");
        Check(field->GetText() == L"alpha", "activation character does not edit the newly focused control");
        Check(! view->DispatchCharacter(character, 0u, receipt), "a correlated receipt is one-use");
    }

    // An unhandled printable Space and an explicitly accepted multiline Return still reach the same editor.
    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"alpha", field, button);
        static_cast<void>(button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        bool recursiveHandled = true;
        field->SetOnTextChanged([&](std::wstring_view) { recursiveHandled = view->DispatchCharacter(L' ', 0u, receipt); });
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "ordinary text Space is unhandled by the key route");
        Check(view->ShouldTranslateCharacter(receipt), "same-owner text key permits OS character translation");
        Check(! view->DispatchKey(VK_SPACE, false, 0u), "key-up remains a separate ordinary key event");
        Check(view->DispatchCharacter(L' ', 0u, receipt) && field->GetText() == L"alpha ", "same-owner Space is inserted once");
        Check(! recursiveHandled && field->GetText() == L"alpha ", "receipt is consumed before a reentrant edit callback");
        field->SetMultiline(true);
        Hr(view->Prepare(480, 240), "prepare multiline key route");
        DxUi::EmbeddedKeyDispatchReceipt enter;
        Check(view->DispatchKey(VK_RETURN, true, 0u, enter), "multiline editor accepts Return keydown");
        Check(view->ShouldTranslateCharacter(enter), "explicitly accepted multiline Return permits OS character translation");
        Check(view->DispatchCharacter(L'\r', 0u, enter) && field->GetText() == L"alpha \n", "explicit multiline Return is inserted once");
    }

    // A receipt rejected by another view or for another character remains usable by its originating view.
    {
        auto first                   = makeView();
        auto second                  = makeView();
        DxUi::TextField* firstField  = nullptr;
        DxUi::Button* firstButton    = nullptr;
        DxUi::TextField* secondField = nullptr;
        DxUi::Button* secondButton   = nullptr;
        makeInputTree(*first, L"first", firstField, firstButton);
        makeInputTree(*second, L"second", secondField, secondButton);
        static_cast<void>(firstButton);
        static_cast<void>(secondButton);
        first->Controls().SetFocusControl(firstField);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! first->DispatchKey(VK_SPACE, true, 0u, receipt), "originating text keydown records receipt");
        Check(! second->ShouldTranslateCharacter(receipt), "foreign view cannot grant translation permission");
        Check(! second->DispatchCharacter(L' ', 0u, receipt) && firstField->GetText() == L"first" && secondField->GetText() == L"second",
              "foreign view rejects without mutating either control");
        Check(! first->DispatchCharacter(L'\r', 0u, receipt) && firstField->GetText() == L"first", "mismatched character is rejected without editing");
        Check(first->DispatchCharacter(L' ', 0u, receipt) && firstField->GetText() == L"first ", "the matching character still consumes the receipt");
    }

    // Stale focus/root identities consume their matching character without routing it into a successor.
    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"old", field, button);
        static_cast<void>(button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "stale-owner keydown records receipt");
        auto replacement = std::make_unique<DxUi::TextField>();
        replacement->SetText(L"new");
        auto* replacementField = replacement.get();
        view->Controls().SetRoot(std::move(replacement));
        Hr(view->Prepare(480, 240), "prepare replacement owner");
        view->Controls().SetFocusControl(replacementField);
        Check(! view->ShouldTranslateCharacter(receipt), "replacement owner cannot inherit stale translation permission");
        Check(view->DispatchCharacter(L' ', 0u, receipt) && replacementField->GetText() == L"new", "stale matching character is consumed, not retargeted");
    }

    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"roundtrip", field, button);
        static_cast<void>(button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "focus-roundtrip keydown records receipt");
        view->Controls().SetFocusControl(nullptr);
        view->Controls().SetFocusControl(field);
        Check(! view->ShouldTranslateCharacter(receipt), "focus roundtrip invalidates translation permission");
        Check(view->DispatchCharacter(L' ', 0u, receipt) && field->GetText() == L"roundtrip", "focus roundtrip consumes without editing");
    }

    {
        auto view = makeView();
        auto rich = std::make_unique<DxUi::RichTextEditor>(L"rich");
        rich->SetMarkdownRenderingEnabled(false);
        auto* editor = rich.get();
        view->Controls().SetRoot(std::move(rich));
        Hr(view->Prepare(480, 240), "prepare rich editor key route");
        view->Controls().SetFocusControl(editor);
        editor->SetSelection({4u, 4u});
        DxUi::EmbeddedKeyDispatchReceipt space;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, space) && view->ShouldTranslateCharacter(space), "rich editor Space keeps same-owner text translation");
        Check(view->DispatchCharacter(L' ', 0u, space) && editor->GetText() == L"rich ", "rich editor accepts its correlated Space once");
        Hr(view->Prepare(480, 240), "prepare rich editor Return route");
        DxUi::EmbeddedKeyDispatchReceipt enter;
        Check(view->DispatchKey(VK_RETURN, true, 0u, enter) && view->ShouldTranslateCharacter(enter), "rich editor explicitly permits Return translation");
        Check(view->DispatchCharacter(L'\r', 0u, enter) && editor->GetText() == L"rich \r\n", "rich editor accepts its correlated Return once");
    }

    {
        auto view = makeView();
        view->Controls().SetRoot(std::make_unique<DxUi::Panel>());
        Hr(view->Prepare(480, 240), "prepare ownerless key route");
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "ownerless keydown still issues a correlation receipt");
        auto successor = std::make_unique<DxUi::TextField>(L"successor");
        auto* field    = successor.get();
        view->Controls().SetRoot(std::move(successor));
        Hr(view->Prepare(480, 240), "prepare ownerless successor");
        view->Controls().SetFocusControl(field);
        Check(view->DispatchCharacter(L' ', 0u, receipt) && field->GetText() == L"successor", "ownerless translated character cannot be retargeted");
    }

    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"reattached", field, button);
        static_cast<void>(button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "pre-detach keydown records receipt");
        view->Detach();
        Hr(view->Attach(pool), "key receipt reattach");
        auto successor = std::make_unique<DxUi::TextField>(L"new attachment");
        auto* newField = successor.get();
        view->Controls().SetRoot(std::move(successor));
        Hr(view->Prepare(480, 240), "prepare reattached text owner");
        view->Controls().SetFocusControl(newField);
        Check(view->DispatchCharacter(L' ', 0u, receipt) && newField->GetText() == L"new attachment", "detach and reattach consume an old receipt");
    }

    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"move", field, button);
        static_cast<void>(button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt original;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, original), "move test records a receipt");
        DxUi::EmbeddedKeyDispatchReceipt moved(std::move(original));
        Check(! view->ShouldTranslateCharacter(original), "moved-from receipt is invalid");
        Check(view->DispatchCharacter(L' ', 0u, moved) && field->GetText() == L"move ", "moved receipt remains one-use and valid");
    }

    // A pending activation receipt does not affect independent text-only input when no translated char arrives.
    {
        auto view              = makeView();
        DxUi::TextField* field = nullptr;
        DxUi::Button* button   = nullptr;
        makeInputTree(*view, L"pending", field, button);
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(! view->DispatchKey(VK_SPACE, true, 0u, receipt), "pending receipt keydown is unhandled");
        Check(view->DispatchCharacter(L'x') && field->GetText() == L"pendingx", "standalone committed text remains independent of a pending receipt");
        Check(view->DispatchCharacter(L' ', 0u, receipt) && field->GetText() == L"pendingx ",
              "later matching translated char remains paired with its own receipt");
    }

    // Nested dispatch has its own translated-character permission and cannot leak it to an outer handled command.
    {
        auto view = makeView();
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
        struct NestedField final : DxUi::TextField
        {
            explicit NestedField(DxUi::EmbeddedHost& embedded) : embedded(embedded)
            {
                SetMultiline(true);
                SetText(L"nested");
            }
            bool OnKeyDown(DxUi::ControlHost& host, UINT key, UINT modifiers) override
            {
                if (key == VK_SPACE)
                {
                    static_cast<void>(embedded.DispatchKey(VK_RETURN, true, modifiers));
                    return true;
                }
                return TextField::OnKeyDown(host, key, modifiers);
            }
            DxUi::EmbeddedHost& embedded;
        };
#pragma warning(pop)
        auto root   = std::make_unique<DxUi::Panel>();
        auto* field = root->AddChild<NestedField>(*view);
        view->Controls().SetRoot(std::move(root));
        Hr(view->Prepare(480, 240), "prepare nested key frame");
        view->Controls().SetFocusControl(field);
        DxUi::EmbeddedKeyDispatchReceipt receipt;
        Check(view->DispatchKey(VK_SPACE, true, 0u, receipt), "nested preview command handles Space");
        Check(view->DispatchCharacter(L' ', 0u, receipt) && field->GetText() == L"nested", "nested Return permission does not authorize outer Space");
    }

    // Reusing the same receipt object for overlapping calls fails closed even if the nested call receives permission.
    {
        auto view = makeView();
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
        struct ReusingReceiptField final : DxUi::TextField
        {
            ReusingReceiptField(DxUi::EmbeddedHost& embedded, DxUi::EmbeddedKeyDispatchReceipt& receipt, bool outerReturn, bool throwAfterNested)
                : embedded(embedded),
                  receipt(receipt),
                  outerReturn(outerReturn),
                  throwAfterNested(throwAfterNested)
            {
                SetMultiline(true);
                SetText(L"unchanged");
            }
            bool OnKeyDown(DxUi::ControlHost& host, UINT key, UINT modifiers) override
            {
                if (outerReturn && ! nested && key == VK_RETURN)
                {
                    const bool handled = TextField::OnKeyDown(host, key, modifiers);
                    nested             = true;
                    static_cast<void>(embedded.DispatchKey(VK_SPACE, true, modifiers, receipt));
                    nested = false;
                    return handled;
                }
                if (! outerReturn && ! nested && key == VK_SPACE)
                {
                    nested = true;
                    static_cast<void>(embedded.DispatchKey(VK_SPACE, true, modifiers, receipt));
                    nested = false;
                    if (throwAfterNested)
                        throw std::runtime_error("intentional nested dispatch failure");
                    return true;
                }
                if (outerReturn && key == VK_SPACE)
                    return true; // The nested activation is a handled command and must not inherit the outer Return grant.
                return TextField::OnKeyDown(host, key, modifiers);
            }
            DxUi::EmbeddedHost& embedded;
            DxUi::EmbeddedKeyDispatchReceipt& receipt;
            bool outerReturn      = false;
            bool throwAfterNested = false;
            bool nested           = false;
        };
#pragma warning(pop)

        for (const auto [outerReturn, throwAfterNested] : {std::pair{true, false}, std::pair{false, false}, std::pair{false, true}})
        {
            DxUi::EmbeddedKeyDispatchReceipt receipt;
            auto root   = std::make_unique<DxUi::Panel>();
            auto* field = root->AddChild<ReusingReceiptField>(*view, receipt, outerReturn, throwAfterNested);
            view->Controls().SetRoot(std::move(root));
            Hr(view->Prepare(480, 240), "prepare overlapping receipt route");
            view->Controls().SetFocusControl(field);
            const UINT outerKey = outerReturn ? VK_RETURN : VK_SPACE;
            Check(view->DispatchKey(outerKey, true, 0u, receipt) == ! throwAfterNested, "outer overlapping key dispatch reports its command result");
            Check(! view->ShouldTranslateCharacter(receipt), "overlapping reuse invalidates the shared receipt at outer completion");
            Check(! view->DispatchCharacter(outerReturn ? L'\r' : L' ', 0u, receipt),
                  "overlapping shared receipt cannot dispatch a delayed correlated character");
            Check(! view->DispatchCharacter(L' ', 0u, receipt), "overlapping shared receipt rejects a second delayed character attempt");
            Check(field->GetText() == L"unchanged", "overlapping receipt rejection leaves the source unchanged");
            view->Controls().SetRoot(nullptr);
        }
    }

    // Distinct receipt objects preserve nested event associations independently.
    {
        auto view = makeView();
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
        struct SeparateReceiptField final : DxUi::TextField
        {
            SeparateReceiptField(DxUi::EmbeddedHost& embedded, DxUi::EmbeddedKeyDispatchReceipt& nestedReceipt)
                : embedded(embedded),
                  nestedReceipt(nestedReceipt)
            {
                SetMultiline(true);
                SetText(L"separate");
            }
            bool OnKeyDown(DxUi::ControlHost& host, UINT key, UINT modifiers) override
            {
                if (key == VK_RETURN && ! nesting)
                {
                    const bool handled = TextField::OnKeyDown(host, key, modifiers);
                    nesting            = true;
                    static_cast<void>(embedded.DispatchKey(VK_SPACE, true, modifiers, nestedReceipt));
                    nesting = false;
                    return handled;
                }
                return TextField::OnKeyDown(host, key, modifiers);
            }
            DxUi::EmbeddedHost& embedded;
            DxUi::EmbeddedKeyDispatchReceipt& nestedReceipt;
            bool nesting = false;
        };
#pragma warning(pop)
        DxUi::EmbeddedKeyDispatchReceipt outerReceipt;
        DxUi::EmbeddedKeyDispatchReceipt nestedReceipt;
        auto root   = std::make_unique<DxUi::Panel>();
        auto* field = root->AddChild<SeparateReceiptField>(*view, nestedReceipt);
        view->Controls().SetRoot(std::move(root));
        Hr(view->Prepare(480, 240), "prepare distinct nested receipt route");
        view->Controls().SetFocusControl(field);
        field->SetSelectionRange(8u, 8u);
        Check(view->DispatchKey(VK_RETURN, true, 0u, outerReceipt), "outer multiline Return is handled");
        Check(view->ShouldTranslateCharacter(outerReceipt), "distinct outer receipt retains its own Return permission");
        // The nested Space has its own key event and receipt; it remains usable independently of the outer event.
        Check(view->ShouldTranslateCharacter(nestedReceipt), "distinct nested receipt preserves its own permission");
        Check(view->DispatchCharacter(L' ', 0u, nestedReceipt) && field->GetText() == L"separate ",
              "distinct nested receipt dispatches its associated character");
        Check(view->DispatchCharacter(L'\r', 0u, outerReceipt) && field->GetText() == L"separate \n",
              "distinct outer receipt dispatches its own character once");
    }

    // Per-view counters can coincide, so receipt issuance also validates the producing EmbeddedHost and ControlHost.
    {
        auto outerView = makeView();
        auto innerView = makeView();
        DxUi::EmbeddedKeyDispatchReceipt sharedReceipt;
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
        struct CrossViewField final : DxUi::TextField
        {
            CrossViewField(DxUi::EmbeddedHost& other, DxUi::EmbeddedKeyDispatchReceipt& receipt) : other(other), receipt(receipt)
            {
                SetMultiline(true);
                SetText(L"outer");
            }
            bool OnKeyDown(DxUi::ControlHost& host, UINT key, UINT modifiers) override
            {
                if (key == VK_RETURN)
                {
                    const bool handled = TextField::OnKeyDown(host, key, modifiers);
                    static_cast<void>(other.DispatchKey(VK_SPACE, true, modifiers, receipt));
                    return handled;
                }
                return TextField::OnKeyDown(host, key, modifiers);
            }
            DxUi::EmbeddedHost& other;
            DxUi::EmbeddedKeyDispatchReceipt& receipt;
        };
#pragma warning(pop)
        auto outerRoot   = std::make_unique<DxUi::Panel>();
        auto* outerField = outerRoot->AddChild<CrossViewField>(*innerView, sharedReceipt);
        outerView->Controls().SetRoot(std::move(outerRoot));
        auto innerRoot   = std::make_unique<DxUi::TextField>(L"inner");
        auto* innerField = innerRoot.get();
        innerView->Controls().SetRoot(std::move(innerRoot));
        Hr(outerView->Prepare(480, 240), "prepare cross-view outer route");
        Hr(innerView->Prepare(480, 240), "prepare cross-view inner route");
        outerView->Controls().SetFocusControl(outerField);
        innerView->Controls().SetFocusControl(innerField);
        Check(outerView->DispatchKey(VK_RETURN, true, 0u, sharedReceipt), "outer host grants Return during cross-view callback");
        Check(! outerView->ShouldTranslateCharacter(sharedReceipt), "equal per-view sequences cannot authorize a foreign nested receipt");
        Check(! innerView->ShouldTranslateCharacter(sharedReceipt), "cross-view overlap invalidates the nested view's translation permission too");
        Check(! outerView->DispatchCharacter(L'\r', 0u, sharedReceipt), "outer host rejects the overwritten foreign receipt");
        Check(! innerView->DispatchCharacter(L' ', 0u, sharedReceipt), "nested host rejects delayed delivery after cross-view overlap");
        Check(outerField->GetText() == L"outer" && innerField->GetText() == L"inner", "cross-view overlap leaves both sources unchanged");
    }
}
