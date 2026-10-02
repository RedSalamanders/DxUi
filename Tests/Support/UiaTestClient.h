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
#include <wrl/implements.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <compare>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace UiaTest
{
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

class Recorder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
                                                           IUIAutomationEventHandler,
                                                           IUIAutomationPropertyChangedEventHandler,
                                                           IUIAutomationStructureChangedEventHandler,
                                                           IUIAutomationFocusChangedEventHandler,
                                                           Microsoft::WRL::FtmBase>
{
public:
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

    [[nodiscard]] size_t Count(const std::function<bool(const HeardEvent&)>& matches) const
    {
        const std::scoped_lock lock(_mutex);
        return static_cast<size_t>(std::ranges::count_if(_events, matches));
    }

private:
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
            _events.push_back(std::move(heard));
        }
        catch (const std::bad_alloc&)
        {
            // A dropped event fails the test's own wait for it; a client's callback never throws.
        }
    }

    mutable std::mutex _mutex;
    std::vector<HeardEvent> _events;
};

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
    Client(HWND target, Subscription subscription, std::function<void()> pump) : _target(target), _subscription(std::move(subscription)), _pump(std::move(pump))
    {
        _recorder.attach(Microsoft::WRL::Make<Recorder>().Detach());
        if (! _recorder || ! _stop || ! _request)
            Fail("allocate the UI Automation client");
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
            Sleep(1);
        }
        return predicate();
    }

    // Lets events already raised reach the client before a test asserts that one never came.
    void Settle(ULONGLONG durationMs = 500u) const
    {
        static_cast<void>(WaitUntil(durationMs, [] { return false; }));
    }

    // Waits until no event has arrived for `quietMs` (for at most `deadlineMs`). An in-process client hears an event again a
    // moment after the first time (about 60 ms apart when several were raised at once), so a step that ends here leaves no repeat
    // to be heard by the next one.
    void WaitOutTheStream(ULONGLONG quietMs = 500u, ULONGLONG deadlineMs = 10000u) const
    {
        size_t arrived        = _recorder->Events().size();
        ULONGLONG lastArrival = GetTickCount64();
        static_cast<void>(WaitUntil(deadlineMs,
                                    [&]
        {
            if (const size_t now = _recorder->Events().size(); now != arrived)
            {
                arrived     = now;
                lastArrival = GetTickCount64();
            }
            return GetTickCount64() - lastArrival >= quietMs;
        }));
    }

    // Where the events heard so far end, for DistinctSince.
    [[nodiscard]] size_t Mark() const
    {
        return _recorder->Events().size();
    }

    // The events heard after `mark`, each once and in a fixed order: UI Automation delivers to a client on threads of its own, so
    // two events raised one after the other may arrive in either order, and it may deliver one twice.
    [[nodiscard]] std::vector<HeardEvent> DistinctSince(size_t mark) const
    {
        std::vector<HeardEvent> events = _recorder->Events();
        events.erase(events.begin(), events.begin() + static_cast<ptrdiff_t>((std::min)(mark, events.size())));
        std::ranges::sort(events);
        events.erase(std::ranges::unique(events).begin(), events.end());
        return events;
    }

    [[nodiscard]] std::vector<HeardEvent> Events() const
    {
        return _recorder->Events();
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

    // For a failure report: every event heard, in order.
    void PrintEvents() const
    {
        std::cerr << "    [UIA] events heard:";
        for (const HeardEvent& heard : _recorder->Events())
        {
            std::cerr << " [";
            for (const wchar_t unit : DescribeEvent(heard))
                std::cerr << (unit < 0x80 ? static_cast<char>(unit) : '?');
            std::cerr << "]";
        }
        std::cerr << '\n';
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
            hr = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(session.automation.put()));
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

// The selection events a client hears for `action`: the different ones that arrived once at least `expected` had (or the wait
// for them ran out) and the stream of events then stopped, so that a repeat of one does not reach the next step. Described by
// DescribeSelectionEvent and sorted, for a comparison with what a test expects.
[[nodiscard]] inline std::vector<std::wstring> HearSelectionEvents(Client& client, size_t expected, const std::function<void()>& action)
{
    client.WaitOutTheStream();
    const size_t mark    = client.Mark();
    const auto described = [&]
    {
        std::vector<std::wstring> texts;
        for (const HeardEvent& heard : client.DistinctSince(mark))
        {
            if (std::wstring text = DescribeSelectionEvent(heard); ! text.empty())
                texts.push_back(std::move(text));
        }
        std::ranges::sort(texts);
        texts.erase(std::ranges::unique(texts).begin(), texts.end());
        return texts;
    };
    action();
    if (expected != 0u)
        static_cast<void>(client.WaitUntil(Client::kNotificationDeadlineMs, [&] { return described().size() >= expected; }));
    client.WaitOutTheStream();
    return described();
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
