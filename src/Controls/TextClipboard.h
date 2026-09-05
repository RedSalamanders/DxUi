#pragma once
#include <DxUi/TextInputServices.h>
namespace DxUi
{
HRESULT DecodeClipboardText(std::span<const wchar_t> buffer, std::wstring& text) noexcept;
[[nodiscard]] TextClipboard& SystemTextClipboard() noexcept;
// Test-only override, borrowed on the calling UI thread. Production never installs this.
#if DXUI_ENABLE_DIAGNOSTICS
extern thread_local TextClipboard* testTextClipboard;
#endif
[[nodiscard]] TextClipboard& ActiveTextClipboard() noexcept;
} // namespace DxUi
