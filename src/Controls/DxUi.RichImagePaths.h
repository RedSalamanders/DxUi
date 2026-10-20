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
inline bool ValidLocalFileComponent(std::wstring_view value) noexcept
{
    if (value.empty() || value.back() == L'.' || value.back() == L' ')
        return false;
    auto base = value.substr(0u, value.find(L'.'));
    while (! base.empty() && base.back() == L' ')
        base.remove_suffix(1u);
    for (const auto* reserved : {L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$"})
        if (EqualWindowsPathInsensitive(base, reserved))
            return false;
    if (base.size() == 4u && (EqualWindowsPathInsensitive(base.substr(0u, 3u), L"COM") || EqualWindowsPathInsensitive(base.substr(0u, 3u), L"LPT")))
    {
        const auto digit = base[3];
        if ((digit >= L'1' && digit <= L'9') || digit == L'\u00b9' || digit == L'\u00b2' || digit == L'\u00b3')
            return false;
    }
    return true;
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
    const bool extended = result.text.starts_with(L"\\\\?\\");
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
            // Extended paths do not have ordinary Win32 dot-segment semantics.
            // Refuse an ambiguous conversion instead of changing its target.
            if (extended)
                return false;
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
            if (! ValidLocalFileComponent(value))
                return false;
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
inline bool SameLocalImagePath(std::wstring_view left, std::wstring_view right) noexcept
{
    if (left.size() >= 2u && right.size() >= 2u && left[1] == L':' && right[1] == L':')
        return EqualWindowsPathInsensitive(left.substr(0u, 2u), right.substr(0u, 2u)) && left.substr(2u) == right.substr(2u);
    if (left.starts_with(L"\\\\") && right.starts_with(L"\\\\"))
    {
        const auto a = left.find(L'\\', 2u), b = right.find(L'\\', 2u);
        return a != left.npos && b != right.npos && EqualWindowsPathInsensitive(left.substr(0u, a), right.substr(0u, b)) && left.substr(a) == right.substr(b);
    }
    return left == right;
}
inline std::optional<std::wstring> DecodeLocalImageUriPath(std::wstring_view value)
{
    if (value.empty() || value.size() > Document::MaximumUnits || ! IsValidUtf16(value) || value.find_first_of(L"?#") != value.npos)
        return std::nullopt;
    const auto bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (! bytes)
        return std::nullopt;
    std::string utf8(static_cast<std::size_t>(bytes), ' ');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), utf8.data(), bytes, nullptr, nullptr) != bytes)
        return std::nullopt;
    const auto hex = [](unsigned char c) noexcept -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };
    std::size_t written{};
    for (std::size_t at = 0u; at < utf8.size(); ++at)
    {
        auto c = static_cast<unsigned char>(utf8[at]);
        if (c == '%')
        {
            if (at + 2u >= utf8.size())
                return std::nullopt;
            const auto high = hex(static_cast<unsigned char>(utf8[at + 1u])), low = hex(static_cast<unsigned char>(utf8[at + 2u]));
            if (high < 0 || low < 0)
                return std::nullopt;
            c = static_cast<unsigned char>(high * 16 + low);
            at += 2u;
            if (c == '/' || c == '\\' || c == ':')
                return std::nullopt;
        }
        if (! c)
            return std::nullopt;
        utf8[written++] = static_cast<char>(c);
    }
    const auto units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(written), nullptr, 0);
    if (! units || units > 32767)
        return std::nullopt;
    std::wstring decoded(static_cast<std::size_t>(units), L' ');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(written), decoded.data(), units) != units)
        return std::nullopt;
    return std::optional<std::wstring>(std::in_place, decoded);
}
// Resolve local file destinations only. Scheme/authority are classified before
// URI decoding; encoded punctuation cannot turn a relative path into a scheme.
inline std::optional<std::wstring> ResolveLocalImagePath(std::wstring_view document, std::wstring_view destination)
{
    std::wstring raw(0u, L' ');
    bool absoluteUri{};
    if (destination.size() >= 5u && EqualWindowsPathInsensitive(destination.substr(0u, 5u), L"file:"))
    {
        absoluteUri = true;
        if (! destination.substr(5u).starts_with(L"//"))
            return std::nullopt;
        auto path = destination.substr(7u);
        if (path.starts_with(L"/"))
        {
            path.remove_prefix(1u);
            if (path.size() < 3u || path[1] != L':' || path[2] != L'/')
                return std::nullopt;
        }
        else
        {
            const auto slash = path.find(L'/');
            if (slash == path.npos || ! slash || path.substr(0u, slash).find_first_of(L"@:%") != path.npos)
                return std::nullopt;
            if (EqualWindowsPathInsensitive(path.substr(0u, slash), L"localhost"))
                path.remove_prefix(slash + 1u);
            else
                raw.assign(L"//");
        }
        raw.append(path);
    }
    else
    {
        const auto colon = destination.find(L':');
        if (colon != destination.npos && ! (colon == 1u && destination.size() > 2u && (destination[2] == L'/' || destination[2] == L'\\')))
            return std::nullopt;
        raw.assign(destination);
    }
    const auto decoded = DecodeLocalImageUriPath(raw);
    if (! decoded)
        return std::nullopt;
    ParsedLocalFilePath absolute;
    if (ParseLocalFilePath(*decoded, absolute))
    {
        const auto canonical = CanonicalLocalFilePath(absolute);
        return std::optional<std::wstring>(std::in_place, canonical);
    }
    // A failed rooted spelling is never reinterpreted under the document base.
    if (absoluteUri || decoded->find(L':') != decoded->npos || decoded->starts_with(L"/") || decoded->starts_with(L"\\") || document.empty())
        return std::nullopt;
    const auto doc   = NormalizeDocumentFilePath(document);
    const auto slash = doc.find_last_of(L'\\');
    if (slash == doc.npos || decoded->size() > 32767u - slash - 1u)
        return std::nullopt;
    std::wstring joined(doc, 0u, slash + 1u);
    joined.append(*decoded);
    ParsedLocalFilePath relative;
    if (! ParseLocalFilePath(joined, relative))
        return std::nullopt;
    const auto canonical = CanonicalLocalFilePath(relative);
    return std::optional<std::wstring>(std::in_place, canonical);
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
inline bool AppendDocumentRelativeFilePath(const ParsedLocalFilePath& from, const ParsedLocalFilePath& to, std::wstring& output)
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
// Recognized image suffixes produce displayed images; every other absolute
// file path produces a text link. Both use the same lexical destination rules.
// No existence/type query, file copy or link activation occurs during paste.
inline std::optional<std::wstring> BuildMarkdownFileReference(std::wstring_view document, std::wstring_view clipboard)
{
    while (! clipboard.empty() && (clipboard.back() == L'\r' || clipboard.back() == L'\n'))
        clipboard.remove_suffix(1u);
    if (clipboard.size() > 1u && clipboard.front() == L'"' && clipboard.back() == L'"')
        clipboard = clipboard.substr(1u, clipboard.size() - 2u);
    ParsedLocalFilePath file;
    if (! ParseLocalFilePath(clipboard, file))
        return std::nullopt;
    const bool image = HasImageFileExtension(file);
    std::wstring destination(0u, L' '), raw(0u, L' ');
    bool relative{};
    if (! document.empty())
    {
        ParsedLocalFilePath doc;
        if (! ParseLocalFilePath(document, doc))
            throw std::invalid_argument("document location must be an absolute file path");
        relative = AppendDocumentRelativeFilePath(doc, file, raw);
    }
    if (relative)
        AppendUriPath(raw, destination);
    else
    {
        raw.assign(file.text, 0u, file.rootEnd);
        for (const auto part : file.parts)
            raw.append(L"\\").append(file.Slice(part));
        std::ranges::replace(raw, L'\\', L'/');
        destination.append(file.unc ? L"file:" : L"file:///");
        AppendUriPath(raw, destination, ! file.unc);
    }
    const auto filename    = file.Slice(file.parts.back());
    const auto dot         = filename.find_last_of(L'.');
    const auto name        = image && dot && dot != filename.npos ? filename.substr(0u, dot) : filename;
    std::size_t labelUnits = name.size();
    for (const auto c : name)
        if (c >= 33 && c <= 126 && ! ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
            ++labelUnits;
    // Check the complete escaped source before allocating its final buffer.
    const auto markerUnits = image ? 5u : 4u;
    if (labelUnits + markerUnits > Document::MaximumUnits || destination.size() > Document::MaximumUnits - labelUnits - markerUnits)
        throw std::length_error("file reference Markdown source capacity exceeded");
    std::wstring result(0u, L' ');
    result.reserve(labelUnits + destination.size() + markerUnits);
    result.append(image ? L"![" : L"[");
    for (const auto c : name)
    {
        if (c >= 33 && c <= 126 && ! ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')))
            result.push_back(L'\\');
        result.push_back(c);
    }
    result.append(L"](").append(destination).append(L")");
    if (result.size() > Document::MaximumUnits)
        throw std::length_error("file reference Markdown source capacity exceeded");
    return std::optional<std::wstring>(std::in_place, result);
}
} // namespace DxUi::RichText::Detail
