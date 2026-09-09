#pragma once
#include <DxUi/TextInputServices.h>
namespace DxUi
{
HRESULT DecodeClipboardText(std::span<const wchar_t> buffer, std::wstring& text) noexcept;
HRESULT ClipboardAllocationSize(size_t textUnits, size_t& bytes) noexcept;
[[nodiscard]] TextClipboard& SystemTextClipboard() noexcept;
// Test-only override, borrowed on the calling UI thread. Production never installs this.
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local TextClipboard* testTextClipboard;
// Private transport seam: tests replace only clipboard ownership calls and allocation failure.
// Global memory still uses real HGLOBALs owned by WIL; no desktop clipboard is accessed.
struct ClipboardSystemCalls final
{
    decltype(&OpenClipboard) open       = &OpenClipboard;
    decltype(&CloseClipboard) close     = &CloseClipboard;
    decltype(&GetClipboardData) getData = &GetClipboardData;
    decltype(&EmptyClipboard) empty     = &EmptyClipboard;
    decltype(&SetClipboardData) setData = &SetClipboardData;
    decltype(&GlobalAlloc) allocate     = &GlobalAlloc;
};
extern thread_local const ClipboardSystemCalls* testClipboardSystemCalls;
#endif
[[nodiscard]] TextClipboard& ActiveTextClipboard() noexcept;
} // namespace DxUi
