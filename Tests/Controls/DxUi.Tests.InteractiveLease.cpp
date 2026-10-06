// The interactive desktop lease behind `test.ps1 -Interactive` (Tests/Support/Support.Tests.DesktopLease.h and InteractiveLease.h), exercised without a
// person's desktop: the lease restores a desktop that a test builds out of fake windows, a fake pointer and a fake foreground, so no
// window is activated and no pointer moves. The few tests of the real Windows backend only read the desktop, or read windows the test
// created itself and never showed. Tests/InteractiveLease holds the Windows services (dialog, warning, children), which its --self-test
// runs on a private desktop; Tools/tests/Test-InteractiveLease.ps1 runs that.

#include "Controls.Tests.DxUiTestHelpers.h"

#include "../Support/Support.Tests.InteractiveLease.h"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

void RunInteractiveLeaseTests();

namespace
{
using namespace DxUi::TestSupport;
using Events = std::vector<std::string>;

[[nodiscard]] HWND FakeWindow(std::uintptr_t id) noexcept
{
    return reinterpret_cast<HWND>(id);
}

[[nodiscard]] size_t Count(const Events& events, std::string_view event)
{
    return static_cast<size_t>(std::ranges::count(events, std::string(event)));
}

[[nodiscard]] long IndexOf(const Events& events, std::string_view event)
{
    const auto found = std::ranges::find(events, std::string(event));
    return found == events.end() ? -1l : static_cast<long>(found - events.begin());
}

[[nodiscard]] std::string Join(const Events& events)
{
    std::string text;
    for (const std::string& event : events)
        text += (text.empty() ? "" : " ") + event;
    return text;
}

// A desktop made of fake windows. It records what the lease asks of it, in order, and behaves as the knobs say: a foreground the system
// refuses to hand over, one that never sticks, an activation that snaps the pointer to a default button or resets the focus.
class FakeDesktop final : public DesktopBackend
{
public:
    explicit FakeDesktop(Events& events) noexcept : _events(events)
    {
    }

    void AddWindow(HWND window, DWORD thread)
    {
        _threads[window] = thread;
    }

    void Destroy(HWND window)
    {
        _threads.erase(window);
        if (_foreground == window)
            _foreground = nullptr;
        for (auto& focus : _focus)
        {
            if (focus.second == window)
                focus.second = nullptr;
        }
    }

    void TakeForeground(HWND window) noexcept
    {
        _foreground = window;
    }

    void SetFocusOf(DWORD thread, HWND window)
    {
        _focus[thread] = window;
    }

    [[nodiscard]] HWND FocusOf(DWORD thread) const
    {
        const auto found = _focus.find(thread);
        return found == _focus.end() ? nullptr : found->second;
    }

    [[nodiscard]] HWND Foreground() override
    {
        return _foreground;
    }

    [[nodiscard]] bool IsWindowAlive(HWND window) override
    {
        return window != nullptr && _threads.contains(window);
    }

    [[nodiscard]] DWORD WindowThread(HWND window) override
    {
        const auto found = _threads.find(window);
        return found == _threads.end() ? 0u : found->second;
    }

    [[nodiscard]] HWND ThreadFocus(DWORD thread) override
    {
        return FocusOf(thread);
    }

    [[nodiscard]] bool Cursor(POINT& position) override
    {
        if (! cursorReadable)
            return false;
        position = cursor;
        return true;
    }

    bool SetCursor(POINT position) override
    {
        _events.push_back(std::format("cursor:{},{}", position.x, position.y));
        cursor = cursorLandsOneToTheRight ? POINT{position.x + 1, position.y} : position;
        return true;
    }

    bool BringToForeground(HWND window) override
    {
        _events.push_back("foreground");
        ++foregroundCalls;
        if (refusedForegroundCalls > 0)
        {
            --refusedForegroundCalls;
            return false;
        }
        if (foregroundNeverSticks)
            return true; // The call reports success and nothing changes: the lease reads the desktop instead of trusting it.
        _foreground = window;
        if (activationResetsFocus)
            _focus[WindowThread(window)] = window;
        if (activationSnapsCursorTo.has_value())
            cursor = *activationSnapsCursorTo;
        return true;
    }

    bool FocusWindow(HWND window, DWORD thread) override
    {
        _events.push_back("focus");
        if (focusRefused)
            return false;
        _focus[thread] = window;
        return true;
    }

    void Wait(std::chrono::milliseconds) override
    {
        ++waits; // Nothing sleeps: the settle loops are bounded by their attempt counts.
    }

    [[nodiscard]] std::string Describe(HWND window) override
    {
        return window != nullptr ? std::format("class 'Fake' of fake.exe (process {})", WindowThread(window)) : std::string{};
    }

    POINT cursor{400, 300};
    bool cursorReadable                          = true;
    bool cursorLandsOneToTheRight                = false;
    int refusedForegroundCalls                   = 0;
    bool foregroundNeverSticks                   = false;
    bool activationResetsFocus                   = false;
    std::optional<POINT> activationSnapsCursorTo = std::nullopt;
    bool focusRefused                            = false;
    int foregroundCalls                          = 0;
    int waits                                    = 0;

private:
    Events& _events;
    HWND _foreground = nullptr;
    std::map<HWND, DWORD> _threads;
    std::map<DWORD, HWND> _focus;
};

// The person's desktop: their editor in front, the caret in a child window of it, the pointer somewhere over it.
constexpr DWORD kPersonThread = 100u;
constexpr DWORD kTestThread   = 200u;

struct Person
{
    HWND editor = FakeWindow(0x1000);
    HWND caret  = FakeWindow(0x1001);
    HWND tests  = FakeWindow(0x2000);
};

void SeatThePerson(FakeDesktop& desktop, const Person& person)
{
    desktop.AddWindow(person.editor, kPersonThread);
    desktop.AddWindow(person.caret, kPersonThread);
    desktop.AddWindow(person.tests, kTestThread);
    desktop.TakeForeground(person.editor);
    desktop.SetFocusOf(kPersonThread, person.caret);
}

// What an interactive run does to the desktop: its window takes the foreground, the editor loses its focus and the pointer ends up
// over a test window.
void TakeOver(FakeDesktop& desktop, const Person& person)
{
    desktop.TakeForeground(person.tests);
    desktop.SetFocusOf(kPersonThread, nullptr);
    desktop.SetFocusOf(kTestThread, person.tests);
    desktop.cursor = POINT{900, 700};
}

[[nodiscard]] bool Is(const RestoreReport& report, Restoration foreground, Restoration focus, Restoration cursor) noexcept
{
    return report.foreground == foreground && report.focus == focus && report.cursor == cursor;
}

void TestDesktopLeaseLeavesAnUntouchedDesktopAlone()
{
    Events events;
    FakeDesktop desktop(events);
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    const RestoreReport report = lease.Restore();
    Require(Is(report, Restoration::Unchanged, Restoration::Unchanged, Restoration::Unchanged), "a run that changed nothing leaves nothing to restore");
    Require(events.empty() && desktop.foregroundCalls == 0, "and the lease asks the desktop for nothing");
    Require(report.Succeeded() && ! report.RunMovedSomething(), "an untouched desktop is a success that the run did not disturb");
    Require(report.Line() == "Restoration: foreground=unchanged focus=unchanged cursor=unchanged", "the report's line names each part");
}

void TestDesktopLeaseGivesBackTheForegroundWindowAndItsFocus()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.activationResetsFocus = true; // An application that puts its focus on the window itself when activated.
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    desktop.cursor             = POINT{400, 300}; // This run left the pointer where it was.
    const RestoreReport report = lease.Restore();
    Require(desktop.Foreground() == person.editor, "the person's window is the foreground window again");
    Require(desktop.FocusOf(kPersonThread) == person.caret, "and the child that held the keyboard focus holds it again");
    Require(Is(report, Restoration::Restored, Restoration::Restored, Restoration::Unchanged), "the report says what the lease restored");
    Require(report.RunMovedSomething(), "and that the run had moved something");
    Require(report.saved.foreground == person.editor && report.atExit.foreground == person.tests, "it keeps what the person had and what the run left");
    Require(IndexOf(events, "foreground") < IndexOf(events, "focus"), "the focus is set after the window that owns it is the foreground");
}

void TestDesktopLeaseGivesBackThePointerLast()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.activationSnapsCursorTo = POINT{5, 5}; // A dialog that snaps the pointer to its default button when activated.
    desktop.activationResetsFocus   = true;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(desktop.cursor.x == 400 && desktop.cursor.y == 300, "the pointer is where the person had it, though activating their window moved it");
    Require(report.atExit.cursor.x == 900 && report.atExit.cursor.y == 700, "the report keeps where the run left it, not where an activation put it");
    Require(Is(report, Restoration::Restored, Restoration::Restored, Restoration::Restored), "all three parts were restored");
    Require(events.back() == "cursor:400,300", "the pointer is the last thing the lease sets");
}

void TestDesktopLeaseRetriesAForegroundTheSystemRefusesOnce()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.refusedForegroundCalls = 2;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.foreground == Restoration::Restored && desktop.Foreground() == person.editor,
            "a foreground the system refused twice is restored by the third try");
    Require(desktop.foregroundCalls == 3, "after exactly three requests");
}

void TestDesktopLeaseReportsAForegroundItCannotRestore()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.refusedForegroundCalls = 1000;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.foreground == Restoration::Failed, "a foreground the system keeps refusing is reported as failed");
    Require(desktop.foregroundCalls == DesktopLease::kForegroundAttempts, "after the documented number of requests, not forever");
    Require(report.focus == Restoration::NotAttempted, "the focus of a window that is not the foreground is not set");
    Require(report.cursor == Restoration::Restored && desktop.cursor.x == 400, "but the pointer is still given back");
    Require(! report.Succeeded(), "and the run is not a success");
}

void TestDesktopLeaseDoesNotTrustACallThatReportsSuccess()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.foregroundNeverSticks = true;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.foreground == Restoration::Failed,
            "a call that returns success while the foreground stays elsewhere is a failure: the lease reads the desktop");
    Require(desktop.waits > 0, "it waited for the change to settle before giving up");
}

void TestDesktopLeaseDoesNotActivateAWindowThatIsGone()
{
    Events events;
    FakeDesktop desktop(events);
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    desktop.Destroy(person.editor);
    desktop.Destroy(person.caret);
    const RestoreReport report = lease.Restore();
    Require(Is(report, Restoration::Gone, Restoration::Gone, Restoration::Restored),
            "a window that closed during the run is reported gone, and the pointer is given back");
    Require(desktop.foregroundCalls == 0 && Count(events, "focus") == 0, "no other window is activated in its place");
    Require(report.Succeeded(), "a closed window is nothing the lease could have restored");
}

void TestDesktopLeaseReportsAPointerItCannotRestore()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.cursorLandsOneToTheRight = true;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.cursor == Restoration::Failed, "a pointer that lands a pixel off is failed, not rounded to a pass");
    Require(! report.Succeeded(), "and the run is not a success");
}

void TestDesktopLeaseWithAnUnreadablePointer()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.cursorReadable = false;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    Require(! lease.Saved().cursorKnown, "an unreadable pointer is not recorded");
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.cursor == Restoration::NotRecorded && Count(events, "cursor:900,700") == 0,
            "so none is invented, and the pointer is not set to an unknown position");
}

void TestDesktopLeaseRecordsNothingWhenThereIsNoForegroundWindow()
{
    Events events;
    FakeDesktop desktop(events);
    const Person person;
    desktop.AddWindow(person.tests, kTestThread);
    DesktopLease lease(desktop);
    lease.Capture();
    desktop.TakeForeground(person.tests);
    const RestoreReport report = lease.Restore();
    Require(report.foreground == Restoration::NotRecorded && report.focus == Restoration::NotRecorded,
            "no foreground window was recorded, so none is restored");
    Require(desktop.Foreground() == person.tests && desktop.foregroundCalls == 0, "and the run's window is not moved out of the way");
}

void TestDesktopLeaseRestoreIsRepeatableAndNeedsACapture()
{
    Events events;
    FakeDesktop desktop(events);
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    TakeOver(desktop, person);
    const RestoreReport uncaptured = lease.Restore();
    Require(Is(uncaptured, Restoration::NotRecorded, Restoration::NotRecorded, Restoration::NotRecorded) && events.empty(),
            "a lease that recorded nothing restores nothing");
    desktop.TakeForeground(person.editor);
    desktop.SetFocusOf(kPersonThread, person.caret);
    desktop.cursor = POINT{400, 300};
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport first  = lease.Restore();
    const RestoreReport second = lease.Restore();
    Require(first.RunMovedSomething() && Is(second, Restoration::Unchanged, Restoration::Unchanged, Restoration::Unchanged),
            "restoring again finds everything where it was put");
}

void TestDesktopLeaseKeepsAFocusWindowThatAlreadyHasIt()
{
    Events events;
    FakeDesktop desktop(events);
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    desktop.TakeForeground(person.tests); // The foreground went elsewhere; the editor keeps its own focus window meanwhile.
    desktop.SetFocusOf(kTestThread, person.tests);
    const RestoreReport report = lease.Restore();
    Require(Is(report, Restoration::Restored, Restoration::Unchanged, Restoration::Unchanged),
            "when activating the window already gives its child the focus, the lease sets nothing");
    Require(Count(events, "focus") == 0, "it does not ask for the focus again");
}

void TestDesktopLeaseReportsAFocusItCannotRestore()
{
    Events events;
    FakeDesktop desktop(events);
    desktop.activationResetsFocus = true;
    desktop.focusRefused          = true;
    const Person person;
    SeatThePerson(desktop, person);
    DesktopLease lease(desktop);
    lease.Capture();
    TakeOver(desktop, person);
    const RestoreReport report = lease.Restore();
    Require(report.foreground == Restoration::Restored && report.focus == Restoration::Failed,
            "a focus the system would not return is failed beside a foreground that was");
    Require(! report.Succeeded(), "and the run is not a success");
}

// ---- Is there a desktop to take? ------------------------------------------------------------------------------------------------

class FakeProbe final : public DesktopProbe
{
public:
    [[nodiscard]] bool InteractiveWindowStation() override
    {
        return interactiveStation;
    }
    [[nodiscard]] DWORD SessionId() override
    {
        return session;
    }
    [[nodiscard]] bool SessionActive(std::string& state) override
    {
        state = sessionState;
        return sessionActive;
    }
    [[nodiscard]] bool InputDesktopName(std::string& name, DWORD& error) override
    {
        error = inputError;
        name  = inputDesktop;
        return inputDesktopOpens;
    }
    [[nodiscard]] std::string ThreadDesktopName() override
    {
        return ownDesktop;
    }
    [[nodiscard]] bool ScreenSaverRunning() override
    {
        return screenSaver;
    }
    [[nodiscard]] bool CursorReadable() override
    {
        return cursorReadable;
    }

    bool interactiveStation  = true;
    DWORD session            = 1u;
    bool sessionActive       = true;
    std::string sessionState = "active";
    bool inputDesktopOpens   = true;
    DWORD inputError         = ERROR_SUCCESS;
    std::string inputDesktop = "Default";
    std::string ownDesktop   = "Default";
    bool screenSaver         = false;
    bool cursorReadable      = true;
};

[[nodiscard]] bool Mentions(const Availability& availability, std::string_view text)
{
    return ! availability.available && availability.reason.find(text) != std::string::npos;
}

void TestInteractiveDesktopCheckAcceptsAnActiveSession()
{
    FakeProbe probe;
    Availability availability = CheckInteractiveDesktop(probe);
    Require(availability.available && availability.reason.empty(), "an active session whose input desktop is this process's desktop is available");
    probe.inputDesktop = "DEFAULT";
    availability       = CheckInteractiveDesktop(probe);
    Require(availability.available, "desktop names compare without regard to case");
}

void TestInteractiveDesktopCheckNamesEachReasonToRefuse()
{
    {
        FakeProbe probe;
        probe.interactiveStation = false;
        Require(Mentions(CheckInteractiveDesktop(probe), "service, a scheduled task or a remote shell"),
                "a process without a visible window station is refused");
    }
    {
        FakeProbe probe;
        probe.session = 0u;
        Require(Mentions(CheckInteractiveDesktop(probe), "session 0"), "session 0 is refused");
    }
    {
        FakeProbe probe;
        probe.sessionActive = false;
        probe.sessionState  = "disconnected";
        Require(Mentions(CheckInteractiveDesktop(probe), "disconnected"), "a disconnected session is refused, naming its state");
    }
    {
        FakeProbe probe;
        probe.inputDesktopOpens         = false;
        probe.inputError                = ERROR_ACCESS_DENIED;
        const Availability availability = CheckInteractiveDesktop(probe);
        Require(Mentions(availability, "locked") && Mentions(availability, std::to_string(ERROR_ACCESS_DENIED)),
                "a locked session is refused, with the error that said so");
    }
    {
        FakeProbe probe;
        probe.inputDesktop              = "Winlogon";
        const Availability availability = CheckInteractiveDesktop(probe);
        Require(Mentions(availability, "Winlogon") && Mentions(availability, "Default"), "an input desktop that is not this process's is refused, naming both");
    }
    {
        FakeProbe probe;
        probe.screenSaver = true;
        Require(Mentions(CheckInteractiveDesktop(probe), "screen saver"), "a running screen saver is refused");
    }
    {
        FakeProbe probe;
        probe.cursorReadable = false;
        Require(Mentions(CheckInteractiveDesktop(probe), "pointer position cannot be read"), "a pointer that cannot be read, hence not restored, is refused");
    }
}

void TestInteractiveDesktopCheckStopsAtTheFirstReason()
{
    FakeProbe probe;
    probe.interactiveStation        = false;
    probe.screenSaver               = true;
    probe.cursorReadable            = false;
    const Availability availability = CheckInteractiveDesktop(probe);
    Require(Mentions(availability, "window station") && ! Mentions(availability, "screen saver"),
            "the reason is the first check that fails, in the order a person would fix them");
}

void TestWin32BackendReadsTheDesktopWithoutChangingIt()
{
    Win32DesktopProbe probe;
    const Availability availability = CheckInteractiveDesktop(probe);
    Require(availability.available || ! availability.reason.empty(), "the real probe answers, and says why when this session has no desktop");

    Win32DesktopBackend backend;
    POINT before{};
    if (GetPhysicalCursorPos(&before) != FALSE)
    {
        // The pointer may move between two reads on a person's desktop, so the backend's read must match one of its neighbours.
        bool same = false;
        for (int attempt = 0; attempt < 20 && ! same; ++attempt)
        {
            POINT cursor{};
            POINT after{};
            Require(GetPhysicalCursorPos(&before) != FALSE && backend.Cursor(cursor) && GetPhysicalCursorPos(&after) != FALSE,
                    "the real backend reads the pointer");
            same = SamePoint(cursor, before) || SamePoint(cursor, after);
        }
        Require(same, "and it is the physical position the system reports");
    }
    Require(! backend.IsWindowAlive(nullptr) && backend.WindowThread(nullptr) == 0u, "no window is alive, or has a thread");

    // A window of this test's own thread that is never shown: nothing of the person's desktop is touched.
    wil::unique_hwnd window(CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                                            L"STATIC",
                                            L"DxUiTestsLease",
                                            WS_POPUP,
                                            -32000,
                                            -32000,
                                            8,
                                            8,
                                            nullptr,
                                            nullptr,
                                            GetModuleHandleW(nullptr),
                                            nullptr));
    Require(window != nullptr, "the test creates a window it never shows");
    const HWND handle = window.get();
    Require(backend.IsWindowAlive(handle) && backend.WindowThread(handle) == GetCurrentThreadId(), "the real backend finds a window and its thread");
    Require(backend.ThreadFocus(GetCurrentThreadId()) == GetFocus(), "and the focus of a thread is what the thread's own GetFocus says");
    std::string description = backend.Describe(handle);
    std::ranges::transform(description, description.begin(), [](char ch) noexcept {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }); // The system names its classes "Static".
    Require(description.find("static") != std::string::npos && description.find("process") != std::string::npos,
            "a window is described by its class and process");
    window.reset();
    Require(! backend.IsWindowAlive(handle) && backend.WindowThread(handle) == 0u, "a destroyed window is gone");
    Require(backend.Describe(handle) == "no window", "and is described as none");
}

// ---- The lease as a whole -------------------------------------------------------------------------------------------------------

class FakeServices final : public LeaseServices
{
public:
    FakeServices(Events& events, FakeDesktop& desktop) noexcept : _events(events), _desktop(desktop)
    {
    }

    [[nodiscard]] Availability CheckDesktop() override
    {
        _events.push_back("check");
        return availability;
    }
    [[nodiscard]] bool TryAcquire() override
    {
        _events.push_back("acquire");
        return acquirable;
    }
    void Release() override
    {
        _events.push_back("release");
    }
    void KeepAwake(bool keep) override
    {
        _events.push_back(keep ? "awake" : "asleep");
    }
    [[nodiscard]] DesktopBackend& Desktop() override
    {
        return _desktop;
    }
    [[nodiscard]] Confirmation Confirm(const LeaseRequest&) override
    {
        _events.push_back("confirm");
        if (onConfirm)
            onConfirm();
        return answer;
    }
    [[nodiscard]] bool ShowWarning(const LeaseRequest&, std::string& reason) override
    {
        _events.push_back("warning");
        if (! warningShows)
        {
            reason = "no window station";
            return false;
        }
        return true;
    }
    void HideWarning() override
    {
        _events.push_back("hide");
    }
    [[nodiscard]] ChildResult RunChild(const ChildRun& run, unsigned) override
    {
        _events.push_back("child:" + ToUtf8(run.name));
        const size_t index = children++;
        if (onChild)
            onChild(index);
        if (index < results.size())
            return results[index];
        ChildResult passed{};
        passed.launched = true;
        passed.seconds  = 1.5;
        return passed;
    }
    [[nodiscard]] bool Interrupted() override
    {
        return stopped;
    }
    void Log(std::string_view line) override
    {
        log.emplace_back(line);
    }

    Availability availability{true, {}};
    bool acquirable     = true;
    Confirmation answer = Confirmation::Started;
    bool warningShows   = true;
    bool stopped        = false;
    std::vector<ChildResult> results;
    std::function<void()> onConfirm;
    std::function<void(size_t)> onChild;
    size_t children = 0u;
    Events log;

private:
    Events& _events;
    FakeDesktop& _desktop;
};

[[nodiscard]] LeaseRequest RequestFor(std::initializer_list<const wchar_t*> suites)
{
    LeaseRequest request;
    request.label           = L"Menu, NativeTextInput (x64 Debug)";
    request.estimateSeconds = 100u;
    for (const wchar_t* suite : suites)
        request.children.push_back({suite, std::wstring(L"C:\\logs\\") + suite + L".log", std::wstring(L"tests.exe --suite=") + suite});
    return request;
}

[[nodiscard]] ChildResult Exited(DWORD code)
{
    ChildResult result{};
    result.launched = true;
    result.exitCode = code;
    result.seconds  = 2.0;
    return result;
}

// A whole lease on a person's fake desktop, in which the tests take the desktop over while the first child runs.
struct Lease
{
    Lease()
    {
        SeatThePerson(desktop, person);
        services.onChild = [this](size_t index)
        {
            if (index == 0u)
                TakeOver(desktop, person);
        };
    }

    Events events;
    FakeDesktop desktop{events};
    FakeServices services{events, desktop};
    Person person;
};

void TestLeaseRefusesWhenThereIsNoDesktopAndTouchesNothing()
{
    Lease lease;
    lease.services.availability = {false, "the session is locked"};
    const LeaseOutcome outcome  = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kNoDesktop && outcome.state == "refused" && outcome.reason == "the session is locked",
            "a run without a desktop is refused with the reason");
    Require(Join(lease.events) == "check", "and nothing else happens: no lease is taken, no one is asked, no child runs, no desktop is read");
    Require(! outcome.restoration.has_value(), "so there is nothing to restore");
}

void TestLeaseRefusesWhenAnotherRunHoldsIt()
{
    Lease lease;
    lease.services.acquirable  = false;
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kBusy && outcome.state == "busy", "a run finds the session's lease held");
    Require(Join(lease.events) == "check acquire", "and neither asks nor runs anything, nor releases what it does not hold");
}

void TestLeaseDoesNotTakeTheDesktopWhenTheAnswerIsCancel()
{
    Lease lease;
    // The dialog took the foreground to be seen, and closing it leaves the foreground wherever Windows put it.
    lease.services.onConfirm   = [&] { lease.desktop.TakeForeground(lease.person.tests); };
    lease.services.answer      = Confirmation::Declined;
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(outcome.exitCode == LeaseExit::kDeclined && outcome.state == "declined", "Cancel ends the run");
    Require(Count(lease.events, "warning") == 0 && lease.services.children == 0u, "with no warning shown and no child run");
    Require(lease.desktop.Foreground() == lease.person.editor, "and the person's window is the foreground again, though the dialog moved it");
    Require(outcome.restoration.has_value() && outcome.restoration->foreground == Restoration::Restored, "which the outcome reports");
    Require(Join(lease.events) == "check acquire awake confirm foreground hide asleep release", "in the order: ask, restore, then let go of the lease");
}

void TestLeaseCancelsWhenNobodyAnswers()
{
    Lease lease;
    lease.services.answer      = Confirmation::TimedOut;
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kDeclined && outcome.state == "timedout" && outcome.confirmation == Confirmation::TimedOut,
            "nobody answering cancels the run, and says so");
    Require(lease.services.children == 0u, "no child runs");
}

void TestLeaseRunsEachChildAndRestoresAfterThem()
{
    Lease lease;
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(outcome.exitCode == LeaseExit::kPassed && outcome.state == "completed", "two passing children are a passing run");
    Require(outcome.children.size() == 2u && outcome.children[0].name == "Menu" && outcome.children[1].name == "NativeTextInput",
            "both are in the outcome, in order");
    Require(Join(lease.events) == "check acquire awake confirm warning child:Menu child:NativeTextInput foreground focus cursor:400,300 hide asleep release",
            "the order is: check, take the lease, ask, warn, run each child, restore (window, focus, pointer), take the warning down, let go");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.FocusOf(kPersonThread) == lease.person.caret && lease.desktop.cursor.x == 400,
            "the person has their window, its focus and their pointer back");
    Require(outcome.restoration.has_value() && outcome.restoration->Succeeded() && outcome.restoration->RunMovedSomething(),
            "and the outcome says the run had taken them");
    Require(outcome.savedForeground == "class 'Fake' of fake.exe (process 100)", "it names the window it restored");
}

void TestLeaseRestoresAfterAFailingChild()
{
    Lease lease;
    lease.services.results     = {Exited(1), Exited(0)};
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(outcome.exitCode == LeaseExit::kChildFailed && outcome.state == "completed", "a failing child is a failed run");
    Require(lease.services.children == 2u, "the next child still runs: one failure does not leave the other suite unqualified");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.cursor.x == 400, "and the desktop is given back");
}

void TestLeaseRestoresAfterTheWatchdogEndsAChild()
{
    Lease lease;
    lease.services.results     = {Exited(kTestTimeoutExitCode)};
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kChildFailed && outcome.children[0].result.exitCode == 124u,
            "a child the watchdog ended, exit code 124, is a failed run");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.FocusOf(kPersonThread) == lease.person.caret && lease.desktop.cursor.x == 400,
            "and the desktop is given back though the child never restored anything itself");
}

void TestLeaseRestoresAfterAChildTheLeaseEnded()
{
    Lease lease;
    ChildResult bound          = Exited(0u); // Whatever code a child the lease ended is recorded with, ending it fails the run.
    bound.timedOut             = true;
    lease.services.results     = {bound};
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kChildFailed, "a child that outlived the lease's bound fails the run, whatever its exit code says");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.cursor.x == 400, "and the desktop is given back");
}

void TestLeaseRestoresWhenAChildCannotBeStarted()
{
    Lease lease;
    ChildResult failed;
    failed.error               = "cannot start tests.exe (error 2)";
    lease.services.results     = {failed};
    lease.services.onChild     = [&](size_t) { TakeOver(lease.desktop, lease.person); };
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(outcome.exitCode == LeaseExit::kLaunchFailed && outcome.state == "launchfailed" && outcome.reason == "cannot start tests.exe (error 2)",
            "a child that cannot be started fails the run with its reason");
    Require(lease.services.children == 1u, "and the next one is not tried");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.cursor.x == 400, "yet the desktop is given back");
}

void TestLeaseRestoresWhenTheRunIsStopped()
{
    Lease lease;
    ChildResult stopped        = Exited(130u);
    stopped.interrupted        = true;
    lease.services.results     = {stopped};
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(outcome.exitCode == LeaseExit::kInterrupted && outcome.state == "interrupted", "a run stopped while a child runs is interrupted");
    Require(lease.services.children == 1u, "and no later child starts");
    Require(lease.desktop.Foreground() == lease.person.editor && lease.desktop.cursor.x == 400, "the desktop is given back");

    Lease between;
    between.services.onChild = [&](size_t)
    {
        TakeOver(between.desktop, between.person);
        between.services.stopped = true;
    };
    const LeaseOutcome second = RunLease(between.services, RequestFor({L"Menu", L"NativeTextInput"}));
    Require(second.exitCode == LeaseExit::kInterrupted && between.services.children == 1u, "a run stopped between two children starts no more");
    Require(between.desktop.Foreground() == between.person.editor, "and gives the desktop back");
}

void TestLeaseRestoresWhenTheConfirmationIsStopped()
{
    Lease lease;
    lease.services.onConfirm   = [&] { lease.desktop.TakeForeground(lease.person.tests); };
    lease.services.answer      = Confirmation::Interrupted;
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kInterrupted && lease.services.children == 0u, "a run stopped while the person is asked runs nothing");
    Require(lease.desktop.Foreground() == lease.person.editor, "and gives the foreground back");
}

void TestLeaseRunsNothingWhenTheWarningCannotBeShown()
{
    Lease lease;
    lease.services.warningShows = false;
    const LeaseOutcome outcome  = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kNoWarning && outcome.state == "unwarned", "a run whose warning cannot be shown does not take the desktop");
    Require(outcome.reason.find("no window station") != std::string::npos, "and says why");
    Require(lease.services.children == 0u && Count(lease.events, "hide") == 1u, "no child runs, and the warning is taken down once");

    Lease unavailable;
    unavailable.services.answer = Confirmation::Unavailable;
    Require(RunLease(unavailable.services, RequestFor({L"Menu"})).exitCode == LeaseExit::kNoWarning, "neither does a run whose confirmation cannot be shown");
}

void TestLeaseReportsADesktopItCouldNotRestore()
{
    Lease lease;
    lease.desktop.refusedForegroundCalls = 1000;
    const LeaseOutcome outcome           = RunLease(lease.services, RequestFor({L"Menu"}));
    Require(outcome.exitCode == LeaseExit::kNotRestored && outcome.state == "completed",
            "children that pass do not make a run that could not give the desktop back a pass");
    Require(outcome.restoration.has_value() && outcome.restoration->foreground == Restoration::Failed, "the outcome says which part failed");

    Lease failing;
    failing.desktop.refusedForegroundCalls = 1000;
    failing.services.results               = {Exited(1)};
    const LeaseOutcome second              = RunLease(failing.services, RequestFor({L"Menu"}));
    Require(second.exitCode == LeaseExit::kChildFailed && second.restoration->foreground == Restoration::Failed,
            "a child's failure keeps its own exit code, with the failed restoration beside it");
}

void TestLeaseReleasesTheSessionOnEveryPath()
{
    const auto releases = [](const std::function<void(Lease&)>& arrange)
    {
        Lease lease;
        arrange(lease);
        static_cast<void>(RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"})));
        return Count(lease.events, "acquire") == Count(lease.events, "release") && Count(lease.events, "awake") == Count(lease.events, "asleep");
    };
    Require(releases([](Lease&) {}), "a passing run lets the session and the display go");
    Require(releases([](Lease& lease) { lease.services.answer = Confirmation::Declined; }), "a cancelled run does");
    Require(releases([](Lease& lease) { lease.services.warningShows = false; }), "a run without a warning does");
    Require(releases([](Lease& lease) { lease.services.results = {Exited(124)}; }), "a run whose child the watchdog ended does");
    Require(releases([](Lease& lease) { lease.services.results = {ChildResult{}}; }), "a run whose child could not start does");
}

void TestLeaseResultNamesEveryFact()
{
    Lease lease;
    lease.services.results     = {Exited(0), Exited(1)};
    const LeaseOutcome outcome = RunLease(lease.services, RequestFor({L"Menu", L"NativeTextInput"}));
    const std::string text     = FormatLeaseResult(outcome);
    std::map<std::string, std::vector<std::string>> facts;
    size_t start = 0u;
    while (start < text.size())
    {
        const size_t end       = text.find('\n', start);
        const std::string line = text.substr(start, end - start);
        const size_t colon     = line.find(": ");
        Require(colon != std::string::npos, "every line of the result is key: value");
        facts[line.substr(0u, colon)].push_back(line.substr(colon + 2u));
        start = end == std::string::npos ? text.size() : end + 1u;
    }
    Require(facts["state"] == std::vector<std::string>{"completed"} && facts["exit"] == std::vector<std::string>{"1"}, "the state and exit code are there");
    Require(facts["confirmation"] == std::vector<std::string>{"started"}, "the confirmation is there");
    Require(facts["restoration.foreground"] == std::vector<std::string>{"restored"} && facts["restoration.focus"] == std::vector<std::string>{"restored"} &&
                facts["restoration.cursor"] == std::vector<std::string>{"restored"},
            "so is what each part of the desktop came to");
    Require(facts["cursor.saved"] == std::vector<std::string>{"400,300"} && facts["cursor.atExit"] == std::vector<std::string>{"900,700"},
            "with the pointer where the person had it and where the run left it");
    Require(facts["restoration.runMovedSomething"] == std::vector<std::string>{"1"}, "and whether the run had moved anything");
    Require(facts["child"].size() == 2u && facts["child"][0].starts_with("Menu launched=1 exit=0 seconds=2.0") &&
                facts["child"][1].starts_with("NativeTextInput launched=1 exit=1"),
            "each child has a line of its own");
    Require(facts["foreground.saved"].size() == 1u, "the window the person had is named");

    LeaseOutcome refused;
    refused.state                 = "refused";
    refused.exitCode              = LeaseExit::kNoDesktop;
    refused.reason                = "line one\nline two\tafter a tab";
    const std::string refusedText = FormatLeaseResult(refused);
    Require(refusedText.find("reason: line one line two after a tab\n") != std::string::npos, "a reason is one line, whatever it holds");
    Require(refusedText.find("restoration") == std::string::npos && refusedText.find("child:") == std::string::npos,
            "and a run that took nothing records no restoration and no child");
}

void TestLeasePlanFileParsing()
{
    std::vector<ChildRun> runs;
    std::string error;
    const std::string plan = "# the plan\r\n\r\nMenu\tC:\\logs\\Menu.log\t\"C:\\a b\\tests.exe\" --suite=Menu --test-timeout=300\r\n"
                             "NativeTextInput\tC:\\logs\\N\xC3\xA9.log\ttests.exe --suite=NativeTextInput\n";
    Require(ParsePlan(plan, runs, error) && runs.size() == 2u, "a plan lists its children, one per line, with comments and blank lines ignored");
    Require(runs[0].name == L"Menu" && runs[0].logPath == L"C:\\logs\\Menu.log" &&
                runs[0].commandLine == L"\"C:\\a b\\tests.exe\" --suite=Menu --test-timeout=300",
            "each line has a name, a log and a command line");
    Require(runs[1].logPath == L"C:\\logs\\N\u00E9.log", "paths are UTF-8");
    for (const char* bad : {"", "# nothing\n", "Menu\tlog-only\n", "Menu\t\tcommand\n", "\tlog\tcommand\n", "Menu\tlog\t\n"})
        Require(! ParsePlan(bad, runs, error) && ! error.empty() && runs.empty(), "a plan with no child, or a line without all three fields, is rejected");
}

void TestLeaseDurationTextIsApproximate()
{
    Require(FormatApproximateDuration(0u) == "less than a minute" && FormatApproximateDuration(44u) == "less than a minute",
            "under 45 seconds is less than a minute");
    Require(FormatApproximateDuration(45u) == "about a minute" && FormatApproximateDuration(89u) == "about a minute", "45 to 89 seconds is about a minute");
    Require(FormatApproximateDuration(90u) == "about 2 minutes" && FormatApproximateDuration(149u) == "about 2 minutes",
            "90 to 149 seconds is about two minutes");
    Require(FormatApproximateDuration(600u) == "about 10 minutes", "and so on");
}

void TestConfirmationCountdownEndsInACancellation()
{
    const ConfirmationCountdown countdown(120u);
    Require(! countdown.Unlimited() && countdown.SecondsLeft(0u) == 120u && countdown.SecondsLeft(1u) == 120u, "a countdown starts at its full time");
    Require(countdown.SecondsLeft(999u) == 120u && countdown.SecondsLeft(1000u) == 119u && countdown.SecondsLeft(119'001u) == 1u,
            "it rounds the seconds left up");
    Require(! countdown.Expired(119'999u) && countdown.Expired(120'000u) && countdown.SecondsLeft(120'000u) == 0u && countdown.SecondsLeft(999'999u) == 0u,
            "and runs out exactly at its time");
    const ConfirmationCountdown unlimited(0u);
    Require(unlimited.Unlimited() && ! unlimited.Expired(0xFFFFFFFFFFull) && unlimited.SecondsLeft(5u) == 0u, "a countdown of zero never runs out");
}

void TestLeaseExitCodesAreDistinctAndStable()
{
    const std::set<int> codes{LeaseExit::kPassed,
                              LeaseExit::kChildFailed,
                              LeaseExit::kUsage,
                              LeaseExit::kNoDesktop,
                              LeaseExit::kDeclined,
                              LeaseExit::kBusy,
                              LeaseExit::kNoWarning,
                              LeaseExit::kLaunchFailed,
                              LeaseExit::kNotRestored,
                              LeaseExit::kInterrupted};
    Require(codes.size() == 10u, "every exit code is different");
    Require(LeaseExit::kPassed == 0 && LeaseExit::kChildFailed == 1 && LeaseExit::kUsage == 2 && LeaseExit::kNoDesktop == 20 && LeaseExit::kInterrupted == 26,
            "and they are the ones Tools/InteractiveRun.psm1 reads");
}
} // namespace

void RunInteractiveLeaseTests()
{
    DXUI_RUN_TEST(TestDesktopLeaseLeavesAnUntouchedDesktopAlone);
    DXUI_RUN_TEST(TestDesktopLeaseGivesBackTheForegroundWindowAndItsFocus);
    DXUI_RUN_TEST(TestDesktopLeaseGivesBackThePointerLast);
    DXUI_RUN_TEST(TestDesktopLeaseRetriesAForegroundTheSystemRefusesOnce);
    DXUI_RUN_TEST(TestDesktopLeaseReportsAForegroundItCannotRestore);
    DXUI_RUN_TEST(TestDesktopLeaseDoesNotTrustACallThatReportsSuccess);
    DXUI_RUN_TEST(TestDesktopLeaseDoesNotActivateAWindowThatIsGone);
    DXUI_RUN_TEST(TestDesktopLeaseReportsAPointerItCannotRestore);
    DXUI_RUN_TEST(TestDesktopLeaseWithAnUnreadablePointer);
    DXUI_RUN_TEST(TestDesktopLeaseRecordsNothingWhenThereIsNoForegroundWindow);
    DXUI_RUN_TEST(TestDesktopLeaseRestoreIsRepeatableAndNeedsACapture);
    DXUI_RUN_TEST(TestDesktopLeaseKeepsAFocusWindowThatAlreadyHasIt);
    DXUI_RUN_TEST(TestDesktopLeaseReportsAFocusItCannotRestore);
    DXUI_RUN_TEST(TestInteractiveDesktopCheckAcceptsAnActiveSession);
    DXUI_RUN_TEST(TestInteractiveDesktopCheckNamesEachReasonToRefuse);
    DXUI_RUN_TEST(TestInteractiveDesktopCheckStopsAtTheFirstReason);
    DXUI_RUN_TEST(TestWin32BackendReadsTheDesktopWithoutChangingIt);
    DXUI_RUN_TEST(TestLeaseRefusesWhenThereIsNoDesktopAndTouchesNothing);
    DXUI_RUN_TEST(TestLeaseRefusesWhenAnotherRunHoldsIt);
    DXUI_RUN_TEST(TestLeaseDoesNotTakeTheDesktopWhenTheAnswerIsCancel);
    DXUI_RUN_TEST(TestLeaseCancelsWhenNobodyAnswers);
    DXUI_RUN_TEST(TestLeaseRunsEachChildAndRestoresAfterThem);
    DXUI_RUN_TEST(TestLeaseRestoresAfterAFailingChild);
    DXUI_RUN_TEST(TestLeaseRestoresAfterTheWatchdogEndsAChild);
    DXUI_RUN_TEST(TestLeaseRestoresAfterAChildTheLeaseEnded);
    DXUI_RUN_TEST(TestLeaseRestoresWhenAChildCannotBeStarted);
    DXUI_RUN_TEST(TestLeaseRestoresWhenTheRunIsStopped);
    DXUI_RUN_TEST(TestLeaseRestoresWhenTheConfirmationIsStopped);
    DXUI_RUN_TEST(TestLeaseRunsNothingWhenTheWarningCannotBeShown);
    DXUI_RUN_TEST(TestLeaseReportsADesktopItCouldNotRestore);
    DXUI_RUN_TEST(TestLeaseReleasesTheSessionOnEveryPath);
    DXUI_RUN_TEST(TestLeaseResultNamesEveryFact);
    DXUI_RUN_TEST(TestLeasePlanFileParsing);
    DXUI_RUN_TEST(TestLeaseDurationTextIsApproximate);
    DXUI_RUN_TEST(TestConfirmationCountdownEndsInACancellation);
    DXUI_RUN_TEST(TestLeaseExitCodesAreDistinctAndStable);
}
