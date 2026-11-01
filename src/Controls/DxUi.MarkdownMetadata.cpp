#include "DxUi.MarkdownMetadata.h"

// Keep this implementation in DxUi.lib, with no TOML runtime DLL dependency.
#define TOML_HEADER_ONLY 1
#define TOML_ENABLE_FORMATTERS 0
#define TOML_ENABLE_UNRELEASED_FEATURES 0
#define TOML_MAX_NESTED_VALUES 128
#include <cstdint>
#include <string>
// The private header must resolve from the exact pinned repository, without
// adding a vendor search path to the consumer compiler profile.
#pragma warning(push)
#pragma warning(disable : 4464)
#define toml dxui_private_toml
#include "../../ThirdParty/tomlplusplus/3.4.0/toml.hpp"
#undef toml
#pragma warning(pop)

namespace DxUi::RichText::Detail
{
bool MetadataUtf8(std::wstring_view source, std::string& utf8)
{
    // Markdown admission guarantees paired surrogates. The source limit bounds
    // conversion to at most 3 UTF-8 bytes per UTF-16 unit. Do not normalize EOLs.
    utf8.clear();
    utf8.reserve(source.size());
    for (std::size_t index = 0; index < source.size(); ++index)
    {
        auto scalar = static_cast<std::uint32_t>(source[index]);
        if (scalar == 0u)
            return false;
        if (scalar >= 0xd800u && scalar <= 0xdbffu)
        {
            const auto trail = static_cast<std::uint32_t>(source[++index]);
            scalar           = 0x10000u + ((scalar - 0xd800u) << 10u) + (trail - 0xdc00u);
        }
        if (scalar < 0x80u)
            utf8.push_back(static_cast<char>(scalar));
        else if (scalar < 0x800u)
        {
            utf8.push_back(static_cast<char>(0xc0u | (scalar >> 6u)));
            utf8.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
        }
        else if (scalar < 0x10000u)
        {
            utf8.push_back(static_cast<char>(0xe0u | (scalar >> 12u)));
            utf8.push_back(static_cast<char>(0x80u | ((scalar >> 6u) & 0x3fu)));
            utf8.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
        }
        else
        {
            utf8.push_back(static_cast<char>(0xf0u | (scalar >> 18u)));
            utf8.push_back(static_cast<char>(0x80u | ((scalar >> 12u) & 0x3fu)));
            utf8.push_back(static_cast<char>(0x80u | ((scalar >> 6u) & 0x3fu)));
            utf8.push_back(static_cast<char>(0x80u | (scalar & 0x3fu)));
        }
    }
    return true;
}

MetadataIssue ValidateTomlMetadata(std::wstring_view source)
{
    std::string utf8;
    if (! MetadataUtf8(source, utf8))
        return MetadataIssue::Invalid;
    try
    {
        const auto document = dxui_private_toml::parse(utf8);
        static_cast<void>(document);
        return MetadataIssue::None;
    }
    catch (const dxui_private_toml::parse_error& error)
    {
        // toml++ 3.4.0 identifies its bounded-depth refusal in this message.
        // Allocation failures are not parse_error and propagate to the staging owner.
        return (error.description() == "Error while parsing value: exceeded maximum nested value depth of 128 (TOML_MAX_NESTED_VALUES)" ||
                error.description() == "Error while parsing key: exceeded maximum dotted key depth of 128 (DXUI_TOML_MAX_KEY_DEPTH)" ||
                error.description() == "Error while parsing key: exceeded maximum key segment count of 1024 (DXUI_TOML_MAX_KEY_SEGMENTS)")
                   ? MetadataIssue::Capacity
                   : MetadataIssue::Invalid;
    }
}
} // namespace DxUi::RichText::Detail
