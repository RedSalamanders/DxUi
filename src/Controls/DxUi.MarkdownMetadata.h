#pragma once

#include <string_view>

namespace DxUi::RichText::Detail
{
enum class MetadataIssue
{
    None,
    Invalid,
    Capacity
};

// Source has already passed Markdown's UTF-16 validation. Parse only private
// UTF-8 staging; the published source and ACPs are never rewritten.
MetadataIssue ValidateTomlMetadata(std::wstring_view source);
} // namespace DxUi::RichText::Detail
