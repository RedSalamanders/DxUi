#pragma once
#include <DxUi/RichCodeTokenizer.h>
#include <algorithm>

namespace DxUi::RichText::Detail
{
inline bool CodeAsciiEqual(std::wstring_view a, std::wstring_view b) noexcept
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](wchar_t x, wchar_t y) {
        return (x >= L'A' && x <= L'Z' ? x + (L'a' - L'A') : x) == y;
    });
}
inline bool TokenizeCode(std::wstring_view language, std::wstring_view text, std::vector<CodeToken>& output)
{
    const bool json = CodeAsciiEqual(language, L"json");
    const bool shell = CodeAsciiEqual(language, L"powershell") || CodeAsciiEqual(language, L"pwsh") || CodeAsciiEqual(language, L"ps1");
    const bool diff = CodeAsciiEqual(language, L"diff") || CodeAsciiEqual(language, L"patch");
    const bool c = CodeAsciiEqual(language, L"c") || CodeAsciiEqual(language, L"cpp") || CodeAsciiEqual(language, L"c++") ||
                   CodeAsciiEqual(language, L"csharp") || CodeAsciiEqual(language, L"cs") || CodeAsciiEqual(language, L"javascript") ||
                   CodeAsciiEqual(language, L"js") || CodeAsciiEqual(language, L"typescript") || CodeAsciiEqual(language, L"ts");
    std::vector<CodeToken> staged(0u);
    if (! json && ! shell && ! diff && ! c)
    {
        output.clear();
        return false;
    }
    bool overflow = false;
    const auto append = [&](std::size_t first, std::size_t last, CodeTokenRole role)
    {
        if (staged.size() == MaximumCodeTokens)
        {
            overflow = true;
            return false;
        }
        staged.push_back({{first, last}, role});
        return true;
    };
    const auto word = [](wchar_t ch) { return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9') || ch == L'_'; };
    const auto keyword = [](std::wstring_view value)
    {
        constexpr std::wstring_view words[] = {L"true", L"false", L"null", L"if", L"else", L"for", L"while", L"return", L"class", L"struct",
                                               L"const", L"auto", L"void", L"int", L"bool", L"namespace", L"using", L"function", L"param", L"throw",
                                               L"try", L"catch", L"public", L"private", L"static", L"new", L"var", L"let", L"def", L"switch"};
        return std::ranges::find(words, value) != std::end(words);
    };
    for (std::size_t i = 0u; i < text.size();)
    {
        const auto first = i;
        if (diff && (i == 0u || text[i - 1u] == L'\n' || text[i - 1u] == L'\r'))
        {
            while (i < text.size() && text[i] != L'\r' && text[i] != L'\n')
                ++i;
            if (i > first && (text[first] == L'+' || text[first] == L'-' || text[first] == L'@') &&
                ! append(first, i, text[first] == L'@' ? CodeTokenRole::Keyword : CodeTokenRole::Literal))
                break;
            if (i == first)
                ++i;
            continue;
        }
        if ((shell && text[i] == L'#') || (c && text[i] == L'/' && i + 1u < text.size() && text[i + 1u] == L'/'))
        {
            while (i < text.size() && text[i] != L'\r' && text[i] != L'\n')
                ++i;
            if (! append(first, i, CodeTokenRole::Comment))
                break;
            continue;
        }
        if (c && text[i] == L'/' && i + 1u < text.size() && text[i + 1u] == L'*')
        {
            i += 2u;
            while (i + 1u < text.size() && !(text[i] == L'*' && text[i + 1u] == L'/'))
                ++i;
            i = i + 1u < text.size() ? i + 2u : text.size();
            if (! append(first, i, CodeTokenRole::Comment))
                break;
            continue;
        }
        if (text[i] == L'"' || (! json && text[i] == L'\''))
        {
            const auto quote = text[i++];
            while (i < text.size())
            {
                if (text[i] == (shell ? L'`' : L'\\') && i + 1u < text.size())
                    i += 2u;
                else if (text[i++] == quote)
                    break;
            }
            if (! append(first, i, CodeTokenRole::Literal))
                break;
            continue;
        }
        if (word(text[i]))
        {
            while (i < text.size() && word(text[i]))
                ++i;
            const auto value = text.substr(first, i - first);
            if ((keyword(value) || (text[first] >= L'0' && text[first] <= L'9')) &&
                ! append(first, i, keyword(value) ? CodeTokenRole::Keyword : CodeTokenRole::Literal))
                break;
            continue;
        }
        ++i;
    }
    if (overflow)
    {
        output.clear();
        return false;
    }
    output.swap(staged);
    return true;
}
} // namespace DxUi::RichText::Detail
