#pragma once

#include "Support.Tests.DesktopLease.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DxUi::TestSupport
{
// Exit codes of DxUi.InteractiveLease.exe. Tools/InteractiveRun.psm1 reads them back, and Tools/tests/Test-InteractiveMode.ps1
// requires the two tables to be the same.
namespace LeaseExit
{
inline constexpr int kPassed       = 0;  // Every child exited 0 and the desktop is as the person had it.
inline constexpr int kChildFailed  = 1;  // A child exited nonzero (its code is in the result).
inline constexpr int kUsage        = 2;  // The command line is malformed.
inline constexpr int kNoDesktop    = 20; // There is no interactive desktop to take; nothing was touched.
inline constexpr int kDeclined     = 21; // The person chose Cancel, or nobody chose within the confirmation's time.
inline constexpr int kBusy         = 22; // Another interactive run holds the session's lease.
inline constexpr int kNoWarning    = 23; // The warning could not be shown, so the desktop was not taken.
inline constexpr int kLaunchFailed = 24; // A child could not be started.
inline constexpr int kNotRestored  = 25; // The children passed, but the desktop could not be given back.
inline constexpr int kInterrupted  = 26; // The run was stopped (Ctrl+C or the console closing).
} // namespace LeaseExit

// UTF-8 text for the plan and result files: PowerShell writes and reads them, Windows takes UTF-16.
[[nodiscard]] inline std::string ToUtf8(std::wstring_view text)
{
    if (text.empty())
        return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>((std::max)(size, 0)), '\0');
    if (size > 0)
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

[[nodiscard]] inline std::wstring FromUtf8(std::string_view text)
{
    if (text.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>((std::max)(size, 0)), L'\0');
    if (size > 0)
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}

// One process the lease runs: DxUi.ControlTests.exe for one suite, with its output going to `logPath`.
struct ChildRun
{
    std::wstring name; // What the result calls it: the suite.
    std::wstring logPath;
    std::wstring commandLine; // Executable path (quoted when it has spaces) and arguments, as CreateProcess takes it.
};

// The plan file lists the children, one per line: name, log path and command line, separated by tabs, in UTF-8. Blank lines and lines
// starting with '#' are ignored.
[[nodiscard]] inline bool ParsePlan(std::string_view text, std::vector<ChildRun>& runs, std::string& error)
{
    runs.clear();
    size_t lineNumber = 0u;
    while (! text.empty())
    {
        const size_t end      = text.find('\n');
        std::string_view line = text.substr(0u, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1u);
        ++lineNumber;
        if (! line.empty() && line.back() == '\r')
            line.remove_suffix(1u);
        if (line.empty() || line.front() == '#')
            continue;
        const size_t firstTab  = line.find('\t');
        const size_t secondTab = firstTab == std::string_view::npos ? std::string_view::npos : line.find('\t', firstTab + 1u);
        if (firstTab == std::string_view::npos || secondTab == std::string_view::npos || firstTab == 0u || secondTab == firstTab + 1u ||
            secondTab + 1u >= line.size())
        {
            error = std::format("plan line {} needs a name, a log path and a command line separated by tabs", lineNumber);
            runs.clear();
            return false;
        }
        runs.push_back(
            {FromUtf8(line.substr(0u, firstTab)), FromUtf8(line.substr(firstTab + 1u, secondTab - firstTab - 1u)), FromUtf8(line.substr(secondTab + 1u))});
    }
    if (runs.empty())
    {
        error = "the plan lists no child";
        return false;
    }
    return true;
}

struct ChildResult
{
    bool launched    = false;
    DWORD exitCode   = 0u;
    double seconds   = 0.0;
    bool timedOut    = false; // The lease's bound ended it; the exit code is the watchdog's, 124.
    bool interrupted = false; // The person stopped the run and the child was ended.
    std::string error;        // Why it could not be started.
};

struct ChildOutcome
{
    std::string name;
    ChildResult result;
};

// What the person is asked, and how long the run takes by the clock, for the confirmation and the warning.
struct LeaseRequest
{
    std::vector<ChildRun> children;
    std::wstring label; // What the run is called to the person: "Menu, NativeTextInput (x64 Debug)".
    unsigned estimateSeconds     = 0u;
    unsigned confirmSeconds      = 120u; // How long the confirmation waits before it is cancelled; 0 waits for as long as it takes.
    unsigned childTimeoutSeconds = 900u; // The most one child may take before the lease ends it; 0 sets no bound.
};

enum class Confirmation
{
    None, // The person was never asked.
    Started,
    Declined,    // Cancel, the dialog closed, or Esc.
    TimedOut,    // Nobody answered: the answer is Cancel.
    Interrupted, // The run was stopped while the person was asked.
    Unavailable, // The confirmation could not be shown.
};

[[nodiscard]] constexpr std::string_view ConfirmationName(Confirmation confirmation) noexcept
{
    switch (confirmation)
    {
        case Confirmation::None: return "none";
        case Confirmation::Started: return "started";
        case Confirmation::Declined: return "declined";
        case Confirmation::TimedOut: return "timedout";
        case Confirmation::Interrupted: return "interrupted";
        case Confirmation::Unavailable: return "unavailable";
    }
    return "none";
}

struct LeaseOutcome
{
    int exitCode = LeaseExit::kPassed;
    std::string state; // completed, refused, busy, declined, timedout, interrupted, unwarned, launchfailed.
    std::string reason;
    Confirmation confirmation = Confirmation::None;
    std::string savedForeground; // The window the person had, for messages.
    std::optional<RestoreReport> restoration;
    std::vector<ChildOutcome> children;
};

// The session the lease works in. The Windows services are in Tests/InteractiveLease; a test replaces every one of them, so that each
// path through RunLease is exercised without a desktop.
class LeaseServices
{
public:
    LeaseServices(const LeaseServices&)            = delete;
    LeaseServices& operator=(const LeaseServices&) = delete;
    LeaseServices(LeaseServices&&)                 = delete;
    LeaseServices& operator=(LeaseServices&&)      = delete;
    virtual ~LeaseServices()                       = default;

    [[nodiscard]] virtual Availability CheckDesktop()                       = 0;
    [[nodiscard]] virtual bool TryAcquire()                                 = 0; // The session-wide lease; false when another run holds it.
    virtual void Release()                                                  = 0;
    virtual void KeepAwake(bool keep)                                       = 0; // No idle sleep, display-off or screen saver while true.
    [[nodiscard]] virtual DesktopBackend& Desktop()                         = 0;
    [[nodiscard]] virtual Confirmation Confirm(const LeaseRequest& request) = 0;
    // The warning that stays on screen during the run, which holds the foreground the children are granted. False, saying why, when
    // it could not be shown or could not take the foreground.
    [[nodiscard]] virtual bool ShowWarning(const LeaseRequest& request, std::string& reason) = 0;
    virtual void HideWarning()                                                               = 0;
    [[nodiscard]] virtual ChildResult RunChild(const ChildRun& run, unsigned timeoutSeconds) = 0;
    [[nodiscard]] virtual bool Interrupted()                                                 = 0;
    virtual void Log(std::string_view line)                                                  = 0;

protected:
    LeaseServices() = default;
};

namespace Detail
{
// The part of a lease in which the person's desktop is at stake: from the moment it is recorded until it is restored. The
// restoration is the last thing every path through here does, whatever ends it (a person's Cancel, a refused warning, a child that
// cannot start or fails, a stopped run), and only the warning's removal follows it.
inline void RunWithDesktop(LeaseServices& services, const LeaseRequest& request, LeaseOutcome& outcome)
{
    services.KeepAwake(true);
    const auto sleepAgain = wil::scope_exit([&] { services.KeepAwake(false); });

    DesktopLease lease(services.Desktop());
    lease.Capture();
    outcome.savedForeground = services.Desktop().Describe(lease.Saved().foreground);
    if (! outcome.savedForeground.empty())
        services.Log(std::format("foreground window to restore: {}", outcome.savedForeground));
    const auto restore = wil::scope_exit([&]
    {
        // The warning goes last: it is the window that holds the foreground while the lease hands it back to the person's.
        outcome.restoration = lease.Restore();
        services.Log(outcome.restoration->Line());
        services.HideWarning();
    });

    outcome.confirmation = services.Confirm(request);
    switch (outcome.confirmation)
    {
        case Confirmation::Started: break;
        case Confirmation::Declined:
            outcome.state    = "declined";
            outcome.exitCode = LeaseExit::kDeclined;
            outcome.reason   = "the person chose Cancel";
            return;
        case Confirmation::TimedOut:
            outcome.state    = "timedout";
            outcome.exitCode = LeaseExit::kDeclined;
            outcome.reason   = "nobody chose within the confirmation's time, so the run was cancelled";
            return;
        case Confirmation::Interrupted:
            outcome.state    = "interrupted";
            outcome.exitCode = LeaseExit::kInterrupted;
            outcome.reason   = "the run was stopped while the person was asked";
            return;
        case Confirmation::None:
        case Confirmation::Unavailable:
            outcome.state    = "unwarned";
            outcome.exitCode = LeaseExit::kNoWarning;
            outcome.reason   = "the confirmation could not be shown, so the desktop was not taken";
            return;
    }
    std::string warningFailure;
    if (! services.ShowWarning(request, warningFailure))
    {
        outcome.state    = "unwarned";
        outcome.exitCode = LeaseExit::kNoWarning;
        outcome.reason   = std::format("the warning that stays on screen during the run could not be shown or could not take the foreground, so the desktop "
                                       "was not taken: {}",
                                       warningFailure);
        return;
    }
    for (const ChildRun& run : request.children)
    {
        if (services.Interrupted())
        {
            outcome.state    = "interrupted";
            outcome.exitCode = LeaseExit::kInterrupted;
            outcome.reason   = "the run was stopped before every child ran";
            return;
        }
        const ChildResult result = services.RunChild(run, request.childTimeoutSeconds);
        outcome.children.push_back({ToUtf8(run.name), result});
        if (! result.launched)
        {
            outcome.state    = "launchfailed";
            outcome.exitCode = LeaseExit::kLaunchFailed;
            outcome.reason   = result.error;
            return;
        }
        if (result.interrupted)
        {
            outcome.state    = "interrupted";
            outcome.exitCode = LeaseExit::kInterrupted;
            outcome.reason   = "the run was stopped while a child ran";
            return;
        }
    }
    outcome.state = "completed";
}
} // namespace Detail

// Runs the children one after another under one lease: checks that a desktop is there to take, takes the session's lease, records
// the desktop, asks the person, shows the warning, runs each child and puts the desktop back, whatever ended the run. The outcome's
// exit code is 0 only when every child exited 0 and the desktop was restored.
[[nodiscard]] inline LeaseOutcome RunLease(LeaseServices& services, const LeaseRequest& request)
{
    LeaseOutcome outcome{};
    const Availability availability = services.CheckDesktop();
    if (! availability.available)
    {
        outcome.state    = "refused";
        outcome.exitCode = LeaseExit::kNoDesktop;
        outcome.reason   = availability.reason;
        return outcome;
    }
    if (! services.TryAcquire())
    {
        outcome.state    = "busy";
        outcome.exitCode = LeaseExit::kBusy;
        outcome.reason   = "another interactive run holds this session's lease";
        return outcome;
    }
    {
        const auto release = wil::scope_exit([&] { services.Release(); });
        Detail::RunWithDesktop(services, request, outcome);
    }
    if (outcome.state == "completed")
    {
        const bool childFailed =
            std::ranges::any_of(outcome.children, [](const ChildOutcome& child) { return child.result.exitCode != 0u || child.result.timedOut; });
        outcome.exitCode = childFailed ? LeaseExit::kChildFailed : LeaseExit::kPassed;
        if (! childFailed && outcome.restoration.has_value() && ! outcome.restoration->Succeeded())
        {
            outcome.exitCode = LeaseExit::kNotRestored;
            outcome.reason   = "the desktop could not be given back";
        }
    }
    return outcome;
}

[[nodiscard]] inline std::string FormatPoint(const DesktopState& state)
{
    return state.cursorKnown ? std::format("{},{}", state.cursor.x, state.cursor.y) : std::string("unknown");
}

// The result file DxUi.InteractiveLease.exe writes and Tools/InteractiveRun.psm1 reads: `key: value` lines, one per fact, `child:`
// once per child, in UTF-8. A reason is one line.
[[nodiscard]] inline std::string FormatLeaseResult(const LeaseOutcome& outcome)
{
    const auto oneLine = [](std::string text)
    {
        for (char& ch : text)
        {
            if (ch == '\r' || ch == '\n' || ch == '\t')
                ch = ' ';
        }
        return text;
    };
    std::string text = std::format("state: {}\nexit: {}\n", outcome.state, outcome.exitCode);
    if (! outcome.reason.empty())
        text += std::format("reason: {}\n", oneLine(outcome.reason));
    text += std::format("confirmation: {}\n", ConfirmationName(outcome.confirmation));
    if (! outcome.savedForeground.empty())
        text += std::format("foreground.saved: {}\n", oneLine(outcome.savedForeground));
    if (outcome.restoration.has_value())
    {
        const RestoreReport& report = *outcome.restoration;
        text += std::format("restoration.foreground: {}\nrestoration.focus: {}\nrestoration.cursor: {}\n",
                            RestorationName(report.foreground),
                            RestorationName(report.focus),
                            RestorationName(report.cursor));
        text += std::format("cursor.saved: {}\ncursor.atExit: {}\n", FormatPoint(report.saved), FormatPoint(report.atExit));
        text += std::format("restoration.runMovedSomething: {}\n", report.RunMovedSomething() ? 1 : 0);
    }
    for (const ChildOutcome& child : outcome.children)
    {
        text += std::format("child: {} launched={} exit={} seconds={:.1f} timedout={} interrupted={}\n",
                            oneLine(child.name),
                            child.result.launched ? 1 : 0,
                            child.result.exitCode,
                            child.result.seconds,
                            child.result.timedOut ? 1 : 0,
                            child.result.interrupted ? 1 : 0);
    }
    return text;
}

// "about 2 minutes": how long a run takes, roughly, for the person asked to let it start.
[[nodiscard]] inline std::string FormatApproximateDuration(unsigned seconds)
{
    if (seconds < 45u)
        return "less than a minute";
    const unsigned minutes = (seconds + 30u) / 60u;
    return minutes <= 1u ? std::string("about a minute") : std::format("about {} minutes", minutes);
}

// How long a confirmation has left. When it runs out the answer is Cancel: nobody was there to be asked.
class ConfirmationCountdown final
{
public:
    explicit ConfirmationCountdown(unsigned totalSeconds) noexcept : _totalMilliseconds(static_cast<std::uint64_t>(totalSeconds) * 1000u)
    {
    }

    [[nodiscard]] bool Unlimited() const noexcept
    {
        return _totalMilliseconds == 0u;
    }

    [[nodiscard]] bool Expired(std::uint64_t elapsedMilliseconds) const noexcept
    {
        return _totalMilliseconds != 0u && elapsedMilliseconds >= _totalMilliseconds;
    }

    // Whole seconds left, rounded up, so the last second is shown as 1 and 0 means it ran out.
    [[nodiscard]] unsigned SecondsLeft(std::uint64_t elapsedMilliseconds) const noexcept
    {
        if (_totalMilliseconds == 0u || elapsedMilliseconds >= _totalMilliseconds)
            return 0u;
        return static_cast<unsigned>((_totalMilliseconds - elapsedMilliseconds + 999u) / 1000u);
    }

private:
    std::uint64_t _totalMilliseconds;
};
} // namespace DxUi::TestSupport
