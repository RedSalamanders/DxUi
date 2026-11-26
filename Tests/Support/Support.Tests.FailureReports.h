#pragma once

#include <atomic>
#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <windows.h>

// A test process never waits on a dialog. In a Debug build a failed runtime check (an STL range check, a CRT assertion)
// otherwise opens a modal Abort/Retry/Ignore box on the desktop: an unattended run hangs until its watchdog ends it, and an
// attended one interrupts the person at the desktop. The report goes to stderr, which the suite log keeps, and the check
// ends the process with exit code 3 (the dialog's Abort), with no Windows Error Reporting dialog either.
namespace DxUiTestFailureReports
{
inline void WriteFailure(const char* message) noexcept
{
    // A previous injected iostream allocation failure can leave cerr failed.
    // Write to the borrowed standard handle without allocating or depending on
    // its C/C++ text orientation; the runner still owns the actual log pipe.
    const auto output = GetStdHandle(STD_ERROR_HANDLE);
    if (output == nullptr || output == INVALID_HANDLE_VALUE)
        return;
    DWORD written{};
    static_cast<void>(WriteFile(output, "FAIL: ", 6u, &written, nullptr));
    if (message)
        static_cast<void>(WriteFile(output, message, static_cast<DWORD>(std::strlen(message)), &written, nullptr));
    static_cast<void>(WriteFile(output, "\n", 1u, &written, nullptr));
}

// The test under way, for the line a failed check prints (set by the runner of the test programs; null outside a test).
[[nodiscard]] inline std::atomic<const char*>& CurrentTestName() noexcept
{
    static std::atomic<const char*> name{nullptr};
    return name;
}

// Ends the run of a failed check at once. std::exit would run the static destructors of a process whose test never unwound: a
// window host or UI Automation provider the test left alive still holds the module gate, so the static failure host's
// uninstall returns ERROR_BUSY, which raises a contract failure, a minidump and a fail-fast (or stalls under load) and turns one
// failed check into the suite's 300-second timeout. The caller has printed its message to the unbuffered stderr; this names the
// test, flushes what the C streams hold and terminates with exit code 1, the code of a failed check, with nothing else running.
[[noreturn]] inline void EndRunAfterFailedCheck() noexcept
{
    if (const char* const test = CurrentTestName().load(); test != nullptr)
    {
        const auto output = GetStdHandle(STD_ERROR_HANDLE);
        if (output != nullptr && output != INVALID_HANDLE_VALUE)
        {
            DWORD written{};
            static_cast<void>(WriteFile(output, "FAILED in test: ", 16u, &written, nullptr));
            static_cast<void>(WriteFile(output, test, static_cast<DWORD>(std::strlen(test)), &written, nullptr));
            static_cast<void>(WriteFile(output, "\n", 1u, &written, nullptr));
        }
    }
    static_cast<void>(std::fflush(nullptr));
    static_cast<void>(TerminateProcess(GetCurrentProcess(), 1u));
    std::abort(); // Only reached if the process could not be terminated.
}
#if defined(_DEBUG)
inline int __cdecl ReportAndEnd(int reportType, wchar_t* message, int* returnValue) noexcept
{
    if (reportType == _CRT_WARN)
    {
        return FALSE; // Warnings keep the CRT's own handling: the debugger output.
    }
    std::fputws(message ? message : L"(a CRT report without text)\n", stderr);
    std::fflush(stderr);
    *returnValue = 0;
    std::abort();
}
#endif

// Call first in main: before any test can fail a check.
inline void RouteAwayFromDialogs() noexcept
{
    static_cast<void>(_set_abort_behavior(0u, _WRITE_ABORT_MSG | _CALL_REPORTFAULT));
    static_cast<void>(SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX));
#if defined(_DEBUG)
    static_cast<void>(_CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, &ReportAndEnd));
#endif
}
} // namespace DxUiTestFailureReports
