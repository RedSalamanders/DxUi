#pragma once
#include <DxUi/RichSemanticDocument.h>
#include <stdexcept>

namespace DxUi::RichText::Detail
{
struct SemanticValueUnit
{
    MarkdownRange source;
    bool generated{}, transformed{};
};
struct SemanticValue
{
    SemanticValue() : text(0u, L' '), units(0u)
    {
    }
    SemanticValue(const SemanticValue&)            = default;
    SemanticValue& operator=(const SemanticValue&) = default;
    std::wstring text;
    std::vector<SemanticValueUnit> units;
    void Append(wchar_t value, MarkdownRange source, bool generated = false, bool transformed = false)
    {
        text.push_back(value);
        units.push_back({source, generated, transformed});
    }
};
inline void NormalizeSemanticLeaf(const MarkdownSyntax& syntax, std::size_t nodeIndex, SemanticValue& result)
{
    result.text.clear();
    result.units.clear();
    const auto source = syntax.Source();
    const auto& node  = syntax.Nodes()[nodeIndex];
    // Until metadata validation/presentation is prepared, show the complete
    // envelope literally, including its fences. Content lines still exclude
    // those fences for the metadata parser; no decoded value edits the source.
    const MarkdownContentLine envelope{node.block.source, 0u};
    const auto lines = node.block.kind == MarkdownBlockKind::FrontMatter ? std::span<const MarkdownContentLine>(&envelope, 1u)
                                                                         : std::span<const MarkdownContentLine>(node.lines);
    for (const auto& line : lines)
    {
        for (unsigned space = 0u; space < line.leadingSpaces; ++space)
            result.Append(L' ', {line.source.first, line.source.first}, true);
        for (auto offset = line.source.first; offset < line.source.last;)
        {
            const auto first = offset;
            if (node.block.kind == MarkdownBlockKind::TableCell && source[offset] == L'\\' && offset + 1u < line.source.last && source[offset + 1u] == L'|')
            {
                result.Append(L'|', {first, offset + 2u}, false, true);
                offset += 2u;
            }
            else if (source[offset] == L'\r')
            {
                ++offset;
                if (offset < line.source.last && source[offset] == L'\n')
                    ++offset;
                result.Append(L'\n', {first, offset}, false, true);
            }
            else
            {
                result.Append(source[offset], {first, offset + 1u});
                ++offset;
            }
        }
    }
}
inline void SliceSemanticValue(const SemanticValue& input, MarkdownRange source, SemanticValue& result)
{
    const auto first = std::lower_bound(input.units.begin(), input.units.end(), source.first, [](const auto& unit, auto offset) {
        return unit.source.last < offset || (unit.source.last == offset && ! unit.generated);
    });
    for (auto entry = first; entry != input.units.end() && entry->source.first < source.last; ++entry)
    {
        const auto index = static_cast<std::size_t>(entry - input.units.begin());
        result.Append(input.text[index], entry->source, entry->generated, entry->transformed);
    }
}
inline void DecodeSemanticText(const MarkdownSyntax& syntax, const SemanticValue& input, SemanticValue& result)
{
    const auto& lexemes = syntax.Lexemes();
    for (std::size_t index = 0u; index < input.units.size();)
    {
        const auto& unit = input.units[index];
        const auto lexeme =
            std::lower_bound(lexemes.begin(), lexemes.end(), unit.source.first, [](const auto& value, auto offset) { return value.source.last <= offset; });
        if (! unit.generated && lexeme != lexemes.end() && lexeme->source.first == unit.source.first &&
            (lexeme->kind == MarkdownLexemeKind::EscapedPunctuation || lexeme->kind == MarkdownLexemeKind::CharacterReference))
        {
            auto end = index;
            while (end < input.units.size() && input.units[end].source.first < lexeme->source.last)
                ++end;
            // A lexical candidate clipped by an inline/URL boundary is literal.
            if (end > index && input.units[end - 1u].source.last == lexeme->source.last)
            {
                for (std::size_t value = 0u; value < lexeme->valueLength; ++value)
                    result.Append(lexeme->value[value], lexeme->source, false, true);
                index = end;
                continue;
            }
        }
        result.Append(input.text[index], unit.source, unit.generated, unit.transformed);
        ++index;
    }
}
inline void ResolveSemanticValue(const MarkdownSyntax& syntax, const SemanticValue& normalized, const MarkdownInline& node, SemanticValue& result)
{
    auto range = node.source;
    if (node.kind == MarkdownInlineKind::Code)
    {
        const auto source = syntax.Source();
        std::size_t fence = 0u;
        while (range.first + fence < range.last && source[range.first + fence] == L'\x60')
            ++fence;
        if (! fence || range.last - range.first < fence * 2u)
            throw std::logic_error("Invalid semantic code delimiter ownership");
        range.first += fence;
        range.last -= fence;
    }
    SemanticValue literal;
    SliceSemanticValue(normalized, range, literal);
    if (node.kind == MarkdownInlineKind::Code)
    {
        for (std::size_t i = 0u; i < literal.text.size(); ++i)
            if (literal.text[i] == L'\n')
            {
                literal.text[i]              = L' ';
                literal.units[i].transformed = true;
            }
        if (literal.text.size() >= 2u && literal.text.front() == L' ' && literal.text.back() == L' ' &&
            literal.text.find_first_not_of(L' ') != std::wstring::npos)
        {
            literal.text.erase(literal.text.begin());
            literal.text.pop_back();
            literal.units.erase(literal.units.begin());
            literal.units.pop_back();
        }
    }
    if (node.kind == MarkdownInlineKind::Html && node.value.size() == literal.text.size() + 1u && node.value.back() == L'\n' &&
        std::wstring_view(node.value).substr(0u, literal.text.size()) == literal.text)
        literal.Append(L'\n', {node.source.last, node.source.last}, true);
    if (literal.text == node.value)
    {
        result = literal;
        return;
    }
    if (node.kind == MarkdownInlineKind::Text)
    {
        SemanticValue decoded;
        DecodeSemanticText(syntax, literal, decoded);
        if (decoded.text == node.value)
        {
            result = decoded;
            return;
        }
        // A partially consumed indentation tab can give generated leading
        // spaces the same enclosing raw range as a following delimiter. Only
        // that verified all-generated prefix may exclude a raw suffix.
        if (! node.value.empty() && literal.text.starts_with(node.value) && literal.units.size() >= node.value.size() &&
            std::all_of(literal.units.begin(), literal.units.begin() + node.value.size(), [](const auto& unit) { return unit.generated; }))
        {
            for (std::size_t i = 0u; i < node.value.size(); ++i)
                result.Append(literal.text[i], literal.units[i].source, true);
            return;
        }
    }
    // Never guess interior source coordinates from an enclosing semantic range.
    throw std::logic_error("Resolved Markdown value does not match its source tokens");
}

class SemanticOutput
{
public:
    SemanticOutput(std::vector<StyleRun>& runs) : text(0u, L' '), spans(0u), runs_(runs)
    {
    }
    std::wstring text;
    std::vector<DisplaySpan> spans;
    void Emit(std::wstring_view value, MarkdownRange source, DisplaySpanKind kind, unsigned flags, unsigned heading)
    {
        if (value.empty())
            return;
        if (value.size() > SourceDisplayMap::MaximumDisplayUnits - text.size())
            throw std::length_error("Semantic display capacity exceeded");
        const auto first      = text.size();
        const auto last       = first + value.size();
        const bool generated  = kind == DisplaySpanKind::Generated;
        const bool sameSource = ! spans.empty() && spans.back().source == source;
        const bool linearJoin = kind == DisplaySpanKind::Source && ! spans.empty() && spans.back().source.last == source.first;
        if (! spans.empty() && spans.back().kind == kind && (linearJoin || (sameSource && kind != DisplaySpanKind::Source)))
        {
            spans.back().display.last = last;
            if (linearJoin)
                spans.back().source.last = source.last;
        }
        else
        {
            if (spans.size() == SourceDisplayMap::MaximumSpans)
                throw std::length_error("Semantic span capacity exceeded");
            spans.push_back({source, {first, last}, kind, ! generated});
        }
        text.append(value);
        if (! runs_.empty() && runs_.back().start + runs_.back().length == first && runs_.back().flags == flags && runs_.back().heading == heading)
            runs_.back().length += value.size();
        else
            runs_.push_back({first, value.size(), flags, heading});
    }
    void Value(const SemanticValue& value, unsigned flags, unsigned heading)
    {
        for (std::size_t i = 0u; i < value.text.size(); ++i)
        {
            const auto& unit = value.units[i];
            Emit(std::wstring_view(value.text).substr(i, 1u),
                 unit.source,
                 unit.generated     ? DisplaySpanKind::Generated
                 : unit.transformed ? DisplaySpanKind::Transformed
                                    : DisplaySpanKind::Source,
                 flags,
                 heading);
        }
    }
    void Generated(std::wstring_view value, std::size_t anchor, unsigned flags = 0u, unsigned heading = 0u)
    {
        Emit(value, {anchor, anchor}, DisplaySpanKind::Generated, flags, heading);
    }

private:
    std::vector<StyleRun>& runs_;
};
} // namespace DxUi::RichText::Detail
