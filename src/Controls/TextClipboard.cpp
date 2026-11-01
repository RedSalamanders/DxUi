#include "TextClipboard.h"
#include <charconv>
#include <cstring>
#include <cwchar>
#include <limits>
#include <ole2.h>
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
class WindowsTextClipboard final : public TextClipboard,
                                   public StructuredTextClipboard,
                                   public SelectiveStructuredTextClipboard,
                                   public BitmapStructuredTextClipboard
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
    {
        return ReadSelectedContent(owner, content, true, false);
    }
    HRESULT ReadSelectedContent(HWND owner, TextClipboardContent& content, bool includeFiles, bool includeHtml) noexcept override
    {
        return ReadSelectedBitmapContent(owner, content, includeFiles, includeHtml, false);
    }
    HRESULT ReadSelectedBitmapContent(HWND owner, TextClipboardContent& content, bool includeFiles, bool includeHtml, bool includeBitmap) noexcept override
    try
    {
        if (! owner)
            return E_INVALIDARG;
        TextClipboardContent staged;
        const auto& calls = ClipboardCalls();
        if (! calls.open(owner))
            return LastClipboardError();
        const auto close = wil::scope_exit([&calls]() noexcept { calls.close(); });
        if (includeFiles)
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
        if (includeBitmap)
            for (const auto format : {CF_DIBV5, CF_DIB})
                if (const auto data = calls.getData(format))
                {
                    const auto bytes = GlobalSize(data);
                    if (bytes > TextClipboardContent::MaximumBitmapBytes)
                        continue;
                    const auto* buffer = static_cast<const std::byte*>(GlobalLock(data));
                    if (! buffer)
                        continue;
                    const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(data); });
                    const auto bitmap = DecodeClipboardBitmap(std::span(buffer, bytes), format == CF_DIBV5, staged);
                    if (bitmap == E_OUTOFMEMORY)
                        return bitmap;
                    if (bitmap == S_OK)
                    {
                        content.Swap(staged);
                        return S_OK;
                    }
                }
        const auto result = ReadUnicodeClipboardData(calls.getData(CF_UNICODETEXT), staged.text);
        if (FAILED(result))
            return result;
        const auto htmlFormat = includeHtml ? RegisterClipboardFormatW(L"HTML Format") : 0u;
        if (htmlFormat)
            if (const auto data = calls.getData(htmlFormat))
            {
                const auto bytes   = GlobalSize(data);
                const auto* buffer = static_cast<const std::byte*>(GlobalLock(data));
                if (buffer)
                {
                    const auto unlock = wil::scope_exit([&]() noexcept { GlobalUnlock(data); });
                    // MSVC Debug's empty size constructor reports proxy OOM to
                    // this HRESULT boundary; its default constructor is noexcept.
                    std::wstring fragment(0u, L' ');
                    const auto htmlResult = DecodeClipboardHtml(std::span(buffer, bytes), fragment);
                    if (htmlResult == E_OUTOFMEMORY)
                        return htmlResult;
                    if (htmlResult == S_OK)
                    {
                        staged.htmlFragment.swap(fragment);
                        staged.hasHtmlFragment = true;
                    }
                }
            }
        if (result == S_OK || staged.hasHtmlFragment)
        {
            content.Swap(staged);
            return S_OK;
        }
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
HRESULT DecodeClipboardHtml(std::span<const std::byte> buffer, std::wstring& fragment) noexcept
try
{
    constexpr size_t maximumBytes = 4u * 65536u + 4096u;
    if (buffer.empty() || buffer.size() > maximumBytes)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const auto bytes      = std::string_view(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    const auto readOffset = [&](std::string_view key, size_t& result) noexcept
    {
        const auto header = bytes.substr(0u, (std::min)(bytes.size(), size_t{4096u}));
        auto at           = header.find(key);
        if (at == header.npos || (at && header[at - 1u] != '\n'))
            return false;
        at += key.size();
        auto end = at;
        while (end < header.size() && header[end] >= '0' && header[end] <= '9')
            ++end;
        if (end == at || end == header.size() || (header[end] != '\r' && header[end] != '\n'))
            return false;
        const auto parsed = std::from_chars(header.data() + at, header.data() + end, result);
        return parsed.ec == std::errc{} && parsed.ptr == header.data() + end;
    };
    size_t htmlFirst{}, htmlLast{}, first{}, last{};
    if (! readOffset("StartHTML:", htmlFirst) || ! readOffset("EndHTML:", htmlLast) || ! readOffset("StartFragment:", first) ||
        ! readOffset("EndFragment:", last) || htmlFirst > first || first > last || last > htmlLast || htmlLast > bytes.size())
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const auto header = bytes.substr(0u, htmlFirst);
    for (const auto key : {std::string_view{"StartHTML:"}, std::string_view{"EndHTML:"}, std::string_view{"StartFragment:"}, std::string_view{"EndFragment:"}})
    {
        const auto at = header.find(key);
        if (at == header.npos || header.find(key, at + key.size()) != header.npos)
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    const auto html = bytes.substr(first, last - first);
    if (html.find('\0') != html.npos || html.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (html.empty())
    {
        fragment.clear();
        return S_OK;
    }
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, html.data(), static_cast<int>(html.size()), nullptr, 0);
    if (! length || length > 65536)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    std::wstring staged(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, html.data(), static_cast<int>(html.size()), staged.data(), length) != length)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    fragment.swap(staged);
    return S_OK;
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
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
HRESULT DecodeClipboardBitmap(std::span<const std::byte> buffer, bool v5, TextClipboardContent& content) noexcept
try
{
    if (buffer.size() < sizeof(BITMAPINFOHEADER) || buffer.size() > TextClipboardContent::MaximumBitmapBytes)
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    BITMAPINFOHEADER header{};
    std::memcpy(&header, buffer.data(), sizeof(header));
    if ((header.biSize != sizeof(BITMAPINFOHEADER) && header.biSize != sizeof(BITMAPV4HEADER) && header.biSize != sizeof(BITMAPV5HEADER)) ||
        (v5 && header.biSize != sizeof(BITMAPV5HEADER)) || header.biSize > buffer.size() || header.biWidth <= 0 || header.biWidth > 8192 ||
        header.biHeight == 0 || header.biHeight < -8192 || header.biHeight > 8192 || header.biPlanes != 1u || header.biClrUsed != 0u ||
        (header.biBitCount != 24u && header.biBitCount != 32u) ||
        (header.biCompression != BI_RGB && (header.biCompression != BI_BITFIELDS || header.biBitCount != 32u)))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (header.biSize == sizeof(BITMAPV5HEADER))
    {
        BITMAPV5HEADER extended{};
        std::memcpy(&extended, buffer.data(), sizeof(extended));
        if (extended.bV5ProfileData || extended.bV5ProfileSize)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }
    std::size_t offset = header.biSize;
    if (header.biCompression == BI_BITFIELDS && header.biSize == sizeof(BITMAPINFOHEADER))
        offset += 3u * sizeof(DWORD);
    if (offset > buffer.size())
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    if (header.biCompression == BI_BITFIELDS)
    {
        DWORD masks[3]{};
        const auto maskOffset = header.biSize == sizeof(BITMAPINFOHEADER) ? sizeof(BITMAPINFOHEADER) : offsetof(BITMAPV4HEADER, bV4RedMask);
        std::memcpy(masks, buffer.data() + maskOffset, sizeof(masks));
        if (! masks[0] || ! masks[1] || ! masks[2] || (masks[0] & masks[1]) || (masks[0] & masks[2]) || (masks[1] & masks[2]))
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        if (header.biSize >= sizeof(BITMAPV4HEADER))
        {
            DWORD alpha{};
            std::memcpy(&alpha, buffer.data() + offsetof(BITMAPV4HEADER, bV4AlphaMask), sizeof(alpha));
            if (alpha & (masks[0] | masks[1] | masks[2]))
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }
    }
    const auto height = static_cast<std::size_t>(header.biHeight < 0 ? -header.biHeight : header.biHeight);
    const auto stride = ((static_cast<std::size_t>(header.biWidth) * header.biBitCount + 31u) / 32u) * 4u;
    if (height > (TextClipboardContent::MaximumBitmapBytes - offset) / stride)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    const auto pixels = stride * height;
    if (pixels > buffer.size() - offset || (header.biSizeImage && header.biSizeImage != pixels))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    std::vector<std::byte> staged(0u);
    staged.assign(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(offset + pixels));
    content.bitmapDib.swap(staged);
    content.bitmapV5 = v5;
    return S_OK;
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
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
HRESULT ReadDataObjectFilePaths(IDataObject* dataObject, TextClipboardContent& content) noexcept
try
{
    if (! dataObject)
        return E_INVALIDARG;
    FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    const HRESULT hr = dataObject->GetData(&format, &medium);
    if (FAILED(hr))
        return hr;
    const auto release = wil::scope_exit([&] noexcept { ReleaseStgMedium(&medium); });
    if (medium.tymed != TYMED_HGLOBAL || ! medium.hGlobal)
        return DV_E_TYMED;
    constexpr SIZE_T maximumBytes = 256u * 1024u;
    const auto bytes              = GlobalSize(medium.hGlobal);
    if (bytes > maximumBytes)
        return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    if (bytes < sizeof(DROPFILES))
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    const auto* data = static_cast<const std::byte*>(GlobalLock(medium.hGlobal));
    if (! data)
    {
        const auto error = GetLastError();
        return HRESULT_FROM_WIN32(error ? error : ERROR_INVALID_DATA);
    }
    const auto unlock = wil::scope_exit([&] noexcept { static_cast<void>(GlobalUnlock(medium.hGlobal)); });
    return DecodeClipboardFilePaths(std::span(data, bytes), content);
}
catch (const std::bad_alloc&)
{
    return E_OUTOFMEMORY;
}
catch (const std::exception&)
{
    // A foreign IDataObject must not propagate an exception across OLE.
    return E_FAIL;
}
HRESULT ReadTextClipboardContent(TextClipboard& clipboard, HWND owner, TextClipboardContent& content, bool includeFiles) noexcept
{
    return ReadTextClipboardContent(clipboard, owner, content, includeFiles, false);
}
HRESULT ReadTextClipboardContent(TextClipboard& clipboard, HWND owner, TextClipboardContent& content, bool includeFiles, bool includeHtml) noexcept
{
    return ReadTextClipboardContent(clipboard, owner, content, includeFiles, includeHtml, false);
}
HRESULT ReadTextClipboardContent(
    TextClipboard& clipboard, HWND owner, TextClipboardContent& content, bool includeFiles, bool includeHtml, bool includeBitmap) noexcept
try
{
    if (includeBitmap)
        if (auto* bitmap = dynamic_cast<BitmapStructuredTextClipboard*>(&clipboard))
            return bitmap->ReadSelectedBitmapContent(owner, content, includeFiles, includeHtml, true);
    if (includeFiles || includeHtml)
    {
        if (auto* selective = dynamic_cast<SelectiveStructuredTextClipboard*>(&clipboard))
            return selective->ReadSelectedContent(owner, content, includeFiles, includeHtml);
        if (auto* structured = dynamic_cast<StructuredTextClipboard*>(&clipboard))
        {
            if (includeFiles && ! includeHtml)
                return structured->ReadContent(owner, content);
            TextClipboardContent staged;
            const auto result = structured->ReadContent(owner, staged);
            if (result != S_OK)
                return result;
            if (! includeFiles && ! staged.filePaths.empty())
                return S_FALSE; // A legacy transport cannot re-read another format within the same ownership attempt.

            content.Swap(staged);
            return S_OK;
        }
    }
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
