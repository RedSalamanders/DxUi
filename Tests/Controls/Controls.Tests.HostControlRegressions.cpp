#include "Controls.Tests.HostControlRegressions.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Embedded.Tests.EmbeddedUiaBridge.h"
#include "GraphicsFixture.h"
#include "Support.Tests.UiaTestClient.h"

#include <thread>

namespace
{
std::shared_ptr<DxUi::GraphicsDevice> MakePool(GraphicsFixture& gpu)
{
    RequireSucceeded(gpu.Create(), "create regression WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    RequireSucceeded(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "share regression graphics");
    return graphics;
}

UiaTest::ElementId FindNamed(UiaTest::Client& client, UiaTest::ElementId parent, std::wstring_view name, size_t depth = 0u)
{
    Require(depth < 12u, "paged client walk stays bounded");
    for (const auto child : client.Children(parent))
    {
        if (client.Describe(child).name == name)
            return child;
        if (const auto found = FindNamed(client, child, name, depth + 1u))
            return found;
    }
    return 0u;
}

void CheckPagedUia(bool embedded)
{
    using namespace DxUi;
    GraphicsFixture gpu;
    EmbeddedHost view;
    std::unique_ptr<AttachedHostWindow> window;
    std::unique_ptr<EmbeddedUiaTest::Bridge> bridge;
    ControlHost* host = nullptr;
    if (embedded)
    {
        RequireSucceeded(view.Attach(MakePool(gpu)), "attach paged embedded view");
        host = &view.Controls();
    }
    else
    {
        window = std::make_unique<AttachedHostWindow>();
        host   = &window->Host();
    }
    host->SetTheme(MakeAnimatedTestThemePalette(false));
    auto root   = std::make_unique<Panel>();
    auto* pages = root->AddChild<PageHost>();
    pages->SetBounds(D2D1::RectF(0, 0, 220, 150));
    auto* tail = root->AddChild<Button>(L"Outside page");
    tail->SetBounds(D2D1::RectF(230, 12, 300, 48));
    host->SetRoot(std::move(root));
    pages->SetPage(std::make_unique<Button>(L"Outgoing page action"));
    auto page    = std::make_unique<Panel>();
    auto* action = page->AddChild<Button>(L"Current page action");
    action->SetBounds(D2D1::RectF(12, 12, 190, 48));
    unsigned invocations = 0u;
    action->SetOnClick([&] { ++invocations; });
    auto* label = page->AddChild<Label>(L"Page editor label");
    label->SetBounds(D2D1::RectF(12, 52, 190, 76));
    auto* editor = page->AddChild<TextField>();
    editor->SetBounds(D2D1::RectF(12, 80, 190, 120));
    label->SetMnemonicTarget(editor);
    pages->SetPage(std::move(page));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    if (embedded)
    {
        RequireSucceeded(view.Prepare(320, 180), "prepare paged view");
        bridge = std::make_unique<EmbeddedUiaTest::Bridge>(view, 320, 180);
        RequireSucceeded(bridge->Attach(false), "attach paged application's UIA bridge");
        RequireSucceeded(view.GetAccessibilityProvider(provider.put()), "get paged embedded provider");
    }
    else
        provider.attach(CreateWindowHostAccessibilityProvider(window->Hwnd()));
    Require(provider != nullptr, "paged host publishes a root provider");
    const auto publish = [&]
    {
        if (embedded)
        {
            RequireSucceeded(view.Prepare(320, 180), "prepare current paged tree");
            RequireSucceeded(bridge->Update(), "publish current paged tree");
        }
        else
            host->RefreshAccessibilitySnapshot();
    };
    publish();
    UiaTest::Client client(embedded ? bridge->Hwnd() : window->Hwnd(),
                           {},
                           [&]
    {
        if (embedded)
            bridge->Pump();
        else
            window->PumpMessages();
    });
    const auto actionId = FindNamed(client, client.Root(), L"Current page action");
    Require(actionId != 0u, "real UIA client reaches the current page action through the window");
    Require(FindNamed(client, client.Root(), L"Outgoing page action") == 0u, "outgoing visual page is absent from logical accessibility");
    Require(FindNamed(client, client.Root(), L"Page editor label") != 0u, "paged labels participate in semantic navigation");
    const auto parent = client.Navigate(actionId, UiaTest::Direction::Parent);
    Require(parent.has_value(), "paged action navigates back to a live semantic parent");
    bool foundAgain = false;
    for (const auto child : client.Children(*parent))
        foundAgain = foundAgain || client.Same(child, actionId);
    Require(foundAgain, "paged action's parent enumerates that same child identity");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    if (embedded)
        RequireSucceeded(provider->ElementProviderFromPoint(-31970, -31970, fragment.put()), "hit current embedded page action");
    else
        fragment = GetProviderAtDipPoint(window->Hwnd(), *host, *provider.get(), 30, 30, "hit current native page action");
    Require(fragment != nullptr, "page hit testing reaches its semantic child");
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(fragment.query_to(simple.put()), "get paged action simple provider");
    Require(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "paged hit name") == L"Current page action", "hit resolves the current logical path");
    wil::com_ptr_nothrow<IUnknown> pattern;
    RequireSucceeded(simple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "get paged invoke pattern");
    Require(pattern != nullptr, "paged action exposes invoke");
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    RequireSucceeded(pattern.query_to(invoke.put()), "query paged invoke");
    RequireSucceeded(invoke->Invoke(), "invoke through paged path resolution");
    Require(invocations == 1u, "paged invoke reaches the current control exactly once");
    wil::com_ptr_nothrow<IRawElementProviderFragment> editorFragment;
    if (embedded)
        RequireSucceeded(provider->ElementProviderFromPoint(-31970, -31900, editorFragment.put()), "hit embedded page editor");
    else
        editorFragment = GetProviderAtDipPoint(window->Hwnd(), *host, *provider.get(), 30, 100, "hit native page editor");
    Require(editorFragment != nullptr, "page editor is hit-testable");
    wil::com_ptr_nothrow<IRawElementProviderSimple> editorSimple;
    RequireSucceeded(editorFragment.query_to(editorSimple.put()), "query paged editor");
    Require(ReadProviderStringProperty(*editorSimple.get(), UIA_NamePropertyId, "paged editor label") == L"Page editor label",
            "logical-child label lookup names the page editor");
    pages->SetVisible(false);
    publish();
    Require(FindNamed(client, client.Root(), L"Current page action") == 0u, "hidden page descendants disappear from the client tree");
    pages->SetVisible(true);
    pages->SetPage(std::make_unique<Button>(L"Replacement page action"));
    publish();
    Require(FindNamed(client, client.Root(), L"Replacement page action") != 0u, "replacement page is client-reachable");
    VARIANT value{};
    const auto clear = wil::scope_exit([&] { VariantClear(&value); });
    Require(simple->GetPropertyValue(UIA_NamePropertyId, &value) == UIA_E_ELEMENTNOTAVAILABLE, "retained old page provider disconnects rather than rebinding");
}
} // namespace

void TestPageHostWindowUiaTraversesInvokesAndRetiresPageChildren()
{
    CheckPagedUia(false);
}

void TestPageHostEmbeddedUiaTraversesInvokesAndRetiresPageChildren()
{
    std::jthread owner([]
    {
        RequireSucceeded(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "initialize paged application STA");
        const auto apartment = wil::scope_exit([] { CoUninitialize(); });
        DxUi::TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "paged application refuses activation");
        CheckPagedUia(true);
    });
}

void TestPageHostTabTraversalUsesCurrentLogicalChildren()
{
    using namespace DxUi;
    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* first = root->AddChild<Button>(L"First");
    auto* pages = root->AddChild<PageHost>();
    auto* last  = root->AddChild<Button>(L"Last");
    host.SetRoot(std::move(root));
    pages->SetPage(std::make_unique<Button>(L"Outgoing"));
    auto page    = std::make_unique<Panel>();
    auto* hidden = page->AddChild<Button>(L"Hidden");
    hidden->SetVisible(false);
    auto* disabled = page->AddChild<Button>(L"Disabled");
    disabled->SetEnabled(false);
    auto* current = page->AddChild<Button>(L"Current");
    pages->SetPage(std::move(page));
    host.SetFocusControl(first, false);
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && host.GetFocusControl() == current, "Tab enters the current logical page and skips unavailable children");
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && host.GetFocusControl() == last, "Tab exits the current page");
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && host.GetFocusControl() == current, "reverse Tab enters the same current page");
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
    pages->SetEnabled(false);
    host.SetFocusControl(first, false);
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && host.GetFocusControl() == last, "disabled page excludes its whole focus subtree");

    GraphicsFixture gpu;
    EmbeddedHost view;
    RequireSucceeded(view.Attach(MakePool(gpu)), "attach paged keyboard view");
    auto embeddedRoot   = std::make_unique<Panel>();
    auto* embeddedPages = embeddedRoot->AddChild<PageHost>();
    embeddedPages->SetBounds(D2D1::RectF(0, 0, 220, 120));
    embeddedPages->SetPage(std::make_unique<Button>(L"Embedded page action"));
    auto* embeddedAction = embeddedPages->GetPage();
    auto* embeddedTail   = embeddedRoot->AddChild<Button>(L"Embedded tail");
    embeddedTail->SetBounds(D2D1::RectF(230, 12, 310, 48));
    view.Controls().SetRoot(std::move(embeddedRoot));
    RequireSucceeded(view.Prepare(320, 180), "prepare paged keyboard view");
    Require(view.DispatchKey(VK_TAB, true) && view.Controls().GetFocusControl() == embeddedAction, "embedded Tab also enters the current logical page");
    Require(view.DispatchKey(VK_TAB, true) && view.Controls().GetFocusControl() == embeddedTail, "embedded Tab exits the page");
    Require(view.DispatchKey(VK_TAB, true, MK_SHIFT) && view.Controls().GetFocusControl() == embeddedAction, "embedded reverse Tab returns to the page");
}

void TestEmbeddedTransitionsUseApplicationClockIncludingZero()
{
    using namespace DxUi;
    GraphicsFixture gpu;
    auto pool = MakePool(gpu);
    for (const uint64_t epoch : {0ull, 1000000000000ull})
    {
        EmbeddedHost view;
        RequireSucceeded(view.Attach(pool), "attach application-clock view");
        auto& host = view.Controls();
        host.SetTheme(MakeAnimatedTestThemePalette(false));
        auto slider = std::make_unique<Slider>();
        auto* live  = slider.get();
        slider->SetValue(20);
        host.SetRoot(std::move(slider));
        RequireSucceeded(view.Prepare(220, 48), "prepare application-clock slider");
        host.SetFocusControl(live, false);
        Require(view.DispatchKey(VK_RIGHT, true), "embedded key starts animated seek");
        Require(view.AdvanceAnimation(epoch), "first application tick anchors active slider animations");
        RequireFloatNear(static_cast<float>(live->DebugGetDisplayedValue()), 20, 0.001f, "first tick does not mix application and system epochs");
        Require(view.AdvanceAnimation(epoch + 40), "application-clock slider advances");
        Require(live->DebugGetDisplayedValue() > 20 && live->DebugGetDisplayedValue() < live->GetValue(), "slider interpolates inside the application epoch");
        static_cast<void>(view.AdvanceAnimation(epoch + 1000));
        static_cast<void>(view.AdvanceAnimation(epoch + 1001));
        Require(! view.NeedsAnimation() && live->DebugGetDisplayedValue() == live->GetValue(), "slider reaches its target and releases animation");
        // Each attachment has its own monotonic application timeline, with the same independently tested epoch.
        EmbeddedHost pageView;
        RequireSucceeded(pageView.Attach(pool), "attach independent application-clock pages");
        pageView.Controls().SetTheme(MakeAnimatedTestThemePalette(false));
        auto pages     = std::make_unique<PageHost>();
        auto* pageHost = pages.get();
        pageView.Controls().SetRoot(std::move(pages));
        pageHost->SetPage(std::make_unique<Button>(L"First page"));
        pageHost->SetPage(std::make_unique<Button>(L"Second page"));
        RequireSucceeded(pageView.Prepare(220, 100), "prepare application-clock pages");
        Require(pageView.AdvanceAnimation(epoch), "first page tick anchors its clock");
        Require(pageHost->HasActiveTransition() && pageHost->DebugGetTransitionState(epoch).linearProgress == 0.0f,
                "first page tick neither wraps nor fast-forwards");
        Require(pageView.AdvanceAnimation(epoch + 125), "page progresses in application time");
        const auto progress = pageHost->DebugGetTransitionState(epoch + 125).linearProgress;
        Require(progress > 0 && progress < 1, "page has intermediate application-clock progress");
        static_cast<void>(pageView.AdvanceAnimation(epoch + 1000));
        static_cast<void>(pageView.AdvanceAnimation(epoch + 1001));
        Require(! pageHost->HasActiveTransition() && ! pageView.NeedsAnimation(), "page settles and releases animation");
    }
}

void TestNativeCaretTicksInvalidateOnlyWhenPixelsChange()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto field   = std::make_unique<TextField>(L"Caret");
    auto* editor = field.get();
    window.Host().SetRoot(std::move(field));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.Host().SetFocusControl(editor, false);
    const uint64_t tick = GetTickCount64();
    Require(window.Host().DebugAnimationTickForTest(tick), "establish the caret's current native blink phase");
    const auto before = window.Host().DebugGetInvalidateCount();
    Require(window.Host().DebugAnimationTickForTest(tick), "focused caret requests future ticks");
    Require(window.Host().DebugAnimationTickForTest(tick), "an unchanged timestamp remains in the same blink phase");
    Require(window.Host().DebugGetInvalidateCount() == before, "unchanged caret ticks do not invalidate the native window");
    Require(window.Host().DebugAnimationTickForTest(tick + 530), "caret phase flip keeps animation active");
    Require(window.Host().DebugGetInvalidateCount() > before, "caret phase flip explicitly invalidates its pixels");
    const auto after = window.Host().DebugGetInvalidateCount();
    static_cast<void>(window.Host().DebugAnimationTickForTest(tick + 530));
    Require(window.Host().DebugGetInvalidateCount() == after, "next unchanged phase remains clean");
    window.Host().SetFocusControl(nullptr, false);
    Require(! window.Host().DebugAnimationTickForTest(tick + 550), "blur retires the caret subscription");
}

void TestScrollPanelForwardsSecondaryPressInContentCoordinates()
{
    using namespace DxUi;
    for (const bool nestedPage : {false, true})
    {
        unsigned menus = 0u, clicks = 0u;
        POINT anchor{};
        bool keyboardInvocation = false;
        WindowHost host;
        auto root          = std::make_unique<ScrollPanel>();
        auto* scroll       = root.get();
        ScrollPanel* inner = nullptr;
        Button* button     = nullptr;
        if (nestedPage)
        {
            auto* pages = root->AddChild<PageHost>();
            pages->SetBounds(D2D1::RectF(0, 100, 220, 220));
            auto page = std::make_unique<ScrollPanel>();
            inner     = page.get();
            button    = page->AddChild<Button>(L"Nested scrolled action");
            button->SetBounds(D2D1::RectF(12, 160, 180, 200));
            pages->SetPage(std::move(page));
        }
        else
        {
            button = root->AddChild<Button>(L"Scrolled action");
            button->SetBounds(D2D1::RectF(12, 100, 180, 140));
        }
        button->SetOnContextMenu([&](POINT point, bool keyboard)
        {
            anchor             = point;
            keyboardInvocation = keyboard;
            ++menus;
        });
        button->SetOnClick([&] { ++clicks; });
        host.SetRoot(std::move(root));
        scroll->SetBounds(D2D1::RectF(0, 0, 220, 160));
        scroll->SetContentHeight(400);
        scroll->SetScrollOffset(90);
        if (inner)
        {
            inner->SetContentHeight(400);
            inner->SetScrollOffset(25);
        }
        const LONG y = nestedPage ? 60 : 30;
        bool handled = false;
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, 0, MAKELPARAM(30, y), handled));
        Require(handled, "secondary press reaches the scrolled child through native routing");
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONUP, 0, MAKELPARAM(30, y), handled));
        Require(menus == 1u && clicks == 0u && ! keyboardInvocation, "scrolled secondary click invokes one context action without primary activation");
        RequirePointNear(anchor, POINT{30, y}, "pointer context anchor maps back through every scroll ancestor and PageHost");
        anchor = {};
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDBLCLK, 0, MAKELPARAM(30, y), handled));
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONUP, 0, MAKELPARAM(30, y), handled));
        Require(menus == 2u && clicks == 0u && ! keyboardInvocation, "a second secondary press within the double-click time is a press like the first");
        RequirePointNear(anchor, POINT{30, y}, "the second secondary press maps through every scroll ancestor too");
        Require(button->OnContextMenu(host, true, D2D1::Point2F()), "keyboard context invocation reaches the same child");
        Require(menus == 3u && clicks == 0u && keyboardInvocation, "keyboard context invocation preserves primary state");
        RequirePointNear(anchor, POINT{28, nestedPage ? 65 : 30}, "keyboard anchor also maps through all scroll ancestors");
        Require(scroll->GetScrollOffset() == 90, "secondary input does not operate the scrollbar");
    }
}

void TestScrollPanelModelContextMenusUseScreenCoordinates()
{
    using namespace DxUi;
    for (const bool gridCase : {false, true})
    {
        MutableTreeModel treeModel;
        treeModel.SetVisibleItems(
            {TreeItemData{.id = 10u, .text = L"First"}, TreeItemData{.id = 20u, .text = L"Second"}, TreeItemData{.id = 30u, .text = L"Third"}});
        RecordingTreeDelegate treeDelegate;
        MultiRowGridModel gridModel(3u);
        RecordingGridDelegate gridDelegate;
        WindowHost host;
        auto root       = std::make_unique<ScrollPanel>();
        auto* scroll    = root.get();
        Control* target = nullptr;
        if (gridCase)
        {
            auto* grid = root->AddChild<Grid>();
            grid->SetBounds(D2D1::RectF(0, 100, 320, 260));
            grid->SetModel(&gridModel);
            grid->SetDelegate(&gridDelegate);
            target = grid;
        }
        else
        {
            auto* tree = root->AddChild<Tree>();
            tree->SetBounds(D2D1::RectF(0, 100, 220, 220));
            tree->SetModel(&treeModel);
            tree->SetDelegate(&treeDelegate);
            target = tree;
        }
        host.SetRoot(std::move(root));
        scroll->SetBounds(D2D1::RectF(0, 0, 340, 160));
        scroll->SetContentHeight(400);
        scroll->SetScrollOffset(90);
        const POINT pointer = gridCase ? POINT{48, 84} : POINT{60, 54};
        bool handled        = false;
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, 0, MAKELPARAM(pointer.x, pointer.y), handled));
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONUP, 0, MAKELPARAM(pointer.x, pointer.y), handled));
        const auto readAnchor = [&] { return gridCase ? gridDelegate.lastContextMenuPoint : treeDelegate.lastContextMenuPoint; };
        Require((gridCase ? gridDelegate.contextMenuCount : treeDelegate.contextMenuCount) == 1u, "scrolled model menu opens once");
        RequirePointNear(readAnchor(), pointer, "model delegate receives the actual pointer screen location");
        scroll->SetScrollOffset(0);
        Require(target->OnContextMenu(host, true, D2D1::Point2F()), "unscrolled selected-row keyboard anchor is available");
        const POINT unscrolled = readAnchor();
        scroll->SetScrollOffset(90);
        Require(target->OnContextMenu(host, true, D2D1::Point2F()), "scrolled selected-row keyboard anchor is available");
        RequirePointNear(readAnchor(), POINT{unscrolled.x, unscrolled.y - 90}, "model keyboard anchor follows the outer viewport transform");
    }
}

// A second button pressed and released while a slider's thumb is held belongs to that drag, scrolled or not: the drag
// keeps the capture, follows the pointer off the slider and commits once on its own release. The second release used to
// end the capture, and in a ScrollPanel the second press handed the gesture to the control under the pointer, so the
// slider never saw its release and went on dragging under a plain hover.
void TestASecondButtonDuringADragLeavesTheDragToItsControl()
{
    using namespace DxUi;
    for (const bool scrolled : {false, true})
    {
        WindowHost host;
        auto root        = std::make_unique<Panel>();
        Panel* rootPanel = root.get();
        Panel* parent    = rootPanel;
        if (scrolled)
        {
            auto* scroll = root->AddChild<ScrollPanel>();
            scroll->SetBounds(D2D1::RectF(0, 0, 300, 200));
            scroll->SetContentHeight(400);
            parent = scroll;
        }
        auto* slider = parent->AddChild<Slider>();
        slider->SetBounds(D2D1::RectF(20, 20, 220, 68));
        slider->SetMinimum(0.0);
        slider->SetMaximum(100.0);
        slider->SetValue(0.0);
        unsigned commits = 0u, cancels = 0u, menus = 0u;
        slider->SetOnChange([&](SliderChange change)
        {
            commits += change.phase == SliderChangePhase::Commit ? 1u : 0u;
            cancels += change.phase == SliderChangePhase::Cancel ? 1u : 0u;
        });
        auto* button = parent->AddChild<Button>(L"Has a menu");
        button->SetBounds(D2D1::RectF(20, 100, 220, 140));
        button->SetOnContextMenu([&](POINT, bool) { ++menus; });
        host.SetRoot(std::move(root));
        rootPanel->SetBounds(D2D1::RectF(0, 0, 320, 240));

        const D2D1_RECT_F thumb = slider->DebugGetThumbRect();
        const LONG thumbX       = static_cast<LONG>((thumb.left + thumb.right) * 0.5f);
        const LONG thumbY       = static_cast<LONG>((thumb.top + thumb.bottom) * 0.5f);
        bool handled            = false;
        static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(thumbX, thumbY), handled));
        static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(thumbX, thumbY), handled));
        Require(host.GetFocusControl() == slider, "the pressed slider is focused");
        static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(120, 120), handled));
        const double overButton = slider->GetValue();
        Require(overButton > 0.0, "the drag follows the pointer over another control");
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, MK_LBUTTON | MK_RBUTTON, MAKELPARAM(120, 120), handled));
        static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONUP, MK_LBUTTON, MAKELPARAM(120, 120), handled));
        Require(menus == 0u, "a second button during the drag opens no menu of the control under the pointer");
        Require(host.GetFocusControl() == slider, "a second button during the drag keeps the dragged slider focused");
        static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(200, 170), handled));
        Require(slider->GetValue() > overButton, "the drag still follows the pointer after the second button's release");
        static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(200, 170), handled));
        Require(commits == 1u && cancels == 0u, "the drag commits once on its own release");
        Require(host.GetCapturedControl() == nullptr, "the drag's release ends the capture");
        const double committed = slider->GetValue();
        static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(60, 44), handled));
        Require(slider->GetValue() == committed && commits == 1u, "a later hover over the slider drags nothing");
    }
}

// The second click of a double-click on a ScrollPanel's track pages again, as on a native scrollbar, and leaves the
// focus where it was. It used to reach the panel as a double-click nothing handled, which lost the page and cleared the
// focus.
void TestScrollPanelScrollbarDoubleClickPagesAgain()
{
    using namespace DxUi;
    WindowHost host;
    auto root        = std::make_unique<Panel>();
    Panel* rootPanel = root.get();
    auto* scroll     = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0, 0, 300, 200));
    scroll->SetContentHeight(1000);
    auto* field = scroll->AddChild<TextField>(L"Focused");
    field->SetBounds(D2D1::RectF(10, 10, 200, 40));
    host.SetRoot(std::move(root));
    rootPanel->SetBounds(D2D1::RectF(0, 0, 320, 240));
    host.SetFocusControl(field);

    D2D1_RECT_F thumb{};
    Require(scroll->DebugGetScrollbarThumbHitRect(thumb), "the panel shows a scrollbar");
    const LONG trackX     = static_cast<LONG>((thumb.left + thumb.right) * 0.5f);
    constexpr LONG trackY = 190;
    bool handled          = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(trackX, trackY), handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(trackX, trackY), handled));
    const float afterFirst = scroll->GetScrollOffset();
    Require(afterFirst > 0.0f, "a click on the track below the thumb pages down");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(trackX, trackY), handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(trackX, trackY), handled));
    Require(scroll->GetScrollOffset() > afterFirst, "the double-click's second click pages again");
    Require(host.GetFocusControl() == field, "clicks on the scrollbar leave the focus where it was");
}

// A right press that focuses a control inside a ScrollPanel without opening a menu leaves it focused, as it does outside
// one. The host compared the press target (the panel) with the focused control and blurred the control again.
void TestScrollPanelKeepsTheFocusAChildTakesOnAnUnhandledPress()
{
    using namespace DxUi;
    WindowHost host;
    auto root        = std::make_unique<Panel>();
    Panel* rootPanel = root.get();
    auto* elsewhere  = root->AddChild<Button>(L"Elsewhere");
    elsewhere->SetBounds(D2D1::RectF(0, 210, 100, 236));
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0, 0, 300, 200));
    scroll->SetContentHeight(400);
    auto* button = scroll->AddChild<Button>(L"Has no menu");
    button->SetBounds(D2D1::RectF(10, 10, 200, 40));
    host.SetRoot(std::move(root));
    rootPanel->SetBounds(D2D1::RectF(0, 0, 320, 240));
    host.SetFocusControl(elsewhere);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(50, 25), handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONUP, 0, MAKELPARAM(50, 25), handled));
    Require(host.GetFocusControl() == button, "the right-pressed control in the panel keeps the focus it took");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(250, 150), handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(250, 150), handled));
    Require(host.GetFocusControl() == nullptr, "a press on the panel's empty space still clears the focus");
}

// A keyboard menu of a control scrolled out of view opens inside the visible viewport, at the reading-order start of the
// control: the right edge in a right-to-left layout.
void TestKeyboardContextAnchorsStayInsideTheScrollViewport()
{
    using namespace DxUi;
    for (const bool rightToLeft : {false, true})
    {
        WindowHost host;
        auto root        = std::make_unique<Panel>();
        Panel* rootPanel = root.get();
        if (rightToLeft)
            root->SetFlowDirection(FlowDirection::RightToLeft);
        auto* scroll = root->AddChild<ScrollPanel>();
        scroll->SetBounds(D2D1::RectF(0, 0, 220, 160));
        scroll->SetContentHeight(400);
        auto* button = scroll->AddChild<Button>(L"Below the fold");
        button->SetBounds(D2D1::RectF(12, 300, 180, 340));
        POINT anchor{};
        button->SetOnContextMenu([&](POINT point, bool) { anchor = point; });
        host.SetRoot(std::move(root));
        rootPanel->SetBounds(D2D1::RectF(0, 0, 240, 200));

        Require(button->OnContextMenu(host, true, D2D1::Point2F()), "keyboard menu of the hidden control opens");
        const D2D1_RECT_F viewport = scroll->GetViewportRect();
        Require(static_cast<float>(anchor.y) <= viewport.bottom && static_cast<float>(anchor.y) >= viewport.top,
                "the keyboard anchor of a control below the viewport is clamped into it");
        RequirePointNear(anchor, POINT{rightToLeft ? 164 : 28, 160}, "the anchor keeps the control's reading-order start");
        scroll->SetScrollOffset(200);
        Require(button->OnContextMenu(host, true, D2D1::Point2F()), "keyboard menu of the shown control opens");
        RequirePointNear(anchor, POINT{rightToLeft ? 164 : 28, 120}, "a visible control keeps its own anchor");
    }
}

void TestNativeUnhandledKeysReachApplicationFallback()
{
    using namespace DxUi;
    AttachedHostWindow window;
    unsigned fallback = 0u;
    window.SetApplicationMessageHandler([&](UINT message, WPARAM key, LPARAM, LRESULT& result)
    {
        if ((message != WM_KEYDOWN && message != WM_SYSKEYDOWN) || key != VK_F6)
            return false;
        ++fallback;
        result = 123;
        return true;
    });
    auto button  = std::make_unique<Button>(L"Action");
    auto* action = button.get();
    window.Host().SetRoot(std::move(button));
    window.Host().SetFocusControl(action, false);
    Require(SendMessageW(window.Hwnd(), WM_KEYDOWN, VK_F6, 0) == 123, "unhandled ordinary key reaches application fallback");
    Require(SendMessageW(window.Hwnd(), WM_SYSKEYDOWN, VK_F6, 0) == 123 && fallback == 2u, "unhandled system key reaches application fallback once");
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "control-consumed key remains handled");

    // A focused editor takes the characters of printable keys; their key-downs are not the application's as well, while
    // its other keys still are.
    auto field   = std::make_unique<TextField>(L"");
    auto* editor = field.get();
    window.Host().SetRoot(std::move(field));
    window.Host().SetFocusControl(editor, false);
    unsigned typedFallback = 0u;
    unsigned otherFallback = 0u;
    window.SetApplicationMessageHandler([&](UINT message, WPARAM key, LPARAM, LRESULT& result)
    {
        if (message != WM_KEYDOWN)
            return false;
        if (key != VK_F6)
        {
            ++typedFallback;
            return false;
        }
        ++otherFallback;
        result = 123;
        return true;
    });
    for (const UINT key : {static_cast<UINT>('A'), static_cast<UINT>(VK_SPACE), static_cast<UINT>(VK_OEM_PERIOD)})
        static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYDOWN, key, 0));
    Require(typedFallback == 0u, "the application's fallback sees none of the keys typed into the focused editor");
    Require(SendMessageW(window.Hwnd(), WM_KEYDOWN, VK_F6, 0) == 123 && otherFallback == 1u,
            "a key the editor neither types nor handles still reaches the application");
}

void TestExtractedRadioDoesNotUseItsFormerGroup()
{
    using namespace DxUi;
    for (const bool destroyGroup : {false, true})
    {
        unsigned groupCalls = 0u, selected = 0u;
        auto group = std::make_unique<RadioButtons>();
        group->SetOnSelectionChanged([&](int) { ++groupCalls; });
        auto* radio = group->AddItem(L"Extracted");
        radio->SetOnSelected([&] { ++selected; });
        auto taken = group->TakeChild(0);
        Require(taken.get() == radio, "radio extraction transfers ownership");
        if (destroyGroup)
            group.reset();
        WindowHost host;
        host.SetRoot(std::move(taken));
        Require(radio->OnKeyDown(host, VK_SPACE, 0), "retained radio remains selectable after extraction");
        Require(radio->IsChecked() && selected == 1u && groupCalls == 0u, "extracted radio selects independently of its live or destroyed former group");
    }
}

void TestEmbeddedGraphicsRetirementPreservesApplicationBindings()
{
    using namespace DxUi;
    GraphicsFixture first, second;
    auto oldPool = MakePool(first);
    auto newPool = MakePool(second);
    EmbeddedHost view;
    RequireSucceeded(view.Attach(oldPool), "attach borrowed-context view");
    view.Controls().SetRoot(std::make_unique<Button>(L"Device recovery"));
    RequireSucceeded(view.Prepare(220, 80), "prepare old-generation surface");
    const auto bind = [](GraphicsFixture& gpu)
    {
        gpu.Bind();
        gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    };
    const auto preserved = [](GraphicsFixture& gpu)
    {
        D3D11_PRIMITIVE_TOPOLOGY topology{};
        gpu.context->IAGetPrimitiveTopology(&topology);
        wil::com_ptr_nothrow<ID3D11RenderTargetView> target;
        gpu.context->OMGetRenderTargets(1, target.put(), nullptr);
        Require(topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP && target.get() == gpu.rtv.get(),
                "retirement preserves application topology and render target bindings");
    };
    bind(first);
    RequireSucceeded(view.ReplaceDevice(newPool), "replace supplied graphics generation");
    preserved(first);
    RequireSucceeded(view.Prepare(220, 80), "recover new-generation surface");
    bind(second);
    view.Detach();
    preserved(second);
}
