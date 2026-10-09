#include <UIAutomation.h>
#include <thread>

struct TestEmbeddedAccessibilitySite final : DxUi::EmbeddedAccessibilitySite
{
    IRawElementProviderFragmentRoot* root = nullptr; // Borrowed fixture reference; cleared before teardown.
    size_t focusRequests                  = 0;
    size_t parentRequests                 = 0;
    size_t completions                    = 0;
    void ActionCompleted() noexcept override
    {
        ++completions;
    }
    HRESULT Navigate(NavigateDirection direction, IRawElementProviderFragment** result) noexcept override
    {
        *result = nullptr;
        if (direction == NavigateDirection_Parent)
            ++parentRequests;
        return S_OK;
    }
    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        *result = root;
        if (root)
            root->AddRef();
        return root ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT RequestFocus() noexcept override
    {
        ++focusRequests;
        return S_OK;
    }
};

static void TestEmbeddedDisclosureAccessibility(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "disclosure supplied-device scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<DxUi::Panel>();
    auto* header  = controls->AddChild<DxUi::Button>(L"Afficher les détails du traitement");
    header->SetBounds(D2D1::RectF(12, 12, 300, 48));
    header->SetDisclosureExpanded(false);
    auto* body = controls->AddChild<DxUi::Checkbox>(L"Appliquer aux éléments similaires");
    body->SetBounds(D2D1::RectF(12, 60, 320, 96));
    body->SetVisible(false);
    unsigned int clicks = 0;
    header->SetOnClick([&]()
    {
        ++clicks;
        const bool expanded = ! header->GetDisclosureExpanded().value();
        if (! expanded && body->HasFocus())
            view.Controls().SetFocusControl(header);
        body->SetVisible(expanded);
        header->SetDisclosureExpanded(expanded);
    });
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(720, 510, 144), "prepare disclosure");
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 720, 510}, true};
    Hr(view.AttachAccessibility(site, 0x2222, placement), "attach disclosure accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "disclosure provider root");
    site->root = root.get();
    wil::com_ptr_nothrow<IRawElementProviderFragment> headerProvider;
    Hr(root->ElementProviderFromPoint(36, 36, headerProvider.put()), "hit disclosure");
    Check(bool(headerProvider), "disclosure header is present");
    wil::com_ptr_nothrow<IExpandCollapseProvider> pattern;
    Hr(headerProvider.query_to(pattern.put()), "embedded disclosure pattern");
    Hr(pattern->Expand(), "expand embedded disclosure");
    Check(clicks == 1 && body->IsVisible(), "embedded action invokes caller once");
    Hr(view.Prepare(720, 510, 144), "prepare expanded body");
    Hr(view.UpdateAccessibility(placement), "publish expanded disclosure");
    ExpandCollapseState state{};
    Hr(pattern->get_ExpandCollapseState(&state), "read published expanded state");
    Check(state == ExpandCollapseState_Expanded, "embedded state follows coherent snapshot");
    Hr(pattern->Expand(), "idempotent embedded expand");
    Check(clicks == 1, "expanded embedded action does not toggle again");
    view.Controls().SetFocusControl(body);
    Hr(pattern->Collapse(), "collapse embedded disclosure");
    Check(clicks == 2 && header->HasFocus() && ! body->IsVisible(), "caller restores focus before hiding body");
    Hr(view.Prepare(720, 510, 144), "prepare collapsed body");
    Hr(view.UpdateAccessibility(placement), "publish collapsed disclosure");
    Hr(pattern->get_ExpandCollapseState(&state), "read published collapsed state");
    Check(state == ExpandCollapseState_Collapsed, "embedded collapsed state agrees");
    view.Detach();
    Check(pattern->Expand() == UIA_E_ELEMENTNOTAVAILABLE, "retained disclosure provider disconnects on detach");
}

// A multi-select tree through an embedded view: pointer and key modifiers reach its gestures, and the retained UI
// Automation providers report and change its selection through the same callbacks.
class EmbeddedSelectionTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 5u;
    }

    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& outItem) const override
    {
        constexpr std::wstring_view names[] = {L"Alpha", L"Beta", L"Gamma", L"Delta", L"Epsilon"};
        outItem                             = DxUi::TreeItemData{.id = visibleIndex + 1u, .text = std::wstring(names[visibleIndex])};
    }
};

struct EmbeddedSelectionTreeDelegate final : DxUi::ITreeDelegate
{
    size_t sets = 0;
    std::vector<uint64_t> lastSet;
    void OnTreeSelectionSetChanged(std::span<const uint64_t> selectedItemIds) override
    {
        ++sets;
        lastSet.assign(selectedItemIds.begin(), selectedItemIds.end());
    }
};

static void TestEmbeddedTreeMultiSelect(GraphicsFixture& gpu)
{
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get()), "multi-select tree scene");
    auto& view    = scene.view;
    auto controls = std::make_unique<DxUi::Panel>();
    auto* tree    = controls->AddChild<DxUi::Tree>();
    tree->SetBounds(D2D1::RectF(0, 0, 300, 180));
    EmbeddedSelectionTreeModel model;
    EmbeddedSelectionTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);
    tree->SetMultiSelectEnabled(true);
    view.Controls().SetRoot(std::move(controls));
    Hr(view.Prepare(720, 510, 144), "prepare the multi-select tree");

    // Rows are 28 DIP apart from 2 DIP down; the view is prepared at 144 DPI, so physical pixels are DIPs times 1.5.
    const auto rowY  = [](size_t row) { return (2.0f + (28.0f * static_cast<float>(row)) + 14.0f) * 1.5f; };
    const auto click = [&](size_t row, UINT modifiers)
    {
        Check(view.DispatchPointer({DxUi::PointerAction::Down, 150.0f, rowY(row), modifiers}), "the tree handles a row press");
        static_cast<void>(view.DispatchPointer({DxUi::PointerAction::Up, 150.0f, rowY(row), modifiers}));
        Hr(view.Prepare(720, 510, 144), "prepare after the tree changed");
    };
    const auto selected = [&]() { return tree->GetSelectedItemIds(); };
    click(1, 0);
    Check(selected() == std::vector<uint64_t>{2u}, "an embedded click selects one row");
    click(3, MK_CONTROL);
    Check(selected() == std::vector<uint64_t>{2u, 4u}, "an embedded Ctrl+click toggles a row and moves the range anchor there");
    click(4, MK_SHIFT);
    Check(selected() == std::vector<uint64_t>{4u, 5u}, "an embedded Shift+click selects the range from the last Ctrl-toggled row");
    Check(view.DispatchKey(VK_UP, true, MK_SHIFT), "the tree handles Shift+Up");
    Check(selected() == std::vector<uint64_t>{4u}, "an embedded Shift+Up shrinks the range to its anchor");
    Check(view.DispatchKey('A', true, MK_CONTROL), "the tree handles Ctrl+A");
    Check(selected().size() == 5u && delegate.lastSet.size() == 5u, "an embedded Ctrl+A selects every row and reports it");
    Hr(view.Prepare(720, 510, 144), "prepare after Ctrl+A");
    click(2, 0);
    Check(selected() == std::vector<uint64_t>{3u}, "an embedded plain click collapses the selection");

    // The retained providers: CanSelectMultiple, the selection, and the actions that change it.
    auto site = std::make_shared<TestEmbeddedAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{0, 0, 720, 510}, true};
    Hr(view.AttachAccessibility(site, 0x4444, placement), "attach the multi-select tree's accessibility");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "multi-select tree provider root");
    site->root        = root.get();
    const auto itemAt = [&](size_t row)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        Hr(root->ElementProviderFromPoint(150.0, rowY(row), fragment.put()), "hit a tree row");
        Check(bool(fragment), "a tree row has an element");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        Hr(fragment.query_to(simple.put()), "a tree row is a simple provider");
        return simple;
    };
    const auto selectionItem = [&](const wil::com_ptr_nothrow<IRawElementProviderSimple>& item)
    {
        wil::com_ptr_nothrow<IUnknown> unknown;
        Hr(item->GetPatternProvider(UIA_SelectionItemPatternId, unknown.put()), "a tree row exposes SelectionItem");
        wil::com_ptr_nothrow<ISelectionItemProvider> pattern;
        Hr(unknown.query_to(pattern.put()), "SelectionItem pattern");
        return pattern;
    };
    const auto isSelected = [&](const wil::com_ptr_nothrow<IRawElementProviderSimple>& item)
    {
        BOOL value = FALSE;
        Hr(selectionItem(item)->get_IsSelected(&value), "IsSelected");
        return value != FALSE;
    };
    const auto row0 = itemAt(0);
    const auto row1 = itemAt(1);
    const auto row2 = itemAt(2);
    Check(! isSelected(row0) && ! isSelected(row1) && isSelected(row2), "the published snapshot reports the one selected row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> container;
    Hr(selectionItem(row0)->get_SelectionContainer(container.put()), "the tree is the selection container");
    wil::com_ptr_nothrow<IUnknown> containerPattern;
    Hr(container->GetPatternProvider(UIA_SelectionPatternId, containerPattern.put()), "the tree exposes Selection");
    wil::com_ptr_nothrow<ISelectionProvider> selection;
    Hr(containerPattern.query_to(selection.put()), "Selection pattern");
    BOOL canSelectMultiple = FALSE;
    Hr(selection->get_CanSelectMultiple(&canSelectMultiple), "CanSelectMultiple");
    Check(canSelectMultiple == TRUE, "an embedded multi-select tree reports CanSelectMultiple");

    Hr(selectionItem(row0)->AddToSelection(), "UIA adds a row to the selection");
    Check(selected() == std::vector<uint64_t>{1u, 3u} && delegate.lastSet == std::vector<uint64_t>{1u, 3u},
          "UIA AddToSelection keeps the selection and reports it once");
    Check(site->completions >= 1, "UIA selection posts application completion work");
    Hr(view.Prepare(720, 510, 144), "prepare after UIA selection");
    Hr(view.UpdateAccessibility(placement), "publish the UIA selection");
    SAFEARRAY* items = nullptr;
    Hr(selection->GetSelection(&items), "UIA selection readback");
    Check(items && items->rgsabound[0].cElements == 2, "the Selection pattern lists both selected rows");
    SafeArrayDestroy(items);
    Check(isSelected(row0) && isSelected(row2) && ! isSelected(row1), "existing row providers see the published selection");
    Hr(selectionItem(row2)->RemoveFromSelection(), "UIA removes a row from the selection");
    Check(selected() == std::vector<uint64_t>{1u}, "UIA RemoveFromSelection keeps the other rows");
    site->root = nullptr;
    view.Detach();
}

static void TestEmbeddedAccessibility(GraphicsFixture& gpu)
{
    TestEmbeddedDisclosureAccessibility(gpu);
    TestEmbeddedTreeMultiSelect(gpu);
    EmbeddedScene scene;
    Hr(scene.Initialize(gpu.device.get(), {}, true), "UIA supplied-device scene");
    auto& view = scene.view;
    auto site  = std::make_shared<TestEmbeddedAccessibilitySite>();
    DxUi::EmbeddedAccessibilityPlacement placement{{-600, 150, 720, 510}, true};
    Check(view.AttachAccessibility(site, 1, placement) == S_FALSE, "UIA attachment waits for preparation");
    Hr(view.Prepare(720, 510, 144), "UIA 144-DPI preparation");
    Check(view.AttachAccessibility(site, 0, placement) == E_INVALIDARG, "UIA identity must be nonzero");
    Hr(view.AttachAccessibility(site, 0x1234567800000001ull, placement), "UIA virtual subtree attachment");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    Hr(view.GetAccessibilityProvider(root.put()), "UIA canonical root");
    site->root                   = root.get();
    const auto allocationsBefore = allocations;
    countAllocations             = true;
    bool clean                   = true;
    for (size_t i = 0; i < 1000; ++i)
        clean = clean && view.UpdateAccessibility(placement) == S_FALSE;
    countAllocations = false;
    Check(clean && allocations == allocationsBefore, "1000 clean accessibility updates allocate nothing");
    auto invalidPlacement = placement;
    invalidPlacement.viewport.width += 1;
    Check(view.UpdateAccessibility(invalidPlacement) == E_INVALIDARG, "accessibility placement cannot rescale prepared content");

    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    Hr(root.query_to(rootSimple.put()), "UIA root simple");
    ProviderOptions options{};
    Hr(rootSimple->get_ProviderOptions(&options), "UIA provider options");
    Check((options & ProviderOptions_UseComThreading) && view.Controls().GetHwnd() == nullptr, "embedded UIA uses caller COM STA without creating a HWND");
    wil::com_ptr_nothrow<IRawElementProviderSimple> host;
    Hr(rootSimple->get_HostRawElementProvider(host.put()), "UIA virtual root host query");
    Check(! host, "virtual child never advertises an unrelated HWND host");
    wil::com_ptr_nothrow<IRawElementProviderFragment> toggle, slider, field;
    Hr(root->ElementProviderFromPoint(-540, 270, toggle.put()), "UIA toggle physical point");
    Hr(root->ElementProviderFromPoint(-540, 425, slider.put()), "UIA slider physical point");
    Hr(root->ElementProviderFromPoint(-540, 570, field.put()), "UIA text physical point");
    Check(toggle && slider && field, "UIA hit testing reaches all three controls");
    UiaRect bounds{};
    Hr(toggle->get_BoundingRectangle(&bounds), "UIA transformed toggle bounds");
    Check(std::abs(bounds.left + 564) < 0.01 && std::abs(bounds.top - 246) < 0.01 && std::abs(bounds.width - 504) < 0.01 && std::abs(bounds.height - 60) < 0.01,
          "UIA applies negative screen origin and DPI exactly once");
    wil::com_ptr_nothrow<IRawElementProviderFragment> outside;
    Hr(root->ElementProviderFromPoint(130, 425, outside.put()), "UIA out-of-viewport hit");
    Check(! outside, "UIA point outside tile cannot hit a control");
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment, parent;
    Hr(root.query_to(rootFragment.put()), "UIA root fragment");
    Hr(rootFragment->Navigate(NavigateDirection_Parent, parent.put()), "UIA app parent navigation");
    Check(site->parentRequests == 1, "UIA parent navigation uses application adapter");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fragmentRoot;
    Hr(toggle->get_FragmentRoot(fragmentRoot.put()), "UIA application fragment root");
    Check(fragmentRoot.get() == root.get(), "UIA fragment root preserves application identity");
    wil::com_ptr_nothrow<IToggleProvider> toggler;
    Hr(toggle.query_to(toggler.put()), "UIA Toggle pattern");
    ToggleState checked{};
    Hr(toggler->get_ToggleState(&checked), "UIA confirmed initial state");
    Check(checked == ToggleState_On, "UIA checked state matches retained control");
    Hr(toggler->Toggle(), "UIA toggles through normal control command");
    Check(! scene.enabled, "UIA toggle updates model once");
    Check(site->completions == 1, "UIA command posts application completion work");
    Hr(view.Prepare(720, 510, 144), "UIA prepare acknowledged toggle");
    Hr(view.UpdateAccessibility(placement), "UIA publish acknowledged toggle");
    Hr(toggler->get_ToggleState(&checked), "UIA latest retained state");
    Check(checked == ToggleState_Off, "existing UIA provider sees updated state");
    wil::com_ptr_nothrow<IRangeValueProvider> range;
    Hr(slider.query_to(range.put()), "UIA RangeValue pattern");
    size_t commits = 0;
    scene.slider->SetOnChange([&](DxUi::SliderChange change)
    {
        if (change.phase == DxUi::SliderChangePhase::Commit)
            ++commits;
        scene.intensity = change.value;
    });
    Hr(range->SetValue(42), "UIA committed slider edit");
    Check(commits == 1 && scene.intensity == 42, "UIA slider commits through model callback exactly once");
    Check(range->SetValue(101) == E_INVALIDARG && range->SetValue(std::numeric_limits<double>::quiet_NaN()) == E_INVALIDARG && commits == 1,
          "UIA invalid slider values cannot emit commands");
    Hr(view.Prepare(720, 510, 144), "UIA prepare slider");
    Hr(view.UpdateAccessibility(placement), "UIA publish slider");
    double value = 0;
    Hr(range->get_Value(&value), "UIA slider readback");
    Check(value == 42, "UIA slider readback matches latest prepared value");
    wil::com_ptr_nothrow<IValueProvider> textValue;
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    Hr(field.query_to(textValue.put()), "UIA editable Value pattern");
    Hr(field.query_to(textPattern.put()), "UIA Text pattern");
    size_t edits = 0;
    scene.text->SetOnTextChanged([&](std::wstring_view) { ++edits; });
    Hr(textValue->SetValue(L"Travel 東京"), "UIA Unicode field edit");
    Check(edits == 1 && scene.text->GetText() == L"Travel 東京", "UIA field edit follows normal model callback");
    Hr(field->SetFocus(), "UIA requests application focus");
    Check(site->focusRequests == 1 && view.Controls().GetFocusControl() == scene.text, "UIA focus routes to app and retained field");
    Hr(view.Prepare(720, 510, 144), "UIA prepare text");
    Hr(view.UpdateAccessibility(placement), "UIA publish text");
    wil::com_ptr_nothrow<ITextRangeProvider> textRange;
    Hr(textPattern->get_DocumentRange(textRange.put()), "UIA document range");
    wil::unique_bstr text;
    Hr(textRange->GetText(-1, text.put()), "UIA Unicode text read");
    Check(std::wstring_view(text.get()) == L"Travel 東京", "UIA text exposes Unicode content");
    SAFEARRAY* rectangles = nullptr;
    Hr(textRange->GetBoundingRectangles(&rectangles), "UIA text physical rectangles");
    Check(rectangles && rectangles->rgsabound[0].cElements >= 4, "UIA text ranges expose prepared geometry");
    SafeArrayDestroy(rectangles);
    wil::com_ptr_nothrow<ITextRangeProvider> hitRange;
    Hr(textPattern->RangeFromPoint({-540, 570}, hitRange.put()), "UIA text point uses physical screen geometry");
    Check(bool(hitRange), "UIA text hit returns an ACP range");
    HRESULT foreign = S_OK;
    std::thread wrongThread([&] { foreign = range->SetValue(50); });
    wrongThread.join();
    Check(foreign == RPC_E_WRONG_THREAD && commits == 1, "raw foreign-thread UIA edits are rejected");
    // Marshal a standard pattern through COM. The UI thread pumps; no private HWND dispatch lane is added.
    wil::com_ptr_nothrow<IStream> stream;
    Hr(CoMarshalInterThreadInterfaceInStream(__uuidof(IRangeValueProvider), range.get(), stream.put()), "marshal UIA pattern");
    wil::unique_event_nothrow completed;
    Hr(completed.create(wil::EventOptions::ManualReset), "UIA marshaled call event");
    HRESULT marshaled    = E_FAIL;
    IStream* ownedStream = stream.detach();
    std::thread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        wil::com_ptr_nothrow<IRangeValueProvider> proxy;
        marshaled = CoGetInterfaceAndReleaseStream(ownedStream, __uuidof(IRangeValueProvider), proxy.put_void());
        if (SUCCEEDED(marshaled))
            marshaled = proxy->SetValue(47);
        proxy.reset();
        if (SUCCEEDED(initialized))
            CoUninitialize();
        completed.SetEvent();
    });
    const auto deadline = GetTickCount64() + 5000;
    while (WaitForSingleObject(completed.get(), 0) != WAIT_OBJECT_0 && GetTickCount64() < deadline)
    {
        const HANDLE eventHandle = completed.get();
        MsgWaitForMultipleObjectsEx(1, &eventHandle, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    Check(WaitForSingleObject(completed.get(), 0) == WAIT_OBJECT_0, "marshaled UIA call completes on owning STA");
    client.join();
    Hr(marshaled, "marshaled UIA slider action");
    Check(commits == 2 && scene.intensity == 47, "COM-marshaled action commits on UI thread");
    Hr(view.Prepare(720, 510, 144), "UIA prepare marshaled change");
    placement.viewport.left = -300;
    Hr(view.UpdateAccessibility(placement), "UIA window movement");
    Hr(toggle->get_BoundingRectangle(&bounds), "UIA moved bounds");
    Check(std::abs(bounds.left + 264) < 0.01, "existing provider follows latest placement");
    placement.hasKeyboardFocus = false;
    Hr(view.UpdateAccessibility(placement), "UIA OS focus loss");
    wil::com_ptr_nothrow<IRawElementProviderFragment> focused;
    Hr(root->GetFocus(focused.put()), "UIA focus after OS loss");
    Check(! focused, "logical field focus does not claim OS focus");
    site->root = nullptr;
    view.SetVisible(false);
    Check(toggler->Toggle() == UIA_E_ELEMENTNOTAVAILABLE && range->SetValue(20) == UIA_E_ELEMENTNOTAVAILABLE &&
              textRange->GetText(-1, text.put()) == UIA_E_ELEMENTNOTAVAILABLE,
          "surviving providers disconnect on hide");
    view.SetVisible(true);
    Hr(view.Prepare(720, 510, 144), "UIA re-show prepare");
    Hr(view.AttachAccessibility(site, 0x1234567800000002ull, placement), "UIA new attachment identity");
    Check(toggler->Toggle() == UIA_E_ELEMENTNOTAVAILABLE, "old provider cannot address a reattached view");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> nextRoot;
    Hr(view.GetAccessibilityProvider(nextRoot.put()), "UIA next root");
    Check(nextRoot.get() != root.get(), "reattachment uses a new canonical provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> oldControl;
    Hr(nextRoot->ElementProviderFromPoint(-240, 270, oldControl.put()), "UIA control before same-path replacement");
    Check(bool(oldControl), "UIA old control exists");
    using Array = wil::unique_any<SAFEARRAY*, decltype(&SafeArrayDestroy), SafeArrayDestroy>;
    Array oldId, newId;
    Hr(oldControl->GetRuntimeId(oldId.put()), "UIA old control runtime identity");
    auto* panel = dynamic_cast<DxUi::Panel*>(view.Controls().GetRoot());
    Check(panel != nullptr, "UIA fixture panel");
    panel->ClearChildren();
    panel->AddChild<DxUi::Label>(L"Replacement")->SetBounds(D2D1::RectF(24, 16, 430, 50));
    auto* replacement = panel->AddChild<DxUi::Toggle>(L"Replacement toggle");
    replacement->SetBounds(D2D1::RectF(24, 64, 360, 104));
    size_t replacementCommands = 0;
    replacement->SetOnToggled([&](bool) { ++replacementCommands; });
    wil::com_ptr_nothrow<IToggleProvider> staleToggle;
    Check(oldControl.query_to(staleToggle.put()) == E_NOINTERFACE, "removed control cannot acquire a new pattern");
    Check(oldControl->SetFocus() == UIA_E_ELEMENTNOTAVAILABLE, "removed control cannot focus its same-path replacement");
    Hr(view.Prepare(720, 510, 144), "UIA prepare same-path replacement");
    Hr(view.UpdateAccessibility(placement), "UIA publish same-path replacement");
    wil::com_ptr_nothrow<IRawElementProviderFragment> newControl;
    Hr(nextRoot->ElementProviderFromPoint(-240, 270, newControl.put()), "UIA replacement control");
    Check(bool(newControl), "UIA replacement control exists");
    Hr(newControl->GetRuntimeId(newId.put()), "UIA replacement runtime identity");
    bool different = false;
    for (LONG i = 0; i < static_cast<LONG>(oldId.get()->rgsabound[0].cElements); ++i)
    {
        LONG a = 0, b = 0;
        Hr(SafeArrayGetElement(oldId.get(), &i, &a), "UIA old runtime element");
        Hr(SafeArrayGetElement(newId.get(), &i, &b), "UIA new runtime element");
        different = different || a != b;
    }
    Check(different && replacementCommands == 0, "same-path replacement changes runtime identity without activation");
    wil::com_ptr_nothrow<IToggleProvider> replacementToggle;
    Hr(newControl.query_to(replacementToggle.put()), "UIA replacement toggle pattern");
    Hr(replacementToggle->Toggle(), "UIA replacement toggle activation");
    Check(replacementCommands == 1, "new provider targets only the new control");
    replacement->SetVisible(false);
    Check(replacementToggle->Toggle() == UIA_E_NOTSUPPORTED && replacementCommands == 1, "hidden control cannot be activated from its old snapshot");
    view.Controls().SetRoot(std::make_unique<DxUi::Panel>());
    Check(nextRoot->GetFocus(focused.put()) == UIA_E_ELEMENTNOTAVAILABLE, "root replacement invalidates providers immediately");
    view.Detach();
}
