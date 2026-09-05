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

static void TestEmbeddedAccessibility(GraphicsFixture& gpu)
{
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
