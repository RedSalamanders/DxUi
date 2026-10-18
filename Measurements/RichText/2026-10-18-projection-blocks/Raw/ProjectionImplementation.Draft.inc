struct PreparedProjection::State
{
    State() : blocks(0u) {}
    SourceSnapshot source;
    std::vector<ProjectionBlock> blocks;
    std::size_t displayLength{};
    std::uint64_t revision{};
    ViewMode mode = ViewMode::Source;
    ProjectionPreparation last{};
};
PreparedProjection::PreparedProjection() noexcept = default;
PreparedProjection::~PreparedProjection() noexcept = default;
PreparedProjection::PreparedProjection(PreparedProjection&&) noexcept = default;
PreparedProjection& PreparedProjection::operator=(PreparedProjection&&) noexcept = default;
bool PreparedProjection::Ready() const noexcept { return static_cast<bool>(state_); }
std::uint64_t PreparedProjection::Revision() const noexcept { return state_ ? state_->revision : 0u; }
ViewMode PreparedProjection::Mode() const noexcept { return state_ ? state_->mode : ViewMode::Source; }
std::size_t PreparedProjection::SourceLength() const noexcept { return state_ ? state_->source.Length() : 0u; }
std::size_t PreparedProjection::DisplayLength() const noexcept { return state_ ? state_->displayLength : 0u; }
ProjectionPreparation PreparedProjection::LastPreparation() const noexcept { return state_ ? state_->last : ProjectionPreparation{}; }
const std::vector<ProjectionBlock>& PreparedProjection::Blocks() const
{
    if (!state_) throw std::logic_error("Projection has not been prepared");
    return state_->blocks;
}
void PreparedProjection::Clear() noexcept { state_.reset(); }
void PreparedProjection::Prepare(SourceSnapshot source, std::uint64_t revision, ViewMode mode)
{
    if (mode != ViewMode::Source && mode != ViewMode::Rich)
        throw std::invalid_argument("Unknown rich-text view mode");
    const auto change = state_ ? source.ChangedRangeFrom(state_->source) : SourceChangeRange{};
    if (state_ && state_->mode == mode && change.kind == SourceChangeKind::Unchanged)
    {
        state_->source = std::move(source);
        state_->revision = revision;
        state_->last = {0u, state_->blocks.size(), 0u};
        return;
    }
    auto next = std::make_unique<State>();
    next->source = source;
    next->revision = revision;
    next->mode = mode;
    std::size_t first = 0u, last = state_ ? state_->blocks.size() : 0u;
    std::size_t sourceBegin = 0u, sourceEnd = source.Length();
    if (state_ && state_->mode == mode && change.kind == SourceChangeKind::TrackedEdits)
    {
        const auto containing = [&](std::size_t offset)
        {
            const auto found = std::upper_bound(state_->blocks.begin(), state_->blocks.end(), offset,
                [](std::size_t position, const ProjectionBlock& block) { return position < block.sourceStart; });
            return found == state_->blocks.begin() ? 0u : static_cast<std::size_t>(found - state_->blocks.begin() - 1);
        };
        first = containing(change.begin);
        if (first) --first;
        last = std::min(state_->blocks.size(), containing(change.previousEnd) + 2u);
        sourceBegin = state_->blocks[first].sourceStart;
        const auto oldEnd = last < state_->blocks.size() ? state_->blocks[last].sourceStart : state_->source.Length();
        sourceEnd = source.Length() - (state_->source.Length() - oldEnd);
        // The context consists of whole physical lines and one neighbor on
        // either side. A future grammar with spanning blocks needs its own
        // semantic widening rule; it cannot reuse this v0 rule unchanged.
        if (!source.IsBoundary(sourceBegin) || !source.IsBoundary(sourceEnd))
        {
            first = 0u;
            last = state_->blocks.size();
            sourceBegin = 0u;
            sourceEnd = source.Length();
        }
    }
    if (state_)
        next->blocks.insert(next->blocks.end(), state_->blocks.begin(), state_->blocks.begin() + static_cast<std::ptrdiff_t>(first));
    next->last.reusedBlocks = first;
    next->last.rebuiltSourceUnits = sourceEnd - sourceBegin;
    const auto changedText = source.Slice(sourceBegin, sourceEnd);
    std::size_t begin = 0u;
    while (begin < changedText.size())
    {
        auto end = changedText.find_first_of(L"\r\n", begin);
        if (end == std::wstring::npos) end = changedText.size();
        else
        {
            ++end;
            if (changedText[end - 1u] == L'\r' && end < changedText.size() && changedText[end] == L'\n') ++end;
        }
        const auto local = ProjectText(changedText.substr(begin, end - begin), 0u, mode);
        auto content = std::make_shared<const Projection>(local);
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
        const auto local = ProjectText(std::wstring(0u, L'\0'), 0u, mode);
        next->blocks.push_back({0u, 0u, std::make_shared<const Projection>(local)});
        ++next->last.rebuiltBlocks;
    }
    std::size_t sourceOffset = 0u;
    for (auto& block : next->blocks)
    {
        block.sourceStart = sourceOffset;
        block.displayStart = next->displayLength;
        sourceOffset += block.content->source.size();
        next->displayLength += block.content->text.size();
    }
    if (sourceOffset != source.Length()) throw std::logic_error("Projection block coverage differs from source");
    state_.swap(next);
}
std::size_t PreparedProjection::ToDisplay(std::size_t offset) const
{
    const auto& blocks = Blocks();
    if (offset > SourceLength()) throw std::out_of_range("Source offset");
    const auto end = std::upper_bound(blocks.begin(), blocks.end(), offset,
        [](std::size_t position, const ProjectionBlock& block) { return position < block.sourceStart; });
    const auto& block = end == blocks.begin() ? blocks.front() : *std::prev(end);
    return block.displayStart + block.content->ToDisplay(offset - block.sourceStart);
}
std::size_t PreparedProjection::ToSource(std::size_t offset, bool trailing) const
{
    const auto& blocks = Blocks();
    if (offset > DisplayLength()) throw std::out_of_range("Display offset");
    if (!DisplayLength()) return SourceLength();
    const bool useEnd = offset == DisplayLength() || (trailing && offset > 0u);
    const auto unitIndex = useEnd ? offset - 1u : offset;
    const auto found = std::upper_bound(blocks.begin(), blocks.end(), unitIndex,
        [](std::size_t position, const ProjectionBlock& block) { return position < block.displayStart + block.content->text.size(); });
    if (found == blocks.end()) throw std::logic_error("Projection block does not cover display unit");
    const auto& unit = found->content->units.at(unitIndex - found->displayStart);
    return found->sourceStart + (useEnd ? unit.end : unit.start);
}
std::wstring PreparedProjection::DisplayText() const
{
    const auto& blocks = Blocks();
    std::wstring result;
    result.reserve(DisplayLength());
    for (const auto& block : blocks) result += block.content->text;
    return result;
}
