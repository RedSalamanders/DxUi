#include "../../src/Controls/DxUi.AccessibilityTextUnits.h"
#include "../Support/Support.Tests.SelectionEventInterruption.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.GridMultilineFixtures.h"

#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iterator>
#include <numeric>
#include <thread>
#include <wrl/implements.h>

namespace
{

class DisclosureChangeObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                           IUIAutomationPropertyChangedEventHandler,
                                                                           Microsoft::WRL::FtmBase>
{
public:
    std::atomic<unsigned int> changes{0};
    std::atomic<LONG> state{ExpandCollapseState_LeafNode};
    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement*, PROPERTYID property, VARIANT newValue) noexcept override
    {
        if (property == UIA_ExpandCollapseExpandCollapseStatePropertyId && newValue.vt == VT_I4)
        {
            state.store(newValue.lVal);
            changes.fetch_add(1);
        }
        return S_OK;
    }
};

void TestDisclosureNotifiesNativeAutomationClient()
{
    using namespace DxUi;
    // Cold in-process UIA client setup takes about 0.1 s locally but has exceeded 3 s on hosted x64 runners.
    // Setup gets its own bounded allowance; notification and unsubscribe checks keep their 3000 ms deadlines.
    constexpr ULONGLONG kClientSetupAllowanceMs = 20000;
    constexpr ULONGLONG kNotificationDeadlineMs = 3000;
    AttachedHostWindow window;
    auto root    = std::make_unique<Button>(L"Afficher les détails");
    auto* button = root.get();
    button->SetBounds(D2D1::RectF(0, 0, 280, 40));
    button->SetDisclosureExpanded(false);
    window.Host().SetRoot(std::move(root));
    wil::com_ptr_nothrow<DisclosureChangeObserver> observer;
    observer.attach(Microsoft::WRL::Make<DisclosureChangeObserver>().Detach());
    Require(observer != nullptr, "allocate disclosure event observer");
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(bool(stop), "create UIA client stop event");
    std::atomic<bool> ready{false};
    std::atomic<bool> finished{false};
    std::atomic<HRESULT> setup{E_PENDING};
    std::atomic<const char*> setupStage{"thread start"};
    std::atomic<ULONGLONG> elementFromHandleMs{0};
    std::atomic<ULONGLONG> subscribeMs{0};
    const ULONGLONG setupStarted = GetTickCount64();
    const HWND hwnd              = window.Hwnd();
    std::jthread client([&]
    {
        setupStage.store("CoInitializeEx");
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
        {
            setupStage.store("CoCreateInstance(CUIAutomation)");
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        }
        if (SUCCEEDED(hr))
        {
            setupStage.store("ElementFromHandle");
            const ULONGLONG started = GetTickCount64();
            hr                      = automation->ElementFromHandle(hwnd, element.put());
            elementFromHandleMs.store(GetTickCount64() - started);
        }
        PROPERTYID property = UIA_ExpandCollapseExpandCollapseStatePropertyId;
        if (SUCCEEDED(hr))
        {
            setupStage.store("AddPropertyChangedEventHandlerNativeArray");
            const ULONGLONG started = GetTickCount64();
            hr = automation->AddPropertyChangedEventHandlerNativeArray(element.get(), TreeScope_Element, nullptr, observer.get(), &property, 1);
            subscribeMs.store(GetTickCount64() - started);
        }
        setup.store(hr);
        ready.store(true);
        if (SUCCEEDED(hr))
        {
            static_cast<void>(WaitForSingleObject(stop.get(), 10000));
            static_cast<void>(automation->RemovePropertyChangedEventHandler(element.get(), observer.get()));
        }
        finished.store(true);
    });
    ULONGLONG longestOwnerPumpMs = 0;
    const auto waitUntil         = [&](ULONGLONG timeoutMs, const auto& predicate)
    {
        const auto deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            const ULONGLONG pumpStarted = GetTickCount64();
            window.PumpMessages();
            longestOwnerPumpMs = (std::max)(longestOwnerPumpMs, GetTickCount64() - pumpStarted);
            Sleep(1);
        }
        return predicate();
    };
    const bool subscribed = waitUntil(kClientSetupAllowanceMs, [&] { return ready.load(); });
    // Provider requests run on this owner thread inside PumpMessages: a long owner pump points at the provider
    // side, while long client stages with short pumps point at UIA client initialization.
    std::cerr << "    [UIA] disclosure subscription stage=" << setupStage.load() << " ready=" << subscribed << " hr=0x" << std::hex
              << static_cast<unsigned long>(setup.load()) << std::dec << " elapsedMs=" << GetTickCount64() - setupStarted
              << " elementFromHandleMs=" << elementFromHandleMs.load() << " subscribeMs=" << subscribeMs.load() << " longestOwnerPumpMs=" << longestOwnerPumpMs
              << " allowanceMs=" << kClientSetupAllowanceMs << '\n';
    Require(subscribed && SUCCEEDED(setup.load()), "subscribe native disclosure property events");
    button->SetDisclosureExpanded(true);
    Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->changes.load() >= 1; }) && observer->state.load() == ExpandCollapseState_Expanded,
            "native automation client receives acknowledged expansion");
    button->SetDisclosureExpanded(false);
    Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->changes.load() >= 2; }) && observer->state.load() == ExpandCollapseState_Collapsed,
            "native automation client receives acknowledged collapse");
    SetEvent(stop.get());
    Require(waitUntil(kNotificationDeadlineMs, [&] { return finished.load(); }), "unsubscribe native disclosure property events");
    client.join();
}

void TestAccessibilityTextUnitHelperSharesGraphemeWordLineAndFallbackPolicy()
{
    using namespace DxUi;

    const std::wstring text                 = L"A\U0001F642e\u0301 word\r\nline";
    const TextRangeUnitMoveResult emojiMove = MoveAccessibilityTextPositionByUnit(text, 1u, TextUnit_Character, 1);
    Require(emojiMove.position == 3u && emojiMove.moved == 1, "shared UIA character movement keeps a surrogate pair intact");
    const AccessibilityTextUnitSpan combiningSpan = GetEnclosingAccessibilityTextUnitSpan(text, 4u, TextUnit_Character);
    Require(combiningSpan.start == 3u && combiningSpan.end == 5u, "shared UIA character expansion keeps a combining sequence intact");

    const AccessibilityTextUnitSpan formatSpan = GetEnclosingAccessibilityTextUnitSpan(text, 7u, TextUnit_Format);
    const AccessibilityTextUnitSpan wordSpan   = GetEnclosingAccessibilityTextUnitSpan(text, 7u, TextUnit_Word);
    Require(formatSpan.start == wordSpan.start && formatSpan.end == wordSpan.end, "unsupported Format falls forward to the shared Word boundary");
    const AccessibilityTextUnitSpan paragraphSpan = GetEnclosingAccessibilityTextUnitSpan(text, 7u, TextUnit_Paragraph);
    Require(paragraphSpan.start == 0u && paragraphSpan.end == text.size(), "unsupported Paragraph falls forward to Document instead of backward to Line");
    const TextRangeUnitMoveResult paragraphForward = MoveAccessibilityTextPositionByUnit(text, 7u, TextUnit_Paragraph, 1);
    Require(paragraphForward.position == text.size() && paragraphForward.moved == 1, "unsupported Paragraph movement advances once to the Document end");
    const TextRangeUnitMoveResult paragraphBackward = MoveAccessibilityTextPositionByUnit(text, 7u, TextUnit_Paragraph, -1);
    Require(paragraphBackward.position == 0u && paragraphBackward.moved == -1, "unsupported Paragraph endpoint movement retreats once to the Document start");
    const AccessibilityTextUnitSpan pageSpan = GetEnclosingAccessibilityTextUnitSpan(text, 7u, TextUnit_Page);
    Require(pageSpan.start == 0u && pageSpan.end == text.size(), "unsupported Page falls forward to Document");
    const TextRangeUnitMoveResult lineMove = MoveAccessibilityTextPositionByUnit(text, 7u, TextUnit_Line, 1);
    Require(lineMove.position == 12u && lineMove.moved == 1, "shared UIA line movement treats CRLF as one boundary");
}

void TestAccessibilityProviderTraversalSurvivesConcurrentRootReplacement()
{
    using namespace DxUi;

    struct StressRoot
    {
        StressRoot() = default;
        StressRoot(std::unique_ptr<Panel> rootValue, Control* focusValue) noexcept : root(std::move(rootValue)), focusTarget(focusValue)
        {
        }
        StressRoot(const StressRoot&)                = delete;
        StressRoot& operator=(const StressRoot&)     = delete;
        StressRoot(StressRoot&&) noexcept            = default;
        StressRoot& operator=(StressRoot&&) noexcept = default;

        std::unique_ptr<Panel> root;
        Control* focusTarget = nullptr;
    };

    auto buildRoot = [](MutableTreeModel& treeModel, MultiRowGridModel& gridModel)
    {
        auto root = std::make_unique<Panel>();

        auto* field = root->AddChild<TextField>(L"alpha beta gamma");
        field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 32.0f));

        auto* button = root->AddChild<Button>(L"Run");
        button->SetBounds(D2D1::RectF(188.0f, 0.0f, 260.0f, 32.0f));

        auto* tree = root->AddChild<Tree>();
        tree->SetBounds(D2D1::RectF(0.0f, 40.0f, 140.0f, 112.0f));
        tree->SetModel(&treeModel);
        tree->SetSelectedItemId(2u);

        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(148.0f, 40.0f, 300.0f, 132.0f));
        grid->SetModel(&gridModel);

        StressRoot result;
        result.focusTarget = field;
        result.root        = std::move(root);
        return result;
    };

    AttachedHostWindow window;
    MutableTreeModel treeModel;
    MultiRowGridModel gridModel(8u);
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Panes"},
        TreeItemData{.id = 3u, .text = L"Viewers"},
    });

    StressRoot firstRoot = buildRoot(treeModel, gridModel);
    Control* firstFocus  = firstRoot.focusTarget;
    window.Host().SetRoot(std::move(firstRoot.root));
    window.Host().SetFocusControl(firstFocus);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "concurrent accessibility traversal creates a root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "concurrent accessibility traversal root supports fragment navigation");

    POINT hitPoint{24, 20};
    Require(ClientToScreen(window.Hwnd(), &hitPoint) != FALSE, "concurrent accessibility traversal computes a screen point");

    std::atomic<bool> stopWorker{false};
    std::atomic<HRESULT> workerFailure{S_OK};
    std::atomic<int> workerStep{0};
    std::atomic<uint32_t> traversalCount{0u};
    std::atomic<uint32_t> goneElementReads{0u};
    std::thread worker([&]
    {
        while (! stopWorker.load(std::memory_order_acquire))
        {
            wil::com_ptr_nothrow<IRawElementProviderFragment> focusedProvider;
            HRESULT hr = rootProvider->GetFocus(focusedProvider.put());
            if (FAILED(hr))
            {
                workerStep.store(1, std::memory_order_release);
                workerFailure.store(hr, std::memory_order_release);
                break;
            }

            wil::com_ptr_nothrow<IRawElementProviderFragment> hitProvider;
            hr = rootProvider->ElementProviderFromPoint(static_cast<double>(hitPoint.x), static_cast<double>(hitPoint.y), hitProvider.put());
            if (FAILED(hr))
            {
                workerStep.store(2, std::memory_order_release);
                workerFailure.store(hr, std::memory_order_release);
                break;
            }

            wil::com_ptr_nothrow<IRawElementProviderFragment> childProvider;
            hr = rootFragment->Navigate(NavigateDirection_FirstChild, childProvider.put());
            if (FAILED(hr))
            {
                workerStep.store(3, std::memory_order_release);
                workerFailure.store(hr, std::memory_order_release);
                break;
            }

            for (int depth = 0; childProvider && depth < 4; ++depth)
            {
                wil::com_ptr_nothrow<IRawElementProviderSimple> childSimple;
                hr = childProvider.query_to(childSimple.put());
                if (FAILED(hr))
                {
                    workerStep.store(4, std::memory_order_release);
                    workerFailure.store(hr, std::memory_order_release);
                    break;
                }

                VARIANT propertyValue;
                VariantInit(&propertyValue);
                hr = childSimple->GetPropertyValue(UIA_NamePropertyId, &propertyValue);
                VariantClear(&propertyValue);
                // An element whose control a replacement removed reports that it is gone; a client starts over.
                if (hr == UIA_E_ELEMENTNOTAVAILABLE)
                {
                    goneElementReads.fetch_add(1u, std::memory_order_acq_rel);
                    break;
                }
                if (FAILED(hr))
                {
                    workerStep.store(5, std::memory_order_release);
                    workerFailure.store(hr, std::memory_order_release);
                    break;
                }

                wil::com_ptr_nothrow<IRawElementProviderFragment> nextProvider;
                hr = childProvider->Navigate(NavigateDirection_NextSibling, nextProvider.put());
                if (hr == UIA_E_ELEMENTNOTAVAILABLE)
                {
                    goneElementReads.fetch_add(1u, std::memory_order_acq_rel);
                    break;
                }
                if (FAILED(hr))
                {
                    workerStep.store(6, std::memory_order_release);
                    workerFailure.store(hr, std::memory_order_release);
                    break;
                }

                childProvider = std::move(nextProvider);
            }

            if (FAILED(workerFailure.load(std::memory_order_acquire)))
            {
                break;
            }
            traversalCount.fetch_add(1u, std::memory_order_acq_rel);
        }
    });

    for (uint32_t iteration = 0u; iteration < 80u && SUCCEEDED(workerFailure.load(std::memory_order_acquire)); ++iteration)
    {
        treeModel.SetVisibleItems({
            TreeItemData{.id = 1u, .text = L"General"},
            TreeItemData{.id = 2u, .text = (iteration % 2u == 0u) ? L"Panes" : L"Layout"},
            TreeItemData{.id = 3u, .text = L"Viewers"},
            TreeItemData{.id = 4u, .text = L"Network"},
        });

        StressRoot replacement = buildRoot(treeModel, gridModel);
        Control* focusTarget   = replacement.focusTarget;
        window.Host().SetRoot(std::move(replacement.root));
        window.Host().SetFocusControl(focusTarget);
        window.PumpMessages();

        if ((iteration % 5u) == 0u)
        {
            window.Host().SetRoot(nullptr);
            window.PumpMessages();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    stopWorker.store(true, std::memory_order_release);
    worker.join();

    RequireSucceeded(workerFailure.load(std::memory_order_acquire), "concurrent accessibility traversal survives root replacement");
    Require(workerStep.load(std::memory_order_acquire) == 0, "concurrent accessibility traversal reports no failed provider read step");
    Require(traversalCount.load(std::memory_order_acquire) > 0u, "concurrent accessibility traversal performs provider reads while roots churn");
    std::cout << "Concurrent traversal: " << traversalCount.load() << " traversals, " << goneElementReads.load() << " reads of replaced elements\n";
}

void TestAttachedWindowHostWmGetObjectReturnsAccessibilityProvider()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Run");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    const LRESULT result = SendMessageW(window.Hwnd(), WM_GETOBJECT, 0, static_cast<LPARAM>(UiaRootObjectId));
    Require(result != 0, "attached DX host returns a UIA provider from WM_GETOBJECT");
}

void TestAccessibilityRootRuntimeIdIncludesProviderSpecificValues()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Run");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "runtime-id test creates a root accessibility provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "runtime-id test root provider exposes fragment navigation");

    SAFEARRAY* runtimeId = nullptr;
    RequireSucceeded(rootFragment->GetRuntimeId(&runtimeId), "root provider runtime-id lookup succeeds");
    Require(runtimeId != nullptr, "root provider returns a runtime-id array");
    const auto destroyRuntimeId = wil::scope_exit([&] { SafeArrayDestroy(runtimeId); });

    LONG lowerBound = 0;
    LONG upperBound = -1;
    RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lowerBound), "root runtime-id lower bound lookup succeeds");
    RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upperBound), "root runtime-id upper bound lookup succeeds");
    Require(lowerBound == 0, "root runtime-id starts at index zero");
    Require(upperBound >= 2, "root runtime-id includes provider-specific values beyond UiaAppendRuntimeId");

    LONG firstValue  = 0;
    LONG secondValue = 0;
    LONG thirdValue  = 0;
    LONG index       = 0;
    RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &firstValue), "root runtime-id first element lookup succeeds");
    index = 1;
    RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &secondValue), "root runtime-id second element lookup succeeds");
    index = 2;
    RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &thirdValue), "root runtime-id third element lookup succeeds");
    Require(firstValue == UiaAppendRuntimeId, "root runtime-id starts with UiaAppendRuntimeId");
    Require(secondValue != 0 || thirdValue != 0, "root runtime-id appends non-zero provider-specific identity values");
}

std::wstring ReadTextRangeText(ITextRangeProvider& range, int maxLength, const char* context)
{
    BSTR text = nullptr;
    RequireSucceeded(range.GetText(maxLength, &text), context);
    const auto freeText = wil::scope_exit([&] { SysFreeString(text); });
    return std::wstring(text ? text : L"");
}

wil::com_ptr_nothrow<ITextRangeProvider> GetSingleTextRangeFromArray(SAFEARRAY* array, const char* context)
{
    Require(array != nullptr, context);

    LONG lowerBound = 0;
    LONG upperBound = -1;
    RequireSucceeded(SafeArrayGetLBound(array, 1, &lowerBound), context);
    RequireSucceeded(SafeArrayGetUBound(array, 1, &upperBound), context);
    Require(lowerBound == upperBound, context);

    IUnknown* rawUnknown = nullptr;
    LONG index           = lowerBound;
    RequireSucceeded(SafeArrayGetElement(array, &index, &rawUnknown), context);
    wil::com_ptr_nothrow<IUnknown> unknown;
    unknown.attach(rawUnknown);
    Require(unknown != nullptr, context);

    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(unknown.query_to(range.put()), context);
    Require(range != nullptr, context);
    return range;
}

std::vector<double> ReadDoubleArray(SAFEARRAY* array, const char* context)
{
    Require(array != nullptr, context);

    LONG lowerBound = 0;
    LONG upperBound = -1;
    RequireSucceeded(SafeArrayGetLBound(array, 1, &lowerBound), context);
    RequireSucceeded(SafeArrayGetUBound(array, 1, &upperBound), context);
    Require(upperBound >= lowerBound, context);

    std::vector<double> values(static_cast<size_t>(upperBound - lowerBound + 1));
    for (LONG index = lowerBound; index <= upperBound; ++index)
    {
        double value = 0.0;
        RequireSucceeded(SafeArrayGetElement(array, &index, &value), context);
        values[static_cast<size_t>(index - lowerBound)] = value;
    }
    return values;
}

std::vector<size_t> ResolveVisualLineStarts(const DxUi::WindowHost& host, const DxUi::TextField& field, std::wstring_view text, const char* context)
{
    std::vector<size_t> starts{0u};
    std::optional<D2D1_RECT_F> lineRect = field.TryGetTextInputCaretRect(host, 0u);
    Require(lineRect.has_value(), context);

    for (size_t index = 1u; index <= text.size(); ++index)
    {
        const std::optional<D2D1_RECT_F> rect = field.TryGetTextInputCaretRect(host, index);
        Require(rect.has_value(), context);
        constexpr float kLineToleranceDip = 1.0f;
        const bool sameVisualLine =
            std::fabs(rect->top - lineRect->top) <= kLineToleranceDip && std::fabs(rect->bottom - lineRect->bottom) <= kLineToleranceDip;
        if (! sameVisualLine)
        {
            starts.push_back(index);
            lineRect = rect;
        }
    }

    return starts;
}

[[nodiscard]] RECT DipRectToScreenRect(AttachedHostWindow& window, const D2D1_RECT_F& rectDip)
{
    POINT points[2]{
        {static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.left))), static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.top)))},
        {static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.right))),
         static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.bottom)))}};
    MapWindowPoints(window.Hwnd(), nullptr, points, 2);
    return RECT{points[0].x, points[0].y, points[1].x, points[1].y};
}

[[nodiscard]] D2D1_RECT_F UnionRects(const D2D1_RECT_F& first, const D2D1_RECT_F& second) noexcept
{
    return D2D1::RectF(
        (std::min)(first.left, second.left), (std::min)(first.top, second.top), (std::max)(first.right, second.right), (std::max)(first.bottom, second.bottom));
}

void TestDisclosureButtonExpandCollapsePreservesAcknowledgedState()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Afficher les informations détaillées");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 40.0f));
    button->SetDisclosureExpanded(false);
    bool expanded        = false;
    unsigned int invoked = 0;
    button->SetOnClick([&]()
    {
        ++invoked;
        expanded = ! expanded;
        button->SetDisclosureExpanded(expanded);
    });
    window.Host().SetRoot(std::move(root));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "disclosure root provider exists");
    auto provider = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 20.0f, 20.0f, "disclosure provider by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(provider.query_to(simple.put()), "disclosure simple provider");
    wil::com_ptr_nothrow<IUnknown> pattern;
    RequireSucceeded(simple->GetPatternProvider(UIA_ExpandCollapsePatternId, pattern.put()), "disclosure ExpandCollapse lookup");
    Require(pattern != nullptr, "disclosure publishes ExpandCollapse pattern");
    wil::com_ptr_nothrow<IExpandCollapseProvider> disclosure;
    RequireSucceeded(pattern.query_to(disclosure.put()), "disclosure pattern interface");
    ExpandCollapseState state{};
    RequireSucceeded(disclosure->get_ExpandCollapseState(&state), "read collapsed disclosure");
    Require(state == ExpandCollapseState_Collapsed, "disclosure starts collapsed");
    RequireSucceeded(disclosure->Expand(), "request disclosure expansion");
    RequireSucceeded(disclosure->Expand(), "repeat expansion is idempotent");
    Require(invoked == 1 && expanded, "same requested state never toggles twice");
    RequireSucceeded(disclosure->get_ExpandCollapseState(&state), "read acknowledged expanded state");
    Require(state == ExpandCollapseState_Expanded, "provider follows acknowledged state");
    Require(ReadProviderLongProperty(*simple.get(), UIA_ExpandCollapseExpandCollapseStatePropertyId, "disclosure state property") ==
                ExpandCollapseState_Expanded,
            "disclosure property and pattern agree");
    button->SetEnabled(false);
    Require(disclosure->Collapse() == UIA_E_ELEMENTNOTENABLED && invoked == 1, "disabled disclosure rejects a state change");
    button->SetEnabled(true);
    RequireSucceeded(disclosure->Collapse(), "collapse disclosure");
    Require(invoked == 2 && ! expanded, "collapse invokes once");
    button->ClearDisclosureState();
    pattern.reset();
    RequireSucceeded(simple->GetPatternProvider(UIA_ExpandCollapsePatternId, pattern.put()), "cleared disclosure lookup");
    Require(! pattern, "ordinary button has no disclosure pattern");
    button->SetDisclosureExpanded(false);
    button->SetOnClick([&]()
    {
        ++invoked;
        window.Host().SetRoot(std::make_unique<Panel>());
    });
    RequireSucceeded(disclosure->Expand(), "disclosure callback may replace the entire root");
    Require(invoked == 3, "root replacement callback invokes once");
    Require(FAILED(disclosure->get_ExpandCollapseState(&state)), "retained disclosure provider disconnects after root replacement");
}

void TestAccessibilityProviderExposesInvokeToggleAndLabeledValuePatterns()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Run");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 32.0f));

    auto* toggle = root->AddChild<Toggle>(L"Menu bar");
    toggle->SetBounds(D2D1::RectF(0.0f, 40.0f, 220.0f, 88.0f));
    toggle->SetChecked(true);

    auto* label = root->AddChild<Label>(L"Search");
    label->SetBounds(D2D1::RectF(0.0f, 100.0f, 120.0f, 124.0f));
    auto* field = root->AddChild<TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(0.0f, 128.0f, 220.0f, 156.0f));
    field->SetAccessibleHelpText(L"Type a local path");
    label->SetMnemonicTarget(field);

    auto* comboLabel = root->AddChild<Label>(L"Mode");
    comboLabel->SetBounds(D2D1::RectF(0.0f, 168.0f, 120.0f, 192.0f));
    auto* combo = root->AddChild<ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"current");
    combo->SetBounds(D2D1::RectF(0.0f, 196.0f, 220.0f, 224.0f));
    comboLabel->SetMnemonicTarget(combo);

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "debug accessibility provider is created for attached DX host");

    wil::com_ptr_nothrow<IRawElementProviderFragment> buttonProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 24.0f, 16.0f, "button accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> buttonSimple;
    RequireSucceeded(buttonProvider.query_to(buttonSimple.put()), "button accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*buttonSimple.get(), UIA_ControlTypePropertyId, "button exposes UIA control type") == UIA_ButtonControlTypeId,
            "button accessibility provider reports button control type");
    Require(ReadProviderStringProperty(*buttonSimple.get(), UIA_NamePropertyId, "button exposes accessibility name") == L"Run",
            "button accessibility provider reports button text as the accessible name");
    wil::com_ptr_nothrow<IUnknown> invokePattern;
    RequireSucceeded(buttonSimple->GetPatternProvider(UIA_InvokePatternId, invokePattern.put()), "button invoke pattern lookup succeeds");
    Require(invokePattern != nullptr, "button accessibility provider exposes invoke pattern");

    wil::com_ptr_nothrow<IRawElementProviderFragment> toggleProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 200.0f, 64.0f, "toggle accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> toggleSimple;
    RequireSucceeded(toggleProvider.query_to(toggleSimple.put()), "toggle accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*toggleSimple.get(), UIA_NamePropertyId, "toggle exposes accessibility name") == L"Menu bar",
            "toggle accessibility provider reports displayed label text");
    wil::com_ptr_nothrow<IUnknown> togglePatternUnknown;
    RequireSucceeded(toggleSimple->GetPatternProvider(UIA_TogglePatternId, togglePatternUnknown.put()), "toggle pattern lookup succeeds");
    Require(togglePatternUnknown != nullptr, "toggle accessibility provider exposes toggle pattern");
    wil::com_ptr_nothrow<IToggleProvider> togglePattern;
    RequireSucceeded(togglePatternUnknown.query_to(togglePattern.put()), "toggle pattern supports IToggleProvider");
    ToggleState toggleState = ToggleState_Off;
    RequireSucceeded(togglePattern->get_ToggleState(&toggleState), "toggle state query succeeds");
    Require(toggleState == ToggleState_On, "toggle accessibility provider reports the checked state");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 142.0f, "text field accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "text field accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*fieldSimple.get(), UIA_ControlTypePropertyId, "text field exposes UIA control type") == UIA_EditControlTypeId,
            "text field accessibility provider reports edit control type");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_NamePropertyId, "text field exposes accessibility name") == L"Search",
            "text field accessibility provider uses its associated label as the accessible name");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_HelpTextPropertyId, "text field exposes accessibility help text") == L"Type a local path",
            "text field accessibility provider reports explicit help text");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_ValueValuePropertyId, "text field exposes current value") == L"alpha",
            "text field accessibility provider reports the current value");
    Require(! ReadProviderBoolProperty(*fieldSimple.get(), UIA_ValueIsReadOnlyPropertyId, "text field exposes editable state"),
            "text field accessibility provider reports editable state");

    wil::com_ptr_nothrow<IUnknown> valuePatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_ValuePatternId, valuePatternUnknown.put()), "text field value pattern lookup succeeds");
    Require(valuePatternUnknown != nullptr, "text field accessibility provider exposes value pattern");
    wil::com_ptr_nothrow<IValueProvider> valuePattern;
    RequireSucceeded(valuePatternUnknown.query_to(valuePattern.put()), "text field value pattern supports IValueProvider");
    RequireSucceeded(valuePattern->SetValue(L"beta"), "text field accessibility provider can set the value");
    Require(field->GetText() == L"beta", "text field accessibility SetValue updates the underlying DX control");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_ValueValuePropertyId, "text field exposes value after SetValue") == L"beta",
            "text field accessibility provider reports the updated value after SetValue");

    wil::com_ptr_nothrow<IRawElementProviderFragment> comboLabelProvider;
    RequireSucceeded(fieldProvider->Navigate(NavigateDirection_NextSibling, comboLabelProvider.put()),
                     "text field accessibility provider navigates to the combo label");
    Require(comboLabelProvider != nullptr, "text field accessibility provider returns the combo label as the next sibling");

    wil::com_ptr_nothrow<IRawElementProviderFragment> comboProvider;
    RequireSucceeded(comboLabelProvider->Navigate(NavigateDirection_NextSibling, comboProvider.put()),
                     "combo label accessibility provider navigates to the combo");
    Require(comboProvider != nullptr, "combo label accessibility provider returns the combo as the next sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> comboSimple;
    RequireSucceeded(comboProvider.query_to(comboSimple.put()), "combo accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*comboSimple.get(), UIA_ControlTypePropertyId, "combo exposes UIA control type") == UIA_ComboBoxControlTypeId,
            "editable combo accessibility provider reports combo-box control type");
    Require(ReadProviderStringProperty(*comboSimple.get(), UIA_NamePropertyId, "combo exposes accessibility name") == L"Mode",
            "editable combo accessibility provider uses its associated label as the accessible name");
    Require(ReadProviderStringProperty(*comboSimple.get(), UIA_ValueValuePropertyId, "combo exposes current value") == L"current",
            "editable combo accessibility provider reports the current value");

    wil::com_ptr_nothrow<IUnknown> comboValuePatternUnknown;
    RequireSucceeded(comboSimple->GetPatternProvider(UIA_ValuePatternId, comboValuePatternUnknown.put()), "combo value pattern lookup succeeds");
    Require(comboValuePatternUnknown != nullptr, "editable combo accessibility provider exposes value pattern");
    wil::com_ptr_nothrow<IValueProvider> comboValuePattern;
    RequireSucceeded(comboValuePatternUnknown.query_to(comboValuePattern.put()), "combo value pattern supports IValueProvider");
    RequireSucceeded(comboValuePattern->SetValue(L"updated"), "editable combo accessibility provider can set the value");
    Require(combo->GetText() == L"updated", "editable combo accessibility SetValue updates the underlying DX control");
    Require(ReadProviderStringProperty(*comboSimple.get(), UIA_ValueValuePropertyId, "combo exposes value after SetValue") == L"updated",
            "editable combo accessibility provider reports the updated value after SetValue");
}

void TestAccessibilityProviderRefreshesButtonSemanticProperties()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>();
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "button semantic refresh test creates an accessibility provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> buttonProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 24.0f, 16.0f, "button semantic refresh provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> buttonSimple;
    RequireSucceeded(buttonProvider.query_to(buttonSimple.put()), "button semantic refresh provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*buttonSimple.get(), UIA_NamePropertyId, "empty button exposes initial accessibility name").empty(),
            "button accessibility provider starts with an empty name");

    button->SetText(L"Cancel");
    Require(ReadProviderStringProperty(*buttonSimple.get(), UIA_NamePropertyId, "button text refresh updates accessibility name") == L"Cancel",
            "button accessibility provider refreshes its name after SetText");

    button->SetEnabled(false);
    Require(! ReadProviderBoolProperty(*buttonSimple.get(), UIA_IsEnabledPropertyId, "button enabled refresh updates accessibility state"),
            "button accessibility provider refreshes its enabled state after SetEnabled");

    button->SetAccessibleName(L"Dismiss");
    Require(ReadProviderStringProperty(*buttonSimple.get(), UIA_NamePropertyId, "explicit button accessible name refreshes") == L"Dismiss",
            "button accessibility provider refreshes explicit accessible name changes");
}

void TestAccessibilityProviderRefreshesLabelAssociations()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root     = std::make_unique<Panel>();
    auto* label   = root->AddChild<Label>(L"Ignore files");
    auto* toggle  = root->AddChild<Toggle>(L"Off");
    auto* pattern = root->AddChild<TextField>(L"*.tmp");
    label->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 24.0f));
    toggle->SetBounds(D2D1::RectF(0.0f, 32.0f, 160.0f, 64.0f));
    pattern->SetBounds(D2D1::RectF(0.0f, 72.0f, 220.0f, 104.0f));
    label->SetMnemonicTarget(toggle);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "label association refresh test creates an accessibility provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 88.0f, "label association field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "label association field provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_NamePropertyId, "unassociated field exposes fallback name") == L"*.tmp",
            "text field starts with its fallback accessible name while the label targets the toggle");

    label->SetMnemonicTarget(pattern);
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_NamePropertyId, "retargeted label updates field name") == L"Ignore files",
            "text field accessibility name refreshes when a label retargets to it");

    label->SetText(L"File patterns");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_NamePropertyId, "label text update refreshes associated field name") == L"File patterns",
            "text field accessibility name refreshes when its associated label text changes");
}

void TestAccessibilityProviderExposesDirectSemanticRootControls()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto combo = std::make_unique<ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"current");
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    auto* comboRaw = combo.get();
    window.Host().SetRoot(std::move(combo));
    window.Host().SetFocusControl(comboRaw);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "direct-root accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "direct-root accessibility root exposes IRawElementProviderFragment");

    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(rootProvider.query_to(rootSimple.put()), "direct-root root provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*rootSimple.get(), UIA_ControlTypePropertyId, "direct-root combo exposes UIA control type") == UIA_ComboBoxControlTypeId,
            "direct-root combo accessibility provider reports combo-box control type");
    Require(ReadProviderStringProperty(*rootSimple.get(), UIA_ValueValuePropertyId, "direct-root combo exposes current value") == L"current",
            "direct-root combo accessibility provider reports the current value");

    wil::com_ptr_nothrow<IUnknown> comboValuePatternUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_ValuePatternId, comboValuePatternUnknown.put()), "direct-root combo value pattern lookup succeeds");
    Require(comboValuePatternUnknown != nullptr, "direct-root combo accessibility provider exposes value pattern");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstChildProvider;
    RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, firstChildProvider.put()),
                     "direct-root collapsed provider first-child lookup succeeds");
    Require(firstChildProvider == nullptr, "direct-root collapsed provider does not expose the same semantic control as a duplicate child");

    wil::com_ptr_nothrow<IRawElementProviderFragment> hitProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 16.0f, "direct-root combo provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> hitRoot;
    RequireSucceeded(hitProvider.query_to(hitRoot.put()), "direct-root point-hit provider is the collapsed root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> hitSimple;
    RequireSucceeded(hitProvider.query_to(hitSimple.put()), "direct-root point-hit provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*hitSimple.get(), UIA_ControlTypePropertyId, "direct-root point-hit provider exposes combo control type") ==
                UIA_ComboBoxControlTypeId,
            "direct-root point-hit provider resolves the combo control");

    wil::com_ptr_nothrow<IRawElementProviderFragment> focusedProvider;
    RequireSucceeded(rootProvider->GetFocus(focusedProvider.put()), "direct-root focus lookup succeeds");
    Require(focusedProvider != nullptr, "direct-root focus lookup returns the combo control");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> focusedRoot;
    RequireSucceeded(focusedProvider.query_to(focusedRoot.put()), "direct-root focused provider is the collapsed root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> focusedSimple;
    RequireSucceeded(focusedProvider.query_to(focusedSimple.put()), "direct-root focused provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*focusedSimple.get(), UIA_ValueValuePropertyId, "direct-root focused combo exposes value") == L"current",
            "direct-root focus lookup returns the semantic root combo provider");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fragmentRoot;
    RequireSucceeded(hitProvider->get_FragmentRoot(fragmentRoot.put()), "direct-root child FragmentRoot lookup succeeds");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> secondFactoryRoot;
    secondFactoryRoot.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(secondFactoryRoot != nullptr, "direct-root repeated factory lookup returns a provider");

    wil::com_ptr_nothrow<IUnknown> rootIdentity;
    wil::com_ptr_nothrow<IUnknown> hitIdentity;
    wil::com_ptr_nothrow<IUnknown> focusedIdentity;
    wil::com_ptr_nothrow<IUnknown> fragmentRootIdentity;
    wil::com_ptr_nothrow<IUnknown> secondFactoryIdentity;
    RequireSucceeded(rootProvider.query_to(rootIdentity.put()), "direct-root provider exposes canonical IUnknown identity");
    RequireSucceeded(hitProvider.query_to(hitIdentity.put()), "direct-root point hit exposes IUnknown identity");
    RequireSucceeded(focusedProvider.query_to(focusedIdentity.put()), "direct-root focus exposes IUnknown identity");
    RequireSucceeded(fragmentRoot.query_to(fragmentRootIdentity.put()), "direct-root FragmentRoot exposes IUnknown identity");
    RequireSucceeded(secondFactoryRoot.query_to(secondFactoryIdentity.put()), "direct-root repeated factory exposes IUnknown identity");
    Require(rootIdentity.get() == hitIdentity.get() && rootIdentity.get() == focusedIdentity.get() && rootIdentity.get() == fragmentRootIdentity.get() &&
                rootIdentity.get() == secondFactoryIdentity.get(),
            "all root-returning accessibility paths preserve one canonical COM identity per HWND");
}

void TestAccessibilityProviderIdentityRetiresAcrossSameHwndReattach()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto oldRoot    = std::make_unique<Button>(L"Old action");
    auto* oldButton = oldRoot.get();
    oldButton->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 32.0f));
    window.Host().SetRoot(std::move(oldRoot));
    window.Host().SetFocusControl(oldButton);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> oldProvider;
    oldProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(oldProvider != nullptr, "same-HWND lifecycle test creates the old root provider");
    wil::com_ptr_nothrow<IUnknown> oldIdentity;
    RequireSucceeded(oldProvider.query_to(oldIdentity.put()), "same-HWND lifecycle old provider exposes IUnknown identity");

    Require(CreateWindowHostAccessibilityProvider(nullptr) == nullptr, "public native provider acquisition rejects a null window");
    const HWND reusedHwnd = window.Hwnd();
    POINT oldHitPoint{40, 16};
    Require(ClientToScreen(reusedHwnd, &oldHitPoint) != FALSE, "same-HWND lifecycle converts the old hit point to screen coordinates");

    window.Host().Detach();
    Require(CreateWindowHostAccessibilityProvider(reusedHwnd) == nullptr, "public native provider acquisition rejects a detached host");
    Require(window.Host().Attach(reusedHwnd), "same-HWND lifecycle reattaches the host to the exact saved HWND");

    auto newRoot    = std::make_unique<Button>(L"New action");
    auto* newButton = newRoot.get();
    newButton->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 32.0f));
    window.Host().SetRoot(std::move(newRoot));
    window.Host().SetFocusControl(newButton);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> newProvider;
    newProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(newProvider != nullptr, "same-HWND lifecycle creates the new root provider after reattach");
    wil::com_ptr_nothrow<IUnknown> newIdentity;
    RequireSucceeded(newProvider.query_to(newIdentity.put()), "same-HWND lifecycle new provider exposes IUnknown identity");
    Require(oldIdentity.get() != newIdentity.get(), "same-HWND lifecycle reattach creates a distinct canonical provider identity");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> repeatedNewProvider;
    repeatedNewProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IUnknown> repeatedNewIdentity;
    Require(repeatedNewProvider != nullptr, "same-HWND lifecycle repeated new-provider acquisition succeeds");
    RequireSucceeded(repeatedNewProvider.query_to(repeatedNewIdentity.put()), "same-HWND lifecycle repeated new provider exposes IUnknown identity");
    Require(newIdentity.get() == repeatedNewIdentity.get(), "same-HWND lifecycle preserves canonical identity within the new attachment");

    wil::com_ptr_nothrow<IRawElementProviderFragment> retiredFocus;
    RequireSucceeded(oldProvider->GetFocus(retiredFocus.put()), "same-HWND lifecycle old provider focus query remains callable");
    Require(retiredFocus == nullptr, "same-HWND lifecycle old provider cannot expose focus from the new attachment");

    wil::com_ptr_nothrow<IRawElementProviderFragment> retiredHit;
    RequireSucceeded(oldProvider->ElementProviderFromPoint(static_cast<double>(oldHitPoint.x), static_cast<double>(oldHitPoint.y), retiredHit.put()),
                     "same-HWND lifecycle old provider hit-test remains callable");
    Require(retiredHit == nullptr, "same-HWND lifecycle old provider cannot hit-test into the new attachment");

    wil::com_ptr_nothrow<IRawElementProviderFragment> newFocus;
    RequireSucceeded(newProvider->GetFocus(newFocus.put()), "same-HWND lifecycle new provider focus query succeeds");
    Require(newFocus != nullptr, "same-HWND lifecycle new provider resolves the new focused control");
    wil::com_ptr_nothrow<IUnknown> newFocusIdentity;
    RequireSucceeded(newFocus.query_to(newFocusIdentity.put()), "same-HWND lifecycle new focused provider exposes IUnknown identity");
    Require(newFocusIdentity.get() == newIdentity.get(), "same-HWND lifecycle new focus resolves through the new canonical root identity");
}

void TestAccessibilityLabelOnlyRootDoesNotUseDirectSemanticRootCollapse()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto label = std::make_unique<Label>(L"Standalone label");
    label->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 24.0f));
    window.Host().SetRoot(std::move(label));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "label-only accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "label-only accessibility root exposes IRawElementProviderFragment");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstChildProvider;
    RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, firstChildProvider.put()), "label-only root first-child lookup succeeds");
    Require(firstChildProvider != nullptr, "label-only root exposes the label as a child instead of collapsing it into the root");

    wil::com_ptr_nothrow<IRawElementProviderSimple> childSimple;
    RequireSucceeded(firstChildProvider.query_to(childSimple.put()), "label-only child provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*childSimple.get(), UIA_ControlTypePropertyId, "label-only child exposes text control type") == UIA_TextControlTypeId,
            "label-only child provider is the label text element");
    Require(ReadProviderStringProperty(*childSimple.get(), UIA_NamePropertyId, "label-only child exposes label text") == L"Standalone label",
            "label-only child provider exposes the label text");

    wil::com_ptr_nothrow<IRawElementProviderFragment> duplicateGrandchild;
    RequireSucceeded(firstChildProvider->Navigate(NavigateDirection_FirstChild, duplicateGrandchild.put()), "label-only child first-child lookup succeeds");
    Require(duplicateGrandchild == nullptr, "label-only label child does not expose a duplicate nested label");
}

void TestAccessibilityDirectSemanticRootMatchesUiAutomationClientTree()
{
    using namespace DxUi;

    const HRESULT coinitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Require(coinitHr == S_OK || coinitHr == S_FALSE || coinitHr == RPC_E_CHANGED_MODE, "UIAutomation direct-root client test initializes COM");
    const bool uninitializeCom       = coinitHr == S_OK || coinitHr == S_FALSE;
    const auto uninitializeComOnExit = wil::scope_exit([&]
    {
        if (uninitializeCom)
        {
            CoUninitialize();
        }
    });

    AttachedHostWindow window;
    auto combo = std::make_unique<ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"current");
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    window.Host().SetRoot(std::move(combo));

    wil::com_ptr_nothrow<IUIAutomation> automation;
    RequireSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put())),
                     "UIAutomation direct-root client is created");
    Require(automation != nullptr, "UIAutomation direct-root client instance is available");

    wil::com_ptr_nothrow<IUIAutomationElement> rootElement;
    RequireSucceeded(automation->ElementFromHandle(window.Hwnd(), rootElement.put()), "UIAutomation direct-root ElementFromHandle succeeds");
    Require(rootElement != nullptr, "UIAutomation direct-root ElementFromHandle returns an element");

    CONTROLTYPEID rootControlType = 0;
    RequireSucceeded(rootElement->get_CurrentControlType(&rootControlType), "UIAutomation direct-root current control type lookup succeeds");
    Require(rootControlType == UIA_ComboBoxControlTypeId, "UIAutomation direct-root element is the semantic combo box, not a wrapper pane");

    wil::com_ptr_nothrow<IUIAutomationValuePattern> valuePattern;
    RequireSucceeded(rootElement->GetCurrentPatternAs(UIA_ValuePatternId, __uuidof(IUIAutomationValuePattern), valuePattern.put_void()),
                     "UIAutomation direct-root value pattern lookup succeeds");
    Require(valuePattern != nullptr, "UIAutomation direct-root exposes the combo value pattern");

    BSTR currentValue = nullptr;
    RequireSucceeded(valuePattern->get_CurrentValue(&currentValue), "UIAutomation direct-root value lookup succeeds");
    const auto freeCurrentValue = wil::scope_exit([&] { SysFreeString(currentValue); });
    Require(std::wstring_view(currentValue ? currentValue : L"", SysStringLen(currentValue)) == L"current", "UIAutomation direct-root reports the combo value");

    BOOL isContentElement = FALSE;
    RequireSucceeded(rootElement->get_CurrentIsContentElement(&isContentElement), "UIAutomation direct-root content-element lookup succeeds");
    Require(isContentElement == TRUE, "UIAutomation direct-root element is a content element");

    VARIANT contentElementProperty{};
    VariantInit(&contentElementProperty);
    contentElementProperty.vt      = VT_BOOL;
    contentElementProperty.boolVal = VARIANT_TRUE;
    wil::com_ptr_nothrow<IUIAutomationCondition> contentViewCondition;
    RequireSucceeded(automation->CreatePropertyCondition(UIA_IsContentElementPropertyId, contentElementProperty, contentViewCondition.put()),
                     "UIAutomation direct-root content-view condition is created");
    Require(contentViewCondition != nullptr, "UIAutomation direct-root content-view condition instance is available");

    wil::com_ptr_nothrow<IUIAutomationElementArray> contentChildren;
    RequireSucceeded(rootElement->FindAll(TreeScope_Children, contentViewCondition.get(), contentChildren.put()),
                     "UIAutomation direct-root content-view child enumeration succeeds");
    Require(contentChildren != nullptr, "UIAutomation direct-root content-view child enumeration returns an array");

    int contentChildCount = -1;
    RequireSucceeded(contentChildren->get_Length(&contentChildCount), "UIAutomation direct-root content-view child count lookup succeeds");
    Require(contentChildCount == 0, "UIAutomation direct-root content view exposes exactly one content element with no duplicate child");
}

void TestAccessibilityDirectSemanticRootTreeSelectionMatchesUiAutomationClientTree()
{
    using namespace DxUi;

    const HRESULT coinitHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Require(coinitHr == S_OK || coinitHr == S_FALSE || coinitHr == RPC_E_CHANGED_MODE, "UIAutomation direct-root tree selection test initializes COM");
    const bool uninitializeCom       = coinitHr == S_OK || coinitHr == S_FALSE;
    const auto uninitializeComOnExit = wil::scope_exit([&]
    {
        if (uninitializeCom)
        {
            CoUninitialize();
        }
    });

    AttachedHostWindow window;
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Panes"},
        TreeItemData{.id = 3u, .text = L"Viewers"},
    });

    auto tree = std::make_unique<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 96.0f));
    tree->SetModel(&treeModel);
    Tree* const liveTree = tree.get();
    window.Host().SetRoot(std::move(tree));
    liveTree->SetSelectedItemId(2u);

    wil::com_ptr_nothrow<IUIAutomation> automation;
    RequireSucceeded(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put())),
                     "UIAutomation direct-root tree selection client is created");
    Require(automation != nullptr, "UIAutomation direct-root tree selection client instance is available");

    wil::com_ptr_nothrow<IUIAutomationElement> rootElement;
    RequireSucceeded(automation->ElementFromHandle(window.Hwnd(), rootElement.put()), "UIAutomation direct-root tree ElementFromHandle succeeds");
    Require(rootElement != nullptr, "UIAutomation direct-root tree ElementFromHandle returns an element");

    CONTROLTYPEID rootControlType = 0;
    RequireSucceeded(rootElement->get_CurrentControlType(&rootControlType), "UIAutomation direct-root tree control type lookup succeeds");
    Require(rootControlType == UIA_TreeControlTypeId, "UIAutomation direct-root tree element is the semantic tree");

    wil::com_ptr_nothrow<IUIAutomationSelectionPattern> selectionPattern;
    RequireSucceeded(rootElement->GetCurrentPatternAs(UIA_SelectionPatternId, __uuidof(IUIAutomationSelectionPattern), selectionPattern.put_void()),
                     "UIAutomation direct-root tree exposes SelectionPattern");
    Require(selectionPattern != nullptr, "UIAutomation direct-root tree selection pattern is available");

    BOOL canSelectMultiple = TRUE;
    RequireSucceeded(selectionPattern->get_CurrentCanSelectMultiple(&canSelectMultiple),
                     "UIAutomation direct-root tree selection pattern reports multi-select capability");
    Require(canSelectMultiple == FALSE, "UIAutomation direct-root tree selection pattern reports single-selection behavior");

    wil::com_ptr_nothrow<IUIAutomationElementArray> selection;
    RequireSucceeded(selectionPattern->GetCurrentSelection(selection.put()), "UIAutomation direct-root tree selected-items lookup succeeds");
    Require(selection != nullptr, "UIAutomation direct-root tree selected-items lookup returns an array");

    int selectionLength = 0;
    RequireSucceeded(selection->get_Length(&selectionLength), "UIAutomation direct-root tree selected-items array reports length");
    Require(selectionLength == 1, "UIAutomation direct-root tree returns exactly one selected item");

    wil::com_ptr_nothrow<IUIAutomationElement> selectedElement;
    RequireSucceeded(selection->GetElement(0, selectedElement.put()), "UIAutomation direct-root tree selected item lookup succeeds");
    Require(selectedElement != nullptr, "UIAutomation direct-root tree selected item is available");

    CONTROLTYPEID selectedControlType = 0;
    RequireSucceeded(selectedElement->get_CurrentControlType(&selectedControlType), "UIAutomation direct-root tree selected item control type lookup succeeds");
    Require(selectedControlType == UIA_TreeItemControlTypeId, "UIAutomation direct-root tree selected element is a tree item");

    BSTR selectedName = nullptr;
    RequireSucceeded(selectedElement->get_CurrentName(&selectedName), "UIAutomation direct-root tree selected item name lookup succeeds");
    const auto freeSelectedName = wil::scope_exit([&] { SysFreeString(selectedName); });
    Require(std::wstring_view(selectedName ? selectedName : L"", SysStringLen(selectedName)) == L"Panes",
            "UIAutomation direct-root tree selected item reports the selected visible item name");
}

void TestAccessibilityProviderReportsFocusedControl()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Run");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 32.0f));
    auto* field = root->AddChild<TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(0.0f, 40.0f, 220.0f, 68.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "focused-control accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> focusedProvider;
    RequireSucceeded(rootProvider->GetFocus(focusedProvider.put()), "root provider focus lookup succeeds");
    Require(focusedProvider != nullptr, "root provider returns the focused control");

    wil::com_ptr_nothrow<IRawElementProviderSimple> focusedSimple;
    RequireSucceeded(focusedProvider.query_to(focusedSimple.put()), "focused provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*focusedSimple.get(), UIA_ValueValuePropertyId, "focused text field exposes value") == L"alpha",
            "root provider focus lookup returns the focused text field provider");
}

void TestAccessibilityProviderMasksPasswordTextFieldValue()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"secret");
    field->SetMasked(true);
    field->SetAccessibleName(L"Password");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "masked text field accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 16.0f, "masked text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "masked text field provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*fieldSimple.get(), UIA_ControlTypePropertyId, "masked text field exposes edit control type") == UIA_EditControlTypeId,
            "masked text field provider reports edit control type");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_NamePropertyId, "masked text field exposes accessible name") == L"Password",
            "masked text field provider does not use the secret value as its accessible name");
    Require(ReadProviderBoolProperty(*fieldSimple.get(), UIA_IsPasswordPropertyId, "masked text field exposes password state"),
            "masked text field provider reports IsPassword");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_ValueValuePropertyId, "masked text field suppresses UIA value").empty(),
            "masked text field provider does not expose the secret through ValuePattern");
}

void TestAccessibilityProviderExposesMaskedRevealButton()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"secret");
    field->SetMasked(true);
    field->SetAccessibleName(L"Password");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "masked reveal accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> revealProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 206.0f, 16.0f, "masked reveal button provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> revealSimple;
    RequireSucceeded(revealProvider.query_to(revealSimple.put()), "masked reveal button provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*revealSimple.get(), UIA_ControlTypePropertyId, "masked reveal button exposes control type") == UIA_ButtonControlTypeId,
            "masked reveal button provider reports button control type");
    Require(ReadProviderStringProperty(*revealSimple.get(), UIA_NamePropertyId, "masked reveal button exposes accessible name") == L"Show password",
            "masked reveal button provider reports the reveal affordance name");

    wil::com_ptr_nothrow<IUnknown> invokePatternUnknown;
    RequireSucceeded(revealSimple->GetPatternProvider(UIA_InvokePatternId, invokePatternUnknown.put()), "masked reveal button invoke pattern lookup succeeds");
    Require(invokePatternUnknown != nullptr, "masked reveal button exposes InvokePattern");
    wil::com_ptr_nothrow<IInvokeProvider> invokePattern;
    RequireSucceeded(invokePatternUnknown.query_to(invokePattern.put()), "masked reveal button invoke pattern supports IInvokeProvider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider;
    RequireSucceeded(revealProvider->Navigate(NavigateDirection_Parent, fieldProvider.put()), "masked reveal button navigates to parent field");
    Require(fieldProvider != nullptr, "masked reveal button returns its owning field as parent");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "masked reveal parent provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*fieldSimple.get(), UIA_ControlTypePropertyId, "masked reveal parent exposes edit type") == UIA_EditControlTypeId,
            "masked reveal button parent is the edit field provider");

    RequireSucceeded(invokePattern->Invoke(), "masked reveal button Invoke succeeds");
    Require(field->GetPasswordRevealState() == PasswordRevealState::Visible, "masked reveal button Invoke reveals the field visually");
    Require(ReadProviderStringProperty(*fieldSimple.get(), UIA_ValueValuePropertyId, "revealed masked text field still suppresses UIA value").empty(),
            "masked text field provider still does not expose the secret after reveal Invoke");
}

void TestAccessibilityProviderExposesTextPatternForTextField()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* textLabel = root->AddChild<Label>(L"Query");
    textLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    field->SetSelectionRange(0u, 5u);
    textLabel->SetMnemonicTarget(field);

    auto* passwordLabel = root->AddChild<Label>(L"Password");
    passwordLabel->SetBounds(D2D1::RectF(0.0f, 72.0f, 120.0f, 96.0f));
    auto* maskedField = root->AddChild<TextField>(L"secret");
    maskedField->SetMasked(true);
    maskedField->SetBounds(D2D1::RectF(0.0f, 100.0f, 260.0f, 132.0f));
    passwordLabel->SetMnemonicTarget(maskedField);

    auto* multilineLabel = root->AddChild<Label>(L"Notes");
    multilineLabel->SetBounds(D2D1::RectF(0.0f, 132.0f, 120.0f, 136.0f));
    auto* multilineField = root->AddChild<TextField>(L"red\ngreen\nblue");
    multilineField->SetMultiline(true);
    multilineField->SetBounds(D2D1::RectF(0.0f, 136.0f, 260.0f, 168.0f));
    multilineField->SetSelectionRange(0u, 3u);
    multilineLabel->SetMnemonicTarget(multilineField);

    std::wstring emojiText;
    emojiText.reserve(7u);
    emojiText.push_back(L'A');
    emojiText.push_back(static_cast<wchar_t>(0xD83D));
    emojiText.push_back(static_cast<wchar_t>(0xDC69));
    emojiText.push_back(static_cast<wchar_t>(0x200D));
    emojiText.push_back(static_cast<wchar_t>(0xD83D));
    emojiText.push_back(static_cast<wchar_t>(0xDCBB));
    emojiText.push_back(L'Z');
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "text pattern accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "text field provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "text field TextPattern supports ITextProvider");

    SupportedTextSelection supportedSelection = SupportedTextSelection_None;
    RequireSucceeded(textPattern->get_SupportedTextSelection(&supportedSelection), "text field TextPattern reports supported selection mode");
    Require(supportedSelection == SupportedTextSelection_Single, "text field TextPattern supports one selection range");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "text field TextPattern exposes a document range");
    Require(documentRange != nullptr, "text field TextPattern returns a document range");
    Require(ReadTextRangeText(*documentRange.get(), -1, "text field document range exposes text") == L"alpha beta",
            "text field TextPattern document range returns the current value");
    wil::com_ptr_nothrow<ITextRangeProvider> clonedDocumentRange;
    RequireSucceeded(documentRange->Clone(clonedDocumentRange.put()), "text field TextPattern document range clones");
    Require(clonedDocumentRange != nullptr, "text field TextPattern document range clone is returned");
    BOOL sameRange = FALSE;
    RequireSucceeded(documentRange->Compare(clonedDocumentRange.get(), &sameRange), "text field TextPattern cloned range compares");
    Require(sameRange == TRUE, "text field TextPattern cloned range compares equal by content");
    int endpointComparison = 0;
    RequireSucceeded(
        documentRange->CompareEndpoints(TextPatternRangeEndpoint_Start, clonedDocumentRange.get(), TextPatternRangeEndpoint_End, &endpointComparison),
        "text field TextPattern endpoint comparison succeeds");
    Require(endpointComparison < 0, "text field TextPattern start endpoint compares before document end");
    wil::com_ptr_nothrow<ITextRangeProvider> wordRange;
    RequireSucceeded(documentRange->Clone(wordRange.put()), "text field TextPattern document range clones for word movement");
    Require(wordRange != nullptr, "text field TextPattern word movement range clone is returned");
    int moved = 0;
    RequireSucceeded(wordRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Word, 1, &moved),
                     "text field TextPattern start endpoint moves by word");
    Require(moved == 1, "text field TextPattern start endpoint reports moved words");
    Require(ReadTextRangeText(*wordRange.get(), -1, "text field document range exposes moved-word text") == L"beta",
            "text field TextPattern word endpoint movement narrows to the next word");
    moved = 0;
    RequireSucceeded(wordRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Word, -1, &moved),
                     "text field TextPattern start endpoint moves backward by word");
    Require(moved == -1, "text field TextPattern start endpoint reports moved words backward");
    Require(ReadTextRangeText(*wordRange.get(), -1, "text field document range restores after moved-word text") == L"alpha beta",
            "text field TextPattern word endpoint movement restores the document text");
    wil::com_ptr_nothrow<ITextRangeProvider> enclosingCharacterRange;
    RequireSucceeded(clonedDocumentRange->Clone(enclosingCharacterRange.put()), "text field document range clones for character expansion");
    moved = 0;
    RequireSucceeded(enclosingCharacterRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 2, &moved),
                     "text field character-expansion range moves inside the first word");
    RequireSucceeded(enclosingCharacterRange->ExpandToEnclosingUnit(TextUnit_Character), "text field range expands to the enclosing character unit");
    Require(ReadTextRangeText(*enclosingCharacterRange.get(), -1, "text field character-expanded range exposes text") == L"p",
            "text field character expansion normalizes a longer range to the text element at its start");
    wil::com_ptr_nothrow<ITextRangeProvider> enclosingWordRange;
    RequireSucceeded(clonedDocumentRange->Clone(enclosingWordRange.put()), "text field document range clones for word expansion");
    moved = 0;
    RequireSucceeded(enclosingWordRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 2, &moved),
                     "text field word-expansion range moves inside the first word");
    RequireSucceeded(enclosingWordRange->ExpandToEnclosingUnit(TextUnit_Word), "text field range expands to the enclosing word unit");
    Require(ReadTextRangeText(*enclosingWordRange.get(), -1, "text field word-expanded range exposes text") == L"alpha",
            "text field word expansion normalizes a longer range to the word containing its start");
    const POINT rangePointScreen = window.Host().DipPointToScreenPoint(D2D1::Point2F(0.0f, 44.0f));
    const UiaPoint rangePoint{static_cast<double>(rangePointScreen.x), static_cast<double>(rangePointScreen.y)};
    wil::com_ptr_nothrow<ITextRangeProvider> pointRange;
    RequireSucceeded(textPattern->RangeFromPoint(rangePoint, pointRange.put()), "text field TextPattern RangeFromPoint succeeds");
    Require(pointRange != nullptr, "text field TextPattern RangeFromPoint returns a range");
    Require(ReadTextRangeText(*pointRange.get(), -1, "text field RangeFromPoint range is collapsed").empty(),
            "text field TextPattern RangeFromPoint returns a collapsed caret range");
    moved = 0;
    wil::com_ptr_nothrow<ITextRangeProvider> wordCaretRange;
    RequireSucceeded(pointRange->Clone(wordCaretRange.put()), "text field RangeFromPoint range clones for word movement");
    Require(wordCaretRange != nullptr, "text field RangeFromPoint word movement clone is returned");
    RequireSucceeded(wordCaretRange->Move(TextUnit_Word, 1, &moved), "text field collapsed range moves forward by word");
    Require(moved == 1, "text field collapsed range reports moved words");
    RequireSucceeded(wordCaretRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved),
                     "text field word-moved collapsed range expands by one character");
    Require(ReadTextRangeText(*wordCaretRange.get(), -1, "text field word-moved collapsed range exposes text") == L"b",
            "text field collapsed word movement lands at the next word");
    moved = 0;
    RequireSucceeded(pointRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved),
                     "text field RangeFromPoint range endpoint expands by one character");
    Require(moved == 1, "text field RangeFromPoint range reports one expanded character");
    Require(ReadTextRangeText(*pointRange.get(), -1, "text field RangeFromPoint expanded range exposes text") == L"a",
            "text field TextPattern RangeFromPoint maps the leading point to the first character");
    moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                     "text field TextPattern start endpoint moves by character");
    Require(moved == 6, "text field TextPattern start endpoint reports moved characters");
    Require(ReadTextRangeText(*documentRange.get(), -1, "text field document range exposes moved-start text") == L"beta",
            "text field TextPattern start endpoint movement narrows the range");
    moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, -2, &moved),
                     "text field TextPattern end endpoint moves backward by character");
    Require(moved == -2, "text field TextPattern end endpoint reports moved characters");
    Require(ReadTextRangeText(*documentRange.get(), -1, "text field document range exposes moved-end text") == L"be",
            "text field TextPattern end endpoint movement narrows the range from the end");

    AttachedHostWindow emojiWindow;
    auto emojiRoot   = std::make_unique<Panel>();
    auto* emojiField = emojiRoot->AddChild<TextField>(emojiText);
    emojiField->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    emojiWindow.Host().SetRoot(std::move(emojiRoot));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> emojiRootProvider;
    emojiRootProvider.attach(emojiWindow.Host().DebugCreateAccessibilityProvider());
    Require(emojiRootProvider != nullptr, "emoji text pattern test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> emojiProvider =
        GetProviderAtDipPoint(emojiWindow.Hwnd(), emojiWindow.Host(), *emojiRootProvider.get(), 40.0f, 44.0f, "emoji text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> emojiSimple;
    RequireSucceeded(emojiProvider.query_to(emojiSimple.put()), "emoji text field provider exposes IRawElementProviderSimple");
    wil::com_ptr_nothrow<IUnknown> emojiTextPatternUnknown;
    RequireSucceeded(emojiSimple->GetPatternProvider(UIA_TextPatternId, emojiTextPatternUnknown.put()), "emoji text field TextPattern lookup succeeds");
    Require(emojiTextPatternUnknown != nullptr, "emoji text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> emojiTextPattern;
    RequireSucceeded(emojiTextPatternUnknown.query_to(emojiTextPattern.put()), "emoji text field TextPattern supports ITextProvider");
    wil::com_ptr_nothrow<ITextRangeProvider> emojiDocumentRange;
    RequireSucceeded(emojiTextPattern->get_DocumentRange(emojiDocumentRange.put()), "emoji text field TextPattern exposes a document range");
    Require(emojiDocumentRange != nullptr, "emoji text field TextPattern returns a document range");
    Require(ReadTextRangeText(*emojiDocumentRange.get(), -1, "emoji text field document range exposes text") == emojiText,
            "emoji text field TextPattern document range returns the current value");
    moved = 0;
    RequireSucceeded(emojiDocumentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 2, &moved),
                     "emoji text field TextPattern start endpoint moves by text elements");
    Require(moved == 2, "emoji text field TextPattern start endpoint reports moved text elements");
    Require(ReadTextRangeText(*emojiDocumentRange.get(), -1, "emoji text field document range exposes text-element moved text") ==
                emojiText.substr(emojiText.size() - 1u),
            "emoji text field TextPattern character movement treats the ZWJ emoji cluster as one text element");
    moved = 0;
    RequireSucceeded(emojiDocumentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, -1, &moved),
                     "emoji text field TextPattern start endpoint moves backward by text element");
    Require(moved == -1, "emoji text field TextPattern start endpoint reports moved text element backward");
    Require(ReadTextRangeText(*emojiDocumentRange.get(), -1, "emoji text field document range exposes backward text-element moved text") ==
                emojiText.substr(1u),
            "emoji text field TextPattern backward character movement restores the full ZWJ emoji cluster");
    wil::com_ptr_nothrow<ITextRangeProvider> emojiEnclosingCharacterRange;
    RequireSucceeded(emojiTextPattern->get_DocumentRange(emojiEnclosingCharacterRange.put()),
                     "emoji text field TextPattern exposes a fresh range for character expansion");
    moved = 0;
    RequireSucceeded(emojiEnclosingCharacterRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 1, &moved),
                     "emoji character-expansion range moves to the ZWJ cluster");
    RequireSucceeded(emojiEnclosingCharacterRange->ExpandToEnclosingUnit(TextUnit_Character), "emoji range expands to the enclosing character unit");
    Require(ReadTextRangeText(*emojiEnclosingCharacterRange.get(), -1, "emoji character-expanded range exposes text") == emojiText.substr(1u, 5u),
            "character expansion keeps the complete ZWJ emoji cluster as one UIA character");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "text field TextPattern selection lookup succeeds");
    const auto destroySelectionRanges                      = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange = GetSingleTextRangeFromArray(selectionRanges, "text field TextPattern exposes one selection range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "text field selected range exposes text") == L"alpha",
            "text field TextPattern selection range exposes the retained selection");
    wil::com_ptr_nothrow<ITextRangeProvider> selectedWordRange;
    RequireSucceeded(selectedRange->Clone(selectedWordRange.put()), "text field selected range clones for word-range movement");
    Require(selectedWordRange != nullptr, "text field selected word movement range clone is returned");
    moved = 0;
    RequireSucceeded(selectedWordRange->Move(TextUnit_Word, 1, &moved), "text field selected range moves by word");
    Require(moved == 1, "text field selected range reports moved words");
    Require(ReadTextRangeText(*selectedWordRange.get(), -1, "text field selected range exposes word-moved text") == L"beta",
            "text field selected range word movement lands on the next word");
    moved = 0;
    RequireSucceeded(selectedRange->Move(TextUnit_Character, 1, &moved), "text field selected range moves by character");
    Require(moved == 1, "text field selected range reports moved characters");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "text field selected range exposes moved text") == L"l",
            "text field selected character movement normalizes to one requested text unit");
    RequireSucceeded(selectedRange->Select(), "text field selected range Select succeeds");
    const std::optional<std::pair<size_t, size_t>> selectedAfterRangeSelect = field->GetSelectionRange();
    Require(selectedAfterRangeSelect.has_value(), "text field selected range Select applies a retained selection");
    Require(selectedAfterRangeSelect.value().first == 1u && selectedAfterRangeSelect.value().second == 2u,
            "text field selected range Select applies the UIA range to the retained TextField");
    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "text field selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles              = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues = ReadDoubleArray(selectedRectangles, "text field selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() >= 4u && selectedRectangleValues.size() % 4u == 0u,
            "text field selected range returns complete bounding rectangle tuples");
    Require(selectedRectangleValues[2] > 0.0 && selectedRectangleValues[3] > 0.0, "text field selected range returns a non-empty bounding rectangle");

    wil::com_ptr_nothrow<IUnknown> textEditPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextEditPatternId, textEditPatternUnknown.put()), "text field TextEditPattern lookup succeeds");
    Require(textEditPatternUnknown != nullptr, "text field exposes TextEditPattern");
    wil::com_ptr_nothrow<ITextEditProvider> textEditPattern;
    RequireSucceeded(textEditPatternUnknown.query_to(textEditPattern.put()), "text field TextEditPattern supports ITextEditProvider");
    wil::com_ptr_nothrow<ITextRangeProvider> activeComposition;
    RequireSucceeded(textEditPattern->GetActiveComposition(activeComposition.put()), "inactive TextEditPattern active-composition lookup succeeds");
    Require(activeComposition == nullptr, "inactive TextEditPattern has no active composition range");

    wil::com_ptr_nothrow<IRawElementProviderFragment> maskedProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 116.0f, "masked text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> maskedSimple;
    RequireSucceeded(maskedProvider.query_to(maskedSimple.put()), "masked text field provider exposes IRawElementProviderSimple");
    wil::com_ptr_nothrow<IUnknown> maskedTextPatternUnknown;
    RequireSucceeded(maskedSimple->GetPatternProvider(UIA_TextPatternId, maskedTextPatternUnknown.put()), "masked text field TextPattern lookup succeeds");
    Require(maskedTextPatternUnknown == nullptr, "masked text field does not expose TextPattern over protected content");
    wil::com_ptr_nothrow<IUnknown> maskedTextEditPatternUnknown;
    RequireSucceeded(maskedSimple->GetPatternProvider(UIA_TextEditPatternId, maskedTextEditPatternUnknown.put()),
                     "masked text field TextEditPattern lookup succeeds");
    Require(maskedTextEditPatternUnknown == nullptr, "masked text field does not expose TextEditPattern over protected content");

    wil::com_ptr_nothrow<IRawElementProviderFragment> multilineProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 148.0f, "multiline text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> multilineSimple;
    RequireSucceeded(multilineProvider.query_to(multilineSimple.put()), "multiline text field provider exposes IRawElementProviderSimple");
    wil::com_ptr_nothrow<IUnknown> multilineValuePatternUnknown;
    RequireSucceeded(multilineSimple->GetPatternProvider(UIA_ValuePatternId, multilineValuePatternUnknown.put()),
                     "multiline text field ValuePattern lookup succeeds");
    Require(multilineValuePatternUnknown == nullptr, "multiline text field does not expose ValuePattern");
    wil::com_ptr_nothrow<IUnknown> multilineTextPatternUnknown;
    RequireSucceeded(multilineSimple->GetPatternProvider(UIA_TextPatternId, multilineTextPatternUnknown.put()),
                     "multiline text field TextPattern lookup succeeds");
    Require(multilineTextPatternUnknown != nullptr, "multiline text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> multilineTextPattern;
    RequireSucceeded(multilineTextPatternUnknown.query_to(multilineTextPattern.put()), "multiline text field TextPattern supports ITextProvider");
    wil::com_ptr_nothrow<ITextRangeProvider> multilineDocumentRange;
    RequireSucceeded(multilineTextPattern->get_DocumentRange(multilineDocumentRange.put()), "multiline text field TextPattern exposes a document range");
    Require(multilineDocumentRange != nullptr, "multiline text field TextPattern returns a document range");
    moved = 0;
    RequireSucceeded(multilineDocumentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Line, 1, &moved),
                     "multiline text field start endpoint moves by line");
    Require(moved == 1, "multiline text field start endpoint reports moved lines");
    Require(ReadTextRangeText(*multilineDocumentRange.get(), -1, "multiline text field document range exposes moved-line text") == L"green\nblue",
            "multiline text field line endpoint movement narrows to the next logical line");
    moved = 0;
    RequireSucceeded(multilineDocumentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Line, -1, &moved),
                     "multiline text field start endpoint moves backward by line");
    Require(moved == -1, "multiline text field start endpoint reports moved lines backward");
    Require(ReadTextRangeText(*multilineDocumentRange.get(), -1, "multiline text field document range restores after moved-line text") == L"red\ngreen\nblue",
            "multiline text field line endpoint movement restores the document text");
    wil::com_ptr_nothrow<ITextRangeProvider> enclosingLineRange;
    RequireSucceeded(multilineTextPattern->get_DocumentRange(enclosingLineRange.put()), "multiline text field exposes a fresh range for line expansion");
    moved = 0;
    RequireSucceeded(enclosingLineRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                     "multiline line-expansion range moves inside the second line");
    RequireSucceeded(enclosingLineRange->ExpandToEnclosingUnit(TextUnit_Line), "multiline range expands to the enclosing line unit");
    Require(ReadTextRangeText(*enclosingLineRange.get(), -1, "multiline line-expanded range exposes text") == L"green",
            "line expansion normalizes a longer range to the logical line containing its start");
    SAFEARRAY* multilineSelectionRanges = nullptr;
    RequireSucceeded(multilineTextPattern->GetSelection(&multilineSelectionRanges), "multiline text field TextPattern selection lookup succeeds");
    const auto destroyMultilineSelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(multilineSelectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> multilineSelectedRange =
        GetSingleTextRangeFromArray(multilineSelectionRanges, "multiline text field TextPattern exposes one selection range");
    moved = 0;
    RequireSucceeded(multilineSelectedRange->Move(TextUnit_Line, 1, &moved), "multiline selected range moves by line");
    Require(moved == 1, "multiline selected range reports moved lines");
    Require(ReadTextRangeText(*multilineSelectedRange.get(), -1, "multiline selected range exposes line-moved text") == L"green",
            "multiline selected range line movement lands on the next logical line");
}

void TestAccessibilityTextFieldSimpleRangeBoundingRectanglesUseCaretGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta gamma");
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 420.0f, 56.0f));
    field->SetSelectionRange(6u, 10u);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "simple range rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 80.0f, 40.0f, "simple text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "simple text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "simple text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "simple text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "simple text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "simple text field selection lookup succeeds");
    const auto destroySelectionRanges                      = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange = GetSingleTextRangeFromArray(selectionRanges, "simple text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "simple selected range exposes text") == L"beta",
            "simple selected range preserves logical selected text");

    D2D1_RECT_F startRectDip{};
    D2D1_RECT_F endRectDip{};
    Require(field->DebugGetCaretRect(window.Host(), 6u, startRectDip), "simple selected range measures the start caret");
    Require(field->DebugGetCaretRect(window.Host(), 10u, endRectDip), "simple selected range measures the end caret");
    const RECT expectedScreen = DipRectToScreenRect(window, UnionRects(startRectDip, endRectDip));

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "simple selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles              = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues = ReadDoubleArray(selectedRectangles, "simple selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == 4u, "simple selected range returns one caret-geometry rectangle tuple");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[0]),
                     static_cast<float>(expectedScreen.left),
                     1.0f,
                     "simple selected range rectangle follows the native start caret x");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[1]),
                     static_cast<float>(expectedScreen.top),
                     1.0f,
                     "simple selected range rectangle follows the native caret top");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[2]),
                     static_cast<float>(expectedScreen.right - expectedScreen.left),
                     1.0f,
                     "simple selected range rectangle width follows native caret geometry");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[3]),
                     static_cast<float>(expectedScreen.bottom - expectedScreen.top),
                     1.0f,
                     "simple selected range rectangle height follows native caret geometry");
}

void TestAccessibilityTextFieldMultilineRangeFromPointUsesNativeHitTest()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"one\ntwo three\nfour");
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 280.0f, 112.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "multiline RangeFromPoint test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "multiline text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "multiline text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "multiline text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "multiline text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "multiline text field TextPattern supports ITextProvider");

    D2D1_RECT_F caretRectDip{};
    Require(field->DebugGetCaretRect(window.Host(), 8u, caretRectDip), "multiline RangeFromPoint test measures the target caret");
    const POINT queryPoint = window.Host().DipPointToScreenPoint(D2D1::Point2F(caretRectDip.left, (caretRectDip.top + caretRectDip.bottom) * 0.5f));
    const UiaPoint rangePoint{static_cast<double>(queryPoint.x), static_cast<double>(queryPoint.y)};

    wil::com_ptr_nothrow<ITextRangeProvider> pointRange;
    RequireSucceeded(textPattern->RangeFromPoint(rangePoint, pointRange.put()), "multiline text field RangeFromPoint succeeds");
    Require(pointRange != nullptr, "multiline text field RangeFromPoint returns a range");
    int moved = 0;
    RequireSucceeded(pointRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved),
                     "multiline RangeFromPoint caret range expands by one character");
    Require(moved == 1, "multiline RangeFromPoint caret range reports one expanded character");
    Require(ReadTextRangeText(*pointRange.get(), -1, "multiline RangeFromPoint expanded range exposes text") == L"t",
            "multiline RangeFromPoint maps the second-line point to the native logical ACP");
}

void TestAccessibilityTextRangeFromPointDispatchesToWindowThread()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"one\ntwo three\nfour");
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 280.0f, 112.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "cross-thread RangeFromPoint test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider = GetProviderAtDipPoint(
        window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "cross-thread RangeFromPoint field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "cross-thread RangeFromPoint provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "cross-thread RangeFromPoint TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "cross-thread RangeFromPoint field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "cross-thread RangeFromPoint TextPattern supports ITextProvider");

    D2D1_RECT_F caretRectDip{};
    Require(field->DebugGetCaretRect(window.Host(), 8u, caretRectDip), "cross-thread RangeFromPoint test measures the target caret");
    const POINT queryPoint = window.Host().DipPointToScreenPoint(D2D1::Point2F(caretRectDip.left, (caretRectDip.top + caretRectDip.bottom) * 0.5f));
    const UiaPoint rangePoint{static_cast<double>(queryPoint.x), static_cast<double>(queryPoint.y)};

    constexpr HRESULT kPendingRange = E_PENDING;
    std::atomic<bool> workerStarted{false};
    std::atomic<HRESULT> rangeResult{kPendingRange};
    ITextRangeProvider* returnedRange = nullptr;
    std::thread worker([&]
    {
        workerStarted.store(true, std::memory_order_release);
        const HRESULT hr = textPattern->RangeFromPoint(rangePoint, &returnedRange);
        rangeResult.store(hr, std::memory_order_release);
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "cross-thread RangeFromPoint worker starts");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (rangeResult.load(std::memory_order_acquire) != kPendingRange)
    {
        worker.join();
        Require(false, "cross-thread RangeFromPoint waits for host window-thread dispatch");
    }

    const auto rangeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (rangeResult.load(std::memory_order_acquire) == kPendingRange && std::chrono::steady_clock::now() < rangeDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT result = rangeResult.load(std::memory_order_acquire);
    worker.join();
    Require(result != kPendingRange, "cross-thread RangeFromPoint completes after host window-thread dispatch");
    RequireSucceeded(result, "cross-thread RangeFromPoint succeeds after dispatch");
    wil::com_ptr_nothrow<ITextRangeProvider> pointRange;
    pointRange.attach(returnedRange);
    Require(pointRange != nullptr, "cross-thread RangeFromPoint returns a range after dispatch");

    int moved = 0;
    RequireSucceeded(pointRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved),
                     "cross-thread RangeFromPoint returned range expands by one character");
    Require(moved == 1, "cross-thread RangeFromPoint range expands by one character");
    const std::wstring expandedText = ReadTextRangeText(*pointRange.get(), -1, "cross-thread RangeFromPoint expanded range exposes text");
    Require(expandedText == L"t", "cross-thread RangeFromPoint maps the second-line point to the native logical ACP");
}

void TestAccessibilityTextFieldMultilineSameLineRangeBoundingRectanglesUseCaretGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"one\ntwo three\nfour");
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 280.0f, 112.0f));
    field->SetSelectionRange(4u, 13u);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "multiline same-line rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "multiline same-line text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "multiline same-line text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "multiline same-line text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "multiline same-line text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "multiline same-line text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "multiline same-line text field selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "multiline same-line text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "multiline same-line selected range exposes text") == L"two three",
            "multiline same-line selected range preserves logical selected text");

    D2D1_RECT_F startRectDip{};
    D2D1_RECT_F endRectDip{};
    Require(field->DebugGetCaretRect(window.Host(), 4u, startRectDip), "multiline same-line selected range measures the start caret");
    Require(field->DebugGetCaretRect(window.Host(), 13u, endRectDip), "multiline same-line selected range measures the end caret");
    const RECT expectedScreen = DipRectToScreenRect(window, UnionRects(startRectDip, endRectDip));

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "multiline same-line selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "multiline same-line selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == 4u, "multiline same-line selected range returns one caret-geometry rectangle tuple");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[0]),
                     static_cast<float>(expectedScreen.left),
                     1.0f,
                     "multiline same-line selected range rectangle follows the native start caret x");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[1]),
                     static_cast<float>(expectedScreen.top),
                     1.0f,
                     "multiline same-line selected range rectangle follows the native caret top");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[2]),
                     static_cast<float>(expectedScreen.right - expectedScreen.left),
                     1.0f,
                     "multiline same-line selected range rectangle width follows native caret geometry");
    RequireFloatNear(static_cast<float>(selectedRectangleValues[3]),
                     static_cast<float>(expectedScreen.bottom - expectedScreen.top),
                     1.0f,
                     "multiline same-line selected range rectangle height follows native caret geometry");
}

void TestAccessibilityTextFieldMultilineRangeBoundingRectanglesUseLineCaretGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"one\ntwo three\nfour");
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 280.0f, 112.0f));
    field->SetSelectionRange(1u, 7u);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "multiline cross-line rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "multiline cross-line text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "multiline cross-line text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "multiline cross-line text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "multiline cross-line text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "multiline cross-line text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "multiline cross-line text field selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "multiline cross-line text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "multiline cross-line selected range exposes text") == L"ne\ntwo",
            "multiline cross-line selected range preserves logical selected text");

    D2D1_RECT_F firstStartRectDip{};
    D2D1_RECT_F firstEndRectDip{};
    D2D1_RECT_F secondStartRectDip{};
    D2D1_RECT_F secondEndRectDip{};
    Require(field->DebugGetCaretRect(window.Host(), 1u, firstStartRectDip), "multiline cross-line selected range measures first-line start caret");
    Require(field->DebugGetCaretRect(window.Host(), 3u, firstEndRectDip), "multiline cross-line selected range measures first-line end caret");
    Require(field->DebugGetCaretRect(window.Host(), 4u, secondStartRectDip), "multiline cross-line selected range measures second-line start caret");
    Require(field->DebugGetCaretRect(window.Host(), 7u, secondEndRectDip), "multiline cross-line selected range measures second-line end caret");
    const std::array<RECT, 2> expectedScreenRects{
        DipRectToScreenRect(window, UnionRects(firstStartRectDip, firstEndRectDip)),
        DipRectToScreenRect(window, UnionRects(secondStartRectDip, secondEndRectDip)),
    };

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "multiline cross-line selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "multiline cross-line selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == 8u, "multiline cross-line selected range returns one rectangle tuple per logical line");
    for (size_t rectIndex = 0u; rectIndex < expectedScreenRects.size(); ++rectIndex)
    {
        const size_t valueIndex = rectIndex * 4u;
        const RECT& expected    = expectedScreenRects[rectIndex];
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex]),
                         static_cast<float>(expected.left),
                         1.0f,
                         "multiline cross-line selected range rectangle follows the native line start caret x");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 1u]),
                         static_cast<float>(expected.top),
                         1.0f,
                         "multiline cross-line selected range rectangle follows the native line caret top");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 2u]),
                         static_cast<float>(expected.right - expected.left),
                         1.0f,
                         "multiline cross-line selected range rectangle width follows native line caret geometry");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 3u]),
                         static_cast<float>(expected.bottom - expected.top),
                         1.0f,
                         "multiline cross-line selected range rectangle height follows native line caret geometry");
    }
}

void TestAccessibilityTextFieldWrappedRangeBoundingRectanglesUseVisualLineGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    constexpr std::wstring_view kWrappedText = L"alpha beta gamma delta epsilon zeta eta theta";
    auto root                                = std::make_unique<Panel>();
    auto* field                              = root->AddChild<TextField>(std::wstring(kWrappedText));
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 150.0f, 140.0f));
    field->SetSelectionRange(0u, kWrappedText.size());
    window.Host().SetRoot(std::move(root));

    TextFieldDebugMultilineState multilineState{};
    Require(field->DebugGetMultilineState(window.Host(), multilineState), "wrapped selected range reads multiline debug state");
    Require(multilineState.totalLineCount > 1u, "wrapped selected range fixture wraps onto multiple visual lines");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "wrapped rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "wrapped text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "wrapped text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "wrapped text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "wrapped text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "wrapped text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "wrapped text field selection lookup succeeds");
    const auto destroySelectionRanges                      = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange = GetSingleTextRangeFromArray(selectionRanges, "wrapped text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "wrapped selected range exposes text") == kWrappedText,
            "wrapped selected range preserves the logical selected text");

    D2D1_RECT_F firstCaretDip{};
    D2D1_RECT_F lastCaretDip{};
    Require(field->DebugGetCaretRect(window.Host(), 0u, firstCaretDip), "wrapped selected range measures the first caret");
    Require(field->DebugGetCaretRect(window.Host(), kWrappedText.size(), lastCaretDip), "wrapped selected range measures the final caret");
    const RECT firstCaretScreen = DipRectToScreenRect(window, firstCaretDip);
    const RECT lastCaretScreen  = DipRectToScreenRect(window, lastCaretDip);
    Require(firstCaretScreen.top != lastCaretScreen.top, "wrapped selected range starts and ends on different visual lines");

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "wrapped selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles              = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues = ReadDoubleArray(selectedRectangles, "wrapped selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() >= 8u, "wrapped selected range returns multiple rectangle tuples");
    Require(selectedRectangleValues.size() % 4u == 0u, "wrapped selected range returns complete rectangle tuples");

    RequireFloatNear(static_cast<float>(selectedRectangleValues[1]),
                     static_cast<float>(firstCaretScreen.top),
                     1.0f,
                     "wrapped selected range first rectangle follows the first visual-line caret top");
    const size_t lastTuple = selectedRectangleValues.size() - 4u;
    RequireFloatNear(static_cast<float>(selectedRectangleValues[lastTuple + 1u]),
                     static_cast<float>(lastCaretScreen.top),
                     1.0f,
                     "wrapped selected range last rectangle follows the final visual-line caret top");
}

void TestAccessibilityTextFieldWrappedCrossLineRangeBoundingRectanglesUseVisualLineGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    constexpr std::wstring_view kWrappedText = L"alpha beta gamma delta epsilon zeta\nomega";
    auto root                                = std::make_unique<Panel>();
    auto* field                              = root->AddChild<TextField>(std::wstring(kWrappedText));
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 18.0f, 150.0f, 150.0f));
    field->SetSelectionRange(0u, kWrappedText.size());
    window.Host().SetRoot(std::move(root));

    TextFieldDebugMultilineState multilineState{};
    Require(field->DebugGetMultilineState(window.Host(), multilineState), "wrapped cross-line range reads multiline debug state");
    Require(multilineState.totalLineCount > 2u, "wrapped cross-line range fixture wraps one logical line and includes a newline");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "wrapped cross-line rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "wrapped cross-line text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "wrapped cross-line text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "wrapped cross-line text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "wrapped cross-line text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "wrapped cross-line text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "wrapped cross-line text field selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "wrapped cross-line text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "wrapped cross-line selected range exposes text") == kWrappedText,
            "wrapped cross-line selected range preserves the logical selected text");

    D2D1_RECT_F firstCaretDip{};
    D2D1_RECT_F lastCaretDip{};
    Require(field->DebugGetCaretRect(window.Host(), 0u, firstCaretDip), "wrapped cross-line selected range measures the first caret");
    Require(field->DebugGetCaretRect(window.Host(), kWrappedText.size(), lastCaretDip), "wrapped cross-line selected range measures the final caret");
    const RECT firstCaretScreen = DipRectToScreenRect(window, firstCaretDip);
    const RECT lastCaretScreen  = DipRectToScreenRect(window, lastCaretDip);
    Require(firstCaretScreen.top != lastCaretScreen.top, "wrapped cross-line selected range starts and ends on different visual lines");

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "wrapped cross-line selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "wrapped cross-line selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() >= 12u, "wrapped cross-line selected range returns visual-line tuples across the newline");
    Require(selectedRectangleValues.size() % 4u == 0u, "wrapped cross-line selected range returns complete rectangle tuples");

    RequireFloatNear(static_cast<float>(selectedRectangleValues[1]),
                     static_cast<float>(firstCaretScreen.top),
                     1.0f,
                     "wrapped cross-line first rectangle follows the first visual-line caret top");
    const size_t lastTuple = selectedRectangleValues.size() - 4u;
    RequireFloatNear(static_cast<float>(selectedRectangleValues[lastTuple + 1u]),
                     static_cast<float>(lastCaretScreen.top),
                     1.0f,
                     "wrapped cross-line last rectangle follows the final visual-line caret top");
}

void TestAccessibilityTextFieldWrappedLineMovementUsesVisualLines()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    const std::wstring text = L"alpha beta gamma delta epsilon zeta";
    auto root               = std::make_unique<Panel>();
    auto* field             = root->AddChild<TextField>(text);
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 118.0f, 112.0f));
    window.Host().SetRoot(std::move(root));

    const std::vector<size_t> visualLineStarts =
        ResolveVisualLineStarts(window.Host(), *field, text, "wrapped line movement test resolves native visual-line starts");
    Require(visualLineStarts.size() >= 3u, "wrapped line movement fixture creates at least three visual lines");
    const size_t secondLineStart = visualLineStarts[1];
    const size_t thirdLineStart  = visualLineStarts[2];
    Require(secondLineStart > 0u && secondLineStart < thirdLineStart && thirdLineStart < text.size(),
            "wrapped line movement fixture exposes stable wrapped visual-line boundaries");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "wrapped line movement test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "wrapped line movement field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "wrapped line movement provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "wrapped line movement TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "wrapped line movement field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "wrapped line movement TextPattern supports ITextProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "wrapped line movement document range lookup succeeds");
    int moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Line, 1, &moved),
                     "wrapped line movement start endpoint moves by visual line");
    Require(moved == 1, "wrapped line movement endpoint reports one visual line");
    Require(ReadTextRangeText(*documentRange.get(), -1, "wrapped line movement document range exposes moved text") == text.substr(secondLineStart),
            "wrapped line movement endpoint lands on the second visual line");

    field->SetSelectionRange(0u, secondLineStart);
    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "wrapped line movement selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "wrapped line movement TextPattern exposes one selection range");
    moved = 0;
    RequireSucceeded(selectedRange->Move(TextUnit_Line, 1, &moved), "wrapped selected range moves by visual line");
    Require(moved == 1, "wrapped selected range reports one visual line");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "wrapped selected range exposes moved visual-line text") ==
                text.substr(secondLineStart, thirdLineStart - secondLineStart),
            "wrapped selected range lands on the next visual line span");
}

void TestAccessibilityTextRangeEndpointLineMovementDispatchesToWindowThread()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    const std::wstring text = L"alpha beta gamma delta epsilon zeta";
    auto root               = std::make_unique<Panel>();
    auto* field             = root->AddChild<TextField>(text);
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 118.0f, 112.0f));
    window.Host().SetRoot(std::move(root));

    const std::vector<size_t> visualLineStarts =
        ResolveVisualLineStarts(window.Host(), *field, text, "cross-thread endpoint line movement resolves native visual-line starts");
    Require(visualLineStarts.size() >= 2u, "cross-thread endpoint line movement fixture creates wrapped visual lines");
    const size_t secondLineStart = visualLineStarts[1];

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "cross-thread endpoint line movement creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider = GetProviderAtDipPoint(
        window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "cross-thread endpoint line movement field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "cross-thread endpoint line movement provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "cross-thread endpoint line movement TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "cross-thread endpoint line movement field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "cross-thread endpoint line movement TextPattern supports ITextProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "cross-thread endpoint line movement TextPattern exposes a document range");

    constexpr HRESULT kPendingMove = E_PENDING;
    std::atomic<bool> workerStarted{false};
    std::atomic<HRESULT> moveResult{kPendingMove};
    std::atomic<int> movedResult{0};
    std::thread worker([&]
    {
        int moved = 0;
        workerStarted.store(true, std::memory_order_release);
        const HRESULT hr = documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Line, 1, &moved);
        movedResult.store(moved, std::memory_order_release);
        moveResult.store(hr, std::memory_order_release);
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "cross-thread TextRange endpoint line movement worker starts");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (moveResult.load(std::memory_order_acquire) != kPendingMove)
    {
        worker.join();
        Require(false, "cross-thread TextRange endpoint line movement waits for host window-thread dispatch");
    }

    const auto moveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (moveResult.load(std::memory_order_acquire) == kPendingMove && std::chrono::steady_clock::now() < moveDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT result = moveResult.load(std::memory_order_acquire);
    worker.join();
    Require(result != kPendingMove, "cross-thread TextRange endpoint line movement completes after host window-thread dispatch");
    RequireSucceeded(result, "cross-thread TextRange endpoint line movement succeeds after dispatch");
    Require(movedResult.load(std::memory_order_acquire) == 1, "cross-thread TextRange endpoint line movement reports one visual line");
    Require(ReadTextRangeText(*documentRange.get(), -1, "cross-thread endpoint line movement exposes moved text") == text.substr(secondLineStart),
            "cross-thread TextRange endpoint line movement lands on the second visual line");

    wil::com_ptr_nothrow<ITextRangeProvider> expandingRange;
    RequireSucceeded(textPattern->get_DocumentRange(expandingRange.put()), "cross-thread line expansion gets a fresh document range");
    int movedInsideLine = 0;
    RequireSucceeded(
        expandingRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, static_cast<int>(secondLineStart + 1u), &movedInsideLine),
        "cross-thread line expansion moves the range start inside the second visual line");

    constexpr HRESULT kPendingExpansion = E_PENDING;
    std::atomic<bool> expansionWorkerStarted{false};
    std::atomic<HRESULT> expansionResult{kPendingExpansion};
    std::thread expansionWorker([&]
    {
        expansionWorkerStarted.store(true, std::memory_order_release);
        expansionResult.store(expandingRange->ExpandToEnclosingUnit(TextUnit_Line), std::memory_order_release);
    });

    const auto expansionStartDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! expansionWorkerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < expansionStartDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(expansionWorkerStarted.load(std::memory_order_acquire), "cross-thread TextRange line expansion worker starts");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (expansionResult.load(std::memory_order_acquire) != kPendingExpansion)
    {
        expansionWorker.join();
        Require(false, "cross-thread TextRange line expansion waits for host window-thread dispatch");
    }

    const auto expansionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (expansionResult.load(std::memory_order_acquire) == kPendingExpansion && std::chrono::steady_clock::now() < expansionDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT expandedResult = expansionResult.load(std::memory_order_acquire);
    expansionWorker.join();
    Require(expandedResult != kPendingExpansion, "cross-thread TextRange line expansion completes after host window-thread dispatch");
    RequireSucceeded(expandedResult, "cross-thread TextRange line expansion succeeds after dispatch");
    const size_t thirdLineStart = visualLineStarts.size() > 2u ? visualLineStarts[2] : text.size();
    Require(ReadTextRangeText(*expandingRange.get(), -1, "cross-thread line-expanded range exposes text") ==
                text.substr(secondLineStart, thirdLineStart - secondLineStart),
            "cross-thread TextRange line expansion normalizes to the visual line containing its start");
}

void TestAccessibilityTextRangeSpanLineMovementDispatchesToWindowThread()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    const std::wstring text = L"alpha beta gamma delta epsilon zeta";
    auto root               = std::make_unique<Panel>();
    auto* field             = root->AddChild<TextField>(text);
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 118.0f, 112.0f));
    window.Host().SetRoot(std::move(root));

    const std::vector<size_t> visualLineStarts =
        ResolveVisualLineStarts(window.Host(), *field, text, "cross-thread span line movement resolves native visual-line starts");
    Require(visualLineStarts.size() >= 3u, "cross-thread span line movement fixture creates wrapped visual lines");
    const size_t secondLineStart = visualLineStarts[1];
    const size_t thirdLineStart  = visualLineStarts[2];

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "cross-thread span line movement creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider = GetProviderAtDipPoint(
        window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 40.0f, "cross-thread span line movement field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "cross-thread span line movement provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "cross-thread span line movement TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "cross-thread span line movement field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "cross-thread span line movement TextPattern supports ITextProvider");

    field->SetSelectionRange(0u, secondLineStart);
    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "cross-thread span line movement selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "cross-thread span line movement TextPattern exposes one selection range");

    constexpr HRESULT kPendingMove = E_PENDING;
    std::atomic<bool> workerStarted{false};
    std::atomic<HRESULT> moveResult{kPendingMove};
    std::atomic<int> movedResult{0};
    std::thread worker([&]
    {
        int moved = 0;
        workerStarted.store(true, std::memory_order_release);
        const HRESULT hr = selectedRange->Move(TextUnit_Line, 1, &moved);
        movedResult.store(moved, std::memory_order_release);
        moveResult.store(hr, std::memory_order_release);
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "cross-thread TextRange span line movement worker starts");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (moveResult.load(std::memory_order_acquire) != kPendingMove)
    {
        worker.join();
        Require(false, "cross-thread TextRange span line movement waits for host window-thread dispatch");
    }

    const auto moveDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (moveResult.load(std::memory_order_acquire) == kPendingMove && std::chrono::steady_clock::now() < moveDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT result = moveResult.load(std::memory_order_acquire);
    worker.join();
    Require(result != kPendingMove, "cross-thread TextRange span line movement completes after host window-thread dispatch");
    RequireSucceeded(result, "cross-thread TextRange span line movement succeeds after dispatch");
    Require(movedResult.load(std::memory_order_acquire) == 1, "cross-thread TextRange span line movement reports one visual line");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "cross-thread span line movement exposes moved text") ==
                text.substr(secondLineStart, thirdLineStart - secondLineStart),
            "cross-thread TextRange span line movement lands on the next visual line span");
}

void TestAccessibilityTextFieldSingleLineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root = std::make_unique<Panel>();
    root->SetFlowDirection(FlowDirection::RightToLeft);
    auto* field = root->AddChild<TextField>(L"abc \x05D0\x05D1\x05D2 123");
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 380.0f, 56.0f));
    field->SetSelectionRange(4u, 7u);
    window.Host().SetRoot(std::move(root));

    const std::optional<std::vector<D2D1_RECT_F>> expectedRectsDip = field->TryGetTextInputRangeRects(window.Host(), 4u, 7u);
    Require(expectedRectsDip.has_value() && ! expectedRectsDip->empty(), "single-line mixed-BiDi selected range has retained DirectWrite range geometry");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "single-line mixed-BiDi range rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider = GetProviderAtDipPoint(
        window.Hwnd(), window.Host(), *rootProvider.get(), 80.0f, 40.0f, "single-line mixed-BiDi text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "single-line mixed-BiDi text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "single-line mixed-BiDi text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "single-line mixed-BiDi text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "single-line mixed-BiDi text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "single-line mixed-BiDi text field selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "single-line mixed-BiDi text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "single-line mixed-BiDi selected range exposes text") == L"\x05D0\x05D1\x05D2",
            "single-line mixed-BiDi selected range preserves logical UTF-16 selected text");

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "single-line mixed-BiDi selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "single-line mixed-BiDi selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == expectedRectsDip->size() * 4u,
            "single-line mixed-BiDi selected range returns the retained DirectWrite rectangle tuple count");
    for (size_t rectIndex = 0u; rectIndex < expectedRectsDip->size(); ++rectIndex)
    {
        const RECT expected     = DipRectToScreenRect(window, expectedRectsDip->at(rectIndex));
        const size_t valueIndex = rectIndex * 4u;
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex]),
                         static_cast<float>(expected.left),
                         1.0f,
                         "single-line mixed-BiDi selected range rectangle uses DirectWrite left edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 1u]),
                         static_cast<float>(expected.top),
                         1.0f,
                         "single-line mixed-BiDi selected range rectangle uses DirectWrite top edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 2u]),
                         static_cast<float>(expected.right - expected.left),
                         1.0f,
                         "single-line mixed-BiDi selected range rectangle uses DirectWrite width");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 3u]),
                         static_cast<float>(expected.bottom - expected.top),
                         1.0f,
                         "single-line mixed-BiDi selected range rectangle uses DirectWrite height");
    }
}

void TestAccessibilityTextFieldMultilineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root = std::make_unique<Panel>();
    root->SetFlowDirection(FlowDirection::RightToLeft);
    auto* field = root->AddChild<TextField>(L"latin \x05D0\x05D1\x05D2 span\nsecond line");
    field->SetMultiline(true);
    field->SetBounds(D2D1::RectF(20.0f, 24.0f, 300.0f, 112.0f));
    field->SetSelectionRange(6u, 9u);
    window.Host().SetRoot(std::move(root));

    const std::optional<std::vector<D2D1_RECT_F>> expectedRectsDip = field->TryGetTextInputRangeRects(window.Host(), 6u, 9u);
    Require(expectedRectsDip.has_value() && ! expectedRectsDip->empty(), "multiline mixed-BiDi selected range has retained DirectWrite range geometry");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "multiline mixed-BiDi range rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 80.0f, 40.0f, "multiline mixed-BiDi text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "multiline mixed-BiDi text field provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()),
                     "multiline mixed-BiDi text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "multiline mixed-BiDi text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "multiline mixed-BiDi text field TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "multiline mixed-BiDi text field selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "multiline mixed-BiDi text field exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "multiline mixed-BiDi selected range exposes text") == L"\x05D0\x05D1\x05D2",
            "multiline mixed-BiDi selected range preserves logical UTF-16 selected text");

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "multiline mixed-BiDi selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "multiline mixed-BiDi selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == expectedRectsDip->size() * 4u,
            "multiline mixed-BiDi selected range returns the retained DirectWrite rectangle tuple count");

    for (size_t index = 0u; index < expectedRectsDip->size(); ++index)
    {
        const RECT expectedScreen = DipRectToScreenRect(window, expectedRectsDip.value()[index]);
        const size_t valueIndex   = index * 4u;
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 0u]),
                         static_cast<float>(expectedScreen.left),
                         1.0f,
                         "multiline mixed-BiDi selected range rectangle uses DirectWrite left edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 1u]),
                         static_cast<float>(expectedScreen.top),
                         1.0f,
                         "multiline mixed-BiDi selected range rectangle uses DirectWrite top edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 2u]),
                         static_cast<float>(expectedScreen.right - expectedScreen.left),
                         1.0f,
                         "multiline mixed-BiDi selected range rectangle uses DirectWrite width");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 3u]),
                         static_cast<float>(expectedScreen.bottom - expectedScreen.top),
                         1.0f,
                         "multiline mixed-BiDi selected range rectangle uses DirectWrite height");
    }
}

void TestAccessibilityEditableComboBoxSingleLineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root = std::make_unique<Panel>();
    root->SetFlowDirection(FlowDirection::RightToLeft);
    auto* combo = root->AddChild<ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"abc \x05D0\x05D1\x05D2 123");
    combo->SetEditableSelectionRange(4u, 7u);
    combo->SetBounds(D2D1::RectF(20.0f, 24.0f, 380.0f, 56.0f));
    window.Host().SetRoot(std::move(root));

    const std::optional<std::vector<D2D1_RECT_F>> expectedRectsDip = combo->TryGetTextInputRangeRects(window.Host(), 4u, 7u);
    Require(expectedRectsDip.has_value() && ! expectedRectsDip->empty(),
            "editable combo single-line mixed-BiDi selected range has retained DirectWrite range geometry");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "editable combo mixed-BiDi range rectangle test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> comboProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 80.0f, 40.0f, "editable combo mixed-BiDi provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> comboSimple;
    RequireSucceeded(comboProvider.query_to(comboSimple.put()), "editable combo mixed-BiDi provider exposes simple provider");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(comboSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "editable combo mixed-BiDi TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "editable combo mixed-BiDi exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "editable combo mixed-BiDi TextPattern supports ITextProvider");

    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "editable combo mixed-BiDi selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "editable combo mixed-BiDi exposes one selected range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "editable combo mixed-BiDi selected range exposes text") == L"\x05D0\x05D1\x05D2",
            "editable combo mixed-BiDi selected range preserves logical UTF-16 selected text");

    SAFEARRAY* selectedRectangles = nullptr;
    RequireSucceeded(selectedRange->GetBoundingRectangles(&selectedRectangles), "editable combo mixed-BiDi selected range bounding rectangles lookup succeeds");
    const auto destroySelectedRectangles = wil::scope_exit([&] { SafeArrayDestroy(selectedRectangles); });
    const std::vector<double> selectedRectangleValues =
        ReadDoubleArray(selectedRectangles, "editable combo mixed-BiDi selected range returns bounding rectangle values");
    Require(selectedRectangleValues.size() == expectedRectsDip->size() * 4u,
            "editable combo mixed-BiDi selected range returns the retained DirectWrite rectangle tuple count");

    for (size_t rectIndex = 0u; rectIndex < expectedRectsDip->size(); ++rectIndex)
    {
        const RECT expected     = DipRectToScreenRect(window, expectedRectsDip->at(rectIndex));
        const size_t valueIndex = rectIndex * 4u;
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex]),
                         static_cast<float>(expected.left),
                         1.0f,
                         "editable combo mixed-BiDi selected range rectangle uses DirectWrite left edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 1u]),
                         static_cast<float>(expected.top),
                         1.0f,
                         "editable combo mixed-BiDi selected range rectangle uses DirectWrite top edge");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 2u]),
                         static_cast<float>(expected.right - expected.left),
                         1.0f,
                         "editable combo mixed-BiDi selected range rectangle uses DirectWrite width");
        RequireFloatNear(static_cast<float>(selectedRectangleValues[valueIndex + 3u]),
                         static_cast<float>(expected.bottom - expected.top),
                         1.0f,
                         "editable combo mixed-BiDi selected range rectangle uses DirectWrite height");
    }
}

void TestAccessibilityProviderExposesTextPatternForEditableComboBox()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root        = std::make_unique<Panel>();
    auto* comboLabel = root->AddChild<Label>(L"Mode");
    comboLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* combo = root->AddChild<ComboBox>();
    combo->SetEditable(true);
    combo->SetText(L"current");
    combo->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    comboLabel->SetMnemonicTarget(combo);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "editable combo text pattern test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> comboProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "editable combo provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> comboSimple;
    RequireSucceeded(comboProvider.query_to(comboSimple.put()), "editable combo provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(comboSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "editable combo TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "editable combo exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "editable combo TextPattern supports ITextProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "editable combo TextPattern exposes a document range");
    Require(documentRange != nullptr, "editable combo TextPattern returns a document range");
    Require(ReadTextRangeText(*documentRange.get(), -1, "editable combo document range exposes text") == L"current",
            "editable combo TextPattern document range returns editable text");

    const D2D1_RECT_F editableTextRect = combo->DebugGetEditableTextRect();
    const POINT rangePointScreen =
        window.Host().DipPointToScreenPoint(D2D1::Point2F(editableTextRect.left + 1.0f, (editableTextRect.top + editableTextRect.bottom) * 0.5f));
    const UiaPoint rangePoint{static_cast<double>(rangePointScreen.x), static_cast<double>(rangePointScreen.y)};
    wil::com_ptr_nothrow<ITextRangeProvider> pointRange;
    RequireSucceeded(textPattern->RangeFromPoint(rangePoint, pointRange.put()), "editable combo TextPattern RangeFromPoint succeeds");
    Require(pointRange != nullptr, "editable combo TextPattern RangeFromPoint returns a range");
    Require(ReadTextRangeText(*pointRange.get(), -1, "editable combo RangeFromPoint range is collapsed").empty(),
            "editable combo TextPattern RangeFromPoint returns a collapsed caret range");
    int moved = 0;
    RequireSucceeded(pointRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved),
                     "editable combo RangeFromPoint range endpoint expands by one character");
    Require(moved == 1, "editable combo RangeFromPoint range reports one expanded character");
    Require(ReadTextRangeText(*pointRange.get(), -1, "editable combo RangeFromPoint expanded range exposes text") == L"c",
            "editable combo TextPattern RangeFromPoint maps the editable text point to the first character");

    Require(combo->OnSelectAll(window.Host()), "editable combo can select all before UIA selection lookup");
    SAFEARRAY* selectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&selectionRanges), "editable combo TextPattern selection lookup succeeds");
    const auto destroySelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(selectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> selectedRange =
        GetSingleTextRangeFromArray(selectionRanges, "editable combo TextPattern exposes one selection range");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "editable combo selected range exposes text") == L"current",
            "editable combo TextPattern selection range exposes the retained editable selection");
    moved = 0;
    RequireSucceeded(selectedRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 2, &moved),
                     "editable combo selected range start moves by character");
    Require(moved == 2, "editable combo selected range start reports moved characters");
    moved = 0;
    RequireSucceeded(selectedRange->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, -2, &moved),
                     "editable combo selected range end moves backward by character");
    Require(moved == -2, "editable combo selected range end reports moved characters");
    Require(ReadTextRangeText(*selectedRange.get(), -1, "editable combo selected range exposes narrowed text") == L"rre",
            "editable combo selected range endpoint movement narrows the range");
    RequireSucceeded(selectedRange->Select(), "editable combo selected range Select succeeds");

    SAFEARRAY* appliedSelectionRanges = nullptr;
    RequireSucceeded(textPattern->GetSelection(&appliedSelectionRanges), "editable combo TextPattern selection lookup succeeds after Select");
    const auto destroyAppliedSelectionRanges = wil::scope_exit([&] { SafeArrayDestroy(appliedSelectionRanges); });
    wil::com_ptr_nothrow<ITextRangeProvider> appliedSelectedRange =
        GetSingleTextRangeFromArray(appliedSelectionRanges, "editable combo TextPattern exposes one applied selection range");
    Require(ReadTextRangeText(*appliedSelectedRange.get(), -1, "editable combo applied selected range exposes text") == L"rre",
            "editable combo TextPattern Select applies the UIA range to retained editable selection");

    wil::com_ptr_nothrow<IUnknown> textEditPatternUnknown;
    RequireSucceeded(comboSimple->GetPatternProvider(UIA_TextEditPatternId, textEditPatternUnknown.put()), "editable combo TextEditPattern lookup succeeds");
    Require(textEditPatternUnknown != nullptr, "editable combo exposes TextEditPattern");
}

void TestAccessibilityTextRangeSelectDispatchesToWindowThread()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    field->SetSelectionRange(0u, 5u);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "cross-thread text range test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "cross-thread text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "cross-thread text field provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "cross-thread text field TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "cross-thread text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "cross-thread text field TextPattern supports ITextProvider");
    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "cross-thread text field TextPattern exposes a document range");

    int moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                     "cross-thread text range start endpoint moves to beta");
    Require(moved == 6, "cross-thread text range start endpoint reports moved characters");

    constexpr HRESULT kPendingSelect = E_PENDING;
    std::atomic<bool> workerStarted{false};
    std::atomic<HRESULT> selectResult{kPendingSelect};
    std::thread worker([&]
    {
        workerStarted.store(true, std::memory_order_release);
        selectResult.store(documentRange->Select(), std::memory_order_release);
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "cross-thread TextRange Select worker starts");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (selectResult.load(std::memory_order_acquire) != kPendingSelect)
    {
        worker.join();
        Require(false, "cross-thread TextRange Select waits for host window-thread dispatch");
    }

    const auto selectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (selectResult.load(std::memory_order_acquire) == kPendingSelect && std::chrono::steady_clock::now() < selectDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT result = selectResult.load(std::memory_order_acquire);
    worker.join();
    Require(result != kPendingSelect, "cross-thread TextRange Select completes after host window-thread dispatch");
    RequireSucceeded(result, "cross-thread TextRange Select succeeds after dispatch");

    const std::optional<std::pair<size_t, size_t>> selectedRange = field->GetSelectionRange();
    Require(selectedRange.has_value(), "cross-thread TextRange Select applies a retained selection");
    Require(selectedRange.value().first == 6u && selectedRange.value().second == 10u,
            "cross-thread TextRange Select applies the range on the host window thread");
}

struct AccessibilityTextRangeSelectFixture
{
    explicit AccessibilityTextRangeSelectFixture(AttachedHostWindow& window)
    {
        using namespace DxUi;

        auto root = std::make_unique<Panel>();
        field     = root->AddChild<TextField>(L"alpha beta");
        field->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
        field->SetSelectionRange(0u, 5u);
        window.Host().SetRoot(std::move(root));

        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "UIA Select dispatch fixture creates a root provider");
        fieldProvider = GetProviderAtDipPoint(
            window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "UIA Select dispatch fixture resolves the text field provider");
        RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "UIA Select dispatch fixture exposes IRawElementProviderSimple");
        RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "UIA Select dispatch fixture gets TextPattern");
        Require(textPatternUnknown != nullptr, "UIA Select dispatch fixture exposes TextPattern");
        RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "UIA Select dispatch fixture gets ITextProvider");
        RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "UIA Select dispatch fixture gets the document range");

        int moved = 0;
        RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                         "UIA Select dispatch fixture moves the range start to beta");
        Require(moved == 6, "UIA Select dispatch fixture reports the moved range start");
    }

    DxUi::TextField* field = nullptr;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider;
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
};

void TestNativeAccessibilityFocusCallbackReplacementStopsOriginalAction()
{
    using namespace DxUi;
    for (const bool invokeAction : {false, true})
    {
        AttachedHostWindow window;
        auto root      = std::make_unique<Panel>();
        auto* original = root->AddChild<Button>(L"Original focus target");
        original->SetBounds(D2D1::RectF(0, 0, 220, 32));
        // Keep the action as an ordinary child, not a collapsed semantic root.
        auto* other = root->AddChild<Button>(L"Other original action");
        other->SetBounds(D2D1::RectF(0, 40, 220, 72));
        unsigned int originalActions    = 0u;
        unsigned int replacementActions = 0u;
        unsigned int replacements       = 0u;
        original->SetOnClick([&] { ++originalActions; });
        window.Host().SetRoot(std::move(root));
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> hostProvider;
        hostProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(hostProvider != nullptr, "focus reentry host provider exists");
        auto fragment = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *hostProvider.get(), 20, 12, "focus reentry original provider");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(fragment.query_to(simple.put()), "focus reentry simple provider");
        wil::com_ptr_nothrow<IUnknown> pattern;
        RequireSucceeded(simple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "focus reentry Invoke lookup");
        Require(pattern != nullptr, "focus reentry original exposes Invoke");
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        RequireSucceeded(pattern.query_to(invoke.put()), "focus reentry Invoke interface");
        Button* replacementFocus = nullptr;
        window.Host().SetOnFocusChanged([&](Control* focused)
        {
            if (focused != original || replacements != 0u)
                return;
            ++replacements;
            // Allocate while the original is alive, then destroy the old tree.
            // The callback's safe focus successor must survive the outer UIA call.
            auto replacement        = std::make_unique<Panel>();
            auto* replacementAction = replacement->AddChild<Button>(L"Replacement action");
            replacementAction->SetBounds(D2D1::RectF(0, 0, 220, 32));
            replacementAction->SetOnClick([&] { ++replacementActions; });
            replacementFocus = replacement->AddChild<Button>(L"Replacement focus successor");
            replacementFocus->SetBounds(D2D1::RectF(0, 40, 220, 72));
            window.Host().SetRoot(std::move(replacement));
            window.Host().SetFocusControl(replacementFocus, false);
        });
        const auto clearCallback = wil::scope_exit([&] { window.Host().SetOnFocusChanged({}); });
        const HWND focusBefore   = GetFocus();
        const HRESULT hr         = invokeAction ? invoke->Invoke() : fragment->SetFocus();
        std::cout << "    [UIA focus reentry] invoke=" << invokeAction << " result=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec
                  << " replacements=" << replacements << " originalActions=" << originalActions << " replacementActions=" << replacementActions << '\n';
        Require(hr == UIA_E_ELEMENTNOTAVAILABLE && replacements == 1u && originalActions == 0u && replacementActions == 0u,
                "focus-triggered tree replacement stops original UIA focus/action without invoking a replacement");
        Require(window.Host().GetFocusControl() == replacementFocus, "outer UIA focus/action preserves callback-selected focus successor");
        if (! DxUiTestWindowsCanActivateFlag())
            Require(GetFocus() == focusBefore, "governed no-activation focus-reentry witness does not change native focus");
    }
}

void TestNativeAccessibilityProvidersRejectReplacementAtSamePath()
{
    using namespace DxUi;
    const auto readRuntimeId = [](IRawElementProviderFragment& provider)
    {
        SAFEARRAY* id = nullptr;
        RequireSucceeded(provider.GetRuntimeId(&id), "native lifetime runtime ID lookup");
        const auto releaseId = wil::scope_exit([&]
        {
            if (id)
                SafeArrayDestroy(id);
        });
        Require(id != nullptr, "native lifetime runtime ID exists");
        LONG first = 0;
        LONG last  = -1;
        RequireSucceeded(SafeArrayGetLBound(id, 1, &first), "native runtime ID lower bound");
        RequireSucceeded(SafeArrayGetUBound(id, 1, &last), "native runtime ID upper bound");
        std::vector<LONG> values;
        for (LONG index = first; index <= last; ++index)
        {
            LONG value = 0;
            RequireSucceeded(SafeArrayGetElement(id, &index, &value), "native runtime ID component");
            values.push_back(value);
        }
        return values;
    };
    for (const bool replaceRoot : {false, true})
    {
        AttachedHostWindow window;
        auto root    = std::make_unique<Panel>();
        Panel* panel = root.get();
        auto* button = panel->AddChild<Button>(L"Original action");
        button->SetBounds(D2D1::RectF(0, 0, 200, 28));
        auto* field = panel->AddChild<TextField>(L"original text");
        field->SetBounds(D2D1::RectF(0, 36, 260, 68));
        unsigned int oldInvocations         = 0u;
        unsigned int replacementInvocations = 0u;
        button->SetOnClick([&] { ++oldInvocations; });
        window.Host().SetRoot(std::move(root));
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> hostProvider;
        hostProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(hostProvider != nullptr, "native lifetime host provider exists");
        auto actionFragment = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *hostProvider.get(), 20, 12, "original native action provider");
        auto fieldFragment  = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *hostProvider.get(), 20, 48, "original native text provider");
        wil::com_ptr_nothrow<IRawElementProviderSimple> actionSimple;
        wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
        RequireSucceeded(actionFragment.query_to(actionSimple.put()), "original action simple provider");
        RequireSucceeded(fieldFragment.query_to(fieldSimple.put()), "original field simple provider");
        wil::com_ptr_nothrow<IUnknown> pattern;
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        RequireSucceeded(actionSimple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "original action Invoke pattern");
        Require(pattern != nullptr, "original action has Invoke");
        RequireSucceeded(pattern.query_to(invoke.put()), "original action Invoke interface");
        RequireSucceeded(invoke->Invoke(), "original live provider invokes its own action");
        Require(oldInvocations == 1u, "original live callback runs once");
        pattern.reset();
        wil::com_ptr_nothrow<IValueProvider> value;
        RequireSucceeded(fieldSimple->GetPatternProvider(UIA_ValuePatternId, pattern.put()), "original field Value pattern");
        Require(pattern != nullptr, "original field has Value");
        RequireSucceeded(pattern.query_to(value.put()), "original field Value interface");
        pattern.reset();
        wil::com_ptr_nothrow<ITextProvider> text;
        RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, pattern.put()), "original field Text pattern");
        Require(pattern != nullptr, "original field has Text");
        RequireSucceeded(pattern.query_to(text.put()), "original field Text interface");
        wil::com_ptr_nothrow<ITextRangeProvider> range;
        RequireSucceeded(text->get_DocumentRange(range.put()), "retain original native document range");
        const auto originalRuntimeId = readRuntimeId(*actionFragment.get());

        button->SetText(L"Original renamed");
        field->SetText(L"original updated");
        window.Host().RefreshAccessibilitySnapshot();
        Require(ReadProviderStringProperty(*actionSimple.get(), UIA_NamePropertyId, "live semantic refresh") == L"Original renamed",
                "same control remains available across semantic snapshot refresh");
        Require(ReadTextRangeText(*range.get(), -1, "live range refresh").starts_with(L"original"),
                "same control text range remains available across text refresh");
        Require(readRuntimeId(*actionFragment.get()) == originalRuntimeId, "unchanged native control retains its runtime identity across refreshed snapshots");

        window.Host().ResetInteractionState();
        std::unique_ptr<Panel> replacementRoot;
        if (replaceRoot)
        {
            replacementRoot = std::make_unique<Panel>();
            panel           = replacementRoot.get();
        }
        else
            panel->ClearChildren();
        auto* replacementButton = panel->AddChild<Button>(L"Replacement action");
        replacementButton->SetBounds(D2D1::RectF(0, 0, 200, 28));
        replacementButton->SetOnClick([&] { ++replacementInvocations; });
        auto* replacementField = panel->AddChild<TextField>(L"replacement secret");
        replacementField->SetBounds(D2D1::RectF(0, 36, 260, 68));
        replacementField->SetSelectionRange(1u, 3u);
        if (replaceRoot)
            window.Host().SetRoot(std::move(replacementRoot));
        window.Host().SetFocusControl(replacementButton, false);
        window.Host().RefreshAccessibilitySnapshot();
        const HWND focusBefore = GetFocus();
        wil::unique_variant staleName;
        const HRESULT propertyHr = actionSimple->GetPropertyValue(UIA_NamePropertyId, &staleName);
        const HRESULT invokeHr   = invoke->Invoke();
        const HRESULT focusHr    = fieldFragment->SetFocus();
        const HRESULT valueHr    = value->SetValue(L"unauthorized replacement");
        wil::unique_bstr staleText;
        const HRESULT rangeTextHr = range->GetText(-1, staleText.put());
        const HRESULT selectHr    = range->Select();
        wil::com_ptr_nothrow<ITextRangeProvider> staleClone;
        const HRESULT cloneHr     = range->Clone(staleClone.put());
        SAFEARRAY* staleRuntimeId = nullptr;
        const HRESULT runtimeIdHr = actionFragment->GetRuntimeId(&staleRuntimeId);
        const auto releaseStaleId = wil::scope_exit([&]
        {
            if (staleRuntimeId)
                SafeArrayDestroy(staleRuntimeId);
        });
        std::cout << "    [UIA lifetime] replaceRoot=" << replaceRoot << " property=0x" << std::hex << static_cast<unsigned long>(propertyHr) << " invoke=0x"
                  << static_cast<unsigned long>(invokeHr) << " focus=0x" << static_cast<unsigned long>(focusHr) << " value=0x"
                  << static_cast<unsigned long>(valueHr) << " rangeText=0x" << static_cast<unsigned long>(rangeTextHr) << " select=0x"
                  << static_cast<unsigned long>(selectHr) << " clone=0x" << static_cast<unsigned long>(cloneHr) << " runtimeId=0x"
                  << static_cast<unsigned long>(runtimeIdHr) << std::dec << " replacementInvocations=" << replacementInvocations << '\n';
        Require(propertyHr == UIA_E_ELEMENTNOTAVAILABLE && invokeHr == UIA_E_ELEMENTNOTAVAILABLE && focusHr == UIA_E_ELEMENTNOTAVAILABLE &&
                    valueHr == UIA_E_ELEMENTNOTAVAILABLE && rangeTextHr == UIA_E_ELEMENTNOTAVAILABLE && selectHr == UIA_E_ELEMENTNOTAVAILABLE &&
                    cloneHr == UIA_E_ELEMENTNOTAVAILABLE && runtimeIdHr == UIA_E_ELEMENTNOTAVAILABLE && ! staleRuntimeId,
                "retained native providers and text ranges reject same-path child/root replacements");
        Require(replacementInvocations == 0u && oldInvocations == 1u && replacementField->GetText() == L"replacement secret" &&
                    replacementField->GetSelectionRange() == std::optional<std::pair<size_t, size_t>>{{1u, 3u}} &&
                    window.Host().GetFocusControl() == replacementButton && GetFocus() == focusBefore,
                "stale native providers cannot read, invoke, edit, select, or focus the replacement");
        auto current = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *hostProvider.get(), 20, 12, "persistent host root finds replacement");
        wil::com_ptr_nothrow<IRawElementProviderSimple> currentSimple;
        RequireSucceeded(current.query_to(currentSimple.put()), "replacement provider exposes properties");
        Require(ReadProviderStringProperty(*currentSimple.get(), UIA_NamePropertyId, "replacement name") == L"Replacement action",
                "fresh native provider reaches replacement through persistent container root");
        Require(readRuntimeId(*current.get()) != originalRuntimeId, "same-path native replacement has a distinct runtime identity for client caches");
    }
}

// A window's root element acquired while the window held no control stands for the host. Once a single control collapses
// into the root, that element is gone for the client holding it, which acquires a fresh root for the control; the old
// root still answers its fragment-root queries, with nothing.
void TestNativeAccessibilityRootRetiresWhenAControlCollapsesIntoIt()
{
    using namespace DxUi;
    AttachedHostWindow window;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> early;
    early.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(early != nullptr, "a window without controls has a root element");
    wil::com_ptr_nothrow<IUnknown> earlyIdentity;
    RequireSucceeded(early.query_to(earlyIdentity.put()), "the early root has a COM identity");

    unsigned int invoked = 0u;
    auto button          = std::make_unique<Button>(L"Collapsed action");
    button->SetOnClick([&] { ++invoked; });
    window.Host().SetRoot(std::move(button));

    wil::com_ptr_nothrow<IRawElementProviderSimple> earlySimple;
    RequireSucceeded(early.query_to(earlySimple.put()), "the early root is a simple provider");
    wil::com_ptr_nothrow<IUnknown> earlyPattern;
    Require(earlySimple->GetPatternProvider(UIA_InvokePatternId, earlyPattern.put()) == UIA_E_ELEMENTNOTAVAILABLE && ! earlyPattern,
            "the root acquired before a control collapsed into it is gone, and invokes nothing");
    wil::com_ptr_nothrow<IRawElementProviderFragment> earlyFocus;
    RequireSucceeded(early->GetFocus(earlyFocus.put()), "the early root still answers its focus query");
    Require(earlyFocus == nullptr, "nothing has focus through the early root");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fresh;
    fresh.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(fresh != nullptr, "the window gives a fresh root once a control collapsed into it");
    wil::com_ptr_nothrow<IUnknown> freshIdentity;
    RequireSucceeded(fresh.query_to(freshIdentity.put()), "the fresh root has a COM identity");
    Require(freshIdentity.get() != earlyIdentity.get(), "the fresh root is a new element");
    wil::com_ptr_nothrow<IRawElementProviderSimple> freshSimple;
    RequireSucceeded(fresh.query_to(freshSimple.put()), "the fresh root is a simple provider");
    Require(ReadProviderStringProperty(*freshSimple.get(), UIA_NamePropertyId, "the fresh root's name") == L"Collapsed action",
            "the fresh root is the collapsed control's element");
    wil::com_ptr_nothrow<IUnknown> freshPattern;
    wil::com_ptr_nothrow<IInvokeProvider> freshInvoke;
    RequireSucceeded(freshSimple->GetPatternProvider(UIA_InvokePatternId, freshPattern.put()), "the fresh root exposes Invoke");
    Require(freshPattern != nullptr, "the fresh root has an Invoke pattern");
    RequireSucceeded(freshPattern.query_to(freshInvoke.put()), "the fresh root's Invoke interface");
    RequireSucceeded(freshInvoke->Invoke(), "the fresh root invokes the collapsed control");
    Require(invoked == 1u, "the collapsed control's action runs once");
}

void TestNativeAccessibilityCollapsedRootRetiresAfterCallbackReplacement()
{
    using namespace DxUi;
    AttachedHostWindow window;
    unsigned int originalCount    = 0u;
    unsigned int replacementCount = 0u;
    auto button                   = std::make_unique<Button>(L"Original semantic root");
    button->SetOnClick([&]
    {
        ++originalCount;
        auto replacement = std::make_unique<Button>(L"Replacement semantic root");
        replacement->SetOnClick([&] { ++replacementCount; });
        window.Host().SetRoot(std::move(replacement));
    });
    window.Host().SetRoot(std::move(button));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    provider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Require(provider != nullptr, "collapsed native root provider exists");
    RequireSucceeded(provider.query_to(simple.put()), "collapsed native root simple interface");
    wil::com_ptr_nothrow<IUnknown> pattern;
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    RequireSucceeded(simple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "collapsed native root Invoke lookup");
    Require(pattern != nullptr, "collapsed native root exposes Invoke");
    RequireSucceeded(pattern.query_to(invoke.put()), "collapsed native root Invoke interface");
    RequireSucceeded(invoke->Invoke(), "native callback may replace its semantic root");
    Require(invoke->Invoke() == UIA_E_ELEMENTNOTAVAILABLE && originalCount == 1u && replacementCount == 0u,
            "retained collapsed-root provider cannot invoke callback replacement");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fresh;
    fresh.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(fresh != nullptr, "fresh native semantic root acquisition remains available after replacement");
    wil::com_ptr_nothrow<IRawElementProviderSimple> freshSimple;
    RequireSucceeded(fresh.query_to(freshSimple.put()), "replacement semantic root simple interface");
    Require(ReadProviderStringProperty(*freshSimple.get(), UIA_NamePropertyId, "replacement semantic root name") == L"Replacement semantic root",
            "canonical root cache must replace the disconnected semantic-root provider");
    pattern.reset();
    wil::com_ptr_nothrow<IInvokeProvider> freshInvoke;
    RequireSucceeded(freshSimple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "replacement semantic root Invoke lookup");
    Require(pattern != nullptr, "replacement semantic root has Invoke");
    RequireSucceeded(pattern.query_to(freshInvoke.put()), "replacement semantic root Invoke interface");
    RequireSucceeded(freshInvoke->Invoke(), "fresh provider invokes replacement semantic root");
    Require(originalCount == 1u && replacementCount == 1u, "fresh semantic root callback runs exactly once");
}

void TestNativeAccessibilityPostedSelectRejectsReplacement()
{
    using namespace DxUi;
    AttachedHostWindow window;
    AccessibilityTextRangeSelectFixture fixture(window);
    wil::unique_event_nothrow posted(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(posted != nullptr, "replacement-before-dispatch test creates posted event");
    DebugSetAccessibilityUiActionPostedEventForTest(posted.get());
    DebugSetAccessibilityUiActionDispatchTimeoutForTest(2000u);
    const auto clearHooks = wil::scope_exit([]() noexcept
    {
        DebugSetAccessibilityUiActionPostedEventForTest(nullptr);
        DebugSetAccessibilityUiActionDispatchTimeoutForTest(0u);
    });
    std::atomic<HRESULT> result{E_PENDING};
    std::thread worker([&] { result.store(fixture.documentRange->Select(), std::memory_order_release); });
    // Always join before reporting failure; no failed Require leaves a live worker.
    const DWORD postedWait = WaitForSingleObject(posted.get(), 2000u);
    auto replacement       = std::make_unique<Panel>();
    auto* field            = replacement->AddChild<TextField>(L"replacement value");
    field->SetBounds(D2D1::RectF(0, 28, 260, 60));
    field->SetSelectionRange(1u, 3u);
    window.Host().SetRoot(std::move(replacement));
    window.Host().RefreshAccessibilitySnapshot();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (result.load(std::memory_order_acquire) == E_PENDING && std::chrono::steady_clock::now() < deadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    const HRESULT selectHr = result.load(std::memory_order_acquire);
    std::cout << "    [UIA lifetime] posted-select result=0x" << std::hex << static_cast<unsigned long>(selectHr) << std::dec << '\n';
    Require(postedWait == WAIT_OBJECT_0, "original native selection request is queued before replacement");
    Require(selectHr == UIA_E_ELEMENTNOTAVAILABLE, "queued native selection revalidates original control lifetime on UI-thread execution");
    Require(field->GetSelectionRange() == std::optional<std::pair<size_t, size_t>>{{1u, 3u}} && field->GetText() == L"replacement value",
            "queued stale selection leaves replacement text and selection unchanged");
}

// A UI Automation action that selects a grid row or a tree item and then focuses its control reports the element gone when
// the selection's delegate replaced every control, and neither focuses nor touches the destroyed control
// (AddressSanitizer catches an action that does).
void TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus()
{
    using namespace DxUi;
    struct ReplacingGridDelegate final : IGridDelegate
    {
        using IGridDelegate::OnGridSelectionChanged;
        std::function<void()> onSelection;
        void OnGridSelectionChanged(Grid& /*sender*/) override
        {
            if (onSelection)
                onSelection();
        }
    };
    struct ReplacingTreeDelegate final : ITreeDelegate
    {
        std::function<void()> onSelection;
        void OnTreeSelectionChanged(uint64_t /*itemId*/) override
        {
            if (onSelection)
                onSelection();
        }
        void OnTreeSelectionSetChanged(std::span<const uint64_t> /*selectedItemIds*/) override
        {
            if (onSelection)
                onSelection();
        }
    };
    enum class Action
    {
        Select,
        AddToSelection,
        SetFocus
    };
    for (const bool tree : {false, true})
    {
        for (const Action action : {Action::Select, Action::AddToSelection, Action::SetFocus})
        {
            // A tree item's focus selects silently, as SetFocusedItemId does, so no delegate runs to replace anything.
            if (tree && action == Action::SetFocus)
                continue;
            const std::string name = std::format("{} {}",
                                                 tree ? "tree item" : "grid row",
                                                 action == Action::Select ? "Select" : (action == Action::AddToSelection ? "AddToSelection" : "SetFocus"));
            // The models and delegates outlive the window, whose controls may still hold them when a check fails.
            MultiRowGridModel gridModel(4u);
            MutableTreeModel treeModel;
            treeModel.SetVisibleItems({TreeItemData{.id = 10u, .text = L"General"}, TreeItemData{.id = 20u, .text = L"Viewers"}});
            ReplacingGridDelegate gridDelegate;
            ReplacingTreeDelegate treeDelegate;
            AttachedHostWindow window;
            auto root   = std::make_unique<Panel>();
            auto* label = root->AddChild<Label>(L"Rows");
            label->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
            if (tree)
            {
                auto* view = root->AddChild<Tree>();
                view->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 160.0f));
                view->SetModel(&treeModel);
                view->SetDelegate(&treeDelegate);
                // Adding to a selection needs a tree that selects several items.
                view->SetMultiSelectEnabled(action == Action::AddToSelection);
            }
            else
            {
                auto* view = root->AddChild<Grid>();
                view->SetBounds(D2D1::RectF(0.0f, 28.0f, 360.0f, 200.0f));
                view->SetModel(&gridModel);
                view->SetDelegate(&gridDelegate);
            }
            window.Host().SetRoot(std::move(root));
            int replacements   = 0;
            const auto replace = [&]
            {
                if (replacements++ == 0)
                    window.Host().SetRoot(std::make_unique<Panel>());
            };
            gridDelegate.onSelection = replace;
            treeDelegate.onSelection = replace;

            wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
            rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
            Require(rootProvider != nullptr, std::format("{}: the window exposes its provider", name).c_str());
            auto labelProvider = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 12.0f, "selection replacement label provider");
            wil::com_ptr_nothrow<IRawElementProviderFragment> viewProvider;
            RequireSucceeded(labelProvider->Navigate(NavigateDirection_NextSibling, viewProvider.put()), "the label's sibling is the view");
            wil::com_ptr_nothrow<IRawElementProviderFragment> element;
            RequireSucceeded(viewProvider->Navigate(NavigateDirection_FirstChild, element.put()), "the view exposes its first child");
            if (! tree)
            {
                // The grid's first child is its column header; the first row follows it.
                wil::com_ptr_nothrow<IRawElementProviderFragment> row;
                RequireSucceeded(element->Navigate(NavigateDirection_NextSibling, row.put()), "the column header is followed by the first row");
                element = row;
            }
            Require(element != nullptr, std::format("{}: the element exists", name).c_str());

            HRESULT hr = E_FAIL;
            if (action == Action::SetFocus)
            {
                hr = element->SetFocus();
            }
            else
            {
                wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
                RequireSucceeded(element.query_to(simple.put()), "the element exposes its properties");
                wil::com_ptr_nothrow<IUnknown> patternUnknown;
                RequireSucceeded(simple->GetPatternProvider(UIA_SelectionItemPatternId, patternUnknown.put()), "the element is a selection item");
                wil::com_ptr_nothrow<ISelectionItemProvider> pattern;
                RequireSucceeded(patternUnknown.query_to(pattern.put()), "the element exposes ISelectionItemProvider");
                hr = action == Action::Select ? pattern->Select() : pattern->AddToSelection();
            }
            window.PumpMessages();
            Require(replacements == 1, std::format("{}: the selection's delegate replaced the controls", name).c_str());
            Require(hr == UIA_E_ELEMENTNOTAVAILABLE,
                    std::format("{}: the action reports its element gone (hr=0x{:08X})", name, static_cast<uint32_t>(hr)).c_str());
            Require(window.Host().GetFocusControl() == nullptr, std::format("{}: nothing of the replaced controls is focused", name).c_str());
        }
    }
}

void TestNativeAccessibilityPostedInvokeRejectsReplacement()
{
    using namespace DxUi;
    AttachedHostWindow window;
    unsigned int invoked            = 0u;
    unsigned int replacementInvoked = 0u;
    auto root                       = std::make_unique<Panel>();
    auto* button                    = root->AddChild<Button>(L"Original queued action");
    button->SetBounds(D2D1::RectF(0, 0, 200, 32));
    button->SetOnClick([&] { ++invoked; });
    window.Host().SetRoot(std::move(root));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> hostProvider;
    hostProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(hostProvider != nullptr, "queued native action host provider exists");
    auto fragment = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *hostProvider.get(), 20, 12, "original queued action provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(fragment.query_to(simple.put()), "queued action simple interface");
    wil::com_ptr_nothrow<IUnknown> pattern;
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    RequireSucceeded(simple->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "queued action Invoke lookup");
    Require(pattern != nullptr, "queued action exposes Invoke");
    RequireSucceeded(pattern.query_to(invoke.put()), "queued action Invoke interface");
    wil::unique_event_nothrow posted(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(posted != nullptr, "queued action posted event");
    DebugSetAccessibilityUiActionPostedEventForTest(posted.get());
    DebugSetAccessibilityUiActionDispatchTimeoutForTest(2000u);
    const auto clearHooks = wil::scope_exit([]() noexcept
    {
        DebugSetAccessibilityUiActionPostedEventForTest(nullptr);
        DebugSetAccessibilityUiActionDispatchTimeoutForTest(0u);
    });
    std::atomic<HRESULT> result{E_PENDING};
    std::thread worker([&] { result.store(invoke->Invoke(), std::memory_order_release); });
    const DWORD postedWait  = WaitForSingleObject(posted.get(), 2000u);
    auto replacement        = std::make_unique<Panel>();
    auto* replacementButton = replacement->AddChild<Button>(L"Replacement queued action");
    replacementButton->SetBounds(D2D1::RectF(0, 0, 200, 32));
    replacementButton->SetOnClick([&] { ++replacementInvoked; });
    window.Host().SetRoot(std::move(replacement));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (result.load(std::memory_order_acquire) == E_PENDING && std::chrono::steady_clock::now() < deadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    const HRESULT invokeHr = result.load(std::memory_order_acquire);
    std::cout << "    [UIA lifetime] posted-invoke result=0x" << std::hex << static_cast<unsigned long>(invokeHr) << std::dec << " original=" << invoked
              << " replacement=" << replacementInvoked << '\n';
    Require(postedWait == WAIT_OBJECT_0, "native Invoke is queued before replacement");
    Require(invokeHr == UIA_E_ELEMENTNOTAVAILABLE && invoked == 0u && replacementInvoked == 0u,
            "queued native Invoke cannot dispatch into the same-path replacement");
}

void TestAccessibilityTimedOutTextRangeSelectDoesNotExecuteLater()
{
    using namespace DxUi;

    AttachedHostWindow window;
    AccessibilityTextRangeSelectFixture fixture(window);

    wil::unique_event_nothrow posted;
    posted.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(posted != nullptr, "timed-out Select test creates the posted event");
    wil::unique_event_nothrow handlerEntered;
    handlerEntered.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(handlerEntered != nullptr, "timed-out Select test creates the handler-entered event");
    wil::unique_event_nothrow releaseHandler;
    releaseHandler.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(releaseHandler != nullptr, "timed-out Select test creates the release event");

    DebugResetAccessibilityUiActionExecutionCountForTest();
    DebugSetAccessibilityUiActionDispatchTimeoutForTest(100u);
    DebugSetAccessibilityUiActionPostedEventForTest(posted.get());
    DebugSetAccessibilityUiActionHandlerStallForTest(handlerEntered.get(), releaseHandler.get());
    const auto clearHooks = wil::scope_exit([]() noexcept
    {
        DebugSetAccessibilityUiActionHandlerStallForTest(nullptr, nullptr);
        DebugSetAccessibilityUiActionPostedEventForTest(nullptr);
        DebugSetAccessibilityUiActionDispatchTimeoutForTest(0u);
    });

    constexpr HRESULT kPending = E_PENDING;
    std::atomic<HRESULT> result{kPending};
    std::atomic<bool> enteredObserved{false};
    std::thread worker([&] { result.store(fixture.documentRange->Select(), std::memory_order_release); });

    Require(WaitForSingleObject(posted.get(), 2000u) == WAIT_OBJECT_0, "timed-out Select request is posted");
    std::thread releaser([&]
    {
        enteredObserved.store(WaitForSingleObject(handlerEntered.get(), 2000u) == WAIT_OBJECT_0, std::memory_order_release);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (result.load(std::memory_order_acquire) == kPending && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        static_cast<void>(SetEvent(releaseHandler.get()));
    });

    window.PumpMessages();
    worker.join();
    releaser.join();

    Require(enteredObserved.load(std::memory_order_acquire), "timed-out Select handler reaches the pre-take stall");
    Require(result.load(std::memory_order_acquire) == HRESULT_FROM_WIN32(ERROR_TIMEOUT), "timed-out Select reports ERROR_TIMEOUT");
    Require(DebugGetAccessibilityUiActionExecutionCountForTest() == 0u, "abandoned Select is not executed after its caller times out");
    const auto selection = fixture.field->GetSelectionRange();
    Require(selection == std::optional<std::pair<size_t, size_t>>{{0u, 5u}}, "abandoned Select leaves the retained selection unchanged");
}

void TestAccessibilityTakenTextRangeSelectExecutesOnlyOnce()
{
    using namespace DxUi;

    AttachedHostWindow window;
    AccessibilityTextRangeSelectFixture fixture(window);

    wil::unique_event_nothrow posted;
    posted.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(posted != nullptr, "taken Select test creates the posted event");
    wil::unique_event_nothrow handlerTaken;
    handlerTaken.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(handlerTaken != nullptr, "taken Select test creates the handler-taken event");
    wil::unique_event_nothrow releaseHandler;
    releaseHandler.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(releaseHandler != nullptr, "taken Select test creates the release event");

    DebugResetAccessibilityUiActionExecutionCountForTest();
    DebugSetAccessibilityUiActionDispatchTimeoutForTest(100u);
    DebugSetAccessibilityUiActionPostedEventForTest(posted.get());
    DebugSetAccessibilityUiActionHandlerTakenStallForTest(handlerTaken.get(), releaseHandler.get());
    const auto clearHooks = wil::scope_exit([]() noexcept
    {
        DebugSetAccessibilityUiActionHandlerTakenStallForTest(nullptr, nullptr);
        DebugSetAccessibilityUiActionPostedEventForTest(nullptr);
        DebugSetAccessibilityUiActionDispatchTimeoutForTest(0u);
    });

    constexpr HRESULT kPending = E_PENDING;
    std::atomic<HRESULT> result{kPending};
    std::atomic<bool> takenObserved{false};
    std::thread worker([&] { result.store(fixture.documentRange->Select(), std::memory_order_release); });

    Require(WaitForSingleObject(posted.get(), 2000u) == WAIT_OBJECT_0, "taken Select request is posted");
    std::thread releaser([&]
    {
        takenObserved.store(WaitForSingleObject(handlerTaken.get(), 2000u) == WAIT_OBJECT_0, std::memory_order_release);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (result.load(std::memory_order_acquire) == kPending && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        static_cast<void>(SetEvent(releaseHandler.get()));
    });

    window.PumpMessages();
    worker.join();
    releaser.join();

    Require(takenObserved.load(std::memory_order_acquire), "taken Select handler owns the dispatch before timeout");
    Require(result.load(std::memory_order_acquire) == HRESULT_FROM_WIN32(ERROR_TIMEOUT), "taken-but-incomplete Select reports ERROR_TIMEOUT");
    Require(DebugGetAccessibilityUiActionExecutionCountForTest() == 1u, "taken Select executes exactly once after the timeout race");
    const auto selection = fixture.field->GetSelectionRange();
    Require(selection == std::optional<std::pair<size_t, size_t>>{{6u, 10u}}, "taken Select applies its range exactly once");
}

void TestAccessibilityDestroyWithPendingDispatchReturnsCancelled()
{
    using namespace DxUi;

    auto window = std::make_unique<AttachedHostWindow>();
    AccessibilityTextRangeSelectFixture fixture(*window);

    wil::unique_event_nothrow posted;
    posted.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(posted != nullptr, "destroy-pending Select test creates the posted event");
    DebugSetAccessibilityUiActionPostedEventForTest(posted.get());
    const auto clearHook = wil::scope_exit([]() noexcept { DebugSetAccessibilityUiActionPostedEventForTest(nullptr); });

    constexpr HRESULT kPending = E_PENDING;
    std::atomic<HRESULT> result{kPending};
    std::thread worker([&] { result.store(fixture.documentRange->Select(), std::memory_order_release); });

    Require(WaitForSingleObject(posted.get(), 2000u) == WAIT_OBJECT_0, "destroy-pending Select request is posted");
    window.reset();
    worker.join();

    Require(result.load(std::memory_order_acquire) == HRESULT_FROM_WIN32(ERROR_CANCELLED),
            "destroying a window drains its pending Select request with ERROR_CANCELLED");
}

void TestAccessibilityTextRangeBoundingRectanglesDispatchesToWindowThread()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "cross-thread text range bounds test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "cross-thread bounds text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "cross-thread bounds text field provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "cross-thread bounds TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "cross-thread bounds text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "cross-thread bounds TextPattern supports ITextProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "cross-thread bounds TextPattern exposes a document range");

    int moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                     "cross-thread bounds range start endpoint moves to beta");
    Require(moved == 6, "cross-thread bounds range start endpoint reports moved characters");

    constexpr HRESULT kPendingBounds = E_PENDING;
    std::atomic<bool> workerStarted{false};
    std::atomic<HRESULT> boundsResult{kPendingBounds};
    SAFEARRAY* workerRectangles = nullptr;
    std::thread worker([&]
    {
        workerStarted.store(true, std::memory_order_release);
        boundsResult.store(documentRange->GetBoundingRectangles(&workerRectangles), std::memory_order_release);
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "cross-thread TextRange bounds worker starts");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (boundsResult.load(std::memory_order_acquire) != kPendingBounds)
    {
        worker.join();
        const auto destroyEarlyRectangles = wil::scope_exit([&]
        {
            if (workerRectangles)
            {
                SafeArrayDestroy(workerRectangles);
            }
        });
        Require(false, "cross-thread TextRange GetBoundingRectangles waits for host window-thread dispatch");
    }

    const auto boundsDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (boundsResult.load(std::memory_order_acquire) == kPendingBounds && std::chrono::steady_clock::now() < boundsDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const HRESULT result = boundsResult.load(std::memory_order_acquire);
    worker.join();
    const auto destroyRectangles = wil::scope_exit([&]
    {
        if (workerRectangles)
        {
            SafeArrayDestroy(workerRectangles);
        }
    });
    Require(result != kPendingBounds, "cross-thread TextRange GetBoundingRectangles completes after host window-thread dispatch");
    RequireSucceeded(result, "cross-thread TextRange GetBoundingRectangles succeeds after dispatch");

    const std::vector<double> rectangleValues = ReadDoubleArray(workerRectangles, "cross-thread moved range returns bounding rectangle values");
    Require(rectangleValues.size() == 4u, "cross-thread moved range returns one bounding rectangle");
    Require(rectangleValues[2] > 0.0 && rectangleValues[3] > 0.0, "cross-thread moved range rectangle is non-empty");
}

void TestAccessibilityTextRangeBoundingRectanglesTimeoutKeepsLateHandlerStorageAlive()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 60.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "late-handler text range bounds test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 44.0f, "late-handler bounds text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "late-handler bounds text field provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextPatternId, textPatternUnknown.put()), "late-handler bounds TextPattern lookup succeeds");
    Require(textPatternUnknown != nullptr, "late-handler bounds text field exposes TextPattern");
    wil::com_ptr_nothrow<ITextProvider> textPattern;
    RequireSucceeded(textPatternUnknown.query_to(textPattern.put()), "late-handler bounds TextPattern supports ITextProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> documentRange;
    RequireSucceeded(textPattern->get_DocumentRange(documentRange.put()), "late-handler bounds TextPattern exposes a document range");

    int moved = 0;
    RequireSucceeded(documentRange->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 6, &moved),
                     "late-handler bounds range start endpoint moves to beta");
    Require(moved == 6, "late-handler bounds range start endpoint reports moved characters");

    wil::unique_event_nothrow handlerEntered;
    handlerEntered.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(handlerEntered != nullptr, "late-handler bounds test creates handler-entered event");
    wil::unique_event_nothrow releaseHandler;
    releaseHandler.reset(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(releaseHandler != nullptr, "late-handler bounds test creates release event");

    DebugSetAccessibilityUiActionHandlerStallForTest(handlerEntered.get(), releaseHandler.get());
    DebugSetAccessibilityUiActionDispatchTimeoutForTest(100u);
    const auto clearStallHook = wil::scope_exit([]() noexcept
    {
        DebugSetAccessibilityUiActionHandlerStallForTest(nullptr, nullptr);
        DebugSetAccessibilityUiActionDispatchTimeoutForTest(0u);
    });

    constexpr HRESULT kPendingBounds   = E_PENDING;
    constexpr HRESULT kExpectedTimeout = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    std::atomic<bool> workerStarted{false};
    std::atomic<bool> handlerEnteredObserved{false};
    std::atomic<bool> timeoutObservedBeforeRelease{false};
    std::atomic<HRESULT> boundsResult{kPendingBounds};
    SAFEARRAY* workerRectangles = nullptr;

    std::thread worker([&]
    {
        workerStarted.store(true, std::memory_order_release);
        boundsResult.store(documentRange->GetBoundingRectangles(&workerRectangles), std::memory_order_release);
    });

    std::thread releaser([&]
    {
        const DWORD entered = ::WaitForSingleObject(handlerEntered.get(), 2000u);
        handlerEnteredObserved.store(entered == WAIT_OBJECT_0, std::memory_order_release);
        if (entered == WAIT_OBJECT_0)
        {
            const auto timeoutDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
            while (boundsResult.load(std::memory_order_acquire) == kPendingBounds && std::chrono::steady_clock::now() < timeoutDeadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            timeoutObservedBeforeRelease.store(boundsResult.load(std::memory_order_acquire) == kExpectedTimeout, std::memory_order_release);
        }
        static_cast<void>(::SetEvent(releaseHandler.get()));
    });

    const auto startDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (! workerStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < startDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(workerStarted.load(std::memory_order_acquire), "late-handler TextRange bounds worker starts");

    const auto completionDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(9);
    while (boundsResult.load(std::memory_order_acquire) == kPendingBounds && std::chrono::steady_clock::now() < completionDeadline)
    {
        window.PumpMessages();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (worker.joinable())
    {
        worker.join();
    }
    if (releaser.joinable())
    {
        releaser.join();
    }
    const auto destroyRectangles = wil::scope_exit([&]
    {
        if (workerRectangles)
        {
            SafeArrayDestroy(workerRectangles);
        }
    });

    Require(handlerEnteredObserved.load(std::memory_order_acquire), "late-handler UIA action is dequeued before the sender times out");
    Require(timeoutObservedBeforeRelease.load(std::memory_order_acquire), "late-handler UIA action sender times out before handler completion");
    Require(boundsResult.load(std::memory_order_acquire) == kExpectedTimeout,
            "late-handler TextRange GetBoundingRectangles reports timeout instead of reading late output");
    Require(workerRectangles == nullptr, "late-handler timeout leaves caller SAFEARRAY output untouched");
}

void TestAccessibilityProviderExposesNativeImeTextEditRanges()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 32.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field);
    field->SetSelectionRange(5u, 5u);
    window.Host().SyncTextInput(field);

    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_STARTCOMPOSITION, 0, 0));

    NativeTextInputImePayload payload;
    payload.hasCompositionString  = true;
    payload.compositionString     = L"-ime";
    payload.compositionAttributes = {ATTR_INPUT, ATTR_TARGET_CONVERTED, ATTR_TARGET_CONVERTED, ATTR_INPUT};
    payload.hasCursorPosition     = true;
    payload.cursorPosition        = 3u;
    window.Host().DebugSetNativeTextInputImePayloadForTest(payload);
    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_COMPOSITION, 0, GCS_COMPSTR | GCS_COMPATTR | GCS_CURSORPOS));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "native ime TextEditPattern test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> fieldProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 16.0f, "native ime text field provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> fieldSimple;
    RequireSucceeded(fieldProvider.query_to(fieldSimple.put()), "native ime text field provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> textEditPatternUnknown;
    RequireSucceeded(fieldSimple->GetPatternProvider(UIA_TextEditPatternId, textEditPatternUnknown.put()), "native ime TextEditPattern lookup succeeds");
    Require(textEditPatternUnknown != nullptr, "native ime text field exposes TextEditPattern");
    wil::com_ptr_nothrow<ITextEditProvider> textEditPattern;
    RequireSucceeded(textEditPatternUnknown.query_to(textEditPattern.put()), "native ime TextEditPattern supports ITextEditProvider");

    wil::com_ptr_nothrow<ITextRangeProvider> activeComposition;
    RequireSucceeded(textEditPattern->GetActiveComposition(activeComposition.put()), "native ime active-composition range lookup succeeds");
    Require(activeComposition != nullptr, "native ime TextEditPattern exposes active composition range");
    Require(ReadTextRangeText(*activeComposition.get(), -1, "native ime active-composition range exposes text") == L"-ime",
            "native ime active-composition range returns the preview string");

    wil::com_ptr_nothrow<ITextRangeProvider> conversionTarget;
    RequireSucceeded(textEditPattern->GetConversionTarget(conversionTarget.put()), "native ime conversion-target range lookup succeeds");
    Require(conversionTarget != nullptr, "native ime TextEditPattern exposes conversion target range");
    Require(ReadTextRangeText(*conversionTarget.get(), -1, "native ime conversion-target range exposes text") == L"im",
            "native ime conversion-target range returns the target-converted span");

    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_ENDCOMPOSITION, 0, 0));
}

void TestAccessibilityNativeTextInputRaisesTextAndTextEditEventCounters()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);

    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha beta");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 32.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field);
    window.Host().SyncTextInput(field);

    const NativeTextInputEventCounters baselineCounters = window.Host().DebugGetNativeTextInputEventCounters();

    field->SetTextAndNotify(L"alpha beta edited");
    window.Host().SyncTextInput(field);

    NativeTextInputEventCounters counters = window.Host().DebugGetNativeTextInputEventCounters();
    Require(counters.uiaTextChangedCount == baselineCounters.uiaTextChangedCount + 1u,
            "native text input raises a UIA TextPattern text-changed event for retained text mutations");

    const NativeTextInputEventCounters afterTextCounters = counters;
    field->SetSelectionRange(6u, 10u);
    window.Host().SyncTextInput(field);
    counters = window.Host().DebugGetNativeTextInputEventCounters();
    Require(counters.uiaTextSelectionChangedCount == afterTextCounters.uiaTextSelectionChangedCount + 1u,
            "native text input raises a UIA TextPattern selection-changed event for retained selection mutations");

    const NativeTextInputEventCounters afterSelectionCounters = counters;
    field->SetSelectionRange(3u, 3u);
    window.Host().SyncTextInput(field);
    counters = window.Host().DebugGetNativeTextInputEventCounters();
    Require(counters.uiaActiveTextPositionChangedCount == afterSelectionCounters.uiaActiveTextPositionChangedCount + 1u,
            "native text input raises a UIA active text position event for retained caret moves");

    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_STARTCOMPOSITION, 0, 0));

    NativeTextInputImePayload payload;
    payload.hasCompositionString  = true;
    payload.compositionString     = L"-ime";
    payload.compositionAttributes = {ATTR_INPUT, ATTR_TARGET_CONVERTED, ATTR_TARGET_CONVERTED, ATTR_INPUT};
    window.Host().DebugSetNativeTextInputImePayloadForTest(payload);
    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_COMPOSITION, 0, GCS_COMPSTR | GCS_COMPATTR));

    counters = window.Host().DebugGetNativeTextInputEventCounters();
    Require(counters.uiaTextEditTextChangedCount >= baselineCounters.uiaTextEditTextChangedCount + 1u,
            "native IME composition raises a UIA TextEdit text-changed event");
    Require(counters.uiaTextEditConversionTargetChangedCount == baselineCounters.uiaTextEditConversionTargetChangedCount + 1u,
            "native IME target conversion raises a UIA TextEdit conversion-target-changed event");

    static_cast<void>(SendMessageW(window.Hwnd(), WM_IME_ENDCOMPOSITION, 0, 0));
}

void TestAccessibilityGridSnapshotRebuildMeetsTenThousandRowSelectionBudget()
{
    using namespace DxUi;

    constexpr size_t kRowCount = 10'000u;
    MultiRowGridModel gridModel(kRowCount);
    AttachedHostWindow window;
    auto grid = std::make_unique<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetModel(&gridModel);
    Grid* const liveGrid = grid.get();
    window.Host().SetRoot(std::move(grid));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "10k-row grid snapshot test creates an accessibility provider");

    std::vector<uint64_t> selectedRowIds(kRowCount);
    std::iota(selectedRowIds.begin(), selectedRowIds.end(), uint64_t{0u});
    liveGrid->GetSelectionModel().SetRange(selectedRowIds, selectedRowIds.front(), selectedRowIds.back());

    DebugSetAccessibilityOffscreenSelectedRowMaterializationLimitForTest(kRowCount);
    const auto resetMaterializationLimit = wil::scope_exit([]() noexcept { DebugSetAccessibilityOffscreenSelectedRowMaterializationLimitForTest(0u); });
    const auto baselineStarted           = std::chrono::steady_clock::now();
    liveGrid->RefreshAccessibilitySnapshot();
    const auto baselineElapsed = std::chrono::steady_clock::now() - baselineStarted;

    DebugSetAccessibilityOffscreenSelectedRowMaterializationLimitForTest(256u);
    const auto candidateStarted = std::chrono::steady_clock::now();
    liveGrid->RefreshAccessibilitySnapshot();
    const auto candidateElapsed = std::chrono::steady_clock::now() - candidateStarted;

    Require(liveGrid->GetSelectionModel().GetCount() == kRowCount, "10k-row grid snapshot retains the complete Ctrl+A selection");
    Require(candidateElapsed < std::chrono::milliseconds(250), "10k-row grid Ctrl+A accessibility snapshot rebuild stays under the 250 ms Debug budget");
    Require(candidateElapsed < baselineElapsed, "capped offscreen row materialization improves the 10k-row Ctrl+A snapshot rebuild");
}

void TestAccessibilityProviderExposesTreeAndGridMetadata()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();

    auto* treeLabel = root->AddChild<Label>(L"Categories");
    treeLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 88.0f));
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        DxUi::TreeItemData{.id = 1u, .text = L"General"},
        DxUi::TreeItemData{.id = 2u, .text = L"Panes"},
        DxUi::TreeItemData{.id = 3u, .text = L"Viewers"},
    });
    tree->SetModel(&treeModel);
    tree->SetSelectedItemId(2u);
    treeLabel->SetMnemonicTarget(tree);

    auto* gridLabel = root->AddChild<Label>(L"Results");
    gridLabel->SetBounds(D2D1::RectF(0.0f, 92.0f, 120.0f, 116.0f));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 120.0f, 240.0f, 188.0f));
    MultiRowGridModel gridModel(6u);
    grid->SetModel(&gridModel);
    gridLabel->SetMnemonicTarget(grid);

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "tree/grid accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> treeLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 12.0f, "tree label accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    RequireSucceeded(treeLabelProvider->Navigate(NavigateDirection_NextSibling, treeProvider.put()), "tree label accessibility provider navigates to the tree");
    Require(treeProvider != nullptr, "tree label accessibility provider returns the tree as the next sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> treeSimple;
    RequireSucceeded(treeProvider.query_to(treeSimple.put()), "tree accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*treeSimple.get(), UIA_ControlTypePropertyId, "tree exposes UIA control type") == UIA_TreeControlTypeId,
            "tree accessibility provider reports tree control type");
    Require(ReadProviderStringProperty(*treeSimple.get(), UIA_NamePropertyId, "tree exposes accessibility name") == L"Categories",
            "tree accessibility provider uses its associated label as the accessible name");

    wil::com_ptr_nothrow<IRawElementProviderFragment> treeItemProvider;
    RequireSucceeded(treeProvider->Navigate(NavigateDirection_FirstChild, treeItemProvider.put()),
                     "tree accessibility provider navigates to the first visible tree item");
    Require(treeItemProvider != nullptr, "tree accessibility provider returns a first tree-item child");
    wil::com_ptr_nothrow<IRawElementProviderSimple> treeItemSimple;
    RequireSucceeded(treeItemProvider.query_to(treeItemSimple.put()), "tree item accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*treeItemSimple.get(), UIA_ControlTypePropertyId, "tree item exposes UIA control type") == UIA_TreeItemControlTypeId,
            "tree item accessibility provider reports tree-item control type");
    Require(ReadProviderStringProperty(*treeItemSimple.get(), UIA_NamePropertyId, "tree item exposes accessibility name") == L"General",
            "tree item accessibility provider exposes the visible item text as its accessible name");
    Require(ReadProviderLongProperty(*treeItemSimple.get(), UIA_LevelPropertyId, "tree item exposes depth level") == 1,
            "tree item accessibility provider exposes a 1-based tree level");
    Require(! ReadProviderBoolProperty(*treeItemSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "first tree item exposes selected state"),
            "tree item accessibility provider reports the unselected first item");

    wil::com_ptr_nothrow<IRawElementProviderFragment> selectedTreeItemProvider;
    RequireSucceeded(treeItemProvider->Navigate(NavigateDirection_NextSibling, selectedTreeItemProvider.put()),
                     "tree item accessibility provider navigates to the next visible tree item");
    Require(selectedTreeItemProvider != nullptr, "tree item accessibility provider returns the next sibling item");
    wil::com_ptr_nothrow<IRawElementProviderSimple> selectedTreeItemSimple;
    RequireSucceeded(selectedTreeItemProvider.query_to(selectedTreeItemSimple.put()),
                     "selected tree item accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*selectedTreeItemSimple.get(), UIA_NamePropertyId, "selected tree item exposes accessibility name") == L"Panes",
            "tree item accessibility provider exposes the selected visible item text");
    Require(ReadProviderBoolProperty(*selectedTreeItemSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "selected tree item exposes selected state"),
            "tree item accessibility provider reports the selected item");

    wil::com_ptr_nothrow<IRawElementProviderFragment> hitTreeItemProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 70.0f, "tree item accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> hitTreeItemSimple;
    RequireSucceeded(hitTreeItemProvider.query_to(hitTreeItemSimple.put()), "tree point-hit provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*hitTreeItemSimple.get(), UIA_ControlTypePropertyId, "tree point-hit provider exposes item control type") ==
                UIA_TreeItemControlTypeId,
            "tree hit-testing resolves the visible tree item provider instead of only the tree container");
    Require(ReadProviderStringProperty(*hitTreeItemSimple.get(), UIA_NamePropertyId, "tree point-hit provider exposes item name") == L"Panes",
            "tree hit-testing resolves the expected visible tree item provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> gridLabelProvider;
    RequireSucceeded(treeProvider->Navigate(NavigateDirection_NextSibling, gridLabelProvider.put()), "tree accessibility provider navigates to the grid label");
    Require(gridLabelProvider != nullptr, "tree accessibility provider returns the grid label as the next sibling");

    wil::com_ptr_nothrow<IRawElementProviderFragment> gridProvider;
    RequireSucceeded(gridLabelProvider->Navigate(NavigateDirection_NextSibling, gridProvider.put()), "grid label accessibility provider navigates to the grid");
    Require(gridProvider != nullptr, "grid label accessibility provider returns the grid as the next sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> gridSimple;
    RequireSucceeded(gridProvider.query_to(gridSimple.put()), "grid accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*gridSimple.get(), UIA_ControlTypePropertyId, "grid exposes UIA control type") == UIA_DataGridControlTypeId,
            "grid accessibility provider reports data-grid control type");
    Require(ReadProviderStringProperty(*gridSimple.get(), UIA_NamePropertyId, "grid exposes accessibility name") == L"Results",
            "grid accessibility provider uses its associated label as the accessible name");
    Require(ReadProviderLongProperty(*gridSimple.get(), UIA_GridRowCountPropertyId, "grid exposes row count") == 6,
            "grid accessibility provider reports model row count");
    Require(ReadProviderLongProperty(*gridSimple.get(), UIA_GridColumnCountPropertyId, "grid exposes column count") == 1,
            "grid accessibility provider reports model column count");

    window.Host().SetFocusControl(tree);
    wil::com_ptr_nothrow<IRawElementProviderFragment> focusedProvider;
    RequireSucceeded(rootProvider->GetFocus(focusedProvider.put()), "root provider focus lookup succeeds for the tree");
    Require(focusedProvider != nullptr, "root provider returns the focused tree-item provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> focusedSimple;
    RequireSucceeded(focusedProvider.query_to(focusedSimple.put()), "focused tree provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*focusedSimple.get(), UIA_ControlTypePropertyId, "focused tree exposes UIA control type") == UIA_TreeItemControlTypeId,
            "root provider focus lookup returns the selected tree item provider for a focused tree");
    Require(ReadProviderStringProperty(*focusedSimple.get(), UIA_NamePropertyId, "focused tree item exposes accessibility name") == L"Panes",
            "root provider focus lookup returns the selected visible tree item");
}

void TestAccessibilityTreeItemProviderKeepsStableIdentityAcrossReorder()
{
    using namespace DxUi;

    constexpr uint64_t kAlphaId = 0x1'0000'0011ull;
    constexpr uint64_t kBetaId  = 0x2'0000'0022ull;
    constexpr uint64_t kGammaId = 0x3'0000'0033ull;

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* treeLabel = root->AddChild<Label>(L"Categories");
    treeLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 120.0f));

    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = kAlphaId, .text = L"Alpha"},
        TreeItemData{.id = kBetaId, .text = L"Beta"},
        TreeItemData{.id = kGammaId, .text = L"Gamma"},
    });
    tree->SetModel(&treeModel);
    treeLabel->SetMnemonicTarget(tree);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "stable tree-item identity test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> treeLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 12.0f, "stable tree label provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    RequireSucceeded(treeLabelProvider->Navigate(NavigateDirection_NextSibling, treeProvider.put()), "stable tree label provider navigates to the tree");
    wil::com_ptr_nothrow<IRawElementProviderFragment> alphaProvider;
    RequireSucceeded(treeProvider->Navigate(NavigateDirection_FirstChild, alphaProvider.put()), "stable tree provider navigates to the first item");
    wil::com_ptr_nothrow<IRawElementProviderFragment> retainedBetaProvider;
    RequireSucceeded(alphaProvider->Navigate(NavigateDirection_NextSibling, retainedBetaProvider.put()), "stable tree first item navigates to Beta");
    Require(retainedBetaProvider != nullptr, "stable tree test retains the Beta provider");

    wil::com_ptr_nothrow<IRawElementProviderSimple> retainedBetaSimple;
    RequireSucceeded(retainedBetaProvider.query_to(retainedBetaSimple.put()), "retained Beta provider exposes the simple-provider interface");
    wil::com_ptr_nothrow<IUnknown> selectionUnknown;
    RequireSucceeded(retainedBetaSimple->GetPatternProvider(UIA_SelectionItemPatternId, selectionUnknown.put()),
                     "retained Beta provider exposes SelectionItemPattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> selectionPattern;
    RequireSucceeded(selectionUnknown.query_to(selectionPattern.put()), "retained Beta selection pattern exposes ISelectionItemProvider");

    const auto readRuntimeId = [](IRawElementProviderFragment& provider, const char* context)
    {
        SAFEARRAY* runtimeId = nullptr;
        RequireSucceeded(provider.GetRuntimeId(&runtimeId), context);
        const auto destroyRuntimeId = wil::scope_exit([&] { SafeArrayDestroy(runtimeId); });
        Require(runtimeId != nullptr, "tree item runtime ID is present");
        LONG lowerBound = 0;
        LONG upperBound = -1;
        RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lowerBound), "tree item runtime ID lower bound is readable");
        RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upperBound), "tree item runtime ID upper bound is readable");
        std::vector<LONG> values;
        values.reserve(static_cast<size_t>(upperBound - lowerBound + 1));
        for (LONG index = lowerBound; index <= upperBound; ++index)
        {
            LONG value = 0;
            RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &value), "tree item runtime ID value is readable");
            values.push_back(value);
        }
        return values;
    };

    const std::vector<LONG> runtimeIdBefore = readRuntimeId(*retainedBetaProvider.get(), "retained Beta runtime ID lookup succeeds before reorder");

    treeModel.SetVisibleItems({
        TreeItemData{.id = kBetaId, .text = L"Beta"},
        TreeItemData{.id = kAlphaId, .text = L"Alpha"},
        TreeItemData{.id = kGammaId, .text = L"Gamma"},
    });
    tree->NotifyDataChanged();

    Require(ReadProviderStringProperty(*retainedBetaSimple.get(), UIA_NamePropertyId, "retained Beta name lookup succeeds after reorder") == L"Beta",
            "retained tree-item provider follows its stable item ID after its visible index changes");
    Require(readRuntimeId(*retainedBetaProvider.get(), "retained Beta runtime ID lookup succeeds after reorder") == runtimeIdBefore,
            "tree-item runtime ID remains unchanged when the item moves to another visible index");

    wil::com_ptr_nothrow<IRawElementProviderFragment> nextAfterBeta;
    RequireSucceeded(retainedBetaProvider->Navigate(NavigateDirection_NextSibling, nextAfterBeta.put()),
                     "retained Beta provider navigates using its current visible position");
    wil::com_ptr_nothrow<IRawElementProviderSimple> nextAfterBetaSimple;
    RequireSucceeded(nextAfterBeta.query_to(nextAfterBetaSimple.put()), "retained Beta next sibling exposes a simple provider");
    Require(ReadProviderStringProperty(*nextAfterBetaSimple.get(), UIA_NamePropertyId, "retained Beta next sibling name lookup succeeds") == L"Alpha",
            "retained tree-item navigation re-resolves the stable item before choosing its sibling");

    RequireSucceeded(selectionPattern->Select(), "retained Beta selection succeeds after reorder");
    Require(tree->GetSelectedItemId() && tree->GetSelectedItemId().value() == kBetaId,
            "retained tree-item action selects the stable item rather than the row that occupied its former index");

    treeModel.SetVisibleItems({
        TreeItemData{.id = kAlphaId, .text = L"Alpha"},
        TreeItemData{.id = kGammaId, .text = L"Gamma"},
    });
    tree->NotifyDataChanged();

    VARIANT staleName{};
    VariantInit(&staleName);
    const HRESULT stalePropertyResult = retainedBetaSimple->GetPropertyValue(UIA_NamePropertyId, &staleName);
    VariantClear(&staleName);
    Require(stalePropertyResult == UIA_E_ELEMENTNOTAVAILABLE, "removed retained tree-item provider reports element-not-available for properties");
    Require(selectionPattern->Select() == UIA_E_ELEMENTNOTAVAILABLE,
            "removed retained tree-item provider reports element-not-available instead of selecting a replacement row");
}

void TestAccessibilityProviderExposesTreeItemSelectionAndExpandCollapsePatterns()
{
    using namespace DxUi;

    class ExpandableTreeModel final : public ITreeModel
    {
    public:
        void SetExpanded(bool expanded)
        {
            _expanded = expanded;
        }

        [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
        {
            return _expanded ? 3u : 2u;
        }

        void GetVisibleItem(size_t visibleIndex, TreeItemData& outItem) const override
        {
            switch (visibleIndex)
            {
                case 0u: outItem = TreeItemData{.id = 10u, .text = L"Plugins", .hasChildren = true, .expanded = _expanded}; return;
                case 1u:
                    if (_expanded)
                    {
                        outItem = TreeItemData{.id = 11u, .parentId = 10u, .text = L"FTP", .depth = 1u};
                    }
                    else
                    {
                        outItem = TreeItemData{.id = 12u, .text = L"Search"};
                    }
                    return;
                case 2u: outItem = TreeItemData{.id = 12u, .text = L"Search"}; return;
                default: throw std::out_of_range("invalid visible tree item");
            }
        }

    private:
        bool _expanded = false;
    };

    class ExpandableTreeDelegate final : public ITreeDelegate
    {
    public:
        ExpandableTreeDelegate(ExpandableTreeModel& model, Tree& tree) : _model(model), _tree(tree)
        {
        }

        ExpandableTreeDelegate(const ExpandableTreeDelegate&)            = delete;
        ExpandableTreeDelegate& operator=(const ExpandableTreeDelegate&) = delete;
        ExpandableTreeDelegate(ExpandableTreeDelegate&&)                 = delete;
        ExpandableTreeDelegate& operator=(ExpandableTreeDelegate&&)      = delete;

        void OnTreeSelectionChanged(uint64_t itemId) override
        {
            ++selectionChangedCount;
            lastSelectedItemId = itemId;
        }

        void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
        {
            ++toggleCount;
            lastToggledItemId = itemId;
            lastExpandedState = expanded;
            _model.SetExpanded(expanded);
            _tree.NotifyDataChanged();
        }

        size_t selectionChangedCount = 0u;
        std::optional<uint64_t> lastSelectedItemId;
        size_t toggleCount = 0u;
        std::optional<uint64_t> lastToggledItemId;
        std::optional<bool> lastExpandedState;

    private:
        ExpandableTreeModel& _model;
        Tree& _tree;
    };

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* treeLabel = root->AddChild<Label>(L"Categories");
    treeLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 120.0f));

    ExpandableTreeModel treeModel;
    ExpandableTreeDelegate delegate(treeModel, *tree);
    tree->SetModel(&treeModel);
    tree->SetDelegate(&delegate);
    tree->SetSelectedItemId(10u);
    treeLabel->SetMnemonicTarget(tree);

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "tree-item pattern accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> treeLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 12.0f, "tree label accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    RequireSucceeded(treeLabelProvider->Navigate(NavigateDirection_NextSibling, treeProvider.put()), "tree label accessibility provider navigates to the tree");
    Require(treeProvider != nullptr, "tree label accessibility provider returns the tree as the next sibling");

    wil::com_ptr_nothrow<IRawElementProviderFragment> parentItemProvider;
    RequireSucceeded(treeProvider->Navigate(NavigateDirection_FirstChild, parentItemProvider.put()),
                     "tree accessibility provider navigates to the expandable parent item");
    Require(parentItemProvider != nullptr, "tree accessibility provider returns the parent tree item");
    wil::com_ptr_nothrow<IRawElementProviderSimple> parentItemSimple;
    RequireSucceeded(parentItemProvider.query_to(parentItemSimple.put()), "parent tree-item provider exposes IRawElementProviderSimple");

    wil::com_ptr_nothrow<IUnknown> selectionPatternUnknown;
    RequireSucceeded(parentItemSimple->GetPatternProvider(UIA_SelectionItemPatternId, selectionPatternUnknown.put()),
                     "tree item selection pattern lookup succeeds");
    Require(selectionPatternUnknown != nullptr, "tree item accessibility provider exposes selection-item pattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> selectionPattern;
    RequireSucceeded(selectionPatternUnknown.query_to(selectionPattern.put()), "tree item selection pattern supports ISelectionItemProvider");

    BOOL isSelected = FALSE;
    RequireSucceeded(selectionPattern->get_IsSelected(&isSelected), "tree item selected-state query succeeds");
    Require(isSelected == TRUE, "parent tree item pattern reports the initial selection");

    wil::com_ptr_nothrow<IRawElementProviderSimple> selectionContainer;
    RequireSucceeded(selectionPattern->get_SelectionContainer(selectionContainer.put()), "tree item selection container lookup succeeds");
    Require(selectionContainer != nullptr, "tree item selection pattern exposes the tree container");
    Require(ReadProviderStringProperty(*selectionContainer.get(), UIA_NamePropertyId, "tree selection container exposes accessibility name") == L"Categories",
            "tree item selection container resolves to the labeled tree host");
    wil::com_ptr_nothrow<IUnknown> treeSelectionContainerUnknown;
    RequireSucceeded(selectionContainer->GetPatternProvider(UIA_SelectionPatternId, treeSelectionContainerUnknown.put()),
                     "tree selection container selection-pattern lookup succeeds");
    Require(treeSelectionContainerUnknown != nullptr, "tree selection container exposes the selection pattern");
    wil::com_ptr_nothrow<ISelectionProvider> treeSelectionProvider;
    RequireSucceeded(treeSelectionContainerUnknown.query_to(treeSelectionProvider.put()), "tree selection container pattern supports ISelectionProvider");
    BOOL canSelectMultiple = TRUE;
    RequireSucceeded(treeSelectionProvider->get_CanSelectMultiple(&canSelectMultiple), "tree selection provider reports multi-select capability");
    Require(canSelectMultiple == FALSE, "tree selection provider reports single-selection behavior");
    Require(ReadSelectionProviderNames(*treeSelectionProvider.get(), "tree selection provider returns selected item names") ==
                std::vector<std::wstring>{L"Plugins"},
            "tree selection provider resolves the currently selected tree item");

    RequireSucceeded(selectionPattern->RemoveFromSelection(), "tree item selection pattern can remove the current selection");
    Require(! tree->GetSelectedItemId().has_value(), "tree selection removal clears the selected item");
    Require(! ReadProviderBoolProperty(*parentItemSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "tree item selected state updates after removal"),
            "tree item provider reports deselection after RemoveFromSelection");

    RequireSucceeded(selectionPattern->Select(), "tree item selection pattern can restore the selection");
    Require(tree->GetSelectedItemId() && tree->GetSelectedItemId().value() == 10u, "tree item selection pattern selects the parent item");
    Require(delegate.selectionChangedCount == 1u && delegate.lastSelectedItemId && delegate.lastSelectedItemId.value() == 10u,
            "tree selection pattern uses the shared delegate-driven selection path");

    wil::com_ptr_nothrow<IUnknown> expandPatternUnknown;
    RequireSucceeded(parentItemSimple->GetPatternProvider(UIA_ExpandCollapsePatternId, expandPatternUnknown.put()),
                     "tree item expand-collapse pattern lookup succeeds");
    Require(expandPatternUnknown != nullptr, "expandable tree item exposes expand-collapse pattern");
    wil::com_ptr_nothrow<IExpandCollapseProvider> expandPattern;
    RequireSucceeded(expandPatternUnknown.query_to(expandPattern.put()), "expand-collapse pattern supports IExpandCollapseProvider");

    ExpandCollapseState expandState = ExpandCollapseState_LeafNode;
    RequireSucceeded(expandPattern->get_ExpandCollapseState(&expandState), "tree item expand state query succeeds");
    Require(expandState == ExpandCollapseState_Collapsed, "tree item expand-collapse pattern reports the collapsed state");

    Require(! window.Host().DebugHasActiveAnimationSubscription(), "tree item expand-collapse pattern starts without an active host animation");
    RequireSucceeded(expandPattern->Expand(), "tree item expand-collapse pattern can expand the parent item");
    Require(delegate.toggleCount == 1u && delegate.lastToggledItemId && delegate.lastToggledItemId.value() == 10u && delegate.lastExpandedState == true,
            "tree item expand-collapse pattern uses the shared delegate-driven expansion path");
    Require(window.Host().DebugHasActiveAnimationSubscription(), "tree item expand-collapse pattern requests host animation for expansion visuals");
    Require(treeModel.GetVisibleItemCount() == 3u, "tree model exposes the child item after Expand");
    RequireSucceeded(expandPattern->get_ExpandCollapseState(&expandState), "expanded tree item state query succeeds");
    Require(expandState == ExpandCollapseState_Expanded, "tree item expand-collapse pattern reports the expanded state");

    wil::com_ptr_nothrow<IRawElementProviderFragment> childItemProvider;
    RequireSucceeded(parentItemProvider->Navigate(NavigateDirection_NextSibling, childItemProvider.put()),
                     "expanded parent tree item navigates to its first visible child");
    Require(childItemProvider != nullptr, "expanded parent tree item returns the child provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> childItemSimple;
    RequireSucceeded(childItemProvider.query_to(childItemSimple.put()), "child tree-item provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*childItemSimple.get(), UIA_NamePropertyId, "child tree item exposes accessibility name") == L"FTP",
            "expanded tree item navigation reaches the expected child");

    wil::com_ptr_nothrow<IUnknown> childSelectionUnknown;
    RequireSucceeded(childItemSimple->GetPatternProvider(UIA_SelectionItemPatternId, childSelectionUnknown.put()),
                     "child tree-item selection pattern lookup succeeds");
    Require(childSelectionUnknown != nullptr, "child tree item exposes selection-item pattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> childSelectionPattern;
    RequireSucceeded(childSelectionUnknown.query_to(childSelectionPattern.put()), "child selection pattern supports ISelectionItemProvider");

    RequireSucceeded(childSelectionPattern->AddToSelection(), "child tree-item selection pattern can select the child");
    Require(tree->GetSelectedItemId() && tree->GetSelectedItemId().value() == 11u, "child tree item selection updates the tree selection");
    Require(delegate.selectionChangedCount == 2u && delegate.lastSelectedItemId && delegate.lastSelectedItemId.value() == 11u,
            "child tree item selection continues to use the shared delegate path");
    Require(ReadSelectionProviderNames(*treeSelectionProvider.get(), "tree selection provider updates after child selection") ==
                std::vector<std::wstring>{L"FTP"},
            "tree selection provider tracks the newly selected visible tree item");

    wil::com_ptr_nothrow<IUnknown> childExpandUnknown;
    RequireSucceeded(childItemSimple->GetPatternProvider(UIA_ExpandCollapsePatternId, childExpandUnknown.put()),
                     "leaf tree-item expand-collapse lookup succeeds");
    Require(childExpandUnknown == nullptr, "leaf tree item does not expose expand-collapse pattern");

    RequireSucceeded(expandPattern->Collapse(), "tree item expand-collapse pattern can collapse the parent item");
    Require(delegate.toggleCount == 2u && delegate.lastExpandedState == false, "tree item collapse again uses the shared delegate path");
    Require(treeModel.GetVisibleItemCount() == 2u, "tree model hides the child item after Collapse");
    Require(! tree->GetSelectedItemId().has_value(), "tree collapse clears a selection that is no longer visible");
    RequireSucceeded(expandPattern->get_ExpandCollapseState(&expandState), "collapsed tree item state query succeeds after Collapse");
    Require(expandState == ExpandCollapseState_Collapsed, "tree item expand-collapse pattern reports the collapsed state after Collapse");
}

// A multi-select tree reports what it holds through the Selection pattern and each item's SelectionItem pattern, and the
// pattern's actions change one item's membership (or the whole selection, for Select) through the tree's own callbacks.
void TestAccessibilityTreeMultiSelectExposesSelectionPatternsAndItemState()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* treeLabel = root->AddChild<Label>(L"Categories");
    treeLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 168.0f));
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Panes"},
        TreeItemData{.id = 3u, .text = L"Viewers"},
        TreeItemData{.id = 4u, .text = L"Network"},
    });
    RecordingTreeDelegate delegate;
    delegate.observedTree = tree;
    tree->SetModel(&treeModel);
    tree->SetDelegate(&delegate);
    treeLabel->SetMnemonicTarget(tree);
    tree->SetMultiSelectEnabled(true);
    tree->SetSelectedItemIds(std::vector<uint64_t>{1u, 3u});
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(tree);

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "multi-select tree accessibility test creates a root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 48.0f, 12.0f, "the label of the multi-select tree is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> treeProvider;
    RequireSucceeded(treeLabelProvider->Navigate(NavigateDirection_NextSibling, treeProvider.put()), "the label navigates to the multi-select tree");
    Require(treeProvider != nullptr, "the tree follows its label");

    // The four items, in order.
    std::vector<wil::com_ptr_nothrow<IRawElementProviderSimple>> items;
    wil::com_ptr_nothrow<IRawElementProviderFragment> walker;
    RequireSucceeded(treeProvider->Navigate(NavigateDirection_FirstChild, walker.put()), "the tree navigates to its first item");
    while (walker)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(walker.query_to(simple.put()), "a tree item is a simple provider");
        items.push_back(simple);
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(walker->Navigate(NavigateDirection_NextSibling, next.put()), "a tree item navigates to its sibling");
        walker = std::move(next);
    }
    Require(items.size() == 4u, "the tree exposes its four items");
    const auto selectionItem = [&](size_t index)
    {
        wil::com_ptr_nothrow<IUnknown> unknown;
        RequireSucceeded(items[index]->GetPatternProvider(UIA_SelectionItemPatternId, unknown.put()), "a tree item exposes SelectionItem");
        wil::com_ptr_nothrow<ISelectionItemProvider> pattern;
        RequireSucceeded(unknown.query_to(pattern.put()), "the pattern is an ISelectionItemProvider");
        return pattern;
    };
    const auto isSelectedProperty = [&](size_t index)
    { return ReadProviderBoolProperty(*items[index].get(), UIA_SelectionItemIsSelectedPropertyId, "a tree item reports IsSelected"); };
    const auto isSelectedPattern = [&](size_t index)
    {
        BOOL selected = FALSE;
        RequireSucceeded(selectionItem(index)->get_IsSelected(&selected), "SelectionItem reports IsSelected");
        return selected != FALSE;
    };
    wil::com_ptr_nothrow<IRawElementProviderSimple> container;
    RequireSucceeded(selectionItem(0u)->get_SelectionContainer(container.put()), "an item names its selection container");
    wil::com_ptr_nothrow<IUnknown> containerPattern;
    RequireSucceeded(container->GetPatternProvider(UIA_SelectionPatternId, containerPattern.put()), "the tree exposes the Selection pattern");
    wil::com_ptr_nothrow<ISelectionProvider> selection;
    RequireSucceeded(containerPattern.query_to(selection.put()), "the pattern is an ISelectionProvider");
    const auto selectedNames = [&](const char* context) { return ReadSelectionProviderNames(*selection.get(), context); };

    // The Selection pattern says the tree can select several items, and every selected item says it is selected.
    BOOL canSelectMultiple = FALSE;
    RequireSucceeded(selection->get_CanSelectMultiple(&canSelectMultiple), "the tree reports whether it selects several items");
    Require(canSelectMultiple == TRUE, "a multi-select tree reports CanSelectMultiple");
    Require(selectedNames("the multi-select tree reports its selection") == std::vector<std::wstring>{L"General", L"Viewers"},
            "the Selection pattern lists every selected item in order");
    Require(isSelectedProperty(0u) && ! isSelectedProperty(1u) && isSelectedProperty(2u) && ! isSelectedProperty(3u),
            "each item's IsSelected property agrees with the selection");
    Require(isSelectedPattern(0u) && ! isSelectedPattern(1u) && isSelectedPattern(2u) && ! isSelectedPattern(3u),
            "each SelectionItem pattern agrees with the selection");

    // Only the focused item has the keyboard focus, whatever is selected.
    const auto hasKeyboardFocus = [&](size_t index)
    { return ReadProviderBoolProperty(*items[index].get(), UIA_HasKeyboardFocusPropertyId, "a tree item reports keyboard focus"); };
    Require(! hasKeyboardFocus(0u) && ! hasKeyboardFocus(1u) && hasKeyboardFocus(2u) && ! hasKeyboardFocus(3u),
            "only the focused item of the selection has the keyboard focus");
    Require(ReadFocusedElementName(*rootProvider.get()) == L"Viewers", "the window's focus is the focused item");
    tree->SetFocusedItemId(2u);
    Require(hasKeyboardFocus(1u) && ! hasKeyboardFocus(2u) && ! isSelectedProperty(1u), "the focus can rest on an item that is not selected");
    Require(ReadFocusedElementName(*rootProvider.get()) == L"Panes", "the window's focus follows the focused item");

    // AddToSelection adds one item and keeps the rest, RemoveFromSelection removes one, Select replaces the selection.
    RequireSucceeded(selectionItem(1u)->AddToSelection(), "AddToSelection adds an item to a multi-select tree");
    RequireTreeIds(tree->GetSelectedItemIds(), {1u, 2u, 3u}, "AddToSelection keeps the items already selected");
    Require(selectedNames("the Selection pattern after AddToSelection") == std::vector<std::wstring>{L"General", L"Panes", L"Viewers"},
            "the Selection pattern lists the added item");
    Require(isSelectedProperty(1u) && isSelectedPattern(1u), "the added item reports itself selected");
    Require(delegate.selectionSetChangedCount == 1u && delegate.callOrder == "PS", "AddToSelection reaches the delegate as the gesture it stands for");
    RequireSucceeded(selectionItem(1u)->AddToSelection(), "AddToSelection of a selected item succeeds");
    RequireTreeIds(tree->GetSelectedItemIds(), {1u, 2u, 3u}, "AddToSelection is not a toggle");
    Require(delegate.selectionSetChangedCount == 1u, "AddToSelection of a selected item changes no set");
    RequireSucceeded(selectionItem(0u)->RemoveFromSelection(), "RemoveFromSelection removes an item");
    RequireTreeIds(tree->GetSelectedItemIds(), {2u, 3u}, "RemoveFromSelection keeps the other items");
    Require(! isSelectedProperty(0u) && ! isSelectedPattern(0u), "the removed item reports itself unselected");
    Require(delegate.selectionSetChangedCount == 2u, "RemoveFromSelection reaches the delegate once");
    RequireSucceeded(selectionItem(0u)->RemoveFromSelection(), "RemoveFromSelection of an unselected item succeeds");
    Require(delegate.selectionSetChangedCount == 2u, "removing an unselected item changes no set");
    RequireSucceeded(selectionItem(3u)->Select(), "Select replaces the selection");
    RequireTreeIds(tree->GetSelectedItemIds(), {4u}, "Select leaves only its item");
    Require(selectedNames("the Selection pattern after Select") == std::vector<std::wstring>{L"Network"}, "the Selection pattern lists the one selected item");
    Require(delegate.selectionSetChangedCount == 3u && delegate.lastSelectedItemId == 4u, "Select reaches the delegate once");

    // SetFocus moves the focus and leaves the selection, which a selection that follows the focus would not.
    wil::com_ptr_nothrow<IRawElementProviderFragment> firstFragment;
    RequireSucceeded(items[0]->QueryInterface(IID_PPV_ARGS(firstFragment.put())), "an item is a fragment");
    RequireSucceeded(firstFragment->SetFocus(), "SetFocus on a tree item succeeds");
    RequireTreeIds(tree->GetSelectedItemIds(), {4u}, "SetFocus leaves the multi-selection alone");
    Require(tree->GetFocusedItemId() == 1u && ! isSelectedProperty(0u), "SetFocus moved the focus to an item that stays unselected");

    // Turning multi-select off restores the single-selection answers.
    tree->SetMultiSelectEnabled(false);
    RequireSucceeded(selection->get_CanSelectMultiple(&canSelectMultiple), "the tree reports whether it selects several items after the switch");
    Require(canSelectMultiple == FALSE, "a tree without multi-select reports CanSelectMultiple FALSE");
    Require(selectedNames("the Selection pattern of a single-select tree") == std::vector<std::wstring>{L"Network"}, "a single-select tree lists its one item");
    Require(! isSelectedProperty(0u) && isSelectedProperty(3u), "the single selection is the one item that stayed selected");
}

void TestAccessibilityOffscreenSelectedGridRowPatternRemainsUsable()
{
    using namespace DxUi;
    constexpr size_t rowCount = 1000;
    MultiRowGridModel model(rowCount);
    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"Results");
    label->SetBounds(D2D1::RectF(0, 0, 120, 24));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0, 28, 320, 140));
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));
    Require(grid->OnSelectAll(window.Host()), "select rows beyond the bounded materialization cache");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "offscreen selection fixture creates a provider");
    auto labelProvider = GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40, 12, "resolve label before grid");
    wil::com_ptr_nothrow<IRawElementProviderFragment> gridProvider;
    RequireSucceeded(labelProvider->Navigate(NavigateDirection_NextSibling, gridProvider.put()), "resolve grid sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> gridSimple;
    RequireSucceeded(gridProvider.query_to(gridSimple.put()), "grid exposes the simple provider");
    wil::com_ptr_nothrow<IUnknown> selectionUnknown;
    RequireSucceeded(gridSimple->GetPatternProvider(UIA_SelectionPatternId, selectionUnknown.put()), "get Grid selection pattern");
    wil::com_ptr_nothrow<ISelectionProvider> selectionProvider;
    RequireSucceeded(selectionUnknown.query_to(selectionProvider.put()), "query Grid selection interface");
    unique_safearray selected;
    RequireSucceeded(selectionProvider->GetSelection(std::out_ptr(selected)), "all selected rows have providers");
    LONG last = -1;
    RequireSucceeded(SafeArrayGetUBound(selected.get(), 1, &last), "selected array has an upper bound");
    Require(last == static_cast<LONG>(rowCount - 1), "selection includes rows beyond the materialization budget");
    wil::com_ptr_nothrow<IUnknown> rowUnknown;
    RequireSucceeded(SafeArrayGetElement(selected.get(), &last, rowUnknown.put()), "read last selected row provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rowSimple;
    RequireSucceeded(rowUnknown.query_to(rowSimple.put()), "offscreen row exposes the simple provider");
    wil::com_ptr_nothrow<IUnknown> patternUnknown;
    RequireSucceeded(rowSimple->GetPatternProvider(UIA_SelectionItemPatternId, patternUnknown.put()), "offscreen selected row exposes SelectionItem");
    Require(patternUnknown != nullptr, "offscreen selected row has a usable pattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> item;
    RequireSucceeded(patternUnknown.query_to(item.put()), "query selected row pattern");
    BOOL isSelected = FALSE;
    RequireSucceeded(item->get_IsSelected(&isSelected), "offscreen selected row selection getter succeeds");
    Require(isSelected != FALSE, "offscreen selection getter agrees with GetSelection");
    wil::com_ptr_nothrow<IRawElementProviderSimple> container;
    RequireSucceeded(item->get_SelectionContainer(container.put()), "offscreen selection container getter succeeds");
    Require(container != nullptr, "offscreen selected row retains its Grid container");
    grid->SetModel(nullptr);
    isSelected = TRUE;
    Require(FAILED(item->get_IsSelected(&isSelected)) && isSelected == FALSE, "removed rows invalidate retained selection patterns");
    container.reset();
    Require(FAILED(item->get_SelectionContainer(container.put())) && ! container, "removed rows cannot retain a stale selection container");
}

void TestAccessibilityTrimmedMultilineGridCellKeepsCompleteNameAndValue()
{
    using namespace DxUi;

    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 620, 550, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    GridCellData cell{};
    cell.kind      = GridCellKind::Text;
    cell.multiline = true;
    cell.text =
        L"Première ligne française\r\nDeuxième ligne é\r\nTroisième ligne 📷 🍊\r\nQuatrième ligne 👨‍👩‍👧\r\nDernière ligne complète";
    const std::wstring completeText = cell.text;
    SingleCellGridModel textModel(cell);
    GridCellData prefix = cell;
    prefix.text         = L"Première ligne française\nDeuxième ligne é";
    SingleCellGridModel prefixModel(prefix);

    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"Grid witness");
    label->SetBounds(D2D1::RectF(360.0f, 20.0f, 500.0f, 44.0f));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 230.0f));
    grid->SetRowHeightDip(160.0f);
    grid->SetHeaderHeightDip(30.0f);
    grid->SetLineClamp(2u);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->SetModel(&prefixModel);
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    const auto detachModel = wil::scope_exit([&] { grid->SetModel(nullptr); });

    const auto capture = [&](const char* context)
    {
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        window.PumpMessages();
        WindowHostBitmapCapture bitmap;
        Require(window.Host().DebugCaptureBitmap(bitmap), context);
        return bitmap;
    };
    const auto prefixCapture = capture("capture two complete multiline Grid prefix lines");
    grid->SetModel(&textModel);
    grid->ApplyColumnLayout(columns);
    const auto trimmedCapture = capture("capture trimmed multiline Grid cell");
    Require(prefixCapture.widthPx == trimmedCapture.widthPx && prefixCapture.heightPx == trimmedCapture.heightPx,
            "trimmed Grid and prefix reference use the same bitmap extent");
    const auto px           = [&](float dip) { return static_cast<UINT>(window.Host().DipsToPixels(dip)); };
    uint64_t omissionPixels = 0u;
    for (UINT y = px(54.0f); y < std::min(px(207.0f), trimmedCapture.heightPx); ++y)
        for (UINT x = px(29.0f); x < std::min(px(311.0f), trimmedCapture.widthPx); ++x)
        {
            const size_t offset = (static_cast<size_t>(y) * trimmedCapture.widthPx + x) * 4u;
            for (size_t channel = 0u; channel < 4u; ++channel)
                omissionPixels += prefixCapture.bgraPixels[offset + channel] != trimmedCapture.bgraPixels[offset + channel] ? 1u : 0u;
        }
    Require(omissionPixels > 0u, "trimmed Grid paints an omission marker beyond the same two complete prefix lines");

    grid->GetSelectionModel().SetSingle(textModel.GetStableRowId(0u));
    grid->NotifyDataChanged();
    Require(grid->GetSelectionModel().GetCount() == 1u && grid->IsRowSelected(0u), "UIA fixture starts with its only row selected");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "trimmed Grid fixture creates an accessibility provider");
    auto labelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 390.0f, 30.0f, "trimmed Grid label provider resolves by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> gridProvider;
    RequireSucceeded(labelProvider->Navigate(NavigateDirection_NextSibling, gridProvider.put()), "trimmed Grid label navigates to Grid sibling");
    Require(gridProvider != nullptr, "trimmed Grid provider exists");
    wil::com_ptr_nothrow<IRawElementProviderFragment> headerProvider;
    RequireSucceeded(gridProvider->Navigate(NavigateDirection_FirstChild, headerProvider.put()), "trimmed Grid exposes its column header");
    Require(headerProvider != nullptr, "trimmed Grid column header exists");
    wil::com_ptr_nothrow<IRawElementProviderFragment> rowProvider;
    RequireSucceeded(headerProvider->Navigate(NavigateDirection_NextSibling, rowProvider.put()), "trimmed Grid header navigates to its row");
    Require(rowProvider != nullptr, "trimmed Grid row provider exists");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rowSimple;
    RequireSucceeded(rowProvider.query_to(rowSimple.put()), "trimmed Grid row exposes a simple provider");
    Require(ReadProviderBoolProperty(*rowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "trimmed Grid row exposes selected state"),
            "trimmed Grid row is selected before reading its cell");
    wil::com_ptr_nothrow<IRawElementProviderFragment> cellProvider;
    RequireSucceeded(rowProvider->Navigate(NavigateDirection_FirstChild, cellProvider.put()), "trimmed Grid row navigates to its cell");
    Require(cellProvider != nullptr, "trimmed Grid cell provider exists");
    wil::com_ptr_nothrow<IRawElementProviderSimple> cellSimple;
    RequireSucceeded(cellProvider.query_to(cellSimple.put()), "trimmed Grid cell exposes a simple provider");
    Require(ReadProviderStringProperty(*cellSimple.get(), UIA_NamePropertyId, "trimmed Grid cell exposes full Name") == completeText,
            "trimmed Grid cell UIA Name retains every original Unicode paragraph");
    Require(ReadProviderStringProperty(*cellSimple.get(), UIA_ValueValuePropertyId, "trimmed Grid cell exposes full Value") == completeText,
            "trimmed Grid cell UIA Value retains every original Unicode paragraph");
    wil::com_ptr_nothrow<IUnknown> valueUnknown;
    RequireSucceeded(cellSimple->GetPatternProvider(UIA_ValuePatternId, valueUnknown.put()), "trimmed Grid cell ValuePattern lookup succeeds");
    Require(valueUnknown != nullptr, "trimmed Grid cell exposes ValuePattern");
    wil::com_ptr_nothrow<IValueProvider> valueProvider;
    RequireSucceeded(valueUnknown.query_to(valueProvider.put()), "trimmed Grid cell ValuePattern supports IValueProvider");
    wil::unique_bstr value;
    RequireSucceeded(valueProvider->get_Value(value.put()), "trimmed Grid cell ValuePattern returns complete value");
    Require(std::wstring(value.get() ? value.get() : L"") == completeText, "trimmed Grid cell ValuePattern preserves the complete model text");
    Require(grid->GetSelectionModel().GetCount() == 1u && grid->IsRowSelected(0u) &&
                ReadProviderBoolProperty(*rowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "trimmed Grid row remains selected"),
            "reading trimmed Grid accessibility text leaves selection unchanged");
}

void TestAccessibilityProviderExposesGridRowSelectionPatterns()
{
    using namespace DxUi;

    class AccessibleGridModel final : public IGridModel
    {
    public:
        struct Row
        {
            uint64_t stableId = 0u;
            std::wstring name;
            std::wstring status;
        };

        explicit AccessibleGridModel(std::vector<Row> rows) : _rows(std::move(rows))
        {
        }

        [[nodiscard]] size_t GetRowCount() const noexcept override
        {
            return _rows.size();
        }

        [[nodiscard]] size_t GetColumnCount() const noexcept override
        {
            return 2u;
        }

        [[nodiscard]] GridColumnDesc GetColumn(size_t columnIndex) const override
        {
            GridColumnDesc column;
            if (columnIndex == 0u)
            {
                column.id       = L"name";
                column.title    = L"Name";
                column.widthDip = 140.0f;
            }
            else
            {
                column.id       = L"status";
                column.title    = L"Status";
                column.widthDip = 100.0f;
            }
            return column;
        }

        void GetCellData(size_t rowIndex, size_t columnIndex, GridCellData& outCell) const override
        {
            const Row& row = _rows.at(rowIndex);
            outCell.kind   = GridCellKind::Text;
            outCell.text   = (columnIndex == 0u) ? row.name : row.status;
        }

        [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
        {
            return _rows[rowIndex].stableId;
        }

        [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
        {
            for (size_t rowIndex = 0u; rowIndex < _rows.size(); ++rowIndex)
            {
                if (_rows[rowIndex].stableId == rowId)
                {
                    return rowIndex;
                }
            }

            return std::nullopt;
        }

    private:
        std::vector<Row> _rows;
    };

    class AccessibleGridDelegate final : public IGridDelegate
    {
    public:
        using IGridDelegate::OnGridSelectionChanged;

        void OnGridSelectionChanged(Grid& sender) override
        {
            ++selectionChangedCount;
            selectionCounts.push_back(sender.GetSelectionModel().GetCount());
            orderedSelection.assign(sender.GetSelectionModel().GetOrderedSelection().begin(), sender.GetSelectionModel().GetOrderedSelection().end());
        }

        size_t selectionChangedCount = 0u;
        std::vector<size_t> selectionCounts;
        std::vector<uint64_t> orderedSelection;
    };

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* gridLabel = root->AddChild<Label>(L"Results");
    gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 260.0f, 140.0f));

    AccessibleGridModel gridModel({AccessibleGridModel::Row{100u, L"Alpha", L"Ready"},
                                   AccessibleGridModel::Row{200u, L"Beta", L"Busy"},
                                   AccessibleGridModel::Row{300u, L"Gamma", L"Idle"}});
    AccessibleGridDelegate delegate;
    grid->SetModel(&gridModel);
    grid->SetDelegate(&delegate);
    gridLabel->SetMnemonicTarget(grid);

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "grid-row accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> gridLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 12.0f, "grid label accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> gridProvider;
    RequireSucceeded(gridLabelProvider->Navigate(NavigateDirection_NextSibling, gridProvider.put()), "grid label accessibility provider navigates to the grid");
    Require(gridProvider != nullptr, "grid label accessibility provider returns the grid as the next sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> gridSimple;
    RequireSucceeded(gridProvider.query_to(gridSimple.put()), "grid accessibility provider exposes IRawElementProviderSimple");
    wil::com_ptr_nothrow<IUnknown> tablePatternUnknown;
    RequireSucceeded(gridSimple->GetPatternProvider(UIA_TablePatternId, tablePatternUnknown.put()), "grid table-pattern lookup succeeds");
    Require(tablePatternUnknown != nullptr, "grid accessibility provider exposes the table pattern");
    wil::com_ptr_nothrow<ITableProvider> tablePattern;
    RequireSucceeded(tablePatternUnknown.query_to(tablePattern.put()), "grid table pattern supports ITableProvider");

    SAFEARRAY* columnHeadersArray = nullptr;
    RequireSucceeded(tablePattern->GetColumnHeaders(&columnHeadersArray), "grid table pattern returns visible column headers");
    Require(columnHeadersArray != nullptr, "grid table pattern returns a column-header array");
    const auto destroyColumnHeadersArray = wil::scope_exit([&] { SafeArrayDestroy(columnHeadersArray); });
    Require(ReadProviderArrayNames(columnHeadersArray, "grid table column headers expose header names") == std::vector<std::wstring>({L"Name", L"Status"}),
            "grid table pattern exposes visible grid header fragments in display order");

    SAFEARRAY* rowHeadersArray = nullptr;
    RequireSucceeded(tablePattern->GetRowHeaders(&rowHeadersArray), "grid table pattern row-header lookup succeeds");
    Require(rowHeadersArray != nullptr, "grid table pattern returns a row-header array");
    const auto destroyRowHeadersArray = wil::scope_exit([&] { SafeArrayDestroy(rowHeadersArray); });
    Require(ReadProviderArrayNames(rowHeadersArray, "grid table row headers return an empty array").empty(),
            "grid table pattern reports no row-header fragments for row-headerless grids");

    RowOrColumnMajor rowOrColumnMajor = RowOrColumnMajor_RowMajor;
    RequireSucceeded(tablePattern->get_RowOrColumnMajor(&rowOrColumnMajor), "grid table pattern row-or-column-major query succeeds");
    Require(rowOrColumnMajor == RowOrColumnMajor_Indeterminate, "grid table pattern reports indeterminate row/column major order");

    const std::optional<D2D1_RECT_F> firstHeaderRect = grid->GetVisibleColumnHeaderRect(0u);
    Require(firstHeaderRect.has_value(), "grid exposes a visible header rect for point hit-testing");
    const float firstHeaderCenterXDip                                = (firstHeaderRect->left + firstHeaderRect->right) * 0.5f;
    const float firstHeaderCenterYDip                                = (firstHeaderRect->top + firstHeaderRect->bottom) * 0.5f;
    wil::com_ptr_nothrow<IRawElementProviderFragment> headerProvider = GetProviderAtDipPoint(window.Hwnd(),
                                                                                             window.Host(),
                                                                                             *rootProvider.get(),
                                                                                             firstHeaderCenterXDip,
                                                                                             firstHeaderCenterYDip,
                                                                                             "grid header accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> headerSimple;
    RequireSucceeded(headerProvider.query_to(headerSimple.put()), "grid header accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*headerSimple.get(), UIA_ControlTypePropertyId, "grid header exposes UIA control type") == UIA_HeaderItemControlTypeId,
            "grid hit-testing resolves the visible column-header fragment");
    Require(ReadProviderStringProperty(*headerSimple.get(), UIA_NamePropertyId, "grid header exposes accessibility name") == L"Name",
            "grid header accessibility provider exposes the visible column header title");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstHeaderFragmentProvider;
    RequireSucceeded(gridProvider->Navigate(NavigateDirection_FirstChild, firstHeaderFragmentProvider.put()),
                     "grid accessibility provider navigates to the first visible header");
    Require(firstHeaderFragmentProvider != nullptr, "grid accessibility provider returns a first header child");
    wil::com_ptr_nothrow<IRawElementProviderSimple> firstHeaderFragmentSimple;
    RequireSucceeded(firstHeaderFragmentProvider.query_to(firstHeaderFragmentSimple.put()), "grid header fragment exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*firstHeaderFragmentSimple.get(), UIA_ControlTypePropertyId, "grid header fragment exposes UIA control type") ==
                UIA_HeaderItemControlTypeId,
            "grid accessibility provider reports a header-item first child when visible headers are present");
    Require(ReadProviderStringProperty(*firstHeaderFragmentSimple.get(), UIA_NamePropertyId, "grid header fragment exposes accessibility name") == L"Name",
            "grid accessibility provider returns the first visible column header before row fragments");

    wil::com_ptr_nothrow<IRawElementProviderFragment> secondHeaderProvider;
    RequireSucceeded(firstHeaderFragmentProvider->Navigate(NavigateDirection_NextSibling, secondHeaderProvider.put()),
                     "grid header fragment navigates to the next visible header");
    Require(secondHeaderProvider != nullptr, "grid header fragment returns the next visible header sibling");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondHeaderSimple;
    RequireSucceeded(secondHeaderProvider.query_to(secondHeaderSimple.put()), "second grid header fragment exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*secondHeaderSimple.get(), UIA_NamePropertyId, "second grid header exposes accessibility name") == L"Status",
            "grid accessibility provider exposes the remaining visible column headers before row fragments");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstRowProvider;
    RequireSucceeded(secondHeaderProvider->Navigate(NavigateDirection_NextSibling, firstRowProvider.put()),
                     "grid header fragment navigates to the first visible row after the last header");
    Require(firstRowProvider != nullptr, "grid accessibility provider returns a first row child after visible headers");
    wil::com_ptr_nothrow<IRawElementProviderSimple> firstRowSimple;
    RequireSucceeded(firstRowProvider.query_to(firstRowSimple.put()), "grid row accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*firstRowSimple.get(), UIA_ControlTypePropertyId, "grid row exposes UIA control type") == UIA_DataItemControlTypeId,
            "grid row accessibility provider reports data-item control type");
    Require(ReadProviderStringProperty(*firstRowSimple.get(), UIA_NamePropertyId, "grid row exposes accessibility name") == L"Alpha | Ready",
            "grid row accessibility provider exposes joined visible cell text");
    Require(! ReadProviderBoolProperty(*firstRowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "grid row exposes selected state"),
            "grid row accessibility provider reports the unselected initial row");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstCellProvider;
    RequireSucceeded(firstRowProvider->Navigate(NavigateDirection_FirstChild, firstCellProvider.put()),
                     "grid row accessibility provider navigates to the first visible cell");
    Require(firstCellProvider != nullptr, "grid row accessibility provider returns a first cell child");
    wil::com_ptr_nothrow<IRawElementProviderSimple> firstCellSimple;
    RequireSucceeded(firstCellProvider.query_to(firstCellSimple.put()), "grid cell accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*firstCellSimple.get(), UIA_ControlTypePropertyId, "grid cell exposes UIA control type") == UIA_TextControlTypeId,
            "grid cell accessibility provider reports text control type for text cells");
    Require(ReadProviderStringProperty(*firstCellSimple.get(), UIA_NamePropertyId, "grid cell exposes accessibility name") == L"Alpha",
            "grid cell accessibility provider exposes the visible cell text");
    Require(ReadProviderStringProperty(*firstCellSimple.get(), UIA_ValueValuePropertyId, "grid cell exposes value") == L"Alpha",
            "grid text cell accessibility provider exposes a read-only value");
    Require(ReadProviderBoolProperty(*firstCellSimple.get(), UIA_ValueIsReadOnlyPropertyId, "grid cell exposes read-only state"),
            "grid text cell accessibility provider reports the value pattern as read-only");

    wil::com_ptr_nothrow<IUnknown> firstCellValueUnknown;
    RequireSucceeded(firstCellSimple->GetPatternProvider(UIA_ValuePatternId, firstCellValueUnknown.put()), "grid cell value-pattern lookup succeeds");
    Require(firstCellValueUnknown != nullptr, "grid text cell accessibility provider exposes the value pattern");
    wil::com_ptr_nothrow<IValueProvider> firstCellValuePattern;
    RequireSucceeded(firstCellValueUnknown.query_to(firstCellValuePattern.put()), "grid cell value pattern supports IValueProvider");
    BSTR firstCellValue = nullptr;
    RequireSucceeded(firstCellValuePattern->get_Value(&firstCellValue), "grid cell value pattern returns the visible cell text");
    const auto freeFirstCellValue = wil::scope_exit([&] { SysFreeString(firstCellValue); });
    Require(std::wstring(firstCellValue ? firstCellValue : L"") == L"Alpha", "grid cell value pattern returns the expected visible text value");
    BOOL firstCellReadOnly = FALSE;
    RequireSucceeded(firstCellValuePattern->get_IsReadOnly(&firstCellReadOnly), "grid cell value pattern read-only lookup succeeds");
    Require(firstCellReadOnly == TRUE, "grid cell value pattern reports a read-only value");

    wil::com_ptr_nothrow<IUnknown> firstCellGridItemUnknown;
    RequireSucceeded(firstCellSimple->GetPatternProvider(UIA_GridItemPatternId, firstCellGridItemUnknown.put()), "grid cell grid-item pattern lookup succeeds");
    Require(firstCellGridItemUnknown != nullptr, "grid cell accessibility provider exposes the grid-item pattern");
    wil::com_ptr_nothrow<IGridItemProvider> firstCellGridItemPattern;
    RequireSucceeded(firstCellGridItemUnknown.query_to(firstCellGridItemPattern.put()), "grid cell grid-item pattern supports IGridItemProvider");
    wil::com_ptr_nothrow<IUnknown> firstCellTableItemUnknown;
    RequireSucceeded(firstCellSimple->GetPatternProvider(UIA_TableItemPatternId, firstCellTableItemUnknown.put()),
                     "grid cell table-item pattern lookup succeeds");
    Require(firstCellTableItemUnknown != nullptr, "grid cell accessibility provider exposes the table-item pattern");
    wil::com_ptr_nothrow<ITableItemProvider> firstCellTableItemPattern;
    RequireSucceeded(firstCellTableItemUnknown.query_to(firstCellTableItemPattern.put()), "grid cell table-item pattern supports ITableItemProvider");

    int firstCellRow        = -1;
    int firstCellColumn     = -1;
    int firstCellRowSpan    = 0;
    int firstCellColumnSpan = 0;
    RequireSucceeded(firstCellGridItemPattern->get_Row(&firstCellRow), "grid cell grid-item row query succeeds");
    RequireSucceeded(firstCellGridItemPattern->get_Column(&firstCellColumn), "grid cell grid-item column query succeeds");
    RequireSucceeded(firstCellGridItemPattern->get_RowSpan(&firstCellRowSpan), "grid cell grid-item row-span query succeeds");
    RequireSucceeded(firstCellGridItemPattern->get_ColumnSpan(&firstCellColumnSpan), "grid cell grid-item column-span query succeeds");
    Require(firstCellRow == 0 && firstCellColumn == 0, "grid cell grid-item metadata reports the expected row and column");
    Require(firstCellRowSpan == 1 && firstCellColumnSpan == 1, "grid cell grid-item metadata reports single-cell spans");

    wil::com_ptr_nothrow<IRawElementProviderSimple> firstCellContainingGrid;
    RequireSucceeded(firstCellGridItemPattern->get_ContainingGrid(firstCellContainingGrid.put()), "grid cell containing-grid lookup succeeds");
    Require(firstCellContainingGrid != nullptr, "grid cell grid-item pattern resolves the containing grid");
    Require(ReadProviderStringProperty(*firstCellContainingGrid.get(), UIA_NamePropertyId, "grid cell containing grid exposes accessibility name") ==
                L"Results",
            "grid cell grid-item pattern resolves the labeled grid container");

    SAFEARRAY* firstCellColumnHeadersArray = nullptr;
    RequireSucceeded(firstCellTableItemPattern->GetColumnHeaderItems(&firstCellColumnHeadersArray),
                     "grid cell table-item pattern returns the owning column header");
    Require(firstCellColumnHeadersArray != nullptr, "grid cell table-item pattern returns a column-header array");
    const auto destroyFirstCellColumnHeadersArray = wil::scope_exit([&] { SafeArrayDestroy(firstCellColumnHeadersArray); });
    Require(ReadProviderArrayNames(firstCellColumnHeadersArray, "grid cell table-item column headers expose the owning header") ==
                std::vector<std::wstring>{L"Name"},
            "grid cell table-item pattern resolves the visible owning column header");

    SAFEARRAY* firstCellRowHeadersArray = nullptr;
    RequireSucceeded(firstCellTableItemPattern->GetRowHeaderItems(&firstCellRowHeadersArray), "grid cell table-item row-header lookup succeeds");
    Require(firstCellRowHeadersArray != nullptr, "grid cell table-item pattern returns a row-header array");
    const auto destroyFirstCellRowHeadersArray = wil::scope_exit([&] { SafeArrayDestroy(firstCellRowHeadersArray); });
    Require(ReadProviderArrayNames(firstCellRowHeadersArray, "grid cell table-item row headers return an empty array").empty(),
            "grid cell table-item pattern reports no row-header fragments for row-headerless grids");

    wil::com_ptr_nothrow<IRawElementProviderFragment> secondCellProvider;
    RequireSucceeded(firstCellProvider->Navigate(NavigateDirection_NextSibling, secondCellProvider.put()),
                     "grid cell accessibility provider navigates to the next visible cell");
    Require(secondCellProvider != nullptr, "grid cell accessibility provider returns the next visible cell");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondCellSimple;
    RequireSucceeded(secondCellProvider.query_to(secondCellSimple.put()), "second grid cell accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*secondCellSimple.get(), UIA_NamePropertyId, "second grid cell exposes accessibility name") == L"Ready",
            "grid cell navigation reaches the expected second visible cell");

    wil::com_ptr_nothrow<IRawElementProviderFragment> previousCellProvider;
    RequireSucceeded(secondCellProvider->Navigate(NavigateDirection_PreviousSibling, previousCellProvider.put()),
                     "second grid cell accessibility provider navigates back to the previous visible cell");
    Require(previousCellProvider != nullptr, "grid cell accessibility provider returns the previous visible cell");
    wil::com_ptr_nothrow<IRawElementProviderSimple> previousCellSimple;
    RequireSucceeded(previousCellProvider.query_to(previousCellSimple.put()), "previous grid cell accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*previousCellSimple.get(), UIA_NamePropertyId, "previous grid cell exposes accessibility name") == L"Alpha",
            "grid cell previous-sibling navigation returns to the first cell");

    wil::com_ptr_nothrow<IRawElementProviderFragment> parentRowFromCell;
    RequireSucceeded(firstCellProvider->Navigate(NavigateDirection_Parent, parentRowFromCell.put()),
                     "grid cell accessibility provider navigates back to its row");
    Require(parentRowFromCell != nullptr, "grid cell accessibility provider returns its parent row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> parentRowSimple;
    RequireSucceeded(parentRowFromCell.query_to(parentRowSimple.put()), "parent row provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*parentRowSimple.get(), UIA_NamePropertyId, "parent row provider exposes accessibility name") == L"Alpha | Ready",
            "grid cell parent navigation returns the owning row provider");

    wil::com_ptr_nothrow<IUnknown> firstSelectionUnknown;
    RequireSucceeded(firstRowSimple->GetPatternProvider(UIA_SelectionItemPatternId, firstSelectionUnknown.put()), "grid row selection pattern lookup succeeds");
    Require(firstSelectionUnknown != nullptr, "grid row accessibility provider exposes selection-item pattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> firstSelectionPattern;
    RequireSucceeded(firstSelectionUnknown.query_to(firstSelectionPattern.put()), "grid row selection pattern supports ISelectionItemProvider");

    wil::com_ptr_nothrow<IRawElementProviderSimple> firstSelectionContainer;
    RequireSucceeded(firstSelectionPattern->get_SelectionContainer(firstSelectionContainer.put()), "grid row selection container lookup succeeds");
    Require(firstSelectionContainer != nullptr, "grid row selection pattern exposes the grid container");
    Require(ReadProviderStringProperty(*firstSelectionContainer.get(), UIA_NamePropertyId, "grid selection container exposes accessibility name") == L"Results",
            "grid row selection container resolves to the labeled grid host");
    wil::com_ptr_nothrow<IUnknown> gridSelectionContainerUnknown;
    RequireSucceeded(firstSelectionContainer->GetPatternProvider(UIA_SelectionPatternId, gridSelectionContainerUnknown.put()),
                     "grid selection container selection-pattern lookup succeeds");
    Require(gridSelectionContainerUnknown != nullptr, "grid selection container exposes the selection pattern");
    wil::com_ptr_nothrow<ISelectionProvider> gridSelectionProvider;
    RequireSucceeded(gridSelectionContainerUnknown.query_to(gridSelectionProvider.put()), "grid selection container pattern supports ISelectionProvider");
    BOOL canSelectMultiple = FALSE;
    RequireSucceeded(gridSelectionProvider->get_CanSelectMultiple(&canSelectMultiple), "grid selection provider reports multi-select capability");
    Require(canSelectMultiple == TRUE, "grid selection provider reports extended multi-selection behavior");

    RequireSucceeded(firstSelectionPattern->Select(), "grid row selection pattern can select the first row");
    Require(grid->IsRowSelected(0u), "grid row selection pattern selects the first row");
    Require(delegate.selectionChangedCount == 1u && delegate.selectionCounts == std::vector<size_t>{1u} &&
                delegate.orderedSelection == std::vector<uint64_t>{100u},
            "grid row selection pattern uses the shared delegate-driven selection path");
    Require(ReadProviderBoolProperty(*firstRowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "grid row selected state updates after Select"),
            "grid row accessibility provider reports the selected row after Select");
    Require(ReadSelectionProviderNames(*gridSelectionProvider.get(), "grid selection provider returns selected row names") ==
                std::vector<std::wstring>{L"Alpha | Ready"},
            "grid selection provider resolves the currently selected visible row");

    const std::optional<D2D1_RECT_F> firstCellRect = grid->GetVisibleCellRect(0u, 0u);
    Require(firstCellRect.has_value(), "grid exposes a visible cell rect for point hit-testing");
    const float firstCellCenterXDip                                   = (firstCellRect->left + firstCellRect->right) * 0.5f;
    const float firstCellCenterYDip                                   = (firstCellRect->top + firstCellRect->bottom) * 0.5f;
    wil::com_ptr_nothrow<IRawElementProviderFragment> hitCellProvider = GetProviderAtDipPoint(
        window.Hwnd(), window.Host(), *rootProvider.get(), firstCellCenterXDip, firstCellCenterYDip, "grid cell accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> hitCellSimple;
    RequireSucceeded(hitCellProvider.query_to(hitCellSimple.put()), "grid point-hit provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*hitCellSimple.get(), UIA_ControlTypePropertyId, "grid point-hit provider exposes cell control type") ==
                UIA_TextControlTypeId,
            "grid hit-testing resolves the visible cell provider instead of only the row or grid container");
    Require(ReadProviderStringProperty(*hitCellSimple.get(), UIA_NamePropertyId, "grid point-hit provider exposes cell name") == L"Alpha",
            "grid hit-testing resolves the expected visible cell provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> secondRowProvider;
    RequireSucceeded(firstRowProvider->Navigate(NavigateDirection_NextSibling, secondRowProvider.put()),
                     "first grid row provider navigates to the next visible row");
    Require(secondRowProvider != nullptr, "first grid row provider returns the next sibling row");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondRowSimple;
    RequireSucceeded(secondRowProvider.query_to(secondRowSimple.put()), "second grid row provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*secondRowSimple.get(), UIA_NamePropertyId, "second grid row exposes accessibility name") == L"Beta | Busy",
            "grid row navigation reaches the expected second row");

    wil::com_ptr_nothrow<IUnknown> secondSelectionUnknown;
    RequireSucceeded(secondRowSimple->GetPatternProvider(UIA_SelectionItemPatternId, secondSelectionUnknown.put()),
                     "second grid row selection pattern lookup succeeds");
    Require(secondSelectionUnknown != nullptr, "second grid row exposes selection-item pattern");
    wil::com_ptr_nothrow<ISelectionItemProvider> secondSelectionPattern;
    RequireSucceeded(secondSelectionUnknown.query_to(secondSelectionPattern.put()), "second grid row selection pattern supports ISelectionItemProvider");

    RequireSucceeded(secondSelectionPattern->AddToSelection(), "grid row selection pattern can extend the selection");
    Require(grid->IsRowSelected(0u) && grid->IsRowSelected(1u), "grid row AddToSelection preserves the first row and adds the second");
    Require(delegate.selectionChangedCount == 2u && delegate.selectionCounts == std::vector<size_t>({1u, 2u}) &&
                delegate.orderedSelection == std::vector<uint64_t>({100u, 200u}),
            "grid row AddToSelection continues to use the shared delegate path");
    Require(ReadProviderBoolProperty(*secondRowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "second grid row exposes selected state"),
            "second grid row provider reports selection after AddToSelection");
    Require(ReadSelectionProviderNames(*gridSelectionProvider.get(), "grid selection provider updates after AddToSelection") ==
                std::vector<std::wstring>({L"Alpha | Ready", L"Beta | Busy"}),
            "grid selection provider tracks the ordered visible row selection");

    window.Host().SetFocusControl(grid);
    wil::com_ptr_nothrow<IRawElementProviderFragment> focusedProvider;
    RequireSucceeded(rootProvider->GetFocus(focusedProvider.put()), "root provider focus lookup succeeds for the grid");
    Require(focusedProvider != nullptr, "root provider returns the focused grid row provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> focusedSimple;
    RequireSucceeded(focusedProvider.query_to(focusedSimple.put()), "focused grid row provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*focusedSimple.get(), UIA_ControlTypePropertyId, "focused grid row exposes UIA control type") == UIA_DataItemControlTypeId,
            "root provider focus lookup returns the selected grid row provider for a focused grid");
    Require(ReadProviderStringProperty(*focusedSimple.get(), UIA_NamePropertyId, "focused grid row exposes accessibility name") == L"Beta | Busy",
            "root provider focus lookup returns the most recently selected visible grid row");

    RequireSucceeded(secondSelectionPattern->RemoveFromSelection(), "grid row selection pattern can remove the second row from the selection");
    Require(grid->IsRowSelected(0u) && ! grid->IsRowSelected(1u), "grid row RemoveFromSelection preserves the remaining visible selection");
    Require(delegate.selectionChangedCount == 3u && delegate.selectionCounts == std::vector<size_t>({1u, 2u, 1u}) &&
                delegate.orderedSelection == std::vector<uint64_t>({100u}),
            "grid row RemoveFromSelection continues to use the shared delegate path");
    Require(! ReadProviderBoolProperty(*secondRowSimple.get(), UIA_SelectionItemIsSelectedPropertyId, "grid row selected state updates after removal"),
            "grid row accessibility provider reports deselection after RemoveFromSelection");
    Require(ReadSelectionProviderNames(*gridSelectionProvider.get(), "grid selection provider updates after removal") ==
                std::vector<std::wstring>{L"Alpha | Ready"},
            "grid selection provider drops the removed row and preserves the remaining selection");
}

void TestAccessibilityProviderExposesHorizontallyScrolledGridRowStructure()
{
    using namespace DxUi;

    class WideGridModel final : public IGridModel
    {
    public:
        [[nodiscard]] size_t GetRowCount() const noexcept override
        {
            return 1u;
        }

        [[nodiscard]] size_t GetColumnCount() const noexcept override
        {
            return 3u;
        }

        [[nodiscard]] GridColumnDesc GetColumn(size_t columnIndex) const override
        {
            GridColumnDesc column;
            switch (columnIndex)
            {
                case 0u:
                    column.id    = L"name";
                    column.title = L"Name";
                    break;
                case 1u:
                    column.id    = L"status";
                    column.title = L"Status";
                    break;
                default:
                    column.id    = L"state";
                    column.title = L"State";
                    break;
            }
            column.widthDip = 120.0f;
            return column;
        }

        void GetCellData(size_t /*rowIndex*/, size_t columnIndex, GridCellData& outCell) const override
        {
            outCell.kind = GridCellKind::Text;
            switch (columnIndex)
            {
                case 0u: outCell.text = L"Alpha"; break;
                case 1u: outCell.text = L"Ready"; break;
                default: outCell.text = L"Archived"; break;
            }
        }

        [[nodiscard]] uint64_t GetStableRowId(size_t /*rowIndex*/) const noexcept override
        {
            return 100u;
        }

        [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
        {
            return (rowId == 100u) ? std::optional<size_t>{0u} : std::nullopt;
        }
    };

    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* gridLabel = root->AddChild<Label>(L"Results");
    gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 190.0f, 128.0f));

    WideGridModel gridModel;
    grid->SetModel(&gridModel);
    grid->DebugSetScrollOffsets(0.0f, 130.0f);
    gridLabel->SetMnemonicTarget(grid);

    Require(! grid->GetVisibleCellRect(0u, 0u).has_value(), "scrolled grid keeps the first column outside the visible cell viewport");
    Require(grid->GetVisibleCellRect(0u, 1u).has_value(), "scrolled grid keeps later columns visible for point-hit comparison");

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "horizontally scrolled grid accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> gridLabelProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 40.0f, 12.0f, "scrolled grid label provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> gridProvider;
    RequireSucceeded(gridLabelProvider->Navigate(NavigateDirection_NextSibling, gridProvider.put()),
                     "scrolled grid label accessibility provider navigates to the grid");
    Require(gridProvider != nullptr, "scrolled grid label provider returns the grid as the next sibling");

    wil::com_ptr_nothrow<IRawElementProviderFragment> childProvider;
    RequireSucceeded(gridProvider->Navigate(NavigateDirection_FirstChild, childProvider.put()),
                     "scrolled grid provider navigates to its first structural child");
    Require(childProvider != nullptr, "scrolled grid provider exposes at least one structural child");

    wil::com_ptr_nothrow<IRawElementProviderFragment> rowProvider;
    for (size_t step = 0u; childProvider && step < 8u; ++step)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> childSimple;
        RequireSucceeded(childProvider.query_to(childSimple.put()), "scrolled grid child exposes IRawElementProviderSimple");
        if (ReadProviderLongProperty(*childSimple.get(), UIA_ControlTypePropertyId, "scrolled grid child exposes UIA control type") ==
            UIA_DataItemControlTypeId)
        {
            rowProvider = childProvider;
            break;
        }

        wil::com_ptr_nothrow<IRawElementProviderFragment> nextProvider;
        RequireSucceeded(childProvider->Navigate(NavigateDirection_NextSibling, nextProvider.put()),
                         "scrolled grid child navigates to the next structural sibling");
        childProvider = nextProvider;
    }

    Require(rowProvider != nullptr, "scrolled grid structure exposes a row provider after visible headers");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rowSimple;
    RequireSucceeded(rowProvider.query_to(rowSimple.put()), "scrolled grid row provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*rowSimple.get(), UIA_NamePropertyId, "scrolled grid row exposes accessibility name") == L"Alpha | Ready | Archived",
            "scrolled grid row name includes all model columns, including horizontally off-view cells");

    wil::com_ptr_nothrow<IRawElementProviderFragment> firstCellProvider;
    RequireSucceeded(rowProvider->Navigate(NavigateDirection_FirstChild, firstCellProvider.put()),
                     "scrolled grid row provider navigates to its first structural cell");
    Require(firstCellProvider != nullptr, "scrolled grid row exposes a first structural cell");
    wil::com_ptr_nothrow<IRawElementProviderSimple> firstCellSimple;
    RequireSucceeded(firstCellProvider.query_to(firstCellSimple.put()), "scrolled grid first cell provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*firstCellSimple.get(), UIA_NamePropertyId, "scrolled grid first cell exposes accessibility name") == L"Alpha",
            "scrolled grid first row child is the horizontally off-view first model column");
    Require(ReadProviderBoolProperty(*firstCellSimple.get(), UIA_IsOffscreenPropertyId, "scrolled grid first cell exposes offscreen state"),
            "scrolled grid off-view cell fragment reports itself offscreen");

    wil::com_ptr_nothrow<IRawElementProviderFragment> secondCellProvider;
    RequireSucceeded(firstCellProvider->Navigate(NavigateDirection_NextSibling, secondCellProvider.put()),
                     "scrolled grid first cell navigates to the second structural cell");
    Require(secondCellProvider != nullptr, "scrolled grid first cell returns the next model-column cell");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondCellSimple;
    RequireSucceeded(secondCellProvider.query_to(secondCellSimple.put()), "scrolled grid second cell provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*secondCellSimple.get(), UIA_NamePropertyId, "scrolled grid second cell exposes accessibility name") == L"Ready",
            "scrolled grid cell navigation continues through the full model column set");
}

void TestAccessibilityProviderPointHitsClipAndTranslateScrollPanelChildren()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));
    scroll->SetContentHeight(260.0f);
    scroll->SetScrollOffset(80.0f);

    auto* offscreenButton = scroll->AddChild<Button>(L"Hidden above");
    offscreenButton->SetBounds(D2D1::RectF(12.0f, 12.0f, 180.0f, 48.0f));
    auto* visibleButton = scroll->AddChild<Button>(L"Visible after scroll");
    visibleButton->SetBounds(D2D1::RectF(12.0f, 112.0f, 180.0f, 148.0f));

    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "scrolled panel point-hit test creates a root provider");

    const POINT rawContentSpacePointPx = window.Host().DipPointToScreenPoint(D2D1::Point2F(24.0f, 24.0f));
    wil::com_ptr_nothrow<IRawElementProviderFragment> rawContentSpaceProvider;
    RequireSucceeded(rootProvider->ElementProviderFromPoint(
                         static_cast<double>(rawContentSpacePointPx.x), static_cast<double>(rawContentSpacePointPx.y), rawContentSpaceProvider.put()),
                     "scrolled panel raw content-space point query succeeds");
    Require(rawContentSpaceProvider != nullptr, "scrolled panel empty viewport point resolves the root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rawContentSpaceRoot;
    RequireSucceeded(rawContentSpaceProvider.query_to(rawContentSpaceRoot.put()), "scrolled panel empty viewport provider is the root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> visibleProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 24.0f, 44.0f, "scrolled panel visible child point is queryable");
    Require(visibleProvider != nullptr, "scrolled ScrollPanel child is hit-testable at its viewport-translated position");
    wil::com_ptr_nothrow<IRawElementProviderSimple> visibleSimple;
    RequireSucceeded(visibleProvider.query_to(visibleSimple.put()), "scrolled panel visible provider exposes IRawElementProviderSimple");
    Require(ReadProviderStringProperty(*visibleSimple.get(), UIA_NamePropertyId, "scrolled panel visible provider exposes accessibility name") ==
                L"Visible after scroll",
            "scrolled panel point-hit translation resolves the visible child rather than its raw content-space position");
}

void TestAccessibilityProviderExposesGridCellToggleAndRangePatterns()
{
    using namespace DxUi;

    {
        AttachedHostWindow window;
        auto root       = std::make_unique<Panel>();
        auto* gridLabel = root->AddChild<Label>(L"Rules");
        gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 340.0f, 128.0f));

        CheckboxGridModel gridModel(0u);
        gridModel.SetRows({CheckboxGridModel::Row{L"Rule A", true, true}});
        RecordingCheckboxGridDelegate delegate(gridModel);
        grid->SetModel(&gridModel);
        grid->SetDelegate(&delegate);
        gridLabel->SetMnemonicTarget(grid);

        window.Host().SetRoot(std::move(root));

        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "grid checkbox accessibility test creates a root provider");

        const std::optional<D2D1_RECT_F> checkboxCellRect = grid->GetVisibleCellRect(0u, 0u);
        Require(checkboxCellRect.has_value(), "grid exposes a visible checkbox cell rect for accessibility hit-testing");
        const float checkboxCellCenterXDip = (checkboxCellRect->left + checkboxCellRect->right) * 0.5f;
        const float checkboxCellCenterYDip = (checkboxCellRect->top + checkboxCellRect->bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> checkboxCellProvider =
            GetProviderAtDipPoint(window.Hwnd(),
                                  window.Host(),
                                  *rootProvider.get(),
                                  checkboxCellCenterXDip,
                                  checkboxCellCenterYDip,
                                  "grid checkbox cell accessibility provider is resolved by point");
        wil::com_ptr_nothrow<IRawElementProviderSimple> checkboxCellSimple;
        RequireSucceeded(checkboxCellProvider.query_to(checkboxCellSimple.put()),
                         "grid checkbox cell accessibility provider exposes IRawElementProviderSimple");
        Require(ReadProviderLongProperty(*checkboxCellSimple.get(), UIA_ControlTypePropertyId, "grid checkbox cell exposes UIA control type") ==
                    UIA_CheckBoxControlTypeId,
                "grid checkbox cell accessibility provider reports checkbox control type");
        Require(ReadProviderStringProperty(*checkboxCellSimple.get(), UIA_NamePropertyId, "grid checkbox cell exposes accessibility name") == L"[x] Enabled",
                "grid checkbox cell accessibility provider exposes the checked cell text");
        Require(ReadProviderLongProperty(*checkboxCellSimple.get(), UIA_ToggleToggleStatePropertyId, "grid checkbox cell exposes toggle state") ==
                    ToggleState_On,
                "grid checkbox cell accessibility provider reports the checked toggle state");

        wil::com_ptr_nothrow<IUnknown> checkboxToggleUnknown;
        RequireSucceeded(checkboxCellSimple->GetPatternProvider(UIA_TogglePatternId, checkboxToggleUnknown.put()),
                         "grid checkbox cell toggle-pattern lookup succeeds");
        Require(checkboxToggleUnknown != nullptr, "grid checkbox cell accessibility provider exposes the toggle pattern");
        wil::com_ptr_nothrow<IToggleProvider> checkboxTogglePattern;
        RequireSucceeded(checkboxToggleUnknown.query_to(checkboxTogglePattern.put()), "grid checkbox cell toggle pattern supports IToggleProvider");

        ToggleState toggleState = ToggleState_Off;
        RequireSucceeded(checkboxTogglePattern->get_ToggleState(&toggleState), "grid checkbox cell toggle-state lookup succeeds");
        Require(toggleState == ToggleState_On, "grid checkbox cell toggle pattern reports the initial checked state");

        RequireSucceeded(checkboxTogglePattern->Toggle(), "grid checkbox cell toggle pattern can toggle the visible checkbox");
        Require(delegate.toggleCount == 1u && delegate.lastToggleRow == 0u && delegate.lastToggleColumn == 0u && ! delegate.lastToggleChecked,
                "grid checkbox cell toggle pattern routes through the shared delegate checkbox path");
        Require(! gridModel.IsChecked(0u), "grid checkbox cell toggle pattern updates the backing model state");
        Require(ReadProviderLongProperty(*checkboxCellSimple.get(), UIA_ToggleToggleStatePropertyId, "grid checkbox cell toggle state updates after Toggle") ==
                    ToggleState_Off,
                "grid checkbox cell accessibility provider reports the toggled unchecked state");
    }

    {
        AttachedHostWindow window;
        auto root       = std::make_unique<Panel>();
        auto* gridLabel = root->AddChild<Label>(L"Rules");
        gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 340.0f, 128.0f));

        CheckboxGridModel gridModel(0u);
        gridModel.SetRows({CheckboxGridModel::Row{L"Rule A", false, false}});
        RecordingCheckboxGridDelegate delegate(gridModel);
        grid->SetModel(&gridModel);
        grid->SetDelegate(&delegate);
        gridLabel->SetMnemonicTarget(grid);

        window.Host().SetRoot(std::move(root));

        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "disabled grid checkbox accessibility test creates a root provider");

        const std::optional<D2D1_RECT_F> checkboxCellRect = grid->GetVisibleCellRect(0u, 0u);
        Require(checkboxCellRect.has_value(), "disabled grid checkbox exposes a visible cell rect for accessibility hit-testing");
        const float checkboxCellCenterXDip = (checkboxCellRect->left + checkboxCellRect->right) * 0.5f;
        const float checkboxCellCenterYDip = (checkboxCellRect->top + checkboxCellRect->bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> checkboxCellProvider =
            GetProviderAtDipPoint(window.Hwnd(),
                                  window.Host(),
                                  *rootProvider.get(),
                                  checkboxCellCenterXDip,
                                  checkboxCellCenterYDip,
                                  "disabled grid checkbox cell accessibility provider is resolved by point");
        wil::com_ptr_nothrow<IRawElementProviderSimple> checkboxCellSimple;
        RequireSucceeded(checkboxCellProvider.query_to(checkboxCellSimple.put()),
                         "disabled grid checkbox cell accessibility provider exposes IRawElementProviderSimple");
        Require(ReadProviderLongProperty(*checkboxCellSimple.get(), UIA_ControlTypePropertyId, "disabled grid checkbox cell exposes UIA control type") ==
                    UIA_CheckBoxControlTypeId,
                "disabled grid checkbox cell accessibility provider reports checkbox control type");
        Require(ReadProviderStringProperty(*checkboxCellSimple.get(), UIA_NamePropertyId, "disabled grid checkbox cell exposes accessibility name") ==
                    L"[ ] Enabled",
                "disabled grid checkbox cell accessibility provider exposes the unchecked cell text");
        Require(! ReadProviderBoolProperty(*checkboxCellSimple.get(), UIA_IsEnabledPropertyId, "disabled grid checkbox cell exposes disabled state"),
                "disabled grid checkbox cell accessibility provider reports disabled state");

        wil::com_ptr_nothrow<IUnknown> checkboxToggleUnknown;
        RequireSucceeded(checkboxCellSimple->GetPatternProvider(UIA_TogglePatternId, checkboxToggleUnknown.put()),
                         "disabled grid checkbox cell toggle-pattern lookup succeeds");
        Require(checkboxToggleUnknown == nullptr, "disabled grid checkbox cell accessibility provider does not expose the toggle pattern");
        Require(delegate.toggleCount == 0u && ! gridModel.IsChecked(0u),
                "disabled grid checkbox cell accessibility provider leaves the backing checkbox state unchanged");
    }

    {
        AttachedHostWindow window;
        auto root       = std::make_unique<Panel>();
        auto* gridLabel = root->AddChild<Label>(L"Plugins");
        gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 340.0f, 128.0f));

        GridCellData pluginCell{};
        pluginCell.kind        = GridCellKind::IconText;
        pluginCell.iconText    = L"*";
        pluginCell.text        = L"Plugin";
        pluginCell.badgeText   = L"Beta";
        pluginCell.tooltipText = L"Plugin is disabled by policy.";
        SingleCellGridModel pluginModel(std::move(pluginCell));
        grid->SetModel(&pluginModel);
        gridLabel->SetMnemonicTarget(grid);

        window.Host().SetRoot(std::move(root));

        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "grid infotip accessibility test creates a root provider");

        const std::optional<D2D1_RECT_F> pluginCellRect = grid->GetVisibleCellRect(0u, 0u);
        Require(pluginCellRect.has_value(), "grid exposes a visible infotip cell rect for accessibility hit-testing");
        const float pluginCellCenterXDip = (pluginCellRect->left + pluginCellRect->right) * 0.5f;
        const float pluginCellCenterYDip = (pluginCellRect->top + pluginCellRect->bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> pluginCellProvider =
            GetProviderAtDipPoint(window.Hwnd(),
                                  window.Host(),
                                  *rootProvider.get(),
                                  pluginCellCenterXDip,
                                  pluginCellCenterYDip,
                                  "grid infotip cell accessibility provider is resolved by point");
        wil::com_ptr_nothrow<IRawElementProviderSimple> pluginCellSimple;
        RequireSucceeded(pluginCellProvider.query_to(pluginCellSimple.put()), "grid infotip cell accessibility provider exposes IRawElementProviderSimple");
        Require(ReadProviderStringProperty(*pluginCellSimple.get(), UIA_NamePropertyId, "grid infotip cell exposes accessibility name") == L"Plugin [Beta]",
                "grid infotip cell accessibility provider keeps icon and badge text in the accessible name");
        Require(ReadProviderStringProperty(*pluginCellSimple.get(), UIA_HelpTextPropertyId, "grid infotip cell exposes help text") ==
                    L"Plugin is disabled by policy.",
                "grid infotip cell accessibility provider exposes explicit tooltip text as UIA HelpText");
    }

    {
        AttachedHostWindow window;
        auto root       = std::make_unique<Panel>();
        auto* gridLabel = root->AddChild<Label>(L"States");
        gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 340.0f, 128.0f));

        StateImageColumnGridModel stateImageModel;
        grid->SetModel(&stateImageModel);
        gridLabel->SetMnemonicTarget(grid);

        window.Host().SetRoot(std::move(root));

        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "grid state-image accessibility test creates a root provider");

        const std::optional<D2D1_RECT_F> stateImageCellRect = grid->GetVisibleCellRect(0u, 0u);
        Require(stateImageCellRect.has_value(), "grid exposes a visible state-image cell rect for accessibility hit-testing");
        const float stateImageCellCenterXDip = (stateImageCellRect->left + stateImageCellRect->right) * 0.5f;
        const float stateImageCellCenterYDip = (stateImageCellRect->top + stateImageCellRect->bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> stateImageCellProvider =
            GetProviderAtDipPoint(window.Hwnd(),
                                  window.Host(),
                                  *rootProvider.get(),
                                  stateImageCellCenterXDip,
                                  stateImageCellCenterYDip,
                                  "grid state-image cell accessibility provider is resolved by point");
        wil::com_ptr_nothrow<IRawElementProviderSimple> stateImageCellSimple;
        RequireSucceeded(stateImageCellProvider.query_to(stateImageCellSimple.put()),
                         "grid state-image cell accessibility provider exposes IRawElementProviderSimple");
        Require(ReadProviderLongProperty(*stateImageCellSimple.get(), UIA_ControlTypePropertyId, "grid state-image cell exposes UIA control type") ==
                    UIA_ImageControlTypeId,
                "grid state-image cell accessibility provider reports image control type");
        Require(ReadProviderStringProperty(*stateImageCellSimple.get(), UIA_NamePropertyId, "grid state-image cell exposes accessibility name") == L"!",
                "grid state-image cell accessibility provider keeps the icon glyph as its accessible name");

        wil::com_ptr_nothrow<IUnknown> stateImageValueUnknown;
        RequireSucceeded(stateImageCellSimple->GetPatternProvider(UIA_ValuePatternId, stateImageValueUnknown.put()),
                         "grid state-image cell value-pattern lookup succeeds");
        Require(stateImageValueUnknown == nullptr, "grid state-image cell accessibility provider does not expose a text value pattern");
    }

    {
        AttachedHostWindow window;
        auto root       = std::make_unique<Panel>();
        auto* gridLabel = root->AddChild<Label>(L"Jobs");
        gridLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 28.0f, 340.0f, 128.0f));

        GridCellData progressCell{};
        progressCell.kind     = GridCellKind::Marquee;
        progressCell.text     = L"Halfway";
        progressCell.progress = 0.5f;
        SingleCellGridModel progressModel(std::move(progressCell));
        grid->SetModel(&progressModel);
        gridLabel->SetMnemonicTarget(grid);

        window.Host().SetRoot(std::move(root));

        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "grid progress accessibility test creates a root provider");

        const std::optional<D2D1_RECT_F> progressCellRect = grid->GetVisibleCellRect(0u, 0u);
        Require(progressCellRect.has_value(), "grid exposes a visible progress cell rect for accessibility hit-testing");
        const float progressCellCenterXDip = (progressCellRect->left + progressCellRect->right) * 0.5f;
        const float progressCellCenterYDip = (progressCellRect->top + progressCellRect->bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> progressCellProvider =
            GetProviderAtDipPoint(window.Hwnd(),
                                  window.Host(),
                                  *rootProvider.get(),
                                  progressCellCenterXDip,
                                  progressCellCenterYDip,
                                  "grid progress cell accessibility provider is resolved by point");
        wil::com_ptr_nothrow<IRawElementProviderSimple> progressCellSimple;
        RequireSucceeded(progressCellProvider.query_to(progressCellSimple.put()),
                         "grid progress cell accessibility provider exposes IRawElementProviderSimple");
        Require(ReadProviderLongProperty(*progressCellSimple.get(), UIA_ControlTypePropertyId, "grid progress cell exposes UIA control type") ==
                    UIA_ProgressBarControlTypeId,
                "grid progress cell accessibility provider reports progress-bar control type");
        Require(ReadProviderStringProperty(*progressCellSimple.get(), UIA_NamePropertyId, "grid progress cell exposes accessibility name") == L"Halfway",
                "grid progress cell accessibility provider exposes the determinate progress label");
        Require(ReadProviderBoolProperty(*progressCellSimple.get(), UIA_ValueIsReadOnlyPropertyId, "grid progress cell exposes read-only state"),
                "grid progress cell accessibility provider reports read-only range semantics");

        wil::com_ptr_nothrow<IUnknown> rangeValueUnknown;
        RequireSucceeded(progressCellSimple->GetPatternProvider(UIA_RangeValuePatternId, rangeValueUnknown.put()),
                         "grid progress cell range-value lookup succeeds");
        Require(rangeValueUnknown != nullptr, "grid progress cell accessibility provider exposes the range-value pattern");
        wil::com_ptr_nothrow<IRangeValueProvider> rangeValuePattern;
        RequireSucceeded(rangeValueUnknown.query_to(rangeValuePattern.put()), "grid progress cell range-value pattern supports IRangeValueProvider");

        double rangeValue       = 0.0;
        double rangeMinimum     = 0.0;
        double rangeMaximum     = 0.0;
        double rangeSmallChange = 1.0;
        double rangeLargeChange = 1.0;
        BOOL rangeReadOnly      = FALSE;
        RequireSucceeded(rangeValuePattern->get_Value(&rangeValue), "grid progress cell range-value query succeeds");
        RequireSucceeded(rangeValuePattern->get_Minimum(&rangeMinimum), "grid progress cell minimum query succeeds");
        RequireSucceeded(rangeValuePattern->get_Maximum(&rangeMaximum), "grid progress cell maximum query succeeds");
        RequireSucceeded(rangeValuePattern->get_SmallChange(&rangeSmallChange), "grid progress cell small-change query succeeds");
        RequireSucceeded(rangeValuePattern->get_LargeChange(&rangeLargeChange), "grid progress cell large-change query succeeds");
        RequireSucceeded(rangeValuePattern->get_IsReadOnly(&rangeReadOnly), "grid progress cell range read-only query succeeds");
        Require(rangeValue == 0.5 && rangeMinimum == 0.0 && rangeMaximum == 1.0,
                "grid progress cell range-value pattern reports the determinate 0..1 progress value");
        Require(rangeSmallChange == 0.0 && rangeLargeChange == 0.0 && rangeReadOnly == TRUE,
                "grid progress cell range-value pattern reports a read-only non-adjustable progress range");
    }
}

void TestAccessibilityProviderExposesSliderRangeValuePattern()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root         = std::make_unique<Panel>();
    auto* sliderLabel = root->AddChild<Label>(L"Opacity");
    sliderLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 32.0f, 240.0f, 64.0f));
    slider->SetMinimum(10.0);
    slider->SetMaximum(90.0);
    slider->SetValue(42.0);
    slider->SetStep(2.0);
    slider->SetLargeStep(10.0);
    sliderLabel->SetMnemonicTarget(slider);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "slider accessibility test creates a root provider");

    wil::com_ptr_nothrow<IRawElementProviderFragment> sliderProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 120.0f, 48.0f, "slider accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> sliderSimple;
    RequireSucceeded(sliderProvider.query_to(sliderSimple.put()), "slider accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*sliderSimple.get(), UIA_ControlTypePropertyId, "slider exposes UIA control type") == UIA_SliderControlTypeId,
            "slider accessibility provider reports slider control type");
    Require(ReadProviderStringProperty(*sliderSimple.get(), UIA_NamePropertyId, "slider exposes accessibility name") == L"Opacity",
            "slider accessibility provider uses its associated label as the accessible name");
    Require(! ReadProviderBoolProperty(*sliderSimple.get(), UIA_ValueIsReadOnlyPropertyId, "slider exposes writable range state"),
            "slider accessibility provider reports an adjustable range value");

    wil::com_ptr_nothrow<IUnknown> rangeValueUnknown;
    RequireSucceeded(sliderSimple->GetPatternProvider(UIA_RangeValuePatternId, rangeValueUnknown.put()), "slider range-value lookup succeeds");
    Require(rangeValueUnknown != nullptr, "slider accessibility provider exposes the range-value pattern");
    wil::com_ptr_nothrow<IRangeValueProvider> rangeValuePattern;
    RequireSucceeded(rangeValueUnknown.query_to(rangeValuePattern.put()), "slider range-value pattern supports IRangeValueProvider");

    double rangeValue       = 0.0;
    double rangeMinimum     = 0.0;
    double rangeMaximum     = 0.0;
    double rangeSmallChange = 0.0;
    double rangeLargeChange = 0.0;
    BOOL rangeReadOnly      = TRUE;
    RequireSucceeded(rangeValuePattern->get_Value(&rangeValue), "slider range-value query succeeds");
    RequireSucceeded(rangeValuePattern->get_Minimum(&rangeMinimum), "slider minimum query succeeds");
    RequireSucceeded(rangeValuePattern->get_Maximum(&rangeMaximum), "slider maximum query succeeds");
    RequireSucceeded(rangeValuePattern->get_SmallChange(&rangeSmallChange), "slider small-change query succeeds");
    RequireSucceeded(rangeValuePattern->get_LargeChange(&rangeLargeChange), "slider large-change query succeeds");
    RequireSucceeded(rangeValuePattern->get_IsReadOnly(&rangeReadOnly), "slider read-only query succeeds");
    Require(rangeValue == 42.0 && rangeMinimum == 10.0 && rangeMaximum == 90.0, "slider range-value pattern reports the configured min/max/value");
    Require(rangeSmallChange == 2.0 && rangeLargeChange == 10.0 && rangeReadOnly == FALSE, "slider range-value pattern reports the configured step values");

    RequireSucceeded(rangeValuePattern->SetValue(68.0), "slider range-value SetValue succeeds");
    Require(slider->GetValue() == 68.0, "slider range-value SetValue updates the underlying control value");
}

void TestAccessibilityProviderExposesSplitterRangeValuePattern()
{
    using namespace DxUi;

    // A focusable, keyboard-adjustable splitter is a Thumb with RangeValue (Core-AAM's focusable separator): screen
    // readers reach it, speak its position and can move it.
    AttachedHostWindow window;
    auto root      = std::make_unique<Panel>();
    auto* splitter = root->AddChild<Splitter>();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    splitter->SetPosition(120.0f);
    splitter->SetAccessibleName(L"Layers panel width");
    size_t commits = 0u;
    splitter->SetOnChange([&commits](SplitterChange change) { commits += change.phase == SplitterChangePhase::Commit ? 1u : 0u; });
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "splitter accessibility test creates a root provider");
    wil::com_ptr_nothrow<IRawElementProviderFragment> splitterProvider =
        GetProviderAtDipPoint(window.Hwnd(), window.Host(), *rootProvider.get(), 123.0f, 60.0f, "splitter accessibility provider is resolved by point");
    wil::com_ptr_nothrow<IRawElementProviderSimple> splitterSimple;
    RequireSucceeded(splitterProvider.query_to(splitterSimple.put()), "splitter accessibility provider exposes IRawElementProviderSimple");
    Require(ReadProviderLongProperty(*splitterSimple.get(), UIA_ControlTypePropertyId, "splitter exposes UIA control type") == UIA_ThumbControlTypeId,
            "splitter reports the thumb control type of a focusable separator");
    Require(ReadProviderStringProperty(*splitterSimple.get(), UIA_NamePropertyId, "splitter exposes accessibility name") == L"Layers panel width",
            "splitter uses its accessible name");

    wil::com_ptr_nothrow<IUnknown> rangeValueUnknown;
    RequireSucceeded(splitterSimple->GetPatternProvider(UIA_RangeValuePatternId, rangeValueUnknown.put()), "splitter range-value lookup succeeds");
    Require(rangeValueUnknown != nullptr, "splitter exposes the range-value pattern");
    wil::com_ptr_nothrow<IRangeValueProvider> rangeValuePattern;
    RequireSucceeded(rangeValueUnknown.query_to(rangeValuePattern.put()), "splitter range-value pattern supports IRangeValueProvider");
    double value       = 0.0;
    double minimum     = 0.0;
    double maximum     = 0.0;
    double smallChange = 0.0;
    double largeChange = 0.0;
    BOOL readOnly      = TRUE;
    RequireSucceeded(rangeValuePattern->get_Value(&value), "splitter range-value query succeeds");
    RequireSucceeded(rangeValuePattern->get_Minimum(&minimum), "splitter minimum query succeeds");
    RequireSucceeded(rangeValuePattern->get_Maximum(&maximum), "splitter maximum query succeeds");
    RequireSucceeded(rangeValuePattern->get_SmallChange(&smallChange), "splitter small-change query succeeds");
    RequireSucceeded(rangeValuePattern->get_LargeChange(&largeChange), "splitter large-change query succeeds");
    RequireSucceeded(rangeValuePattern->get_IsReadOnly(&readOnly), "splitter read-only query succeeds");
    Require(value == 120.0 && minimum == 48.0 && maximum == 246.0, "splitter reports its position between the pane-minimum limits");
    Require(smallChange == 8.0 && largeChange == 32.0 && readOnly == FALSE, "splitter reports its keyboard steps and is adjustable");

    RequireSucceeded(rangeValuePattern->SetValue(200.0), "splitter range-value SetValue succeeds");
    Require(splitter->GetPosition() == 200.0f && commits == 1u, "splitter range-value SetValue commits one position change");
}

void TestAccessibilityStatusRootExposesChildrenAndNonFocusingInvoke()
{
    using namespace DxUi;

    AttachedHostWindow window;
    uint32_t invokeCount = 0u;
    auto root            = std::make_unique<Panel>();
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    root->SetAccessibilityRole(AccessibilityRole::Status);
    root->SetAccessibleAutomationId(L"TransientStatus");
    root->SetAccessibleName(L"Theme changed to Dark");
    root->SetFocusable(false);
    root->SetAccessibleInvoke([&invokeCount](WindowHost&) { ++invokeCount; });
    auto* previous = root->AddChild<Label>(L"Light");
    previous->SetBounds(D2D1::RectF(16.0f, 16.0f, 120.0f, 40.0f));
    previous->SetAccessibleAutomationId(L"TransientStatus.Previous");
    auto* current = root->AddChild<Label>(L"Dark");
    current->SetBounds(D2D1::RectF(100.0f, 60.0f, 220.0f, 100.0f));
    current->SetAccessibleAutomationId(L"TransientStatus.Current");
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "status-root test creates a root provider");
    wil::com_ptr_nothrow<IRawElementProviderSimple> rootSimple;
    RequireSucceeded(rootProvider.query_to(rootSimple.put()), "status root exposes provider-simple");
    Require(ReadProviderLongProperty(*rootSimple.get(), UIA_ControlTypePropertyId, "status root exposes control type") == UIA_StatusBarControlTypeId,
            "explicit status root collapses into the WindowHost fragment root");
    Require(ReadProviderStringProperty(*rootSimple.get(), UIA_AutomationIdPropertyId, "status root exposes automation id") == L"TransientStatus",
            "status root retains its stable automation id");
    Require(! ReadProviderBoolProperty(*rootSimple.get(), UIA_IsKeyboardFocusablePropertyId, "status root exposes focusability"),
            "status root remains non-focusable");

    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "status root exposes fragment navigation");
    wil::com_ptr_nothrow<IRawElementProviderFragment> firstChild;
    RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, firstChild.put()), "status root first-child navigation succeeds");
    Require(firstChild != nullptr, "collapsed status root retains its semantic text children");
    wil::com_ptr_nothrow<IRawElementProviderSimple> firstChildSimple;
    RequireSucceeded(firstChild.query_to(firstChildSimple.put()), "status child exposes provider-simple");
    Require(ReadProviderStringProperty(*firstChildSimple.get(), UIA_AutomationIdPropertyId, "status child exposes automation id") ==
                L"TransientStatus.Previous",
            "status child retains its stable automation id");

    wil::com_ptr_nothrow<IUnknown> invokeUnknown;
    RequireSucceeded(rootSimple->GetPatternProvider(UIA_InvokePatternId, invokeUnknown.put()), "status root Invoke lookup succeeds");
    Require(invokeUnknown != nullptr, "status root exposes Invoke");
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    RequireSucceeded(invokeUnknown.query_to(invoke.put()), "status root Invoke supports IInvokeProvider");
    RequireSucceeded(invoke->Invoke(), "status root Invoke succeeds");
    Require(invokeCount == 1u, "status root Invoke calls the non-focusing callback exactly once");
    Require(GetFocus() != window.Hwnd(), "status root Invoke does not focus its host window");
}

// Rebuilding a list puts a different control where an element's control was. The old element must neither act on the
// replacement nor share its runtime id, while a fresh element reaches it, in a window host as in an embedded one.
void TestWindowHostStaleElementCannotActOnAReplacementControl()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* list = root->AddChild<Panel>();
    list->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    int clicksA  = 0;
    int clicksC  = 0;
    auto* first  = list->AddChild<Button>(L"Section A");
    auto* second = list->AddChild<Button>(L"Section B");
    first->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    second->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    first->SetOnClick([&clicksA] { ++clicksA; });
    window.Host().SetRoot(std::move(root));

    const auto firstElement = [&](const char* context)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, context);
        wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
        RequireSucceeded(rootProvider.query_to(rootFragment.put()), context);
        wil::com_ptr_nothrow<IRawElementProviderFragment> element;
        const HRESULT navigated = rootFragment->Navigate(NavigateDirection_FirstChild, element.put());
        if (FAILED(navigated) || ! element)
            std::cerr << "    [UIA] first-child navigation hr=0x" << std::hex << static_cast<unsigned long>(navigated) << std::dec
                      << " element=" << (element != nullptr) << '\n';
        RequireSucceeded(navigated, context);
        Require(element != nullptr, context);
        return element;
    };
    const auto readRuntimeId = [](IRawElementProviderFragment& provider, const char* context)
    {
        SAFEARRAY* runtimeId = nullptr;
        RequireSucceeded(provider.GetRuntimeId(&runtimeId), context);
        const auto destroyRuntimeId = wil::scope_exit([&] { SafeArrayDestroy(runtimeId); });
        Require(runtimeId != nullptr, context);
        LONG lowerBound = 0;
        LONG upperBound = -1;
        RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lowerBound), context);
        RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upperBound), context);
        std::vector<LONG> values;
        for (LONG index = lowerBound; index <= upperBound; ++index)
        {
            LONG value = 0;
            RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &value), context);
            values.push_back(value);
        }
        return values;
    };
    const auto invokeOf = [](IRawElementProviderFragment& element, const char* context)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(element.QueryInterface(IID_PPV_ARGS(simple.put())), context);
        wil::com_ptr_nothrow<IUnknown> invokeUnknown;
        const HRESULT patternResult = simple->GetPatternProvider(UIA_InvokePatternId, invokeUnknown.put());
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        if (SUCCEEDED(patternResult) && invokeUnknown)
            RequireSucceeded(invokeUnknown.query_to(invoke.put()), context);
        return invoke;
    };

    const auto stale = firstElement("the first section's element is reachable");
    wil::com_ptr_nothrow<IRawElementProviderSimple> staleSimple;
    RequireSucceeded(stale.query_to(staleSimple.put()), "the first section's element is a simple provider");
    Require(ReadProviderStringProperty(*staleSimple.get(), UIA_NamePropertyId, "the first section's name is readable") == L"Section A",
            "the first element is Section A");
    const std::vector<LONG> staleRuntimeId = readRuntimeId(*stale.get(), "Section A's runtime id is readable");
    const auto staleInvoke                 = invokeOf(*stale.get(), "Section A exposes Invoke");
    Require(staleInvoke != nullptr, "Section A exposes Invoke before the rebuild");

    // Panel has no single-child removal: an application rebuilds the list, and Section C takes Section A's path.
    list->ClearChildren();
    auto* replacement = list->AddChild<Button>(L"Section C");
    auto* neighbour   = list->AddChild<Button>(L"Section D"); // Two semantic controls keep the root from collapsing into one.
    replacement->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    neighbour->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    replacement->SetOnClick([&clicksC] { ++clicksC; });
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();

    Require(staleInvoke->Invoke() == UIA_E_ELEMENTNOTAVAILABLE, "the old element reports that its control is gone");
    Require(clicksC == 0 && clicksA == 0, "Invoke through the old element clicks neither the removed nor the replacement control");

    const auto fresh = firstElement("the replacement's element is reachable");
    wil::com_ptr_nothrow<IRawElementProviderSimple> freshSimple;
    RequireSucceeded(fresh.query_to(freshSimple.put()), "the replacement's element is a simple provider");
    Require(ReadProviderStringProperty(*freshSimple.get(), UIA_NamePropertyId, "the replacement's name is readable") == L"Section C",
            "the fresh first element is Section C");
    Require(readRuntimeId(*fresh.get(), "Section C's runtime id is readable") != staleRuntimeId,
            "the replacement does not reuse the removed control's runtime id");
    const auto freshInvoke = invokeOf(*fresh.get(), "Section C exposes Invoke");
    Require(freshInvoke != nullptr, "Section C exposes Invoke");
    RequireSucceeded(freshInvoke->Invoke(), "a fresh element invokes the replacement");
    Require(clicksC == 1, "Invoke through a fresh element clicks the replacement once");
}

// A rebuilt tree is a different control at the old tree's path. Its items get runtime ids of their own, so a client
// never takes a new item for the removed one it had cached, and the removed item's element reports that it is gone.
void TestWindowHostReplacedTreeItemsGetNewRuntimeIds()
{
    using namespace DxUi;
    MutableTreeModel model;
    model.SetVisibleItems({TreeItemData{.id = 41u, .text = L"Alpha"}, TreeItemData{.id = 42u, .text = L"Beta"}});
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* holder = root->AddChild<Panel>();
    holder->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
    auto* tree = holder->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
    tree->SetModel(&model);
    root->AddChild<Button>(L"Après")->SetBounds(D2D1::RectF(0.0f, 128.0f, 120.0f, 160.0f)); // Keeps the root from collapsing.
    window.Host().SetRoot(std::move(root));

    const auto firstItem = [&](const char* context)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, context);
        wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
        RequireSucceeded(rootProvider.query_to(rootFragment.put()), context);
        wil::com_ptr_nothrow<IRawElementProviderFragment> treeElement;
        RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, treeElement.put()), context);
        Require(treeElement != nullptr, context);
        wil::com_ptr_nothrow<IRawElementProviderFragment> item;
        RequireSucceeded(treeElement->Navigate(NavigateDirection_FirstChild, item.put()), context);
        Require(item != nullptr, context);
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(item.query_to(simple.put()), context);
        Require(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, context) == L"Alpha", context);
        return simple;
    };
    const auto readRuntimeId = [](IRawElementProviderSimple& provider, const char* context)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        RequireSucceeded(provider.QueryInterface(IID_PPV_ARGS(fragment.put())), context);
        SAFEARRAY* runtimeId = nullptr;
        RequireSucceeded(fragment->GetRuntimeId(&runtimeId), context);
        const auto destroyRuntimeId = wil::scope_exit([&] { SafeArrayDestroy(runtimeId); });
        Require(runtimeId != nullptr, context);
        LONG lowerBound = 0;
        LONG upperBound = -1;
        RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lowerBound), context);
        RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upperBound), context);
        std::vector<LONG> values;
        for (LONG index = lowerBound; index <= upperBound; ++index)
        {
            LONG value = 0;
            RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &value), context);
            values.push_back(value);
        }
        return values;
    };

    const auto stale                       = firstItem("the first tree's Alpha item is reachable");
    const std::vector<LONG> staleRuntimeId = readRuntimeId(*stale.get(), "the first tree's Alpha runtime id is readable");

    holder->ClearChildren();
    auto* replacement = holder->AddChild<Tree>();
    replacement->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
    replacement->SetModel(&model);
    window.Host().RefreshAccessibilitySnapshot();

    const auto fresh = firstItem("the replacement tree's Alpha item is reachable");
    Require(readRuntimeId(*fresh.get(), "the replacement's Alpha runtime id is readable") != staleRuntimeId,
            "an item of the replacement tree does not reuse the removed tree's item runtime id");
    VARIANT staleName{};
    VariantInit(&staleName);
    const HRESULT staleResult = stale->GetPropertyValue(UIA_NamePropertyId, &staleName);
    VariantClear(&staleName);
    Require(staleResult == UIA_E_ELEMENTNOTAVAILABLE, "the removed tree's item element reports that it is gone");
}

class StructureInvalidationObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                                IUIAutomationStructureChangedEventHandler,
                                                                                Microsoft::WRL::FtmBase>
{
public:
    std::atomic<unsigned int> invalidations{0};
    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement*, StructureChangeType change, SAFEARRAY*) noexcept override
    {
        if (change == StructureChangeType_ChildrenInvalidated)
            invalidations.fetch_add(1);
        return S_OK;
    }
};

// Rebuilding a window's controls invalidates what a client navigated: the host tells it so (it drops elements that now
// report they are gone and navigates again), and a republish that changed no control tells it nothing.
void TestWindowHostRebuildRaisesStructureInvalidation()
{
    using namespace DxUi;
    constexpr ULONGLONG kClientSetupAllowanceMs = 20000;
    constexpr ULONGLONG kNotificationDeadlineMs = 3000;
    constexpr ULONGLONG kQuietPeriodMs          = 500;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* list = root->AddChild<Panel>();
    list->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    list->AddChild<Button>(L"Section A")->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    list->AddChild<Button>(L"Section B")->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<StructureInvalidationObserver> observer;
    observer.attach(Microsoft::WRL::Make<StructureInvalidationObserver>().Detach());
    Require(observer != nullptr, "allocate the structure observer");
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(bool(stop), "create the UIA client stop event");
    std::atomic<bool> ready{false};
    std::atomic<bool> finished{false};
    std::atomic<HRESULT> setup{E_PENDING};
    const HWND hwnd = window.Hwnd();
    std::jthread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        if (SUCCEEDED(hr))
            hr = automation->ElementFromHandle(hwnd, element.put());
        if (SUCCEEDED(hr))
            hr = automation->AddStructureChangedEventHandler(element.get(), TreeScope_Subtree, nullptr, observer.get());
        setup.store(hr);
        ready.store(true);
        if (SUCCEEDED(hr))
        {
            static_cast<void>(WaitForSingleObject(stop.get(), 15000));
            static_cast<void>(automation->RemoveStructureChangedEventHandler(element.get(), observer.get()));
        }
        finished.store(true);
    });
    const auto waitUntil = [&](ULONGLONG timeoutMs, const auto& predicate)
    {
        const auto deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            window.PumpMessages();
            Sleep(1);
        }
        return predicate();
    };
    const auto stopClient = wil::scope_exit([&]() noexcept
    {
        SetEvent(stop.get());
        // The jthread joins after this without pumping, and the client's teardown may need this thread's providers to answer:
        // wait for it here, pumping, as long as its setup was allowed, and fail instead of hanging in the join.
        Require(waitUntil(kClientSetupAllowanceMs, [&] { return finished.load(); }), "the UIA client thread ends");
    });
    Require(waitUntil(kClientSetupAllowanceMs, [&] { return ready.load(); }) && SUCCEEDED(setup.load()), "subscribe UIA structure changes");

    // Panel has no single-child removal: an application rebuilds the list, and other controls take the old paths.
    list->ClearChildren();
    list->AddChild<Button>(L"Section C")->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    list->AddChild<Button>(L"Section D")->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    window.Host().RefreshAccessibilitySnapshot();
    Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->invalidations.load() >= 1u; }),
            "a UIA client learns that the rebuilt controls invalidated its elements");

    // UIA may deliver more than one notification for the rebuild; the quiet period lets them all arrive first.
    static_cast<void>(waitUntil(kQuietPeriodMs, [] { return false; }));
    const unsigned int afterRebuild = observer->invalidations.load();
    window.Host().RefreshAccessibilitySnapshot();
    static_cast<void>(waitUntil(kQuietPeriodMs, [] { return false; }));
    Require(observer->invalidations.load() == afterRebuild, "a republish that changed no control raises no structure change");

    // Replacing the whole tree is compared with the tree before it, not with the empty snapshot standing in meanwhile.
    auto replacement = std::make_unique<Panel>();
    replacement->AddChild<Button>(L"Nouvelle page")->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    replacement->AddChild<Button>(L"Retour")->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    window.Host().SetRoot(std::move(replacement));
    Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->invalidations.load() > afterRebuild; }),
            "a UIA client learns that a new root replaced every element");
}

class DisclosureSenderObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                           IUIAutomationPropertyChangedEventHandler,
                                                                           Microsoft::WRL::FtmBase>
{
public:
    std::atomic<unsigned int> changes{0};
    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID property, VARIANT) noexcept override
    {
        if (property != UIA_ExpandCollapseExpandCollapseStatePropertyId)
            return S_OK;
        // The automation id is cached with the event, so the handler never calls back into the provider's thread.
        wil::unique_bstr automationId;
        if (sender && SUCCEEDED(sender->get_CachedAutomationId(automationId.put())) && automationId)
        {
            const std::scoped_lock lock(_mutex);
            _senderAutomationId.assign(automationId.get(), SysStringLen(automationId.get()));
        }
        changes.fetch_add(1);
        return S_OK;
    }

    [[nodiscard]] std::wstring SenderAutomationId() const
    {
        const std::scoped_lock lock(_mutex);
        return _senderAutomationId;
    }

private:
    mutable std::mutex _mutex;
    std::wstring _senderAutomationId;
};

// A status root collapses into its window's fragment root while its children stay elements of their own: an event
// about a child comes from that child, never from the root that stands for the window.
void TestCollapsedStatusRootChildEventComesFromTheChild()
{
    using namespace DxUi;
    constexpr ULONGLONG kClientSetupAllowanceMs = 20000;
    constexpr ULONGLONG kNotificationDeadlineMs = 3000;
    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 120.0f));
    root->SetAccessibilityRole(AccessibilityRole::Status);
    root->SetAccessibleAutomationId(L"UndoStatus");
    root->SetAccessibleName(L"Fichier supprimé");
    root->SetFocusable(false);
    auto* details = root->AddChild<Button>(L"Détails");
    details->SetBounds(D2D1::RectF(16.0f, 16.0f, 200.0f, 48.0f));
    details->SetAccessibleAutomationId(L"UndoStatus.Details");
    details->SetDisclosureExpanded(false);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<DisclosureSenderObserver> observer;
    observer.attach(Microsoft::WRL::Make<DisclosureSenderObserver>().Detach());
    Require(observer != nullptr, "allocate the disclosure sender observer");
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(bool(stop), "create the UIA client stop event");
    std::atomic<bool> ready{false};
    std::atomic<bool> finished{false};
    std::atomic<HRESULT> setup{E_PENDING};
    const HWND hwnd = window.Hwnd();
    std::jthread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        wil::com_ptr_nothrow<IUIAutomationCacheRequest> cache;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        if (SUCCEEDED(hr))
            hr = automation->ElementFromHandle(hwnd, element.put());
        if (SUCCEEDED(hr))
            hr = automation->CreateCacheRequest(cache.put());
        if (SUCCEEDED(hr))
            hr = cache->AddProperty(UIA_AutomationIdPropertyId);
        PROPERTYID property = UIA_ExpandCollapseExpandCollapseStatePropertyId;
        if (SUCCEEDED(hr))
            hr = automation->AddPropertyChangedEventHandlerNativeArray(element.get(), TreeScope_Subtree, cache.get(), observer.get(), &property, 1);
        setup.store(hr);
        ready.store(true);
        if (SUCCEEDED(hr))
        {
            static_cast<void>(WaitForSingleObject(stop.get(), 15000));
            static_cast<void>(automation->RemovePropertyChangedEventHandler(element.get(), observer.get()));
        }
        finished.store(true);
    });
    const auto waitUntil = [&](ULONGLONG timeoutMs, const auto& predicate)
    {
        const auto deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            window.PumpMessages();
            Sleep(1);
        }
        return predicate();
    };
    const auto stopClient = wil::scope_exit([&]() noexcept
    {
        SetEvent(stop.get());
        // The jthread joins after this without pumping, and the client's teardown may need this thread's providers to answer:
        // wait for it here, pumping, as long as its setup was allowed, and fail instead of hanging in the join.
        Require(waitUntil(kClientSetupAllowanceMs, [&] { return finished.load(); }), "the UIA client thread ends");
    });
    Require(waitUntil(kClientSetupAllowanceMs, [&] { return ready.load(); }) && SUCCEEDED(setup.load()), "subscribe disclosure changes below the status root");

    details->SetDisclosureExpanded(true);
    Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->changes.load() >= 1u; }), "a UIA client receives the child's disclosure change");
    Require(observer->SenderAutomationId() == L"UndoStatus.Details", "the disclosure change comes from the child, not the collapsed status root");
}

// What an in-process UI Automation client hears of a tree's selection: the four selection events and the IsSelected
// property change, each with the name of the element it came from (cached with the event, so the handler never calls back
// into the provider's thread).
struct HeardSelectionEvent
{
    std::wstring what; // "Selected", "Added", "Removed", "Invalidated" or "IsSelected:true|false".
    std::wstring name;
    [[nodiscard]] auto operator<=>(const HeardSelectionEvent&) const = default;
    [[nodiscard]] bool operator==(const HeardSelectionEvent&) const  = default;
};

class SelectionEventObserver final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                                         IUIAutomationEventHandler,
                                                                         IUIAutomationPropertyChangedEventHandler,
                                                                         Microsoft::WRL::FtmBase>
{
public:
    HRESULT STDMETHODCALLTYPE HandleAutomationEvent(IUIAutomationElement* sender, EVENTID eventId) noexcept override
    {
        const wchar_t* what = nullptr;
        switch (eventId)
        {
            case UIA_SelectionItem_ElementSelectedEventId: what = L"Selected"; break;
            case UIA_SelectionItem_ElementAddedToSelectionEventId: what = L"Added"; break;
            case UIA_SelectionItem_ElementRemovedFromSelectionEventId: what = L"Removed"; break;
            case UIA_Selection_InvalidatedEventId: what = L"Invalidated"; break;
            default: return S_OK;
        }
        Record(what, sender);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID property, VARIANT newValue) noexcept override
    {
        if (property == UIA_SelectionItemIsSelectedPropertyId && newValue.vt == VT_BOOL)
            Record(newValue.boolVal == VARIANT_TRUE ? L"IsSelected:true" : L"IsSelected:false", sender);
        return S_OK;
    }

    // How many events, repeats included, arrived since the last Take.
    [[nodiscard]] size_t Count() const
    {
        const std::scoped_lock lock(_mutex);
        return _events.size();
    }

    // How many different events were heard since the last Take. An in-process client hears each event twice: once as it is
    // raised and again a moment later (about 60 ms apart when several were raised at once).
    [[nodiscard]] size_t DistinctCount() const
    {
        const std::scoped_lock lock(_mutex);
        std::vector<HeardSelectionEvent> events = _events;
        std::ranges::sort(events);
        return static_cast<size_t>(std::ranges::unique(events).begin() - events.begin());
    }

    // The different events heard since the last call, in a fixed order: UI Automation delivers to its client on threads of
    // its own, so two events raised one after the other may arrive in either order, and it may deliver one twice.
    [[nodiscard]] std::vector<HeardSelectionEvent> Take()
    {
        const std::scoped_lock lock(_mutex);
        std::vector<HeardSelectionEvent> events = std::move(_events);
        _events.clear();
        std::ranges::sort(events);
        events.erase(std::ranges::unique(events).begin(), events.end());
        return events;
    }

private:
    void Record(const wchar_t* what, IUIAutomationElement* sender)
    {
        wil::unique_bstr name;
        if (sender)
            static_cast<void>(sender->get_CachedName(name.put()));
        const std::scoped_lock lock(_mutex);
        _events.push_back(HeardSelectionEvent{what, name ? std::wstring(name.get(), SysStringLen(name.get())) : std::wstring{}});
    }

    mutable std::mutex _mutex;
    std::vector<HeardSelectionEvent> _events;
};

// The selection events of a multi-select tree, heard by a UI Automation client: a selection that became one new item is that
// item being selected, other changes are items added to and removed from it (each also changes IsSelected), a change too large
// to name is one invalidation of the tree, and turning multi-select off removes the items it leaves out. The tree is beside a
// label, or it fills its window, where the window's root element stands for it (its items hang from that element, so a client
// subscribed to the window hears the events raised on them, and the invalidation is raised on the root itself).
void RunTreeMultiSelectSelectionEventsTest(bool fillsItsWindow)
{
    using namespace DxUi;
    constexpr ULONGLONG kClientSetupAllowanceMs = 20000;
    constexpr ULONGLONG kNotificationDeadlineMs = 3000;
    constexpr ULONGLONG kQuietPeriodMs          = 500;
    constexpr ULONGLONG kStreamDeadlineMs       = 10000;
    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();
    if (! fillsItsWindow)
        root->AddChild<Label>(L"Titre")->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(fillsItsWindow ? D2D1::RectF(0.0f, 0.0f, 240.0f, 168.0f) : D2D1::RectF(0.0f, 28.0f, 240.0f, 168.0f));
    tree->SetAccessibleName(L"Catégories");
    MutableTreeModel model;
    const auto items = [](uint64_t count)
    {
        std::vector<TreeItemData> rows;
        for (uint64_t id = 1u; id <= count; ++id)
            rows.push_back(TreeItemData{.id = id, .text = L"Élément " + std::to_wstring(id)});
        return rows;
    };
    model.SetVisibleItems(items(6u));
    tree->SetModel(&model);
    tree->SetMultiSelectEnabled(true);
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<SelectionEventObserver> observer;
    observer.attach(Microsoft::WRL::Make<SelectionEventObserver>().Detach());
    Require(observer != nullptr, "allocate the selection event observer");
    wil::unique_event stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(bool(stop), "create the UIA client stop event");
    std::atomic<bool> ready{false};
    std::atomic<bool> finished{false};
    std::atomic<HRESULT> setup{E_PENDING};
    const HWND hwnd = window.Hwnd();
    std::jthread client([&]
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto uninitialize   = wil::scope_exit([&]
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IUIAutomation> automation;
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        wil::com_ptr_nothrow<IUIAutomationCacheRequest> cache;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
        if (SUCCEEDED(hr))
            hr = automation->ElementFromHandle(hwnd, element.put());
        if (SUCCEEDED(hr))
            hr = automation->CreateCacheRequest(cache.put());
        if (SUCCEEDED(hr))
            hr = cache->AddProperty(UIA_NamePropertyId);
        constexpr std::array<EVENTID, 4> kEvents{UIA_SelectionItem_ElementSelectedEventId,
                                                 UIA_SelectionItem_ElementAddedToSelectionEventId,
                                                 UIA_SelectionItem_ElementRemovedFromSelectionEventId,
                                                 UIA_Selection_InvalidatedEventId};
        for (size_t index = 0u; SUCCEEDED(hr) && index < kEvents.size(); ++index)
            hr = automation->AddAutomationEventHandler(kEvents[index], element.get(), TreeScope_Subtree, cache.get(), observer.get());
        PROPERTYID property = UIA_SelectionItemIsSelectedPropertyId;
        if (SUCCEEDED(hr))
            hr = automation->AddPropertyChangedEventHandlerNativeArray(element.get(), TreeScope_Subtree, cache.get(), observer.get(), &property, 1);
        setup.store(hr);
        ready.store(true);
        if (SUCCEEDED(hr))
        {
            static_cast<void>(WaitForSingleObject(stop.get(), 30000));
            static_cast<void>(automation->RemoveAllEventHandlers());
        }
        finished.store(true);
    });
    const auto waitUntil = [&](ULONGLONG timeoutMs, const auto& predicate)
    {
        const auto deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            window.PumpMessages();
            Sleep(1);
        }
        return predicate();
    };
    const auto stopClient = wil::scope_exit([&]() noexcept
    {
        SetEvent(stop.get());
        // The jthread joins after this without pumping, and the client's teardown may need this thread's providers to answer:
        // wait for it here, pumping, as long as its setup was allowed, and fail instead of hanging in the join.
        Require(waitUntil(kClientSetupAllowanceMs, [&] { return finished.load(); }), "the UIA client thread ends");
    });
    Require(waitUntil(kClientSetupAllowanceMs, [&] { return ready.load(); }) && SUCCEEDED(setup.load()), "subscribe the UIA selection events");

    // What the client heard after `action`: the different events that arrived once the expected number had, and then none
    // came for a quiet period. The client hears each event again a moment after the first time, so the stream is waited
    // out: a repeat must not reach the next step.
    const auto hear = [&](size_t expected, const auto& action)
    {
        static_cast<void>(observer->Take());
        action();
        if (expected != 0u)
            Require(waitUntil(kNotificationDeadlineMs, [&] { return observer->DistinctCount() >= expected; }), "the UIA client hears the selection events");
        size_t arrived        = observer->Count();
        ULONGLONG lastArrival = GetTickCount64();
        static_cast<void>(waitUntil(kStreamDeadlineMs,
                                    [&]
        {
            if (observer->Count() != arrived)
            {
                arrived     = observer->Count();
                lastArrival = GetTickCount64();
            }
            return GetTickCount64() - lastArrival >= kQuietPeriodMs;
        }));
        return observer->Take();
    };
    using Heard     = std::vector<HeardSelectionEvent>;
    const auto utf8 = [](const std::wstring& text)
    {
        std::string narrow(
            static_cast<size_t>((std::max)(0, WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr))),
            '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), static_cast<int>(narrow.size()), nullptr, nullptr);
        return narrow;
    };
    const auto requireHeard = [&](const Heard& actual, Heard expected, const char* context)
    {
        std::ranges::sort(expected);
        if (actual == expected)
            return;
        std::string message = std::string(context) + ": the client heard";
        for (const HeardSelectionEvent& event : actual)
            message += " [" + utf8(event.what) + " " + utf8(event.name) + "]";
        Require(false, message.c_str());
    };
    const auto item = [](uint64_t id) { return std::wstring(L"Élément ") + std::to_wstring(id); };

    // A selection that became one new item is that item being selected.
    requireHeard(hear(2u, [&] { tree->SetSelectedItemIds(std::vector<uint64_t>{2u}); }),
                 {{L"Selected", item(2u)}, {L"IsSelected:true", item(2u)}},
                 "selecting one item of an empty selection");

    // An item added to a selection is added, and one taken out is removed.
    requireHeard(hear(2u, [&] { static_cast<void>(tree->RequestAddVisibleItemToSelection(3u)); }),
                 {{L"Added", item(4u)}, {L"IsSelected:true", item(4u)}},
                 "adding an item to a selection");
    requireHeard(hear(2u, [&] { static_cast<void>(tree->RequestRemoveVisibleItemFromSelection(1u)); }),
                 {{L"Removed", item(2u)}, {L"IsSelected:false", item(2u)}},
                 "removing an item from a selection");

    // Replacing the selection with one other item is that item being selected: the old one leaves without an event of its own.
    requireHeard(hear(3u, [&] { static_cast<void>(tree->RequestSelectVisibleItem(5u)); }),
                 {{L"Selected", item(6u)}, {L"IsSelected:true", item(6u)}, {L"IsSelected:false", item(4u)}},
                 "replacing the selection with one item");

    // Several items at once are each added.
    requireHeard(hear(10u, [&] { tree->SetSelectedItemIds(std::vector<uint64_t>{1u, 2u, 3u, 4u, 5u, 6u}); }),
                 {{L"Added", item(1u)},
                  {L"Added", item(2u)},
                  {L"Added", item(3u)},
                  {L"Added", item(4u)},
                  {L"Added", item(5u)},
                  {L"IsSelected:true", item(1u)},
                  {L"IsSelected:true", item(2u)},
                  {L"IsSelected:true", item(3u)},
                  {L"IsSelected:true", item(4u)},
                  {L"IsSelected:true", item(5u)}},
                 "adding five items at once");

    // More changes than a client is told of one by one are one invalidation of the tree.
    model.SetVisibleItems(items(30u));
    tree->NotifyDataChanged();
    static_cast<void>(observer->Take());
    requireHeard(
        hear(1u, [&] { Require(tree->OnSelectAll(window.Host()), "select everything"); }), {{L"Invalidated", L"Catégories"}}, "selecting thirty items");
    Require(tree->GetSelectedItemIds().size() == 30u, "every row is selected");

    // Rows that only moved are silent however many are selected: all thirty, in the reverse order.
    requireHeard(hear(0u,
                      [&]
    {
        std::vector<TreeItemData> reversed = items(30u);
        std::ranges::reverse(reversed);
        model.SetVisibleItems(std::move(reversed));
        tree->NotifyDataChanged();
    }),
                 {},
                 "moving thirty selected rows");

    // Selected items that left the tree cannot be named: that is an invalidation too.
    requireHeard(hear(1u,
                      [&]
    {
        model.SetVisibleItems(items(3u));
        tree->NotifyDataChanged();
    }),
                 {{L"Invalidated", L"Catégories"}},
                 "removing selected rows");
    RequireTreeIds(tree->GetSelectedItemIds(), {1u, 2u, 3u}, "the rows that stayed are still selected");

    // A republish that changed no selection, and rows that only moved, are silent.
    requireHeard(hear(0u, [&] { window.Host().RefreshAccessibilitySnapshot(); }), {}, "republishing an unchanged selection");
    requireHeard(hear(0u,
                      [&]
    {
        model.SetVisibleItems(
            {TreeItemData{.id = 3u, .text = L"Élément 3"}, TreeItemData{.id = 1u, .text = L"Élément 1"}, TreeItemData{.id = 2u, .text = L"Élément 2"}});
        tree->NotifyDataChanged();
    }),
                 {},
                 "moving selected rows");

    // Turning multi-select off keeps the last selected item alone, as none has the focus: the others are removed. The single
    // selection then raises its own events (see ExpectClientHearsSingleSelectionTreeEvents), and turning multi-select on again
    // keeps that item as the whole selection, which changes nothing.
    requireHeard(hear(4u, [&] { tree->SetMultiSelectEnabled(false); }),
                 {{L"Removed", item(1u)}, {L"Removed", item(3u)}, {L"IsSelected:false", item(1u)}, {L"IsSelected:false", item(3u)}},
                 "turning multi-select off");
    RequireTreeIds(tree->GetSelectedItemIds(), {2u}, "the last selected item stays selected alone");
    requireHeard(hear(3u, [&] { static_cast<void>(tree->RequestSelectVisibleItem(0u)); }),
                 {{L"Selected", item(3u)}, {L"IsSelected:true", item(3u)}, {L"IsSelected:false", item(2u)}},
                 "selecting in a tree without multi-select");
    requireHeard(hear(0u, [&] { tree->SetMultiSelectEnabled(true); }), {}, "turning multi-select on again");

    // A selection that becomes one new item while a selected item leaves the tree: the new item's being selected says the others
    // left it, so the item that has no element needs no event and the one still in the tree reports only its IsSelected change.
    requireHeard(hear(2u, [&] { tree->SetSelectedItemIds(std::vector<uint64_t>{1u, 3u}); }),
                 {{L"Added", item(1u)}, {L"IsSelected:true", item(1u)}},
                 "adding an item to the selection again");
    requireHeard(hear(3u,
                      [&]
    {
        model.SetVisibleItems({TreeItemData{.id = 1u, .text = item(1u)}, TreeItemData{.id = 2u, .text = item(2u)}});
        tree->SetSelectedItemIds(std::vector<uint64_t>{2u});
        tree->NotifyDataChanged();
    }),
                 {{L"Selected", item(2u)}, {L"IsSelected:true", item(2u)}, {L"IsSelected:false", item(1u)}},
                 "selecting one item as a selected item leaves the tree");
    RequireTreeIds(tree->GetSelectedItemIds(), {2u}, "the item the application chose is the whole selection");
}

void TestAccessibilityTreeMultiSelectRaisesSelectionEvents()
{
    RunTreeMultiSelectSelectionEventsTest(false);
}

// The same events and silences for a tree that is its window's only control, which is what a dialog with one list is.
void TestAccessibilityTreeMultiSelectRaisesSelectionEventsWhenItFillsItsWindow()
{
    RunTreeMultiSelectSelectionEventsTest(true);
}

// A focus-changed callback may rebuild the controls around the one it was told about. The host neither keeps nor
// publishes focus on a control the callback removed.
void TestWindowHostFocusCallbackThatRemovesTheControlLeavesNoFocus()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* list = root->AddChild<Panel>();
    list->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    auto* removed = list->AddChild<Button>(L"Section A");
    removed->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    list->AddChild<Button>(L"Section B")->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    window.Host().SetRoot(std::move(root));
    bool rebuilt = false;
    window.Host().SetOnFocusChanged([&](Control* control)
    {
        if (rebuilt || control != removed)
            return;
        rebuilt = true;
        list->ClearChildren();
        list->AddChild<Button>(L"Section C")->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
        list->AddChild<Button>(L"Section D")->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    });

    window.Host().SetFocusControl(removed);
    Require(rebuilt, "the focus callback rebuilt the list");
    Require(window.Host().GetFocusControl() == nullptr, "the host keeps no focus on the control its callback removed");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "the window exposes its fragment root");
    wil::com_ptr_nothrow<IRawElementProviderFragment> focused;
    RequireSucceeded(rootProvider->GetFocus(focused.put()), "the fragment root reports its focus");
    Require(focused == nullptr, "the published snapshot has no focused element");
    window.Host().SetOnFocusChanged({});
}

// A rebuilt list reuses tree paths, and the allocator reuses the memory of controls no element references any more:
// every generation of controls at a path still gets runtime ids no earlier one had.
void TestWindowHostRuntimeIdsNeverRepeatAcrossRebuilds()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* list = root->AddChild<Panel>();
    list->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 120.0f));
    const auto fill = [list](int generation)
    {
        list->AddChild<Button>(L"Section " + std::to_wstring(generation))->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
        list->AddChild<Button>(L"Suite " + std::to_wstring(generation))->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
    };
    fill(0);
    window.Host().SetRoot(std::move(root));
    const auto firstRuntimeId = [&]
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "the window exposes its fragment root");
        wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
        RequireSucceeded(rootProvider.query_to(rootFragment.put()), "the fragment root navigates");
        wil::com_ptr_nothrow<IRawElementProviderFragment> first;
        RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, first.put()), "reach the first section");
        Require(first != nullptr, "the first section has an element");
        SAFEARRAY* runtimeId = nullptr;
        RequireSucceeded(first->GetRuntimeId(&runtimeId), "read the first section's runtime id");
        const auto destroyRuntimeId = wil::scope_exit([&] { SafeArrayDestroy(runtimeId); });
        LONG lowerBound             = 0;
        LONG upperBound             = -1;
        RequireSucceeded(SafeArrayGetLBound(runtimeId, 1, &lowerBound), "runtime id lower bound");
        RequireSucceeded(SafeArrayGetUBound(runtimeId, 1, &upperBound), "runtime id upper bound");
        std::vector<LONG> values;
        for (LONG index = lowerBound; index <= upperBound; ++index)
        {
            LONG value = 0;
            RequireSucceeded(SafeArrayGetElement(runtimeId, &index, &value), "runtime id value");
            values.push_back(value);
        }
        return values; // Every element is released here, so nothing keeps an old control's memory in use.
    };
    std::vector<std::vector<LONG>> seen{firstRuntimeId()};
    for (int generation = 1; generation <= 6; ++generation)
    {
        list->ClearChildren();
        fill(generation);
        window.Host().RefreshAccessibilitySnapshot();
        std::vector<LONG> current = firstRuntimeId();
        Require(std::ranges::find(seen, current) == seen.end(), "a rebuilt control never reuses an earlier control's runtime id");
        seen.push_back(std::move(current));
    }
}

// A panel may clear its children, the focused control with them, before the host's next message prunes that focus; a
// republish in between (any accessible property change) must not touch the removed control.
void TestWindowHostPublishSkipsAFocusedControlItsPanelRemoved()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* list = root->AddChild<Panel>();
    list->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 60.0f));
    auto* focused = list->AddChild<Button>(L"Section A");
    focused->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    auto* status = root->AddChild<Label>(L"Prêt");
    status->SetBounds(D2D1::RectF(8.0f, 70.0f, 200.0f, 100.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(focused);

    list->ClearChildren(); // The focused button is destroyed; no message has let the host prune it yet.
    status->SetAccessibleName(L"Liste vidée");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "the window exposes its fragment root");
    wil::com_ptr_nothrow<IRawElementProviderFragment> focus;
    RequireSucceeded(rootProvider->GetFocus(focus.put()), "the fragment root reports its focus");
    Require(focus == nullptr, "the published snapshot does not keep focus on the removed control");
    Require(PostMessageW(window.Hwnd(), WM_NULL, 0, 0) != FALSE, "queue a message for the host");
    window.PumpMessages();
    Require(window.Host().GetFocusControl() == nullptr, "the host prunes the removed control's focus at its next message");
}

// A host without a tree may focus a control it does not own; a focus-changed callback leaves that focus in place.
void TestRootlessHostKeepsFocusAfterItsCallback()
{
    using namespace DxUi;
    WindowHost host;
    Button button(L"Seul");
    Control* notified = nullptr;
    host.SetOnFocusChanged([&notified](Control* control) { notified = control; });
    host.SetFocusControl(&button);
    Require(notified == &button, "the callback hears the focused control");
    Require(host.GetFocusControl() == &button, "a rootless host keeps focus on a live control after its callback");
    host.SetOnFocusChanged({});
    host.SetFocusControl(nullptr);
}

// A screen reader reaches a NumericStepper's step buttons by name, not by the private-use glyphs they paint.
void TestNumericStepperStepButtonsAreNamedForAutomation()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root     = std::make_unique<Panel>();
    auto* stepper = root->AddChild<NumericStepper>();
    stepper->SetBounds(D2D1::RectF(8.0f, 8.0f, 220.0f, 40.0f));
    stepper->SetLabel(L"Largeur", 60.0f);
    stepper->SetStepButtonNames(L"Augmenter la largeur", L"Diminuer la largeur");
    window.Host().SetRoot(std::move(root));

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "the stepper window publishes a UIA root");
    wil::com_ptr_nothrow<IRawElementProviderFragment> element;
    RequireSucceeded(rootProvider.query_to(element.put()), "the stepper root navigates");
    wil::com_ptr_nothrow<IRawElementProviderFragment> child;
    RequireSucceeded(element->Navigate(NavigateDirection_FirstChild, child.put()), "the stepper root has children");
    std::vector<std::wstring> names;
    for (size_t guard = 0u; child && guard < 16u; ++guard)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(child.query_to(simple.put()), "a stepper element is a simple provider");
        names.push_back(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "a stepper element name is readable"));
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(child->Navigate(NavigateDirection_NextSibling, next.put()), "stepper elements navigate to their siblings");
        child = std::move(next);
    }
    const auto named = [&](std::wstring_view name) { return std::ranges::find(names, name) != names.end(); };
    Require(named(L"Augmenter la largeur") && named(L"Diminuer la largeur"), "UI Automation exposes the step buttons by their localized names");
    Require(std::ranges::none_of(names, [](const std::wstring& name) { return name.size() == 1u && name[0] >= L'\xE000' && name[0] <= L'\xF8FF'; }),
            "no step button is announced as a private-use glyph");
}

// What a call examines to resolve a control, as the diagnostics counter counts it, for each kind of call a client makes.
struct AccessibilityResolutionCost
{
    uint64_t worst = 0u;
    uint64_t total = 0u;
    size_t calls   = 0u;

    [[nodiscard]] double Average() const noexcept
    {
        return calls == 0u ? 0.0 : static_cast<double>(total) / static_cast<double>(calls);
    }

    template <typename Call> void Measure(const Call& call)
    {
        DxUi::DebugResetAccessibilityResolutionVisitCountForTest();
        call();
        const uint64_t visited = DxUi::DebugGetAccessibilityResolutionVisitCountForTest();
        worst                  = (std::max)(worst, visited);
        total += visited;
        ++calls;
    }
};

// A window of panels holding 49 buttons each: 40 of them are about 2,000 controls.
class LargeAccessibilityWindow final
{
public:
    static constexpr size_t kPerGroup = 49u;

    explicit LargeAccessibilityWindow(size_t groupCount)
    {
        using namespace DxUi;
        auto root = std::make_unique<Panel>();
        for (size_t group = 0u; group < groupCount; ++group)
        {
            auto* panel = root->AddChild<Panel>();
            groups.push_back(panel);
            panel->SetBounds(D2D1::RectF(static_cast<float>(group) * 120.0f, 0.0f, static_cast<float>(group + 1u) * 120.0f, 1000.0f));
            for (size_t index = 0u; index < kPerGroup; ++index)
            {
                auto* button = panel->AddChild<Button>(std::format(L"Bouton {}-{}", group, index));
                button->SetBounds(D2D1::RectF(static_cast<float>(group) * 120.0f,
                                              static_cast<float>(index) * 20.0f,
                                              static_cast<float>(group + 1u) * 120.0f - 8.0f,
                                              static_cast<float>(index) * 20.0f + 18.0f));
                buttons.push_back(button);
            }
        }
        window.Host().SetRoot(std::move(root));
    }

    AttachedHostWindow window;
    std::vector<DxUi::Panel*> groups;
    std::vector<DxUi::Button*> buttons;
};

struct AccessibilityWalkCosts
{
    AccessibilityResolutionCost navigate;
    AccessibilityResolutionCost name;
    AccessibilityResolutionCost bounds;
    AccessibilityResolutionCost pattern;
};

// Walks every element of the window from its root, as a client does, reading a name, bounds and a pattern of each.
[[nodiscard]] AccessibilityWalkCosts WalkAccessibilityElements(LargeAccessibilityWindow& target)
{
    AccessibilityWalkCosts costs;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(DxUi::CreateWindowHostAccessibilityProvider(target.window.Hwnd()));
    Require(rootProvider != nullptr, "the large window publishes a UIA root");
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "the large window's root navigates");
    wil::com_ptr_nothrow<IRawElementProviderFragment> element;
    costs.navigate.Measure([&] { RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, element.put()), "the root has a first child"); });
    size_t walked = 0u;
    while (element)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(element.query_to(simple.put()), "an element is a simple provider");
        std::wstring elementName;
        costs.name.Measure([&] { elementName = ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "an element's name is readable"); });
        Require(elementName == std::format(L"Bouton {}-{}", walked / LargeAccessibilityWindow::kPerGroup, walked % LargeAccessibilityWindow::kPerGroup),
                "the walk reaches every button in tree order");
        UiaRect rect{};
        costs.bounds.Measure([&] { RequireSucceeded(element->get_BoundingRectangle(&rect), "an element's bounds are readable"); });
        Require(rect.width > 0.0 && rect.height > 0.0, "an element's bounds are its button's");
        wil::com_ptr_nothrow<IUnknown> invoke;
        costs.pattern.Measure([&] { RequireSucceeded(simple->GetPatternProvider(UIA_InvokePatternId, invoke.put()), "an element's patterns are queryable"); });
        Require(invoke != nullptr, "every button exposes Invoke");
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        costs.navigate.Measure([&] { RequireSucceeded(element->Navigate(NavigateDirection_NextSibling, next.put()), "an element navigates to its sibling"); });
        element = std::move(next);
        ++walked;
    }
    Require(walked == target.buttons.size(), "the walk reaches every button");
    return costs;
}

// What resolving a window host's controls examines: a provider call reads its control's record from tables built when
// the snapshot was published, so its cost stays the same as the tree grows. A client that walks every element of a
// window of about 2,000 controls examines a few table slots per call, where a scan of the published records examined
// about 1,000 on average for each lookup (4,901 per Navigate call before the tables, 5.8 after).
void TestAccessibilityProvidersResolveTheirControlWithoutScanningTheTree()
{
    using namespace DxUi;
    DebugSetAccessibilityResolutionCountingForTest(true);
    const auto stopCounting = wil::scope_exit([]() noexcept { DebugSetAccessibilityResolutionCountingForTest(false); });
    LargeAccessibilityWindow fewControls(5u);
    const AccessibilityWalkCosts smallCosts = WalkAccessibilityElements(fewControls);
    LargeAccessibilityWindow manyControls(40u);
    const AccessibilityWalkCosts largeCosts = WalkAccessibilityElements(manyControls);
    const auto report                       = [](const char* what, const AccessibilityResolutionCost& cost)
    {
        std::cout << "Accessibility resolution: " << what << " examined " << cost.Average() << " on average, " << cost.worst << " at most, over " << cost.calls
                  << " calls\n";
    };
    report("Navigate (245 buttons)", smallCosts.navigate);
    report("Navigate (1,960 buttons)", largeCosts.navigate);
    report("GetPropertyValue (1,960 buttons)", largeCosts.name);
    report("get_BoundingRectangle (1,960 buttons)", largeCosts.bounds);
    report("GetPatternProvider (1,960 buttons)", largeCosts.pattern);

    // A few table probes per lookup and a few lookups per call, however many controls the window holds.
    constexpr uint64_t kMostExaminedPerCall = 32u;
    Require(largeCosts.navigate.worst <= kMostExaminedPerCall, "Navigate resolves its element and its sibling in a few steps");
    Require(largeCosts.name.worst <= kMostExaminedPerCall, "reading a property resolves the element's control in a few steps");
    Require(largeCosts.bounds.worst <= kMostExaminedPerCall, "reading bounds resolves the element's control and hit rectangle in a few steps");
    Require(largeCosts.pattern.worst <= kMostExaminedPerCall, "querying a pattern resolves the element's control in a few steps");
    const auto sameAsSmall = [](const AccessibilityResolutionCost& few, const AccessibilityResolutionCost& many)
    { return many.Average() <= few.Average() * 1.5 + 1.0; };
    Require(sameAsSmall(smallCosts.navigate, largeCosts.navigate) && sameAsSmall(smallCosts.name, largeCosts.name) &&
                sameAsSmall(smallCosts.bounds, largeCosts.bounds) && sameAsSmall(smallCosts.pattern, largeCosts.pattern),
            "a call costs no more in a tree eight times the size");

    // An event resolves the path of the control it is about the same way: the child indices from the root.
    const auto resolveEventPath = [&](const DxUi::Control* control, uint32_t& depth, std::array<uint16_t, 16>& indices)
    { return DebugResolveWindowHostEventPathForTest(manyControls.window.Hwnd(), control, indices, depth); };
    AccessibilityResolutionCost eventPath;
    for (size_t position = 0u; position < manyControls.buttons.size(); ++position)
    {
        uint32_t depth = 0u;
        std::array<uint16_t, 16> indices{};
        bool resolved = false;
        eventPath.Measure([&] { resolved = resolveEventPath(manyControls.buttons[position], depth, indices); });
        Require(resolved && depth == 2u && indices[0] == position / LargeAccessibilityWindow::kPerGroup &&
                    indices[1] == position % LargeAccessibilityWindow::kPerGroup,
                "an event finds its button's path");
    }
    report("event path (1,960 buttons)", eventPath);
    // A search of the sorted addresses (about log2 of 1,960) and two walks down a path two levels deep.
    constexpr uint64_t kMostExaminedPerEvent = 24u;
    Require(eventPath.worst <= kMostExaminedPerEvent, "an event finds its control's path without searching the tree");

    // The tables describe the published tree. A control added since has no entry and is still found, as a search of
    // the live tree would find it; one removed or hidden is not, and a control that is no element never is.
    uint32_t depth = 0u;
    std::array<uint16_t, 16> indices{};
    auto* added = manyControls.groups[7]->AddChild<Button>(L"Ajouté");
    added->SetBounds(D2D1::RectF(840.0f, 980.0f, 952.0f, 998.0f));
    Require(resolveEventPath(added, depth, indices) && depth == 2u && indices[0] == 7u && indices[1] == LargeAccessibilityWindow::kPerGroup,
            "a control added since the last publish is still found");
    Require(! resolveEventPath(manyControls.groups[7], depth, indices), "a panel is no element");
    manyControls.groups[9]->SetVisible(false);
    Require(! resolveEventPath(manyControls.buttons[9u * LargeAccessibilityWindow::kPerGroup + 4u], depth, indices),
            "a control under a hidden panel is no element");
    const Button* const removed = manyControls.buttons[11u * LargeAccessibilityWindow::kPerGroup + 5u];
    manyControls.groups[11]->ClearChildren();
    Require(! resolveEventPath(removed, depth, indices), "a removed control is no element and is never dereferenced");
}

// The lookup tables built with each snapshot answer as a scan of its records does, for every kind of fragment it holds
// (controls, tree items, grid headers, rows and cells, a password field's reveal button, a scrolled panel's clipped
// hits) and at the deepest path it records, before and after the tree changes.
void TestAccessibilityLookupTablesAgreeWithAScanOfTheRecords()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root       = std::make_unique<Panel>();
    auto* treeLabel = root->AddChild<Label>(L"Categories");
    treeLabel->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 28.0f, 240.0f, 88.0f));
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems(
        {TreeItemData{.id = 1u, .text = L"General"}, TreeItemData{.id = 2u, .text = L"Panes"}, TreeItemData{.id = 3u, .text = L"Viewers"}});
    tree->SetModel(&treeModel);
    tree->SetSelectedItemId(2u);
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 120.0f, 240.0f, 188.0f));
    MultiRowGridModel gridModel(6u);
    grid->SetModel(&gridModel);
    auto* field = root->AddChild<TextField>(L"secret");
    field->SetMasked(true);
    field->SetAccessibleName(L"Mot de passe");
    field->SetBounds(D2D1::RectF(0.0f, 192.0f, 220.0f, 224.0f));
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(300.0f, 0.0f, 520.0f, 120.0f));
    scroll->SetContentHeight(260.0f);
    scroll->SetScrollOffset(80.0f);
    scroll->AddChild<Button>(L"Au-dessus")->SetBounds(D2D1::RectF(312.0f, 12.0f, 480.0f, 48.0f));
    scroll->AddChild<Button>(L"Visible")->SetBounds(D2D1::RectF(312.0f, 112.0f, 480.0f, 148.0f));
    auto* hidden = root->AddChild<Panel>();
    hidden->AddChild<Button>(L"Caché")->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 30.0f));
    hidden->SetVisible(false);
    // A snapshot records paths of at most 16 indices: the button at the 16th level is an element, the one below it is not.
    Panel* level = root->AddChild<Panel>();
    for (size_t nested = 2u; nested <= 15u; ++nested)
        level = level->AddChild<Panel>();
    auto* deepest = level->AddChild<Button>(L"Le plus profond");
    deepest->SetBounds(D2D1::RectF(0.0f, 400.0f, 100.0f, 430.0f));
    auto* tooDeep = level->AddChild<Panel>()->AddChild<Button>(L"Trop profond");
    tooDeep->SetBounds(D2D1::RectF(0.0f, 440.0f, 100.0f, 470.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field);
    window.Host().RefreshAccessibilitySnapshot();
    Require(DebugCountAccessibilityIndexMismatchesForTest(window.Hwnd()) == 0u, "every lookup agrees with a scan of the records");

    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    Require(rootProvider != nullptr, "the window publishes a UIA root");
    wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
    RequireSucceeded(rootProvider.query_to(rootFragment.put()), "the root navigates");
    std::vector<std::wstring> names;
    wil::com_ptr_nothrow<IRawElementProviderFragment> element;
    RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, element.put()), "the root has a first child");
    while (element)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(element.query_to(simple.put()), "an element is a simple provider");
        names.push_back(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "an element's name is readable"));
        UiaRect rect{};
        RequireSucceeded(element->get_BoundingRectangle(&rect), "an element's bounds are readable");
        if (names.back() == L"Visible" || names.back() == L"Le plus profond")
            Require(rect.width > 0.0 && rect.height > 0.0, "an element in view has bounds");
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(element->Navigate(NavigateDirection_NextSibling, next.put()), "an element navigates to its sibling");
        element = std::move(next);
    }
    const auto listed = [&](std::wstring_view name) { return std::ranges::find(names, name) != names.end(); };
    Require(listed(L"Categories") && listed(L"Mot de passe") && listed(L"Le plus profond"), "the walk reaches controls at every level, the 16th included");
    Require(! listed(L"Trop profond") && ! listed(L"Caché"), "a control below the 16th level and one under a hidden panel are no elements");

    uint32_t depth = 0u;
    std::array<uint16_t, 16> indices{};
    Require(DebugResolveWindowHostEventPathForTest(window.Hwnd(), deepest, indices, depth) && depth == 16u, "an event finds the button at the 16th level");
    Require(! DebugResolveWindowHostEventPathForTest(window.Hwnd(), tooDeep, indices, depth), "an event finds no path below the 16th level");
    Require(DebugResolveWindowHostEventPathForTest(window.Hwnd(), field, indices, depth) && depth == 1u && indices[0] == 3u,
            "an event finds the password field");

    // The same after the tree changes and is published again.
    scroll->SetScrollOffset(0.0f);
    scroll->AddChild<Button>(L"Ajouté")->SetBounds(D2D1::RectF(312.0f, 60.0f, 480.0f, 90.0f));
    window.Host().SetFocusControl(tree);
    window.Host().RefreshAccessibilitySnapshot();
    Require(DebugCountAccessibilityIndexMismatchesForTest(window.Hwnd()) == 0u, "every lookup still agrees after the tree changed");
    scroll->ClearChildren();
    window.Host().RefreshAccessibilitySnapshot();
    Require(DebugCountAccessibilityIndexMismatchesForTest(window.Hwnd()) == 0u, "every lookup still agrees after controls were removed");
}

// The providers of a Grid's cells in a window whose tree is a label (found by the point `labelX`, `labelY`) and the grid after it:
// the grid's, and every cell's, row by row as the row's structure offers them.
struct GridCellProviders
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> grid;
    std::vector<std::vector<wil::com_ptr_nothrow<IRawElementProviderFragment>>> rows;
};

[[nodiscard]] GridCellProviders ResolveGridCellProviders(AttachedHostWindow& window, IRawElementProviderFragmentRoot& rootProvider, float labelX, float labelY)
{
    GridCellProviders result;
    auto labelProvider = GetProviderAtDipPoint(window.Hwnd(), window.Host(), rootProvider, labelX, labelY, "the grid's label resolves by point");
    RequireSucceeded(labelProvider->Navigate(NavigateDirection_NextSibling, result.grid.put()), "the label navigates to the grid");
    Require(result.grid != nullptr, "the grid's provider exists");
    // The grid's children are its column headers, then its rows; a row's children are its cells.
    wil::com_ptr_nothrow<IRawElementProviderFragment> child;
    RequireSucceeded(result.grid->Navigate(NavigateDirection_FirstChild, child.put()), "the grid exposes its structure");
    Require(child != nullptr, "the grid exposes a column header");
    while (child)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(child->QueryInterface(IID_PPV_ARGS(simple.put())), "a grid child exposes a simple provider");
        if (ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "a grid child exposes its control type") == UIA_DataItemControlTypeId)
        {
            auto& cells = result.rows.emplace_back();
            wil::com_ptr_nothrow<IRawElementProviderFragment> cell;
            RequireSucceeded(child->Navigate(NavigateDirection_FirstChild, cell.put()), "a row navigates to its first cell");
            while (cell)
            {
                cells.push_back(cell);
                wil::com_ptr_nothrow<IRawElementProviderFragment> nextCell;
                RequireSucceeded(cell->Navigate(NavigateDirection_NextSibling, nextCell.put()), "a cell navigates to the next cell");
                cell = nextCell;
            }
        }
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(child->Navigate(NavigateDirection_NextSibling, next.put()), "a grid child navigates to the next one");
        child = next;
    }
    return result;
}

// What UI Automation reads of one cell: its Name, its Value property and its ValuePattern.
struct CellTexts
{
    std::wstring name;
    std::wstring value;
    std::wstring patternValue;
};

[[nodiscard]] CellTexts ReadCellTexts(IRawElementProviderFragment& cellProvider)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(cellProvider.QueryInterface(IID_PPV_ARGS(simple.put())), "a cell exposes a simple provider");
    CellTexts texts;
    texts.name  = ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "a cell exposes its Name");
    texts.value = ReadProviderStringProperty(*simple.get(), UIA_ValueValuePropertyId, "a cell exposes its Value");
    wil::com_ptr_nothrow<IUnknown> valueUnknown;
    RequireSucceeded(simple->GetPatternProvider(UIA_ValuePatternId, valueUnknown.put()), "a cell's ValuePattern lookup succeeds");
    Require(valueUnknown != nullptr, "a text cell exposes the ValuePattern");
    wil::com_ptr_nothrow<IValueProvider> valueProvider;
    RequireSucceeded(valueUnknown.query_to(valueProvider.put()), "the pattern is an IValueProvider");
    wil::unique_bstr pattern;
    RequireSucceeded(valueProvider->get_Value(pattern.put()), "the ValuePattern returns the value");
    texts.patternValue = pattern.get() ? std::wstring(pattern.get(), SysStringLen(pattern.get())) : std::wstring{};
    return texts;
}

// UI Automation carries the complete value of a multiline cell, unit for unit, whatever the paint shows: CR LF, U+2028 and U+2029
// breaks, a zero-width-joiner emoji, a 5,000-unit word, decomposed accents (they stay decomposed: nothing normalizes a value),
// Arabic, separators the paint ignores and a value of 100,000 units far past what a cell shapes. The Name, the Value property and
// the ValuePattern agree, in a left-to-right grid and in a right-to-left one.
void TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    const std::vector<std::vector<std::wstring>> values{
        {L"Première ligne\r\nDeuxième ligne\r\nTroisième ligne masquée",
         L"Un\u2028Deux\u2028Trois\u2028Quatre",
         L"Équipe \U0001F468\u200D\U0001F469\u200D\U0001F467\nAppareil \U0001F4F7\nAgrumes \U0001F34A"},
        {std::wstring(5000u, L'\u00E9'), L"Cre\u0301me bru\u0302le\u0301e\nDeuxie\u0300me ligne\nTroisie\u0300me ligne", L"Ligne A\r\nLigne B\r\nLigne C\r\n"},
        {RepeatToUnits(L"mot suivant très long \u00E9t\u00E9 ", 100000u),
         L"مرحبا بالعالم الجميل\nالسطر الثاني\nالسطر الثالث",
         L"\u2029Début\u2029Milieu\u2029Fin \u2028\u2028"},
    };
    for (const bool rightToLeft : {false, true})
    {
        AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
        ConfigureHostPlace(window, 96u, false, Density::Standard, D2D1::SizeF(520.0f, 240.0f));
        TextTableModel model(values, {150.0f, 150.0f, 150.0f});
        auto root   = std::make_unique<Panel>();
        auto* label = root->AddChild<Label>(L"Grid witness");
        label->SetBounds(D2D1::RectF(10.0f, 4.0f, 200.0f, 28.0f));
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(10.0f, 30.0f, 510.0f, 230.0f));
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(48.0f);
        grid->SetLineClamp(2u);
        if (rightToLeft)
            grid->SetFlowDirection(FlowDirection::RightToLeft);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
        const auto detachModel = wil::scope_exit([&] { grid->SetModel(nullptr); });
        static_cast<void>(CaptureWindow(window, "the grid paints its trimmed cells"));
        Require(grid->DebugGetTextLayoutStatistics().displayCapacity > 0u, "the paint laid out omitted tails");
        window.Host().RefreshAccessibilitySnapshot();
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "the window exposes an accessibility provider");
        const GridCellProviders providers = ResolveGridCellProviders(window, *rootProvider.get(), 60.0f, 16.0f);
        Require(providers.rows.size() == values.size(), "every row of the grid has a provider");
        for (size_t row = 0u; row < values.size(); ++row)
        {
            Require(providers.rows[row].size() == values[row].size(), "every cell of a row has a provider");
            for (size_t column = 0u; column < values[row].size(); ++column)
            {
                const CellTexts texts   = ReadCellTexts(*providers.rows[row][column].get());
                const std::string where = std::format("{} grid, cell {},{}", rightToLeft ? "right-to-left" : "left-to-right", row, column);
                if (texts.name != values[row][column])
                    std::cerr << "    [DIFFERENCE] " << where << ": Name holds " << texts.name.size() << " units, the value " << values[row][column].size()
                              << '\n';
                Require(texts.name == values[row][column], (where + ": the Name is the complete value").c_str());
                Require(texts.value == values[row][column], (where + ": the Value property is the complete value").c_str());
                Require(texts.patternValue == values[row][column], (where + ": the ValuePattern is the complete value").c_str());
            }
        }
    }
}

// A cell the viewport cuts keeps its complete Name and Value and exposes the rectangle of what shows of it: the bounding rectangle
// is the viewport-clipped cell (the same pixels a hit test answers for), at several scroll offsets cutting the cell at its top, bottom,
// left and right; a cell scrolled out of view says it is offscreen and has no rectangle. The value of 100,000 units in one of them
// is complete too.
void TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    constexpr size_t rowCount = 6u;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < rowCount; ++row)
        cells.push_back({std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production prévue pour la semaine "
                                     L"prochaine, puis confirmer auprès de l’équipe.",
                                     row),
                         std::format(L"État {}.1 : synchronisation interrompue après trois tentatives, consulter le journal détaillé pour connaître la cause "
                                     L"exacte de l’échec.\r\nNouvelle tentative prévue.",
                                     row)});
    cells[2][0] = RepeatToUnits(L"mot suivant très long ", 100000u);
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    ConfigureHostPlace(window, 96u, false, Density::Standard, D2D1::SizeF(480.0f, 280.0f));
    TextTableModel model(cells, {400.0f, 400.0f});
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"Grid witness");
    label->SetBounds(D2D1::RectF(360.0f, 20.0f, 470.0f, 44.0f));
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetRowHeightDip(64.0f);
    grid->SetLineClamp(2u);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));
    const auto detachModel = wil::scope_exit([&] { grid->SetModel(nullptr); });
    const auto toScreen    = [&](const D2D1_RECT_F& rectDip)
    {
        POINT topLeft{static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.left))),
                      static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.top)))};
        POINT bottomRight{static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.right))),
                          static_cast<LONG>(std::lround(window.Host().DipsToPixels(rectDip.bottom)))};
        Require(ClientToScreen(window.Hwnd(), &topLeft) != FALSE && ClientToScreen(window.Hwnd(), &bottomRight) != FALSE,
                "the client rectangle maps to the screen");
        return UiaRect{static_cast<double>(topLeft.x),
                       static_cast<double>(topLeft.y),
                       static_cast<double>(bottomRight.x - topLeft.x),
                       static_cast<double>(bottomRight.y - topLeft.y)};
    };
    const auto describe = [](const UiaRect& rect) { return std::format("({}, {}) {} x {}", rect.left, rect.top, rect.width, rect.height); };
    // Paints the grid as it is scrolled and checks what UI Automation says of every cell it exposes; returns how many cells the
    // viewport cuts at their top.
    const auto verifyCells = [&](const std::string& state)
    {
        static_cast<void>(CaptureWindow(window, "the grid paints scrolled"));
        window.Host().RefreshAccessibilitySnapshot();
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
        rootProvider.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(rootProvider != nullptr, "the window exposes an accessibility provider");
        const GridCellProviders providers = ResolveGridCellProviders(window, *rootProvider.get(), 400.0f, 30.0f);
        Require(! providers.rows.empty(), "the scrolled grid exposes its visible rows");
        size_t cutAtTop = 0u;
        for (size_t visibleRow = 0u; visibleRow < providers.rows.size(); ++visibleRow)
        {
            const size_t row = grid->GetVisibleRowAt(visibleRow).value();
            Require(providers.rows[visibleRow].size() == 2u, "a row exposes both cells, in view or not");
            for (size_t column = 0u; column < 2u; ++column)
            {
                const std::string where               = std::format("{}, cell {},{}", state, row, column);
                IRawElementProviderFragment& provider = *providers.rows[visibleRow][column].get();
                const CellTexts texts                 = ReadCellTexts(provider);
                Require(texts.name == cells[row][column] && texts.value == cells[row][column] && texts.patternValue == cells[row][column],
                        (where + ": the Name, Value and ValuePattern are the complete value").c_str());
                wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
                RequireSucceeded(provider.QueryInterface(IID_PPV_ARGS(simple.put())), "a cell exposes a simple provider");
                const bool offscreen = ReadProviderBoolProperty(*simple.get(), UIA_IsOffscreenPropertyId, "a cell reports whether it is offscreen");
                UiaRect bounds{};
                RequireSucceeded(provider.get_BoundingRectangle(&bounds), "a cell reports its bounding rectangle");
                const auto visible = grid->GetVisibleCellRect(row, column);
                Require(offscreen == ! visible.has_value(), (where + ": the cell is offscreen when the viewport shows none of it").c_str());
                if (! visible)
                {
                    Require(bounds.width == 0.0 && bounds.height == 0.0, (where + ": a cell scrolled out of view has no rectangle").c_str());
                    continue;
                }
                const UiaRect expected  = toScreen(*visible);
                const D2D1_RECT_F whole = grid->GetCellLayoutMetrics(window.Host(), row, column).cellRect;
                std::cout << "UIA " << where << ": bounds " << describe(bounds) << ", visible part " << describe(expected) << ", whole cell "
                          << (whole.right - whole.left) << " x " << (whole.bottom - whole.top) << " DIP\n";
                Require(bounds.left == expected.left && bounds.top == expected.top && bounds.width == expected.width && bounds.height == expected.height,
                        (where + ": the bounding rectangle is the viewport-clipped cell").c_str());
                const bool cut = visible->left > whole.left + 0.5f || visible->top > whole.top + 0.5f || visible->right < whole.right - 0.5f ||
                                 visible->bottom < whole.bottom - 0.5f;
                if (cut)
                    Require(bounds.width * bounds.height < static_cast<double>(window.Host().DipsToPixels(whole.right - whole.left)) *
                                                               static_cast<double>(window.Host().DipsToPixels(whole.bottom - whole.top)),
                            (where + ": a cut cell's rectangle is smaller than the whole cell").c_str());
                cutAtTop += visible->top > whole.top + 0.5f ? 1u : 0u;
                // A point inside the visible part answers with this cell.
                const D2D1_POINT_2F inside = D2D1::Point2F((visible->left + visible->right) * 0.5f, (visible->top + visible->bottom) * 0.5f);
                wil::com_ptr_nothrow<IRawElementProviderFragment> hit = GetProviderAtDipPoint(
                    window.Hwnd(), window.Host(), *rootProvider.get(), inside.x, inside.y, "a point in the visible part of the cell resolves");
                Require(ReadCellTexts(*hit.get()).name == cells[row][column], (where + ": a point in the visible part resolves to the cell").c_str());
            }
        }
        return cutAtTop;
    };
    // The vertical scroll rests on whole rows, so the bottom and side edges cut cells at these offsets.
    for (const auto [verticalDip, horizontalDip] :
         {std::pair{0.0f, 0.0f}, std::pair{64.0f, 40.0f}, std::pair{128.0f, 0.0f}, std::pair{0.0f, 300.0f}, std::pair{128.0f, 380.0f}})
    {
        grid->DebugSetScrollOffsets(verticalDip, horizontalDip);
        static_cast<void>(verifyCells(std::format("scrolled ({}, {})", verticalDip, horizontalDip)));
    }
    // A dragged scrollbar thumb rests between rows, so a row is cut under the header too.
    grid->DebugSetScrollOffsets(0.0f, 0.0f);
    static_cast<void>(CaptureWindow(window, "the grid paints at the top"));
    const D2D1_RECT_F thumb  = grid->DebugGetScrollbarVisualState(window.Host().GetTheme()).verticalThumbRect;
    const D2D1_POINT_2F grab = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
    Require(grid->OnMouseDown(window.Host(), grab, false, 0u), "the vertical scrollbar thumb is grabbed");
    Require(grid->OnMouseMove(window.Host(), D2D1::Point2F(grab.x, grab.y + 9.0f), 0u), "the thumb is dragged");
    Require(verifyCells("under a dragged thumb") > 0u, "the dragged thumb cuts a cell at its top");
    static_cast<void>(grid->OnMouseUp(window.Host(), D2D1::Point2F(grab.x, grab.y + 9.0f), false, 0u));
}

// A window whose only semantic control is a Tree, a Grid or a masked TextField has one element, the window's, which stands for
// that control. What the control exposes of its own (a tree's items, a grid's headers, rows and cells, a field's reveal button)
// are that element's children, and their parent is that element. UI Automation drops an event raised on an element it cannot
// reach from the window through the parents, so an item whose parent chain ends at a second element for its tree (one nobody
// reaches from the window) never reaches a client subscribed to the window. The tests below ask a UI Automation client what it
// sees, as a screen reader does: the walk from the window's element to the items and back, and the events raised on them. Each
// of them has a twin for a window where the control has a sibling, in which it has an element of its own: what a client sees of
// the control's parts is the same in both, apart from which element stands for the control.

[[nodiscard]] bool SameComObject(IUnknown* first, IUnknown* second)
{
    wil::com_ptr_nothrow<IUnknown> a;
    wil::com_ptr_nothrow<IUnknown> b;
    return first && second && SUCCEEDED(first->QueryInterface(IID_PPV_ARGS(a.put()))) && SUCCEEDED(second->QueryInterface(IID_PPV_ARGS(b.put()))) &&
           a.get() == b.get();
}

// A UI Automation client of one test window, with what the tests ask of it. `root` is the window's element.
class SingleControlClient final
{
public:
    explicit SingleControlClient(AttachedHostWindow& window, UiaTest::Subscription subscription = {})
        : client(window.Hwnd(), std::move(subscription), [&window] { window.PumpMessages(); }),
          root(client.Root())
    {
    }

    // An expectation of what the client sees. A failed one first reports the tree the client walks and the events it heard.
    void Expect(bool condition, const char* message)
    {
        if (! condition)
        {
            std::cerr << "    [UIA] the client walks from the window's element:\n";
            for (const wchar_t unit : client.Dump(root))
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            client.PrintEvents();
        }
        Require(condition, message);
    }

    [[nodiscard]] std::wstring Name(UiaTest::ElementId element)
    {
        return client.Describe(element).name;
    }

    [[nodiscard]] std::vector<std::wstring> Names(const std::vector<UiaTest::ElementId>& elements)
    {
        std::vector<std::wstring> names;
        for (const UiaTest::ElementId element : elements)
            names.push_back(Name(element));
        return names;
    }

    // The names of `element` and of what follows it by next (or previous) sibling.
    [[nodiscard]] std::vector<std::wstring> Walk(std::optional<UiaTest::ElementId> element, UiaTest::Direction direction)
    {
        std::vector<std::wstring> names;
        for (size_t guard = 0u; element && guard < 64u; ++guard)
        {
            names.push_back(Name(*element));
            element = client.Navigate(*element, direction);
        }
        return names;
    }

    // Whether UI Automation takes the parent of `element` for `parent`.
    [[nodiscard]] bool HasParent(UiaTest::ElementId element, UiaTest::ElementId parent)
    {
        const std::optional<UiaTest::ElementId> found = client.Navigate(element, UiaTest::Direction::Parent);
        return found && client.Same(*found, parent);
    }

    UiaTest::Client client;
    UiaTest::ElementId root;
};

void FillTreeModel(MutableTreeModel& model)
{
    model.SetVisibleItems({DxUi::TreeItemData{.id = 1u, .text = L"Général"},
                           DxUi::TreeItemData{.id = 2u, .text = L"Volets"},
                           DxUi::TreeItemData{.id = 3u, .text = L"Afficheurs"}});
}

void ConfigureCategoriesTree(DxUi::Tree& tree, MutableTreeModel& model)
{
    tree.SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 96.0f));
    tree.SetModel(&model);
    tree.SetAccessibleName(L"Catégories");
    tree.SetSelectedItemId(2u);
}

// The window of a tree and nothing else.
struct SingleTreeWindow final
{
    SingleTreeWindow()
    {
        FillTreeModel(model);
        auto control = std::make_unique<DxUi::Tree>();
        ConfigureCategoriesTree(*control, model);
        tree = control.get();
        window.Host().SetRoot(std::move(control));
    }

    MutableTreeModel model; // Before the window: the tree it holds ends first.
    AttachedHostWindow window;
    DxUi::Tree* tree = nullptr;
};

// The window of a tree and a button.
struct TreeBesideAButtonWindow final
{
    TreeBesideAButtonWindow()
    {
        FillTreeModel(model);
        auto panel = std::make_unique<DxUi::Panel>();
        tree       = panel->AddChild<DxUi::Tree>();
        ConfigureCategoriesTree(*tree, model);
        panel->AddChild<DxUi::Button>(L"Appliquer")->SetBounds(D2D1::RectF(0.0f, 104.0f, 120.0f, 136.0f));
        window.Host().SetRoot(std::move(panel));
    }

    MutableTreeModel model;
    AttachedHostWindow window;
    DxUi::Tree* tree = nullptr;
};

class StatusGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return std::size(kRows);
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = columnIndex == 0u ? L"name" : L"status";
        column.title    = columnIndex == 0u ? L"Nom" : L"État";
        column.widthDip = 120.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = columnIndex == 0u ? kRows[rowIndex].name : kRows[rowIndex].status;
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return kRows[rowIndex].id;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        for (size_t rowIndex = 0u; rowIndex < std::size(kRows); ++rowIndex)
        {
            if (kRows[rowIndex].id == rowId)
                return rowIndex;
        }
        return std::nullopt;
    }

private:
    struct Row
    {
        uint64_t id;
        const wchar_t* name;
        const wchar_t* status;
    };

    static constexpr Row kRows[] = {{10u, L"Alpha", L"Prête"}, {20u, L"Beta", L"Occupée"}, {30u, L"Gamma", L"Libre"}};
};

void ConfigureResultsGrid(DxUi::Grid& grid, StatusGridModel& model)
{
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 160.0f));
    grid.SetModel(&model);
    grid.SetAccessibleName(L"Résultats");
}

// The window of a grid and nothing else.
struct SingleGridWindow final
{
    SingleGridWindow()
    {
        auto control = std::make_unique<DxUi::Grid>();
        ConfigureResultsGrid(*control, model);
        grid = control.get();
        window.Host().SetRoot(std::move(control));
    }

    StatusGridModel model;
    AttachedHostWindow window;
    DxUi::Grid* grid = nullptr;
};

// The window of a grid and a button.
struct GridBesideAButtonWindow final
{
    GridBesideAButtonWindow()
    {
        auto panel = std::make_unique<DxUi::Panel>();
        grid       = panel->AddChild<DxUi::Grid>();
        ConfigureResultsGrid(*grid, model);
        panel->AddChild<DxUi::Button>(L"Appliquer")->SetBounds(D2D1::RectF(0.0f, 168.0f, 120.0f, 200.0f));
        window.Host().SetRoot(std::move(panel));
    }

    StatusGridModel model;
    AttachedHostWindow window;
    DxUi::Grid* grid = nullptr;
};

// What a client sees of a tree and its items; `tree` is the tree's element: the window's in a window the tree fills, its own in
// a window it shares.
void ExpectClientSeesTreeItems(SingleControlClient& walk, UiaTest::ElementId tree)
{
    using UiaTest::Direction;
    const UiaTest::ElementInfo info = walk.client.Describe(tree);
    walk.Expect(info.controlType == UIA_TreeControlTypeId && info.name == L"Catégories", "the client reaches the tree's element");

    const std::vector<std::wstring> items{L"Général", L"Volets", L"Afficheurs"};
    const std::vector<std::wstring> reversed{L"Afficheurs", L"Volets", L"Général"};
    const std::optional<UiaTest::ElementId> first = walk.client.Navigate(tree, Direction::FirstChild);
    walk.Expect(first && walk.Name(*first) == items.front(), "the first child of the tree's element is the tree's first item");
    const std::optional<UiaTest::ElementId> last = walk.client.Navigate(tree, Direction::LastChild);
    walk.Expect(last && walk.Name(*last) == items.back(), "the last child of the tree's element is the tree's last item");
    walk.Expect(walk.Walk(first, Direction::NextSibling) == items, "the items follow one another by next sibling");
    walk.Expect(walk.Walk(last, Direction::PreviousSibling) == reversed, "the items follow one another by previous sibling");
    const std::vector<UiaTest::ElementId> children = walk.client.Children(tree);
    walk.Expect(walk.Names(children) == items, "a search of the tree element's children finds the tree's items");
    for (const UiaTest::ElementId item : children)
    {
        walk.Expect(walk.client.Describe(item).controlType == UIA_TreeItemControlTypeId, "a child of the tree's element is a tree item");
        walk.Expect(walk.HasParent(item, tree), "the parent of a tree item is the tree's element");
        const std::optional<UiaTest::ElementId> container = walk.client.SelectionContainer(item);
        walk.Expect(container && walk.client.Same(*container, tree), "the selection container of a tree item is the tree's element");
    }
    const std::vector<UiaTest::ElementId> selection = walk.client.Selection(tree);
    walk.Expect(selection.size() == 1u && walk.Name(selection.front()) == L"Volets" && walk.HasParent(selection.front(), tree),
                "the selected item the tree's element reports is a child of that element");
}

// What a client sees of a grid and its parts; `grid` is the grid's element.
void ExpectClientSeesGridParts(SingleControlClient& walk, UiaTest::ElementId grid, DxUi::Grid& live)
{
    using UiaTest::Direction;
    const UiaTest::ElementInfo info = walk.client.Describe(grid);
    walk.Expect(info.controlType == UIA_DataGridControlTypeId && info.name == L"Résultats", "the client reaches the grid's element");

    const std::vector<std::wstring> parts{L"Nom", L"État", L"Alpha | Prête", L"Beta | Occupée", L"Gamma | Libre"};
    const std::vector<std::wstring> reversed(parts.rbegin(), parts.rend());
    const std::optional<UiaTest::ElementId> first = walk.client.Navigate(grid, Direction::FirstChild);
    walk.Expect(first && walk.Name(*first) == L"Nom", "the first child of the grid's element is the grid's first header");
    const std::optional<UiaTest::ElementId> last = walk.client.Navigate(grid, Direction::LastChild);
    walk.Expect(last && walk.Name(*last) == L"Gamma | Libre", "the last child of the grid's element is the grid's last row");
    walk.Expect(walk.Walk(first, Direction::NextSibling) == parts, "the headers and the rows follow one another by next sibling");
    walk.Expect(walk.Walk(last, Direction::PreviousSibling) == reversed, "the rows and the headers follow one another by previous sibling");
    const std::vector<UiaTest::ElementId> children = walk.client.Children(grid);
    walk.Expect(walk.Names(children) == parts, "a search of the grid element's children finds the grid's headers and rows");
    for (const UiaTest::ElementId child : children)
    {
        const long controlType = walk.client.Describe(child).controlType;
        walk.Expect(controlType == UIA_HeaderItemControlTypeId || controlType == UIA_DataItemControlTypeId,
                    "a child of the grid's element is a header or a row");
        walk.Expect(walk.HasParent(child, grid), "the parent of a grid header or row is the grid's element");
        if (controlType != UIA_DataItemControlTypeId)
            continue;
        const std::optional<UiaTest::ElementId> container = walk.client.SelectionContainer(child);
        walk.Expect(container && walk.client.Same(*container, grid), "the selection container of a grid row is the grid's element");
    }

    // The cells of the second row: their parent is the row, whose parent is the grid's element.
    const UiaTest::ElementId row                      = children[3];
    const std::optional<UiaTest::ElementId> firstCell = walk.client.Navigate(row, Direction::FirstChild);
    walk.Expect(firstCell && walk.Walk(firstCell, Direction::NextSibling) == std::vector<std::wstring>({L"Beta", L"Occupée"}),
                "a grid row's children are its cells");
    const std::vector<UiaTest::ElementId> cells = walk.client.Children(row);
    walk.Expect(cells.size() == 2u, "a search of a grid row's children finds its cells");
    for (const UiaTest::ElementId cell : cells)
    {
        walk.Expect(walk.HasParent(cell, row), "the parent of a grid cell is its row");
        const std::optional<UiaTest::ElementId> containing = walk.client.ContainingGrid(cell);
        walk.Expect(containing && walk.client.Same(*containing, grid), "the containing grid of a grid cell is the grid's element");
    }

    Require(live.RequestSelectRow(1u, 0u), "the grid selects its second row");
    const std::vector<UiaTest::ElementId> selection = walk.client.Selection(grid);
    walk.Expect(selection.size() == 1u && walk.Name(selection.front()) == L"Beta | Occupée" && walk.HasParent(selection.front(), grid),
                "the selected row the grid's element reports is a child of that element");
}

void TestSingleTreeWindowElementIsTheParentOfItsItems()
{
    SingleTreeWindow test;
    SingleControlClient walk(test.window);
    ExpectClientSeesTreeItems(walk, walk.root);

    // The element a client reaches through a provider call is the window's one canonical element, not an equal copy of it.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(DxUi::CreateWindowHostAccessibilityProvider(test.window.Hwnd()));
    Require(rootProvider != nullptr, "the single-tree window exposes its fragment root");
    const std::optional<D2D1_RECT_F> rect = test.tree->GetVisibleItemHitRect(1u);
    Require(rect.has_value(), "the tree has a rectangle for its second item");
    const wil::com_ptr_nothrow<IRawElementProviderFragment> item = GetProviderAtDipPoint(test.window.Hwnd(),
                                                                                         test.window.Host(),
                                                                                         *rootProvider.get(),
                                                                                         (rect->left + rect->right) * 0.5f,
                                                                                         (rect->top + rect->bottom) * 0.5f,
                                                                                         "a tree item is found by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> parent;
    RequireSucceeded(item->Navigate(NavigateDirection_Parent, parent.put()), "a tree item navigates to its parent");
    Require(parent != nullptr && SameComObject(parent.get(), rootProvider.get()), "the parent of a tree item is the window's canonical element");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fragmentRoot;
    RequireSucceeded(item->get_FragmentRoot(fragmentRoot.put()), "a tree item names its fragment root");
    Require(SameComObject(fragmentRoot.get(), rootProvider.get()), "the fragment root of a tree item is the window's canonical element");
}

// The window's element outlives the tree it stands for: the items of a replacement tree are elements of their own, those of the
// replaced tree report that they are gone, and a client that listens for structure changes hears that it must navigate again.
void TestSingleTreeWindowReplacementTreeGetsItemElementsOfItsOwn()
{
    using UiaTest::Direction;
    SingleTreeWindow test;
    UiaTest::Subscription subscription;
    subscription.structure = true;
    SingleControlClient walk(test.window, std::move(subscription));
    const std::optional<UiaTest::ElementId> oldFirst = walk.client.Navigate(walk.root, Direction::FirstChild);
    walk.Expect(oldFirst && walk.Name(*oldFirst) == L"Général", "the first child of the window's element is the tree's first item");

    auto replacement = std::make_unique<DxUi::Tree>();
    ConfigureCategoriesTree(*replacement, test.model);
    test.window.Host().SetRoot(std::move(replacement)); // The tree the fixture points at is gone with it.
    UiaTest::ElementInfo gone;
    walk.Expect(walk.client.TryDescribe(*oldFirst, gone) == UIA_E_ELEMENTNOTAVAILABLE, "an item of the replaced tree reports that it is gone");
    const std::optional<UiaTest::ElementId> newFirst = walk.client.Navigate(walk.root, Direction::FirstChild);
    walk.Expect(newFirst && walk.Name(*newFirst) == L"Général" && ! walk.client.Same(*newFirst, *oldFirst),
                "the replacement tree's first item is an element of its own");
    walk.Expect(newFirst && walk.HasParent(*newFirst, walk.root), "the parent of the replacement tree's item is the window's element");
    walk.Expect(walk.client.WaitForEvent(
                    [](const UiaTest::HeardEvent& heard)
    {
        return heard.kind == UiaTest::EventKind::Structure && heard.id == StructureChangeType_ChildrenInvalidated && heard.controlType == UIA_TreeControlTypeId;
    }),
                "a client subscribed to the window hears that the window's children were invalidated");
}

void TestTreeBesideAnotherControlIsTheParentOfItsItems()
{
    TreeBesideAButtonWindow test;
    SingleControlClient walk(test.window);
    const std::optional<UiaTest::ElementId> tree = walk.client.Navigate(walk.root, UiaTest::Direction::FirstChild);
    walk.Expect(tree.has_value() && walk.HasParent(*tree, walk.root), "the tree has an element of its own, a child of the window's");
    ExpectClientSeesTreeItems(walk, *tree);
}

void TestSingleGridWindowElementIsTheParentOfItsHeadersRowsAndCells()
{
    SingleGridWindow test;
    SingleControlClient walk(test.window);
    ExpectClientSeesGridParts(walk, walk.root, *test.grid);

    // The element a client reaches through a provider call is the window's one canonical element, not an equal copy of it.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(DxUi::CreateWindowHostAccessibilityProvider(test.window.Hwnd()));
    Require(rootProvider != nullptr, "the single-grid window exposes its fragment root");
    const std::optional<D2D1_RECT_F> cellRect = test.grid->GetVisibleCellRect(1u, 0u);
    Require(cellRect.has_value(), "the grid has a rectangle for a cell of its second row");
    const wil::com_ptr_nothrow<IRawElementProviderFragment> cell = GetProviderAtDipPoint(test.window.Hwnd(),
                                                                                         test.window.Host(),
                                                                                         *rootProvider.get(),
                                                                                         (cellRect->left + cellRect->right) * 0.5f,
                                                                                         (cellRect->top + cellRect->bottom) * 0.5f,
                                                                                         "a grid cell is found by point");
    wil::com_ptr_nothrow<IRawElementProviderFragment> row;
    RequireSucceeded(cell->Navigate(NavigateDirection_Parent, row.put()), "a grid cell navigates to its row");
    Require(row != nullptr, "a grid cell has a row");
    wil::com_ptr_nothrow<IRawElementProviderFragment> parent;
    RequireSucceeded(row->Navigate(NavigateDirection_Parent, parent.put()), "a grid row navigates to its parent");
    Require(parent != nullptr && SameComObject(parent.get(), rootProvider.get()), "the parent of a grid row is the window's canonical element");
}

void TestGridBesideAnotherControlIsTheParentOfItsHeadersRowsAndCells()
{
    GridBesideAButtonWindow test;
    SingleControlClient walk(test.window);
    const std::optional<UiaTest::ElementId> grid = walk.client.Navigate(walk.root, UiaTest::Direction::FirstChild);
    walk.Expect(grid.has_value() && walk.HasParent(*grid, walk.root), "the grid has an element of its own, a child of the window's");
    ExpectClientSeesGridParts(walk, *grid, *test.grid);
}

void TestSingleMaskedFieldWindowElementIsTheParentOfItsRevealButton()
{
    using namespace DxUi;
    using UiaTest::Direction;
    AttachedHostWindow window;
    auto field = std::make_unique<TextField>(L"secret");
    field->SetMasked(true);
    field->SetAccessibleName(L"Mot de passe");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    TextField* const liveField = field.get();
    window.Host().SetRoot(std::move(field));
    window.Host().SetFocusControl(liveField); // The reveal button shows while its masked field has focus.
    SingleControlClient walk(window);
    walk.Expect(walk.client.Describe(walk.root).controlType == UIA_EditControlTypeId, "the window's element stands for the field");

    const std::optional<UiaTest::ElementId> first = walk.client.Navigate(walk.root, Direction::FirstChild);
    walk.Expect(first && walk.client.Describe(*first).controlType == UIA_ButtonControlTypeId, "the first child of the window's element is the reveal button");
    const std::optional<UiaTest::ElementId> last = walk.client.Navigate(walk.root, Direction::LastChild);
    walk.Expect(first && last && walk.client.Same(*first, *last), "the reveal button is the only child of the window's element");
    walk.Expect(first && walk.HasParent(*first, walk.root), "the parent of the reveal button is the window's element");
    walk.Expect(first && ! walk.client.Navigate(*first, Direction::NextSibling) && ! walk.client.Navigate(*first, Direction::PreviousSibling),
                "the reveal button has no sibling");
}

// What a client heard that matches the sender's control type and name, and the kind of event (and for a property its value).
[[nodiscard]] std::function<bool(const UiaTest::HeardEvent&)> HeardAutomationEvent(EVENTID eventId, long controlType, std::wstring name)
{
    return [=](const UiaTest::HeardEvent& heard)
    {
        return heard.kind == UiaTest::EventKind::Automation && heard.id == static_cast<long>(eventId) && heard.controlType == controlType && heard.name == name;
    };
}

[[nodiscard]] wil::com_ptr_nothrow<IRawElementProviderSimple> SimpleProviderOf(const wil::com_ptr_nothrow<IRawElementProviderFragment>& fragment)
{
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(fragment.query_to(simple.put()), "an element is a simple provider");
    return simple;
}

// A click and keys reach a test window as its own messages, at a point given in DIPs and with the modifiers the system reports
// with a press (MK_CONTROL, MK_SHIFT) or the key held around a key (VK_CONTROL, VK_SHIFT): nothing goes through the desktop, so
// the window needs no foreground.
void SendClick(AttachedHostWindow& window, D2D1_POINT_2F pointDip, WPARAM modifiers = 0u)
{
    const LPARAM point = MAKELPARAM(static_cast<int>(std::lround(window.Host().DipsToPixels(pointDip.x))),
                                    static_cast<int>(std::lround(window.Host().DipsToPixels(pointDip.y))));
    static_cast<void>(SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON | modifiers, point));
    static_cast<void>(SendMessageW(window.Hwnd(), WM_LBUTTONUP, modifiers, point));
}

void SendKey(AttachedHostWindow& window, UINT virtualKey, UINT heldKey = 0u)
{
    if (heldKey != 0u)
        static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYDOWN, heldKey, 0));
    static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYDOWN, virtualKey, 0));
    static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYUP, virtualKey, 0));
    if (heldKey != 0u)
        static_cast<void>(SendMessageW(window.Hwnd(), WM_KEYUP, heldKey, 0));
}

// Gives a test window the client area of a size in DIPs, which a root that fills it takes.
void SizeClientAreaInDips(AttachedHostWindow& window, float widthDip, float heightDip)
{
    RECT rect{0, 0, static_cast<LONG>(std::ceil(window.Host().DipsToPixels(widthDip))), static_cast<LONG>(std::ceil(window.Host().DipsToPixels(heightDip)))};
    const DWORD style   = static_cast<DWORD>(GetWindowLongPtrW(window.Hwnd(), GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE));
    Require(AdjustWindowRectEx(&rect, style, FALSE, exStyle) != FALSE, "the test window's frame adjusts to a client area");
    window.AllowOuterSizeBeyondDesktop(SIZE{rect.right - rect.left, rect.bottom - rect.top});
    Require(SetWindowPos(window.Hwnd(), nullptr, -32000, -32000, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "the test window takes its client area");
    window.PumpMessages();
}

// Requires that the client heard `expected`, no more and no less, for a step of a test: what HearSelectionEvents returned.
void ExpectHeard(SingleControlClient& walk, const std::vector<std::wstring>& heard, std::vector<std::wstring> expected, const char* step)
{
    expected = UiaTest::SortedEvents(std::move(expected));
    if (heard == expected)
        return;
    const std::string message = std::string(step) + ": the client heard" + UiaTest::DescribeEventList(heard) + ", not" + UiaTest::DescribeEventList(expected);
    walk.Expect(false, message.c_str());
}

// The selection events of a tree with a single selection, as a client subscribed to the window hears them from the library: a
// click, a key or the application moving the selection to another item is that item being selected (which says that the other
// left the selection; each item whose state changed also reports its IsSelected change); clearing the selection removes its
// item; a republish that changed no selection is silent; and a selected item that leaves the tree has no element to name, so it
// is left to the item that became the selection in the same change, or else the tree's selection is invalidated. The tree fills
// the window, whose element stands for it, or has a button beside it: a client hears the same events, from the same items and
// the same tree, in both.
void ExpectClientHearsSingleSelectionTreeEvents(AttachedHostWindow& window, DxUi::Tree& tree, MutableTreeModel& model)
{
    SingleControlClient walk(window, UiaTest::SelectionEventsSubscription());
    const auto hear  = [&](size_t expected, const std::function<void()>& action) { return UiaTest::HearSelectionEvents(walk.client, expected, action); };
    const auto item  = [](std::wstring_view what, std::wstring_view name) { return std::wstring(what) + L" TreeItem '" + std::wstring(name) + L"'"; };
    const auto click = [&](size_t visibleIndex)
    {
        const std::optional<D2D1_RECT_F> rect = tree.GetVisibleItemHitRect(visibleIndex);
        Require(rect.has_value(), "the tree has a rectangle for the item");
        SendClick(window, D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f));
    };
    Require(tree.GetSelectedItemId() == 2u && ! tree.MultiSelectEnabled(), "the tree selects one item, Volets");

    ExpectHeard(walk,
                hear(3u, [&] { click(2u); }),
                {item(L"Selected", L"Afficheurs"), item(L"IsSelected:true", L"Afficheurs"), item(L"IsSelected:false", L"Volets")},
                "a click on another item");
    Require(tree.GetSelectedItemId() == 3u, "the click selected the item");

    window.Host().SetFocusControl(&tree);
    ExpectHeard(walk,
                hear(3u, [&] { SendKey(window, VK_UP); }),
                {item(L"Selected", L"Volets"), item(L"IsSelected:true", L"Volets"), item(L"IsSelected:false", L"Afficheurs")},
                "Up");
    Require(tree.GetSelectedItemId() == 2u, "Up selected the item above");

    ExpectHeard(walk,
                hear(3u, [&] { tree.SetSelectedItemId(1u); }),
                {item(L"Selected", L"Général"), item(L"IsSelected:true", L"Général"), item(L"IsSelected:false", L"Volets")},
                "the application selecting an item");

    ExpectHeard(walk,
                hear(2u, [&] { tree.SetSelectedItemId(std::nullopt); }),
                {item(L"Removed", L"Général"), item(L"IsSelected:false", L"Général")},
                "the application clearing the selection");

    ExpectHeard(walk,
                hear(2u, [&] { tree.SetSelectedItemId(3u); }),
                {item(L"Selected", L"Afficheurs"), item(L"IsSelected:true", L"Afficheurs")},
                "selecting an item of an empty selection");

    ExpectHeard(walk, hear(0u, [&] { window.Host().RefreshAccessibilitySnapshot(); }), {}, "republishing an unchanged selection");

    // An item that left the tree cannot be named, but when another became the selection in the same change, that item's being
    // selected says the other left it, as for a grid's row out of view.
    ExpectHeard(walk,
                hear(2u,
                     [&]
    {
        model.SetVisibleItems({DxUi::TreeItemData{.id = 1u, .text = L"Général"}, DxUi::TreeItemData{.id = 2u, .text = L"Volets"}});
        tree.SetSelectedItemId(1u);
        tree.NotifyDataChanged();
    }),
                {item(L"Selected", L"Général"), item(L"IsSelected:true", L"Général")},
                "selecting an item as the selected one leaves the tree");
    Require(tree.GetSelectedItemId() == 1u, "the tree selects the item the application chose");
    ExpectHeard(walk,
                hear(1u,
                     [&]
    {
        model.SetVisibleItems({DxUi::TreeItemData{.id = 2u, .text = L"Volets"}});
        tree.NotifyDataChanged();
    }),
                {L"Invalidated Tree 'Catégories'"},
                "the selected item leaving the tree");
    Require(! tree.GetSelectedItemId().has_value(), "the tree has no selected item once it left");
}

void TestSingleTreeWindowItemEventsReachAClientSubscribedToTheWindow()
{
    SingleTreeWindow test;
    ExpectClientHearsSingleSelectionTreeEvents(test.window, *test.tree, test.model);
}

void TestTreeBesideAnotherControlItemEventsReachAClientSubscribedToTheWindow()
{
    TreeBesideAButtonWindow test;
    ExpectClientHearsSingleSelectionTreeEvents(test.window, *test.tree, test.model);
}

// Numbered rows in one column, "Ligne 1" to "Ligne n" with the stable id 1000 + n, from which a test removes a row.
class NumberedRowsGridModel final : public DxUi::IGridModel
{
public:
    explicit NumberedRowsGridModel(size_t count)
    {
        for (size_t number = 1u; number <= count; ++number)
            _rows.push_back(number);
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows.size();
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Nom";
        column.widthDip = 200.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = L"Ligne " + std::to_wstring(_rows[rowIndex]);
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return IdOf(_rows[rowIndex]);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        for (size_t rowIndex = 0u; rowIndex < _rows.size(); ++rowIndex)
        {
            if (IdOf(_rows[rowIndex]) == rowId)
                return rowIndex;
        }
        return std::nullopt;
    }

    [[nodiscard]] static uint64_t IdOf(size_t number) noexcept
    {
        return 1000u + number;
    }

    // The ids of the rows numbered `first` to `last`, in the model's order.
    [[nodiscard]] std::vector<uint64_t> Ids(size_t first, size_t last) const
    {
        std::vector<uint64_t> ids;
        for (size_t number = first; number <= last; ++number)
            ids.push_back(IdOf(number));
        return ids;
    }

    void RemoveRow(size_t number)
    {
        std::erase(_rows, number);
    }

private:
    std::vector<size_t> _rows; // The number of each row, in order.
};

// The window of a grid of thirty rows in its default extended selection: alone, so that the window's element stands for it, or
// beside a button. Its client area is set in DIPs, so that the grid shows its first nine rows or more at any scale, and its last
// rows are off screen.
struct SelectionGridWindow final
{
    explicit SelectionGridWindow(bool fillsItsWindow)
    {
        SizeClientAreaInDips(window, 280.0f, 360.0f);
        const auto configure = [&](DxUi::Grid& control)
        {
            control.SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 300.0f));
            control.SetModel(&model);
            control.SetAccessibleName(L"Résultats");
            grid = &control;
        };
        if (fillsItsWindow)
        {
            auto control = std::make_unique<DxUi::Grid>();
            configure(*control);
            window.Host().SetRoot(std::move(control)); // It takes the whole client area.
            return;
        }
        auto panel = std::make_unique<DxUi::Panel>();
        configure(*panel->AddChild<DxUi::Grid>());
        panel->AddChild<DxUi::Button>(L"Appliquer")->SetBounds(D2D1::RectF(0.0f, 308.0f, 120.0f, 340.0f));
        window.Host().SetRoot(std::move(panel));
    }

    NumberedRowsGridModel model{30u}; // Before the window: the grid it holds ends first.
    AttachedHostWindow window;
    DxUi::Grid* grid = nullptr;
};

// The selection events of a grid's rows, as a client subscribed to the window hears them from the library. A row the selection
// becomes alone is selected, which says that the others left it; rows that Ctrl+click, Shift+click, clearing or the application
// add or remove are each added or removed; each row whose state changed reports its IsSelected change; more than 20 changes, or a
// row that left the selection and has no element (out of view, or gone from the grid), are one invalidation of the grid's
// selection; and a republish that changed no selection is silent. What a client hears is the same whether the window's element
// stands for the grid or the grid has an element of its own. A cell is a child of its row: an event raised on it reaches the
// client too.
void ExpectClientHearsGridSelectionEvents(SelectionGridWindow& test)
{
    SingleControlClient walk(test.window, UiaTest::SelectionEventsSubscription());
    DxUi::Grid& grid                   = *test.grid;
    const NumberedRowsGridModel& model = test.model;
    const auto hear = [&](size_t expected, const std::function<void()>& action) { return UiaTest::HearSelectionEvents(walk.client, expected, action); };
    const auto row  = [](std::wstring_view what, size_t number) { return std::wstring(what) + L" DataItem 'Ligne " + std::to_wstring(number) + L"'"; };
    const auto rows = [&](std::wstring_view what, size_t first, size_t last)
    {
        std::vector<std::wstring> events;
        for (size_t number = first; number <= last; ++number)
            events.push_back(row(what, number));
        return events;
    };
    const auto both = [](std::vector<std::wstring> first, const std::vector<std::wstring>& second)
    {
        first.insert(first.end(), second.begin(), second.end());
        return first;
    };
    const std::wstring invalidated = L"Invalidated DataGrid 'Résultats'";
    const auto click               = [&](size_t number, WPARAM modifiers = 0u)
    {
        const std::optional<size_t> rowIndex  = model.FindRowByStableId(NumberedRowsGridModel::IdOf(number));
        const std::optional<D2D1_RECT_F> cell = rowIndex ? grid.GetVisibleCellRect(rowIndex.value(), 0u) : std::nullopt;
        Require(cell.has_value(), "the row to click is on screen");
        SendClick(test.window, D2D1::Point2F((cell->left + cell->right) * 0.5f, (cell->top + cell->bottom) * 0.5f), modifiers);
    };
    // The application's own selection: the rows `first` to `last` (in the model's order), then a republish.
    const auto select = [&](size_t first, size_t last)
    {
        grid.GetSelectionModel().SetRange(model.Ids(1u, 30u), NumberedRowsGridModel::IdOf(first), NumberedRowsGridModel::IdOf(last));
        grid.RefreshAccessibilitySnapshot();
    };
    Require(! grid.GetVisibleCellRect(model.FindRowByStableId(NumberedRowsGridModel::IdOf(22u)).value(), 0u).has_value(), "the last rows are off screen");

    // One row, by a click on a selection of none, then on another row, then by the keyboard.
    ExpectHeard(walk, hear(2u, [&] { click(1u); }), {row(L"Selected", 1u), row(L"IsSelected:true", 1u)}, "a click on a row of an empty selection");
    ExpectHeard(
        walk, hear(3u, [&] { click(2u); }), {row(L"Selected", 2u), row(L"IsSelected:true", 2u), row(L"IsSelected:false", 1u)}, "a click on another row");
    test.window.Host().SetFocusControl(&grid);
    ExpectHeard(
        walk, hear(3u, [&] { SendKey(test.window, VK_DOWN); }), {row(L"Selected", 3u), row(L"IsSelected:true", 3u), row(L"IsSelected:false", 2u)}, "Down");

    // Several rows: Ctrl+click adds one, Shift+click the range from the anchor (Ligne 3), and Ctrl+click on a selected row removes it.
    ExpectHeard(walk, hear(2u, [&] { click(5u, MK_CONTROL); }), {row(L"Added", 5u), row(L"IsSelected:true", 5u)}, "Ctrl+click");
    ExpectHeard(
        walk,
        hear(6u, [&] { click(7u, MK_SHIFT); }),
        {row(L"Added", 4u), row(L"IsSelected:true", 4u), row(L"Added", 6u), row(L"IsSelected:true", 6u), row(L"Added", 7u), row(L"IsSelected:true", 7u)},
        "Shift+click");
    ExpectHeard(walk, hear(2u, [&] { click(4u, MK_CONTROL); }), {row(L"Removed", 4u), row(L"IsSelected:false", 4u)}, "Ctrl+click on a selected row");

    // Clearing a selection removes each of its rows.
    const auto clear = [&]
    {
        grid.GetSelectionModel().Clear();
        grid.NotifyDataChanged();
    };
    ExpectHeard(walk,
                hear(8u, clear),
                {row(L"Removed", 3u),
                 row(L"Removed", 5u),
                 row(L"Removed", 6u),
                 row(L"Removed", 7u),
                 row(L"IsSelected:false", 3u),
                 row(L"IsSelected:false", 5u),
                 row(L"IsSelected:false", 6u),
                 row(L"IsSelected:false", 7u)},
                "clearing the selection");

    // Selecting every row is more changes than a client is told of one by one, and so is a click that leaves one of them.
    ExpectHeard(walk, hear(1u, [&] { SendKey(test.window, 'A', VK_CONTROL); }), {invalidated}, "Ctrl+A");
    Require(grid.GetSelectionModel().GetCount() == 30u, "Ctrl+A selected every row");
    ExpectHeard(walk, hear(1u, [&] { click(1u); }), {invalidated}, "a click after Ctrl+A");

    // Exactly 20 changes are named one by one, the rows off screen among them (selected rows have elements); 21 are not, however
    // many rows are selected before and after.
    ExpectHeard(walk, hear(40u, [&] { select(1u, 21u); }), both(rows(L"Added", 2u, 21u), rows(L"IsSelected:true", 2u, 21u)), "twenty rows added at once");
    ExpectHeard(walk, hear(1u, [&] { select(22u, 30u); }), {invalidated}, "nine rows replacing twenty-one");

    // A row out of view that leaves the selection has no element a client can read (the grid's rows are virtualized): clearing a
    // selection of such rows is an invalidation, and when a row on screen becomes the whole selection, its being selected says
    // that the row out of view left.
    ExpectHeard(walk, hear(1u, clear), {invalidated}, "clearing a selection out of view");
    ExpectHeard(walk, hear(2u, [&] { select(25u, 25u); }), {row(L"Selected", 25u), row(L"IsSelected:true", 25u)}, "the application selecting a row off screen");
    ExpectHeard(walk, hear(2u, [&] { click(3u); }), {row(L"Selected", 3u), row(L"IsSelected:true", 3u)}, "a click that replaces a selection out of view");

    ExpectHeard(walk, hear(0u, [&] { test.window.Host().RefreshAccessibilitySnapshot(); }), {}, "republishing an unchanged selection");

    // A selected row that leaves the grid cannot be named.
    ExpectHeard(walk,
                hear(1u,
                     [&]
    {
        test.model.RemoveRow(3u);
        grid.NotifyDataChanged();
    }),
                {invalidated},
                "the selected row leaving the grid");
    Require(grid.GetSelectionModel().GetCount() == 0u, "the row that left took the selection with it");

    // A cell is a child of its row: an event raised on it reaches the client too.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> rootProvider;
    rootProvider.attach(DxUi::CreateWindowHostAccessibilityProvider(test.window.Hwnd()));
    Require(rootProvider != nullptr, "the window exposes its fragment root");
    const std::optional<D2D1_RECT_F> cellRect = grid.GetVisibleCellRect(1u, 0u);
    Require(cellRect.has_value(), "the grid has a rectangle for a cell of its second row");
    const auto cell = SimpleProviderOf(GetProviderAtDipPoint(test.window.Hwnd(),
                                                             test.window.Host(),
                                                             *rootProvider.get(),
                                                             (cellRect->left + cellRect->right) * 0.5f,
                                                             (cellRect->top + cellRect->bottom) * 0.5f,
                                                             "a grid cell is found by point"));
    RequireSucceeded(UiaRaiseAutomationEvent(cell.get(), UIA_SelectionItem_ElementSelectedEventId), "raise an event on a grid cell");
    walk.Expect(walk.client.WaitForEvent(HeardAutomationEvent(UIA_SelectionItem_ElementSelectedEventId, UIA_TextControlTypeId, L"Ligne 2")),
                "a client subscribed to the window hears an event raised on a grid cell");
}

void TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow()
{
    SelectionGridWindow test(true);
    ExpectClientHearsGridSelectionEvents(test);
}

void TestGridBesideAnotherControlRowAndCellEventsReachAClientSubscribedToTheWindow()
{
    SelectionGridWindow test(false);
    ExpectClientHearsGridSelectionEvents(test);
}

// When the grid whose selection events are being raised is hidden or replaced, or its host detached, by something that runs
// while they are raised (see UiaTest::SelectionEventInterruption), the raising ends there: the events left are not raised for an
// element that is gone. Each case adds two rows to a selection, four events.
void TestSelectionEventsEndWhenTheControlLeavesWhileTheyAreRaised()
{
    const auto run = [](const char* what, const std::function<void(SelectionGridWindow&, std::unique_ptr<DxUi::Panel>&)>& interrupt)
    {
        SelectionGridWindow test(false);
        auto replacement = std::make_unique<DxUi::Panel>();
        SingleControlClient walk(test.window, UiaTest::SelectionEventsSubscription()); // A client listens, so the host raises them.
        Require(test.grid->RequestSelectRow(0u, 0u), "the grid selects its first row");
        size_t events = 0u;
        {
            UiaTest::SelectionEventInterruption interruption([&] { interrupt(test, replacement); });
            test.grid->GetSelectionModel().SetRange(test.model.Ids(1u, 30u), NumberedRowsGridModel::IdOf(1u), NumberedRowsGridModel::IdOf(3u));
            test.window.Host().RefreshAccessibilitySnapshot(); // The grid may be gone by now.
            events = interruption.Events();
        }
        Require(events == 1u, what);
    };
    run("hiding the grid while its selection events are raised ends them",
        [](SelectionGridWindow& test, std::unique_ptr<DxUi::Panel>&) { test.grid->SetVisible(false); });
    run("replacing the window's root while the grid's selection events are raised ends them",
        [](SelectionGridWindow& test, std::unique_ptr<DxUi::Panel>& replacement) { test.window.Host().SetRoot(std::move(replacement)); });
    run("detaching the host while the grid's selection events are raised ends them",
        [](SelectionGridWindow& test, std::unique_ptr<DxUi::Panel>&) { test.window.Host().Detach(); });
}

// A listening client makes the host raise real selection events. The existing interruption hook plays the message that
// replaces the controls during the first raise; the input/request that published them must then stop using its control.
// Allocate the replacement first so its address cannot be the removed control's, and check only the host afterward.
void ExpectRootReplacementDuringSelectionPublish(AttachedHostWindow& window, const char* scenario, const std::function<void()>& action)
{
    std::cerr << "    [UIA publish lifetime] " << scenario << '\n';
    SingleControlClient walk(window, UiaTest::SelectionEventsSubscription());
    auto replacement                   = std::make_unique<DxUi::Panel>();
    DxUi::Panel* const replacementRoot = replacement.get();
    uint64_t replacementInvalidations  = 0u;
    UiaTest::SelectionEventInterruption interruption([&]
    {
        window.Host().SetRoot(std::move(replacement));
        replacementInvalidations = window.Host().DebugGetInvalidateCount();
    });
    action();
    Require(interruption.Events() == 1u, "the first selection event replaced the root and ended the raising");
    Require(window.Host().GetRoot() == replacementRoot, "the replacement root survives the original selection handler");
    Require(window.Host().GetFocusControl() == nullptr, "the selection handler never focuses a destroyed control");
    Require(window.Host().GetCapturedControl() == nullptr, "the selection handler never captures a destroyed control");
    Require(window.Host().DebugGetInvalidateCount() == replacementInvalidations, "a destroyed control never invalidates the replacement after its publish");
}

struct SelectionPublishTreeWindow final
{
    SelectionPublishTreeWindow(bool fillsItsWindow, bool multiSelect)
    {
        SizeClientAreaInDips(window, 280.0f, 200.0f);
        model.SetVisibleItems({DxUi::TreeItemData{.id = 1u, .text = L"Général", .hasChildren = true, .expanded = true},
                               DxUi::TreeItemData{.id = 2u, .text = L"Volets", .depth = 1u},
                               DxUi::TreeItemData{.id = 3u, .text = L"Afficheurs"}});
        const auto configure = [&](DxUi::Tree& control)
        {
            control.SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 96.0f));
            control.SetModel(&model);
            control.SetDelegate(&delegate);
            control.SetMultiSelectEnabled(multiSelect);
            control.SetSelectedItemId(2u);
            control.SetReorderEnabled(true);
            tree = &control;
        };
        if (fillsItsWindow)
        {
            auto control = std::make_unique<DxUi::Tree>();
            configure(*control);
            window.Host().SetRoot(std::move(control));
        }
        else
        {
            auto panel = std::make_unique<DxUi::Panel>();
            configure(*panel->AddChild<DxUi::Tree>());
            panel->AddChild<DxUi::Button>(L"Appliquer")->SetBounds(D2D1::RectF(0.0f, 104.0f, 120.0f, 136.0f));
            window.Host().SetRoot(std::move(panel));
        }
    }

    MutableTreeModel model;
    RecordingTreeDelegate delegate;
    AttachedHostWindow window;
    DxUi::Tree* tree = nullptr;
};

void TestTreeSelectionPublishReplacementStopsRequests()
{
    enum class Request
    {
        Select,
        Add,
        Remove
    };
    for (const bool fillsItsWindow : {false, true})
    {
        for (const bool multiSelect : {false, true})
        {
            for (const Request request : {Request::Select, Request::Add, Request::Remove})
            {
                // Without multi-select there is no selection to remove an item from.
                if (! multiSelect && request == Request::Remove)
                    continue;
                SelectionPublishTreeWindow test(fillsItsWindow, multiSelect);
                const char* const scenario =
                    request == Request::Select ? "Tree Select" : (request == Request::Add ? "Tree AddToSelection" : "Tree RemoveFromSelection");
                ExpectRootReplacementDuringSelectionPublish(test.window,
                                                            scenario,
                                                            [&]
                {
                    bool survived = true;
                    switch (request)
                    {
                        case Request::Select: survived = test.tree->RequestSelectVisibleItem(2u); break;
                        case Request::Add: survived = test.tree->RequestAddVisibleItemToSelection(2u); break;
                        case Request::Remove: survived = test.tree->RequestRemoveVisibleItemFromSelection(1u); break;
                    }
                    Require(! survived, "a tree selection request reports that its accessibility publish destroyed the tree");
                });
            }
        }
    }
}

void TestTreeSelectionPublishReplacementStopsPointerHandlers()
{
    enum class Action
    {
        Press,
        Expander,
        DoubleClickLeaf,
        DoubleClickGroup,
        ContextMenu
    };
    for (const bool fillsItsWindow : {false, true})
    {
        for (const bool multiSelect : {false, true})
        {
            for (const Action action : {Action::Press, Action::Expander, Action::DoubleClickLeaf, Action::DoubleClickGroup, Action::ContextMenu})
            {
                // A multi-select expander moves focus alone: it changes no selection and raises no selection event.
                if (multiSelect && action == Action::Expander)
                    continue;
                SelectionPublishTreeWindow test(fillsItsWindow, multiSelect);
                const size_t index = action == Action::Expander || action == Action::DoubleClickGroup ? 0u : 2u;
                const auto metrics = test.tree->GetItemLayoutMetrics(test.window.Host(), index);
                const auto rect    = action == Action::Expander ? metrics.expanderRect : metrics.textRect;
                const auto point   = D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f);
                ExpectRootReplacementDuringSelectionPublish(test.window,
                                                            "Tree pointer handler",
                                                            [&]
                {
                    bool handled = false;
                    switch (action)
                    {
                        case Action::Press:
                        case Action::Expander: handled = test.tree->OnMouseDown(test.window.Host(), point, false, 0u); break;
                        case Action::DoubleClickLeaf:
                        case Action::DoubleClickGroup: handled = test.tree->OnMouseDoubleClick(test.window.Host(), point, false, 0u); break;
                        case Action::ContextMenu: handled = test.tree->OnContextMenu(test.window.Host(), false, point); break;
                    }
                    Require(handled, "the pointer input remains handled when selection publishing destroys its tree");
                });
                Require(test.delegate.toggleCount == 0u && test.delegate.invokedCount == 0u && test.delegate.contextMenuCount == 0u,
                        "a destroyed tree neither expands nor invokes nor opens a context menu after publishing selection");
            }
        }
    }
}

// A press on a row of a multi-selection keeps the selection for a drag (it changes no selection, so it raises no selection
// event); the release of a click that never became a drag then selects that row alone, and publishing that may destroy the tree.
void TestTreeSelectionPublishReplacementStopsReorderClick()
{
    for (const bool fillsItsWindow : {false, true})
    {
        SelectionPublishTreeWindow test(fillsItsWindow, true);
        test.tree->SetSelectedItemIds(std::array<uint64_t, 2>{2u, 3u});
        const auto metrics = test.tree->GetItemLayoutMetrics(test.window.Host(), 2u);
        const auto point   = D2D1::Point2F((metrics.textRect.left + metrics.textRect.right) * 0.5f, (metrics.textRect.top + metrics.textRect.bottom) * 0.5f);
        Require(test.tree->OnMouseDown(test.window.Host(), point, false, 0u), "the press on a row of the multi-selection is handled");
        Require(test.window.Host().GetCapturedControl() == test.tree, "the press armed a row drag");
        ExpectRootReplacementDuringSelectionPublish(test.window, "Tree click on a row of a multi-selection", [&] {
            Require(! test.tree->OnMouseUp(test.window.Host(), point, false, 0u), "a click that never became a drag reports unhandled");
        });
        Require(test.delegate.reorderCount == 0u, "a tree destroyed while publishing its collapsed selection commits no reorder");
    }
}

void TestTreeSelectionPublishReplacementStopsKeyboardHandlers()
{
    for (const bool fillsItsWindow : {false, true})
    {
        for (const bool multiSelect : {false, true})
        {
            for (const bool typeAhead : {false, true})
            {
                SelectionPublishTreeWindow test(fillsItsWindow, multiSelect);
                ExpectRootReplacementDuringSelectionPublish(test.window,
                                                            typeAhead ? "Tree type-ahead" : "Tree arrow key",
                                                            [&]
                {
                    const bool handled = typeAhead ? test.tree->OnChar(test.window.Host(), L'A', 0u) : test.tree->OnKeyDown(test.window.Host(), VK_DOWN, 0u);
                    Require(handled, "the keyboard input remains handled when selection publishing destroys its tree");
                });
            }
        }
    }
}

void TestTreeSelectionPublishReplacementStopsSelectAll()
{
    for (const bool fillsItsWindow : {false, true})
    {
        SelectionPublishTreeWindow test(fillsItsWindow, true);
        ExpectRootReplacementDuringSelectionPublish(test.window, "Tree SelectAll", [&] {
            Require(test.tree->OnSelectAll(test.window.Host()), "SelectAll remains handled when its publish destroys the tree");
        });
    }
}

void TestTreeSelectionPublishReplacementStopsModeChange()
{
    for (const bool fillsItsWindow : {false, true})
    {
        SelectionPublishTreeWindow test(fillsItsWindow, true);
        test.tree->SetSelectedItemIds(std::array<uint64_t, 2>{1u, 2u});
        ExpectRootReplacementDuringSelectionPublish(test.window, "Tree multi-select disabled", [&] { test.tree->SetMultiSelectEnabled(false); });
    }
}

void TestTreeDataChangedPublishReplacementSkipsSelectionDelegate()
{
    for (const bool fillsItsWindow : {false, true})
    {
        SelectionPublishTreeWindow test(fillsItsWindow, true);
        test.tree->SetSelectedItemIds(std::array<uint64_t, 2>{1u, 2u});
        test.model.SetVisibleItems({DxUi::TreeItemData{.id = 2u, .text = L"Volets"}, DxUi::TreeItemData{.id = 3u, .text = L"Afficheurs"}});
        ExpectRootReplacementDuringSelectionPublish(test.window, "Tree NotifyDataChanged", [&] { test.tree->NotifyDataChanged(); });
        Require(test.delegate.selectionSetChangedCount == 0u, "model reconciliation never calls the selection delegate after its publish destroyed the tree");
    }
}

void TestGridSelectionPublishReplacementStopsInput()
{
    enum class Action
    {
        Press,
        DoubleClick,
        Key
    };
    for (const bool fillsItsWindow : {false, true})
    {
        for (const Action action : {Action::Press, Action::DoubleClick, Action::Key})
        {
            RecordingGridDelegate delegate;
            SelectionGridWindow test(fillsItsWindow);
            test.grid->SetDelegate(&delegate);
            Require(test.grid->RequestSelectRow(0u, 0u), "the grid initially selects its first row");
            const auto rect = test.grid->GetVisibleCellRect(1u, 0u);
            Require(rect.has_value(), "the grid's next row has a visible cell");
            const auto point = D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
            ExpectRootReplacementDuringSelectionPublish(test.window,
                                                        "Grid selection input",
                                                        [&]
            {
                bool handled = false;
                switch (action)
                {
                    case Action::Press: handled = test.grid->OnMouseDown(test.window.Host(), point, false, 0u); break;
                    case Action::DoubleClick: handled = test.grid->OnMouseDoubleClick(test.window.Host(), point, false, 0u); break;
                    case Action::Key: handled = test.grid->OnKeyDown(test.window.Host(), VK_DOWN, 0u); break;
                }
                Require(handled, "the input remains handled when selection publishing destroys its grid");
            });
            Require(delegate.rowActivatedCount == 0u, "a grid destroyed during selection publishing never activates its row");
        }
    }
}

void TestGridSelectionPublishReplacementStopsSelectAll()
{
    for (const bool fillsItsWindow : {false, true})
    {
        SelectionGridWindow test(fillsItsWindow);
        ExpectRootReplacementDuringSelectionPublish(test.window, "Grid SelectAll", [&] {
            Require(test.grid->OnSelectAll(test.window.Host()), "SelectAll remains handled when its publish destroys the grid");
        });
    }
}

void TestGridGroupSelectionPublishReplacementStopsKeyboardHandler()
{
    for (const bool fillsItsWindow : {false, true})
    {
        GroupedGridModel model(4u);
        model.SetGroups({GroupedGridModel::Group{.stableId = 10u, .title = L"First", .startRowIndex = 0u, .rowCount = 2u},
                         GroupedGridModel::Group{.stableId = 20u, .title = L"Second", .startRowIndex = 2u, .rowCount = 2u}});
        CollapsibleGroupedGridDelegate delegate(model);
        AttachedHostWindow window;
        SizeClientAreaInDips(window, 280.0f, 240.0f);
        std::unique_ptr<DxUi::Control> root;
        DxUi::Grid* grid = nullptr;
        if (fillsItsWindow)
        {
            auto control = std::make_unique<DxUi::Grid>();
            grid         = control.get();
            root         = std::move(control);
        }
        else
        {
            auto panel = std::make_unique<DxUi::Panel>();
            grid       = panel->AddChild<DxUi::Grid>();
            panel->AddChild<DxUi::Button>(L"Apply")->SetBounds(D2D1::RectF(0.0f, 188.0f, 120.0f, 220.0f));
            root = std::move(panel);
        }
        grid->SetModel(&model);
        grid->SetDelegate(&delegate);
        grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 180.0f));
        grid->GetSelectionModel().SetSingle(model.GetStableRowId(0u));
        window.Host().SetRoot(std::move(root));
        ExpectRootReplacementDuringSelectionPublish(window, "Grid keyboard group collapse", [&] {
            Require(grid->OnKeyDown(window.Host(), VK_LEFT, 0u), "group collapse remains handled when publishing destroys the grid");
        });
        Require(model.IsGroupCollapsed(10u) && delegate.groupToggleCount == 1u, "the model acknowledged the collapse before event-time replacement");
    }
}

// A text field is the window's element in the same way, and the text events the library raises for it (its native text-input
// session raises them whenever it synchronizes, without the window holding the foreground) and the elements its text ranges name
// come from that element. The twin has the field beside a button.
void ConfigureRequestField(DxUi::TextField& field)
{
    field.SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 32.0f));
    field.SetAccessibleName(L"Requête");
}

struct SingleFieldWindow final
{
    SingleFieldWindow()
    {
        window.Host().SetTextInputBackend(DxUi::TextInputBackend::Native);
        auto control = std::make_unique<DxUi::TextField>(L"alpha beta");
        ConfigureRequestField(*control);
        field = control.get();
        window.Host().SetRoot(std::move(control));
        window.Host().SetFocusControl(field);
        window.Host().SyncTextInput(field);
    }

    AttachedHostWindow window;
    DxUi::TextField* field = nullptr;
};

struct FieldBesideAButtonWindow final
{
    FieldBesideAButtonWindow()
    {
        window.Host().SetTextInputBackend(DxUi::TextInputBackend::Native);
        auto panel = std::make_unique<DxUi::Panel>();
        field      = panel->AddChild<DxUi::TextField>(L"alpha beta");
        ConfigureRequestField(*field);
        panel->AddChild<DxUi::Button>(L"Chercher")->SetBounds(D2D1::RectF(0.0f, 40.0f, 120.0f, 72.0f));
        window.Host().SetRoot(std::move(panel));
        window.Host().SetFocusControl(field);
        window.Host().SyncTextInput(field);
    }

    AttachedHostWindow window;
    DxUi::TextField* field = nullptr;
};

UiaTest::Subscription TextEventsSubscription()
{
    UiaTest::Subscription subscription;
    subscription.automationEvents = {UIA_Text_TextChangedEventId, UIA_Text_TextSelectionChangedEventId};
    return subscription;
}

// Edits the field and moves its selection, which the host raises UI Automation's text events for, and expects a client subscribed
// to the window to hear each from the field's element.
void ExpectClientHearsTextEventsFromTheFieldsElement(AttachedHostWindow& window,
                                                     DxUi::TextField& field,
                                                     SingleControlClient& walk,
                                                     UiaTest::ElementId fieldElement)
{
    const UiaTest::ElementInfo info = walk.client.Describe(fieldElement);
    walk.Expect(info.controlType == UIA_EditControlTypeId && info.name == L"Requête", "the client reaches the field's element");
    field.SetTextAndNotify(L"alpha beta edited");
    window.Host().SyncTextInput(&field);
    walk.Expect(walk.client.WaitForEvent(HeardAutomationEvent(UIA_Text_TextChangedEventId, UIA_EditControlTypeId, L"Requête")),
                "a client subscribed to the window hears the text change the field raises");
    field.SetSelectionRange(6u, 10u);
    window.Host().SyncTextInput(&field);
    walk.Expect(walk.client.WaitForEvent(HeardAutomationEvent(UIA_Text_TextSelectionChangedEventId, UIA_EditControlTypeId, L"Requête")),
                "a client subscribed to the window hears the selection change the field raises");
}

// The element a Text pattern names as enclosing the field's text is the field's element.
void ExpectTextEnclosingElementIsTheFieldsElement(SingleControlClient& walk, UiaTest::ElementId fieldElement)
{
    const std::optional<UiaTest::ElementId> enclosing = walk.client.TextEnclosingElement(fieldElement);
    walk.Expect(enclosing && walk.client.Same(*enclosing, fieldElement), "the element that encloses the field's text is the field's element");
}

void TestSingleTextFieldWindowElementRaisesTheFieldsTextEvents()
{
    SingleFieldWindow test;
    SingleControlClient walk(test.window, TextEventsSubscription());
    ExpectClientHearsTextEventsFromTheFieldsElement(test.window, *test.field, walk, walk.root);
}

void TestTextFieldBesideAnotherControlRaisesItsTextEventsFromItsOwnElement()
{
    FieldBesideAButtonWindow test;
    SingleControlClient walk(test.window, TextEventsSubscription());
    const std::optional<UiaTest::ElementId> fieldElement = walk.client.Navigate(walk.root, UiaTest::Direction::FirstChild);
    walk.Expect(fieldElement.has_value() && walk.HasParent(*fieldElement, walk.root), "the field has an element of its own, a child of the window's");
    ExpectClientHearsTextEventsFromTheFieldsElement(test.window, *test.field, walk, *fieldElement);
}

void TestSingleTextFieldWindowElementEnclosesTheFieldsTextRanges()
{
    SingleFieldWindow test;
    SingleControlClient walk(test.window);
    ExpectTextEnclosingElementIsTheFieldsElement(walk, walk.root);
}

void TestTextFieldBesideAnotherControlEnclosesItsTextRangesInItsOwnElement()
{
    FieldBesideAButtonWindow test;
    SingleControlClient walk(test.window);
    const std::optional<UiaTest::ElementId> fieldElement = walk.client.Navigate(walk.root, UiaTest::Direction::FirstChild);
    walk.Expect(fieldElement.has_value() && walk.HasParent(*fieldElement, walk.root), "the field has an element of its own, a child of the window's");
    ExpectTextEnclosingElementIsTheFieldsElement(walk, *fieldElement);
}

} // namespace

void RunAccessibilityTests()
{
    DXUI_RUN_TEST(TestNativeAccessibilityFocusCallbackReplacementStopsOriginalAction);
    DXUI_RUN_TEST(TestNativeAccessibilityProvidersRejectReplacementAtSamePath);
    DXUI_RUN_TEST(TestNativeAccessibilityRootRetiresWhenAControlCollapsesIntoIt);
    DXUI_RUN_TEST(TestNativeAccessibilityCollapsedRootRetiresAfterCallbackReplacement);
    DXUI_RUN_TEST(TestNativeAccessibilityPostedSelectRejectsReplacement);
    DXUI_RUN_TEST(TestNativeAccessibilityPostedInvokeRejectsReplacement);
    DXUI_RUN_TEST(TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus);
    DXUI_RUN_TEST(TestSingleTreeWindowElementIsTheParentOfItsItems);
    DXUI_RUN_TEST(TestSingleTreeWindowReplacementTreeGetsItemElementsOfItsOwn);
    DXUI_RUN_TEST(TestTreeBesideAnotherControlIsTheParentOfItsItems);
    DXUI_RUN_TEST(TestSingleGridWindowElementIsTheParentOfItsHeadersRowsAndCells);
    DXUI_RUN_TEST(TestGridBesideAnotherControlIsTheParentOfItsHeadersRowsAndCells);
    DXUI_RUN_TEST(TestSingleMaskedFieldWindowElementIsTheParentOfItsRevealButton);
    DXUI_RUN_TEST(TestSingleTreeWindowItemEventsReachAClientSubscribedToTheWindow);
    DXUI_RUN_TEST(TestTreeBesideAnotherControlItemEventsReachAClientSubscribedToTheWindow);
    DXUI_RUN_TEST(TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow);
    DXUI_RUN_TEST(TestGridBesideAnotherControlRowAndCellEventsReachAClientSubscribedToTheWindow);
    DXUI_RUN_TEST(TestSelectionEventsEndWhenTheControlLeavesWhileTheyAreRaised);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsRequests);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsPointerHandlers);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsReorderClick);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsKeyboardHandlers);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsSelectAll);
    DXUI_RUN_TEST(TestTreeSelectionPublishReplacementStopsModeChange);
    DXUI_RUN_TEST(TestTreeDataChangedPublishReplacementSkipsSelectionDelegate);
    DXUI_RUN_TEST(TestGridSelectionPublishReplacementStopsInput);
    DXUI_RUN_TEST(TestGridSelectionPublishReplacementStopsSelectAll);
    DXUI_RUN_TEST(TestGridGroupSelectionPublishReplacementStopsKeyboardHandler);
    DXUI_RUN_TEST(TestSingleTextFieldWindowElementRaisesTheFieldsTextEvents);
    DXUI_RUN_TEST(TestTextFieldBesideAnotherControlRaisesItsTextEventsFromItsOwnElement);
    DXUI_RUN_TEST(TestSingleTextFieldWindowElementEnclosesTheFieldsTextRanges);
    DXUI_RUN_TEST(TestTextFieldBesideAnotherControlEnclosesItsTextRangesInItsOwnElement);
    DXUI_RUN_TEST(TestNumericStepperStepButtonsAreNamedForAutomation);
    DXUI_RUN_TEST(TestWindowHostStaleElementCannotActOnAReplacementControl);
    DXUI_RUN_TEST(TestWindowHostReplacedTreeItemsGetNewRuntimeIds);
    DXUI_RUN_TEST(TestWindowHostRebuildRaisesStructureInvalidation);
    DXUI_RUN_TEST(TestCollapsedStatusRootChildEventComesFromTheChild);
    DXUI_RUN_TEST(TestWindowHostFocusCallbackThatRemovesTheControlLeavesNoFocus);
    DXUI_RUN_TEST(TestWindowHostRuntimeIdsNeverRepeatAcrossRebuilds);
    DXUI_RUN_TEST(TestWindowHostPublishSkipsAFocusedControlItsPanelRemoved);
    DXUI_RUN_TEST(TestRootlessHostKeepsFocusAfterItsCallback);
    DXUI_RUN_TEST(TestDisclosureButtonExpandCollapsePreservesAcknowledgedState);
    DXUI_RUN_TEST(TestDisclosureNotifiesNativeAutomationClient);
    DXUI_RUN_TEST(TestAccessibilityTextUnitHelperSharesGraphemeWordLineAndFallbackPolicy);
    DXUI_RUN_TEST(TestAccessibilityProviderTraversalSurvivesConcurrentRootReplacement);
    DXUI_RUN_TEST(TestAttachedWindowHostWmGetObjectReturnsAccessibilityProvider);
    DXUI_RUN_TEST(TestAccessibilityRootRuntimeIdIncludesProviderSpecificValues);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesInvokeToggleAndLabeledValuePatterns);
    DXUI_RUN_TEST(TestAccessibilityProviderRefreshesButtonSemanticProperties);
    DXUI_RUN_TEST(TestAccessibilityProviderRefreshesLabelAssociations);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesDirectSemanticRootControls);
    DXUI_RUN_TEST(TestAccessibilityProviderIdentityRetiresAcrossSameHwndReattach);
    DXUI_RUN_TEST(TestAccessibilityLabelOnlyRootDoesNotUseDirectSemanticRootCollapse);
    DXUI_RUN_TEST(TestAccessibilityDirectSemanticRootMatchesUiAutomationClientTree);
    DXUI_RUN_TEST(TestAccessibilityDirectSemanticRootTreeSelectionMatchesUiAutomationClientTree);
    DXUI_RUN_TEST(TestAccessibilityProviderReportsFocusedControl);
    DXUI_RUN_TEST(TestAccessibilityProviderMasksPasswordTextFieldValue);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesMaskedRevealButton);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesTextPatternForTextField);
    DXUI_RUN_TEST(TestAccessibilityTextFieldSimpleRangeBoundingRectanglesUseCaretGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldMultilineRangeFromPointUsesNativeHitTest);
    DXUI_RUN_TEST(TestAccessibilityTextRangeFromPointDispatchesToWindowThread);
    DXUI_RUN_TEST(TestAccessibilityTextFieldMultilineSameLineRangeBoundingRectanglesUseCaretGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldMultilineRangeBoundingRectanglesUseLineCaretGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldWrappedRangeBoundingRectanglesUseVisualLineGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldWrappedCrossLineRangeBoundingRectanglesUseVisualLineGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldWrappedLineMovementUsesVisualLines);
    DXUI_RUN_TEST(TestAccessibilityTextRangeEndpointLineMovementDispatchesToWindowThread);
    DXUI_RUN_TEST(TestAccessibilityTextRangeSpanLineMovementDispatchesToWindowThread);
    DXUI_RUN_TEST(TestAccessibilityTextFieldSingleLineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry);
    DXUI_RUN_TEST(TestAccessibilityTextFieldMultilineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry);
    DXUI_RUN_TEST(TestAccessibilityEditableComboBoxSingleLineMixedBiDiRangeBoundingRectanglesUseDirectWriteGeometry);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesTextPatternForEditableComboBox);
    DXUI_RUN_TEST(TestAccessibilityTextRangeSelectDispatchesToWindowThread);
    DXUI_RUN_TEST(TestAccessibilityTimedOutTextRangeSelectDoesNotExecuteLater);
    DXUI_RUN_TEST(TestAccessibilityTakenTextRangeSelectExecutesOnlyOnce);
    DXUI_RUN_TEST(TestAccessibilityDestroyWithPendingDispatchReturnsCancelled);
    DXUI_RUN_TEST(TestAccessibilityTextRangeBoundingRectanglesDispatchesToWindowThread);
    DXUI_RUN_TEST(TestAccessibilityTextRangeBoundingRectanglesTimeoutKeepsLateHandlerStorageAlive);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesNativeImeTextEditRanges);
    DXUI_RUN_TEST(TestAccessibilityNativeTextInputRaisesTextAndTextEditEventCounters);
    DXUI_RUN_TEST(TestAccessibilityGridSnapshotRebuildMeetsTenThousandRowSelectionBudget);
    DXUI_RUN_TEST(TestAccessibilityProvidersResolveTheirControlWithoutScanningTheTree);
    DXUI_RUN_TEST(TestAccessibilityLookupTablesAgreeWithAScanOfTheRecords);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesTreeAndGridMetadata);
    DXUI_RUN_TEST(TestAccessibilityTreeItemProviderKeepsStableIdentityAcrossReorder);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesTreeItemSelectionAndExpandCollapsePatterns);
    DXUI_RUN_TEST(TestAccessibilityTreeMultiSelectExposesSelectionPatternsAndItemState);
    DXUI_RUN_TEST(TestAccessibilityTreeMultiSelectRaisesSelectionEvents);
    DXUI_RUN_TEST(TestAccessibilityTreeMultiSelectRaisesSelectionEventsWhenItFillsItsWindow);
    DXUI_RUN_TEST(TestAccessibilityOffscreenSelectedGridRowPatternRemainsUsable);
    DXUI_RUN_TEST(TestAccessibilityTrimmedMultilineGridCellKeepsCompleteNameAndValue);
    DXUI_RUN_TEST(TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues);
    DXUI_RUN_TEST(TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesGridRowSelectionPatterns);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesHorizontallyScrolledGridRowStructure);
    DXUI_RUN_TEST(TestAccessibilityProviderPointHitsClipAndTranslateScrollPanelChildren);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesGridCellToggleAndRangePatterns);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesSliderRangeValuePattern);
    DXUI_RUN_TEST(TestAccessibilityProviderExposesSplitterRangeValuePattern);
    DXUI_RUN_TEST(TestAccessibilityStatusRootExposesChildrenAndNonFocusingInvoke);
}
