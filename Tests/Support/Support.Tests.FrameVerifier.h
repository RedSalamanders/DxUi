#pragma once
// The frame verifier of paint regions and culling (plan item W4.10; Testing_Validation.md, "Frame verifier"), as the Debug test
// programs run it: every embedded preparation that painted a region, culled a child or skipped a clip is compared, pixel for
// pixel, with the same scene painted whole with regions and culling off (RetainedCaches.Tests.Hooks.h). A difference fails the
// running test at once with the count and the bounding box of the differing pixels. DXUI_FRAME_VERIFY=report logs every
// difference and lets the run go on (a survey of what the verifier finds); DXUI_FRAME_VERIFY=0 turns the verifier off.
#include "Support.Tests.FailureReports.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include <Windows.h>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace FrameVerifier
{
enum class Mode : uint8_t
{
    Off,
    Fail,
    Report,
};

[[nodiscard]] inline Mode SelectedMode() noexcept
{
    wchar_t value[16]{};
    const DWORD length = GetEnvironmentVariableW(L"DXUI_FRAME_VERIFY", value, 16u);
    if (length == 0u || length >= 16u)
        return Mode::Fail;
    const std::wstring_view text(value, length);
    if (text == L"0" || text == L"off")
        return Mode::Off;
    if (text == L"report")
        return Mode::Report;
    return Mode::Fail;
}

inline void OnMismatchFail(const char* description) noexcept
{
    std::cerr << "[W4.10-VERIFY] " << description << '\n'
              << "FAILED: W4.10 frame verifier: a frame painted with regions and culling differs from the same scene painted whole with "
                 "regions and culling off\n"
              << std::flush;
    DxUiTestFailureReports::EndRunAfterFailedCheck();
}

// The test functions on the stack of a mismatch (report mode: which test made the frame).
inline void PrintMismatchStack() noexcept
{
    static bool initialized = false;
    if (! initialized)
        initialized = SymInitialize(GetCurrentProcess(), nullptr, TRUE) != FALSE;
    std::array<void*, 40u> frames{};
    const USHORT count = CaptureStackBackTrace(2u, static_cast<DWORD>(frames.size()), frames.data(), nullptr);
    std::cerr << "[W4.10-VERIFY]   made by:";
    for (USHORT index = 0u; index < count; ++index)
    {
        alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + 256u> buffer{};
        auto* symbol         = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen   = 255u;
        DWORD64 displacement = 0u;
        if (SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(frames[index]), &displacement, symbol))
        {
            const std::string_view name(symbol->Name);
            if (name.find("Test") != std::string_view::npos || name.find("Step") != std::string_view::npos || name.find("Run") != std::string_view::npos)
                std::cerr << " <- " << name;
        }
    }
    std::cerr << '\n';
}

inline void OnMismatchReport(const char* description) noexcept
{
    std::cerr << "[W4.10-VERIFY] " << description << '\n';
    PrintMismatchStack();
    std::cerr << std::flush;
}

// Turns the verifier on for the scope (unless DXUI_FRAME_VERIFY=0) and restores what was there.
class Scope final
{
public:
    Scope() noexcept : _previous(DxUi::Detail::GetPaintRegionVerificationForTest())
    {
        const Mode mode = SelectedMode();
        if (mode == Mode::Off)
            return;
        DxUi::Detail::SetPaintRegionMismatchObserverForTest(mode == Mode::Report ? &OnMismatchReport : &OnMismatchFail);
        DxUi::Detail::SetPaintRegionVerificationForTest(true);
    }
    ~Scope()
    {
        DxUi::Detail::SetPaintRegionVerificationForTest(_previous);
    }
    Scope(const Scope&)            = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&)                 = delete;
    Scope& operator=(Scope&&)      = delete;

private:
    bool _previous = false;
};

// The program's totals since it started.
struct Totals final
{
    uint64_t verified   = 0u;
    uint64_t mismatches = 0u;
};
[[nodiscard]] inline Totals Read() noexcept
{
    Totals totals;
    DxUi::Detail::GetPaintRegionVerificationTotalsForTest(totals.verified, totals.mismatches);
    return totals;
}
} // namespace FrameVerifier
