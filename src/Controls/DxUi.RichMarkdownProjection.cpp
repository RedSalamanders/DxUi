#include <DxUi/RichMarkdown.h>
#include <stdexcept>
namespace DxUi::RichText
{
std::size_t Projection::ToDisplay(std::size_t offset) const
{
    if (offset > source.size())
        throw std::out_of_range("Source offset");
    const auto result = sourceToDisplay.at(offset);
    if (result > text.size())
        throw std::invalid_argument("Invalid source mapping");
    return result;
}
std::size_t Projection::ToSource(std::size_t offset, bool trailing) const
{
    if (offset > text.size())
        throw std::out_of_range("Display offset");
    if (units.size() != text.size())
        throw std::invalid_argument("Invalid display mapping");
    if (units.empty())
        return source.size();
    const bool useEnd = offset == units.size() || (trailing && offset > 0);
    const auto& unit  = units.at(useEnd ? offset - 1 : offset);
    if (unit.start >= unit.end || unit.end > source.size())
        throw std::invalid_argument("Invalid display source range");
    return useEnd ? unit.end : unit.start;
}
namespace
{
class Builder
{
public:
    Projection result;
    void Hide(std::size_t first, std::size_t last)
    {
        for (auto i = first; i <= last; ++i)
            result.sourceToDisplay[i] = result.text.size();
    }
    void Emit(std::size_t first, std::size_t last, unsigned flags, unsigned heading)
    {
        const auto start = result.text.size();
        for (auto i = first; i < last; ++i)
        {
            result.sourceToDisplay[i] = result.text.size();
            auto end                  = i + 1;
            if (result.source[i] == L'\r' && end < last && result.source[end] == L'\n')
            {
                result.sourceToDisplay[end] = result.text.size();
                ++end;
            }
            result.text.push_back(result.source[i] == L'\r' ? L'\n' : result.source[i]);
            result.units.push_back({i, end});
            i = end - 1;
        }
        result.sourceToDisplay[last] = result.text.size();
        if (start != result.text.size())
        {
            if (! result.runs.empty() && result.runs.back().flags == flags && result.runs.back().heading == heading &&
                result.runs.back().start + result.runs.back().length == start)
                result.runs.back().length = result.text.size() - result.runs.back().start;
            else
                result.runs.push_back({start, result.text.size() - start, flags, heading});
        }
    }
    void Inline(std::size_t first, std::size_t last, unsigned flags, unsigned heading, unsigned depth = 0)
    {
        auto cursor = first;
        while (cursor < last)
        {
            if (depth < 16 && (result.source[cursor] == L'*' || result.source[cursor] == L'`' || result.source[cursor] == L'~'))
            {
                std::wstring marker(1, result.source[cursor]);
                unsigned effect = Italic;
                if (result.source[cursor] == L'`')
                    effect = Code;
                else if (cursor + 1 < last && result.source[cursor + 1] == result.source[cursor])
                {
                    marker.push_back(result.source[cursor]);
                    effect = result.source[cursor] == L'*' ? Bold : Strike;
                }
                else if (result.source[cursor] == L'~')
                {
                    Emit(cursor, cursor + 1, flags, heading);
                    ++cursor;
                    continue;
                }
                const auto close = result.source.find(marker, cursor + marker.size());
                if (close != std::wstring::npos && close < last && close > cursor + marker.size())
                {
                    Hide(cursor, cursor + marker.size());
                    if (effect == Code)
                        Emit(cursor + marker.size(), close, flags | effect, heading);
                    else
                        Inline(cursor + marker.size(), close, flags | effect, heading, depth + 1);
                    Hide(close, close + marker.size());
                    cursor = close + marker.size();
                    continue;
                }
            }
            Emit(cursor, cursor + 1, flags, heading);
            ++cursor;
        }
    }
};
} // namespace
Projection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode)
{
    if (mode != ViewMode::Source && mode != ViewMode::Rich)
        throw std::invalid_argument("Unknown rich-text view mode");
    Builder builder;
    auto& result  = builder.result;
    result.source = source.Text();
    result.sourceToDisplay.resize(result.source.size() + 1);
    result.revision = revision;
    result.mode     = mode;
    if (mode == ViewMode::Source)
        builder.Emit(0, result.source.size(), Regular, 0);
    else
    {
        std::size_t first = 0;
        while (first < result.source.size())
        {
            auto last = result.source.find_first_of(L"\r\n", first);
            if (last == std::wstring::npos)
                last = result.source.size();
            auto content     = first;
            unsigned heading = 0;
            while (content < last && result.source[content] == L'#' && heading < 6)
            {
                ++heading;
                ++content;
            }
            if (heading && content < last && result.source[content] == L' ')
            {
                ++content;
                builder.Hide(first, content);
            }
            else
            {
                heading = 0;
                content = first;
            }
            builder.Inline(content, last, Regular, heading);
            auto end = last;
            if (end < result.source.size())
            {
                ++end;
                if (result.source[last] == L'\r' && end < result.source.size() && result.source[end] == L'\n')
                    ++end;
            }
            builder.Emit(last, end, Regular, 0);
            first = end;
        }
    }
    return std::move(result);
}
bool ToggleMarker(Document& document, std::wstring_view marker)
{
    if (marker != L"**" && marker != L"*" && marker != L"`" && marker != L"~~")
        throw std::invalid_argument("Unsupported prototype format marker");
    const auto snapshot  = document.Snapshot();
    const auto selection = document.GetSelection();
    const auto first = selection.Start(), last = selection.End();
    // CTxtRange::ReplaceRange (Office RVA 0x360a0) distinguishes signed
    // selection directions. Apply delimiter deltas to anchor/caret separately
    // so a backwards rich selection keeps its active end.
    const auto size = marker.size();
    if (first >= size && size <= snapshot.Length() - last && snapshot.IsBoundary(first - size) && snapshot.IsBoundary(last + size) &&
        snapshot.Slice(first - size, first) == marker && snapshot.Slice(last, last + size) == marker)
    {
        auto content = snapshot.Slice(first, last);
        return document.Replace(first - size, last + size, std::move(content), {selection.anchor - size, selection.caret - size});
    }
    auto content = std::wstring(marker) + snapshot.Slice(first, last) + std::wstring(marker);
    return document.Replace(first, last, std::move(content), {selection.anchor + size, selection.caret + size});
}
} // namespace DxUi::RichText
