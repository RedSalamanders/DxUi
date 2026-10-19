#pragma once
#include "DxUi.MarkdownCharacterReference.h"
#include "DxUi.RichSemanticValues.h"

namespace DxUi::RichText::Detail
{
enum class NativeHtmlKind
{
    B,
    Strong,
    I,
    Em,
    U,
    S,
    Del,
    Code,
    Span,
    A,
    Br,
    P,
    Div,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6,
    Pre
};
inline bool HtmlSpace(wchar_t value) noexcept
{
    return value == L' ' || value == L'\t' || value == L'\r' || value == L'\n' || value == L'\f';
}
inline wchar_t HtmlLower(wchar_t value) noexcept
{
    return value >= L'A' && value <= L'Z' ? static_cast<wchar_t>(value + (L'a' - L'A')) : value;
}
inline bool HtmlName(std::wstring_view value, std::wstring_view expected) noexcept
{
    return value.size() == expected.size() && std::equal(value.begin(), value.end(), expected.begin(), [](auto a, auto b) { return HtmlLower(a) == b; });
}
inline unsigned HtmlFlags(NativeHtmlKind kind) noexcept
{
    switch (kind)
    {
        case NativeHtmlKind::B:
        case NativeHtmlKind::Strong: return Bold;
        case NativeHtmlKind::I:
        case NativeHtmlKind::Em: return Italic;
        case NativeHtmlKind::U: return Underline;
        case NativeHtmlKind::S:
        case NativeHtmlKind::Del: return Strike;
        case NativeHtmlKind::Code:
        case NativeHtmlKind::Pre: return Code;
        case NativeHtmlKind::A: return Link;
        default: return 0u;
    }
}
inline unsigned HtmlHeading(NativeHtmlKind kind) noexcept
{
    return kind >= NativeHtmlKind::H1 && kind <= NativeHtmlKind::H6 ? static_cast<unsigned>(kind) - static_cast<unsigned>(NativeHtmlKind::H1) + 1u : 0u;
}
inline bool HtmlBlockWrapper(NativeHtmlKind kind) noexcept
{
    return kind >= NativeHtmlKind::P;
}
inline std::wstring DecodeHtmlAttribute(std::wstring_view input)
{
    std::wstring result(0u, L' ');
    for (std::size_t at = 0u; at < input.size();)
    {
        MarkdownLexeme reference;
        if (input[at] == L'&' && DecodeMarkdownCharacterReference(input, at, reference))
        {
            result.append(reference.value.data(), reference.valueLength);
            at = reference.source.last;
        }
        else
            result.push_back(input[at++]);
    }
    return std::wstring(result);
}
inline bool NativeHtmlDestination(std::wstring_view value) noexcept
{
    // A presentation policy, not an URL resolver. No destination is executed.
    // Decode references before checking schemes/control obfuscation.
    for (auto character : value)
        if (character <= L' ' || (character >= 0x7f && character <= 0x9f) || character == L'\\' || character == L'<' || character == L'>' ||
            character == L'\"' || character == L'\'')
            return false;
    const auto colon = value.find(L':'), path = value.find_first_of(L"/?#");
    if (colon == std::wstring_view::npos || (path != std::wstring_view::npos && path < colon))
        return true;
    const auto scheme = value.substr(0u, colon);
    return HtmlName(scheme, L"http") || HtmlName(scheme, L"https") || HtmlName(scheme, L"mailto");
}
struct NativeHtmlTag
{
    NativeHtmlTag() : destination(0u, L' '), title(0u, L' ')
    {
    }
    NativeHtmlTag(const NativeHtmlTag&)            = default;
    NativeHtmlTag& operator=(const NativeHtmlTag&) = default;
    NativeHtmlKind kind{};
    MarkdownRange source; // Original UTF-16 ACPs, including raw CRLF within a tag.
    // first/last are normalized block-leaf offsets; mate is a tag-vector index.
    std::size_t first{}, last{}, mate = MarkdownNoNode;
    bool closing{};
    unsigned flagsAfter{}, headingAfter{};
    std::wstring destination, title;
};
inline bool ParseNativeHtmlTag(std::wstring_view value, NativeHtmlTag& tag)
{
    constexpr std::wstring_view names[] = {L"b",  L"strong", L"i",   L"em", L"u",  L"s",  L"del", L"code", L"span", L"a",
                                           L"br", L"p",      L"div", L"h1", L"h2", L"h3", L"h4",  L"h5",   L"h6",   L"pre"};
    if (value.size() < 3u || value.front() != L'<' || value.back() != L'>')
        return false;
    std::size_t at = 1u;
    if (value[at] == L'/')
    {
        tag.closing = true;
        ++at;
    }
    const auto first = at;
    while (at < value.size() && ((HtmlLower(value[at]) >= L'a' && HtmlLower(value[at]) <= L'z') || (value[at] >= L'0' && value[at] <= L'9')))
        ++at;
    const auto found = std::find_if(std::begin(names), std::end(names), [&](auto name) { return HtmlName(value.substr(first, at - first), name); });
    if (found == std::end(names))
        return false;
    tag.kind  = static_cast<NativeHtmlKind>(found - std::begin(names));
    bool href = false, title = false;
    for (;;)
    {
        const auto before = at;
        while (at < value.size() && HtmlSpace(value[at]))
            ++at;
        if (at == value.size() - 1u)
            return tag.kind != NativeHtmlKind::A || tag.closing || href;
        if (value[at] == L'/')
            return ! tag.closing && tag.kind == NativeHtmlKind::Br && at + 1u == value.size() - 1u;
        if (at == before || tag.closing || tag.kind != NativeHtmlKind::A)
            return false;
        const auto nameFirst = at;
        while (at < value.size() && HtmlLower(value[at]) >= L'a' && HtmlLower(value[at]) <= L'z')
            ++at;
        const auto name   = value.substr(nameFirst, at - nameFirst);
        const bool isHref = HtmlName(name, L"href"), isTitle = HtmlName(name, L"title");
        if ((! isHref && ! isTitle) || (isHref ? href : title))
            return false;
        while (at < value.size() && HtmlSpace(value[at]))
            ++at;
        if (at == value.size() || value[at++] != L'=')
            return false;
        while (at < value.size() && HtmlSpace(value[at]))
            ++at;
        if (at == value.size() || (value[at] != L'\'' && value[at] != L'\"'))
            return false;
        const auto quote     = value[at++];
        const auto textFirst = at;
        while (at < value.size() && value[at] != quote)
            ++at;
        if (at == value.size())
            return false;
        const auto decoded = DecodeHtmlAttribute(value.substr(textFirst, at - textFirst));
        ++at;
        if (isHref)
        {
            if (! NativeHtmlDestination(decoded))
                return false;
            tag.destination = decoded;
            href            = true;
        }
        else
        {
            tag.title = decoded;
            title     = true;
        }
    }
}

// One leaf is validated completely before any tag is hidden. There is no HTML
// recovery tree: unsupported attributes/tags or crossed/unclosed nesting retain
// that leaf's literal normalized source. Limits are policy, not parser grammar.
struct NativeHtmlPlan
{
    static constexpr std::size_t MaximumTags = 4096u, MaximumDepth = 32u;
    NativeHtmlPlan() : tags(0u)
    {
    }
    std::vector<NativeHtmlTag> tags;
    bool present{}, valid{};
    unsigned flags{}, heading{};
    std::size_t linkElement = MarkdownNoNode, emptyInsertion{}, bodyFirst{}, bodyLast{};
    std::optional<NativeHtmlKind> wrapper;
    void Reset()
    {
        tags.clear();
        present = valid = false;
        flags = heading = 0u;
        linkElement     = MarkdownNoNode;
        emptyInsertion = bodyFirst = bodyLast = 0u;
        wrapper.reset();
    }
    bool Validate(bool block, std::wstring_view text)
    {
        std::vector<std::size_t> stack(0u);
        unsigned activeFlags = 0u, activeHeading = 0u;
        bool anchor = false;
        for (std::size_t index = 0u; index < tags.size(); ++index)
        {
            auto& tag = tags[index];
            if (tag.kind == NativeHtmlKind::Br)
            {
                if (tag.closing)
                    return false;
            }
            else if (tag.closing)
            {
                if (stack.empty() || tags[stack.back()].kind != tag.kind)
                    return false;
                const auto opening = stack.back();
                stack.pop_back();
                tag.mate           = opening;
                tags[opening].mate = index;
                if (tag.kind == NativeHtmlKind::A)
                    anchor = false;
                if (HtmlBlockWrapper(tag.kind))
                {
                    bodyLast = tag.first;
                    if (! std::all_of(text.begin() + static_cast<std::ptrdiff_t>(tag.last), text.end(), HtmlSpace))
                        return false;
                }
                activeFlags   = stack.empty() ? 0u : tags[stack.back()].flagsAfter;
                activeHeading = stack.empty() ? 0u : tags[stack.back()].headingAfter;
            }
            else
            {
                if (stack.size() == MaximumDepth)
                    return false;
                if (HtmlBlockWrapper(tag.kind))
                {
                    if (! block || wrapper || ! stack.empty() || ! std::all_of(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(tag.first), HtmlSpace))
                        return false;
                    wrapper   = tag.kind;
                    bodyFirst = tag.last;
                }
                if (tag.kind == NativeHtmlKind::A)
                {
                    if (anchor)
                        return false;
                    anchor = true;
                }
                activeFlags |= HtmlFlags(tag.kind);
                if (HtmlHeading(tag.kind))
                    activeHeading = HtmlHeading(tag.kind);
                stack.push_back(index);
                emptyInsertion = tag.source.last;
            }
            tag.flagsAfter   = activeFlags;
            tag.headingAfter = activeHeading;
        }
        return stack.empty();
    }
    void Inline(const MarkdownSyntax& syntax, const MarkdownNode& leaf, const SemanticValue& normalized)
    {
        Reset();
        if (normalized.text.find(L'<') == std::wstring::npos || leaf.firstInline == MarkdownNoNode)
            return;
        std::vector<std::size_t> pending(1u, leaf.firstInline);
        std::vector<MarkdownRange> markdownLinks(0u);
        const auto& inlines = syntax.Inlines();
        while (! pending.empty())
        {
            const auto index = pending.back();
            pending.pop_back();
            const auto& node = inlines[index];
            if (node.nextSibling != MarkdownNoNode)
                pending.push_back(node.nextSibling);
            if (node.kind == MarkdownInlineKind::Image)
                continue; // Alternatives are atomic literal labels.
            if (node.firstChild != MarkdownNoNode)
                pending.push_back(node.firstChild);
            if (node.kind == MarkdownInlineKind::Link)
                markdownLinks.push_back(node.source);
            if (node.kind != MarkdownInlineKind::Html)
                continue;
            present = true;
            NativeHtmlTag tag;
            tag.source = node.source;
            if (tags.size() == MaximumTags || ! ParseNativeHtmlTag(node.value, tag))
                return;
            tags.push_back(tag);
        }
        if (! present || ! Validate(false, {}))
            return;
        // Markdown and native anchors must not overlap. Sorting keeps admission
        // O(n log n), rather than comparing every tag with every source link.
        std::sort(markdownLinks.begin(), markdownLinks.end(), [](auto a, auto b) { return a.first < b.first; });
        for (const auto& tag : tags)
            if (tag.kind == NativeHtmlKind::A && ! tag.closing)
            {
                const auto last = tags[tag.mate].source.last;
                const auto next =
                    std::lower_bound(markdownLinks.begin(), markdownLinks.end(), tag.source.first, [](auto range, auto first) { return range.last <= first; });
                if (next != markdownLinks.end() && next->first < last)
                    return;
            }
        valid = true;
    }
    void Block(const SemanticValue& input)
    {
        Reset();
        present  = true;
        bodyLast = input.text.size();
        for (std::size_t at = 0u; at < input.text.size();)
        {
            if (input.text[at] != L'<')
            {
                ++at;
                continue;
            }
            NativeHtmlTag tag;
            tag.first     = at;
            wchar_t quote = 0;
            for (++at; at < input.text.size(); ++at)
            {
                const auto character = input.text[at];
                if (quote)
                {
                    if (character == quote)
                        quote = 0;
                }
                else if (character == L'\'' || character == L'\"')
                    quote = character;
                else if (character == L'>')
                    break;
                else if (character == L'<')
                    return;
            }
            if (at == input.text.size())
                return;
            tag.last   = ++at;
            tag.source = {input.units[tag.first].source.first, input.units[tag.last - 1u].source.last};
            if (tags.size() == MaximumTags || ! ParseNativeHtmlTag(std::wstring_view(input.text).substr(tag.first, tag.last - tag.first), tag))
                return;
            tags.push_back(tag);
        }
        valid = ! tags.empty() && Validate(true, input.text);
    }
    const NativeHtmlTag& At(MarkdownRange source) const
    {
        const auto found = std::lower_bound(tags.begin(), tags.end(), source.first, [](const auto& tag, auto first) { return tag.source.first < first; });
        if (found == tags.end() || found->source != source)
            throw std::logic_error("Native HTML token ownership mismatch");
        return *found;
    }
};
inline void NativeHtmlText(const SemanticValue& input, std::size_t first, std::size_t last, SemanticOutput& output, unsigned flags, unsigned heading)
{
    for (auto at = first; at < last;)
    {
        MarkdownLexeme reference;
        if (input.text[at] == L'&' && DecodeMarkdownCharacterReference(input.text, at, reference) && reference.source.last <= last)
        {
            output.Emit(std::wstring_view(reference.value.data(), reference.valueLength),
                        {input.units[at].source.first, input.units[reference.source.last - 1u].source.last},
                        DisplaySpanKind::Transformed,
                        flags,
                        heading);
            at = reference.source.last;
        }
        else
        {
            const auto& unit = input.units[at];
            output.Emit(std::wstring_view(input.text).substr(at, 1u),
                        unit.source,
                        unit.generated     ? DisplaySpanKind::Generated
                        : unit.transformed ? DisplaySpanKind::Transformed
                                           : DisplaySpanKind::Source,
                        flags,
                        heading);
            ++at;
        }
    }
}
} // namespace DxUi::RichText::Detail
