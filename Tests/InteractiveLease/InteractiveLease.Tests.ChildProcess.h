#pragma once

#include "../Support/Support.Tests.InteractiveLease.h"
#include "../Support/Support.Tests.TestWatchdog.h"

#include <chrono>
#include <format>
#include <functional>
#include <string>
#include <vector>

namespace DxUi::InteractiveLease
{
// Dispatches this thread's pending messages, so the warning window the lease shows keeps painting while it waits for a child.
inline void PumpMessages() noexcept
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

// Runs one child of a plan: its standard output and error go to its log, it is bound to a job that dies with the lease (so a lease
// that is killed leaves no test process running on the person's desktop), and it is granted the foreground, which it can then take
// for its own windows. Only this one process is ever granted anything, and only this one is ever ended: never another process by name.
//
// The wait pumps messages, ends when the child exits, when `interruptEvent` is set (the person pressed Ctrl+C: the child is ended)
// or when `timeoutSeconds` pass (the child is ended and reported with the watchdog's exit code, 124). The runner's own watchdog
// bounds each test; this bound is for what the watchdog cannot reach, such as a child that hangs before it starts a test.
//
// Only a process that holds the foreground can grant it, and the child before this one may have left the person's window there when
// its own windows closed: `holdForeground` takes it back when the grant is refused for that reason, and again until it is granted.
[[nodiscard]] inline TestSupport::ChildResult RunChildProcess(
    const TestSupport::ChildRun& run, unsigned timeoutSeconds, HANDLE interruptEvent, bool grantForeground, const std::function<void()>& holdForeground = {})
{
    TestSupport::ChildResult result{};

    SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const wil::unique_hfile log(CreateFileW(run.logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inheritable, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! log)
    {
        result.error = std::format("cannot create the log {} (error {})", TestSupport::ToUtf8(run.logPath), GetLastError());
        return result;
    }
    const wil::unique_hfile nothing(
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (! nothing)
    {
        result.error = std::format("cannot open NUL for the child's input (error {})", GetLastError());
        return result;
    }

    // The child inherits exactly its three standard handles, nothing else this process holds.
    SIZE_T attributeBytes = 0u;
    InitializeProcThreadAttributeList(nullptr, 1u, 0u, &attributeBytes);
    std::vector<std::byte> attributeStorage(attributeBytes);
    auto* const attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (attributeBytes == 0u || InitializeProcThreadAttributeList(attributes, 1u, 0u, &attributeBytes) == FALSE)
    {
        result.error = std::format("cannot prepare the child's handle list (error {})", GetLastError());
        return result;
    }
    const auto deleteAttributes = wil::scope_exit([&]() noexcept { DeleteProcThreadAttributeList(attributes); });
    HANDLE inherited[2]         = {nothing.get(), log.get()};
    if (UpdateProcThreadAttribute(attributes, 0u, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr) == FALSE)
    {
        result.error = std::format("cannot restrict the child's inherited handles (error {})", GetLastError());
        return result;
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb         = sizeof(startup);
    startup.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput  = nothing.get();
    startup.StartupInfo.hStdOutput = log.get();
    startup.StartupInfo.hStdError  = log.get();
    startup.lpAttributeList        = attributes;

    std::wstring commandLine = run.commandLine; // CreateProcess may write to its command line.
    wil::unique_process_information process;
    if (CreateProcessW(nullptr,
                       commandLine.data(),
                       nullptr,
                       nullptr,
                       TRUE,
                       EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW,
                       nullptr,
                       nullptr,
                       &startup.StartupInfo,
                       process.addressof()) == FALSE)
    {
        result.error = std::format("cannot start {} (error {})", TestSupport::ToUtf8(run.commandLine), GetLastError());
        return result;
    }
    result.launched = true;
    // Whatever happens next, a child this function started does not outlive it.
    const auto endChild = wil::scope_exit([&]() noexcept
    {
        if (WaitForSingleObject(process.hProcess, 0) != WAIT_OBJECT_0)
        {
            TerminateProcess(process.hProcess, 1u);
            WaitForSingleObject(process.hProcess, 5000u);
        }
    });

    wil::unique_handle job(CreateJobObjectW(nullptr, nullptr));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    const bool jobbed                       = job && SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) != FALSE &&
                                              AssignProcessToJobObject(job.get(), process.hProcess) != FALSE;
    if (! jobbed)
        job.reset(); // A lease that cannot job its child still ends exactly that process, with TerminateProcess.

    const auto started = std::chrono::steady_clock::now();
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1))
    {
        result.launched = false;
        result.error    = std::format("cannot resume the child (error {})", GetLastError());
        return result;
    }

    // A process that was just resumed has not registered with the window system, so the grant fails with ERROR_INVALID_PARAMETER
    // until it has: that is waited out. A refusal (ERROR_ACCESS_DENIED) means this process does not hold the foreground, so it takes it
    // again, at most three times; any other error, or running out of the four seconds, ends the attempt.
    if (grantForeground)
    {
        const auto grantDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        int regained             = 0;
        for (;;)
        {
            if (AllowSetForegroundWindow(process.dwProcessId) != FALSE)
                break;
            const DWORD error = GetLastError();
            const bool retry  = error == ERROR_INVALID_PARAMETER || (error == ERROR_ACCESS_DENIED && holdForeground && regained < 3);
            if (! retry || std::chrono::steady_clock::now() >= grantDeadline || WaitForSingleObject(process.hProcess, 0) == WAIT_OBJECT_0)
            {
                result.launched = false;
                result.error    = std::format("the foreground could not be granted to the child (error {}): this process does not hold it", error);
                return result;
            }
            if (error == ERROR_ACCESS_DENIED)
            {
                ++regained;
                holdForeground();
            }
            else
            {
                MsgWaitForMultipleObjects(0u, nullptr, FALSE, 10u, QS_ALLINPUT);
            }
            PumpMessages();
        }
    }

    const std::chrono::steady_clock::time_point deadline =
        timeoutSeconds != 0u ? started + std::chrono::seconds(timeoutSeconds) : std::chrono::steady_clock::time_point::max();
    const HANDLE waits[2] = {process.hProcess, interruptEvent};
    const DWORD waitCount = interruptEvent != nullptr ? 2u : 1u;
    for (;;)
    {
        DWORD timeoutMilliseconds = INFINITE;
        if (timeoutSeconds != 0u)
        {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0)
            {
                result.timedOut = true;
                break;
            }
            timeoutMilliseconds = static_cast<DWORD>((std::min)(remaining.count(), static_cast<std::chrono::milliseconds::rep>(0x7FFFFFFF)));
        }
        const DWORD wait = MsgWaitForMultipleObjects(waitCount, waits, FALSE, timeoutMilliseconds, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0)
            break; // The child exited.
        if (wait == WAIT_OBJECT_0 + 1u && interruptEvent != nullptr)
        {
            result.interrupted = true;
            break;
        }
        if (wait == WAIT_OBJECT_0 + waitCount)
        {
            PumpMessages();
            continue;
        }
        if (wait == WAIT_TIMEOUT)
        {
            result.timedOut = true;
            break;
        }
        result.error = std::format("waiting for the child failed (error {})", GetLastError());
        break;
    }

    if (result.timedOut || result.interrupted || ! result.error.empty())
    {
        const UINT code = result.timedOut ? TestSupport::kTestTimeoutExitCode : 130u;
        if (job)
            TerminateJobObject(job.get(), code);
        else
            TerminateProcess(process.hProcess, code);
        WaitForSingleObject(process.hProcess, 10000u);
    }
    DWORD exitCode = 0u;
    if (GetExitCodeProcess(process.hProcess, &exitCode) == FALSE || exitCode == STILL_ACTIVE)
        exitCode = result.timedOut ? TestSupport::kTestTimeoutExitCode : 1u;
    result.exitCode = exitCode;
    result.seconds  = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    return result;
}
} // namespace DxUi::InteractiveLease
