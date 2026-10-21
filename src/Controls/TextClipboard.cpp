#include "TextClipboard.h"
#include <cstring>
#include <cwchar>
#include <limits>
#include <shlobj.h>

namespace DxUi
{
namespace
{
const ClipboardSystemCalls& ClipboardCalls() noexcept
{
    static const ClipboardSystemCalls calls;
    return testClipboardSystemCalls ? *testClipboardSystemCalls : calls;
}
HRESULT LastClipboardError() noexcept
{
    const DWORD error = GetLastError();
    return HRESULT_FROM_WIN32(error ? error : ERROR_ACCESS_DENIED);
}
HRESULT ReadUnicodeClipboardData(HANDLE data, std::wstring& text) noexcept
{
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
class WindowsTextClipboard final : public TextClipboard, public StructuredTextClipboard
{
public:
    HRESULT Read(HWND owner, std::wstring& text) noexcept override
    {
        text.clear();
        if (! owner)
            return E_INVALIDARG;
        const auto& calls = ClipboardCalls();
        if (! calls.open(owner))
            return LastClipboardError();
        const auto close = wil::scope_exit([&calls]() noexcept { calls.close(); });
        return ReadUnicodeClipboardData(calls.getData(CF_UNICODETEXT), text);
    }
    HRESULT ReadContent(HWND owner, TextClipboardContent& content) noexcept override
    try
    {
        if (! owner)
            return E_INVALIDARG;
        TextClipboardContent staged;
        const auto& calls = ClipboardCalls();
        if (! calls.open(owner))
            return LastClipboardError();
        const auto close = wil::scope_exit([&calls]() noexcept { calls.close(); });
        if (const auto data = calls.getData(CF_HDROP))
        {
            const auto bytes   = GlobalSize(data);
            const auto* buffer = static_cast<const std::byte*>(GlobalLock(data));
            if (! buffer)
                return LastClipboardError();
            const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(data); });
            const auto result = DecodeClipboardFilePaths(std::span(buffer, bytes), staged);
            if (result == S_OK)
            {
                content.Swap(staged);
                return S_OK;
            }
            if (FAILED(result))
                return result;
        }
        const auto result = ReadUnicodeClipboardData(calls.getData(CF_UNICODETEXT), staged.text);
        if (result == S_OK)
            content.Swap(staged);
        return result;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT Write(HWND owner, std::wstring_view text) noexcept override
    {
        if (! owner)
            return E_INVALIDARG;
        size_t bytes = 0;
        RETURN_IF_FAILED(ClipboardAllocationSize(text.size(), bytes));
        if (text.find(L'\0') != std::wstring_view::npos)
            return E_INVALIDARG;
        const auto& calls = ClipboardCalls();
        wil::unique_hglobal memory(calls.allocate(GMEM_MOVEABLE, bytes));
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
        if (! calls.open(owner))
            return LastClipboardError();
        const auto close = wil::scope_exit([&calls]() noexcept { calls.close(); });
        if (! calls.empty() || ! calls.setData(CF_UNICODETEXT, memory.get()))
            return LastClipboardError();
        static_cast<void>(memory.release());
        return S_OK;
    }
};
} // namespace
#if DXUI_ENABLE_DIAGNOSTICS
thread_local TextClipboard* testTextClipboard                     = nullptr;
thread_local const ClipboardSystemCalls* testClipboardSystemCalls = nullptr;
#endif
HRESULT ClipboardAllocationSize(size_t textUnits, size_t& bytes) noexcept
{
    bytes = 0;
    // Include the terminator before multiplying; reject overflow before scanning or allocating.
    if (textUnits > (std::numeric_limits<size_t>::max)() / sizeof(wchar_t) - 1)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    bytes = (textUnits + 1) * sizeof(wchar_t);
    return S_OK;
}
bool ValidClipboardUnicode(std::wstring_view text) noexcept
{
    if (text.find(L'\0') != text.npos)
        return false;
    for (size_t at = 0u; at < text.size(); ++at)
    {
        const auto value = static_cast<uint16_t>(text[at]);
        if (value >= 0xd800u && value <= 0xdbffu)
        {
            if (++at == text.size() || static_cast<uint16_t>(text[at]) < 0xdc00u || static_cast<uint16_t>(text[at]) > 0xdfffu)
                return false;
        }
        else if (value >= 0xdc00u && value <= 0xdfffu)
            return false;
    }
    return true;
}
HRESULT DecodeClipboardText(std::span<const wchar_t> buffer, std::wstring& text) noexcept
{
    text.clear();
    if (buffer.empty())
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const wchar_t* end = std::wmemchr(buffer.data(), L'\0', buffer.size());
    if (! end)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (static_cast<size_t>(end - buffer.data()) > text.max_size())
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    // Clipboard contents are external data; reject malformed UTF-16 instead of creating broken caret boundaries.
    if (! ValidClipboardUnicode(std::wstring_view(buffer.data(), static_cast<size_t>(end - buffer.data()))))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
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
HRESULT DecodeClipboardFilePaths(std::span<const std::byte> buffer, TextClipboardContent& content) noexcept
try
{
    if (buffer.size() < sizeof(DROPFILES))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    DROPFILES header{};
    std::memcpy(&header, buffer.data(), sizeof(header));
    if (! header.fWide || header.pFiles < sizeof(header) || header.pFiles > buffer.size() || buffer.size() - header.pFiles < 2u * sizeof(wchar_t) ||
        (buffer.size() - header.pFiles) % sizeof(wchar_t) || reinterpret_cast<uintptr_t>(buffer.data() + header.pFiles) % alignof(wchar_t))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const auto* paths = reinterpret_cast<const wchar_t*>(buffer.data() + header.pFiles);
    const auto units =
        (std::min)((buffer.size() - header.pFiles) / sizeof(wchar_t), TextClipboardContent::MaximumTotalFileUnits + TextClipboardContent::MaximumFiles + 1u);
    TextClipboardContent staged;
    staged.filePaths.reserve(TextClipboardContent::MaximumFiles);
    size_t at{}, total{};
    while (at < units)
    {
        const auto count = (std::min)(units - at, TextClipboardContent::MaximumFileUnits + 1u);
        const auto* end  = std::wmemchr(paths + at, L'\0', count);
        if (! end)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        const auto length = static_cast<size_t>(end - (paths + at));
        if (! length)
        {
            if (staged.filePaths.empty())
                return at + 1u < units && paths[at + 1u] == L'\0' ? S_FALSE : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            content.Swap(staged);
            return S_OK;
        }
        if (staged.filePaths.size() == TextClipboardContent::MaximumFiles || length > TextClipboardContent::MaximumTotalFileUnits - total)
            return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        total += length;
        staged.filePaths.emplace_back(0u, L' ');
        const auto decoded = DecodeClipboardText(std::span(paths + at, length + 1u), staged.filePaths.back());
        if (decoded != S_OK)
            return decoded;
        at += length + 1u;
    }
    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA); // A Unicode file list requires its second terminating NUL.
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
HRESULT ReadTextClipboardContent(TextClipboard& clipboard, HWND owner, TextClipboardContent& content, bool includeFiles) noexcept
try
{
    if (includeFiles)
        if (auto* structured = dynamic_cast<StructuredTextClipboard*>(&clipboard))
            return structured->ReadContent(owner, content);
    TextClipboardContent staged;
    const auto result = clipboard.Read(owner, staged.text);
    if (result == S_OK)
        content.Swap(staged);
    return result;
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
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
