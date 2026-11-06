#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace DxUi::RichText::Detail
{
enum class MetadataIssue
{
    None,
    Invalid,
    Capacity,
    DuplicateKey,
    UnresolvedAlias
};

// Source has already passed Markdown's UTF-16 validation. Parse only private
// UTF-8 staging; the published source and ACPs are never rewritten.
bool MetadataUtf8(std::wstring_view source, std::string& utf8);
MetadataIssue ValidateTomlMetadata(std::wstring_view source);
MetadataIssue ValidateYamlMetadata(std::wstring_view source, std::size_t backendBudget = 4u * 1024u * 1024u);
} // namespace DxUi::RichText::Detail
