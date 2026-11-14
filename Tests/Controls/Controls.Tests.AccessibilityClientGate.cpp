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
using AdviseRecord = DxUi::AccessibilityAdviseRecordForTest;

[[nodiscard]] std::vector<AdviseRecord> AdviseRecordsOf(HWND hwnd)
{
    std::array<AdviseRecord, 256u> records{};
    const size_t total = DxUi::DebugCopyAccessibilityAdviseRecordsForTest(records);
    std::vector<AdviseRecord> result;
    for (size_t index = 0u; index < (std::min)(total, records.size()); ++index)
        if (records[index].hwnd == hwnd)
            result.push_back(records[index]);
    return result;
}

[[nodiscard]] std::string DescribeAdvise(const std::vector<AdviseRecord>& records, bool added)
{
    std::string text;
    for (const AdviseRecord& record : records)
    {
        if (record.added != added)
            continue;
        text += " " + std::to_string(record.eventId);
        if (record.propertyCount != 0u)
        {
            text += "[";
            for (uint32_t index = 0u; index < (std::min)(record.propertyCount, static_cast<uint32_t>(record.properties.size())); ++index)
                text += (index ? "," : "") + std::to_string(record.properties[index]);
            text += "]";
        }
    }
    return text.empty() ? std::string(" none") : text;
}

[[nodiscard]] wil::com_ptr_nothrow<IUIAutomationElement> WindowElement(UiaTest::Session& session)
{
    wil::com_ptr_nothrow<IUIAutomationElement> element;
    static_cast<void>(session.automation->ElementFromHandle(session.target, element.put()));
    return element;
}

struct SubscriptionKind
{
    const char* name;
    std::function<HRESULT(UiaTest::Session&)> add;
};

[[nodiscard]] std::vector<SubscriptionKind> SubscriptionKinds()
{
    const auto automationAt = [](TreeScope scope, bool cached)
    {
        return [scope, cached](UiaTest::Session& session) -> HRESULT
        {
            const auto window = WindowElement(session);
            return session.automation->AddAutomationEventHandler(
                UIA_Invoke_InvokedEventId, window.get(), scope, cached ? session.cache.get() : nullptr, session.recorder);
        };
    };
    std::vector<SubscriptionKind> kinds;
    kinds.push_back({"automation event, window, element scope", automationAt(TreeScope_Element, true)});
    kinds.push_back({"automation event, window, children scope", automationAt(TreeScope_Children, true)});
    kinds.push_back({"automation event, window, descendants scope", automationAt(TreeScope_Descendants, true)});
    kinds.push_back({"automation event, window, subtree scope", automationAt(TreeScope_Subtree, true)});
    kinds.push_back({"automation event, window, subtree scope, uncached", automationAt(TreeScope_Subtree, false)});
    kinds.push_back({"automation event, desktop root, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomationElement> desktop;
        RETURN_IF_FAILED(session.automation->GetRootElement(desktop.put()));
        return session.automation->AddAutomationEventHandler(
            UIA_Invoke_InvokedEventId, desktop.get(), TreeScope_Subtree, session.cache.get(), session.recorder);
    }});
    kinds.push_back({"automation event, child element, element scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        const auto window = WindowElement(session);
        wil::com_ptr_nothrow<IUIAutomationElement> child;
        RETURN_IF_FAILED(session.walker->GetFirstChildElement(window.get(), child.put()));
        if (! child)
            return E_FAIL;
        return session.automation->AddAutomationEventHandler(UIA_Invoke_InvokedEventId, child.get(), TreeScope_Element, session.cache.get(), session.recorder);
    }});
    kinds.push_back({"property changed, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        const auto window              = WindowElement(session);
        std::array<PROPERTYID, 2u> ids = {{UIA_NamePropertyId, UIA_ValueValuePropertyId}};
        return session.automation->AddPropertyChangedEventHandlerNativeArray(
            window.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size()));
    }});
    kinds.push_back({"property changed, desktop root, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomationElement> desktop;
        RETURN_IF_FAILED(session.automation->GetRootElement(desktop.put()));
        std::array<PROPERTYID, 1u> ids = {{UIA_NamePropertyId}};
        return session.automation->AddPropertyChangedEventHandlerNativeArray(
            desktop.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size()));
    }});
    kinds.push_back({"structure changed, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    { return session.automation->AddStructureChangedEventHandler(WindowElement(session).get(), TreeScope_Subtree, session.cache.get(), session.recorder); }});
    kinds.push_back({"focus changed, cached",
                     [](UiaTest::Session& session) -> HRESULT
    { return session.automation->AddFocusChangedEventHandler(session.cache.get(), session.recorder); }});

    kinds.push_back({"text edit text changed, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomation3> automation3;
        RETURN_IF_FAILED(session.automation.query_to(automation3.put()));
        return automation3->AddTextEditTextChangedEventHandler(
            WindowElement(session).get(), TreeScope_Subtree, TextEditChangeType_AutoCorrect, session.cache.get(), session.recorder);
    }});
    kinds.push_back({"notification, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomation5> automation5;
        RETURN_IF_FAILED(session.automation.query_to(automation5.put()));
        return automation5->AddNotificationEventHandler(WindowElement(session).get(), TreeScope_Subtree, session.cache.get(), session.recorder);
    }});
    kinds.push_back({"active text position changed, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomation6> automation6;
        RETURN_IF_FAILED(session.automation.query_to(automation6.put()));
        return automation6->AddActiveTextPositionChangedEventHandler(WindowElement(session).get(), TreeScope_Subtree, session.cache.get(), session.recorder);
    }});
    kinds.push_back({"event handler group, window, subtree scope",
                     [](UiaTest::Session& session) -> HRESULT
    {
        wil::com_ptr_nothrow<IUIAutomation6> automation6;
        RETURN_IF_FAILED(session.automation.query_to(automation6.put()));
        wil::com_ptr_nothrow<IUIAutomationEventHandlerGroup> group;
        RETURN_IF_FAILED(automation6->CreateEventHandlerGroup(group.put()));
        RETURN_IF_FAILED(group->AddAutomationEventHandler(UIA_Invoke_InvokedEventId, TreeScope_Subtree, session.cache.get(), session.recorder));
        std::array<PROPERTYID, 1u> ids = {{UIA_NamePropertyId}};
        RETURN_IF_FAILED(
            group->AddPropertyChangedEventHandler(TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size())));
        return automation6->AddEventHandlerGroup(WindowElement(session).get(), group.get());
    }});
    return kinds;
}
} // namespace

// UI Automation advises a window's root provider of every subscription scoped to the window or its elements, and of its end;
// it advises none of a subscription at the desktop root or of a focus-changed handler, also for a window it discovers after
// them (printed: the reason the client gate keeps UiaClientsAreListening, see AccessibilityClientsMayListen).
void TestAccessibilityClientGateAdviseCoversWindowScopedSubscriptions()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Advise probe");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
    auto* field = root->AddChild<TextField>(L"probe text");
    field->SetBounds(D2D1::RectF(0.0f, 36.0f, 240.0f, 64.0f));
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    const HWND hwnd = window.Hwnd();
    Require(DebugGetAccessibilityEventListenerCountForTest(hwnd) == 0u, "a query-only client is advised of no subscription");
    bool windowScopedAdvised = true;
    for (const SubscriptionKind& kind : SubscriptionKinds())
    {
        DebugResetAccessibilityAdviseRecordsForTest();
        const HRESULT added = client.Perform(kind.name, kind.add);
        static_cast<void>(client.WaitUntil(3000u, [&] { return DebugGetAccessibilityEventListenerCountForTest(hwnd) != 0u; }));
        client.Settle(300u);
        const auto afterAdd      = AdviseRecordsOf(hwnd);
        const uint64_t listening = DebugGetAccessibilityEventListenerCountForTest(hwnd);
        const HRESULT removed = client.Perform("remove every handler", [](UiaTest::Session& session) { return session.automation->RemoveAllEventHandlers(); });
        static_cast<void>(client.WaitUntil(3000u, [&] { return DebugGetAccessibilityEventListenerCountForTest(hwnd) == 0u; }));
        client.Settle(300u);
        const auto afterRemove   = AdviseRecordsOf(hwnd);
        const uint64_t remaining = DebugGetAccessibilityEventListenerCountForTest(hwnd);
        std::cerr << "    [advise] " << kind.name << ": add hr=0x" << std::hex << static_cast<unsigned long>(added) << " remove hr=0x"
                  << static_cast<unsigned long>(removed) << std::dec << " added:" << DescribeAdvise(afterAdd, true)
                  << " | removed:" << DescribeAdvise(afterRemove, false) << " | listeners " << listening << " -> " << remaining << '\n';
        const std::string_view name(kind.name);
        if (name.find("desktop") == std::string_view::npos && name.find("focus") == std::string_view::npos)
            windowScopedAdvised = windowScopedAdvised && SUCCEEDED(added) && listening != 0u && remaining == 0u;
    }
    std::cerr << "    [advise] UiaClientsAreListening=" << (UiaClientsAreListening() != FALSE) << '\n';
    // A screen reader subscribes at the desktop before a window exists. Is a window it discovers later advised?
    {
        DebugResetAccessibilityAdviseRecordsForTest();
        const HRESULT added = client.Perform("subscribe at the desktop and to focus",
                                             [](UiaTest::Session& session) -> HRESULT
        {
            wil::com_ptr_nothrow<IUIAutomationElement> desktop;
            RETURN_IF_FAILED(session.automation->GetRootElement(desktop.put()));
            RETURN_IF_FAILED(session.automation->AddAutomationEventHandler(
                UIA_Invoke_InvokedEventId, desktop.get(), TreeScope_Subtree, session.cache.get(), session.recorder));
            std::array<PROPERTYID, 1u> ids = {{UIA_NamePropertyId}};
            RETURN_IF_FAILED(session.automation->AddPropertyChangedEventHandlerNativeArray(
                desktop.get(), TreeScope_Subtree, session.cache.get(), session.recorder, ids.data(), static_cast<int>(ids.size())));
            RETURN_IF_FAILED(session.automation->AddStructureChangedEventHandler(desktop.get(), TreeScope_Subtree, session.cache.get(), session.recorder));
            return session.automation->AddFocusChangedEventHandler(session.cache.get(), session.recorder);
        });
        AttachedHostWindow later;
        auto laterRoot = std::make_unique<Panel>();
        laterRoot->AddChild<Button>(L"Discovered later")->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
        later.Host().SetRoot(std::move(laterRoot));
        later.PumpMessages();
        const HWND laterHwnd = later.Hwnd();
        const auto pumpBoth  = [&] { later.PumpMessages(); };
        UiaTest::Client discoverer(laterHwnd, {}, pumpBoth);
        static_cast<void>(discoverer.Root());
        discoverer.Settle(1000u);
        client.Settle(300u);
        const auto discovered = AdviseRecordsOf(laterHwnd);
        std::cerr << "    [advise] desktop and focus subscription made before a window is discovered: add hr=0x" << std::hex
                  << static_cast<unsigned long>(added) << std::dec << " advised to the discovered window:" << DescribeAdvise(discovered, true)
                  << " | listeners " << DebugGetAccessibilityEventListenerCountForTest(laterHwnd) << '\n';
        static_cast<void>(client.Perform("remove every handler", [](UiaTest::Session& session) { return session.automation->RemoveAllEventHandlers(); }));
        client.Settle(300u);
    }
    Require(windowScopedAdvised, "UI Automation advises a window's root of each subscription scoped to it and of its removal");
}

namespace
{
constexpr std::wstring_view kSubscriberChildPrefix = L"--uia-subscriber-child=";
constexpr std::wstring_view kQueryChildPrefix      = L"--uia-query-child=";

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

// A UI Automation client in a process of its own: it subscribes to an event of the window it is given, signals the event it
// inherited, and waits to be terminated without unsubscribing, as a client that crashes does. With the query prefix it only
// reads the window's elements (no subscription), then signals and waits.
int RunAccessibilityClientGateSubscriberChild(int argc, wchar_t** argv) noexcept
{
    if (argc != 2 || ! argv[1])
        return -1;
    const std::wstring_view argument = argv[1];
    const bool queryOnly             = argument.rfind(kQueryChildPrefix, 0) == 0;
    if (! queryOnly && argument.rfind(kSubscriberChildPrefix, 0) != 0)
        return -1;
    const std::wstring_view arguments = argument.substr(queryOnly ? kQueryChildPrefix.size() : kSubscriberChildPrefix.size());
    const size_t comma                = arguments.find(L',');
    const auto window                 = ParseDecimal(arguments.substr(0, comma));
    const auto ready                  = comma == std::wstring_view::npos ? std::nullopt : ParseDecimal(arguments.substr(comma + 1u));
    if (! window || ! ready)
        return 2;
    const auto com = wil::CoInitializeEx_failfast(COINIT_MULTITHREADED);
    wil::com_ptr_nothrow<IUIAutomation> automation;
    wil::com_ptr_nothrow<IUIAutomationElement> element;
    wil::com_ptr_nothrow<UiaTest::Recorder> recorder;
    recorder.attach(Microsoft::WRL::Make<UiaTest::Recorder>().Detach());
    HRESULT hr = recorder ? UiaTest::CreateAutomationClient(automation.put()) : E_OUTOFMEMORY;
    if (SUCCEEDED(hr))
        hr = automation->ElementFromHandle(reinterpret_cast<HWND>(*window), element.put());
    if (SUCCEEDED(hr) && queryOnly)
    {
        wil::com_ptr_nothrow<IUIAutomationCondition> all;
        wil::com_ptr_nothrow<IUIAutomationElementArray> found;
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
    }
    else if (SUCCEEDED(hr))
        hr = automation->AddAutomationEventHandler(UIA_Invoke_InvokedEventId, element.get(), TreeScope_Subtree, nullptr, recorder.get());
    if (FAILED(hr))
        return 3;
    static_cast<void>(SetEvent(reinterpret_cast<HANDLE>(*ready)));
    Sleep(INFINITE);
    return 0;
}

// A client process that ends without unsubscribing: UI Automation reports its subscription's end, so the advised count of a
// client that crashed does not keep mutations building.
void TestAccessibilityClientGateTerminatedClientSubscriptionEnds()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Terminated client probe");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();
    const HWND hwnd = window.Hwnd();
    Require(DebugGetAccessibilityEventListenerCountForTest(hwnd) == 0u, "no subscription before the client process starts");
    SECURITY_ATTRIBUTES inheritable{sizeof(inheritable), nullptr, TRUE};
    wil::unique_event_nothrow ready(CreateEventW(&inheritable, TRUE, FALSE, nullptr));
    Require(static_cast<bool>(ready), "create the client process's ready event");
    std::array<wchar_t, MAX_PATH> path{};
    Require(GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())) != 0u, "find the test executable");
    std::wstring commandLine = L"\"" + std::wstring(path.data()) + L"\" " + std::wstring(kSubscriberChildPrefix) +
                               std::to_wstring(reinterpret_cast<uintptr_t>(hwnd)) + L"," + std::to_wstring(reinterpret_cast<uintptr_t>(ready.get()));
    SIZE_T attributeBytes    = 0u;
    static_cast<void>(InitializeProcThreadAttributeList(nullptr, 1u, 0u, &attributeBytes));
    std::vector<std::byte> attributeStorage(attributeBytes);
    auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    Require(InitializeProcThreadAttributeList(attributes, 1u, 0u, &attributeBytes) != FALSE, "initialize the client process's attributes");
    const auto deleteAttributes = wil::scope_exit([&]() noexcept { DeleteProcThreadAttributeList(attributes); });
    HANDLE inherited            = ready.get();
    Require(UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &inherited, sizeof(inherited), nullptr, nullptr) != FALSE,
            "pass only the ready event to the client process");
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
            "start the client process");
    const auto waitPumping = [&](ULONGLONG timeoutMs, const auto& predicate)
    {
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            window.PumpMessages();
            static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
        }
        return predicate();
    };
    const bool subscribed    = waitPumping(20000u, [&] { return WaitForSingleObject(ready.get(), 0u) == WAIT_OBJECT_0; }) &&
                               waitPumping(5000u, [&] { return DebugGetAccessibilityEventListenerCountForTest(hwnd) != 0u; });
    const uint64_t listening = DebugGetAccessibilityEventListenerCountForTest(hwnd);
    Require(TerminateProcess(process.hProcess, 1u) != FALSE, "terminate the client process without unsubscribing");
    Require(WaitForSingleObject(process.hProcess, 10000u) == WAIT_OBJECT_0, "the client process ends");
    const ULONGLONG terminated = GetTickCount64();
    const bool released        = waitPumping(60000u, [&] { return DebugGetAccessibilityEventListenerCountForTest(hwnd) == 0u; });
    std::cerr << "    [advise] terminated client: subscribed=" << subscribed << " listeners=" << listening << " released=" << released << " after "
              << (GetTickCount64() - terminated) << " ms\n";
    Require(subscribed, "the client process's subscription is advised");
    Require(released, "the end of a terminated client's subscription is advised");
}

// What UI Automation advises an embedded view's root, which hangs below the application's window element.
void TestAccessibilityClientGateEmbeddedAdviseProbe()
{
    RequireSucceeded(DxUiFailureTests::RunRetainedAccessibilitySta(
                         []()
    {
        DxUi::TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "embedded advise probe's application STA refuses activation");
        GraphicsFixture graphics;
        RequireSucceeded(graphics.Create(), "create WARP device for the embedded advise probe");
        DxUi::EmbeddedHost view;
        std::shared_ptr<DxUi::GraphicsDevice> pool;
        RequireSucceeded(DxUi::GraphicsDevice::Create(graphics.device.get(), pool), "create the embedded advise probe's graphics pool");
        RequireSucceeded(view.Attach(std::move(pool)), "attach the embedded advise probe to borrowed graphics");
        auto root    = std::make_unique<DxUi::Panel>();
        auto* button = root->AddChild<DxUi::Button>(L"Embedded advise probe");
        button->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(320u, 200u, 96.0f), "prepare the embedded advise probe");
        {
            EmbeddedUiaTest::Bridge bridge(view, 320u, 200u);
            RequireSucceeded(bridge.Attach(true, 0x41445631ull), "attach the embedded advise probe's bridge");
            UiaTest::Client client(bridge.Hwnd(), {}, [&bridge] { bridge.Pump(); });
            for (const SubscriptionKind& kind : SubscriptionKinds())
            {
                DxUi::DebugResetAccessibilityAdviseRecordsForTest();
                const HRESULT added = client.Perform(kind.name, kind.add);
                client.Settle(600u);
                const auto afterAdd = AdviseRecordsOf(nullptr);
                const HRESULT removed =
                    client.Perform("remove every handler", [](UiaTest::Session& session) { return session.automation->RemoveAllEventHandlers(); });
                client.Settle(600u);
                const auto afterRemove = AdviseRecordsOf(nullptr);
                std::cerr << "    [embedded advise] " << kind.name << ": add hr=0x" << std::hex << static_cast<unsigned long>(added) << " remove hr=0x"
                          << static_cast<unsigned long>(removed) << std::dec << " added:" << DescribeAdvise(afterAdd, true)
                          << " | removed:" << DescribeAdvise(afterRemove, false) << '\n';
            }
        }
        view.Detach();
    }),
                     "run the embedded advise probe in a retained application STA");
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
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }),
            "the FX6 client's subscriptions are advised on the window's root");
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
    Require(DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) == 0u, "FX6 starts with no advised subscription");
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

// FX6 after a probe that queried the window and let it go: 100 mutations build no snapshot.
void TestAccessibilityClientGateFx6ReleasedProbeBuildsNothing()
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
    const DxUi::Detail::WorkCounterScope work;
    MutateFx6(controls, window);
    Require(work.Delta().accessibility.snapshotBuilds == 0u, "FX6 after a released probe: the work counters count no snapshot build");
    std::cerr << "    [FX6] released probe: builds +" << (Builds() - before) << '\n';
    Require(Builds() == before, "FX6 after a released probe: 100 mutations build no snapshot (G9)");
}

// A client that holds elements and only queries them, on its own thread, reads what the mutations left: the window
// thread publishes for its first query.
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
    const uint64_t before = Builds();
    MutateFx6(controls, window);
    Require(Builds() == before, "mutations under a query-only client build no snapshot");
    Require(client.StringProperty(field, UIA_ValueValuePropertyId) == Fx6::kFinalField, "a held element reads the field's current value");
    Require(RangeValue(client, slider) == Fx6::kFinalSlider, "a held element reads the slider's current value");
    std::cerr << "    [FX6] held elements: builds +" << (Builds() - before) << '\n';
    Require(Builds() == before + 1u, "the held elements' queries build one snapshot");
}

// A client that subscribes, hears, unsubscribes and subscribes again hears every mutation made while it listens; while it
// does not, mutations build nothing.
void TestAccessibilityClientGateResubscribedClientMissesNothing()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    Fx6::Controls controls;
    controls.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    static_cast<void>(client.Root());
    const HWND hwnd      = window.Hwnd();
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
        Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(hwnd) != 0u; }),
                "the subscription is advised on the window's root");
    };
    const auto unsubscribe = [&]
    {
        RequireSucceeded(client.Perform("unsubscribe", [](UiaTest::Session& session) { return session.automation->RemoveAllEventHandlers(); }), "unsubscribe");
        Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(hwnd) == 0u; }),
                "the subscription's end is advised on the window's root");
    };
    const auto hears = [&](std::wstring value)
    { return client.WaitForEvent([&](const UiaTest::HeardEvent& event) { return event.kind == UiaTest::EventKind::Property && event.value == value; }); };
    subscribe();
    MutateFx6(controls, window, 0u, 3u);
    Require(hears(L"Status 0") && hears(L"Value 1") && hears(std::to_wstring(2.0)), "the subscribed client hears each mutation");
    unsubscribe();
    // Events already raised may still be delivered, and UI Automation reads their senders' cached properties then.
    static_cast<void>(client.WaitOutTheStream());
    const uint64_t unobserved = Builds();
    MutateFx6(controls, window, 3u, 3u);
    client.Settle(300u);
    std::cerr << "    [FX6] unsubscribed: builds +" << (Builds() - unobserved) << " for 3 mutations\n";
    Require(Builds() == unobserved, "mutations made while nobody listens build nothing");
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
            Require(Builds() == before + 1u, "the embedded client's queries build one snapshot");
            const SystemListeners listening(true);
            const uint64_t listened = Builds();
            for (size_t index = 0u; index < 3u; ++index)
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
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }), "the subscription is advised");
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

// Queries on another thread while the window thread does not answer (it is busy) ask it to publish once per unheard
// revision: five queries wait one timeout in all, not one each, and read the last complete snapshot. A later revision asks
// again, and a window thread that answers publishes it.
void TestAccessibilityClientGateBusyWindowThreadCostsOneWaitPerRevision()
{
    using namespace DxUi;
    const std::optional<bool> outerListeners = DebugGetAccessibilitySystemListenersForTest();
    DebugSetAccessibilitySystemListenersForTest(false);
    const auto restoreListeners = wil::scope_exit([&]() noexcept { DebugSetAccessibilitySystemListenersForTest(outerListeners); });
    AttachedHostWindow window;
    auto field  = std::make_unique<TextField>(L"before");
    auto* typed = field.get();
    field->SetBounds(D2D1::RectF(0, 0, 240, 28));
    window.Host().SetRoot(std::move(field));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    root.attach(CreateWindowHostAccessibilityProvider(window.Hwnd()));
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    Require(root && SUCCEEDED(root.query_to(simple.put())), "the single field's window element");
    Require(ReadProviderStringProperty(*simple.get(), UIA_ValueValuePropertyId, "initial value") == L"before", "the window publishes the field");
    window.PumpMessages();
    typed->SetText(L"after"); // Nobody listens: the snapshot stays behind.
    const uint64_t requests = DebugGetAccessibilityQueryPublicationRequestCountForTest();
    std::array<std::wstring, 5u> values;
    ULONGLONG elapsed = 0u;
    std::thread worker([&]() noexcept
    {
        const ULONGLONG started = GetTickCount64();
        for (auto& value : values)
        {
            VARIANT read{};
            if (SUCCEEDED(simple->GetPropertyValue(UIA_ValueValuePropertyId, &read)) && read.vt == VT_BSTR && read.bstrVal)
                value.assign(read.bstrVal, SysStringLen(read.bstrVal));
            VariantClear(&read);
        }
        elapsed = GetTickCount64() - started;
    });
    worker.join(); // The window thread pumps nothing meanwhile: it is busy.
    std::cerr << "    [busy window] 5 queries took " << elapsed << " ms with " << (DebugGetAccessibilityQueryPublicationRequestCountForTest() - requests)
              << " publication request(s)\n";
    Require(DebugGetAccessibilityQueryPublicationRequestCountForTest() == requests + 1u, "the queries of one unheard revision send one publication request");
    Require(elapsed < 2500u, "five queries of a busy window wait one timeout in all");
    Require(std::ranges::all_of(values, [](const std::wstring& value) { return value == L"before"; }),
            "queries the busy window thread does not answer read the last complete snapshot");
    window.PumpMessages();
    typed->SetText(L"later");
    std::wstring later;
    std::atomic<bool> done{false};
    std::thread answered([&]() noexcept
    {
        VARIANT read{};
        if (SUCCEEDED(simple->GetPropertyValue(UIA_ValueValuePropertyId, &read)) && read.vt == VT_BSTR && read.bstrVal)
            later.assign(read.bstrVal, SysStringLen(read.bstrVal));
        VariantClear(&read);
        done.store(true);
    });
    while (! done.load())
    {
        window.PumpMessages();
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    answered.join();
    Require(later == L"later", "a later revision asks again, and a window thread that answers publishes it");
    Require(DebugGetAccessibilityQueryPublicationRequestCountForTest() == requests + 2u, "one more request for the later revision");
}

// A client UI Automation advised on the window hears a tree's selection events with no other listener on the desktop: every
// place that decides which events to compute uses the one client gate.
void TestAccessibilityClientGateAdvisedClientHearsSelectionEvents()
{
    const SystemListeners onlyThisClient(false);
    MutableTreeModel model;
    model.SetVisibleItems({DxUi::TreeItemData{.id = 1u, .text = L"First"}, DxUi::TreeItemData{.id = 2u, .text = L"Second"}});
    AttachedHostWindow window;
    auto control     = std::make_unique<DxUi::Tree>();
    DxUi::Tree* tree = control.get();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 96.0f));
    tree->SetModel(&model);
    tree->SetAccessibleName(L"Items");
    tree->SetSelectedItemId(2u);
    window.Host().SetRoot(std::move(control));
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), UiaTest::SelectionEventsSubscription(), [&window] { window.PumpMessages(); });
    static_cast<void>(client.Root());
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }),
            "the selection subscription is advised on the window's root");
    const auto heard    = UiaTest::HearSelectionEvents(client, 3u, [&] { tree->SetSelectedItemId(1u); });
    const auto expected = UiaTest::SortedEvents({L"Selected TreeItem 'First'", L"IsSelected:true TreeItem 'First'", L"IsSelected:false TreeItem 'Second'"});
    if (heard != expected)
        std::cerr << "    [selection] heard" << UiaTest::DescribeEventList(heard) << " expected" << UiaTest::DescribeEventList(expected) << '\n';
    Require(heard == expected, "an advised client alone hears the selection events of the application selecting an item");
}
// An MSAA or IAccessible2 client asks a window for its client object (WM_GETOBJECT with OBJID_CLIENT, what
// AccessibleObjectFromWindow sends) and then listens through WinEvents, which no advise and, on a desktop where another
// client already listens, no change of UiaClientsAreListening reveals. Such a window counts it as a listener: with nothing
// else listening, a later mutation publishes as before the client gate. A UI Automation client alone never sends that id
// (TestAccessibilityClientGateFx6ReleasedProbeBuildsNothing stays at 0).
void TestAccessibilityClientGateMsaaClientCountsAsListener()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* field = root->AddChild<DxUi::TextField>(L"before");
    field->SetBounds(D2D1::RectF(8.0f, 8.0f, 240.0f, 36.0f));
    field->SetAccessibleName(L"Entry");
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();
    uint64_t before = Builds();
    field->SetText(L"unheard");
    window.PumpMessages();
    Require(Builds() == before, "with no client a mutation builds nothing");
    wil::unique_hmodule oleacc(LoadLibraryExW(L"oleacc.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    using AccessibleObjectFromWindowFunction = HRESULT(WINAPI*)(HWND, DWORD, REFIID, void**);
    const auto accessibleObjectFromWindow =
        oleacc ? reinterpret_cast<AccessibleObjectFromWindowFunction>(reinterpret_cast<void*>(GetProcAddress(oleacc.get(), "AccessibleObjectFromWindow")))
               : nullptr;
    Require(accessibleObjectFromWindow != nullptr, "find AccessibleObjectFromWindow");
    {
        wil::com_ptr_nothrow<IUnknown> client;
        static_cast<void>(accessibleObjectFromWindow(window.Hwnd(), static_cast<DWORD>(OBJID_CLIENT), __uuidof(IUnknown), client.put_void()));
    }
    window.PumpMessages();
    before = Builds();
    field->SetText(L"heard by the MSAA client");
    window.PumpMessages();
    std::cerr << "    [msaa] after an MSAA client object request, the mutation built " << (Builds() - before) << " snapshot(s)\n";
    Require(Builds() > before, "a window an MSAA client asked for its client object publishes its mutations for it");
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

// Reads a provider's string value on a thread of its own while the window thread pumps, as a client in another process does.
[[nodiscard]] std::wstring ReadOnAnotherThread(AttachedHostWindow& window, IRawElementProviderSimple& provider, PROPERTYID property)
{
    std::wstring value;
    std::atomic<bool> done{false};
    std::thread reader([&]() noexcept
    {
        VARIANT read{};
        if (SUCCEEDED(provider.GetPropertyValue(property, &read)) && read.vt == VT_BSTR && read.bstrVal)
            value.assign(read.bstrVal, SysStringLen(read.bstrVal));
        VariantClear(&read);
        done.store(true);
    });
    while (! done.load())
    {
        window.PumpMessages();
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    reader.join();
    return value;
}

// A single-field window whose element stands for the field, published once by a window-thread query.
struct SingleFieldWindow final
{
    SingleFieldWindow()
    {
        auto control = std::make_unique<DxUi::TextField>(L"before");
        field        = control.get();
        field->SetBounds(D2D1::RectF(0, 0, 240, 28));
        window.Host().SetRoot(std::move(control));
        root.attach(DxUi::CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(root && SUCCEEDED(root.query_to(simple.put())), "the single field's window element");
        Require(ReadProviderStringProperty(*simple.get(), UIA_ValueValuePropertyId, "initial value") == L"before", "the window publishes the field");
        window.PumpMessages();
    }
    SingleFieldWindow(const SingleFieldWindow&)            = delete;
    SingleFieldWindow& operator=(const SingleFieldWindow&) = delete;
    SingleFieldWindow(SingleFieldWindow&&)                 = delete;
    SingleFieldWindow& operator=(SingleFieldWindow&&)      = delete;
    ~SingleFieldWindow()                                   = default;

    AttachedHostWindow window;
    DxUi::TextField* field = nullptr;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
};
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
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }), "the subscription is advised");
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
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }), "the subscription is advised");
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

// A publication request that fails (the window thread's flush cannot publish) is released: the next query asks again and
// reads the current state, with no further mutation.
void TestAccessibilityClientGateFailedPublicationRequestIsRetried()
{
    const SystemListeners nobody(false);
    SingleFieldWindow single;
    single.field->SetText(L"after");
    DxUi::DebugFailNextAccessibilityPublicationsForTest(1u);
    const uint64_t requests   = DxUi::DebugGetAccessibilityQueryPublicationRequestCountForTest();
    const std::wstring first  = ReadOnAnotherThread(single.window, *single.simple.get(), UIA_ValueValuePropertyId);
    const std::wstring second = ReadOnAnotherThread(single.window, *single.simple.get(), UIA_ValueValuePropertyId);
    std::cerr << "    [failed request] first read '" << (first == L"after" ? "after" : "before") << "', second '" << (second == L"after" ? "after" : "before")
              << "', requests " << (DxUi::DebugGetAccessibilityQueryPublicationRequestCountForTest() - requests) << '\n';
    Require(DxUi::DebugGetAccessibilityQueryPublicationRequestCountForTest() >= requests + 2u, "the failed request is released and asked again");
    Require(second == L"after", "a query after a failed publication reads the current state without another mutation");
}

// A window thread that stops turning long enough for the system to take the window for hung drops a sent request: it is
// posted instead, and once the thread turns again a query reads the current state without another mutation.
void TestAccessibilityClientGateRequestToAHungWindowIsPosted()
{
    const SystemListeners nobody(false);
    SingleFieldWindow single;
    single.field->SetText(L"after");
    Sleep(5600u); // The window thread does not turn: past the system's five seconds, the window counts as hung.
    std::wstring during;
    ULONGLONG waited = 0u;
    std::thread reader([&]() noexcept
    {
        const ULONGLONG started = GetTickCount64();
        VARIANT read{};
        if (SUCCEEDED(single.simple->GetPropertyValue(UIA_ValueValuePropertyId, &read)) && read.vt == VT_BSTR && read.bstrVal)
            during.assign(read.bstrVal, SysStringLen(read.bstrVal));
        VariantClear(&read);
        waited = GetTickCount64() - started;
    });
    reader.join();
    std::cerr << "    [hung window] the query waited " << waited << " ms (hung: " << (IsHungAppWindow(single.window.Hwnd()) != FALSE) << ")\n";
    Require(during == L"before", "the query of a window that does not turn reads the last complete snapshot");
    single.window.PumpMessages();
    Require(ReadOnAnotherThread(single.window, *single.simple.get(), UIA_ValueValuePropertyId) == L"after",
            "once the window thread turns, a query reads the current state without another mutation");
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

// A UI Automation client in another process that only queries the window (no subscription) does not make it an MSAA client:
// nothing keeps the window publishing for it.
void TestAccessibilityClientGateOutOfProcessQueryOnlyClientIsNoMsaaClient()
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
    Require(! DxUi::DebugGetAccessibilityMsaaClientForTest(hwnd), "a UI Automation client that only queries never asks for the MSAA client object");
    const uint64_t before = Builds();
    fields.first->SetText(L"after the query client");
    window.PumpMessages();
    Require(Builds() == before, "nothing keeps the window publishing for a client that only queried");
    static_cast<void>(TerminateProcess(process.hProcess, 0u));
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
// The same with the query on the window thread (a provider request made there, as the conductor's scenario has it): the
// window-thread publication of unheard changes made after the client subscribed is the baseline of the change back.
void TestAccessibilityClientGateSubscribedThenOwnerQueriedStateIsTheBaselineOfLaterEvents()
{
    const SystemListeners nobody(false);
    AttachedHostWindow window;
    TwoFields fields;
    fields.Build(window.Host());
    window.PumpMessages();
    UiaTest::Client client(window.Hwnd(), {}, [&window] { window.PumpMessages(); });
    static_cast<void>(client.Root());
    SubscribeToFieldChanges(client);
    Require(client.WaitUntil(3000u, [&] { return DxUi::DebugGetAccessibilityEventListenerCountForTest(window.Hwnd()) != 0u; }), "the subscription is advised");
    static_cast<void>(client.WaitOutTheStream());
    // Unheard only for the window: take the client away from the gate for the mutation, as a client that subscribed
    // a moment later would be. The advised count is what keeps the gate open, so the override alone cannot close it.
    const uint64_t buildsBefore = Builds();
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), false), "make the next mutations unheard");
    fields.first->SetText(L"y");
    window.Host().SetFocusControl(fields.second, false);
    Require(DxUi::DebugSetAccessibilityMutationInterestForTest(window.Hwnd(), std::nullopt), "restore the gate's own decision");
    window.PumpMessages();
    Require(Builds() == buildsBefore, "the unheard mutations published nothing");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    provider.attach(DxUi::CreateWindowHostAccessibilityProvider(window.Hwnd())); // The window-thread query: it publishes.
    Require(provider != nullptr && Builds() > buildsBefore, "the window-thread query publishes the unheard changes");
    window.PumpMessages();
    static_cast<void>(client.WaitOutTheStream());
    const size_t mark = client.Mark();
    fields.first->SetText(L"x");
    window.Host().SetFocusControl(fields.first, false);
    window.PumpMessages();
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_ValueValuePropertyId, L"x"), "the client hears the value change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Automation, UIA_Text_TextChangedEventId), "the client hears the text change back");
    Require(HeardFromFirst(client, mark, UiaTest::EventKind::Property, UIA_HasKeyboardFocusPropertyId, L"true"), "the client hears the focus return");
}
