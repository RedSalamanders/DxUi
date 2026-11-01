#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <functional>
#include <string_view>
#include <vector>

namespace DxUi::RichText
{
// Token coordinates are UTF-16 offsets relative to the borrowed code text.
// Returned ranges must be sorted, disjoint, nonempty, and inside that text.
enum class CodeTokenRole : unsigned char
{
    Comment,
    Keyword,
    Literal
};
struct CodeToken
{
    MarkdownRange text;
    CodeTokenRole role{};
};
// Called on the owner thread during preparation. Both views are borrowed for
// the duration of the call. Return false to use ordinary uncolored code.
using CodeTokenizer                            = std::function<bool(std::wstring_view language, std::wstring_view text, std::vector<CodeToken>& tokens)>;
inline constexpr std::size_t MaximumCodeTokens = 4096u;
// Built-in JSON, C-like, PowerShell and diff tokenization. False leaves output
// empty for an unknown language or an input exceeding the bounded run count.
[[nodiscard]] bool TokenizeBuiltinCode(std::wstring_view language, std::wstring_view text, std::vector<CodeToken>& tokens);
} // namespace DxUi::RichText
