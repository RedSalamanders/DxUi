#include "Controls.Tests.AccessibilityClientGate.h"

#include "Controls.Tests.AccessibilityFx6.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Embedded.Tests.EmbeddedUiaBridge.h"
#include "GraphicsFixture.h"
#include "Support.Tests.FailureHost.h"
#include "Support.Tests.UiaTestClient.h"
#include "Support/WorkCounters.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
constexpr std::wstring_view kQueryChildPrefix = L"--uia-query-child=";

[[nodiscard]] std::optional<uintptr_t> ParseDecimal(std::wstring_view digits) noexcept
{
    if (digits.empty() || digits.size() > 20u)
        return std::nullopt;
    uintptr_t value = 0u;
    for (const wchar_t ch : digits)
    {
        if (ch < L'0' || ch > L'9')
            return std::nullopt;
        value = value * 10u + static_cast<uintptr_t>(ch - L'0');
    }
    return value;
}
} // namespace

// A UI Automation client in a process of its own that only queries: it reads the names of the elements of the window it is
// given (no subscription), signals the event it inherited and waits to be terminated.
int RunAccessibilityClientGateQueryChild(int argc, wchar_t** argv) noexcept
{
    if (argc != 2 || ! argv[1] || std::wstring_view(argv[1]).rfind(kQueryChildPrefix, 0) != 0)
        return -1;
    const std::wstring_view arguments = std::wstring_view(argv[1]).substr(kQueryChildPrefix.size());
    const size_t comma                = arguments.find(L',');
    const auto window                 = ParseDecimal(arguments.substr(0, comma));
    const auto ready                  = comma == std::wstring_view::npos ? std::nullopt : ParseDecimal(arguments.substr(comma + 1u));
    if (! window || ! ready)
        return 2;
    const auto com = wil::CoInitializeEx_failfast(COINIT_MULTITHREADED);
    wil::com_ptr_nothrow<IUIAutomation> automation;
    wil::com_ptr_nothrow<IUIAutomationElement> element;
    wil::com_ptr_nothrow<IUIAutomationCondition> all;
    wil::com_ptr_nothrow<IUIAutomationElementArray> found;
    HRESULT hr = UiaTest::CreateAutomationClient(automation.put());
    if (SUCCEEDED(hr))
        hr = automation->ElementFromHandle(reinterpret_cast<HWND>(*window), element.put());
    if (SUCCEEDED(hr))
        hr = automation->CreateTrueCondition(all.put());
    if (SUCCEEDED(hr))
        hr = element->FindAll(TreeScope_Subtree, all.get(), found.put());
    int length = 0;
    if (SUCCEEDED(hr) && found && SUCCEEDED(found->get_Length(&length)))
        for (int index = 0; index < length; ++index)
        {
            wil::com_ptr_nothrow<IUIAutomationElement> item;
            wil::unique_bstr name;
            if (SUCCEEDED(found->GetElement(index, item.put())) && item)
                static_cast<void>(item->get_CurrentName(name.put()));
        }
    if (FAILED(hr))
        return 3;
    static_cast<void>(SetEvent(reinterpret_cast<HANDLE>(*ready)));
    Sleep(INFINITE);
    return 0;
}
namespace
{
// For a test of the client gate: what it takes UiaClientsAreListening to answer, which another process on the desktop
// decides otherwise. Subscriptions UI Automation advises on a root still count. Nested, it restores the outer value.
class SystemListeners final
{
public:
    explicit SystemListeners(bool listening) noexcept : _outer(DxUi::DebugGetAccessibilitySystemListenersForTest())
    {
        DxUi::DebugSetAccessibilitySystemListenersForTest(listening);
    }
    ~SystemListeners()
    {
        DxUi::DebugSetAccessibilitySystemListenersForTest(_outer);
    }
    SystemListeners(const SystemListeners&)            = delete;
    SystemListeners& operator=(const SystemListeners&) = delete;
    SystemListeners(SystemListeners&&)                 = delete;
    SystemListeners& operator=(SystemListeners&&)      = delete;

private:
    std::optional<bool> _outer;
};
// What FX6's client subscribes to: the changes its mutations make.
[[nodiscard]] UiaTest::Subscription Fx6Subscription()
{
    UiaTest::Subscription subscription;
    subscription.automationEvents = {UIA_Text_TextChangedEventId};
    subscription.properties       = {UIA_NamePropertyId, UIA_ValueValuePropertyId, UIA_RangeValueValuePropertyId};
    subscription.structure        = true;
    return subscription;
}

// An event as FX6 compares it: kind, id, the sender's control type and the value. Not the sender's name, which UI
// Automation caches when it delivers the event, so a label's later text can already stand in it.
[[nodiscard]] std::wstring Fx6Key(const UiaTest::HeardEvent& event)
{
    return std::to_wstring(static_cast<int>(event.kind)) + L" " + std::to_wstring(event.id) + L" " + UiaTest::ControlTypeName(event.controlType) + L" " +
           event.value;
}

[[nodiscard]] std::vector<std::wstring> Fx6Keys(const std::vector<UiaTest::HeardEvent>& events)
{
    std::vector<std::wstring> keys;
    for (const auto& event : events)
        keys.push_back(Fx6Key(event));
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
    return keys;
}

// The events FX6's mutation `index` raises for a client subscribed with Fx6Subscription, as Fx6Key writes them.
[[nodiscard]] std::vector<std::wstring> Fx6MutationKeys(size_t index)
{
    const std::wstring number = std::to_wstring(index);
    switch (index % 3u)
    {
        case 0u: return {Fx6Key({UiaTest::EventKind::Property, UIA_NamePropertyId, UIA_TextControlTypeId, L"", L"", L"Status " + number})};
        case 1u:
            return {Fx6Key({UiaTest::EventKind::Property, UIA_ValueValuePropertyId, UIA_EditControlTypeId, L"", L"", L"Value " + number}),
                    Fx6Key({UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId, UIA_EditControlTypeId, L"", L"", L""})};
        default:
            return {Fx6Key(
                {UiaTest::EventKind::Property, UIA_RangeValueValuePropertyId, UIA_SliderControlTypeId, L"", L"", std::to_wstring(static_cast<double>(index))})};
    }
}

// The events a client subscribed with Fx6Subscription hears for FX6's 100 mutations, each distinct one once. Measured with
// this fixture and the same client on W8.1's base (0247dd92) before the client gate changed: these 101 distinct events.
[[nodiscard]] std::vector<std::wstring> Fx6ExpectedEvents()
{
    std::vector<std::wstring> keys;
    for (size_t index = 0u; index < Fx6::kMutations; ++index)
        for (auto& key : Fx6MutationKeys(index))
            keys.push_back(std::move(key));
    std::ranges::sort(keys);
    keys.erase(std::ranges::unique(keys).begin(), keys.end());
    return keys;
}
void PrintEvents(const char* label, const std::vector<std::wstring>& events)
{
    std::cerr << "    [FX6] " << label << " (" << events.size() << "):";
    for (const auto& event : events)
    {
        std::cerr << " [";
        for (const wchar_t unit : event)
            std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
        std::cerr << "]";
    }
    std::cerr << '\n';
}
} // namespace

// FX6 with one client: what it hears of the 100 mutations, each pumped as an application's loop would, is what it heard
// before the client gate: each mutation's own events, in order, with their values, and the whole stream heard once per
// mutation (UI Automation may deliver an event twice, never less than once). The desktop's other clients are taken to be
// silent, so the client's own advised subscription is what keeps the window publishing.
void TestAccessibilityClientGateFx6OneClientHearsTheReferenceEvents()
{
    const SystemListeners onlyThisClient(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), Fx6Subscription(), [&window] { window.PumpMessages(); });
    static_cast<void>(client.Root());
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    for (size_t index = 0u; index < Fx6::kMutations; ++index)
    {
        const size_t mutationMark = client.Mark();
        controls.Mutate(index);
        window.PumpMessages();
        const auto keys  = Fx6MutationKeys(index);
        const bool heard = client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                                            [&]
        {
            const auto since = Fx6Keys(client.DistinctSince(mutationMark));
            return std::ranges::all_of(keys, [&](const std::wstring& key) { return std::ranges::binary_search(since, key); });
        });
        if (! heard)
        {
            PrintEvents("heard after the mutation", Fx6Keys(client.DistinctSince(mutationMark)));
            PrintEvents("expected after the mutation", keys);
        }
        Require(heard, "FX6 with one client hears each mutation's own events, in order, with their values");
    }
    static_cast<void>(client.WaitOutTheStream());
    const auto expected = Fx6ExpectedEvents();
    std::vector<std::wstring> all;
    for (const auto& event : client.Events())
        all.push_back(Fx6Key(event));
    all.erase(all.begin(), all.begin() + static_cast<ptrdiff_t>(mark));
    std::vector<std::wstring> distinct = all;
    std::ranges::sort(distinct);
    distinct.erase(std::ranges::unique(distinct).begin(), distinct.end());
    if (distinct != expected)
    {
        PrintEvents("heard", distinct);
        PrintEvents("expected", expected);
    }
    Require(distinct == expected, "FX6 with one client hears no event it did not hear before the client gate");
    for (const auto& key : expected)
    {
        const size_t count  = static_cast<size_t>(std::ranges::count(all, key));
        const size_t raised = key == Fx6MutationKeys(1u)[1] ? 33u : 1u; // One text change per field mutation.
        Require(count >= raised && count <= 2u * raised, "FX6 with one client hears each event once per mutation that raised it");
    }
}
namespace
{
[[nodiscard]] uint64_t Builds() noexcept
{
    return DxUi::DebugGetAccessibilitySnapshotBuildCountForTest();
}

// What the client gate decides is whether a host publishes: since W8.2 a publication may share the records before it
// instead of building the whole window, so the gate's tests count publications of either kind.
[[nodiscard]] uint64_t Publications() noexcept
{
    return DxUi::DebugGetAccessibilityPublicationCountForTest();
}

// The element among `elements` (and their descendants, three levels down) whose name is `name`; zero when none.
[[nodiscard]] UiaTest::ElementId FindNamed(UiaTest::Client& client, UiaTest::ElementId from, std::wstring_view name, int depth = 3)
{
    for (const UiaTest::ElementId child : client.Children(from))
    {
        if (client.Describe(child).name == name)
            return child;
        if (depth > 0)
            if (const UiaTest::ElementId found = FindNamed(client, child, name, depth - 1))
                return found;
    }
    return 0u;
}

[[nodiscard]] double RangeValue(UiaTest::Client& client, UiaTest::ElementId element)
{
    double value = -1.0;
    RequireSucceeded(client.Perform("read a range value",
                                    [&](UiaTest::Session& session) -> HRESULT
    {
        IUIAutomationElement* const target = session.Get(element);
        if (! target)
            return E_INVALIDARG;
        wil::unique_variant result;
        RETURN_IF_FAILED(target->GetCurrentPropertyValue(UIA_RangeValueValuePropertyId, &result));
        if (result.vt != VT_R8)
            return E_UNEXPECTED;
        value = result.dblVal;
        return S_OK;
    }),
                     "a client reads the slider's range value");
    return value;
}

// What a client reads of FX6's controls is what the 100 mutations left.
void RequireFinalFx6Tree(UiaTest::Client& client, UiaTest::ElementId root, const char* what)
{
    const UiaTest::ElementId label  = FindNamed(client, root, Fx6::kFinalLabel);
    const UiaTest::ElementId field  = FindNamed(client, root, L"Entry");
    const UiaTest::ElementId slider = FindNamed(client, root, L"Level");
    Require(label != 0u && field != 0u && slider != 0u, what);
    Require(client.StringProperty(field, UIA_ValueValuePropertyId) == Fx6::kFinalField, what);
    Require(RangeValue(client, slider) == Fx6::kFinalSlider, what);
}

void MutateFx6(const Fx6::Controls& controls, AttachedHostWindow& window, size_t first = 0u, size_t count = Fx6::kMutations)
{
    for (size_t index = first; index < first + count; ++index)
    {
        controls.Mutate(index);
        window.PumpMessages();
    }
}
} // namespace

// FX6, no client: 100 mutations build no snapshot, and a client that attaches after them reads the current tree on its
// first query, which builds once.
void TestAccessibilityClientGateFx6NoClientBuildsNothing()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    const uint64_t before = Publications();
    const DxUi::Detail::WorkCounterScope work;
    MutateFx6(controls, window);
    Require(work.Delta().accessibility.snapshotBuilds == 0u && work.Delta().accessibility.snapshotNodes == 0u,
            "FX6 with no client: the work counters count no snapshot build or record");
    std::cerr << "    [FX6] no client: builds +" << (Publications() - before) << " for " << Fx6::kMutations << " mutations\n";
    Require(Publications() == before, "FX6 with no client: 100 mutations build no snapshot (G9)");
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root = client.Root();
    RequireFinalFx6Tree(client, root, "a client attaching after 100 unobserved mutations reads the current tree");
    std::cerr << "    [FX6] late client: builds +" << (Publications() - before) << '\n';
    Require(Publications() == before + 1u, "the late client's queries build one snapshot");
}

// FX6 after a probe that queried the window and let it go: the request latched the window for this attachment, so each of
// the 100 mutations publishes (once per turn), as before the client gate.
void TestAccessibilityClientGateFx6RequestedWindowKeepsBuilding()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    {
        UiaTest::Client probe(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
        const UiaTest::ElementId root = probe.Root();
        Require(FindNamed(probe, root, L"Entry") != 0u, "the probe reads the window's tree");
    }
    window.PumpMessages();
    const uint64_t before = Publications();
    MutateFx6(controls, window);
    std::cerr << "    [FX6] after a released probe: builds +" << (Publications() - before) << '\n';
    Require(Publications() >= before + Fx6::kMutations, "a window a client requested keeps building at each mutation for the attachment");
}

// A client that holds elements and only queries them, on its own thread, reads what the mutations left.
void TestAccessibilityClientGateHeldElementQueryReadsCurrentState()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root   = client.Root();
    const UiaTest::ElementId field  = FindNamed(client, root, L"Entry");
    const UiaTest::ElementId slider = FindNamed(client, root, L"Level");
    Require(field != 0u && slider != 0u, "the client holds the field's and the slider's elements");
    MutateFx6(controls, window);
    Require(client.StringProperty(field, UIA_ValueValuePropertyId) == Fx6::kFinalField, "a held element reads the field's current value");
    Require(RangeValue(client, slider) == Fx6::kFinalSlider, "a held element reads the slider's current value");
}

// A client that subscribes, hears, unsubscribes and subscribes again hears every mutation made while it listens.
void TestAccessibilityClientGateResubscribedClientMissesNothing()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    static_cast<void>(client.Root());
    const auto subscribe = [&]
    {
        RequireSucceeded(client.Perform("subscribe to FX6's properties",
                                        [](UiaTest::Session& session) -> HRESULT
        {
            wil::com_ptr_nothrow<IUIAutomationElement> element;
            RETURN_IF_FAILED(session.automation->ElementFromHandle(session.target, element.put()));
            std::array<PROPERTYID, 3u> ids = {{UIA_NamePropertyId, UIA_ValueValuePropertyId, UIA_RangeValueValuePropertyId}};
            return session.automation->AddPropertyChangedEventHandlerNativeArray(
                element.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size()));
        }),
                         "subscribe to FX6's properties");
        client.Settle(300u);
    };
    const auto unsubscribe = [&]
    {
        RequireSucceeded(client.Perform("unsubscribe", [](UiaTest::Session& session) { return session.automation->RemoveAllEventHandlers(); }), "unsubscribe");
        static_cast<void>(client.WaitOutTheStream());
    };
    const auto hears = [&](std::wstring value)
    { return client.WaitForEvent([&](const UiaTest::HeardEvent& event) { return event.kind == UiaTest::EventKind::Property && event.value == value; }); };
    subscribe();
    MutateFx6(controls, window, 0u, 3u);
    Require(hears(L"Status 0") && hears(L"Value 1") && hears(std::to_wstring(2.0)), "the subscribed client hears each mutation");
    unsubscribe();
    MutateFx6(controls, window, 3u, 3u);
    subscribe();
    MutateFx6(controls, window, 6u, 3u);
    Require(hears(L"Status 6") && hears(L"Value 7") && hears(std::to_wstring(8.0)), "the resubscribed client hears each mutation made after it subscribed");
    unsubscribe();
}
// FX6 in an embedded view, no client: UpdateAccessibility on every frame of 100 mutations builds no snapshot; a client
// then reads the current tree, and a listening update builds as before.
void TestAccessibilityClientGateFx6EmbeddedEveryFrameBuildsNothing()
{
    RequireSucceeded(DxUiFailureTests::RunRetainedAccessibilitySta(
                         []()
    {
        DxUi::TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "FX6 embedded application STA refuses activation");
        const SystemListeners nobody(false);
        GraphicsFixture graphics;
        RequireSucceeded(graphics.Create(), "create WARP device for FX6 embedded");
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> pool;
        RequireSucceeded(DxUi::GraphicsDevice::Create(graphics.device.get(), pool), "create FX6 embedded graphics pool");
        RequireSucceeded(view.Attach(std::move(pool), StoppedTestClock()), "attach FX6 embedded to borrowed graphics");
        Fx6::Controls controls;
        controls.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare FX6 embedded");
        {
            EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
            const uint64_t beforeAttach = Publications();
            RequireSucceeded(bridge.Attach(false, 0x46583601ull), "attach FX6 embedded accessibility");
            Require(Publications() == beforeAttach + 1u, "attaching accessibility builds the first snapshot");
            const uint64_t before = Publications();
            const DxUi::Detail::WorkCounterScope work;
            for (size_t index = 0u; index < Fx6::kMutations; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare an FX6 frame");
                Require(bridge.Update() == S_OK, "UpdateAccessibility accepts every changed frame");
                Require(bridge.Update() == S_FALSE, "an unchanged frame's update is a no-op");
            }
            std::cerr << "    [FX6] embedded, every frame, no client: builds +" << (Publications() - before) << '\n';
            Require(Publications() == before, "FX6 embedded with no client: UpdateAccessibility every frame builds no snapshot (G9)");
            Require(work.Delta().accessibility.snapshotBuilds == 0u, "FX6 embedded with no client: the work counters count no snapshot build");
            Require(work.Delta().frame.treeVisitsStatistics == 0u, "a recorded update reads its preparation count without walking the tree");
            {
                UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
                const UiaTest::ElementId root = client.Root();
                RequireFinalFx6Tree(client, root, "a client of the embedded view reads the current tree after deferred updates");
            }
            std::cerr << "    [FX6] embedded late client: builds +" << (Publications() - before) << '\n';
            Require(Publications() == before + 1u, "the embedded client's first request builds the recorded update once");
            const uint64_t requested = Publications();
            for (size_t index = 0u; index < 3u; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a frame of the requested view");
                Require(bridge.Update() == S_OK, "a requested view's frame publishes");
            }
            Require(Publications() == requested + 3u, "once requested, each changed frame builds, with no listener on the desktop");
            const SystemListeners listening(true);
            const uint64_t listened = Publications();
            for (size_t index = 3u; index < 6u; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a listened FX6 frame");
                Require(bridge.Update() == S_OK, "a listened frame publishes");
            }
            Require(Publications() == listened + 3u, "with a listening client each changed frame builds, as before the gate");
        }
        view.Detach();
    }),
                     "run FX6 embedded in a retained application STA");
}

namespace
{
// Two named fields, the first focused, in a window host or an embedded view: the fixture of the stale-baseline cases.
struct TwoFields final
{
    DxUi::TextField* first  = nullptr;
    DxUi::TextField* second = nullptr;

    void Build(DxUi::ControlHost& host)
    {
        auto root = std::make_unique<DxUi::Panel>();
        first     = root->AddChild<DxUi::TextField>(L"x");
        first->SetBounds(D2D1::RectF(8.0f, 8.0f, 240.0f, 36.0f));
        first->SetAccessibleName(L"First");
        second = root->AddChild<DxUi::TextField>(L"other");
        second->SetBounds(D2D1::RectF(8.0f, 44.0f, 240.0f, 72.0f));
        second->SetAccessibleName(L"Second");
        host.SetRoot(std::move(root));
        host.SetFocusControl(first, false);
    }
};

// Subscribes `client` to the window's value, focus-property and text changes, scoped to the window (advised on its root).
void SubscribeToFieldChanges(UiaTest::Client& client)
{
    RequireSucceeded(client.Perform("subscribe to the fields' changes",
                                    [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        RETURN_IF_FAILED(session.automation->ElementFromHandle(session.target, element.put()));
        std::array<PROPERTYID, 2u> ids = {{UIA_ValueValuePropertyId, UIA_HasKeyboardFocusPropertyId}};
        RETURN_IF_FAILED(session.automation->AddPropertyChangedEventHandlerNativeArray(
            element.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size())));
        RETURN_IF_FAILED(session.automation->AddAutomationEventHandler(
            UIA_Text_TextChangedEventId, element.get(), TreeScope_Subtree, session.cache.get(), session.recorder));
        return session.automation->AddFocusChangedEventHandler(session.cache.get(), session.recorder);
    }),
                     "subscribe to the fields' changes");
}

[[nodiscard]] bool HeardFromFirst(UiaTest::Client& client, size_t mark, UiaTest::EventKind kind, long id, std::wstring_view value = {})
{
    return client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                            [&]
    {
        for (const auto& event : client.DistinctSince(mark))
            if (event.kind == kind && event.id == id && event.name == L"First" && (value.empty() || event.value == value))
                return true;
        return false;
    });
}
} // namespace

// A window host: nobody listens while the first field changes to "y" and the focus moves to the second; a client then reads
// that, subscribes, and the user changes both back. The client hears the value, the text change and the focus return:
// the publication its query made, which nobody could hear, moved the event baseline with it.
void TestAccessibilityClientGateQueriedStateIsTheBaselineOfLaterEvents()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    TwoFields fields;
    fields.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root = client.Root();
    fields.first->SetText(L"y");
    window.Host().SetFocusControl(fields.second, false);
    window.PumpMessages();
    const UiaTest::ElementId first  = FindNamed(client, root, L"First");
    const UiaTest::ElementId second = FindNamed(client, root, L"Second");
    Require(first != 0u && second != 0u, "the client finds both fields");
    Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the client reads the unheard value");
    Require(client.Describe(second).hasKeyboardFocus, "the client reads the unheard focus move");
    SubscribeToFieldChanges(client);
    client.Settle(300u);
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    fields.first->SetText(L"x");
    window.Host().SetFocusControl(fields.first, false);
    window.PumpMessages();
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "the client hears the value change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId), "the client hears the text change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_HasKeyboardFocusPropertyId, L"true"), "the client hears the focus return");
}

namespace
{
// An embedded view of `Build`'s controls in a retained application STA, attached to an application window for UI Automation.
template <typename Body> void RunEmbeddedGateCase(const char* what, Body body)
{
    RequireSucceeded(DxUiFailureTests::RunRetainedAccessibilitySta(
                         [&]()
    {
        DxUi::TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "the embedded gate case's application STA refuses activation");
        GraphicsFixture graphics;
        RequireSucceeded(graphics.Create(), "create WARP device for an embedded gate case");
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> pool;
        RequireSucceeded(DxUi::GraphicsDevice::Create(graphics.device.get(), pool), "create an embedded gate case's graphics pool");
        RequireSucceeded(view.Attach(std::move(pool), StoppedTestClock()), "attach an embedded gate case to borrowed graphics");
        body(view);
        view.Detach();
    }),
                     what);
}
} // namespace

// The same in an embedded view: the query builds the deferred update, and the next listening update raises the way back.
void TestAccessibilityClientGateEmbeddedQueriedStateIsTheBaselineOfLaterEvents()
{
    RunEmbeddedGateCase("run the embedded stale-baseline case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the embedded fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(true, 0x46583602ull), "attach the embedded fields' accessibility");
        fields.first->SetText(L"y");
        view.Controls().SetFocusControl(fields.second, false);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the unheard change");
        Require(bridge.Update() == S_OK, "the unheard update is recorded");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        const UiaTest::ElementId root  = client.Root();
        const UiaTest::ElementId first = FindNamed(client, root, L"First");
        Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the client reads the unheard value");
        SubscribeToFieldChanges(client);
        const SystemListeners listening(true); // An embedded view is never advised: the client is the desktop's listener.
        static_cast<void>(client.WaitOutTheStream());
        const size_t mark = client.Mark();
        fields.first->SetText(L"x");
        view.Controls().SetFocusControl(fields.first, false);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the change back");
        Require(bridge.Update() == S_OK, "the listening update publishes the change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "the embedded client hears the value change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId), "the embedded client hears the text change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Focus, 0), "the embedded client hears the focus return");
    });
}

// A query that comes after an input handler changed a value and before the next Prepare (the view is paint-dirty, its geometry
// still the prepared one) reads the value as it is now, also for a client attaching then, after many deferred updates.
void TestAccessibilityClientGateEmbeddedQueryBeforePrepareReadsCurrentValue()
{
    RunEmbeddedGateCase("run the embedded unprepared-query case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        Fx6::Controls controls;
        controls.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare FX6 embedded");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x46583603ull), "attach FX6 embedded accessibility");
        for (size_t index = 0u; index < 30u; ++index)
        {
            controls.Mutate(index);
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a deferred frame");
            Require(bridge.Update() == S_OK, "a deferred frame's update is recorded");
        }
        controls.Mutate(31u); // "Value 31", made by an input handler: no Prepare yet.
        Require(view.NeedsPreparation(), "the view waits for its next Prepare");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        const UiaTest::ElementId root  = client.Root();
        const UiaTest::ElementId field = FindNamed(client, root, L"Entry");
        Require(field != 0u && client.StringProperty(field, UIA_ValueValuePropertyId) == L"Value 31",
                "a client attaching before the next Prepare reads the value the input handler set");
        Require(FindNamed(client, root, L"Status 27") != 0u, "and the label of the last deferred frame");
    });
}

// A query of an embedded view during a splitter drag whose sibling layout changed the interaction revision, before the next
// Prepare, leaves the drag alone: the capture survives and the drag continues.
void TestAccessibilityClientGateEmbeddedQueryDoesNotCancelADrag()
{
    RunEmbeddedGateCase("run the embedded drag case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        constexpr UINT kWidth = 480u, kHeight = 240u;
        auto root      = std::make_unique<DxUi::Panel>();
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        splitter->SetAccessibleName(L"Panes");
        auto* sibling = root->AddChild<DxUi::Slider>();
        sibling->SetBounds(D2D1::RectF(24, 160, 440, 208));
        sibling->SetAccessibleName(L"Sibling");
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the splitter");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x46583604ull), "attach the splitter's accessibility");
        sibling->SetValue(10.0);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a deferred frame");
        Require(bridge.Update() == S_OK, "the frame's update is deferred");
        Require(view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "the splitter drag starts");
        Require(view.DispatchPointer({DxUi::PointerAction::Move, 260, 120}), "the splitter drag previews");
        const float firstPosition = splitter->GetPosition();
        sibling->SetBounds(D2D1::RectF(24, 168, 440, 216)); // Sibling layout: the interaction revision changes.
        {
            UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
            Require(FindNamed(client, client.Root(), L"Panes") != 0u, "a client queries the view during the drag");
        }
        Require(view.Controls().GetCapturedControl() == splitter && splitter->IsDragging(), "the query left the splitter's capture");
        RequireSucceeded(view.PrepareInteraction(kWidth, kHeight), "the sibling's bounds are acknowledged during the drag");
        Require(view.DispatchPointer({DxUi::PointerAction::Move, 300, 120}), "the splitter drag continues");
        Require(splitter->GetPosition() > firstPosition, "the continued drag advances the splitter");
    });
}

namespace
{
[[nodiscard]] bool HeardFrom(UiaTest::Client& client, size_t mark, std::wstring_view name, UiaTest::EventKind kind, long id, std::wstring_view value = {})
{
    return client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                            [&]
    {
        for (const auto& event : client.DistinctSince(mark))
            if (event.kind == kind && event.id == id && event.name == name && (value.empty() || event.value == value))
                return true;
        return false;
    });
}

} // namespace

// A window host: nobody listens while the first field changes to "y" and the focus moves to the second; a client then
// subscribes, and only then reads that; the user changes both back. The client hears the value, the text change and the
// focus return: the publication its query made shows only unheard changes, so the event baseline caught up with it.
void TestAccessibilityClientGateSubscribedThenQueriedStateIsTheBaselineOfLaterEvents()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    TwoFields fields;
    fields.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root = client.Root();
    fields.first->SetText(L"y");
    window.Host().SetFocusControl(fields.second, false);
    window.PumpMessages();
    SubscribeToFieldChanges(client);
    client.Settle(300u);
    const UiaTest::ElementId first  = FindNamed(client, root, L"First");
    const UiaTest::ElementId second = FindNamed(client, root, L"Second");
    Require(first != 0u && second != 0u, "the subscribed client finds both fields");
    Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the subscribed client reads the unheard value");
    Require(client.Describe(second).hasKeyboardFocus, "the subscribed client reads the unheard focus move");
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    fields.first->SetText(L"x");
    window.Host().SetFocusControl(fields.first, false);
    window.PumpMessages();
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "the client hears the value change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId), "the client hears the text change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_HasKeyboardFocusPropertyId, L"true"), "the client hears the focus return");
}

// A mixed batch: an unheard change, then a client subscribes, then a heard change, then a query, all before the drain. The
// query's publication must not swallow the heard change: the drain raises it (the unheard one too), and later changes follow.
void TestAccessibilityClientGateMixedBatchKeepsTheHeardChange()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    TwoFields fields;
    fields.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root = client.Root();
    fields.first->SetText(L"y"); // Unheard.
    SubscribeToFieldChanges(client);
    client.Settle(300u);
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    fields.second->SetText(L"z"); // Heard: the drain is posted, not yet run.
    const UiaTest::ElementId first = FindNamed(client, root, L"First");
    Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the query reads the batch");
    Require(HeardFrom(client, mark, L"Second", UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"z"), "the client hears the heard change of the batch");
    const size_t later = client.Mark();
    fields.first->SetText(L"x");
    window.PumpMessages();
    Require(HeardFromFirst(client, later, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "and the change after it");
}

// An embedded view: an update deferred while nobody listened, then a client subscribes (a listener on the desktop) and
// queries, which builds the deferred update and raises its delta; the change back is heard at the next update.
void TestAccessibilityClientGateEmbeddedSubscribedThenQueriedStateIsTheBaselineOfLaterEvents()
{
    RunEmbeddedGateCase("run the embedded subscribe-then-query case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the embedded fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(true, 0x46583605ull), "attach the embedded fields' accessibility");
        fields.first->SetText(L"y");
        view.Controls().SetFocusControl(fields.second, false);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the unheard change");
        Require(bridge.Update() == S_OK, "the unheard update is recorded");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        SubscribeToFieldChanges(client);
        const SystemListeners listening(true);
        const UiaTest::ElementId root  = client.Root();
        const UiaTest::ElementId first = FindNamed(client, root, L"First");
        Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the subscribed client reads the unheard value");
        static_cast<void>(client.WaitOutTheStream());
        const size_t mark = client.Mark();
        fields.first->SetText(L"x");
        view.Controls().SetFocusControl(fields.first, false);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the change back");
        Require(bridge.Update() == S_OK, "the listening update publishes the change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "the embedded client hears the value change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId), "the embedded client hears the text change back");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Focus, 0), "the embedded client hears the focus return");
    });
}

// The embedded mixed batch: a deferred unheard change, then a listener, then a heard change before any update, then a query.
void TestAccessibilityClientGateEmbeddedMixedBatchKeepsTheHeardChange()
{
    RunEmbeddedGateCase("run the embedded mixed-batch case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the embedded fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(true, 0x46583606ull), "attach the embedded fields' accessibility");
        fields.first->SetText(L"y");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the unheard change");
        Require(bridge.Update() == S_OK, "the unheard update is recorded");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        SubscribeToFieldChanges(client);
        const SystemListeners listening(true);
        static_cast<void>(client.WaitOutTheStream());
        const size_t mark = client.Mark();
        fields.second->SetText(L"z"); // Heard, before any update.
        const UiaTest::ElementId first = FindNamed(client, client.Root(), L"First");
        Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the query reads the batch");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the heard change");
        static_cast<void>(bridge.Update());
        Require(HeardFrom(client, mark, L"Second", UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"z"), "the embedded client hears the heard change");
    });
}

namespace
{
class ManyRowsGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 200u;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"row";
        column.title    = L"Row";
        column.widthDip = 120.0f;
        return column;
    }
    void GetCellData(size_t rowIndex, size_t, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = L"Row " + std::to_wstring(rowIndex);
    }
    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return rowIndex + 1u;
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId >= 1u && rowId <= 200u ? std::optional<size_t>(rowId - 1u) : std::nullopt;
    }
};

// A splitter drag during which a sibling's bounds change, before the next Prepare: what each embedded query path does then.
template <typename Query> void RunDragDuringQuery(const char* what, Query query)
{
    RunEmbeddedGateCase(what,
                        [&](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        constexpr UINT kWidth = 480u, kHeight = 400u;
        ManyRowsGridModel model;
        auto root      = std::make_unique<DxUi::Panel>();
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(0, 0, 480, 240));
        splitter->SetMinimumFirstPane(80);
        splitter->SetMinimumSecondPane(80);
        splitter->SetPosition(200);
        splitter->SetAccessibleName(L"Panes");
        auto* sibling = root->AddChild<DxUi::Slider>();
        sibling->SetBounds(D2D1::RectF(24, 160, 440, 208));
        auto* grid = root->AddChild<DxUi::Grid>();
        grid->SetBounds(D2D1::RectF(0, 248, 480, 400));
        grid->SetModel(&model);
        grid->SetAccessibleName(L"Rows");
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the drag fixture");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x46583607ull), "attach the drag fixture's accessibility");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        const UiaTest::ElementId rows = FindNamed(client, client.Root(), L"Rows");
        Require(rows != 0u, "the client holds the grid's element");
        Require(view.DispatchPointer({DxUi::PointerAction::Down, 203, 120}), "the splitter drag starts");
        Require(view.DispatchPointer({DxUi::PointerAction::Move, 260, 120}), "the splitter drag previews");
        const float firstPosition = splitter->GetPosition();
        sibling->SetBounds(D2D1::RectF(24, 168, 440, 216)); // The interaction revision changes.
        query(view, client, rows);
        Require(view.Controls().GetCapturedControl() == splitter && splitter->IsDragging(), "the query left the splitter's capture");
        RequireSucceeded(view.PrepareInteraction(kWidth, kHeight), "the sibling's bounds are acknowledged during the drag");
        Require(view.DispatchPointer({DxUi::PointerAction::Move, 300, 120}), "the splitter drag continues");
        Require(splitter->GetPosition() > firstPosition, "the continued drag advances the splitter");
    });
}
} // namespace

// EmbeddedHost::GetAccessibilityProvider during that drag does not cancel it.
void TestAccessibilityClientGateEmbeddedProviderRequestDoesNotCancelADrag()
{
    RunDragDuringQuery("run the provider-request drag case",
                       [](DxUi::EmbeddedHost& view, UiaTest::Client&, UiaTest::ElementId)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
        const HRESULT hr = view.GetAccessibilityProvider(root.put());
        Require(hr == S_OK || hr == S_FALSE, "the provider request answers during the drag");
    });
}

// A GridPattern.GetItem of an uncached row during that drag does not cancel it.
void TestAccessibilityClientGateEmbeddedGetItemDoesNotCancelADrag()
{
    RunDragDuringQuery("run the GetItem drag case",
                       [](DxUi::EmbeddedHost&, UiaTest::Client& client, UiaTest::ElementId rows)
    {
        const HRESULT hr = client.Perform("GetItem of an uncached row",
                                          [rows](UiaTest::Session& session) -> HRESULT
        {
            IUIAutomationElement* const grid = session.Get(rows);
            if (! grid)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationGridPattern> pattern;
            RETURN_IF_FAILED(grid->GetCurrentPatternAs(UIA_GridPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationElement> item;
            static_cast<void>(pattern->GetItem(150, 0, item.put()));
            return S_OK;
        });
        RequireSucceeded(hr, "the client asks for an uncached row during the drag");
    });
}

// A UI Automation client in another process that only queries the window (no subscription) requests its provider: the window
// keeps building at each mutation for the rest of the attachment, before and after the client ends (the latch).
void TestAccessibilityClientGateOutOfProcessQueryOnlyClientLatchesTheWindow()
{
    const SystemListeners nobody(false);
    TwoFields fields;
    AttachedHostWindow window;
    fields.Build(window.Host());
    window.PumpMessages();
    const HWND hwnd = window.Hwnd();
    SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
    wil::unique_event_nothrow ready(CreateEventW(&inheritable, TRUE, FALSE, nullptr));
    Require(static_cast<bool>(ready), "create the query client's ready event");
    std::array<wchar_t, MAX_PATH> path{};
    Require(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())) != 0u, "find the test executable");
    std::wstring commandLine = L"\"" + std::wstring(path.data()) + L"\" " + std::wstring(kQueryChildPrefix) +
                               std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) + L"," + std::to_wstring(reinterpret_cast<uintptr_t>(ready.get()));
    SIZE_T attributeBytes    = 0u;
    static_cast<void>(InitializeProcThreadAttributeList(nullptr, 1u, 0u, &attributeBytes));
    std::vector<std::byte> attributeStorage(attributeBytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    Require(InitializeProcThreadAttributeList(attributes, 1u, 0u, &attributeBytes) != FALSE, "initialize the query client's attributes");
    const auto deleteAttributes = wil::scope_exit([&]() noexcept { DeleteProcThreadAttributeList(attributes); });
    HANDLE inherited            = ready.get();
    Require(UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherited, sizeof(inherited), nullptr, nullptr) != FALSE,
            "pass only the ready event to the query client");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb  = sizeof(startup);
    startup.lpAttributeList = attributes;
    wil::unique_process_information process;
    Require(CreateProcessW(nullptr,
                           commandLine.data(),
                           nullptr,
                           nullptr,
                           TRUE,
                           CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                           nullptr,
                           nullptr,
                           &startup.StartupInfo,
                           &process) != FALSE,
            "start the query client");
    wil::unique_handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    Require(job && SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE &&
                AssignProcessToJobObject(job.get(), process.hProcess) != FALSE,
            "tie the query client to this process");
    const ULONGLONG deadline = GetTickCount64() + 20000u;
    while (WaitForSingleObject(ready.get(), 0u) != WAIT_OBJECT_0 && GetTickCount64() < deadline)
    {
        window.PumpMessages();
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    Require(WaitForSingleObject(ready.get(), 0u) == WAIT_OBJECT_0, "the query client read the window's elements");
    const uint64_t before = Publications();
    fields.first->SetText(L"after the query client");
    window.PumpMessages();
    Require(Publications() > before, "a window an out-of-process client requested keeps building at a mutation");
    static_cast<void>(TerminateProcess(process.hProcess, 0u));
    static_cast<void>(WaitForSingleObject(process.hProcess, 10000u));
    const uint64_t ended = Publications();
    fields.first->SetText(L"after the query client ended");
    window.PumpMessages();
    Require(Publications() > ended, "and keeps building after that client ended, for the attachment");
}

// Building when only paint is dirty: a query of a single-line field whose text an input handler changed before the next
// Prepare reads the new text and text-range rectangles from the field's current layout, inside the field.
void TestAccessibilityClientGateEmbeddedUnpreparedTextHasCurrentRangeBounds()
{
    RunEmbeddedGateCase("run the unprepared text-bounds case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x46583608ull), "attach the fields' accessibility");
        fields.second->SetText(L"other");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a deferred frame");
        Require(bridge.Update() == S_OK, "the frame's update is deferred");
        fields.first->SetText(L"a considerably longer value typed before the next Prepare");
        Require(view.NeedsPreparation(), "the view waits for its next Prepare");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        const UiaTest::ElementId first = FindNamed(client, client.Root(), L"First");
        Require(first != 0u && client.DocumentText(first) == L"a considerably longer value typed before the next Prepare", "the query reads the new text");
        const UiaTest::ElementInfo info = client.Describe(first);
        size_t rectangles               = 0u;
        bool inside                     = true;
        RequireSucceeded(client.Perform("the document range's rectangles",
                                        [&](UiaTest::Session& session) -> HRESULT
        {
            auto* element = session.Get(first);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> range;
            RETURN_IF_FAILED(pattern->get_DocumentRange(range.put()));
            SAFEARRAY* raw = nullptr;
            RETURN_IF_FAILED(range->GetBoundingRectangles(&raw));
            const auto destroy = wil::scope_exit([&]() noexcept
            {
                if (raw)
                    SafeArrayDestroy(raw);
            });
            LONG last          = -1;
            if (raw)
                RETURN_IF_FAILED(SafeArrayGetUBound(raw, 1u, &last));
            rectangles = static_cast<size_t>(last + 1) / 4u;
            for (LONG index = 0; index + 3 <= last; index += 4)
            {
                std::array<double, 4u> box{};
                for (LONG part = 0; part < 4; ++part)
                {
                    LONG at = index + part;
                    RETURN_IF_FAILED(SafeArrayGetElement(raw, &at, &box[static_cast<size_t>(part)]));
                }
                inside = inside && box[0] >= info.boundingRectangle.left - 1 && box[1] >= info.boundingRectangle.top - 1 &&
                         box[0] + box[2] <= info.boundingRectangle.right + 1 && box[1] + box[3] <= info.boundingRectangle.bottom + 1;
            }
            return S_OK;
        }),
                         "read the document range's rectangles");
        std::cerr << "    [unprepared text] " << rectangles << " rectangle(s), inside the field: " << inside << '\n';
        Require(rectangles != 0u && inside, "the new text's rectangles come from the field's current layout, inside the field");
    });
}

// An embedded view's first request that comes after its tree or bounds changed and before the next Prepare cannot build the
// recorded update (its geometry is not the prepared one): it reads the snapshot attachment built, and the next update, now
// that the view is requested, builds the current tree.
void TestAccessibilityClientGateEmbeddedFirstRequestBeforePrepareReadsTheLastBuild()
{
    RunEmbeddedGateCase("run the embedded unprepared first-request case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x46583609ull), "attach the fields' accessibility");
        fields.first->SetText(L"y");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a recorded frame");
        Require(bridge.Update() == S_OK, "the frame's update is recorded");
        fields.second->SetBounds(D2D1::RectF(8.0f, 48.0f, 240.0f, 76.0f)); // The interaction revision changes; no Prepare yet.
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        const UiaTest::ElementId first = FindNamed(client, client.Root(), L"First");
        Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"x", "the first request reads the snapshot attachment built");
        const uint64_t before = Publications();
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the next frame");
        Require(bridge.Update() == S_OK && Publications() == before + 1u, "the next update of the requested view builds");
        Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "and the client reads the current tree");
    });
}

namespace
{
[[nodiscard]] size_t PropertyEventsFrom(UiaTest::Client& client, size_t mark, std::wstring_view name, std::wstring_view value)
{
    size_t count      = 0u;
    const auto events = client.Events();
    for (size_t index = mark; index < events.size(); ++index)
        if (events[index].kind == UiaTest::EventKind::Property && events[index].name == name && events[index].value == value)
            ++count;
    return count;
}

[[nodiscard]] HRESULT GetGridItem(UiaTest::Client& client, UiaTest::ElementId grid, int row)
{
    HRESULT itemResult = E_PENDING;
    RequireSucceeded(client.Perform("GridPattern.GetItem",
                                    [&](UiaTest::Session& session) -> HRESULT
    {
        IUIAutomationElement* const element = session.Get(grid);
        if (! element)
            return E_INVALIDARG;
        wil::com_ptr_nothrow<IUIAutomationGridPattern> pattern;
        RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_GridPatternId, IID_PPV_ARGS(pattern.put())));
        wil::com_ptr_nothrow<IUIAutomationElement> item;
        itemResult = pattern->GetItem(row, 0, item.put());
        if (SUCCEEDED(itemResult) && ! item)
            itemResult = E_FAIL;
        return S_OK;
    }),
                     "ask the grid for an item");
    return itemResult;
}
} // namespace

// A first request that builds a recorded update while a client listens raises nothing on the stack of that provider call: the
// next UpdateAccessibility raises the delta, even an otherwise unchanged one, and exactly once.
void TestAccessibilityClientGateEmbeddedFirstRequestRaisesAtTheNextUpdateOnce()
{
    RunEmbeddedGateCase("run the embedded pending-raise case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x4658360Aull), "attach the fields' accessibility");
        fields.first->SetText(L"y");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the recorded change");
        Require(bridge.Update() == S_OK, "the change is recorded");
        const SystemListeners listening(true);
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        SubscribeToFieldChanges(client);
        static_cast<void>(client.WaitOutTheStream());
        const size_t beforeQuery       = client.Mark();
        const UiaTest::ElementId first = FindNamed(client, client.Root(), L"First");
        Require(first != 0u && client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the first request builds the recorded update");
        client.Settle(500u);
        Require(PropertyEventsFrom(client, beforeQuery, L"First", L"y") == 0u, "the first request raises nothing on the stack of the provider call");
        Require(bridge.Update() == S_OK, "the next update, otherwise unchanged, raises the pending delta");
        Require(HeardFromFirst(client, beforeQuery, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"y"), "the client hears the change then");
        static_cast<void>(client.WaitOutTheStream());
        const size_t afterRaise = client.Mark();
        Require(bridge.Update() == S_FALSE, "the update after it is unchanged again");
        client.Settle(500u);
        Require(PropertyEventsFrom(client, afterRaise, L"First", L"y") == 0u, "the pending delta is raised exactly once");
    });
}

// A listener that appears before any provider call reaches the view makes the next changed update build and raise the whole
// delta from the last built snapshot: the change recorded unheard and the new one.
void TestAccessibilityClientGateEmbeddedListenerBeforeAnyRequestRaisesTheWholeDelta()
{
    RunEmbeddedGateCase("run the embedded listener-first case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the fields");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x4658360Bull), "attach the fields' accessibility");
        fields.first->SetText(L"y");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the recorded change");
        Require(bridge.Update() == S_OK, "the change is recorded");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        SubscribeToFieldChanges(client);
        const SystemListeners listening(true);
        static_cast<void>(client.WaitOutTheStream());
        const size_t mark = client.Mark();
        fields.second->SetText(L"z");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the heard change");
        const uint64_t before = Publications();
        Require(bridge.Update() == S_OK && Publications() == before + 1u, "with a listener the next changed update builds");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"y"), "it raises the change recorded unheard");
        Require(HeardFrom(client, mark, L"Second", UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"z"), "and the new one");
    });
}

// Detaching and attaching again starts unrequested; dropping the retained snapshot keeps the request, and the next update
// builds a tree again.
void TestAccessibilityClientGateEmbeddedReattachStartsUnrequested()
{
    RunEmbeddedGateCase("run the embedded reattach case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        TwoFields fields;
        fields.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the fields");
        {
            EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
            RequireSucceeded(bridge.Attach(false, 0x4658360Cull), "attach the fields' accessibility");
            {
                UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
                Require(FindNamed(client, client.Root(), L"First") != 0u, "a client requests the view");
            }
            fields.first->SetText(L"requested");
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a requested frame");
            uint64_t before = Publications();
            Require(bridge.Update() == S_OK && Publications() == before + 1u, "a requested view builds at each changed update");
            view.DebugDropRetainedStateForTest();
            fields.first->SetText(L"after the drop");
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare after the drop");
            before = Publications();
            Require(bridge.Update() == S_OK && Publications() == before + 1u, "after a drop the next update builds a tree again");
            fields.first->SetText(L"still requested");
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare another frame");
            before = Publications();
            Require(bridge.Update() == S_OK && Publications() == before + 1u, "a drop keeps the request");
        } // The bridge disconnects the view's accessibility.
        EmbeddedUiaTest::Bridge again(view, kWidth, kHeight);
        RequireSucceeded(again.Attach(false, 0x4658360Dull), "attach the view's accessibility again");
        fields.first->SetText(L"unrequested");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a frame of the new attachment");
        const uint64_t before = Publications();
        Require(again.Update() == S_OK && Publications() == before, "a new attachment starts unrequested: the changed update builds nothing");
    });
}

// A view resized and not yet updated: the first request cannot build the recorded update (no prepared geometry at its
// placement) and serves the snapshot built at attachment; an uncached-row GetItem refuses; the next accepted update builds
// the current tree, raises the delta, and GetItem then succeeds.
void TestAccessibilityClientGateEmbeddedResizedFirstRequestIsRepairedByTheNextUpdate()
{
    RunEmbeddedGateCase("run the embedded resize case",
                        [](DxUi::EmbeddedHost& view)
    {
        const SystemListeners nobody(false);
        constexpr UINT kWidth = 480u, kHeight = 400u, kWiderWidth = 520u;
        ManyRowsGridModel model;
        TwoFields fields;
        fields.Build(view.Controls());
        auto* grid = static_cast<DxUi::Panel*>(view.Controls().GetRoot())->AddChild<DxUi::Grid>();
        grid->SetBounds(D2D1::RectF(0, 248, 480, 400));
        grid->SetModel(&model);
        grid->SetAccessibleName(L"Rows");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the view");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(false, 0x4658360Eull), "attach the view's accessibility");
        fields.first->SetText(L"y");
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the recorded change");
        Require(bridge.Update() == S_OK, "the change is recorded");
        RequireSucceeded(view.Prepare(kWiderWidth, kHeight, 96.0f), "resize the view without an update");
        UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
        SubscribeToFieldChanges(client);
        const UiaTest::ElementId root  = client.Root();
        const UiaTest::ElementId first = FindNamed(client, root, L"First");
        const UiaTest::ElementId rows  = FindNamed(client, root, L"Rows");
        Require(first != 0u && rows != 0u, "the client finds the field and the grid");
        Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"x", "a first request that cannot build serves the snapshot built at attachment");
        Require(FAILED(GetGridItem(client, rows, 150)), "an uncached-row GetItem refuses while the update is recorded");
        const SystemListeners listening(true);
        static_cast<void>(client.WaitOutTheStream());
        const size_t mark = client.Mark();
        const DxUi::EmbeddedAccessibilityPlacement wider{{-32000.0, -32000.0, static_cast<double>(kWiderWidth), static_cast<double>(kHeight)}, false};
        const uint64_t before = Publications();
        Require(view.UpdateAccessibility(wider) == S_OK && Publications() == before + 1u, "the next accepted update builds the current tree");
        Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"y"), "and raises the delta since the snapshot served");
        Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "the client reads the current tree");
        Require(SUCCEEDED(GetGridItem(client, rows, 150)), "and GetItem of the uncached row succeeds");
    });
}

// ---- Incremental publication (W8.2) --------------------------------------------------------------------------------------

namespace
{
// While alive: incremental publications are verified against a whole build, and the verification starts from zero.
class VerifiedPublications final
{
public:
    VerifiedPublications() noexcept : previous_(DxUi::DebugGetAccessibilityIncrementalVerificationForTest())
    {
        DxUi::DebugResetAccessibilityIncrementalVerificationForTest();
        DxUi::DebugSetAccessibilityIncrementalVerificationForTest(true);
    }
    ~VerifiedPublications()
    {
        DxUi::DebugSetAccessibilityIncrementalVerificationForTest(previous_);
    }
    VerifiedPublications(const VerifiedPublications&)            = delete;
    VerifiedPublications& operator=(const VerifiedPublications&) = delete;
    VerifiedPublications(VerifiedPublications&&)                 = delete;
    VerifiedPublications& operator=(VerifiedPublications&&)      = delete;
    void RequireNoDifference(const char* what) const
    {
        const uint64_t mismatches = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest();
        if (mismatches != 0u)
            std::wcerr << L"    [W8.2] first difference from a whole build: " << DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest() << L'\n';
        Require(mismatches == 0u, what);
    }

private:
    bool previous_;
};
} // namespace

// The event baseline (W8.1 record, closed by W8.2): another process listened while the field became "a", then stopped; the
// field became "b" with nobody listening; a client then starts, queries and subscribes; the field returns to "a". The client
// read "b", so it is owed the change back to "a". The record expected the event baseline to still hold "a" here and nothing
// to be raised; on this tree the scenario passes with and without W8.2's baseline rule (the in-process client's arrival
// publishes through the drain), so this test guards the scenario rather than reproducing the loss.
void TestAccessibilityIncrementalLostEventAfterListenerLeft()
{
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    {
        const SystemListeners another(true);
        controls.field->SetText(L"a");
        window.PumpMessages();
    }
    const SystemListeners nobody(false);
    controls.field->SetText(L"b");
    window.PumpMessages();
    {
        // The screen reader's first request reaches the window's thread (WM_GETOBJECT and the root's first calls), which
        // publishes the unheard change there, outside the queued drain: the publication that left the baseline behind.
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> owner;
        owner.attach(window.Host().DebugCreateAccessibilityProvider());
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        Require(owner && SUCCEEDED(owner.query_to(fragment.put())), "the window's root provider is requested on its thread");
        wil::com_ptr_nothrow<IRawElementProviderFragment> first;
        Require(SUCCEEDED(fragment->Navigate(NavigateDirection_FirstChild, first.put())) && first, "the request reads the window's tree");
    }
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root  = client.Root();
    const UiaTest::ElementId field = FindNamed(client, root, L"Entry");
    Require(field != 0u && client.StringProperty(field, UIA_ValueValuePropertyId) == L"b", "the starting client reads the unheard value");
    RequireSucceeded(client.Perform("subscribe to the field's value",
                                    [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomationElement> element;
        RETURN_IF_FAILED(session.automation->ElementFromHandle(session.target, element.put()));
        std::array<PROPERTYID, 1u> ids = {{UIA_ValueValuePropertyId}};
        return session.automation->AddPropertyChangedEventHandlerNativeArray(
            element.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size()));
    }),
                     "subscribe to the field's value");
    client.Settle(300u);
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    controls.field->SetText(L"a");
    window.PumpMessages();
    const bool heard = HeardFrom(client, mark, L"Entry", UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"a");
    for (const auto& event : client.DistinctSince(mark))
        std::wcerr << L"    [W8.2] heard " << UiaTest::DescribeEvent(event) << L'\n';
    Require(heard, "the client hears the value return to the state another listener last heard");
}

// A native window's control moved by SetBounds: a client reads the new bounding rectangle without any other change
// republishing (the W0.5 record), and the move costs no whole-window build: its record and the rectangles are republished.
void TestAccessibilityIncrementalNativeBoundsFollowSetBounds()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    const VerifiedPublications verified;
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root  = client.Root();
    const UiaTest::ElementId field = FindNamed(client, root, L"Entry");
    Require(field != 0u, "the client finds the field");
    const RECT before    = client.Describe(field).boundingRectangle;
    const uint64_t built = Builds();
    controls.field->SetBounds(D2D1::RectF(8.0f, 140.0f, 240.0f, 168.0f));
    window.PumpMessages();
    const RECT after   = client.Describe(field).boundingRectangle;
    const float scale  = window.Host().GetDpi() / 96.0f;
    const LONG shifted = static_cast<LONG>(std::lround(100.0f * scale));
    std::cerr << "    [W8.2] field top " << before.top << " -> " << after.top << ", whole builds +" << (Builds() - built) << '\n';
    Require(after.top - before.top == shifted && after.bottom - before.bottom == shifted, "a client reads the moved control's new bounding rectangle");
    Require(Builds() == built, "a move republishes the moved record and the rectangles, not the whole window");
    verified.RequireNoDifference("the move's publication equals a whole build");
}

// A published snapshot never changes: a reader on another thread describes one generation over and over while the owner
// publishes 100 later ones, each sharing every record but the slider's with the one before.
void TestAccessibilityIncrementalOldSnapshotIsStableAcrossThreads()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), true), "the window publishes at each mutation");
    const auto clearInterest = wil::scope_exit([&] { static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), std::nullopt)); });
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();
    const VerifiedPublications verified;
    const std::shared_ptr<const void> held = DxUi::DebugHoldAccessibilitySnapshotForTest(window.Host());
    Require(held != nullptr, "the window published a snapshot");
    std::vector<std::wstring> expected;
    DxUi::DebugDescribeAccessibilitySnapshotForTest(held, expected);
    std::atomic<bool> stop{false};
    std::atomic<size_t> reads{0u}, changed{0u};
    std::thread reader([&]
    {
        std::vector<std::wstring> lines;
        while (! stop.load(std::memory_order_acquire))
        {
            DxUi::DebugDescribeAccessibilitySnapshotForTest(held, lines);
            if (lines != expected)
                changed.fetch_add(1u, std::memory_order_relaxed);
            reads.fetch_add(1u, std::memory_order_relaxed);
        }
    });
    const uint64_t built                   = Builds();
    std::shared_ptr<const void> generation = held;
    size_t fewestShared                    = (std::numeric_limits<size_t>::max)();
    for (size_t step = 0u; step < 100u; ++step)
    {
        controls.slider->SetValue(static_cast<double>(step + 1u));
        window.PumpMessages();
        const std::shared_ptr<const void> next = DxUi::DebugHoldAccessibilitySnapshotForTest(window.Host());
        fewestShared                           = (std::min)(fewestShared, DxUi::DebugCountSharedAccessibilityRecordsForTest(generation, next));
        generation                             = next;
    }
    while (reads.load(std::memory_order_relaxed) < 100u)
        std::this_thread::yield();
    stop.store(true, std::memory_order_release);
    reader.join();
    std::vector<std::wstring> latest;
    DxUi::DebugDescribeAccessibilitySnapshotForTest(generation, latest);
    std::cerr << "    [W8.2] " << reads.load() << " reads of the held generation, " << changed.load() << " changed; whole builds +" << (Builds() - built)
              << ", fewest records shared with the previous generation " << fewestShared << '\n';
    Require(changed.load() == 0u, "a held generation reads the same on another thread while later ones are published");
    Require(latest != expected, "the latest generation shows the slider's value");
    Require(Builds() == built, "100 slider values publish 100 successors and no whole build");
    Require(fewestShared == 2u, "each successor shares the two records the value did not change");
    verified.RequireNoDifference("every successor equals a whole build");
}

// FX6's slider on a native window: 100 Right keys through the window's message path publish successors, with no
// whole-window build after the first key, one record re-read each.
void TestAccessibilityIncrementalNativeSliderKeys()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), true), "the window publishes at each mutation");
    const auto clearInterest = wil::scope_exit([&] { static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), std::nullopt)); });
    window.Host().SetFocusControl(controls.slider, false);
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();
    const auto key = [&]
    {
        SendMessageW(window.Hwnd(), WM_KEYDOWN, VK_RIGHT, 1);
        SendMessageW(window.Hwnd(), WM_KEYUP, VK_RIGHT, static_cast<LPARAM>(0xC0000001u));
        window.PumpMessages();
    };
    key(); // The first key may turn the keyboard focus ring on.
    const VerifiedPublications verified;
    const uint64_t built = Builds();
    const DxUi::Detail::WorkCounterScope work;
    for (size_t step = 1u; step < 100u; ++step)
        key();
    const auto delta = work.Delta().accessibility;
    std::cerr << "    [W8.2] native slider keys: whole builds +" << (Builds() - built) << ", incremental " << delta.incrementalPublications << ", records "
              << delta.recordsCloned << '\n';
    Require(controls.slider->GetValue() == 100.0, "the keys moved the slider");
    Require(Builds() == built && delta.incrementalPublications == 99u && delta.recordsCloned == 99u,
            "each native slider key publishes one successor that re-reads the slider's record alone");
    verified.RequireNoDifference("every native slider key's publication equals a whole build");
}

namespace
{
// One frame of the embedded slider and drag script: every event raised (kind, id, sender path, old and new values), the
// property ids among them, the whole builds it made and the geometry marks it collapsed.
struct FrameEvents final
{
    std::vector<std::wstring> events;
    std::vector<long> ids;
    uint64_t builds    = 0u;
    uint64_t collapsed = 0u;
};

struct RecordedEvents final
{
    std::vector<std::wstring> events;
    std::vector<long> ids;
    static void Record(const DxUi::AccessibilityRaisedEventForTest& event, void* context) noexcept
    {
        try
        {
            auto& self = *static_cast<RecordedEvents*>(context);
            self.events.push_back(std::format(L"{} {} {} {} -> {}", static_cast<int>(event.kind), event.id, event.sender, event.oldValue, event.newValue));
            if (event.kind == DxUi::AccessibilityRaisedEventKindForTest::Property)
                self.ids.push_back(event.id);
        }
        catch (const std::bad_alloc&)
        {
            DxUi::Failure::FailAllocation();
        }
        catch (const std::exception&)
        {
            DxUi::Failure::FailUnexpected();
        }
    }
};

// FX6's slider and splitter in an embedded view with a real in-process client listening: 100 Right keys, then a 200 px drag.
// The splitter's change lays out an application's pane of 40 buttons beside it, as a pane layout does during a drag, so each
// move also moves 40 controls (more geometry marks than a host keeps, collapsed into one on their common parent). Each input
// is followed by a Prepare and an UpdateAccessibility (one frame). `whole` publishes every frame by a whole build.
[[nodiscard]] std::vector<FrameEvents> RunEmbeddedSliderAndDrag(bool whole)
{
    std::vector<FrameEvents> frames;
    RunEmbeddedGateCase("run the embedded slider and splitter script",
                        [&](DxUi::EmbeddedHost& view)
    {
        const SystemListeners listening(true);
        DxUi::DebugSetAccessibilityWholePublicationForTest(whole);
        const auto restoreWhole = wil::scope_exit([] { DxUi::DebugSetAccessibilityWholePublicationForTest(false); });
        constexpr UINT kWidth = 640u, kHeight = 720u;
        auto root    = std::make_unique<DxUi::Panel>();
        auto* slider = root->AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(8.0f, 76.0f, 240.0f, 112.0f));
        slider->SetAccessibleName(L"Level");
        slider->SetMinimum(0.0);
        slider->SetMaximum(200.0);
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(8.0f, 120.0f, 632.0f, 232.0f));
        splitter->SetAccessibleName(L"Panes");
        splitter->SetPosition(100.0f);
        auto* pane = root->AddChild<DxUi::Panel>();
        pane->SetBounds(D2D1::RectF(0.0f, 240.0f, 640.0f, 720.0f));
        std::vector<DxUi::Button*> paneItems;
        for (size_t item = 0u; item < 40u; ++item)
            paneItems.push_back(pane->AddChild<DxUi::Button>(L"Item " + std::to_wstring(item)));
        const auto layoutPane = [&paneItems](float position)
        {
            for (size_t item = 0u; item < paneItems.size(); ++item)
            {
                const float top = 244.0f + 11.0f * static_cast<float>(item);
                paneItems[item]->SetBounds(D2D1::RectF(position + 8.0f, top, position + 108.0f, top + 10.0f));
            }
        };
        layoutPane(splitter->GetPosition());
        splitter->SetOnChange([layoutPane](DxUi::SplitterChange change) { layoutPane(change.position); });
        view.Controls().SetRoot(std::move(root));
        view.Controls().SetFocusControl(slider, false);
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the slider and splitter");
        EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
        RequireSucceeded(bridge.Attach(true, 0x46583611ull), "attach the view's accessibility");
        UiaTest::Subscription subscription;
        subscription.properties = {UIA_RangeValueValuePropertyId, UIA_HasKeyboardFocusPropertyId};
        subscription.structure  = true;
        UiaTest::Client client(bridge.Hwnd(), subscription, [&bridge] { bridge.Pump(); });
        static_cast<void>(client.Root());
        static_cast<void>(client.WaitOutTheStream());
        RecordedEvents raised;
        DxUi::DebugSetAccessibilityEventRecorderForTest(&RecordedEvents::Record, &raised);
        const auto clearRecorder = wil::scope_exit([] { DxUi::DebugSetAccessibilityEventRecorderForTest(nullptr, nullptr); });
        // Marks collapse as the input lays out the pane, before the frame: counted from one frame's end to the next.
        uint64_t collapsedBefore = DxUi::Detail::Work().accessibility.geometryMarksCollapsed;
        const auto frame         = [&]
        {
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a frame");
            raised.events.clear();
            raised.ids.clear();
            const uint64_t before = Builds();
            Require(SUCCEEDED(bridge.Update()), "the frame's update publishes");
            const uint64_t collapsed = DxUi::Detail::Work().accessibility.geometryMarksCollapsed;
            frames.push_back({raised.events, raised.ids, Builds() - before, collapsed - collapsedBefore});
            collapsedBefore = collapsed;
        };
        frame(); // Settles the attachment.
        for (size_t step = 0u; step < 100u; ++step)
        {
            static_cast<void>(view.DispatchKey(VK_RIGHT, true, 0u));
            static_cast<void>(view.DispatchKey(VK_RIGHT, false, 0u));
            frame();
        }
        Require(view.DispatchPointer({DxUi::PointerAction::Down, 111.0f, 176.0f}), "the drag starts on the separator");
        frame();
        for (size_t pixel = 1u; pixel <= 200u; ++pixel)
        {
            Require(view.DispatchPointer({DxUi::PointerAction::Move, 111.0f + static_cast<float>(pixel), 176.0f}), "the drag moves");
            frame();
        }
        Require(view.DispatchPointer({DxUi::PointerAction::Up, 311.0f, 176.0f}), "the drag ends");
        frame();
        Require(slider->GetValue() == 100.0 && splitter->GetPosition() == 300.0f, "the script moved the slider and the splitter");
    });
    return frames;
}
} // namespace

// The gate's events (plan W8.2): with a real client listening, the slider keys and the drag (whose change lays out a pane of
// 40 buttons) raise exactly what a whole build of every frame raises, in the same order: each event's kind, id, sender, old
// and new values, focus and structure events included. The drag builds nothing while it moves, raises at most one property
// per frame (the splitter's RangeValue), collapses the pane's geometry marks, and builds once at the release.
void TestAccessibilityIncrementalEventsEqualWholeBuilds()
{
    const VerifiedPublications verified;
    const std::vector<FrameEvents> reference   = RunEmbeddedSliderAndDrag(true);
    const std::vector<FrameEvents> incremental = RunEmbeddedSliderAndDrag(false);
    Require(reference.size() == incremental.size() && reference.size() == 1u + 100u + 1u + 200u + 1u, "both runs publish every frame");
    size_t differing = 0u, events = 0u;
    for (size_t index = 0u; index < reference.size(); ++index)
    {
        events += reference[index].events.size();
        if (reference[index].events != incremental[index].events && differing++ == 0u)
        {
            std::cerr << "    [W8.2] frame " << index << " differs\n";
            for (const auto& event : reference[index].events)
                std::wcerr << L"      whole:       " << event << L'\n';
            for (const auto& event : incremental[index].events)
                std::wcerr << L"      incremental: " << event << L'\n';
        }
    }
    Require(differing == 0u, "every frame raises the events a whole build raises, with the same senders and values, in the same order");
    uint64_t keyBuilds = 0u, moveBuilds = 0u, moveCollapses = 0u;
    size_t mostMoveEvents = 0u;
    bool onlyRange        = true;
    for (size_t index = 1u; index <= 100u; ++index)
        keyBuilds += incremental[index].builds;
    for (size_t index = 102u; index < 302u; ++index)
    {
        moveBuilds += incremental[index].builds;
        moveCollapses += incremental[index].collapsed;
        mostMoveEvents = (std::max)(mostMoveEvents, incremental[index].ids.size());
        for (const long id : incremental[index].ids)
            onlyRange = onlyRange && id == UIA_RangeValueValuePropertyId;
    }
    std::cerr << "    [W8.2] embedded with a client: " << events << " events compared; slider keys whole builds " << keyBuilds << ", press "
              << incremental[101].builds << ", moves " << moveBuilds << " (at most " << mostMoveEvents << " property per frame, " << moveCollapses
              << " pane marks collapsed), release " << incremental[302].builds << '\n';
    Require(keyBuilds == 0u, "100 slider keys build no whole window");
    Require(incremental[101].builds == 0u, "the press's focus move builds no whole window");
    Require(moveBuilds == 0u && mostMoveEvents <= 1u && onlyRange,
            "the drag, laying out a 40-button pane, builds nothing while it moves and raises at most its RangeValue per frame");
    Require(moveCollapses != 0u, "the pane's geometry marks overflow and collapse onto their common parent");
    Require(incremental[302].builds == 1u, "the release builds the whole window once");
    verified.RequireNoDifference("every successor of the script equals a whole build");
}

// A control whose elements are laid out inside it (a tab header, a grid cell, a tree row, a reveal button) moves them when
// its name or value changes: retitling a tab moves the headers after it, which a successor rebuilds the rectangles of instead
// of keeping the ones before (W8.2 review R1).
void TestAccessibilityIncrementalTabTitleMovesHeaderRectangles()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    auto root  = std::make_unique<DxUi::Panel>();
    auto* tabs = root->AddChild<DxUi::TabControl>();
    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 180.0f));
    static_cast<void>(tabs->AddTab<DxUi::Panel>(L"A"));
    static_cast<void>(tabs->AddTab<DxUi::Panel>(L"B"));
    static_cast<void>(tabs->AddTab<DxUi::Panel>(L"C"));
    window.Host().SetRoot(std::move(root));
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), true), "the window publishes at each mutation");
    const auto clearInterest = wil::scope_exit([&] { static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), std::nullopt)); });
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();
    const VerifiedPublications verified;
    const uint64_t built = Builds();
    const DxUi::Detail::WorkCounterScope work;
    tabs->SetTabTitle(0u, L"A much, much longer first title");
    window.PumpMessages();
    const auto delta = work.Delta().accessibility;
    std::cerr << "    [W8.2] tab title: whole builds +" << (Builds() - built) << ", incremental " << delta.incrementalPublications << ", verified "
              << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
    Require(delta.incrementalPublications >= 1u && DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() >= 1u,
            "the retitle publishes a verified successor");
    verified.RequireNoDifference("a retitled tab's successor carries the moved header rectangles a whole build has");
}

// A slider's change callback that moves the keyboard focus publishes inside the callback (a focus transition publishes at
// once): that publication, and the one after it, equal whole builds (W8.2 review R3).
void TestAccessibilityIncrementalPublicationInsideSliderCallback()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    auto* other = const_cast<DxUi::Panel*>(static_cast<const DxUi::Panel*>(window.Host().GetRoot()))->AddChild<DxUi::Slider>();
    other->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 150.0f));
    other->SetAccessibleName(L"Other");
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), true), "the window publishes at each mutation");
    const auto clearInterest = wil::scope_exit([&] { static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), std::nullopt)); });
    window.Host().SetFocusControl(controls.slider, false);
    window.Host().RefreshAccessibilitySnapshot();
    window.PumpMessages();
    std::vector<std::wstring> inCallback;
    controls.slider->SetOnChange([&](DxUi::SliderChange)
    {
        window.Host().SetFocusControl(other, false);
        DxUi::DebugDescribeAccessibilitySnapshotForTest(DxUi::DebugHoldAccessibilitySnapshotForTest(window.Host()), inCallback);
    });
    const VerifiedPublications verified;
    Require(controls.slider->RequestValue(window.Host(), 42.0), "request a value");
    window.PumpMessages();
    std::vector<std::wstring> settled;
    DxUi::DebugDescribeAccessibilitySnapshotForTest(DxUi::DebugHoldAccessibilitySnapshotForTest(window.Host()), settled);
    const auto has = [](const std::vector<std::wstring>& lines, std::wstring_view text)
    { return std::ranges::any_of(lines, [text](const std::wstring& line) { return line.find(text) != std::wstring::npos; }); };
    std::cerr << "    [W8.2] slider callback: verified " << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
    Require(! inCallback.empty() && ! settled.empty(), "the window published inside the callback and after it");
    Require(controls.slider->GetValue() == 42.0 && has(settled, L"42"), "the settled publication reads the requested value");
    verified.RequireNoDifference("the publications inside and after the slider's callback equal whole builds");
}

// A window host detached with a flush still owed and attached again publishes a listened change: the marks of the
// attachment before are not the new one's (W8.2 review R4).
void TestAccessibilityIncrementalFlushRequestAcrossReattach()
{
    AttachedHostWindow window;
    Fx6::Controls controls;
    {
        const SystemListeners listening(true);
        controls.Build(window.Host());
        window.Host().RefreshAccessibilitySnapshot();
        window.PumpMessages();
        controls.label->SetText(L"pending"); // A flush is requested and never drained before the detach.
    }
    {
        const SystemListeners nobody(false);
        window.Host().Detach();
        Require(window.Host().Attach(window.Hwnd()), "the host attaches to its window again");
        controls.Build(window.Host());
        window.PumpMessages();
    }
    const SystemListeners listening(true);
    const uint64_t before = Publications();
    controls.label->SetText(L"heard");
    window.PumpMessages();
    const uint64_t published = Publications() - before;
    std::cerr << "    [W8.2] reattached host: publications after a listened label change " << published << '\n';
    Require(published >= 1u, "a listened change on a reattached host publishes");
}

// The marks cost a host one pointer (an empty deleter that is not final takes no room).
static_assert(sizeof(std::unique_ptr<DxUi::Detail::AccessibilityRecordMarks, DxUi::Detail::AccessibilityRecordMarksDeleter>) == sizeof(void*),
              "a host's record marks are one pointer wide");

// No client: a layout of 1000 SetBounds and a value change after one listened publication refresh the window once at most,
// then a client that arrives reads the current value and bounds, not those of the last publication (W8.2 review R6, and
// the W8.1 record: a client arriving after unheard changes reads them).
void TestAccessibilityIncrementalMarksWithoutClientStayCurrent()
{
    AttachedHostWindow window;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"Value");
    field->SetBounds(D2D1::RectF(8.0f, 600.0f, 240.0f, 628.0f));
    field->SetAccessibleName(L"Entry");
    std::vector<DxUi::Button*> buttons;
    for (size_t index = 0u; index < 100u; ++index)
        buttons.push_back(root->AddChild<DxUi::Button>(L"B" + std::to_wstring(index)));
    const auto layout = [&buttons](size_t frame)
    {
        const float left = 8.0f + 20.0f * static_cast<float>(frame);
        for (size_t index = 0u; index < buttons.size(); ++index)
            buttons[index]->SetBounds(D2D1::RectF(left, 4.0f * static_cast<float>(index), left + 50.0f, 4.0f * static_cast<float>(index) + 4.0f));
    };
    layout(0u);
    {
        const SystemListeners listening(true);
        window.Host().SetRoot(std::move(root));
        window.Host().RefreshAccessibilitySnapshot();
        window.PumpMessages();
    }
    const SystemListeners nobody(false);
    const uint64_t before = Publications();
    const DxUi::Detail::WorkCounterScope work;
    for (size_t frame = 1u; frame <= 10u; ++frame)
        layout(frame);
    const auto delta = work.Delta().accessibility;
    window.PumpMessages();
    field->SetText(L"current");
    window.PumpMessages();
    std::cerr << "    [W8.2] no client, 1000 SetBounds: mutations " << delta.mutations << ", marks while dormant " << delta.marksWhileDormant
              << ", publications " << (Publications() - before) << '\n';
    Require(delta.mutations <= 1u && delta.marksWhileDormant >= 998u, "1000 SetBounds with no client refresh the window once at most");
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId rootElement = client.Root();
    const UiaTest::ElementId entry       = FindNamed(client, rootElement, L"Entry");
    const UiaTest::ElementId first       = FindNamed(client, rootElement, L"B0");
    Require(entry != 0u && first != 0u, "the arriving client finds the field and the first button");
    Require(client.StringProperty(entry, UIA_ValueValuePropertyId) == L"current", "the arriving client reads the value set while nobody listened");
    const RECT entryRect  = client.Describe(entry).boundingRectangle;
    const RECT firstRect  = client.Describe(first).boundingRectangle;
    const float scale     = window.Host().GetDpi() / 96.0f;
    const LONG expected   = static_cast<LONG>(std::lround((8.0f + 200.0f - 8.0f) * scale));
    const LONG difference = firstRect.left - entryRect.left;
    std::cerr << "    [W8.2] arriving client: B0 left - field left " << difference << " (expected " << expected << ")\n";
    Require(difference >= expected - 1 && difference <= expected + 1, "the arriving client reads the bounds of the last layout");
}

namespace
{
// A window host whose every mutation publishes, with each incremental publication verified against a whole build.
class InterestedWindow final
{
public:
    explicit InterestedWindow(std::unique_ptr<DxUi::Panel> root)
    {
        _window.Host().SetRoot(std::move(root));
        Require(DxUi::DebugSetAccessibilityMutationInterestForTest(_window.Hwnd(), true), "the window publishes at each mutation");
        _window.Host().RefreshAccessibilitySnapshot();
        _window.PumpMessages();
    }
    ~InterestedWindow()
    {
        static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(_window.Hwnd(), std::nullopt));
    }
    InterestedWindow(const InterestedWindow&)            = delete;
    InterestedWindow& operator=(const InterestedWindow&) = delete;
    InterestedWindow(InterestedWindow&&)                 = delete;
    InterestedWindow& operator=(InterestedWindow&&)      = delete;
    [[nodiscard]] AttachedHostWindow& Window() noexcept
    {
        return _window;
    }

private:
    AttachedHostWindow _window;
};
} // namespace

// A Splitter laid out narrower clamps its position in OnBoundsChanged, and its range maximum follows its extent: a
// SetBounds marks only its geometry, so its record must be read again (W8.2 review A1: the successor kept position 400
// where a whole build has 146).
void TestAccessibilityIncrementalSplitterResizeRange()
{
    const SystemListeners nobody(false);
    auto root      = std::make_unique<DxUi::Panel>();
    auto* splitter = root->AddChild<DxUi::Splitter>();
    splitter->SetAccessibleName(L"Divider");
    splitter->SetOrientation(DxUi::SplitterOrientation::Vertical);
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 600.0f, 100.0f));
    splitter->SetPosition(400.0f);
    InterestedWindow interested(std::move(root));
    const VerifiedPublications verified;
    const uint64_t built = Builds();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 100.0f));
    interested.Window().PumpMessages();
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 900.0f, 100.0f));
    interested.Window().PumpMessages();
    std::cerr << "    [W8.2] splitter resized 600 -> 200 -> 900: position " << splitter->GetPosition() << ", whole builds +" << (Builds() - built)
              << ", verified " << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
    Require(DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() >= 2u, "both resizes publish verified successors");
    verified.RequireNoDifference("a resized splitter publishes the position and range a whole build has");
}

// Every library control that overrides OnBoundsChanged, resized through SetBounds (narrower, then wider) with a client
// interested: each publication equals a whole build of the same tree (W8.2 review A1 sweep). What a resize changes either
// reaches a re-read record (text, rich text, grid, tree, tab control, range value) or a child's own SetBounds.
void TestAccessibilityIncrementalResizeOfEveryBoundsOverride()
{
    const SystemListeners nobody(false);
    struct Case
    {
        const char* name;
        std::function<DxUi::Control*(DxUi::Panel&)> make;
    };
    const std::vector<Case> cases{
        {"FlexPanel",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* flex = root.AddChild<DxUi::FlexPanel>();
        static_cast<void>(flex->AddChild<DxUi::Button>(L"One"));
        static_cast<void>(flex->AddChild<DxUi::TextField>(L"Two"));
        return flex;
    }},
        {"Grid", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::Grid>(); }},
        {"FormField",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* field = root.AddChild<DxUi::FormField>(L"name", L"Name");
        return field;
    }},
        {"FormPanel", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::FormPanel>(); }},
        {"Splitter",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* splitter = root.AddChild<DxUi::Splitter>();
        splitter->SetPosition(300.0f);
        return splitter;
    }},
        {"NumericStepper", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::NumericStepper>(); }},
        {"ColorPicker", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::ColorPicker>(); }},
        {"PageHost",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* pages = root.AddChild<DxUi::PageHost>();
        pages->SetPage(std::make_unique<DxUi::Button>(L"Page"));
        return pages;
    }},
        {"Label", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::Label>(L"A label long enough to wrap when it is narrow"); }},
        {"Button", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::Button>(L"A button long enough to elide"); }},
        {"TagPicker",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* tags = root.AddChild<DxUi::TagPicker>();
        tags->SetOptions(L"All", {L"Red", L"Green", L"Blue"});
        tags->SetSelectedValues({L"Red", L"Blue"});
        return tags;
    }},
        {"MenuBar",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* menu = root.AddChild<DxUi::MenuBar>();
        menu->SetItems({DxUi::MenuBarItem{.text = L"File"}, DxUi::MenuBarItem{.text = L"Edit"}, DxUi::MenuBarItem{.text = L"View"}});
        return menu;
    }},
        {"TabControl",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* tabs = root.AddChild<DxUi::TabControl>();
        static_cast<void>(tabs->AddTab<DxUi::Panel>(L"First"));
        static_cast<void>(tabs->AddTab<DxUi::Panel>(L"Second"));
        static_cast<void>(tabs->AddTab<DxUi::Panel>(L"Third"));
        return tabs;
    }},
        {"StackPanel",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* stack = root.AddChild<DxUi::StackPanel>();
        static_cast<void>(stack->AddChild<DxUi::Button>(L"One"));
        static_cast<void>(stack->AddChild<DxUi::Button>(L"Two"));
        return stack;
    }},
        {"ScrollPanel",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* scroll = root.AddChild<DxUi::ScrollPanel>();
        for (size_t index = 0u; index < 10u; ++index)
            scroll->AddChild<DxUi::Button>(std::format(L"Row {}", index))
                ->SetBounds(D2D1::RectF(0.0f, 30.0f * static_cast<float>(index), 100.0f, 30.0f * static_cast<float>(index) + 28.0f));
        scroll->SetContentHeight(300.0f);
        scroll->SetScrollOffset(200.0f);
        return scroll;
    }},
        {"ComboBox",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* combo = root.AddChild<DxUi::ComboBox>();
        combo->SetItems({{L"a", L"Alpha"}, {L"b", L"Beta"}});
        combo->SetSelectedIndex(1u);
        return combo;
    }},
        {"Tree", [](DxUi::Panel& root) -> DxUi::Control* { return root.AddChild<DxUi::Tree>(); }},
        {"Grid (200 rows: its visible rows follow its height)",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        static ManyRowsGridModel model;
        auto* grid = root.AddChild<DxUi::Grid>();
        grid->SetModel(&model);
        return grid;
    }},
        {"Tree (60 rows: its visible rows follow its height)",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        static MutableTreeModel model;
        std::vector<DxUi::TreeItemData> items;
        for (uint64_t id = 1u; id <= 60u; ++id)
            items.push_back(DxUi::TreeItemData{.id = id, .text = std::format(L"Row {}", id)});
        model.SetVisibleItems(std::move(items));
        auto* tree = root.AddChild<DxUi::Tree>();
        tree->SetModel(&model);
        return tree;
    }},
        {"Label (elided when narrow)",
         [](DxUi::Panel& root) -> DxUi::Control*
    { return root.AddChild<DxUi::Label>(L"A label whose text is elided when the label is laid out narrower than its text"); }},
        {"TextField",
         [](DxUi::Panel& root) -> DxUi::Control*
    { return root.AddChild<DxUi::TextField>(L"A single-line field with text that scrolls when the field is narrow"); }},
        {"TextField (multiline)",
         [](DxUi::Panel& root) -> DxUi::Control*
    {
        auto* field = root.AddChild<DxUi::TextField>(L"A multiline field\nwith lines\nthat wrap and scroll when it is small");
        field->SetMultiline(true);
        return field;
    }},
        {"RichTextEditor",
         [](DxUi::Panel& root) -> DxUi::Control*
    { return root.AddChild<DxUi::RichTextEditor>(L"Rich text long enough to wrap differently at another width."); }},
    };
    size_t differing = 0u, incrementalMissing = 0u;
    for (const Case& test : cases)
    {
        auto root                = std::make_unique<DxUi::Panel>();
        DxUi::Control* const box = test.make(*root);
        box->SetBounds(D2D1::RectF(8.0f, 8.0f, 600.0f, 300.0f));
        InterestedWindow interested(std::move(root));
        const VerifiedPublications verified;
        const uint64_t built = Builds();
        box->SetBounds(D2D1::RectF(8.0f, 8.0f, 120.0f, 60.0f));
        interested.Window().PumpMessages();
        box->SetBounds(D2D1::RectF(8.0f, 8.0f, 700.0f, 400.0f));
        interested.Window().PumpMessages();
        const uint64_t verifiedCount = DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest();
        const uint64_t mismatches    = DxUi::DebugGetAccessibilityIncrementalMismatchCountForTest();
        std::cerr << "    [W8.2] resize " << test.name << ": incremental publications verified " << verifiedCount << ", differences " << mismatches
                  << ", whole builds " << (Builds() - built) << '\n';
        // Incremental, so the comparison proves something: a resize marks only geometry (W8.2 round 4: a model owner's geometry
        // change, a layout request and a tab control's resize are declared ones).
        if (verifiedCount < 2u || Builds() != built)
            ++incrementalMissing;
        if (mismatches != 0u)
        {
            ++differing;
            std::wcerr << L"      first difference: " << DxUi::DebugGetAccessibilityFirstIncrementalMismatchForTest() << L'\n';
        }
    }
    Require(differing == 0u, "every bounds override's resize publishes what a whole build has");
    Require(incrementalMissing == 0u, "every bounds override's resize publishes two incremental successors and no whole build");
}

// A PageIndicator that gains pages becomes an element (review probe): the publication equals a whole build.
void TestAccessibilityIncrementalPageIndicatorBecomesPaged()
{
    const SystemListeners nobody(false);
    auto root   = std::make_unique<DxUi::Panel>();
    auto* pages = root->AddChild<DxUi::PageIndicator>();
    pages->SetBounds(D2D1::RectF(8.0f, 40.0f, 200.0f, 64.0f));
    pages->SetPageCount(1u);
    InterestedWindow interested(std::move(root));
    const VerifiedPublications verified;
    const uint64_t publications = Publications();
    pages->SetPageCount(3u);
    interested.Window().PumpMessages();
    Require(Publications() > publications, "the page count publishes");
    verified.RequireNoDifference("a page indicator that gains pages publishes what a whole build has");
}

// Native coalescing of geometry-only publications (W8.2 reviews F3c, B2), on a window shown without activation: a second
// move within 100 ms is deferred and published once the layout settles, with nothing left waking; a client's query
// publishes a deferred move at once; a host hidden with a deferral pending publishes it at its next wake and holds no
// wake; a hidden window never defers.
void TestAccessibilityIncrementalNativeGeometryCoalescing()
{
    const SystemListeners nobody(false);
    const auto pumpUntil = [](AttachedHostWindow& window, auto done)
    {
        const ULONGLONG started = GetTickCount64();
        while (! done() && GetTickCount64() - started < 2'000u)
        {
            window.PumpMessages(20u);
            Sleep(1u);
        }
        return done();
    };
    const auto build = []
    {
        auto root   = std::make_unique<DxUi::Panel>();
        auto* field = root->AddChild<DxUi::TextField>(L"Value");
        field->SetBounds(D2D1::RectF(8.0f, 40.0f, 240.0f, 68.0f));
        field->SetAccessibleName(L"Entry");
        return std::pair{std::move(root), field};
    };
    enum class Then : uint8_t
    {
        Settle,
        Query,
        Hide,
    };
    for (const Then then : {Then::Settle, Then::Query, Then::Hide})
    {
        auto [root, field] = build();
        InterestedWindow interested(std::move(root));
        AttachedHostWindow& window = interested.Window();
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        const VerifiedPublications verified;
        const DxUi::Detail::WorkCounterScope work;
        field->SetBounds(D2D1::RectF(8.0f, 80.0f, 240.0f, 108.0f));
        window.PumpMessages();
        const uint64_t afterFirst = Publications();
        field->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 148.0f));
        window.PumpMessages();
        const uint64_t deferred = work.Delta().accessibility.geometryPublicationsDeferred;
        const bool waiting      = Publications() == afterFirst;
        if (then == Then::Query)
        {
            wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> owner;
            owner.attach(window.Host().DebugCreateAccessibilityProvider());
            wil::com_ptr_nothrow<IRawElementProviderFragment> fragment, first;
            Require(owner && SUCCEEDED(owner.query_to(fragment.put())) && SUCCEEDED(fragment->Navigate(NavigateDirection_FirstChild, first.put())),
                    "a client queries the window");
            Require(Publications() > afterFirst, "a client's query publishes the deferred move at once");
        }
        else if (then == Then::Hide)
            ShowWindow(window.Hwnd(), SW_HIDE);
        const bool published = pumpUntil(window, [&] { return Publications() > afterFirst; });
        // A query that published early leaves the dispatcher's entry for the deferral's wake, which then finds nothing to do and
        // leaves it.
        const bool quiet = pumpUntil(
            window, [&] { return window.Host().DebugGetFrameScheduleWakeForTest().kind == 0u && ! window.Host().DebugHasActiveAnimationSubscription(); });
        std::cerr << "    [W8.2] native coalescing ("
                  << (then == Then::Settle  ? "settle"
                      : then == Then::Query ? "query"
                                            : "hide")
                  << "): deferred " << deferred << ", waiting " << waiting << ", published " << published << ", schedule quiet " << quiet << '\n';
        Require(deferred >= 1u && waiting, "a second move within 100 ms on a shown window is deferred");
        Require(published, "the deferred move is published");
        Require(quiet, "nothing keeps the host's schedule awake afterwards");
        verified.RequireNoDifference("the coalesced publication equals a whole build");
    }
    {
        auto [root, field] = build();
        InterestedWindow interested(std::move(root));
        AttachedHostWindow& window = interested.Window(); // Never shown.
        const DxUi::Detail::WorkCounterScope work;
        const uint64_t before = Publications();
        field->SetBounds(D2D1::RectF(8.0f, 80.0f, 240.0f, 108.0f));
        window.PumpMessages();
        field->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 148.0f));
        window.PumpMessages();
        Require(work.Delta().accessibility.geometryPublicationsDeferred == 0u && Publications() == before + 2u,
                "a hidden window publishes every move at once: it presents no frame that could publish a deferral");
        Require(window.Host().DebugGetFrameScheduleWakeForTest().kind == 0u, "and holds no wake");
    }
}

namespace
{
// A control that animates every frame and repaints, so its host is paced (W3.6) while it holds the request.
class PacedAnimator final : public DxUi::Control
{
public:
    PacedAnimator()                                = default;
    PacedAnimator(const PacedAnimator&)            = delete;
    PacedAnimator& operator=(const PacedAnimator&) = delete;
    PacedAnimator(PacedAnimator&&)                 = delete;
    PacedAnimator& operator=(PacedAnimator&&)      = delete;
    ~PacedAnimator() override                      = default;
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost& host, uint64_t) override
    {
        ++ticks;
        Invalidate(host);
        return true;
    }
    size_t ticks = 0u;
};
} // namespace

// W8.2 with W3.6's pacer: a host animating through paced frames defers a geometry-only publication like any shown host; when
// it is hidden (its presentation, minimized, its own window) the deferral is published without waiting for a frame the host
// will not present, the pacer arms nothing, and no paced or timer wake follows. A pending accessibility deadline alone never
// arms the pacer: it is a deadline of the dispatcher, served by paced frames only while the host animates anyway.
void TestAccessibilityIncrementalGeometryOnAPacedHost()
{
    const SystemListeners nobody(false);
    enum class Hide : uint8_t
    {
        Presentation,
        Minimize,
        OwnWindow,
    };
    for (const Hide hide : {Hide::Presentation, Hide::Minimize, Hide::OwnWindow})
    {
        auto root      = std::make_unique<DxUi::Panel>();
        auto* field    = root->AddChild<DxUi::TextField>(L"Value");
        auto* animator = root->AddChild<PacedAnimator>();
        field->SetBounds(D2D1::RectF(8.0f, 40.0f, 240.0f, 68.0f));
        field->SetAccessibleName(L"Entry");
        animator->SetBounds(D2D1::RectF(250.0f, 8.0f, 270.0f, 28.0f));
        InterestedWindow interested(std::move(root));
        AttachedHostWindow& window = interested.Window();
        auto& host                 = window.Host();
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        Require(host.DebugGetFramePacerStateForTest().paced, "the shown host is paced");

        // A deadline only: the accessibility deferral wakes through the dispatcher, the pacer stays unarmed.
        const uint64_t before = Publications();
        field->SetBounds(D2D1::RectF(8.0f, 80.0f, 240.0f, 108.0f));
        window.PumpMessages();
        field->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 148.0f));
        {
            const DxUi::Detail::WorkCounterScope deadlineOnly;
            const auto state = host.DebugGetFramePacerStateForTest();
            Require(! state.continuous && ! state.skipTimerArmed, "a pending accessibility deadline does not make the host animate");
            const ULONGLONG started = GetTickCount64();
            while (Publications() <= before + 1u && GetTickCount64() - started < 2'000u)
                window.PumpMessages(20u);
            Require(Publications() > before + 1u, "the deferred move is published at its deadline");
            Require(deadlineOnly.Delta().wakes.pacedFrame == deadlineOnly.Delta().frame.pacerDeferredPaints && deadlineOnly.Delta().wakes.pacerTimer == 0u,
                    "an accessibility deadline wakes no pacer: paced frames come only for paints that had to defer");
        }

        // Animating, then a deferral, then the hide.
        host.RequestAnimation(*animator);
        const ULONGLONG started = GetTickCount64();
        while (animator->ticks < 3u && GetTickCount64() - started < 2'000u)
            window.PumpMessages(20u);
        Require(animator->ticks >= 3u && host.DebugGetFramePacerStateForTest().continuous, "the animator ticks through paced frames");
        const uint64_t beforeHide = Publications();
        field->SetBounds(D2D1::RectF(8.0f, 160.0f, 240.0f, 188.0f));
        window.PumpMessages(5u);
        field->SetBounds(D2D1::RectF(8.0f, 200.0f, 240.0f, 228.0f));
        switch (hide)
        {
            case Hide::Presentation: Require(host.SetNativePresentationVisible(false) == S_OK, "hide the presentation"); break;
            case Hide::Minimize: ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE); break;
            case Hide::OwnWindow: ShowWindow(window.Hwnd(), SW_HIDE); break;
        }
        const ULONGLONG hidden = GetTickCount64();
        while (Publications() <= beforeHide + 1u && GetTickCount64() - hidden < 2'000u)
            window.PumpMessages(20u);
        Require(Publications() > beforeHide + 1u, "a paced host that is hidden still publishes its deferred geometry");
        window.PumpMessages(100u);
        const DxUi::Detail::WorkCounterScope quiet;
        const ULONGLONG quietStarted = GetTickCount64();
        while (GetTickCount64() - quietStarted < 600u)
            window.PumpMessages(50u);
        const auto state = host.DebugGetFramePacerStateForTest();
        std::cerr << "    [W3.6+W8.2] hide " << static_cast<int>(hide) << ": quiet 600 ms, paced " << quiet.Delta().wakes.pacedFrame << ", skip "
                  << quiet.Delta().wakes.pacerTimer << ", timers " << quiet.Delta().wakes.animationTimer + quiet.Delta().wakes.deadlineTimer << '\n';
        // A window hidden without a schedule change (its own SW_HIDE) is observed hidden by at most one paced frame, as the frame
        // schedule allows; the explicit hides cancel the wait at once.
        const uint64_t allowedPaced = hide == Hide::OwnWindow ? 1u : 0u;
        Require(! state.waitArmed && ! state.continuous && ! state.skipTimerArmed && quiet.Delta().wakes.pacedFrame <= allowedPaced &&
                    quiet.Delta().wakes.pacerTimer == 0u && quiet.Delta().wakes.animationTimer + quiet.Delta().wakes.deadlineTimer == 0u,
                "nothing wakes a hidden paced host once its deferral is published");
        host.CancelAnimation(*animator);
        if (hide == Hide::Presentation)
            static_cast<void>(host.SetNativePresentationVisible(true));
    }
}

// An AttestedInvalidation alive while its host is destroyed (application code run by the invalidation destroyed it) does
// not write to the host's freed marks when it ends (W8.2 review; ASan reports the write without the guard).
void TestAccessibilityAttestationOutlivesItsHost()
{
    const SystemListeners nobody(false);
    auto window = std::make_unique<AttachedHostWindow>();
    window->Host().SetRoot(std::make_unique<DxUi::Panel>());
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window->Hwnd(), true), "the window publishes at each mutation");
    window->Host().RefreshAccessibilitySnapshot();
    window->PumpMessages();
    // The override is the thread's one: cleared before the window goes, as InterestedWindow does.
    static_cast<void>(DxUi::DebugSetAccessibilityMutationInterestForTest(window->Hwnd(), std::nullopt));
    {
        const DxUi::AttestedInvalidation attested(&window->Host());
        window.reset(); // The host and its marks are gone before the attestation ends.
    }
    Require(window == nullptr, "the host was destroyed inside the attestation");
}

namespace
{
template <typename Done> bool PumpUntil(AttachedHostWindow& window, Done done, ULONGLONG limitMs = 1'000u)
{
    const ULONGLONG started = GetTickCount64();
    while (! done() && GetTickCount64() - started < limitMs)
    {
        window.PumpMessages(20u);
        Sleep(1u);
    }
    return done();
}

[[nodiscard]] std::pair<std::unique_ptr<DxUi::Panel>, DxUi::TextField*> MovableField()
{
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"Value");
    field->SetBounds(D2D1::RectF(8.0f, 40.0f, 240.0f, 68.0f));
    field->SetAccessibleName(L"Entry");
    return {std::move(root), field};
}

[[nodiscard]] bool ScheduleQuiet(AttachedHostWindow& window)
{
    return window.Host().DebugGetFrameScheduleWakeForTest().kind == 0u && ! window.Host().DebugHasActiveAnimationSubscription();
}
} // namespace

// A window host that presents no frame never holds a geometry deferral (W8.2 review B2a), for each way it can stop
// presenting: presentation turned off (SetNativePresentationVisible, before the moves or with a deferral pending), the window
// minimized without activation, and the window hidden through an ancestor (a parent hidden without taking focus). Each
// deferred or later move is published, no wake is left subscribed, and each publication equals a whole build.
void TestAccessibilityIncrementalNoFrameHoldsNoDeferral()
{
    const SystemListeners nobody(false);
    enum class Stop : uint8_t
    {
        PresentationOffBefore,
        PresentationOffPending,
        Minimized,
        AncestorHidden,
    };
    for (const Stop stop : {Stop::PresentationOffBefore, Stop::PresentationOffPending, Stop::Minimized, Stop::AncestorHidden})
    {
        const char* const name = stop == Stop::PresentationOffBefore    ? "presentation off before the moves"
                                 : stop == Stop::PresentationOffPending ? "presentation off with a deferral pending"
                                 : stop == Stop::Minimized              ? "minimized with a deferral pending"
                                                                        : "hidden through an ancestor with a deferral pending";
        auto [root, field]     = MovableField();
        InterestedWindow interested(std::move(root));
        AttachedHostWindow& window = interested.Window();
        wil::unique_hwnd parent;
        if (stop == Stop::AncestorHidden)
        {
            parent.reset(CreateWindowExW(
                WS_EX_NOACTIVATE, L"STATIC", L"W8.2 parent", WS_OVERLAPPED, -32000, -32000, 360, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
            Require(parent != nullptr, "the parent window is created");
            SetWindowLongPtrW(window.Hwnd(), GWL_STYLE, WS_CHILD | WS_VISIBLE);
            Require(SetParent(window.Hwnd(), parent.get()) != nullptr || GetLastError() == ERROR_SUCCESS, "the host window becomes a child");
            SetWindowPos(window.Hwnd(), nullptr, 0, 0, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
            // A wake that ran while the parent was still hidden left the host held as hidden through its ancestor until its
            // next look (at most a second): one wake now sees it visible, as that look would, and a restore releases the latch.
            SendMessageW(window.Hwnd(), WM_SIZE, SIZE_RESTORED, MAKELPARAM(320, 200));
            static_cast<void>(window.Host().DebugAnimationTickForTest(window.Host().GetAnimationTickMs()));
        }
        else
            ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        const VerifiedPublications verified;
        const DxUi::Detail::WorkCounterScope work;
        if (stop == Stop::PresentationOffBefore)
            Require(window.Host().SetNativePresentationVisible(false) == S_OK, "presentation off before the moves");
        field->SetBounds(D2D1::RectF(8.0f, 80.0f, 240.0f, 108.0f));
        window.PumpMessages();
        const uint64_t afterFirst = Publications();
        field->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 148.0f));
        window.PumpMessages();
        const uint64_t deferred = work.Delta().accessibility.geometryPublicationsDeferred;
        switch (stop)
        {
            case Stop::PresentationOffBefore: break;
            case Stop::PresentationOffPending:
                Require(window.Host().SetNativePresentationVisible(false) == S_OK, "presentation off with the deferral pending");
                break;
            case Stop::Minimized: ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE); break;
            case Stop::AncestorHidden: ShowWindow(parent.get(), SW_HIDE); break;
        }
        const bool published = PumpUntil(window, [&] { return Publications() > afterFirst; });
        const bool quiet     = PumpUntil(window, [&] { return ScheduleQuiet(window); });
        std::cerr << "    [W8.2] " << name << ": deferred " << deferred << ", published " << published << ", schedule quiet " << quiet << '\n';
        if (stop == Stop::PresentationOffBefore)
            Require(deferred == 0u, "a host whose presentation is off never defers");
        else
            Require(deferred >= 1u, "the second move within 100 ms on a presenting window is deferred");
        Require(published, "the move is published while the host presents no frame");
        Require(quiet, "and no wake is held for it");
        verified.RequireNoDifference("the publication equals a whole build");
        static_cast<void>(window.Host().SetNativePresentationVisible(true));
        if (stop == Stop::AncestorHidden)
        {
            SetParent(window.Hwnd(), nullptr);
            ShowWindow(window.Hwnd(), SW_HIDE);
        }
        else
            ShowWindow(window.Hwnd(), SW_HIDE);
        window.PumpMessages();
    }
}

// A real UI Automation client of a native window queries off the owner thread and reads the published snapshot (W8.2 review
// I1b). While a geometry-only publication is deferred (the host's clock set far ahead, so the settle wake is far away) the
// client's first read is the last published bounds, and that read asks the owner thread to publish: the next read, after the
// owner's next message turn, is current.
void TestAccessibilityIncrementalClientQueryDuringDeferral()
{
    const SystemListeners nobody(false);
    auto [root, field] = MovableField();
    InterestedWindow interested(std::move(root));
    AttachedHostWindow& window = interested.Window();
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(20u); });
    const UiaTest::ElementId entry = FindNamed(client, client.Root(), L"Entry");
    Require(entry != 0u, "the client finds the field");
    window.Host().DebugSetAnimationClockForTest(GetTickCount64() + 600'000u);
    const auto restoreClock = wil::scope_exit([&] { window.Host().DebugSetAnimationClockForTest(std::nullopt); });
    field->SetBounds(D2D1::RectF(8.0f, 80.0f, 240.0f, 108.0f));
    window.PumpMessages(20u);
    const RECT first = client.Describe(entry).boundingRectangle;
    window.Host().DebugSetAnimationClockForTest(GetTickCount64() + 600'010u);
    const DxUi::Detail::WorkCounterScope work;
    field->SetBounds(D2D1::RectF(8.0f, 120.0f, 240.0f, 148.0f));
    window.PumpMessages(20u);
    const uint64_t deferred = work.Delta().accessibility.geometryPublicationsDeferred;
    const RECT stale        = client.Describe(entry).boundingRectangle; // posts the owner's publication
    window.PumpMessages(20u);
    const RECT current = client.Describe(entry).boundingRectangle;
    const LONG shift   = std::lround(40.0f * window.Host().GetDpi() / 96.0f);
    std::cerr << "    [W8.2] client query during a deferral: deferred " << deferred << ", top after first move " << first.top << ", first read after the "
              << "second " << stale.top << ", next read " << current.top << " (expected " << (first.top + shift) << ")\n";
    Require(deferred >= 1u, "the second move within 100 ms is deferred");
    Require(current.top - first.top == shift, "a client's query during a deferral causes the publication its next read sees");
    ShowWindow(window.Hwnd(), SW_HIDE);
    window.PumpMessages();
}

// A pane of 8, 40 and 200 sliders moved five times (a splitter drag laying it out), beside 20 labels: incremental on every
// move, equal to whole builds (W8.2 review A5: a slider's record does not follow its bounds, so a moved slider re-reads
// nothing; before, every range-value record of a moved subtree was re-read and 40 sliders made every move a whole build).
void TestAccessibilityIncrementalPaneOfSliders()
{
    const SystemListeners nobody(false);
    for (const size_t sliders : {size_t{8u}, size_t{40u}, size_t{200u}})
    {
        auto root  = std::make_unique<DxUi::Panel>();
        auto* pane = root->AddChild<DxUi::Panel>();
        pane->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 2000.0f));
        for (size_t index = 0u; index < 20u; ++index)
            root->AddChild<DxUi::Label>(std::format(L"Outside {}", index))
                ->SetBounds(D2D1::RectF(400.0f, 10.0f * static_cast<float>(index), 500.0f, 10.0f * static_cast<float>(index) + 9.0f));
        std::vector<DxUi::Slider*> moved;
        for (size_t index = 0u; index < sliders; ++index)
        {
            auto* slider = pane->AddChild<DxUi::Slider>();
            slider->SetAccessibleName(std::format(L"S{}", index));
            moved.push_back(slider);
        }
        const auto layout = [&moved](float left)
        {
            for (size_t index = 0u; index < moved.size(); ++index)
                moved[index]->SetBounds(D2D1::RectF(left, 9.0f * static_cast<float>(index), left + 100.0f, 9.0f * static_cast<float>(index) + 8.0f));
        };
        layout(0.0f);
        InterestedWindow interested(std::move(root)); // Never shown: no deferral, every flush publishes.
        const VerifiedPublications verified;
        const DxUi::Detail::WorkCounterScope work;
        const uint64_t built = Builds();
        for (size_t frame = 1u; frame <= 5u; ++frame)
        {
            layout(4.0f * static_cast<float>(frame));
            interested.Window().PumpMessages();
        }
        const auto delta = work.Delta().accessibility;
        std::cerr << "    [W8.2] pane of " << sliders << " sliders moved 5 times: whole builds +" << (Builds() - built) << ", incremental "
                  << delta.incrementalPublications << ", records re-read " << delta.recordsCloned << ", verified "
                  << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
        Require(Builds() == built && delta.incrementalPublications == 5u, "every move of the pane is an incremental publication");
        verified.RequireNoDifference("the pane's publications equal whole builds");
    }
}

// Range-value controls moved by a resized ancestor's layout (a StackPanel laid out narrower: a splitter and a page indicator),
// and a scroll panel scrolled over a splitter and a field then resized so its offset clamps (W8.2 review probes): each
// publication equals a whole build.
void TestAccessibilityIncrementalNestedResizes()
{
    const SystemListeners nobody(false);
    {
        auto root   = std::make_unique<DxUi::Panel>();
        auto* stack = root->AddChild<DxUi::StackPanel>();
        stack->SetBounds(D2D1::RectF(0.0f, 0.0f, 600.0f, 300.0f));
        auto* splitter = stack->AddChild<DxUi::Splitter>();
        splitter->SetAccessibleName(L"Inner");
        splitter->SetOrientation(DxUi::SplitterOrientation::Vertical);
        splitter->SetPosition(450.0f);
        auto* pages = stack->AddChild<DxUi::PageIndicator>();
        pages->SetPageCount(4u);
        stack->SetChildExtent(splitter, 100.0f);
        stack->SetChildExtent(pages, 24.0f);
        stack->ApplyLayout();
        InterestedWindow interested(std::move(root));
        const VerifiedPublications verified;
        stack->SetBounds(D2D1::RectF(0.0f, 0.0f, 150.0f, 300.0f));
        stack->ApplyLayout(); // The application lays the stack out for its new width.
        interested.Window().PumpMessages();
        std::cerr << "    [W8.2] stack resized: splitter position " << splitter->GetPosition() << ", verified "
                  << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
        verified.RequireNoDifference("a splitter laid out by a resized stack equals a whole build");
    }
    {
        auto root    = std::make_unique<DxUi::Panel>();
        auto* scroll = root->AddChild<DxUi::ScrollPanel>();
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 200.0f));
        auto* splitter = scroll->AddChild<DxUi::Splitter>();
        splitter->SetAccessibleName(L"Scrolled");
        splitter->SetBounds(D2D1::RectF(0.0f, 100.0f, 280.0f, 400.0f));
        auto* field = scroll->AddChild<DxUi::TextField>(L"Field");
        field->SetBounds(D2D1::RectF(0.0f, 420.0f, 280.0f, 448.0f));
        scroll->SetContentHeight(600.0f);
        InterestedWindow interested(std::move(root));
        const VerifiedPublications verified;
        scroll->SetScrollOffset(250.0f);
        interested.Window().PumpMessages();
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 80.0f));
        interested.Window().PumpMessages();
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 590.0f));
        interested.Window().PumpMessages();
        std::cerr << "    [W8.2] scroll panel scrolled and resized: verified " << DxUi::DebugGetAccessibilityIncrementalVerificationCountForTest() << '\n';
        verified.RequireNoDifference("a scrolled and resized scroll panel equals a whole build");
    }
}
