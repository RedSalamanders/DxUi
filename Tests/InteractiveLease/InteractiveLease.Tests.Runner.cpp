// DxUi.InteractiveLease.exe: the interactive desktop lease behind `test.ps1 -Interactive`.
//
// The control suites that need real focus (Menu, NativeTextInput and the two menu resource fixtures) take the foreground, the keyboard
// focus and the pointer. This program runs them for a person who agreed to it, or for an exactly verified GitHub-hosted Windows job:
// it refuses when there is no desktop to take, asks a local person first (the default answer is Cancel), shows a warning for the length
// of the run, runs each suite as a child of its own, and puts the
// person's foreground window, keyboard focus and pointer position back whatever ended the run: a failed suite, the runner's watchdog
// (exit code 124), a hung child, Ctrl+C. The logic is in Tests/Support/Support.Tests.InteractiveLease.h and DesktopLease.h, which the control
// tests exercise with a desktop of their own making; this file holds the Windows services and the command line.
//
//   DxUi.InteractiveLease.exe --check [--result=<file>]
//   DxUi.InteractiveLease.exe --run --plan=<file> --result=<file> [--label=<text>] [--estimate=<seconds>]
//                             [--confirm-timeout=<seconds>] [--child-timeout=<seconds>]
//   DxUi.InteractiveLease.exe --self-test
//
// Exit codes are in DxUi::TestSupport::LeaseExit. --self-test takes nothing of the person's desktop: it opens the dialog and the
// warning on a private desktop that no one sees, and runs children without a foreground grant. Tools/tests/Test-InteractiveLease.ps1
// runs it.

#include "../Support/Support.Tests.FailureReports.h"
#include "../Support/Support.Tests.InteractiveLease.h"
#include "../Support/Support.Tests.TestWatchdog.h"
#include "InteractiveLease.Tests.ChildProcess.h"
#include "InteractiveLease.Tests.Confirmation.h"
#include "InteractiveLease.Tests.WarningBanner.h"

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
namespace TS = DxUi::TestSupport;
namespace IL = DxUi::InteractiveLease;

constexpr PCWSTR kLeaseMutexName = L"Local\\DxUi.InteractiveTestRun.v1";

std::atomic<bool> g_interrupted{false};
wil::unique_event_nothrow g_interruptEvent; // Set when the person stops the run.
wil::unique_event_nothrow g_finished;       // Set when the lease has cleaned up, which a closing console waits for.

BOOL WINAPI ConsoleHandler(DWORD type) noexcept
{
    switch (type)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_interrupted.store(true);
            if (g_interruptEvent)
                g_interruptEvent.SetEvent();
            // The process ends when a close handler returns, so the lease gets the few seconds Windows allows to restore the desktop.
            if (type == CTRL_CLOSE_EVENT && g_finished)
                static_cast<void>(g_finished.wait(4500u));
            return TRUE;
        default: return FALSE;
    }
}

void Print(std::string_view line) noexcept
{
    std::printf("[LEASE] %.*s\n", static_cast<int>(line.size()), line.data());
    std::fflush(stdout);
}

// The Windows services of one lease.
class Win32LeaseServices final : public TS::LeaseServices
{
public:
    explicit Win32LeaseServices(PCWSTR mutexName, bool hosted = false) noexcept : _mutexName(mutexName), _hosted(hosted)
    {
    }

    [[nodiscard]] TS::Availability CheckDesktop() override
    {
        return TS::CheckInteractiveDesktop(_probe);
    }

    [[nodiscard]] bool TryAcquire() override
    {
        _mutex.reset(CreateMutexW(nullptr, FALSE, _mutexName));
        if (! _mutex)
            return false;
        const DWORD wait = WaitForSingleObject(_mutex.get(), 0u);
        if (wait == WAIT_ABANDONED)
            Log("the previous interactive run ended without releasing the session's lease; taking it over");
        _owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        return _owned;
    }

    void Release() override
    {
        if (_owned && _mutex)
            ReleaseMutex(_mutex.get());
        _owned = false;
        _mutex.reset();
    }

    void KeepAwake(bool keep) override
    {
        // No idle sleep, display-off or screen saver, which would lock the desktop in the middle of the run; the state belongs to
        // this thread and ends with it.
        SetThreadExecutionState(keep ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS);
    }

    [[nodiscard]] TS::DesktopBackend& Desktop() override
    {
        return _desktop;
    }

    [[nodiscard]] TS::Confirmation Confirm(const TS::LeaseRequest& request) override
    {
        if (_hosted)
        {
            Log("confirmation: hosted runner authorization");
            return TS::Confirmation::Started;
        }
        Log(std::format("{}: {}; {}",
                        TS::ToUtf8(request.label),
                        TS::FormatApproximateDuration(request.estimateSeconds),
                        request.confirmSeconds == 0u ? std::string("the confirmation waits for an answer")
                                                     : std::format("the confirmation cancels itself in {} s", request.confirmSeconds)));
        const TS::Confirmation answer = IL::Confirm(request, g_interrupted);
        Log(std::format("confirmation: {}", TS::ConfirmationName(answer)));
        return answer;
    }

    [[nodiscard]] bool ShowWarning(const TS::LeaseRequest& request, std::string& reason) override
    {
        if (! _banner.Show(request.label, request.estimateSeconds))
        {
            reason = "the warning window could not be created";
            return false;
        }
        _desktop.SetAnchor(_banner.Window());
        if (! _desktop.ActivateAnchor())
        {
            reason = "the warning window could not take the foreground (the system refused)";
            return false;
        }
        return true;
    }

    void HideWarning() override
    {
        _banner.Hide();
        _desktop.SetAnchor(nullptr);
    }

    [[nodiscard]] TS::ChildResult RunChild(const TS::ChildRun& run, unsigned timeoutSeconds) override
    {
        Log(std::format("{}: started", TS::ToUtf8(run.name)));
        // The child can be granted the foreground only by a process that holds it, and the child before it may have left the person's
        // window in front: hold it before the launch, and again if the grant is refused.
        const auto holdForeground = [this]
        {
            if (_banner.Window() != nullptr && GetForegroundWindow() != _banner.Window())
                static_cast<void>(_desktop.ActivateAnchor());
        };
        holdForeground();
        const TS::ChildResult result = IL::RunChildProcess(run, timeoutSeconds, g_interruptEvent.get(), true, holdForeground);
        if (! result.launched)
            Log(std::format("{}: not started: {}", TS::ToUtf8(run.name), result.error));
        else if (result.timedOut)
            Log(std::format("{}: ended after {:.1f} s, the lease's bound: exit code {}", TS::ToUtf8(run.name), result.seconds, result.exitCode));
        else if (result.interrupted)
            Log(std::format("{}: stopped after {:.1f} s", TS::ToUtf8(run.name), result.seconds));
        else
            Log(std::format("{}: exit code {} in {:.1f} s", TS::ToUtf8(run.name), result.exitCode, result.seconds));
        return result;
    }

    [[nodiscard]] bool Interrupted() override
    {
        return g_interrupted.load();
    }

    void Log(std::string_view line) override
    {
        Print(line);
    }

private:
    PCWSTR _mutexName;
    bool _hosted = false;
    wil::unique_mutex_nothrow _mutex;
    bool _owned = false;
    TS::Win32DesktopProbe _probe;
    TS::Win32DesktopBackend _desktop;
    IL::WarningBanner _banner;
};

[[nodiscard]] std::optional<std::wstring_view> ValueOf(std::wstring_view argument, std::wstring_view name)
{
    if (argument.size() <= name.size() + 1u || argument.substr(0u, name.size()) != name || argument[name.size()] != L'=')
        return std::nullopt;
    return argument.substr(name.size() + 1u);
}

[[nodiscard]] bool ParseSeconds(std::wstring_view digits, unsigned& seconds) noexcept
{
    if (digits.empty() || digits.size() > 6u)
        return false;
    unsigned value = 0u;
    for (const wchar_t ch : digits)
    {
        if (ch < L'0' || ch > L'9')
            return false;
        value = value * 10u + static_cast<unsigned>(ch - L'0');
    }
    seconds = value;
    return true;
}

[[nodiscard]] bool ReadFileUtf8(const std::filesystem::path& path, std::string& text)
{
    std::ifstream file(path, std::ios::binary);
    if (! file)
        return false;
    text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (text.size() >= 3u && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
        text.erase(0u, 3u); // PowerShell writes a byte-order mark unless told not to.
    return true;
}

[[nodiscard]] bool WriteFileUtf8(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (! file)
        return false;
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return file.good();
}

void WriteResult(const std::filesystem::path& path, const TS::LeaseOutcome& outcome)
{
    if (path.empty())
        return;
    if (! WriteFileUtf8(path, TS::FormatLeaseResult(outcome)))
        Print(std::format("the result file could not be written: {}", path.string()));
}

int Usage(std::string_view problem)
{
    std::fprintf(stderr,
                 "%.*s\n"
                 "usage: DxUi.InteractiveLease.exe --check [--result=<file>]\n"
                 "       DxUi.InteractiveLease.exe --run --plan=<file> --result=<file> [--label=<text>] [--estimate=<seconds>]\n"
                 "       DxUi.InteractiveLease.exe --run-hosted --plan=<file> --result=<file> [--label=<text>] [--estimate=<seconds>]\n"
                 "                                 [--confirm-timeout=<seconds>] [--child-timeout=<seconds>]\n"
                 "       DxUi.InteractiveLease.exe --self-test\n",
                 static_cast<int>(problem.size()),
                 problem.data());
    return TS::LeaseExit::kUsage;
}

int Check(const std::filesystem::path& resultPath)
{
    TS::Win32DesktopProbe probe;
    const TS::Availability availability = TS::CheckInteractiveDesktop(probe);
    TS::LeaseOutcome outcome{};
    outcome.state    = availability.available ? "checked" : "refused";
    outcome.exitCode = availability.available ? TS::LeaseExit::kPassed : TS::LeaseExit::kNoDesktop;
    outcome.reason   = availability.reason;
    Print(availability.available ? std::string("an interactive desktop is available") : std::format("no interactive desktop: {}", availability.reason));
    WriteResult(resultPath, outcome);
    return outcome.exitCode;
}

struct RunOptions
{
    std::filesystem::path plan;
    std::filesystem::path result;
    std::wstring label;
    unsigned estimateSeconds     = 0u;
    unsigned confirmSeconds      = 120u;
    unsigned childTimeoutSeconds = 900u;
};

int Run(const RunOptions& options, bool hosted)
{
    std::string planText;
    if (! ReadFileUtf8(options.plan, planText))
        return Usage(std::format("the plan file cannot be read: {}", options.plan.string()));
    TS::LeaseRequest request;
    std::string error;
    if (! TS::ParsePlan(planText, request.children, error))
        return Usage(error);
    request.label = options.label;
    if (request.label.empty())
    {
        for (const TS::ChildRun& child : request.children)
            request.label += (request.label.empty() ? L"" : L", ") + child.name;
    }
    request.estimateSeconds     = options.estimateSeconds;
    request.confirmSeconds      = options.confirmSeconds;
    request.childTimeoutSeconds = options.childTimeoutSeconds;

    Win32LeaseServices services(kLeaseMutexName, hosted);
    const TS::LeaseOutcome outcome = TS::RunLease(services, request);
    WriteResult(options.result, outcome);
    Print(std::format("{}{}{}", outcome.state, outcome.reason.empty() ? "" : ": ", outcome.reason));
    return outcome.exitCode;
}

// ---- The self-test ----------------------------------------------------------------------------------------------------------
//
// It proves the Windows services without a person's desktop: the children, the dialog and the warning. The dialog and the warning open
// on a private desktop (CreateDesktop) that is never made the input desktop, so no one sees them and the foreground is not touched.
// Children run without a foreground grant. The session's lease is taken under a name of the test's own.

class SelfTest final
{
public:
    [[nodiscard]] int Run()
    {
        if (! SetUpDirectory())
            return TS::LeaseExit::kUsage;
        ChildProcesses();
        SessionLease();
        PrivateDesktop();
        std::error_code ignored;
        std::filesystem::remove_all(_directory, ignored); // The directory this test made: a fresh name under the temporary directory.
        std::printf("%s: %d of %d passed\n", _failures == 0 ? "PASS" : "FAIL", _checks - _failures, _checks);
        return _failures == 0 ? 0 : 1;
    }

private:
    void Check(bool condition, const char* what)
    {
        ++_checks;
        if (! condition)
            ++_failures;
        std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
        std::fflush(stdout);
    }

    [[nodiscard]] bool SetUpDirectory()
    {
        std::error_code error;
        _directory = std::filesystem::temp_directory_path(error) / std::format(L"DxUi.InteractiveLease.SelfTest.{}", GetCurrentProcessId());
        if (error)
            return false;
        std::filesystem::remove_all(_directory, error);
        return std::filesystem::create_directory(_directory, error) && ! error;
    }

    [[nodiscard]] std::wstring SelfPath() const
    {
        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
        return std::wstring(path, length);
    }

    [[nodiscard]] TS::ChildRun ChildOf(const wchar_t* name, const std::wstring& mode) const
    {
        return {name, (_directory / (std::wstring(name) + L".log")).wstring(), std::format(L"\"{}\" --self-test-child={}", SelfPath(), mode)};
    }

    [[nodiscard]] static std::string ReadText(const std::wstring& path)
    {
        std::string text;
        return ReadFileUtf8(path, text) ? text : std::string{};
    }

    void ChildProcesses()
    {
        wil::unique_event_nothrow never(CreateEventW(nullptr, TRUE, FALSE, nullptr));
        {
            const TS::ChildRun run       = ChildOf(L"exits", L"exit:7");
            const TS::ChildResult result = IL::RunChildProcess(run, 60u, never.get(), false);
            const std::string log        = ReadText(run.logPath);
            Check(result.launched && result.error.empty(), "a child is started");
            Check(result.exitCode == 7u && ! result.timedOut && ! result.interrupted, "its exit code is reported as the child's own");
            Check(log.find("child output") != std::string::npos && log.find("child error") != std::string::npos, "its standard output and error go to its log");
        }
        {
            const TS::ChildRun run       = ChildOf(L"hangs", L"hang");
            const TS::ChildResult result = IL::RunChildProcess(run, 1u, never.get(), false);
            Check(result.launched && result.timedOut && result.exitCode == TS::kTestTimeoutExitCode,
                  "a child that outlives the lease's bound is ended with the watchdog's exit code, 124");
            Check(result.seconds >= 0.9 && result.seconds < 30.0, "and it is ended at the bound, not long after");
        }
        {
            wil::unique_event_nothrow stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
            std::thread presser([&]
            {
                Sleep(500u);
                stop.SetEvent();
            });
            const TS::ChildRun run       = ChildOf(L"stopped", L"hang");
            const TS::ChildResult result = IL::RunChildProcess(run, 60u, stop.get(), false);
            presser.join();
            Check(result.launched && result.interrupted && ! result.timedOut, "a stopped run ends its child and says it was stopped");
            Check(result.seconds < 30.0, "and does not wait for the bound");
        }
        {
            const TS::ChildRun run = {L"missing", (_directory / L"missing.log").wstring(), L"\"" + (_directory / L"no-such-program.exe").wstring() + L"\""};
            const TS::ChildResult result = IL::RunChildProcess(run, 60u, never.get(), false);
            Check(! result.launched && ! result.error.empty(), "a program that cannot be started is a launch failure with its reason");
        }
        {
            const TS::ChildRun run       = {L"nolog", (_directory / L"no-such-directory" / L"x.log").wstring(), L"cmd.exe /c exit 0"};
            const TS::ChildResult result = IL::RunChildProcess(run, 60u, never.get(), false);
            Check(! result.launched && ! result.error.empty(), "a log that cannot be created stops the child from starting");
        }
    }

    void SessionLease()
    {
        const std::wstring name = std::format(L"Local\\DxUi.InteractiveTestRun.v1.SelfTest.{}", GetCurrentProcessId());
        Win32LeaseServices first(name.c_str());
        Win32LeaseServices second(name.c_str());
        // A mutex is recursive for the thread that owns it, so the second run is another thread, as it is another process.
        const auto acquireElsewhere = [](Win32LeaseServices& services)
        {
            bool acquired = false;
            std::thread([&]
            {
                acquired = services.TryAcquire();
                if (acquired)
                    services.Release();
            }).join();
            return acquired;
        };
        Check(first.TryAcquire(), "the session's lease is taken");
        Check(! acquireElsewhere(second), "a second run in the session finds it held");
        first.Release();
        Check(acquireElsewhere(second), "and takes it once the first run released it");
        TS::Win32DesktopProbe probe;
        const TS::Availability availability = TS::CheckInteractiveDesktop(probe);
        Check(availability.available || ! availability.reason.empty(), "the desktop check answers, and says why when there is no desktop");
    }

    // Runs `body` on a thread attached to a private desktop. False when the session cannot make one.
    [[nodiscard]] bool OnPrivateDesktop(const std::function<void()>& body)
    {
        const std::wstring name = std::format(L"DxUi.InteractiveLease.SelfTest.{}", GetCurrentProcessId());
        const wil::unique_hdesk desktop(CreateDesktopW(name.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr));
        if (! desktop)
            return false;
        bool attached = false;
        std::thread worker([&]
        {
            attached = SetThreadDesktop(desktop.get()) != FALSE;
            if (attached)
                body();
        });
        worker.join();
        Check(attached, "a thread attaches to the private desktop");
        return true;
    }

    void PrivateDesktop()
    {
        const bool available = OnPrivateDesktop([this]
        {
            TS::LeaseRequest request;
            request.label           = L"Menu, NativeTextInput (x64 Debug)";
            request.estimateSeconds = 100u;
            const std::atomic<bool> calm{false};
            const auto ask = [&](unsigned seconds, IL::DialogScript script, const std::atomic<bool>& stopped)
            {
                request.confirmSeconds = seconds;
                script.silent          = true; // A test run makes no sound.
                return IL::Confirm(request, stopped, script);
            };
            const TS::Confirmation first = ask(30u, {IL::kStartButton, 300u}, calm);
            if (first == TS::Confirmation::Unavailable)
            {
                // Common Controls refused to open a dialog here (a session with no window station to show one on): nothing to prove.
                std::printf("SKIPPED: the confirmation cannot open on this session's private desktop, so the dialog and the warning were not exercised\n");
                return;
            }
            Check(first == TS::Confirmation::Started, "the confirmation starts the run when Start is chosen");
            Check(ask(30u, {IDCANCEL, 300u}, calm) == TS::Confirmation::Declined, "and does not when Cancel is chosen");
            {
                const auto before             = std::chrono::steady_clock::now();
                const TS::Confirmation answer = ask(1u, {}, calm);
                const double seconds          = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
                Check(answer == TS::Confirmation::TimedOut, "nobody answering is a cancellation");
                Check(seconds >= 0.9 && seconds < 20.0, "which comes when the confirmation's time runs out");
            }
            {
                const std::atomic<bool> stopped{true};
                Check(ask(30u, {}, stopped) == TS::Confirmation::Interrupted, "a run stopped while the person is asked ends the confirmation");
            }
            Check(ask(0u, {IL::kStartButton, 300u}, calm) == TS::Confirmation::Started, "a confirmation without a time limit waits for the person");

            IL::WarningBanner banner;
            Check(banner.Show(request.label, request.estimateSeconds), "the warning is shown");
            const HWND window = banner.Window();
            Check(window != nullptr && IsWindowVisible(window) != FALSE, "it is visible");
            const LONG_PTR style = window != nullptr ? GetWindowLongPtrW(window, GWL_EXSTYLE) : 0;
            Check((style & WS_EX_TOPMOST) != 0 && (style & WS_EX_TRANSPARENT) != 0 && (style & WS_EX_LAYERED) != 0,
                  "above every window, and the pointer and keys pass through it");
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1300);
            while (std::chrono::steady_clock::now() < until)
            {
                MsgWaitForMultipleObjects(0u, nullptr, FALSE, 100u, QS_ALLINPUT);
                IL::PumpMessages();
            }
            Check(IsWindow(window) != FALSE, "it survives its own timer and painting");
            banner.Hide();
            Check(IsWindow(window) == FALSE, "and is gone when hidden");
        });
        if (! available)
            std::printf("SKIPPED: this session cannot create a private desktop, so the dialog and the warning were not exercised\n");
    }

    std::filesystem::path _directory;
    int _checks   = 0;
    int _failures = 0;
};

// The children the self-test starts: this program with a hidden switch.
int SelfTestChild(std::wstring_view mode)
{
    std::fputs("child output\n", stdout);
    std::fputs("child error\n", stderr);
    std::fflush(stdout);
    if (mode == L"hang")
    {
        Sleep(INFINITE);
        return 1;
    }
    if (mode.starts_with(L"exit:"))
    {
        unsigned code = 0u;
        return ParseSeconds(mode.substr(5u), code) ? static_cast<int>(code) : TS::LeaseExit::kUsage;
    }
    return TS::LeaseExit::kUsage;
}

[[nodiscard]] bool IsVerifiedGitHubHostedRunner() noexcept
{
    const auto isExact = [](PCWSTR name, PCWSTR expected) noexcept
    {
        wchar_t value[64]{};
        const DWORD length = GetEnvironmentVariableW(name, value, static_cast<DWORD>(std::size(value)));
        return length != 0u && length < std::size(value) && std::wstring_view(value, length) == expected;
    };
    return isExact(L"CI", L"true") && isExact(L"GITHUB_ACTIONS", L"true") && isExact(L"RUNNER_ENVIRONMENT", L"github-hosted") &&
           isExact(L"RUNNER_OS", L"Windows");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    enum class Mode
    {
        None,
        Check,
        Run,
        RunHosted,
        SelfTest
    } mode = Mode::None;
    RunOptions options;
    std::filesystem::path checkResult;
    std::optional<std::wstring> selfTestChildMode;
    for (int index = 1; index < argc; ++index)
    {
        const std::wstring_view argument = argv[index] != nullptr ? std::wstring_view(argv[index]) : std::wstring_view{};
        if (argument == L"--check")
            mode = Mode::Check;
        else if (argument == L"--run")
            mode = Mode::Run;
        else if (argument == L"--run-hosted")
            mode = Mode::RunHosted;
        else if (argument == L"--self-test")
            mode = Mode::SelfTest;
        else if (const auto child = ValueOf(argument, L"--self-test-child"))
            selfTestChildMode = std::wstring(*child);
        else if (const auto plan = ValueOf(argument, L"--plan"))
            options.plan = std::filesystem::path(*plan);
        else if (const auto result = ValueOf(argument, L"--result"))
        {
            options.result = std::filesystem::path(*result);
            checkResult    = options.result;
        }
        else if (const auto label = ValueOf(argument, L"--label"))
            options.label = std::wstring(*label);
        else if (const auto estimate = ValueOf(argument, L"--estimate"))
        {
            if (! ParseSeconds(*estimate, options.estimateSeconds))
                return Usage("--estimate takes a whole number of seconds");
        }
        else if (const auto confirm = ValueOf(argument, L"--confirm-timeout"))
        {
            if (! ParseSeconds(*confirm, options.confirmSeconds))
                return Usage("--confirm-timeout takes a whole number of seconds (0 waits for as long as it takes)");
        }
        else if (const auto bound = ValueOf(argument, L"--child-timeout"))
        {
            if (! ParseSeconds(*bound, options.childTimeoutSeconds))
                return Usage("--child-timeout takes a whole number of seconds (0 sets no bound)");
        }
        else
            return Usage(std::format("unknown argument: {}", TS::ToUtf8(argument)));
    }
    if (mode == Mode::None && ! selfTestChildMode.has_value())
        return Usage("choose --check, --run, --run-hosted or --self-test");
    if ((mode == Mode::Run || mode == Mode::RunHosted) && (options.plan.empty() || options.result.empty()))
        return Usage("--run and --run-hosted need --plan=<file> and --result=<file>");
    // The hosted path bypasses the person-facing confirmation only when every runner marker is exact. Keep this check before desktop
    // inspection, DPI changes, warning creation, lease acquisition or result-file writes.
    if (mode == Mode::RunHosted && ! IsVerifiedGitHubHostedRunner())
        return Usage("--run-hosted is limited to verified GitHub-hosted Windows runners");

    // A failed runtime check reports to stderr and ends the run (exit code 3); only the lease's own confirmation and warning are
    // windows a person sees.
    DxUiTestFailureReports::RouteAwayFromDialogs();

    // The intentional runtime-failure child is still a test process: install the no-dialog handler before executing its deliberate
    // failure, just as the prior early child dispatch did.
    if (selfTestChildMode.has_value())
        return SelfTestChild(*selfTestChildMode);

    // Per-monitor DPI, so the dialog and the warning are sharp on every monitor and the pointer is read in physical pixels.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    g_interruptEvent.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    g_finished.reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    const auto finished = wil::scope_exit([]() noexcept
    {
        if (g_finished)
            g_finished.SetEvent();
    });

    switch (mode)
    {
        case Mode::Check: return Check(checkResult);
        case Mode::Run: return Run(options, false);
        case Mode::RunHosted: return Run(options, true);
        case Mode::SelfTest: return SelfTest().Run();
        case Mode::None: break;
    }
    return TS::LeaseExit::kUsage;
}
