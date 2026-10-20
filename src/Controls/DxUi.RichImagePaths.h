#pragma once
#include <DxUi/RichDocument.h>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <windows.h>
namespace DxUi::RichText::Detail
{
// Lexical Windows paths only: no filesystem access, process-current-directory
// lookup or codec work. The parser owns one bounded spelling plus index spans.
// Fallible empty containers/copy construction avoid MSVC Debug's noexcept
// filesystem iterator/string proxy allocations during failure injection.
inline bool EqualWindowsPathInsensitive(std::wstring_view a, std::wstring_view b) noexcept
{
    return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(), static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
struct LocalPathComponent
{
    std::size_t first{}, last{};
};
struct ParsedLocalFilePath
{
    ParsedLocalFilePath() : text(0u, L' '), parts(0u)
    {
    }
    ParsedLocalFilePath(const ParsedLocalFilePath&)            = default;
    ParsedLocalFilePath& operator=(const ParsedLocalFilePath&) = default;
    std::wstring text;
    std::vector<LocalPathComponent> parts;
    bool unc{};
    LocalPathComponent server, share;
    std::size_t rootEnd{};
    std::wstring_view Slice(LocalPathComponent value) const noexcept
    {
        return std::wstring_view(text).substr(value.first, value.last - value.first);
    }
};
inline bool ValidLocalPathText(std::wstring_view text) noexcept
{
    if (text.empty() || text.size() > 32767u || text.find_first_of(L"<>\"|?*\r\n") != text.npos || text.find(L'\0') != text.npos)
        return false;
    if (! IsValidUtf16(text))
        return false;
    return std::ranges::none_of(text, [](wchar_t value) { return static_cast<unsigned>(value) < 32u; });
}
inline bool ParseLocalFilePath(std::wstring_view input, ParsedLocalFilePath& result)
{
    if (input.size() > 32767u)
        return false;
    result.text.assign(input);
    result.parts.clear();
    result.unc     = false;
    result.rootEnd = 0u;
    std::ranges::replace(result.text, L'/', L'\\');
    if (result.text.size() >= 8u && EqualWindowsPathInsensitive(std::wstring_view(result.text).substr(0u, 8u), L"\\\\?\\UNC\\"))
    {
        std::wstring temp(0u, L' ');
        temp.append(L"\\\\").append(result.text, 8u);
        result.text.swap(temp);
    }
    else if (result.text.starts_with(L"\\\\?\\"))
        result.text.erase(0u, 4u);
    if (! ValidLocalPathText(result.text) || result.text.back() == L'\\' || result.text.starts_with(L"\\\\.\\"))
        return false;
    const std::wstring_view text(result.text);
    std::size_t at{};
    if (text.size() >= 3u && ((text[0] >= L'A' && text[0] <= L'Z') || (text[0] >= L'a' && text[0] <= L'z')) && text[1] == L':' && text[2] == L'\\')
    {
        result.rootEnd = 2u;
        at             = 3u;
    }
    else if (text.starts_with(L"\\\\"))
    {
        result.unc           = true;
        const auto endServer = text.find(L'\\', 2u);
        if (endServer == text.npos || endServer == 2u)
            return false;
        const auto endShare = text.find(L'\\', endServer + 1u);
        if (endShare == text.npos || endShare == endServer + 1u)
            return false;
        result.server  = {2u, endServer};
        result.share   = {endServer + 1u, endShare};
        result.rootEnd = endShare;
        at             = endShare + 1u;
        if (result.Slice(result.share) == L"." || result.Slice(result.share) == L".." || result.Slice(result.server).find(L':') != std::wstring_view::npos ||
            result.Slice(result.share).find(L':') != std::wstring_view::npos)
            return false;
    }
    else
        return false;
    while (at < text.size())
    {
        const auto found = text.find(L'\\', at);
        const auto end   = found == text.npos ? text.size() : found;
        const auto value = text.substr(at, end - at);
        if (value.find(L':') != text.npos)
            return false;
        if (value == L"." || value == L"..")
        {
            if (end == text.size())
                return false;
            if (value == L"..")
            {
                if (result.parts.empty())
                    return false;
                result.parts.pop_back();
            }
        }
        else if (! value.empty())
        {
            if (result.parts.size() >= 512u)
                throw std::length_error("image path component capacity exceeded");
            result.parts.push_back({at, end});
        }
        if (found == text.npos)
            break;
        at = end + 1u;
    }
    return ! result.parts.empty();
}
inline std::wstring CanonicalLocalFilePath(const ParsedLocalFilePath& path)
{
    std::wstring result(0u, L' ');
    result.append(path.text, 0u, path.rootEnd);
    for (const auto component : path.parts)
        result.append(L"\\").append(path.Slice(component));
    return std::wstring(result);
}
inline std::wstring NormalizeDocumentFilePath(std::wstring_view path)
{
    if (path.empty())
        return std::wstring(0u, L' ');
    ParsedLocalFilePath parsed;
    if (! ParseLocalFilePath(path, parsed))
        throw std::invalid_argument("Document location must be an absolute Unicode file path");
    return CanonicalLocalFilePath(parsed);
}
inline bool HasImageFileExtension(const ParsedLocalFilePath& path)
{
    const auto name = path.Slice(path.parts.back());
    const auto dot  = name.find_last_of(L'.');
    if (dot == name.npos)
        return false;
    const auto extension = name.substr(dot);
    for (const auto type : {L".png", L".jpg", L".jpeg", L".gif", L".bmp", L".tif", L".tiff", L".webp", L".avif", L".heic", L".heif", L".svg"})
        if (EqualWindowsPathInsensitive(extension, type))
            return true;
    return false;
}
inline bool LocalPathsShareRoot(const ParsedLocalFilePath& from, const ParsedLocalFilePath& to) noexcept
{
    if (from.unc != to.unc)
        return false;
    if (from.unc)
        return EqualWindowsPathInsensitive(from.Slice(from.server), to.Slice(to.server)) && from.Slice(from.share) == to.Slice(to.share);
    return EqualWindowsPathInsensitive(std::wstring_view(from.text).substr(0u, 2u), std::wstring_view(to.text).substr(0u, 2u));
}
inline bool AppendDocumentRelativeImagePath(const ParsedLocalFilePath& from, const ParsedLocalFilePath& to, std::wstring& output)
{
    if (! LocalPathsShareRoot(from, to))
        return false;
    const auto parentCount = from.parts.size() - 1u;
    std::size_t common{};
    // Exact component matching also preserves correctness on case-sensitive
    // directories. Drive/server names retain Windows root matching rules.
    while (common < parentCount && common < to.parts.size() && from.Slice(from.parts[common]) == to.Slice(to.parts[common]))
        ++common;
    for (auto index = common; index < parentCount; ++index)
    {
        if (! output.empty())
            output.push_back(L'/');
        output.append(L"..");
    }
    for (auto index = common; index < to.parts.size(); ++index)
    {
        if (! output.empty())
            output.push_back(L'/');
        output.append(to.Slice(to.parts[index]));
    }
    return ! output.empty();
}
inline void AppendUriEncodedByte(std::wstring& result, unsigned byte)
{
    constexpr wchar_t hex[] = L"0123456789ABCDEF";
    result.push_back(L'%');
    result.push_back(hex[(byte >> 4u) & 15u]);
    result.push_back(hex[byte & 15u]);
}
inline void AppendUriPath(std::wstring_view text, std::wstring& result, bool drive = false)
{
    for (std::size_t at = 0; at < text.size(); ++at)
    {
        unsigned c = static_cast<unsigned>(text[at]);
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'.' || c == L'~' || c == L'/' ||
            (drive && at == 1u && c == L':'))
        {
            if (result.size() == Document::MaximumUnits)
                throw std::length_error("image URI capacity exceeded");
            result.push_back(static_cast<wchar_t>(c));
            continue;
        }
        if (c >= 0xd800u && c <= 0xdbffu)
        {
            const auto trail = static_cast<unsigned>(text[++at]);
            c                = 0x10000u + ((c - 0xd800u) << 10u) + trail - 0xdc00u;
        }
        const auto encodedUnits = c < 0x80u ? 3u : c < 0x800u ? 6u : c < 0x10000u ? 9u : 12u;
        if (encodedUnits > Document::MaximumUnits - result.size())
            throw std::length_error("image URI capacity exceeded");
        if (c < 0x80u)
            AppendUriEncodedByte(result, c);
        else if (c < 0x800u)
        {
            AppendUriEncodedByte(result, 0xc0u | (c >> 6u));
            AppendUriEncodedByte(result, 0x80u | (c & 63u));
        }
        else if (c < 0x10000u)
        {
            AppendUriEncodedByte(result, 0xe0u | (c >> 12u));
            AppendUriEncodedByte(result, 0x80u | ((c >> 6u) & 63u));
            AppendUriEncodedByte(result, 0x80u | (c & 63u));
        }
        else
        {
            AppendUriEncodedByte(result, 0xf0u | (c >> 18u));
            AppendUriEncodedByte(result, 0x80u | ((c >> 12u) & 63u));
            AppendUriEncodedByte(result, 0x80u | ((c >> 6u) & 63u));
            AppendUriEncodedByte(result, 0x80u | (c & 63u));
        }
    }
}
inline std::optional<std::wstring> BuildMarkdownImagePath(std::wstring_view document, std::wstring_view clipboard)
{
    while (! clipboard.empty() && (clipboard.back() == L'\r' || clipboard.back() == L'\n'))
        clipboard.remove_suffix(1u);
    if (clipboard.size() > 1u && clipboard.front() == L'"' && clipboard.back() == L'"')
        clipboard = clipboard.substr(1u, clipboard.size() - 2u);
    ParsedLocalFilePath image;
    if (! ParseLocalFilePath(clipboard, image) || ! HasImageFileExtension(image))
        return std::nullopt;
    std::wstring destination(0u, L' '), raw(0u, L' ');
    bool relative{};
    if (! document.empty())
    {
        ParsedLocalFilePath doc;
        if (! ParseLocalFilePath(document, doc))
            throw std::invalid_argument("document location must be an absolute file path");
        relative = AppendDocumentRelativeImagePath(doc, image, raw);
    }
    if (relative)
        AppendUriPath(raw, destination);
    else
    {
        raw.assign(image.text, 0u, image.rootEnd);
        for (const auto part : image.parts)
            raw.append(L"\\").append(image.Slice(part));
        std::ranges::replace(raw, L'\\', L'/');
        destination.append(image.unc ? L"file:" : L"file:///");
        AppendUriPath(raw, destination, ! image.unc);
    }
    const auto filename  = image.Slice(image.parts.back());
    const auto dot       = filename.find_last_of(L'.');
    const auto name      = dot && dot != filename.npos ? filename.substr(0u, dot) : filename;
    std::size_t altUnits = name.size();
    for (const auto c : name)
        if (c >= 33 && c <= 126 && ! ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
            ++altUnits;
    // Check the complete escaped source before allocating its final buffer.
    if (altUnits + 5u > Document::MaximumUnits || destination.size() > Document::MaximumUnits - altUnits - 5u)
        throw std::length_error("image Markdown source capacity exceeded");
    std::wstring result(0u, L' ');
    result.reserve(altUnits + destination.size() + 5u);
    result.append(L"![");
    for (const auto c : name)
    {
        if (c >= 33 && c <= 126 && ! ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
            result.push_back(L'\\');
        result.push_back(c);
    }
    result.append(L"](").append(destination).append(L")");
    if (result.size() > Document::MaximumUnits)
        throw std::length_error("image Markdown source capacity exceeded");
    return std::optional<std::wstring>(std::in_place, result);
}
} // namespace DxUi::RichText::Detail
