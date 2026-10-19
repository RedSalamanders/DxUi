#pragma once
#include <DxUi/MarkdownSyntax.h>

namespace DxUi::RichText::Detail
{
// Shared pinned reference table and scalar replacement rules, not a second
// entity decoder for the native HTML projection.
bool DecodeMarkdownCharacterReference(std::wstring_view input, std::size_t first, MarkdownLexeme& token) noexcept;
} // namespace DxUi::RichText::Detail
