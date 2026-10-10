#pragma once

#include <Windows.h>
#include <wtsapi32.h>

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027 28182)
#include <wil/resource.h>
#pragma warning(pop)

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>

#pragma comment(lib, "wtsapi32.lib")

namespace DxUi::TestSupport
{
// What a person has on the desktop at one moment: the foreground window, the keyboard focus inside the thread that owns it, and
// the physical pointer position. An interactive test run takes all three, so a run that ends, fails or is ended must hand all
// three back. `DesktopLease` records them before the run and restores them after it.
struct DesktopState
{
    HWND foreground        = nullptr;
    DWORD foregroundThread = 0u;
    HWND focus             = nullptr; // The focus window of foregroundThread's input state; null when it has none.
    POINT cursor{};
    bool cursorKnown = false;
};

[[nodiscard]] constexpr bool SamePoint(const POINT& left, const POINT& right) noexcept
{
    return left.x == right.x && left.y == right.y;
}

// What became of one part of the desktop when the lease restored it.
enum class Restoration
{
    NotRecorded,  // Nothing was recorded for it (no foreground window, no focus window, an unreadable pointer).
    Unchanged,    // The run left it where it was, so the lease did nothing.
    Restored,     // The run moved it, and the lease put it back and saw it there.
    Gone,         // The window the person had is gone (closed during the run): there is nothing to give back.
    Failed,       // The lease could not put it back, or could not see that it had.
    NotAttempted, // It follows from a part that failed: the keyboard focus of a window that could not be made the foreground.
};

[[nodiscard]] constexpr std::string_view RestorationName(Restoration restoration) noexcept
{
    switch (restoration)
    {
        case Restoration::NotRecorded: return "none";
        case Restoration::Unchanged: return "unchanged";
        case Restoration::Restored: return "restored";
        case Restoration::Gone: return "gone";
        case Restoration::Failed: return "failed";
        case Restoration::NotAttempted: return "skipped";
    }
    return "none";
}

struct RestoreReport
{
    Restoration foreground = Restoration::NotRecorded;
    Restoration focus      = Restoration::NotRecorded;
    Restoration cursor     = Restoration::NotRecorded;
    DesktopState saved;  // What the person had.
    DesktopState atExit; // What the run left, read before the lease acted: it says what the lease had to restore.

    // The desktop is as the person had it, or could not be given back because the window is gone.
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return foreground != Restoration::Failed && focus != Restoration::Failed && cursor != Restoration::Failed;
    }

    // Whether the run left any part elsewhere than the person had it. A pointer or window the run did not put back itself is a
    // finding of its own: the lease's restoration never stands in for a fixture's.
    [[nodiscard]] bool RunMovedSomething() const noexcept
    {
        return foreground == Restoration::Restored || focus == Restoration::Restored || cursor == Restoration::Restored;
    }

    [[nodiscard]] std::string Line() const
    {
        return std::format("Restoration: foreground={} focus={} cursor={}", RestorationName(foreground), RestorationName(focus), RestorationName(cursor));
    }
};

// The operations a lease needs from the desktop, so that restoring is tested without moving a pointer or taking a foreground.
class DesktopBackend
{
public:
    DesktopBackend(const DesktopBackend&)            = delete;
    DesktopBackend& operator=(const DesktopBackend&) = delete;
    DesktopBackend(DesktopBackend&&)                 = delete;
    DesktopBackend& operator=(DesktopBackend&&)      = delete;
    virtual ~DesktopBackend()                        = default;

    [[nodiscard]] virtual HWND Foreground()               = 0;
    [[nodiscard]] virtual bool IsWindowAlive(HWND window) = 0;
    [[nodiscard]] virtual DWORD WindowThread(HWND window) = 0; // 0 when the window is gone.
    [[nodiscard]] virtual HWND ThreadFocus(DWORD thread)  = 0; // The focus window of the thread's input state.
    [[nodiscard]] virtual bool Cursor(POINT& position)    = 0; // Physical coordinates.
    virtual bool SetCursor(POINT position)                = 0; // Whether the call was accepted.
    virtual bool BringToForeground(HWND window)           = 0; // Whether the call was accepted; the change itself is asynchronous.
    virtual bool FocusWindow(HWND window, DWORD thread)   = 0; // Whether the call was accepted.
    virtual void Wait(std::chrono::milliseconds duration) = 0; // Lets an asynchronous change settle.
    // For messages: the class and process of a window; empty where a backend has nothing to say.
    [[nodiscard]] virtual std::string Describe(HWND /*window*/)
    {
        return {};
    }

protected:
    DesktopBackend() = default;
};

// Records the desktop before an interactive run and puts it back afterwards. Restoration is an action, not a check: it never makes
// a fixture's own restoration pass, which is why the report keeps what the run left (`atExit`) beside what the lease did.
//
// The order is the foreground window first, then its keyboard focus, then the pointer: activating a window can move the pointer
// (a dialog that snaps it to its default button), so the pointer is the last thing the lease sets. Every step is verified by reading
// the desktop again, never by trusting the call's result, because the foreground changes asynchronously and the foreground lock
// can refuse a call that reports success.
class DesktopLease final
{
public:
    // A foreground change is read back for at most kSettleAttempts * kSettleStep, and asked for kForegroundAttempts times.
    static constexpr int kSettleAttempts                   = 50;
    static constexpr std::chrono::milliseconds kSettleStep = std::chrono::milliseconds(20);
    static constexpr int kForegroundAttempts               = 3;

    explicit DesktopLease(DesktopBackend& desktop) noexcept : _desktop(desktop)
    {
    }

    DesktopLease(const DesktopLease&)            = delete;
    DesktopLease& operator=(const DesktopLease&) = delete;

    // Records what the person has. Call it before anything takes the desktop (before the confirmation appears).
    void Capture()
    {
        _saved    = Read();
        _captured = true;
    }

    [[nodiscard]] const DesktopState& Saved() const noexcept
    {
        return _saved;
    }

    // Gives back what Capture recorded; calling it again finds everything unchanged.
    [[nodiscard]] RestoreReport Restore()
    {
        RestoreReport report{};
        report.saved  = _saved;
        report.atExit = Read();
        if (! _captured)
            return report; // Nothing was recorded, so nothing is restored.

        report.foreground = RestoreForeground(report.atExit);
        report.focus      = RestoreFocus(report.foreground);
        report.cursor     = RestoreCursor();
        return report;
    }

private:
    [[nodiscard]] DesktopState Read() const
    {
        DesktopState state{};
        state.foreground       = _desktop.Foreground();
        state.foregroundThread = state.foreground != nullptr ? _desktop.WindowThread(state.foreground) : 0u;
        state.focus            = state.foregroundThread != 0u ? _desktop.ThreadFocus(state.foregroundThread) : nullptr;
        state.cursorKnown      = _desktop.Cursor(state.cursor);
        return state;
    }

    template <typename Predicate> [[nodiscard]] bool Settles(const Predicate& predicate) const
    {
        for (int attempt = 0; attempt < kSettleAttempts; ++attempt)
        {
            if (predicate())
                return true;
            _desktop.Wait(kSettleStep);
        }
        return predicate();
    }

    [[nodiscard]] Restoration RestoreForeground(const DesktopState& atExit) const
    {
        if (_saved.foreground == nullptr)
            return Restoration::NotRecorded;
        if (! _desktop.IsWindowAlive(_saved.foreground))
            return Restoration::Gone;
        if (atExit.foreground == _saved.foreground)
            return Restoration::Unchanged;
        for (int attempt = 0; attempt < kForegroundAttempts; ++attempt)
        {
            static_cast<void>(_desktop.BringToForeground(_saved.foreground));
            if (Settles([&] { return _desktop.Foreground() == _saved.foreground; }))
                return Restoration::Restored;
            if (! _desktop.IsWindowAlive(_saved.foreground))
                return Restoration::Gone;
        }
        return Restoration::Failed;
    }

    [[nodiscard]] Restoration RestoreFocus(Restoration foreground) const
    {
        if (_saved.focus == nullptr)
            return Restoration::NotRecorded;
        if (! _desktop.IsWindowAlive(_saved.focus))
            return Restoration::Gone;
        if (foreground != Restoration::Unchanged && foreground != Restoration::Restored)
            return Restoration::NotAttempted; // The window that held the focus is not the foreground, so its focus cannot be given back.
        if (_desktop.ThreadFocus(_saved.foregroundThread) == _saved.focus)
            return Restoration::Unchanged;
        static_cast<void>(_desktop.FocusWindow(_saved.focus, _saved.foregroundThread));
        return Settles([&] { return _desktop.ThreadFocus(_saved.foregroundThread) == _saved.focus; }) ? Restoration::Restored : Restoration::Failed;
    }

    [[nodiscard]] Restoration RestoreCursor() const
    {
        if (! _saved.cursorKnown)
            return Restoration::NotRecorded;
        // Read again, after the foreground and focus steps: they may have moved the pointer (a dialog's snap to its default button).
        POINT now{};
        if (! _desktop.Cursor(now))
            return Restoration::Failed;
        if (SamePoint(now, _saved.cursor))
            return Restoration::Unchanged;
        static_cast<void>(_desktop.SetCursor(_saved.cursor));
        POINT verified{};
        return _desktop.Cursor(verified) && SamePoint(verified, _saved.cursor) ? Restoration::Restored : Restoration::Failed;
    }

    DesktopBackend& _desktop;
    DesktopState _saved{};
    bool _captured = false;
};

// The Windows desktop. Reading it has no side effects; BringToForeground and FocusWindow are the only calls that act on it besides
// the pointer. A lease reaches them through Restore; authorized foreground fixtures can reuse the bounded attachment
// in BringToForeground, while their activation blocker keeps noninteractive runs from reaching it.
//
// Windows lets only certain callers change the foreground (the foreground process, a process that received the last input, a
// process the foreground process allowed). A lease runs after its tests took the foreground and gave it away, so it holds a window of
// its own, the anchor, and makes that the foreground first: it attaches this thread's input to the current foreground thread for
// the call (the thread then counts as part of the foreground queue), activates the anchor, and as the foreground process hands the
// foreground to the person's window. The same attachment lets it set the keyboard focus inside a thread it does not own.
class Win32DesktopBackend final : public DesktopBackend
{
public:
    explicit Win32DesktopBackend(HWND anchor = nullptr) noexcept : _anchor(anchor)
    {
    }

    // The window of the calling thread that holds the foreground while the lease hands it back; null for none.
    void SetAnchor(HWND anchor) noexcept
    {
        _anchor = anchor;
    }

    [[nodiscard]] HWND Foreground() override
    {
        return GetForegroundWindow();
    }

    [[nodiscard]] bool IsWindowAlive(HWND window) override
    {
        return window != nullptr && IsWindow(window) != FALSE;
    }

    [[nodiscard]] DWORD WindowThread(HWND window) override
    {
        return IsWindowAlive(window) ? GetWindowThreadProcessId(window, nullptr) : 0u;
    }

    [[nodiscard]] HWND ThreadFocus(DWORD thread) override
    {
        GUITHREADINFO info{sizeof(GUITHREADINFO)};
        return thread != 0u && GetGUIThreadInfo(thread, &info) != FALSE ? info.hwndFocus : nullptr;
    }

    [[nodiscard]] bool Cursor(POINT& position) override
    {
        return GetPhysicalCursorPos(&position) != FALSE;
    }

    bool SetCursor(POINT position) override
    {
        return SetPhysicalCursorPos(position.x, position.y) != FALSE;
    }

    // Makes the anchor the foreground window; false when the system refused.
    bool ActivateAnchor() noexcept
    {
        if (_anchor == nullptr || IsWindow(_anchor) == FALSE)
            return false;
        const DWORD ownThread        = GetCurrentThreadId();
        const HWND foreground        = GetForegroundWindow();
        const DWORD foregroundThread = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0u;
        const bool attached          = foregroundThread != 0u && foregroundThread != ownThread && AttachThreadInput(foregroundThread, ownThread, TRUE) != FALSE;
        const auto detach            = wil::scope_exit([&]() noexcept
        {
            if (attached)
                AttachThreadInput(foregroundThread, ownThread, FALSE);
        });
        ShowWindow(_anchor, SW_SHOWNORMAL);
        BringWindowToTop(_anchor);
        SetActiveWindow(_anchor);
        SetForegroundWindow(_anchor);
        SetFocus(_anchor);
        // The change is asynchronous: give it up to half a second, as the lease gives every foreground change it reads back.
        for (int attempt = 0; attempt < 25 && GetForegroundWindow() != _anchor; ++attempt)
            Sleep(20);
        return GetForegroundWindow() == _anchor;
    }

    bool BringToForeground(HWND window) override
    {
        if (! IsWindowAlive(window))
            return false;
        if (_anchor != nullptr && GetForegroundWindow() != _anchor)
            static_cast<void>(ActivateAnchor());
        const DWORD ownThread        = GetCurrentThreadId();
        const HWND foreground        = GetForegroundWindow();
        const DWORD foregroundThread = foreground != nullptr ? GetWindowThreadProcessId(foreground, nullptr) : 0u;
        const bool attached          = foregroundThread != 0u && foregroundThread != ownThread && AttachThreadInput(ownThread, foregroundThread, TRUE) != FALSE;
        const auto detach            = wil::scope_exit([&]() noexcept
        {
            if (attached)
                AttachThreadInput(ownThread, foregroundThread, FALSE);
        });
        if (IsIconic(window) != FALSE)
            ShowWindow(window, SW_RESTORE);
        return SetForegroundWindow(window) != FALSE;
    }

    bool FocusWindow(HWND window, DWORD thread) override
    {
        if (! IsWindowAlive(window) || thread == 0u)
            return false;
        const DWORD ownThread = GetCurrentThreadId();
        const bool attached   = thread != ownThread && AttachThreadInput(ownThread, thread, TRUE) != FALSE;
        const auto detach     = wil::scope_exit([&]() noexcept
        {
            if (attached)
                AttachThreadInput(ownThread, thread, FALSE);
        });
        // SetFocus returns the window that had the focus, or null for none as well as for an error: only the error code tells.
        SetLastError(ERROR_SUCCESS);
        SetFocus(window);
        return GetLastError() == ERROR_SUCCESS;
    }

    void Wait(std::chrono::milliseconds duration) override
    {
        Sleep(static_cast<DWORD>(duration.count()));
    }

    [[nodiscard]] std::string Describe(HWND window) override
    {
        if (! IsWindowAlive(window))
            return "no window";
        wchar_t className[128]{};
        static_cast<void>(GetClassNameW(window, className, static_cast<int>(std::size(className))));
        DWORD processId = 0u;
        GetWindowThreadProcessId(window, &processId);
        std::wstring image;
        if (const wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId)); process)
        {
            wchar_t path[MAX_PATH]{};
            DWORD length = static_cast<DWORD>(std::size(path));
            if (QueryFullProcessImageNameW(process.get(), 0, path, &length) != FALSE)
                image = std::filesystem::path(std::wstring_view(path, length)).filename().wstring();
        }
        std::string text = std::format("class '{}' of ", Narrow(className));
        text += image.empty() ? std::format("process {}", processId) : std::format("{} (process {})", Narrow(image), processId);
        return text;
    }

private:
    // Message text only: ASCII stays, anything else becomes '?'.
    [[nodiscard]] static std::string Narrow(std::wstring_view text)
    {
        std::string narrow;
        for (const wchar_t ch : text)
            narrow.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
        return narrow;
    }

    HWND _anchor;
};

// Whether this session has an interactive desktop that a run may take over, and if not, why. A run must refuse when there is none:
// a service, a scheduled task or a remote shell has no desktop to take, a locked screen or a disconnected session has an input
// desktop the process cannot reach, and a screen saver means nobody is working at it.
struct Availability
{
    bool available = false;
    std::string reason; // Empty when available.
};

class DesktopProbe
{
public:
    DesktopProbe(const DesktopProbe&)            = delete;
    DesktopProbe& operator=(const DesktopProbe&) = delete;
    DesktopProbe(DesktopProbe&&)                 = delete;
    DesktopProbe& operator=(DesktopProbe&&)      = delete;
    virtual ~DesktopProbe()                      = default;

    [[nodiscard]] virtual bool InteractiveWindowStation()                        = 0; // The window station is the visible one (WinSta0).
    [[nodiscard]] virtual DWORD SessionId()                                      = 0;
    [[nodiscard]] virtual bool SessionActive(std::string& state)                 = 0; // false: say what the connection state is.
    [[nodiscard]] virtual bool InputDesktopName(std::string& name, DWORD& error) = 0; // false: the input desktop cannot be opened.
    [[nodiscard]] virtual std::string ThreadDesktopName()                        = 0;
    [[nodiscard]] virtual bool ScreenSaverRunning()                              = 0;
    [[nodiscard]] virtual bool CursorReadable()                                  = 0;

protected:
    DesktopProbe() = default;
};

[[nodiscard]] inline Availability CheckInteractiveDesktop(DesktopProbe& probe)
{
    if (! probe.InteractiveWindowStation())
        return {false, "this process has no interactive window station: it runs as a service, a scheduled task or a remote shell"};
    if (probe.SessionId() == 0u)
        return {false, "this is session 0, the session of services, which has no desktop a person works at"};
    std::string state;
    if (! probe.SessionActive(state))
        return {false, std::format("the session is not active ({}): it is disconnected or still starting", state)};
    std::string input;
    DWORD error = ERROR_SUCCESS;
    if (! probe.InputDesktopName(input, error))
        return {false, std::format("the input desktop cannot be opened (error {}): the session is locked, or a secure desktop has the input", error)};
    const std::string own = probe.ThreadDesktopName();
    if (_stricmp(input.c_str(), own.c_str()) != 0)
        return {false, std::format("the input desktop is '{}', not this process's desktop '{}'", input, own)};
    if (probe.ScreenSaverRunning())
        return {false, "a screen saver is running, so nobody is working at this desktop"};
    if (! probe.CursorReadable())
        return {false, "the pointer position cannot be read, so it could not be restored"};
    return {true, {}};
}

// The probe of the Windows session this process runs in.
class Win32DesktopProbe final : public DesktopProbe
{
public:
    Win32DesktopProbe() = default;

    [[nodiscard]] bool InteractiveWindowStation() override
    {
        USEROBJECTFLAGS flags{};
        DWORD needed = 0u;
        return GetUserObjectInformationW(GetProcessWindowStation(), UOI_FLAGS, &flags, sizeof(flags), &needed) != FALSE && (flags.dwFlags & WSF_VISIBLE) != 0u;
    }

    [[nodiscard]] DWORD SessionId() override
    {
        DWORD session = 0u;
        return ProcessIdToSessionId(GetCurrentProcessId(), &session) != FALSE ? session : 0u;
    }

    [[nodiscard]] bool SessionActive(std::string& state) override
    {
        // WTS_CONNECTSTATE_CLASS values as text, so a message names the state; a failed query cannot say, and the input desktop
        // check still catches a disconnected session.
        LPWSTR buffer = nullptr;
        DWORD bytes   = 0u;
        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSConnectState, &buffer, &bytes) == FALSE || buffer == nullptr)
            return true;
        const auto freeBuffer   = wil::scope_exit([&]() noexcept { WTSFreeMemory(buffer); });
        const auto connectState = *reinterpret_cast<const WTS_CONNECTSTATE_CLASS*>(buffer);
        switch (connectState)
        {
            case WTSActive: return true;
            case WTSConnected: state = "connected"; break;
            case WTSConnectQuery: state = "connecting"; break;
            case WTSShadow: state = "shadowing"; break;
            case WTSDisconnected: state = "disconnected"; break;
            case WTSIdle: state = "idle"; break;
            case WTSListen: state = "listening"; break;
            case WTSReset: state = "resetting"; break;
            case WTSDown: state = "down"; break;
            case WTSInit: state = "initializing"; break;
            default: state = std::format("state {}", static_cast<int>(connectState)); break;
        }
        return false;
    }

    [[nodiscard]] bool InputDesktopName(std::string& name, DWORD& error) override
    {
        const wil::unique_hdesk input(OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS));
        if (! input)
        {
            error = GetLastError();
            return false;
        }
        name = DesktopName(input.get());
        return true;
    }

    [[nodiscard]] std::string ThreadDesktopName() override
    {
        return DesktopName(GetThreadDesktop(GetCurrentThreadId()));
    }

    [[nodiscard]] bool ScreenSaverRunning() override
    {
        BOOL running = FALSE;
        return SystemParametersInfoW(SPI_GETSCREENSAVERRUNNING, 0, &running, 0) != FALSE && running != FALSE;
    }

    [[nodiscard]] bool CursorReadable() override
    {
        POINT position{};
        return GetPhysicalCursorPos(&position) != FALSE;
    }

private:
    [[nodiscard]] static std::string DesktopName(HDESK desktop)
    {
        wchar_t name[256]{};
        DWORD needed = 0u;
        if (desktop == nullptr || GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &needed) == FALSE)
            return "unknown";
        std::string narrow;
        for (const wchar_t* ch = name; *ch != L'\0'; ++ch)
            narrow.push_back(*ch < 0x80 ? static_cast<char>(*ch) : '?');
        return narrow;
    }
};
} // namespace DxUi::TestSupport
