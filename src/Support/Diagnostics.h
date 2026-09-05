#pragma once
#include "PostedPayload.h"
#include "Utilities.h"
#include <DxUi/FrameRuntime.h>
#include <chrono>
#include <cwctype>
#include <format>
#include <string>
#include <utility>
#include <windows.h>

#include <DxUi/Diagnostics.h>
namespace SecureWipe
{
inline void SecureClear(std::wstring& value) noexcept
{
    if (! value.empty())
        SecureZeroMemory(value.data(), value.size() * sizeof(wchar_t));
    value.clear();
}
} // namespace SecureWipe
namespace Debug
{
template <class... Args> void Write(std::wstring_view level, std::wformat_string<Args...> format, Args&&... args) noexcept
{
    if (! DxUi::Diagnostics::sink)
        return;
    try
    {
        auto message = std::format(format, std::forward<Args>(args)...);
        DxUi::Diagnostics::sink(level, message);
    }
    catch (const std::exception&)
    {
    }
}
template <class... Args> void Error(std::wformat_string<Args...> format, Args&&... args) noexcept
{
    Write(L"error", format, std::forward<Args>(args)...);
}
template <class... Args> void Warning(std::wformat_string<Args...> format, Args&&... args) noexcept
{
    Write(L"warning", format, std::forward<Args>(args)...);
}
template <class... Args> void Info(std::wformat_string<Args...> format, Args&&... args) noexcept
{
    Write(L"info", format, std::forward<Args>(args)...);
}
template <class... Args> DWORD ErrorWithLastError(std::wformat_string<Args...> format, Args&&... args) noexcept
{
    auto error = GetLastError();
    Write(L"error", format, std::forward<Args>(args)...);
    return error;
}
namespace Perf
{
inline bool IsCaptureEnabled() noexcept
{
    return DxUi::Diagnostics::performanceSink != nullptr;
}
inline uint64_t ElapsedUs(std::chrono::steady_clock::time_point start) noexcept
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}
inline void Emit(std::wstring_view name, std::wstring_view detail, uint64_t duration, uint64_t value0 = 0, uint64_t value1 = 0, HRESULT hr = S_OK) noexcept
{
    if (auto sink = DxUi::Diagnostics::performanceSink)
        sink({name, detail, duration, value0, value1, hr});
}
inline void EmitValue(std::wstring_view name, uint64_t value, std::wstring_view detail = {}) noexcept
{
    Emit(name, detail, 0, value);
}
inline void EmitDurationUs(std::wstring_view name, uint64_t value, std::wstring_view detail = {}) noexcept
{
    Emit(name, detail, value);
}
inline void EmitCounter(std::wstring_view name, uint64_t value = 1, std::wstring_view detail = {}) noexcept
{
    EmitValue(name, value, detail);
}
class Scope
{
    DxUi::Diagnostics::PerformanceRecord _record{};
    std::chrono::steady_clock::time_point _start;

public:
    explicit Scope(std::wstring_view metric, std::wstring_view detail = {}) noexcept
        : _record{metric, detail, 0, 0, 0, S_OK},
          _start(std::chrono::steady_clock::now())
    {
    }
    ~Scope()
    {
        Emit(_record.name, _record.detail, ElapsedUs(_start), _record.value0, _record.value1, _record.result);
    }
    void SetHr(HRESULT value) noexcept
    {
        _record.result = value;
    }
    void SetDetail(std::wstring_view value) noexcept
    {
        _record.detail = value;
    }
    void SetValue0(uint64_t value) noexcept
    {
        _record.value0 = value;
    }
    void SetValue1(uint64_t value) noexcept
    {
        _record.value1 = value;
    }
};
} // namespace Perf
} // namespace Debug
