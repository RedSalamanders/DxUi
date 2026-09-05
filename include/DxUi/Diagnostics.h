#pragma once
#include <cstdint>
#include <string_view>
#include <windows.h>
// Sinks and their string views are borrowed synchronously on the calling UI thread. Do not re-enter controls.
namespace DxUi::Diagnostics
{
struct PerformanceRecord
{
    std::wstring_view name;
    std::wstring_view detail;
    uint64_t durationUs;
    uint64_t value0;
    uint64_t value1;
    HRESULT result;
};
using Sink                                          = void (*)(std::wstring_view level, std::wstring_view message) noexcept;
using PerformanceSink                               = void (*)(const PerformanceRecord&) noexcept;
inline thread_local Sink sink                       = nullptr;
inline thread_local PerformanceSink performanceSink = nullptr;
} // namespace DxUi::Diagnostics
