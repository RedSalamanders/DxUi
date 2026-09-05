#include "TextClipboard.h"
#include <algorithm>
#include <cstring>
#include <cwchar>

namespace DxUi
{
namespace
{
constexpr size_t kLimit = 65536;
HRESULT LastClipboardError() noexcept
{
    const DWORD error = GetLastError();
    return HRESULT_FROM_WIN32(error ? error : ERROR_ACCESS_DENIED);
}
class WindowsTextClipboard final : public TextClipboard
{
public:
    HRESULT Read(HWND owner, std::wstring& text) noexcept override
    {
        text.clear();
        if (! owner)
            return E_INVALIDARG;
        if (! OpenClipboard(owner))
            return LastClipboardError();
        const auto close  = wil::scope_exit([]() noexcept { CloseClipboard(); });
        const HANDLE data = GetClipboardData(CF_UNICODETEXT);
        if (! data)
            return S_FALSE;
        const SIZE_T bytes = GlobalSize(data);
        if (bytes < sizeof(wchar_t) || bytes % sizeof(wchar_t))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        const auto* value = static_cast<const wchar_t*>(GlobalLock(data));
        if (! value)
            return LastClipboardError();
        const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(data); });
        return DecodeClipboardText(std::span(value, bytes / sizeof(wchar_t)), text);
    }
    HRESULT Write(HWND owner, std::wstring_view text) noexcept override
    {
        if (! owner)
            return E_INVALIDARG;
        if (text.size() > kLimit)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        if (text.find(L'\0') != std::wstring_view::npos)
            return E_INVALIDARG;
        wil::unique_hglobal memory(GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t)));
        if (! memory)
            return E_OUTOFMEMORY;
        auto* value = static_cast<wchar_t*>(GlobalLock(memory.get()));
        if (! value)
            return LastClipboardError();
        {
            const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(memory.get()); });
            if (! text.empty())
                std::memcpy(value, text.data(), text.size() * sizeof(wchar_t));
            value[text.size()] = 0;
        }
        if (! OpenClipboard(owner))
            return LastClipboardError();
        const auto close = wil::scope_exit([]() noexcept { CloseClipboard(); });
        if (! EmptyClipboard() || ! SetClipboardData(CF_UNICODETEXT, memory.get()))
            return LastClipboardError();
        static_cast<void>(memory.release());
        return S_OK;
    }
};
} // namespace
#if DXUI_ENABLE_DIAGNOSTICS
thread_local TextClipboard* testTextClipboard = nullptr;
#endif
HRESULT DecodeClipboardText(std::span<const wchar_t> buffer, std::wstring& text) noexcept
{
    text.clear();
    const size_t capacity = (std::min)(buffer.size(), kLimit + 1);
    if (! capacity)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const wchar_t* end = std::wmemchr(buffer.data(), L'\0', capacity);
    if (! end)
        return HRESULT_FROM_WIN32(buffer.size() > kLimit ? ERROR_BUFFER_OVERFLOW : ERROR_INVALID_DATA);
    // Clipboard contents are external data; reject malformed UTF-16 instead of creating broken caret boundaries.
    for (const wchar_t* cursor = buffer.data(); cursor != end; ++cursor)
    {
        const auto unit = static_cast<uint16_t>(*cursor);
        if (unit >= 0xd800 && unit <= 0xdbff)
        {
            if (++cursor == end || static_cast<uint16_t>(*cursor) < 0xdc00 || static_cast<uint16_t>(*cursor) > 0xdfff)
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
        else if (unit >= 0xdc00 && unit <= 0xdfff)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    try
    {
        text.assign(buffer.data(), end);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
TextClipboard& SystemTextClipboard() noexcept
{
    static WindowsTextClipboard clipboard;
    return clipboard;
}
TextClipboard& ActiveTextClipboard() noexcept
{
#if DXUI_ENABLE_DIAGNOSTICS
    if (testTextClipboard)
        return *testTextClipboard;
#endif
    return SystemTextClipboard();
}
} // namespace DxUi
