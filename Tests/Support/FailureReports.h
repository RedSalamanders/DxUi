#pragma once

#include <crtdbg.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <windows.h>

// A test process never waits on a dialog. In a Debug build a failed runtime check (an STL range check, a CRT assertion)
// otherwise opens a modal Abort/Retry/Ignore box on the desktop: an unattended run hangs until its watchdog ends it, and an
// attended one interrupts the person at the desktop. The report goes to stderr, which the suite log keeps, and the check
// ends the process with exit code 3 (the dialog's Abort), with no Windows Error Reporting dialog either.
namespace DxUiTestFailureReports
{
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
