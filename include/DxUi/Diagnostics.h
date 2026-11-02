#pragma once
#include <cstdint>
#include <string_view>
#include <windows.h>
// Sinks and their string views are borrowed synchronously on the calling UI thread. Do not re-enter controls.
namespace DxUi::Diagnostics
{
// Counts the checked native control-object heap beside ordinary C++ allocation
// instrumentation. Synchronous, borrowed and non-reentrant, like the sinks below.
struct NativeAllocationRecord
{
    bool allocating;
    size_t requestedBytes;
    size_t reservedBytes;
};
using NativeAllocationSink                                    = void (*)(const NativeAllocationRecord&) noexcept;
inline thread_local NativeAllocationSink nativeAllocationSink = nullptr;
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
