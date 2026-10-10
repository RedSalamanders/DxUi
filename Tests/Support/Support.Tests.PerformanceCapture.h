#pragma once
#include "../../src/Support/Diagnostics.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace TestPerformanceCapture
{
inline std::ofstream output;
inline std::string Utf8(std::wstring_view text)
{
    if (text.empty())
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
inline void String(std::wstring_view value)
{
    output << '"';
    for (unsigned char c : Utf8(value))
    {
        if (c == '"' || c == '\\')
            output << '\\' << c;
        else if (c < 32)
        {
            const char* hex = "0123456789abcdef";
            output << "\\u00" << hex[c >> 4] << hex[c & 15];
        }
        else
            output << c;
    }
    output << '"';
}
inline void Emit(const DxUi::Diagnostics::PerformanceRecord& r) noexcept
{
    try
    {
        output << "{\"metric\":";
        String(r.name);
        output << ",\"detail\":";
        String(r.detail);
        output << ",\"durationUs\":" << r.durationUs << ",\"value0\":" << r.value0 << ",\"value1\":" << r.value1 << ",\"result\":" << r.result
               << ",\"value\":" << (r.durationUs ? r.durationUs : r.value0) << "}\n";
        output.flush();
    }
    catch (const std::exception&)
    {
    }
}
inline void Start(const std::filesystem::path& path, std::wstring_view, std::wstring_view)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    output.open(path, std::ios::binary | std::ios::trunc);
    if (! output)
        throw std::runtime_error("Cannot create performance artifact");
    DxUi::Diagnostics::performanceSink = Emit;
    DxUi::SetFrameMetricSink([](void*, std::wstring_view name, uint64_t value) noexcept { Emit({name, {}, 0, value, 0, S_OK}); }, nullptr);
}
inline void Stop() noexcept
{
    DxUi::Diagnostics::performanceSink = nullptr;
    DxUi::SetFrameMetricSink(nullptr, nullptr);
    output.close();
}
} // namespace TestPerformanceCapture
