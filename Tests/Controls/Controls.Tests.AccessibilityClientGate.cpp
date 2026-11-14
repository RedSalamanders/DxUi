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
    const uint64_t before = Builds();
    const DxUi::Detail::WorkCounterScope work;
    MutateFx6(controls, window);
    Require(work.Delta().accessibility.snapshotBuilds == 0u && work.Delta().accessibility.snapshotNodes == 0u,
            "FX6 with no client: the work counters count no snapshot build or record");
    std::cerr << "    [FX6] no client: builds +" << (Builds() - before) << " for " << Fx6::kMutations << " mutations\n";
    Require(Builds() == before, "FX6 with no client: 100 mutations build no snapshot (G9)");
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const UiaTest::ElementId root = client.Root();
    RequireFinalFx6Tree(client, root, "a client attaching after 100 unobserved mutations reads the current tree");
    std::cerr << "    [FX6] late client: builds +" << (Builds() - before) << '\n';
    Require(Builds() == before + 1u, "the late client's queries build one snapshot");
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
    const uint64_t before = Builds();
    MutateFx6(controls, window);
    std::cerr << "    [FX6] after a released probe: builds +" << (Builds() - before) << '\n';
    Require(Builds() >= before + Fx6::kMutations, "a window a client requested keeps building at each mutation for the attachment");
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
        RequireSucceeded(view.Attach(std::move(pool)), "attach FX6 embedded to borrowed graphics");
        Fx6::Controls controls;
        controls.Build(view.Controls());
        constexpr UINT kWidth = 320u, kHeight = 200u;
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare FX6 embedded");
        {
            EmbeddedUiaTest::Bridge bridge(view, kWidth, kHeight);
            const uint64_t beforeAttach = Builds();
            RequireSucceeded(bridge.Attach(false, 0x46583601ull), "attach FX6 embedded accessibility");
            Require(Builds() == beforeAttach + 1u, "attaching accessibility builds the first snapshot");
            const uint64_t before = Builds();
            const DxUi::Detail::WorkCounterScope work;
            for (size_t index = 0u; index < Fx6::kMutations; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare an FX6 frame");
                Require(bridge.Update() == S_OK, "UpdateAccessibility accepts every changed frame");
                Require(bridge.Update() == S_FALSE, "an unchanged frame's update is a no-op");
            }
            std::cerr << "    [FX6] embedded, every frame, no client: builds +" << (Builds() - before) << '\n';
            Require(Builds() == before, "FX6 embedded with no client: UpdateAccessibility every frame builds no snapshot (G9)");
            Require(work.Delta().accessibility.snapshotBuilds == 0u, "FX6 embedded with no client: the work counters count no snapshot build");
            {
                UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
                const UiaTest::ElementId root = client.Root();
                RequireFinalFx6Tree(client, root, "a client of the embedded view reads the current tree after deferred updates");
            }
            std::cerr << "    [FX6] embedded late client: builds +" << (Builds() - before) << '\n';
            Require(Builds() == before + 1u, "the embedded client's first request builds the recorded update once");
            const uint64_t requested = Builds();
            for (size_t index = 0u; index < 3u; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a frame of the requested view");
                Require(bridge.Update() == S_OK, "a requested view's frame publishes");
            }
            Require(Builds() == requested + 3u, "once requested, each changed frame builds, with no listener on the desktop");
            const SystemListeners listening(true);
            const uint64_t listened = Builds();
            for (size_t index = 3u; index < 6u; ++index)
            {
                controls.Mutate(index);
                RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare a listened FX6 frame");
                Require(bridge.Update() == S_OK, "a listened frame publishes");
            }
            Require(Builds() == listened + 3u, "with a listening client each changed frame builds, as before the gate");
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
        RequireSucceeded(view.Attach(std::move(pool)), "attach an embedded gate case to borrowed graphics");
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
    const uint64_t before = Builds();
    fields.first->SetText(L"after the query client");
    window.PumpMessages();
    Require(Builds() > before, "a window an out-of-process client requested keeps building at a mutation");
    static_cast<void>(TerminateProcess(process.hProcess, 0u));
    static_cast<void>(WaitForSingleObject(process.hProcess, 10000u));
    const uint64_t ended = Builds();
    fields.first->SetText(L"after the query client ended");
    window.PumpMessages();
    Require(Builds() > ended, "and keeps building after that client ended, for the attachment");
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
        const uint64_t before = Builds();
        RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare the next frame");
        Require(bridge.Update() == S_OK && Builds() == before + 1u, "the next update of the requested view builds");
        Require(client.StringProperty(first, UIA_ValueValuePropertyId) == L"y", "and the client reads the current tree");
    });
}
