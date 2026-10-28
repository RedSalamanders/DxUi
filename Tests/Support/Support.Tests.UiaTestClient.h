#pragma once

// An in-process UI Automation client for tests, shared by the control and the embedded suites. It runs on a thread of its own,
// as a screen reader is another process, and does what one does: it subscribes to events at a window's element and records
// each one with what UI Automation cached for its sender, and it walks the element tree with a tree walker over the content view
// (what a screen reader reads: UI Automation adds the window's own title bar to the children of every window, which no test of a
// control's elements asks about), so a test asserts what a client sees and not what a provider says about itself: that the parent
// of an element is the element it was reached from, that an event raised on an element reaches a client subscribed to the window
// it belongs to.
//
// The providers answer on the thread that owns them, so each wait of the owner pumps its messages (`pump`). The header needs no
// DxUi header and no helper of either suite: a failure of the client itself (it did not start, an answer did not come) ends the
// run with a message, as a failed check does, and what a test expects of the tree or of an event it asserts itself.

#include <Windows.h>

#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>
// Windows SDK WRL owns its interlocked/volatile implementation and target semantics.
// Scope is this system header only. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

#include <algorithm>
#include <array>
#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace UiaTest
{
// Focus subscriptions can inspect providers outside the target window. Apply UIA's bounded connection and transaction
// waits before any call that can reach those providers; COM activation itself necessarily happens before the settings exist.
[[nodiscard]] inline HRESULT CreateAutomationClient(IUIAutomation** result) noexcept
{
    if (! result)
        return E_POINTER;
    *result = nullptr;

    wil::com_ptr_nothrow<IUIAutomation2> automation;
    HRESULT hr = CoCreateInstance(CLSID_CUIAutomation8, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.put()));
    if (SUCCEEDED(hr))
        hr = automation->put_ConnectionTimeout(2000);
    if (SUCCEEDED(hr))
        hr = automation->put_TransactionTimeout(3000);
    if (SUCCEEDED(hr))
        hr = automation.query_to(result);
    return hr;
}

[[noreturn]] inline void Fail(const char* what, HRESULT hr = S_OK)
{
    std::cerr << "FAILED: " << what;
    if (FAILED(hr))
        std::cerr << " hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec;
    std::cerr << '\n' << std::flush;
    std::exit(1);
}

enum class EventKind
{
    Automation, // A pattern or window event: `id` is its EVENTID.
    Property,   // A property change: `id` is its PROPERTYID and `value` the new value.
    Structure,  // A change of structure: `id` is its StructureChangeType.
    Focus,      // The keyboard focus moved to the sender.
};
enum class TextRangeCommand
{
    AddToSelection,
    RemoveFromSelection,
    ScrollToTop,
    ScrollToBottom
};

// One event the client heard, with the name, control type and automation id UI Automation cached for its sender when it
// delivered the event, so the handler never calls back into a provider.
struct HeardEvent
{
    EventKind kind   = EventKind::Automation;
    long id          = 0;
    long controlType = 0;
    std::wstring name;
    std::wstring automationId;
    std::wstring value;

    [[nodiscard]] auto operator<=>(const HeardEvent&) const = default;
    [[nodiscard]] bool operator==(const HeardEvent&) const  = default;
};

// Numeric diagnostics are separate from HeardEvent: arrival timing never changes equality or deduplication.
// Entry precedes cached payload reads; completion records the payload insertion immediately before the handler returns.
inline constexpr size_t kEventTimelineCapacity = 1024u;
struct EventArrival
{
    uint64_t sequence   = 0u;
    size_t eventIndex   = 0u;
    ULONGLONG entered   = 0u;
    ULONGLONG completed = 0u;
};

struct EventSnapshot
{
    std::vector<HeardEvent> events;
    std::vector<EventArrival> arrivals;
    ULONGLONG cutoffTick     = 0u;
    uint64_t callbackEntries = 0u;
    size_t timelineOverflow  = 0u;
    size_t failedCallbacks   = 0u;
};

struct StreamQuiescence
{
    bool quiet            = false;
    size_t startCount     = 0u;
    size_t endCount       = 0u;
    ULONGLONG started     = 0u;
    ULONGLONG lastArrival = 0u;
    ULONGLONG completed   = 0u;
};

// Clock, count and pump are injectable so timeout and arrival boundaries can be checked without a window or sleeps.
// Observed silence is bounded evidence; it cannot promise that another notification will not arrive later.
template <typename Clock, typename Count, typename Pump>
[[nodiscard]] inline StreamQuiescence WaitForQuiescence(ULONGLONG quietMs, ULONGLONG deadlineMs, const Clock& clock, const Count& count, const Pump& pump)
{
    StreamQuiescence result;
    result.startCount  = count();
    result.endCount    = result.startCount;
    result.started     = clock();
    result.lastArrival = result.started;
    for (;;)
    {
        const size_t arrived = count();
        result.completed     = clock();
        if (arrived != result.endCount)
        {
            result.endCount    = arrived;
            result.lastArrival = result.completed;
        }
        // Equality is inclusive, but a slow pump cannot manufacture successful quiet after the deadline has passed.
        if (result.completed - result.started > deadlineMs)
            return result;
        if (result.completed - result.lastArrival >= quietMs)
        {
            result.quiet = true;
            return result;
        }
        if (result.completed - result.started >= deadlineMs)
            return result;
        pump();
    }
}

[[nodiscard]] inline std::vector<HeardEvent> DistinctEventsSince(std::vector<HeardEvent> events, size_t mark)
{
    events.erase(events.begin(), events.begin() + static_cast<ptrdiff_t>((std::min)(mark, events.size())));
    std::ranges::sort(events);
    events.erase(std::ranges::unique(events).begin(), events.end());
    return events;
}

[[nodiscard]] inline std::wstring ControlTypeName(long controlType)
{
    switch (controlType)
    {
        case UIA_ButtonControlTypeId: return L"Button";
        case UIA_DataGridControlTypeId: return L"DataGrid";
        case UIA_DataItemControlTypeId: return L"DataItem";
        case UIA_EditControlTypeId: return L"Edit";
        case UIA_HeaderItemControlTypeId: return L"HeaderItem";
        case UIA_PaneControlTypeId: return L"Pane";
        case UIA_RadioButtonControlTypeId: return L"RadioButton";
        case UIA_TabControlTypeId: return L"Tab";
        case UIA_TabItemControlTypeId: return L"TabItem";
        case UIA_TableControlTypeId: return L"Table";
        case UIA_TextControlTypeId: return L"Text";
        case UIA_TreeControlTypeId: return L"Tree";
        case UIA_TreeItemControlTypeId: return L"TreeItem";
        default: return L"#" + std::to_wstring(controlType);
    }
}

[[nodiscard]] inline std::wstring DescribeEvent(const HeardEvent& heard)
{
    const wchar_t* kind = heard.kind == EventKind::Automation  ? L"event"
                          : heard.kind == EventKind::Property  ? L"property"
                          : heard.kind == EventKind::Structure ? L"structure"
                                                               : L"focus";
    std::wstring text   = std::wstring(kind) + L" " + std::to_wstring(heard.id) + L" from " + ControlTypeName(heard.controlType) + L" '" + heard.name + L"'";
    if (! heard.value.empty())
        text += L" = " + heard.value;
    return text;
}

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class Recorder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                           IUIAutomationEventHandler,
                                                           IUIAutomationPropertyChangedEventHandler,
                                                           IUIAutomationStructureChangedEventHandler,
                                                           IUIAutomationFocusChangedEventHandler,
                                                           Microsoft::WRL::FtmBase>
{
public:
    // Called before the client thread starts. Ordinary clients allocate no timeline and read no callback clock.
    void EnableTimeline()
    {
        _timeline.reset(new (std::nothrow) Timeline{});
        if (! _timeline)
            Fail("allocate the bounded UI Automation arrival timeline");
        std::cerr << "    [UIA arrivals] capacity=" << kEventTimelineCapacity << " storageBytes=" << sizeof(Timeline)
                  << " callbackData=numeric-only clock=GetTickCount64\n";
    }

    [[nodiscard]] bool HasTimeline() const noexcept
    {
        return _timeline != nullptr;
    }

    HRESULT STDMETHODCALLTYPE HandleAutomationEvent(IUIAutomationElement* sender, EVENTID eventId) noexcept override
    {
        Add(EventKind::Automation, static_cast<long>(eventId), sender, nullptr);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandlePropertyChangedEvent(IUIAutomationElement* sender, PROPERTYID propertyId, VARIANT newValue) noexcept override
    {
        Add(EventKind::Property, static_cast<long>(propertyId), sender, &newValue);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandleStructureChangedEvent(IUIAutomationElement* sender, StructureChangeType change, SAFEARRAY*) noexcept override
    {
        Add(EventKind::Structure, static_cast<long>(change), sender, nullptr);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE HandleFocusChangedEvent(IUIAutomationElement* sender) noexcept override
    {
        Add(EventKind::Focus, 0, sender, nullptr);
        return S_OK;
    }

    [[nodiscard]] std::vector<HeardEvent> Events() const
    {
        const std::scoped_lock lock(_mutex);
        return _events;
    }

    [[nodiscard]] size_t EventCount() const
    {
        const std::scoped_lock lock(_mutex);
        return _events.size();
    }

    [[nodiscard]] EventSnapshot Capture() const
    {
        const std::scoped_lock lock(_mutex);
        EventSnapshot snapshot;
        if (_timeline)
        {
            snapshot.callbackEntries  = _timeline->sequence.load(std::memory_order_relaxed);
            snapshot.timelineOverflow = _timeline->overflow;
            snapshot.failedCallbacks  = _timeline->failed.load(std::memory_order_relaxed);
        }
        snapshot.cutoffTick = GetTickCount64();
        snapshot.events     = _events;
        if (_timeline)
            snapshot.arrivals.assign(_timeline->arrivals.begin(), _timeline->arrivals.begin() + static_cast<ptrdiff_t>(_timeline->stored));
        return snapshot;
    }

    [[nodiscard]] size_t Count(const std::function<bool(const HeardEvent&)>& matches) const
    {
        const std::scoped_lock lock(_mutex);
        return static_cast<size_t>(std::ranges::count_if(_events, matches));
    }

private:
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
    struct Timeline
    {
        std::array<EventArrival, kEventTimelineCapacity> arrivals{};
        std::atomic<uint64_t> sequence{0u};
        std::atomic<size_t> failed{0u};
        size_t stored   = 0u;
        size_t overflow = 0u;
    };
#pragma warning(pop)

    [[nodiscard]] static std::wstring Render(const VARIANT& variant)
    {
        switch (variant.vt)
        {
            case VT_BOOL: return variant.boolVal != VARIANT_FALSE ? L"true" : L"false";
            case VT_I4: return std::to_wstring(variant.lVal);
            case VT_R8: return std::to_wstring(variant.dblVal);
            case VT_BSTR: return variant.bstrVal ? std::wstring(variant.bstrVal, SysStringLen(variant.bstrVal)) : std::wstring{};
            default: return L"?";
        }
    }

    void Add(EventKind kind, long id, IUIAutomationElement* sender, const VARIANT* newValue) noexcept
    {
        const uint64_t sequence = _timeline ? _timeline->sequence.fetch_add(1u, std::memory_order_relaxed) + 1u : 0u;
        const ULONGLONG entered = _timeline ? GetTickCount64() : 0u;
        try
        {
            HeardEvent heard;
            heard.kind = kind;
            heard.id   = id;
            if (newValue)
                heard.value = Render(*newValue);
            if (sender)
            {
                CONTROLTYPEID controlType = 0;
                if (SUCCEEDED(sender->get_CachedControlType(&controlType)))
                    heard.controlType = static_cast<long>(controlType);
                wil::unique_bstr name;
                if (SUCCEEDED(sender->get_CachedName(name.put())) && name)
                    heard.name.assign(name.get(), SysStringLen(name.get()));
                wil::unique_bstr automationId;
                if (SUCCEEDED(sender->get_CachedAutomationId(automationId.put())) && automationId)
                    heard.automationId.assign(automationId.get(), SysStringLen(automationId.get()));
            }
            const std::scoped_lock lock(_mutex);
            const size_t index = _events.size();
            _events.push_back(std::move(heard));
            if (_timeline)
            {
                if (_timeline->stored < _timeline->arrivals.size())
                    _timeline->arrivals[_timeline->stored++] = EventArrival{sequence, index, entered, GetTickCount64()};
                else
                    ++_timeline->overflow;
            }
        }
        catch (const std::bad_alloc&)
        {
            if (_timeline)
                _timeline->failed.fetch_add(1u, std::memory_order_relaxed);
            // A dropped event fails the test's own wait for it; a client's callback never throws.
        }
    }

    mutable std::mutex _mutex;
    std::vector<HeardEvent> _events;
    std::unique_ptr<Timeline> _timeline;
};
#pragma warning(pop)

// What the client subscribes to, at the window's element and its whole subtree.
struct Subscription
{
    std::vector<EVENTID> automationEvents;
    std::vector<PROPERTYID> properties;
    bool structure = false;
    bool focus     = false; // Focus changes of the whole desktop, not only of the window's subtree.
};

enum class Direction
{
    Parent,
    FirstChild,
    LastChild,
    NextSibling,
    PreviousSibling,
};

// An element the client holds, by number; zero is none. The client thread keeps the elements, so the owner thread never
// holds a UI Automation interface.
using ElementId = size_t;

// What a client reads of an element.
struct ElementInfo
{
    std::wstring name;
    std::wstring automationId;
    long controlType      = 0;
    bool isControlElement = false;
    bool hasKeyboardFocus = false;
    RECT boundingRectangle{};
};
struct TextAttributeValue
{
    VARTYPE type  = VT_EMPTY;
    double number = 0.0;
    std::wstring text;
    bool mixed = false, unsupported = false;
};
struct TextRangeInfo
{
    std::wstring text;
    std::vector<TextAttributeValue> attributes;
    std::vector<ElementId> children;
    ElementId enclosing = 0u;
};

// The client thread's state. Only the client thread touches it.
struct Session
{
    wil::com_ptr_nothrow<IUIAutomation> automation;
    wil::com_ptr_nothrow<IUIAutomationTreeWalker> walker; // The content view.
    wil::com_ptr_nothrow<IUIAutomationCondition> content; // Matches the elements of the content view.
    std::vector<wil::com_ptr_nothrow<IUIAutomationElement>> elements;
    HWND target = nullptr;

    [[nodiscard]] ElementId Remember(IUIAutomationElement* element)
    {
        wil::com_ptr_nothrow<IUIAutomationElement> held;
        held = element;
        elements.push_back(std::move(held));
        return elements.size();
    }

    [[nodiscard]] IUIAutomationElement* Get(ElementId id) const noexcept
    {
        return id >= 1u && id <= elements.size() ? elements[id - 1u].get() : nullptr;
    }
};

class Client final
{
public:
    static constexpr ULONGLONG kSetupAllowanceMs       = 20000; // Cold client setup has exceeded 3 s on hosted runners.
    static constexpr ULONGLONG kNotificationDeadlineMs = 3000;
    static constexpr ULONGLONG kRequestAllowanceMs     = 20000;
    static constexpr ULONGLONG kSlowArrivalMs          = 1000; // An arrival this slow is logged: it shows a slow runner.

    // `pump` dispatches the pending messages of the thread that owns the providers.
    Client(HWND target, Subscription subscription, std::function<void()> pump, bool timeline = false)
        : _target(target),
          _subscription(std::move(subscription)),
          _pump(std::move(pump))
    {
        _recorder.attach(Microsoft::WRL::Make<Recorder>().Detach());
        if (! _recorder || ! _stop || ! _request)
            Fail("allocate the UI Automation client");
        if (timeline)
            _recorder->EnableTimeline();
        _thread = std::jthread([this] { Run(); });
        if (! WaitUntil(kSetupAllowanceMs, [this] { return _ready.load(); }))
            Fail("the UI Automation client starts");
        if (FAILED(_setup.load()))
            Fail("the UI Automation client subscribes", _setup.load());
    }

    Client(const Client&)            = delete;
    Client& operator=(const Client&) = delete;

    ~Client()
    {
        SetEvent(_stop.get());
        // The thread member joins after this without pumping, and the client's teardown may need the owner's providers to
        // answer: wait here, pumping, as long as its setup was allowed, and fail instead of hanging in the join.
        if (! WaitUntil(kSetupAllowanceMs, [this] { return _finished.load(); }))
            Fail("the UI Automation client ends");
    }

    template <typename Predicate> [[nodiscard]] bool WaitUntil(ULONGLONG timeoutMs, const Predicate& predicate) const
    {
        const ULONGLONG deadline = GetTickCount64() + timeoutMs;
        while (! predicate() && GetTickCount64() < deadline)
        {
            _pump();
            WaitForOwnerMessages();
        }
        return predicate();
    }

    // Lets events already raised reach the client before a test asserts that one never came.
    void Settle(ULONGLONG durationMs = 500u) const
    {
        static_cast<void>(WaitUntil(durationMs, [] { return false; }));
    }

    // Requires an observed quiet interval, or fails after the deadline. A later arrival can still cross a test's step mark.
    StreamQuiescence WaitOutTheStream(ULONGLONG quietMs = 500u, ULONGLONG deadlineMs = 10000u) const
    {
        const auto result = WaitForQuiescence(quietMs,
                                              deadlineMs,
                                              [] { return GetTickCount64(); },
                                              [this] { return _recorder->EventCount(); },
                                              [this]
        {
            _pump();
            WaitForOwnerMessages();
        });
        if (! result.quiet)
        {
            PrintQuiescence("timeout", result);
            PrintEvents();
            Fail("UI Automation events did not become quiet before the deadline");
        }
        return result;
    }

    // Where the events heard so far end, for DistinctSince.
    [[nodiscard]] size_t Mark() const
    {
        return _recorder->EventCount();
    }

    // The events heard after `mark`, each once and in a fixed order: UI Automation delivers to a client on threads of its own, so
    // two events raised one after the other may arrive in either order, and it may deliver one twice.
    [[nodiscard]] std::vector<HeardEvent> DistinctSince(size_t mark) const
    {
        return DistinctEventsSince(_recorder->Events(), mark);
    }

    [[nodiscard]] std::vector<HeardEvent> Events() const
    {
        return _recorder->Events();
    }

    [[nodiscard]] bool TimelineEnabled() const noexcept
    {
        return _recorder->HasTimeline();
    }

    [[nodiscard]] EventSnapshot CaptureEvents() const
    {
        return _recorder->Capture();
    }

    // The owner may supply a bounded provider-attempt dump. It must not pump messages or outlive its scope.
    void SetFailureDiagnostic(std::function<void()> diagnostic)
    {
        _failureDiagnostic = std::move(diagnostic);
    }

    [[nodiscard]] size_t DiagnosticStep() const noexcept
    {
        return _diagnosticStep;
    }

    void BeginSelectionStep(size_t mark, size_t expected, const StreamQuiescence& before)
    {
        ++_diagnosticStep;
        _diagnosticMark = mark;
        std::cerr << "    [UIA timeline] begin step=" << _diagnosticStep << " mark=" << mark << " expected=" << expected << '\n';
        PrintQuiescence("before", before);
    }

    void PrintSelectionStep(const EventSnapshot& snapshot,
                            size_t mark,
                            size_t expected,
                            bool expectedArrived,
                            ULONGLONG actionStarted,
                            ULONGLONG actionCompleted,
                            const StreamQuiescence& after)
    {
        std::cerr << "    [UIA timeline] step=" << _diagnosticStep << " mark=" << mark << " cutoff=" << snapshot.events.size()
                  << " cutoffTick=" << snapshot.cutoffTick << " expected=" << expected << " expectedArrived=" << expectedArrived << " action=" << actionStarted
                  << ".." << actionCompleted << '\n';
        PrintQuiescence("after", after);
        PrintArrivals(snapshot, _diagnosticPrintedCount);
        _diagnosticPrintedCount = snapshot.events.size();
        if (snapshot.timelineOverflow != 0u || snapshot.failedCallbacks != 0u)
        {
            if (_failureDiagnostic)
                _failureDiagnostic();
            Fail("the bounded UI Automation timeline lost diagnostics or an event");
        }
    }

    void PrintArrivalTail()
    {
        const auto snapshot = _recorder->Capture();
        std::cerr << "    [UIA timeline] tail step=" << _diagnosticStep << " mark=" << _diagnosticMark << " cutoff=" << snapshot.events.size()
                  << " cutoffTick=" << snapshot.cutoffTick << '\n';
        PrintArrivals(snapshot, _diagnosticPrintedCount);
        _diagnosticPrintedCount = snapshot.events.size();
        if (snapshot.timelineOverflow != 0u || snapshot.failedCallbacks != 0u)
        {
            if (_failureDiagnostic)
                _failureDiagnostic();
            Fail("the bounded UI Automation timeline lost diagnostics or an event");
        }
    }

    [[nodiscard]] size_t Count(const std::function<bool(const HeardEvent&)>& matches) const
    {
        return _recorder->Count(matches);
    }

    // Whether an event matching `matches` arrives in time. An arrival that took long is logged, which shows a slow runner.
    [[nodiscard]] bool WaitForEvent(const std::function<bool(const HeardEvent&)>& matches, ULONGLONG timeoutMs = kNotificationDeadlineMs) const
    {
        const ULONGLONG started = GetTickCount64();
        const bool heard        = WaitUntil(timeoutMs, [&] { return _recorder->Count(matches) != 0u; });
        const ULONGLONG waited  = GetTickCount64() - started;
        if (heard && waited >= kSlowArrivalMs)
            std::cerr << "    [UIA] an event took " << waited << " ms to arrive\n";
        return heard;
    }

    // Capture and print before a failure's tree query pumps any more messages.
    void PrintEvents() const
    {
        const auto snapshot = _recorder->Capture();
        if (TimelineEnabled())
            std::cerr << "    [UIA timeline] failure step=" << _diagnosticStep << " mark=" << _diagnosticMark << " cutoff=" << snapshot.events.size()
                      << " cutoffTick=" << snapshot.cutoffTick << '\n';
        std::cerr << "    [UIA] frozen event cutoff=" << snapshot.events.size() << " tick=" << snapshot.cutoffTick << ":";
        for (const HeardEvent& heard : snapshot.events)
        {
            std::cerr << " [";
            for (const wchar_t unit : DescribeEvent(heard))
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            std::cerr << "]";
        }
        std::cerr << '\n';
        if (TimelineEnabled())
            PrintArrivals(snapshot, 0u);
        if (_failureDiagnostic)
            _failureDiagnostic();
    }

    // The window's element, as UI Automation reaches it from the window handle.
    [[nodiscard]] ElementId Root()
    {
        return Ask<ElementId>("the window's element",
                              [](Session& session, ElementId& id)
        {
            wil::com_ptr_nothrow<IUIAutomationElement> element;
            const HRESULT hr = session.automation->ElementFromHandle(session.target, element.put());
            if (SUCCEEDED(hr) && element)
                id = session.Remember(element.get());
            return hr;
        });
    }

    // The element in `direction` of `from`, which UI Automation's tree walker finds through the providers' own navigation.
    [[nodiscard]] std::optional<ElementId> Navigate(ElementId from, Direction direction)
    {
        const ElementId found = Ask<ElementId>("navigation",
                                               [from, direction](Session& session, ElementId& id)
        {
            IUIAutomationElement* const origin = session.Get(from);
            if (! origin)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationElement> element;
            HRESULT hr = E_INVALIDARG;
            switch (direction)
            {
                case Direction::Parent: hr = session.walker->GetParentElement(origin, element.put()); break;
                case Direction::FirstChild: hr = session.walker->GetFirstChildElement(origin, element.put()); break;
                case Direction::LastChild: hr = session.walker->GetLastChildElement(origin, element.put()); break;
                case Direction::NextSibling: hr = session.walker->GetNextSiblingElement(origin, element.put()); break;
                case Direction::PreviousSibling: hr = session.walker->GetPreviousSiblingElement(origin, element.put()); break;
            }
            if (SUCCEEDED(hr) && element)
                id = session.Remember(element.get());
            return hr;
        });
        return found != 0u ? std::optional<ElementId>(found) : std::nullopt;
    }

    // The children UI Automation enumerates for `of`, which is the same walk by another route (a search of its children).
    [[nodiscard]] std::vector<ElementId> Children(ElementId of)
    {
        return Ask<std::vector<ElementId>>("children",
                                           [of](Session& session, std::vector<ElementId>& ids)
        {
            IUIAutomationElement* const origin = session.Get(of);
            if (! origin)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationElementArray> array;
            const HRESULT hr = origin->FindAll(TreeScope_Children, session.content.get(), array.put());
            int length       = 0;
            if (SUCCEEDED(hr) && array && SUCCEEDED(array->get_Length(&length)))
            {
                for (int index = 0; index < length; ++index)
                {
                    wil::com_ptr_nothrow<IUIAutomationElement> element;
                    if (SUCCEEDED(array->GetElement(index, element.put())) && element)
                        ids.push_back(session.Remember(element.get()));
                }
            }
            return hr;
        });
    }

    [[nodiscard]] ElementInfo Describe(ElementId id)
    {
        return Ask<ElementInfo>("an element's properties",
                                [id](Session& session, ElementInfo& info)
        {
            IUIAutomationElement* const element = session.Get(id);
            if (! element)
                return E_INVALIDARG;
            return ReadInfo(*element, info);
        });
    }

    [[nodiscard]] std::wstring StringProperty(ElementId id, PROPERTYID property)
    {
        return Ask<std::wstring>("a string property",
                                 [id, property](Session& session, std::wstring& result)
        {
            IUIAutomationElement* const element = session.Get(id);
            if (! element)
                return E_INVALIDARG;
            VARIANT value{};
            const auto clear = wil::scope_exit([&] noexcept { VariantClear(&value); });
            RETURN_IF_FAILED(element->GetCurrentPropertyValue(property, &value));
            if (value.vt != VT_BSTR)
                return E_UNEXPECTED;
            if (value.bstrVal)
                result.assign(value.bstrVal, SysStringLen(value.bstrVal));
            else
                result.clear();
            return S_OK;
        });
    }

    // What UI Automation answers when asked for an element's properties: a failure, UIA_E_ELEMENTNOTAVAILABLE, for an element
    // whose control is gone. `info` is filled when it succeeds.
    [[nodiscard]] HRESULT TryDescribe(ElementId id, ElementInfo& info)
    {
        return TryAsk("an element's properties",
                      info,
                      [id](Session& session, ElementInfo& result)
        {
            IUIAutomationElement* const element = session.Get(id);
            if (! element)
                return E_INVALIDARG;
            return ReadInfo(*element, result);
        });
    }

    // Whether UI Automation takes the two for the same element (it compares their runtime ids).
    [[nodiscard]] bool Same(ElementId first, ElementId second)
    {
        return Ask<bool>("an element comparison",
                         [first, second](Session& session, bool& same)
        {
            IUIAutomationElement* const a = session.Get(first);
            IUIAutomationElement* const b = session.Get(second);
            if (! a || ! b)
                return E_INVALIDARG;
            BOOL equal       = FALSE;
            const HRESULT hr = session.automation->CompareElements(a, b, &equal);
            same             = equal != FALSE;
            return hr;
        });
    }

    // The container a SelectionItem pattern names for `item`.
    [[nodiscard]] std::optional<ElementId> SelectionContainer(ElementId item)
    {
        const ElementId found = Ask<ElementId>("a selection container",
                                               [item](Session& session, ElementId& id)
        {
            IUIAutomationElement* const element = session.Get(item);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationSelectionItemPattern> pattern;
            HRESULT hr = element->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(pattern.put()));
            wil::com_ptr_nothrow<IUIAutomationElement> container;
            if (SUCCEEDED(hr) && pattern)
                hr = pattern->get_CurrentSelectionContainer(container.put());
            if (SUCCEEDED(hr) && container)
                id = session.Remember(container.get());
            return hr;
        });
        return found != 0u ? std::optional<ElementId>(found) : std::nullopt;
    }

    // The grid a GridItem pattern names for `cell`.
    [[nodiscard]] std::optional<ElementId> ContainingGrid(ElementId cell)
    {
        const ElementId found = Ask<ElementId>("a containing grid",
                                               [cell](Session& session, ElementId& id)
        {
            IUIAutomationElement* const element = session.Get(cell);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationGridItemPattern> pattern;
            HRESULT hr = element->GetCurrentPatternAs(UIA_GridItemPatternId, IID_PPV_ARGS(pattern.put()));
            wil::com_ptr_nothrow<IUIAutomationElement> grid;
            if (SUCCEEDED(hr) && pattern)
                hr = pattern->get_CurrentContainingGrid(grid.put());
            if (SUCCEEDED(hr) && grid)
                id = session.Remember(grid.get());
            return hr;
        });
        return found != 0u ? std::optional<ElementId>(found) : std::nullopt;
    }

    // The element a Text pattern names as the one enclosing the whole text of `field` (the enclosing element of its document range).
    [[nodiscard]] std::optional<ElementId> TextEnclosingElement(ElementId field)
    {
        const ElementId found = Ask<ElementId>("a text range's enclosing element",
                                               [field](Session& session, ElementId& id)
        {
            IUIAutomationElement* const element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            HRESULT hr = element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put()));
            wil::com_ptr_nothrow<IUIAutomationTextRange> range;
            if (SUCCEEDED(hr) && pattern)
                hr = pattern->get_DocumentRange(range.put());
            wil::com_ptr_nothrow<IUIAutomationElement> enclosing;
            if (SUCCEEDED(hr) && range)
                hr = range->GetEnclosingElement(enclosing.put());
            if (SUCCEEDED(hr) && enclosing)
                id = session.Remember(enclosing.get());
            return hr;
        });
        return found != 0u ? std::optional<ElementId>(found) : std::nullopt;
    }

    // Exact document text as a client reads it, across the actual UIA transport.
    [[nodiscard]] std::wstring DocumentText(ElementId field)
    {
        return Ask<std::wstring>("a document's text",
                                 [field](Session& session, std::wstring& result)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> range;
            RETURN_IF_FAILED(pattern->get_DocumentRange(range.put()));
            wil::unique_bstr text;
            RETURN_IF_FAILED(range->GetText(-1, text.put()));
            result.assign(text.get(), SysStringLen(text.get()));
            return S_OK;
        });
    }

    [[nodiscard]] TextRangeInfo InspectTextRange(ElementId field, std::wstring needle, std::vector<TEXTATTRIBUTEID> attributes = {}, bool format = false)
    {
        return Ask<TextRangeInfo>("semantic text attributes and range children",
                                  [field, needle = std::move(needle), attributes = std::move(attributes), format](Session& session, TextRangeInfo& result)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> document, range;
            RETURN_IF_FAILED(pattern->get_DocumentRange(document.put()));
            if (needle.empty())
                range = document;
            else
            {
                wil::unique_bstr value(SysAllocStringLen(needle.data(), static_cast<UINT>(needle.size())));
                RETURN_IF_FAILED(document->FindText(value.get(), FALSE, FALSE, range.put()));
                if (! range)
                    return E_INVALIDARG;
            }
            if (format)
                RETURN_IF_FAILED(range->ExpandToEnclosingUnit(TextUnit_Format));
            wil::unique_bstr text;
            RETURN_IF_FAILED(range->GetText(-1, text.put()));
            result.text.assign(text.get(), SysStringLen(text.get()));
            for (const auto attribute : attributes)
            {
                wil::unique_variant value;
                RETURN_IF_FAILED(range->GetAttributeValue(attribute, &value));
                TextAttributeValue actual;
                actual.type = value.vt;
                if (value.vt == VT_I4)
                    actual.number = value.lVal;
                else if (value.vt == VT_R8)
                    actual.number = value.dblVal;
                else if (value.vt == VT_BOOL)
                    actual.number = value.boolVal != VARIANT_FALSE ? 1.0 : 0.0;
                else if (value.vt == VT_BSTR)
                    actual.text.assign(value.bstrVal, SysStringLen(value.bstrVal));
                else if (value.vt == VT_UNKNOWN)
                {
                    BOOL reserved = FALSE;
                    RETURN_IF_FAILED(session.automation->CheckNotSupported(value, &reserved));
                    actual.unsupported = reserved != FALSE;
                    wil::com_ptr_nothrow<IUnknown> mixed;
                    RETURN_IF_FAILED(session.automation->get_ReservedMixedAttributeValue(mixed.put()));
                    wil::com_ptr_nothrow<IUnknown> identity;
                    if (value.punkVal)
                        RETURN_IF_FAILED(value.punkVal->QueryInterface(IID_PPV_ARGS(identity.put())));
                    actual.mixed = identity.get() == mixed.get();
                }
                result.attributes.push_back(std::move(actual));
            }
            wil::com_ptr_nothrow<IUIAutomationElement> enclosing;
            RETURN_IF_FAILED(range->GetEnclosingElement(enclosing.put()));
            if (enclosing)
                result.enclosing = session.Remember(enclosing.get());
            wil::com_ptr_nothrow<IUIAutomationElementArray> children;
            RETURN_IF_FAILED(range->GetChildren(children.put()));
            int count = 0;
            RETURN_IF_FAILED(children->get_Length(&count));
            for (int index = 0; index < count; ++index)
            {
                wil::com_ptr_nothrow<IUIAutomationElement> child;
                RETURN_IF_FAILED(children->GetElement(index, child.put()));
                result.children.push_back(session.Remember(child.get()));
            }
            return S_OK;
        });
    }
    [[nodiscard]] std::wstring TextRangeFromChild(ElementId field, ElementId child)
    {
        return Ask<std::wstring>("a semantic child's exact source range",
                                 [field, child](Session& session, std::wstring& result)
        {
            auto* element = session.Get(field);
            auto* item    = session.Get(child);
            if (! element || ! item)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> range;
            RETURN_IF_FAILED(pattern->RangeFromChild(item, range.put()));
            wil::unique_bstr text;
            RETURN_IF_FAILED(range->GetText(-1, text.put()));
            result.assign(text.get(), SysStringLen(text.get()));
            return S_OK;
        });
    }
    [[nodiscard]] HRESULT Toggle(ElementId child)
    {
        return Ask<HRESULT>("a semantic task action",
                            [child](Session& session, HRESULT& result)
        {
            auto* element = session.Get(child);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTogglePattern> pattern;
            const auto hr = element->GetCurrentPatternAs(UIA_TogglePatternId, IID_PPV_ARGS(pattern.put()));
            result        = SUCCEEDED(hr) ? pattern->Toggle() : hr;
            return S_OK;
        });
    }
    [[nodiscard]] HRESULT ExecuteTextRangeCommand(ElementId field, std::wstring needle, TextRangeCommand command)
    {
        return Ask<HRESULT>("a text range command",
                            [field, needle = std::move(needle), command](Session& session, HRESULT& result)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> document, range;
            RETURN_IF_FAILED(pattern->get_DocumentRange(document.put()));
            wil::unique_bstr value(SysAllocStringLen(needle.data(), static_cast<UINT>(needle.size())));
            RETURN_IF_FAILED(document->FindText(value.get(), FALSE, FALSE, range.put()));
            if (! range)
                return E_INVALIDARG;
            switch (command)
            {
                case TextRangeCommand::AddToSelection: result = range->AddToSelection(); break;
                case TextRangeCommand::RemoveFromSelection: result = range->RemoveFromSelection(); break;
                case TextRangeCommand::ScrollToTop: result = range->ScrollIntoView(TRUE); break;
                case TextRangeCommand::ScrollToBottom: result = range->ScrollIntoView(FALSE); break;
            }
            return S_OK; // Preserve the command HRESULT for assertions by the caller.
        });
    }

    [[nodiscard]] std::wstring TextSelectionText(ElementId field)
    {
        return Ask<std::wstring>("selected source text",
                                 [field](Session& session, std::wstring& selected)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRangeArray> ranges;
            RETURN_IF_FAILED(pattern->GetSelection(ranges.put()));
            int length = 0;
            RETURN_IF_FAILED(ranges->get_Length(&length));
            if (! length)
                return S_OK;
            wil::com_ptr_nothrow<IUIAutomationTextRange> range;
            RETURN_IF_FAILED(ranges->GetElement(0, range.put()));
            wil::unique_bstr text;
            RETURN_IF_FAILED(range->GetText(-1, text.put()));
            selected.assign(text.get(), SysStringLen(text.get()));
            return S_OK;
        });
    }

    // Caret source offset through TextPattern2 and real cross-range endpoint
    // movement. Counting the returned UTF-16 prefix matches the editor ACP API.
    [[nodiscard]] size_t TextCaretPosition(ElementId field)
    {
        return Ask<size_t>("a document's caret position",
                           [field](Session& session, size_t& position)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern2> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPattern2Id, IID_PPV_ARGS(pattern.put())));
            BOOL active = FALSE;
            wil::com_ptr_nothrow<IUIAutomationTextRange> caret, document;
            RETURN_IF_FAILED(pattern->GetCaretRange(&active, caret.put()));
            RETURN_IF_FAILED(pattern->get_DocumentRange(document.put()));
            RETURN_IF_FAILED(document->MoveEndpointByRange(TextPatternRangeEndpoint_End, caret.get(), TextPatternRangeEndpoint_Start));
            wil::unique_bstr prefix;
            RETURN_IF_FAILED(document->GetText(-1, prefix.put()));
            position = SysStringLen(prefix.get());
            return S_OK;
        });
    }

    // Ask through the real text client, then count its exact UTF-16 source
    // prefix. Character-unit movement would collapse CRLF and supplementary ACPs.
    [[nodiscard]] size_t TextPositionFromPoint(ElementId field, POINT point)
    {
        return Ask<size_t>("a source text position at a screen point",
                           [field, point](Session& session, size_t& position)
        {
            auto* element = session.Get(field);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationTextPattern> pattern;
            RETURN_IF_FAILED(element->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(pattern.put())));
            wil::com_ptr_nothrow<IUIAutomationTextRange> hit, document;
            RETURN_IF_FAILED(pattern->RangeFromPoint(point, hit.put()));
            RETURN_IF_FAILED(pattern->get_DocumentRange(document.put()));
            RETURN_IF_FAILED(document->MoveEndpointByRange(TextPatternRangeEndpoint_End, hit.get(), TextPatternRangeEndpoint_Start));
            wil::unique_bstr prefix;
            RETURN_IF_FAILED(document->GetText(-1, prefix.put()));
            position = SysStringLen(prefix.get());
            return S_OK;
        });
    }

    // What a Selection pattern reports as selected in `container`.
    [[nodiscard]] std::vector<ElementId> Selection(ElementId container)
    {
        return Ask<std::vector<ElementId>>("a selection",
                                           [container](Session& session, std::vector<ElementId>& ids)
        {
            IUIAutomationElement* const element = session.Get(container);
            if (! element)
                return E_INVALIDARG;
            wil::com_ptr_nothrow<IUIAutomationSelectionPattern> pattern;
            HRESULT hr = element->GetCurrentPatternAs(UIA_SelectionPatternId, IID_PPV_ARGS(pattern.put()));
            wil::com_ptr_nothrow<IUIAutomationElementArray> array;
            int length = 0;
            if (SUCCEEDED(hr) && pattern)
                hr = pattern->GetCurrentSelection(array.put());
            if (SUCCEEDED(hr) && array && SUCCEEDED(array->get_Length(&length)))
            {
                for (int index = 0; index < length; ++index)
                {
                    wil::com_ptr_nothrow<IUIAutomationElement> selected;
                    if (SUCCEEDED(array->GetElement(index, selected.put())) && selected)
                        ids.push_back(session.Remember(selected.get()));
                }
            }
            return hr;
        });
    }

    // The tree below `from` as the client walks it, a line per element, for a failure report.
    [[nodiscard]] std::wstring Dump(ElementId from, int maximumDepth = 3)
    {
        return Ask<std::wstring>("a dump of the tree",
                                 [from, maximumDepth](Session& session, std::wstring& text)
        {
            IUIAutomationElement* const origin = session.Get(from);
            if (! origin)
                return E_INVALIDARG;
            size_t lines = 0u;
            DumpElement(session, *origin, 0, maximumDepth, lines, text);
            return S_OK;
        });
    }

private:
    // UIA can make a chain of COM calls into this STA while constructing an event's cached sender. A Sleep between
    // dispatches can delay each chained call until another scheduler tick. Wake for queued/sent messages so that the owner services
    // the next call promptly; the short timeout still observes predicates that change on the client's MTA.
    static void WaitForOwnerMessages()
    {
        if (MsgWaitForMultipleObjectsEx(0u, nullptr, 5u, QS_ALLINPUT, MWMO_INPUTAVAILABLE) == WAIT_FAILED)
            Fail("wait for the UI Automation owner's messages", HRESULT_FROM_WIN32(GetLastError()));
    }

    static void PrintQuiescence(const char* phase, const StreamQuiescence& result)
    {
        std::cerr << "    [UIA stream] phase=" << phase << " quiet=" << result.quiet << " ticks=" << result.started << ".." << result.completed
                  << " lastArrival=" << result.lastArrival << " counts=" << result.startCount << ".." << result.endCount << '\n';
    }

    static void PrintArrivals(const EventSnapshot& snapshot, size_t first)
    {
        std::cerr << "    [UIA arrivals] entries=" << snapshot.callbackEntries << " completed=" << snapshot.events.size()
                  << " capacity=" << kEventTimelineCapacity << " overflow=" << snapshot.timelineOverflow << " failed=" << snapshot.failedCallbacks << '\n';
        for (const auto& arrival : snapshot.arrivals)
        {
            if (arrival.eventIndex < first || arrival.eventIndex >= snapshot.events.size())
                continue;
            std::cerr << "    [UIA arrival] seq=" << arrival.sequence << " index=" << arrival.eventIndex << " entered=" << arrival.entered
                      << " completed=" << arrival.completed << " [";
            for (const wchar_t unit : DescribeEvent(snapshot.events[arrival.eventIndex]))
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            std::cerr << "]\n";
        }
    }

    // A request to the client thread: `request` fills `result` and returns what UI Automation answered, which is the answer a
    // test that expects a failure reads. The owner thread waits for the answer pumping, as the answer may need its providers;
    // an answer that does not come ends the run.
    template <typename Result, typename Request> [[nodiscard]] HRESULT TryAsk(const char* what, Result& result, Request request)
    {
        HRESULT answered = E_PENDING;
        _task            = [&](Session& session) { answered = request(session, result); };
        _taskDone.store(false);
        SetEvent(_request.get());
        const bool done = WaitUntil(kRequestAllowanceMs, [this] { return _taskDone.load(); });
        _task           = nullptr;
        if (! done)
            Fail(what);
        return answered;
    }

    // The same, for a request that is expected to succeed: a failure ends the run.
    template <typename Result, typename Request> [[nodiscard]] Result Ask(const char* what, Request request)
    {
        Result result{};
        const HRESULT answered = TryAsk(what, result, std::move(request));
        if (FAILED(answered))
            Fail(what, answered);
        return result;
    }

    static HRESULT ReadInfo(IUIAutomationElement& element, ElementInfo& info)
    {
        wil::unique_bstr name;
        HRESULT hr = element.get_CurrentName(name.put());
        if (SUCCEEDED(hr) && name)
            info.name.assign(name.get(), SysStringLen(name.get()));
        wil::unique_bstr automationId;
        if (SUCCEEDED(hr))
            hr = element.get_CurrentAutomationId(automationId.put());
        if (SUCCEEDED(hr) && automationId)
            info.automationId.assign(automationId.get(), SysStringLen(automationId.get()));
        CONTROLTYPEID controlType = 0;
        if (SUCCEEDED(hr))
            hr = element.get_CurrentControlType(&controlType);
        info.controlType = static_cast<long>(controlType);
        BOOL flag        = FALSE;
        if (SUCCEEDED(hr))
            hr = element.get_CurrentIsControlElement(&flag);
        info.isControlElement = flag != FALSE;
        if (SUCCEEDED(hr))
            hr = element.get_CurrentHasKeyboardFocus(&flag);
        info.hasKeyboardFocus = flag != FALSE;
        if (SUCCEEDED(hr))
            hr = element.get_CurrentBoundingRectangle(&info.boundingRectangle);
        return hr;
    }

    static void DumpElement(Session& session, IUIAutomationElement& element, int depth, int maximumDepth, size_t& lines, std::wstring& text)
    {
        constexpr size_t kMaximumLines = 60u;
        if (lines++ >= kMaximumLines)
            return;
        ElementInfo info;
        const HRESULT hr = ReadInfo(element, info);
        text += std::wstring(static_cast<size_t>(depth) * 2u, L' ');
        text += SUCCEEDED(hr) ? std::wstring(ControlTypeName(info.controlType)) + L" '" + info.name + L"'" : L"(unreadable)";
        text += L'\n';
        if (depth >= maximumDepth)
            return;
        wil::com_ptr_nothrow<IUIAutomationElement> child;
        if (FAILED(session.walker->GetFirstChildElement(&element, child.put())))
            return;
        while (child)
        {
            DumpElement(session, *child.get(), depth + 1, maximumDepth, lines, text);
            wil::com_ptr_nothrow<IUIAutomationElement> next;
            if (FAILED(session.walker->GetNextSiblingElement(child.get(), next.put())))
                return;
            child = std::move(next);
        }
    }

    void Run() noexcept
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        RunSession(initialized);
        if (SUCCEEDED(initialized))
            CoUninitialize();
        _finished.store(true);
    }

    // Everything that holds a UI Automation interface lives and ends in here, before COM is uninitialized.
    void RunSession(HRESULT initialized) noexcept
    {
        Session session;
        session.target = _target;
        wil::com_ptr_nothrow<IUIAutomationCacheRequest> cache;
        wil::com_ptr_nothrow<IUIAutomationElement> root;
        HRESULT hr = initialized;
        if (SUCCEEDED(hr))
            hr = CreateAutomationClient(session.automation.put());
        if (SUCCEEDED(hr))
            hr = session.automation->get_ContentViewWalker(session.walker.put());
        VARIANT isContent{};
        isContent.vt      = VT_BOOL;
        isContent.boolVal = VARIANT_TRUE;
        if (SUCCEEDED(hr))
            hr = session.automation->CreatePropertyCondition(UIA_IsContentElementPropertyId, isContent, session.content.put());
        if (SUCCEEDED(hr))
            hr = session.automation->CreateCacheRequest(cache.put());
        // What a handler reads of its sender comes with the event, so it never calls back into the provider's thread.
        for (const PROPERTYID property : {UIA_NamePropertyId, UIA_ControlTypePropertyId, UIA_AutomationIdPropertyId})
        {
            if (SUCCEEDED(hr))
                hr = cache->AddProperty(property);
        }
        if (SUCCEEDED(hr))
            hr = session.automation->ElementFromHandle(_target, root.put());
        for (const EVENTID eventId : _subscription.automationEvents)
        {
            if (SUCCEEDED(hr))
                hr = session.automation->AddAutomationEventHandler(eventId, root.get(), TreeScope_Subtree, cache.get(), _recorder.get());
        }
        if (SUCCEEDED(hr) && ! _subscription.properties.empty())
        {
            hr = session.automation->AddPropertyChangedEventHandlerNativeArray(root.get(),
                                                                               TreeScope_Subtree,
                                                                               cache.get(),
                                                                               _recorder.get(),
                                                                               _subscription.properties.data(),
                                                                               static_cast<int>(_subscription.properties.size()));
        }
        if (SUCCEEDED(hr) && _subscription.structure)
            hr = session.automation->AddStructureChangedEventHandler(root.get(), TreeScope_Subtree, cache.get(), _recorder.get());
        if (SUCCEEDED(hr) && _subscription.focus)
            hr = session.automation->AddFocusChangedEventHandler(cache.get(), _recorder.get());
        _setup.store(hr);
        _ready.store(true);
        if (SUCCEEDED(hr))
        {
            const std::array<HANDLE, 2> events{_stop.get(), _request.get()};
            while (WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, INFINITE) == WAIT_OBJECT_0 + 1)
            {
                try
                {
                    _task(session);
                }
                catch (const std::bad_alloc&)
                {
                    // The request answers as it left its answer: E_PENDING, which the asking thread reports.
                }
                _taskDone.store(true);
            }
            static_cast<void>(session.automation->RemoveAllEventHandlers());
        }
    }

    HWND _target = nullptr;
    Subscription _subscription;
    std::function<void()> _pump;
    std::function<void()> _failureDiagnostic;
    size_t _diagnosticStep         = 0u;
    size_t _diagnosticMark         = 0u;
    size_t _diagnosticPrintedCount = 0u;
    wil::com_ptr_nothrow<Recorder> _recorder;
    wil::unique_event _stop{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    wil::unique_event _request{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    std::function<void(Session&)> _task;
    std::atomic<bool> _taskDone{false};
    std::atomic<HRESULT> _setup{E_PENDING};
    std::atomic<bool> _ready{false};
    std::atomic<bool> _finished{false};
    std::jthread _thread; // Last: it joins before the state it uses is destroyed.
};

// What a client subscribes to for the selection events of a tree or grid: the four events of the Selection and SelectionItem
// patterns and the SelectionItem IsSelected property.
[[nodiscard]] inline Subscription SelectionEventsSubscription()
{
    Subscription subscription;
    subscription.automationEvents = {UIA_SelectionItem_ElementSelectedEventId,
                                     UIA_SelectionItem_ElementAddedToSelectionEventId,
                                     UIA_SelectionItem_ElementRemovedFromSelectionEventId,
                                     UIA_Selection_InvalidatedEventId};
    subscription.properties       = {UIA_SelectionItemIsSelectedPropertyId};
    return subscription;
}

// A selection event as a test names it: what it says ("Selected", "Added", "Removed", "Invalidated", or "IsSelected:true" and
// "IsSelected:false" for the property change), then the control type and name of the element it came from, as in
// "Selected TreeItem 'Volets'". Empty for any other event.
[[nodiscard]] inline std::wstring DescribeSelectionEvent(const HeardEvent& heard)
{
    std::wstring what;
    if (heard.kind == EventKind::Automation)
    {
        switch (heard.id)
        {
            case UIA_SelectionItem_ElementSelectedEventId: what = L"Selected"; break;
            case UIA_SelectionItem_ElementAddedToSelectionEventId: what = L"Added"; break;
            case UIA_SelectionItem_ElementRemovedFromSelectionEventId: what = L"Removed"; break;
            case UIA_Selection_InvalidatedEventId: what = L"Invalidated"; break;
            default: return {};
        }
    }
    else if (heard.kind == EventKind::Property && heard.id == UIA_SelectionItemIsSelectedPropertyId)
    {
        what = L"IsSelected:" + heard.value;
    }
    else
    {
        return {};
    }
    return what + L" " + ControlTypeName(heard.controlType) + L" '" + heard.name + L"'";
}

// The distinct selection events after this action's mark, followed by an observed quiet interval. That interval is not a
// guarantee against later repeats. Described by DescribeSelectionEvent and sorted for comparison with the expected set.
[[nodiscard]] inline std::vector<std::wstring> HearSelectionEvents(Client& client, size_t expected, const std::function<void()>& action)
{
    const auto before = client.WaitOutTheStream();
    const size_t mark = client.Mark();
    if (client.TimelineEnabled())
        client.BeginSelectionStep(mark, expected, before);
    const auto describe = [mark](std::vector<HeardEvent> events)
    {
        std::vector<std::wstring> texts;
        for (const HeardEvent& heard : DistinctEventsSince(std::move(events), mark))
        {
            if (std::wstring text = DescribeSelectionEvent(heard); ! text.empty())
                texts.push_back(std::move(text));
        }
        std::ranges::sort(texts);
        texts.erase(std::ranges::unique(texts).begin(), texts.end());
        return texts;
    };
    const auto described          = [&] { return describe(client.Events()); };
    const ULONGLONG actionStarted = client.TimelineEnabled() ? GetTickCount64() : 0u;
    action();
    const ULONGLONG actionCompleted = client.TimelineEnabled() ? GetTickCount64() : 0u;
    bool expectedArrived            = true;
    if (expected != 0u)
        expectedArrived = client.WaitUntil(Client::kNotificationDeadlineMs, [&] { return described().size() >= expected; });
    const auto after = client.WaitOutTheStream();
    if (! client.TimelineEnabled())
        return described();
    const auto snapshot = client.CaptureEvents();
    auto result         = describe(snapshot.events);
    client.PrintSelectionStep(snapshot, mark, expected, expectedArrived, actionStarted, actionCompleted, after);
    return result;
}

// The events `expected` lists, sorted as HearSelectionEvents sorts what it heard.
[[nodiscard]] inline std::vector<std::wstring> SortedEvents(std::vector<std::wstring> expected)
{
    std::ranges::sort(expected);
    return expected;
}

// For a failure report: each event, its non-ASCII characters as '?'.
[[nodiscard]] inline std::string DescribeEventList(const std::vector<std::wstring>& events)
{
    std::string text;
    for (const std::wstring& event : events)
    {
        text += " [";
        for (const wchar_t unit : event)
            text += unit < 0x80 ? static_cast<char>(unit) : '?';
        text += "]";
    }
    return text.empty() ? std::string(" nothing") : text;
}
} // namespace UiaTest
