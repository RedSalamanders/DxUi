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
    explicit Builder(Projection& projection) noexcept : result(projection)
    {
    }
    Projection& result;
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
void FillProjection(Projection& result, std::wstring&& text, std::uint64_t revision, ViewMode mode)
{
    if (mode != ViewMode::Source && mode != ViewMode::Rich)
        throw std::invalid_argument("Unknown rich-text view mode");
    Builder builder(result);
    result.source = std::move(text);
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
}
// Use throwing size constructors for Debug STL proxy failures. Block preparation
// copies this empty value into owned staging, then fills it directly: no complete
// source/display/mapping payload is moved or copied into another Projection.
Projection MakeEmptyProjection()
{
    return {std::wstring(0u, L'\0'),
            std::wstring(0u, L'\0'),
            std::vector<std::size_t>(0u),
            std::vector<SourceUnit>(0u),
            std::vector<StyleRun>(0u),
            0u,
            ViewMode::Source};
}
std::shared_ptr<const Projection> ProjectBlock(std::wstring&& text, ViewMode mode)
{
    const auto empty = MakeEmptyProjection();
    auto staged      = std::make_shared<Projection>(empty);
    FillProjection(*staged, std::move(text), 0u, mode);
    return staged;
}
} // namespace
Projection Project(SourceSnapshot source, std::uint64_t revision, ViewMode mode)
{
    auto result = MakeEmptyProjection();
    FillProjection(result, source.Text(), revision, mode);
    return result;
}
struct PreparedProjection::State
{
    State() : blocks(0u)
    {
    }
    SourceSnapshot source;
    std::vector<ProjectionBlock> blocks;
    std::size_t displayLength{};
    std::uint64_t revision{};
    ViewMode mode = ViewMode::Source;
    ProjectionPreparation last{};
};
PreparedProjection::PreparedProjection() noexcept                                = default;
PreparedProjection::~PreparedProjection() noexcept                               = default;
PreparedProjection::PreparedProjection(PreparedProjection&&) noexcept            = default;
PreparedProjection& PreparedProjection::operator=(PreparedProjection&&) noexcept = default;
bool PreparedProjection::Ready() const noexcept
{
    return static_cast<bool>(state_);
}
std::uint64_t PreparedProjection::Revision() const noexcept
{
    return state_ ? state_->revision : 0u;
}
ViewMode PreparedProjection::Mode() const noexcept
{
    return state_ ? state_->mode : ViewMode::Source;
}
std::size_t PreparedProjection::SourceLength() const noexcept
{
    return state_ ? state_->source.Length() : 0u;
}
std::size_t PreparedProjection::DisplayLength() const noexcept
{
    return state_ ? state_->displayLength : 0u;
}
ProjectionPreparation PreparedProjection::LastPreparation() const noexcept
{
    return state_ ? state_->last : ProjectionPreparation{};
}
const std::vector<ProjectionBlock>& PreparedProjection::Blocks() const
{
    if (! state_)
        throw std::logic_error("Projection has not been prepared");
    return state_->blocks;
}
void PreparedProjection::Clear() noexcept
{
    state_.reset();
}
PreparedProjection PreparedProjection::Clone() const
{
    PreparedProjection copy;
    if (state_)
        copy.state_ = std::make_unique<State>(*state_);
    return copy;
}
void PreparedProjection::Prepare(SourceSnapshot source, std::uint64_t revision, ViewMode mode)
{
    if (mode != ViewMode::Source && mode != ViewMode::Rich)
        throw std::invalid_argument("Unknown rich-text view mode");
    const auto change = state_ ? source.ChangedRangeFrom(state_->source) : SourceChangeRange{};
    if (state_ && state_->mode == mode && change.kind == SourceChangeKind::Unchanged)
    {
        state_->source   = std::move(source);
        state_->revision = revision;
        state_->last     = {0u, state_->blocks.size(), 0u};
        return;
    }
    auto next         = std::make_unique<State>();
    next->source      = source;
    next->revision    = revision;
    next->mode        = mode;
    std::size_t first = 0u, last = state_ ? state_->blocks.size() : 0u;
    std::size_t sourceBegin = 0u, sourceEnd = source.Length();
    if (state_ && state_->mode == mode && change.kind == SourceChangeKind::TrackedEdits)
    {
        const auto containing = [&](std::size_t offset)
        {
            const auto found = std::upper_bound(state_->blocks.begin(), state_->blocks.end(), offset, [](std::size_t position, const ProjectionBlock& block) {
                return position < block.sourceStart;
            });
            return found == state_->blocks.begin() ? 0u : static_cast<std::size_t>(found - state_->blocks.begin() - 1);
        };
        first = containing(change.begin);
        if (first)
            --first;
        last              = std::min(state_->blocks.size(), containing(change.previousEnd) + 2u);
        sourceBegin       = state_->blocks[first].sourceStart;
        const auto oldEnd = last < state_->blocks.size() ? state_->blocks[last].sourceStart : state_->source.Length();
        sourceEnd         = source.Length() - (state_->source.Length() - oldEnd);
        // The context consists of whole physical lines and one neighbor on
        // either side. A future grammar with spanning blocks needs its own
        // semantic widening rule; it cannot reuse this v0 rule unchanged.
        if (! source.IsBoundary(sourceBegin) || ! source.IsBoundary(sourceEnd))
        {
            first       = 0u;
            last        = state_->blocks.size();
            sourceBegin = 0u;
            sourceEnd   = source.Length();
        }
    }
    if (state_)
        next->blocks.insert(next->blocks.end(), state_->blocks.begin(), state_->blocks.begin() + static_cast<std::ptrdiff_t>(first));
    next->last.reusedBlocks       = first;
    next->last.rebuiltSourceUnits = sourceEnd - sourceBegin;
    const auto changedText        = source.Slice(sourceBegin, sourceEnd);
    std::size_t begin             = 0u;
    while (begin < changedText.size())
    {
        auto end = changedText.find_first_of(L"\r\n", begin);
        if (end == std::wstring::npos)
            end = changedText.size();
        else
        {
            ++end;
            if (changedText[end - 1u] == L'\r' && end < changedText.size() && changedText[end] == L'\n')
                ++end;
        }
        const auto line = std::wstring_view(changedText).substr(begin, end - begin);
        std::shared_ptr<const Projection> content;
        // Repeated adjacent physical lines have identical v0 context. Share the
        // immutable local payload, especially for the maximum all-newline case.
        // Statistics count rebuilt block slots/source units, not parser calls.
        if (! next->blocks.empty() && next->blocks.back().content->source == line)
            content = next->blocks.back().content;
        else
            content = ProjectBlock(std::wstring(line), mode);
        next->blocks.push_back({0u, 0u, std::move(content)});
        ++next->last.rebuiltBlocks;
        begin = end;
    }
    if (state_)
    {
        next->blocks.insert(next->blocks.end(), state_->blocks.begin() + static_cast<std::ptrdiff_t>(last), state_->blocks.end());
        next->last.reusedBlocks += state_->blocks.size() - last;
    }
    if (next->blocks.empty())
    {
        next->blocks.push_back({0u, 0u, ProjectBlock(std::wstring(0u, L'\0'), mode)});
        ++next->last.rebuiltBlocks;
    }
    std::size_t sourceOffset = 0u;
    for (auto& block : next->blocks)
    {
        block.sourceStart  = sourceOffset;
        block.displayStart = next->displayLength;
        sourceOffset += block.content->source.size();
        next->displayLength += block.content->text.size();
    }
    if (sourceOffset != source.Length())
        throw std::logic_error("Projection block coverage differs from source");
    state_.swap(next);
}
std::size_t PreparedProjection::ToDisplay(std::size_t offset) const
{
    const auto& blocks = Blocks();
    if (offset > SourceLength())
        throw std::out_of_range("Source offset");
    const auto end =
        std::upper_bound(blocks.begin(), blocks.end(), offset, [](std::size_t position, const ProjectionBlock& block) { return position < block.sourceStart; });
    const auto& block = end == blocks.begin() ? blocks.front() : *std::prev(end);
    return block.displayStart + block.content->ToDisplay(offset - block.sourceStart);
}
std::size_t PreparedProjection::ToSource(std::size_t offset, bool trailing) const
{
    const auto& blocks = Blocks();
    if (offset > DisplayLength())
        throw std::out_of_range("Display offset");
    if (! DisplayLength())
        return SourceLength();
    const bool useEnd    = offset == DisplayLength() || (trailing && offset > 0u);
    const auto unitIndex = useEnd ? offset - 1u : offset;
    const auto found     = std::upper_bound(blocks.begin(), blocks.end(), unitIndex, [](std::size_t position, const ProjectionBlock& block) {
        return position < block.displayStart + block.content->text.size();
    });
    if (found == blocks.end())
        throw std::logic_error("Projection block does not cover display unit");
    const auto& unit = found->content->units.at(unitIndex - found->displayStart);
    return found->sourceStart + (useEnd ? unit.end : unit.start);
}
std::wstring PreparedProjection::DisplayText() const
{
    const auto& blocks = Blocks();
    std::wstring result(0u, L'\0');
    result.reserve(DisplayLength());
    for (const auto& block : blocks)
        result += block.content->text;
    return result;
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
