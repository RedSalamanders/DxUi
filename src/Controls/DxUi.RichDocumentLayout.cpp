#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <wil/resource.h>

namespace DxUi::RichText
{
namespace
{
// Reserve the final slot for one whole-layout compatibility resource.
constexpr std::size_t TargetChunkUnits = 1024u, MaximumChunks = 127u;
// Stack-only rounding/observation barrier; no inter-thread synchronization or memory-mapped I/O.
// Both target volatile models preserve these accesses. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
float AddCoordinate(float left, float right) noexcept
{
    // Match individual single-precision DirectWrite accumulation, rather than
    // letting an optimizer reassociate the compatibility test.
    volatile float result = left + right;
    return result;
}
#pragma warning(pop)
bool SameStyles(const Projection& left, const Projection& right) noexcept
{
    return left.text == right.text && left.runs.size() == right.runs.size() &&
           std::equal(left.runs.begin(), left.runs.end(), right.runs.begin(), [](const StyleRun& a, const StyleRun& b) {
        return a.start == b.start && a.length == b.length && a.flags == b.flags && a.heading == b.heading;
    });
}
bool SameOptions(const LayoutOptions& left, const LayoutOptions& right) noexcept
{
    return left.widthDip == right.widthDip && left.fontSizeDip == right.fontSizeDip && left.codeFontSizeDip == right.codeFontSizeDip &&
           left.lineHeightMultiplier == right.lineHeightMultiplier && left.headingScaleStep == right.headingScaleStep && left.fontFamily == right.fontFamily &&
           left.headingFontSizeDip == right.headingFontSizeDip && left.headingLineHeightMultiplier == right.headingLineHeightMultiplier &&
           left.paragraphSpacingAfterLines == right.paragraphSpacingAfterLines && left.listItemSpacingDip == right.listItemSpacingDip &&
           left.semanticSpacing == right.semanticSpacing && left.codeFontFamily == right.codeFontFamily && left.locale == right.locale &&
           left.readingDirection == right.readingDirection && left.textAlignment == right.textAlignment;
}
void JoinBlocks(Projection& result, const std::vector<ProjectionBlock>& blocks, std::size_t first, std::size_t end, ViewMode mode)
{
    // Fill a fallibly constructed destination in place. Returning a named
    // Projection could move it without NRVO; Debug string proxy allocation in
    // that noexcept move would terminate rather than preserve the old layout.
    result.mode = mode;
    result.sourceToDisplay.push_back(0u);
    for (auto index = first; index < end; ++index)
    {
        const auto& block      = *blocks[index].content;
        const auto sourceStart = result.source.size(), displayStart = result.text.size();
        result.source += block.source;
        result.text += block.text;
        for (std::size_t offset = 1u; offset < block.sourceToDisplay.size(); ++offset)
            result.sourceToDisplay.push_back(displayStart + block.sourceToDisplay[offset]);
        for (const auto unit : block.units)
            result.units.push_back({sourceStart + unit.start, sourceStart + unit.end});
        for (const auto run : block.runs)
        {
            if (! result.runs.empty() && result.runs.back().start + result.runs.back().length == displayStart + run.start &&
                result.runs.back().flags == run.flags && result.runs.back().heading == run.heading)
                result.runs.back().length += run.length;
            else
                result.runs.push_back({displayStart + run.start, run.length, run.flags, run.heading});
        }
    }
}
struct GeometryChunk
{
    PreparedLayout layout;
    // All layout coordinates are already in document space. A nonfinal chunk's
    // terminal LF creates a phantom empty line; exclude it from the next origin.
    float continuationBottom{};
    float drawingTop{}, drawingBottom{};
};
struct ChunkPlacement
{
    std::size_t sourceStart{}, displayStart{}, firstBlock{}, blockCount{};
    float y{}, height{};
    std::shared_ptr<const GeometryChunk> geometry;
};
} // namespace
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct PreparedDocumentLayout::State
{
    State() : chunks(0u)
    {
    }
    SourceSnapshot source;
    PreparedProjection projection;
    LayoutOptions options;
    wil::com_ptr<IDWriteFactory> factory;
    std::vector<ChunkPlacement> chunks;
    mutable std::shared_ptr<const PreparedLayout> reference;
    bool referenceForGeometry{};
    float height{};
    mutable LayoutPreparation last;
};
#pragma warning(pop)
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626)
struct PreparedDocumentLayout::Preparation
{
    enum class Phase
    {
        Reuse,
        Chunks,
        Placement,
        Reference,
        Publish
    };
    Preparation() : next(std::make_unique<State>()), reusable(0u)
    {
    }
    std::unique_ptr<State> next;
    SourceChangeRange change;
    // Pointers borrow the immutable published graph, which remains alive until
    // publication or cancellation. Nothing leaves this preparation owner.
    std::vector<const ChunkPlacement*> reusable;
    const ChunkPlacement* scanning{};
    std::size_t scanBlock{}, scanLocal{}, first{}, end{}, units{}, checkedChunks{};
    Phase phase = Phase::Chunks;
    bool compatible{}, compact{}, exactPlacement = true;

    const ChunkPlacement* Candidate(const State& previous, std::size_t block) const
    {
        const auto& blocks      = next->projection.Blocks();
        const auto currentStart = blocks[block].sourceStart;
        std::size_t oldStart    = currentStart;
        if (change.kind != SourceChangeKind::Unchanged)
        {
            if (currentStart >= change.currentEnd)
                oldStart = currentStart - change.currentEnd + change.previousEnd;
            else if (currentStart >= change.begin)
                return nullptr;
        }
        const auto found = std::lower_bound(previous.chunks.begin(), previous.chunks.end(), oldStart, [](const ChunkPlacement& chunk, std::size_t offset) {
            return chunk.sourceStart < offset;
        });
        if (found == previous.chunks.end() || found->sourceStart != oldStart || found->blockCount > blocks.size() - block)
            return nullptr;
        const auto oldLength = found->geometry->layout.Content().source.size();
        if (change.kind != SourceChangeKind::Unchanged && oldStart < change.begin && oldLength > change.begin - oldStart)
            return nullptr;
        return &*found;
    }
    const ChunkPlacement* Reusable(std::size_t block) const noexcept
    {
        return ! compact && block < reusable.size() ? reusable[block] : nullptr;
    }
    void Append(std::size_t begin, std::size_t limit, std::shared_ptr<const GeometryChunk> geometry)
    {
        const auto& blocks = next->projection.Blocks();
        const auto& block  = blocks[begin];
        const auto bottom  = limit == blocks.size() ? geometry->layout.Height() : geometry->continuationBottom;
        next->chunks.push_back({block.sourceStart, block.displayStart, begin, limit - begin, next->height, bottom - next->height, std::move(geometry)});
        next->height = bottom;
        if (! std::isfinite(bottom))
            throw std::overflow_error("Rich geometry height overflow");
    }
};
#pragma warning(pop)
PreparedDocumentLayout::PreparedDocumentLayout() noexcept  = default;
PreparedDocumentLayout::~PreparedDocumentLayout() noexcept = default;
const PreparedDocumentLayout::State& PreparedDocumentLayout::Prepared() const
{
    if (! Ready())
        throw std::logic_error("Unprepared rich document geometry");
    return *state_;
}
bool PreparedDocumentLayout::Ready() const noexcept
{
    return ready_ && static_cast<bool>(state_);
}
float PreparedDocumentLayout::Height() const noexcept
{
    return Ready() ? state_->height : 0.0f;
}
std::uint64_t PreparedDocumentLayout::Generation() const noexcept
{
    return generation_;
}
std::uint64_t PreparedDocumentLayout::Revision() const noexcept
{
    return Ready() ? state_->projection.Revision() : 0u;
}
LayoutPreparation PreparedDocumentLayout::LastPreparation() const noexcept
{
    return Ready() ? state_->last : LayoutPreparation{};
}
std::size_t PreparedDocumentLayout::ChunkCount() const noexcept
{
    return state_ ? state_->chunks.size() + (state_->reference ? 1u : 0u) : 0u;
}
bool PreparedDocumentLayout::UsesWholeGeometry() const noexcept
{
    return state_ && state_->referenceForGeometry;
}
bool PreparedDocumentLayout::UsesWholeRaster() const noexcept
{
    return state_ && static_cast<bool>(state_->reference);
}
void PreparedDocumentLayout::Invalidate() noexcept
{
    CancelPreparation();
    ready_ = false;
}
void PreparedDocumentLayout::Clear() noexcept
{
    CancelPreparation();
    ready_ = false;
    state_.reset();
    ++generation_;
}
void PreparedDocumentLayout::Prepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options)
{
    if (BeginPreparation(factory, std::move(source), revision, mode, options) == LayoutPreparationStatus::Complete)
        return;
    const LayoutWorkBudget unlimited{std::numeric_limits<std::size_t>::max(), std::numeric_limits<std::size_t>::max()};
    while (AdvancePreparation(unlimited).status == LayoutPreparationStatus::Pending)
    {
    }
}
bool PreparedDocumentLayout::HasPendingPreparation() const noexcept
{
    return static_cast<bool>(pending_);
}
LayoutPreparation PreparedDocumentLayout::PendingPreparation() const noexcept
{
    return pending_ ? pending_->next->last : LayoutPreparation{};
}
void PreparedDocumentLayout::CancelPreparation() noexcept
{
    // No retirement container is constructed in this noexcept path.
    pending_.reset();
}
LayoutPreparationStatus PreparedDocumentLayout::BeginPreparation(
    IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options)
{
    // Retain compatible pending work so the host can resume preparation in
    // bounded steps on the thread that owns the layout factory.
    if (pending_ && pending_->next->factory.get() == factory && pending_->next->projection.Mode() == mode &&
        pending_->next->projection.Revision() == revision && SameOptions(pending_->next->options, options) &&
        source.ChangedRangeFrom(pending_->next->source).kind == SourceChangeKind::Unchanged)
        return LayoutPreparationStatus::Pending;
    const bool compatible = state_ && state_->factory.get() == factory && state_->projection.Mode() == mode && SameOptions(state_->options, options);
    const auto change     = state_ ? source.ChangedRangeFrom(state_->source) : SourceChangeRange{};
    if (compatible && change.kind == SourceChangeKind::Unchanged)
    {
        state_->projection.Prepare(source, revision, mode);
        state_->source = std::move(source);
        state_->last   = {0u, state_->chunks.size(), 0u, 0u, state_->reference ? 1u : 0u, 0u};
        CancelPreparation();
        ready_ = true;
        ++generation_;
        return LayoutPreparationStatus::Complete;
    }
    auto request = std::make_unique<Preparation>();
    auto& next   = *request->next;
    if (state_)
        next.projection = state_->projection.Clone();
    // This bounded v0 parser/index pass is still indivisible. Do not describe a
    // work-unit budget around DirectWrite as a resumable Markdown parser.
    next.projection.Prepare(source, revision, mode);
    next.source         = std::move(source);
    next.options        = options;
    next.factory        = factory;
    request->compatible = compatible;
    request->change     = change;
    if (compatible && change.kind != SourceChangeKind::FullReplacement && ! state_->chunks.empty())
    {
        request->reusable.resize(next.projection.Blocks().size(), nullptr);
        request->phase = Preparation::Phase::Reuse;
    }
    // Replacing a pending request is atomic even if cloning/indexing/options fail.
    pending_.swap(request);
    return LayoutPreparationStatus::Pending;
}
LayoutAdvance PreparedDocumentLayout::AdvancePreparation(const LayoutWorkBudget& budget)
{
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps)
        throw std::invalid_argument("Rich preparation budgets must be positive");
    if (! pending_)
        return {};
    LayoutAdvance result;
    auto discardOnFailure = wil::scope_exit([&] { pending_.reset(); });
    const auto leave      = [&](LayoutPreparationStatus status)
    {
        result.status = status;
        discardOnFailure.release();
        return result;
    };
    auto& request      = *pending_;
    auto& next         = *request.next;
    const auto& blocks = next.projection.Blocks();
    for (;;)
    {
        switch (request.phase)
        {
            case Preparation::Phase::Reuse:
            {
                if (request.scanBlock == blocks.size())
                {
                    request.phase = Preparation::Phase::Chunks;
                    continue;
                }
                if (result.blockSteps == budget.maximumBlockSteps)
                    return leave(LayoutPreparationStatus::Pending);
                ++result.blockSteps;
                if (! request.scanning)
                    request.scanning = request.Candidate(*state_, request.scanBlock);
                if (! request.scanning)
                {
                    ++request.scanBlock;
                    continue;
                }
                const auto& old       = *request.scanning;
                const auto& oldBlocks = state_->projection.Blocks();
                if (blocks[request.scanBlock + request.scanLocal].content->source != oldBlocks[old.firstBlock + request.scanLocal].content->source)
                {
                    ++request.scanBlock;
                    request.scanLocal = 0u;
                    request.scanning  = nullptr;
                }
                else if (++request.scanLocal == old.blockCount)
                {
                    request.reusable[request.scanBlock] = request.scanning;
                    request.scanBlock += old.blockCount;
                    request.scanLocal = 0u;
                    request.scanning  = nullptr;
                }
                continue;
            }
            case Preparation::Phase::Chunks:
            {
                if (request.first == blocks.size())
                {
                    request.phase = Preparation::Phase::Placement;
                    continue;
                }
                if (next.chunks.size() == MaximumChunks)
                {
                    if (request.compact)
                        throw std::logic_error("Rich geometry chunk bound exceeded");
                    if (result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    // Discard only staging and count all abandoned preparation.
                    request.compact = true;
                    request.first = request.end = request.units = 0u;
                    next.chunks.clear();
                    next.height            = 0.0f;
                    next.last.reusedChunks = 0u;
                    continue;
                }
                if (request.end == request.first)
                {
                    if (const auto* old = request.Reusable(request.first); old && old->geometry->layout.VerticalOrigin() == next.height)
                    {
                        if (result.blockSteps == budget.maximumBlockSteps)
                            return leave(LayoutPreparationStatus::Pending);
                        ++result.blockSteps;
                        request.Append(request.first, request.first + old->blockCount, old->geometry);
                        request.first += old->blockCount;
                        request.end = request.first;
                        ++next.last.reusedChunks;
                        continue;
                    }
                }
                while (request.end < blocks.size() && (request.end == request.first || (request.units < TargetChunkUnits && ! request.Reusable(request.end))))
                {
                    if (result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    request.units += blocks[request.end++].content->source.size();
                }
                if (result.layoutsPrepared == budget.maximumLayouts)
                    return leave(LayoutPreparationStatus::Pending);
                auto geometry = std::make_shared<GeometryChunk>();
                Projection joined{
                    std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
                JoinBlocks(joined, blocks, request.first, request.end, next.projection.Mode());
                geometry->layout.PrepareOwned(next.factory.get(), joined, next.options, next.height);
                ++result.layoutsPrepared;
                const auto drawingBounds     = geometry->layout.DrawingVerticalBounds();
                geometry->drawingTop         = drawingBounds.first;
                geometry->drawingBottom      = drawingBounds.second;
                geometry->continuationBottom = next.height;
                const auto& text             = geometry->layout.Content().text;
                const auto lineMetrics       = geometry->layout.LineMetrics();
                const auto count             = ! text.empty() && text.back() == L'\n' && ! lineMetrics.empty() ? lineMetrics.size() - 1u : lineMetrics.size();
                for (std::size_t line = 0u; line < count; ++line)
                    geometry->continuationBottom = AddCoordinate(geometry->continuationBottom, lineMetrics[line].height);
                request.Append(request.first, request.end, std::move(geometry));
                next.last.preparedSourceUnits += request.units;
                ++next.last.preparedChunks;
                request.first = request.end;
                request.units = 0u;
                continue;
            }
            case Preparation::Phase::Placement:
            {
                if (request.checkedChunks == next.chunks.size())
                {
                    request.phase = Preparation::Phase::Reference;
                    continue;
                }
                if (result.blockSteps == budget.maximumBlockSteps)
                    return leave(LayoutPreparationStatus::Pending);
                ++result.blockSteps;
                const auto& chunk = next.chunks[request.checkedChunks++];
                request.exactPlacement =
                    request.exactPlacement && chunk.geometry->layout.PositionedExactly() && chunk.geometry->layout.VerticalOrigin() == chunk.y;
                continue;
            }
            case Preparation::Phase::Reference:
            {
                if (! request.exactPlacement || (request.compatible && state_->reference))
                {
                    if (result.layoutsPrepared == budget.maximumLayouts || result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    // One explicit compatibility work unit; joining a whole
                    // projection/SDK layout is not a hard CPU-time guarantee.
                    Projection joined{
                        std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
                    JoinBlocks(joined, blocks, 0u, blocks.size(), next.projection.Mode());
                    if (request.compatible && state_->reference && SameStyles(joined, state_->reference->Content()))
                    {
                        next.reference = state_->reference;
                        ++next.last.wholeLayoutsReused;
                    }
                    else if (! request.exactPlacement)
                    {
                        auto reference = std::make_shared<PreparedLayout>();
                        reference->PrepareOwned(next.factory.get(), joined, next.options);
                        next.reference = std::move(reference);
                        ++result.layoutsPrepared;
                        ++next.last.wholeLayoutsPrepared;
                        next.last.wholeLayoutSourceUnits = next.source.Length();
                    }
                }
                if (! request.exactPlacement)
                {
                    next.referenceForGeometry = true;
                    next.height               = next.reference->Height();
                    next.chunks.clear();
                }
                request.phase = Preparation::Phase::Publish;
                continue;
            }
            case Preparation::Phase::Publish:
            {
                // Only this nonthrowing commit changes the published pair.
                state_.swap(request.next);
                ready_ = true;
                ++generation_;
                pending_.reset();
                return leave(LayoutPreparationStatus::Complete);
            }
        }
    }
}
std::size_t PreparedDocumentLayout::DisplayLength() const
{
    return Prepared().projection.DisplayLength();
}
std::size_t PreparedDocumentLayout::SourceLength() const
{
    return Prepared().projection.SourceLength();
}
wchar_t PreparedDocumentLayout::DisplayUnit(std::size_t position) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->Content().text.at(position);
    const auto& chunk = Prepared().chunks[ChunkAt(position)];
    return chunk.geometry->layout.Content().text.at(position - chunk.displayStart);
}
bool PreparedDocumentLayout::SourceMatches(std::wstring_view source) const
{
    const auto& state = Prepared();
    if (source.size() != state.projection.SourceLength())
        return false;
    for (const auto& block : state.projection.Blocks())
    {
        const auto& text = block.content->source;
        if (source.substr(block.sourceStart, text.size()) != text)
            return false;
    }
    return true;
}
std::size_t PreparedDocumentLayout::SourceBoundary(std::size_t position) const
{
    const auto& projection = Prepared().projection;
    if (position > projection.DisplayLength())
        throw std::out_of_range("Rich display boundary");
    std::size_t first = 0u, end = projection.SourceLength();
    while (first < end)
    {
        const auto middle = first + (end - first) / 2u;
        if (projection.ToDisplay(middle) < position)
            first = middle + 1u;
        else
            end = middle;
    }
    return first;
}
std::size_t PreparedDocumentLayout::ToDisplay(std::size_t position) const
{
    return Prepared().projection.ToDisplay(position);
}
std::size_t PreparedDocumentLayout::ToSource(std::size_t position, bool trailing) const
{
    return Prepared().projection.ToSource(position, trailing);
}
std::wstring PreparedDocumentLayout::DisplayText() const
{
    return Prepared().projection.DisplayText();
}
std::size_t PreparedDocumentLayout::ChunkAt(std::size_t position) const
{
    const auto& state = Prepared();
    if (position > state.projection.DisplayLength())
        throw std::out_of_range("Rich geometry position");
    const auto found = std::upper_bound(
        state.chunks.begin(), state.chunks.end(), position, [](std::size_t offset, const ChunkPlacement& chunk) { return offset < chunk.displayStart; });
    return found == state.chunks.begin() ? 0u : static_cast<std::size_t>(found - state.chunks.begin() - 1);
}
CaretGeometry PreparedDocumentLayout::Caret(std::size_t position) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->Caret(position);
    const auto& chunk = Prepared().chunks[ChunkAt(position)];
    return chunk.geometry->layout.Caret(position - chunk.displayStart);
}
Hit PreparedDocumentLayout::HitTest(float x, float y) const
{
    const auto& state = Prepared();
    if (state.referenceForGeometry)
        return state.reference->HitTest(x, y);
    if (! std::isfinite(x) || ! std::isfinite(y))
        throw std::invalid_argument("Invalid rich document hit test");
    const auto found  = std::upper_bound(state.chunks.begin(), state.chunks.end(), y, [](float top, const ChunkPlacement& chunk) { return top < chunk.y; });
    const auto& chunk = found == state.chunks.begin() ? state.chunks.front() : *std::prev(found);
    auto hit          = chunk.geometry->layout.HitTest(x, y);
    hit.position += chunk.displayStart;
    return hit;
}
std::vector<D2D1_RECT_F> PreparedDocumentLayout::Range(std::size_t first, std::size_t last) const
{
    const auto& state = Prepared();
    if (state.referenceForGeometry)
        return state.reference->Range(first, last);
    if (first > last || last > state.projection.DisplayLength())
        throw std::out_of_range("Rich document selection range");
    std::vector<D2D1_RECT_F> rectangles(0u);
    if (first == last)
        return std::vector<D2D1_RECT_F>(0u);
    for (auto index = ChunkAt(first); index < state.chunks.size(); ++index)
    {
        const auto& chunk = state.chunks[index];
        if (chunk.displayStart >= last)
            break;
        const auto end = std::min(last - chunk.displayStart, chunk.geometry->layout.Content().text.size());
        auto local     = chunk.geometry->layout.Range(first > chunk.displayStart ? first - chunk.displayStart : 0u, end);
        rectangles.insert(rectangles.end(), local.begin(), local.end());
    }
    // Preserve the query's fallible boundary when Debug omits NRVO. Its vector
    // move constructor can allocate a proxy despite being noexcept.
    return std::vector<D2D1_RECT_F>(rectangles);
}
std::size_t PreparedDocumentLayout::PreviousCluster(std::size_t position) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->PreviousCluster(position);
    auto index         = ChunkAt(position);
    const auto& chunks = Prepared().chunks;
    if (index && position == chunks[index].displayStart)
        --index;
    const auto& chunk = chunks[index];
    return chunk.displayStart + chunk.geometry->layout.PreviousCluster(position - chunk.displayStart);
}
std::size_t PreparedDocumentLayout::NextCluster(std::size_t position) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->NextCluster(position);
    const auto& chunk = Prepared().chunks[ChunkAt(position)];
    return chunk.displayStart + chunk.geometry->layout.NextCluster(position - chunk.displayStart);
}
std::pair<std::size_t, std::size_t> PreparedDocumentLayout::DeletionRange(std::size_t position, bool backwards) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->DeletionRange(position, backwards);
    auto index         = ChunkAt(position);
    const auto& chunks = Prepared().chunks;
    if (backwards && index && position == chunks[index].displayStart)
        --index;
    const auto& chunk = chunks[index];
    const auto range  = chunk.geometry->layout.DeletionRange(position - chunk.displayStart, backwards);
    return {chunk.displayStart + range.first, chunk.displayStart + range.second};
}
std::size_t PreparedDocumentLayout::LineBoundary(std::size_t position, bool end) const
{
    if (const auto& state = Prepared(); state.referenceForGeometry)
        return state.reference->LineBoundary(position, end);
    const auto& chunk = Prepared().chunks[ChunkAt(position)];
    return chunk.displayStart + chunk.geometry->layout.LineBoundary(position - chunk.displayStart, end);
}
std::pair<std::size_t, std::size_t> PreparedDocumentLayout::VisibleRange(float top, float bottom) const
{
    const auto& state = Prepared();
    if (state.referenceForGeometry)
        return state.reference->VisibleRange(top, bottom);
    if (! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        throw std::invalid_argument("Invalid rich document viewport");
    std::optional<std::size_t> first;
    std::size_t last = 0u;
    for (const auto& chunk : state.chunks)
    {
        if (chunk.y >= bottom)
            break;
        if (chunk.y + chunk.height <= top)
            continue;
        const auto range = chunk.geometry->layout.VisibleRange(top, std::min(bottom, chunk.y + chunk.height));
        // A collapsed range at the final display ACP can be a visible empty
        // line/end caret. Only {0,0} for a nonempty layout means no line here.
        if (range.first == range.second && range.first != chunk.geometry->layout.Content().text.size())
            continue;
        if (! first)
            first = chunk.displayStart + range.first;
        last = chunk.displayStart + range.second;
    }
    return {first.value_or(0u), last};
}
void PreparedDocumentLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom, const LinkDrawing* links) const
{
    const auto& state = Prepared();
    if (! target || ! brush || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        throw std::invalid_argument("Invalid rich document drawing");
    D2D1_MATRIX_3X2_F transform{};
    target->GetTransform(&transform);
    float dpiX = 0.0f, dpiY = 0.0f;
    target->GetDpi(&dpiX, &dpiY);
    const auto scale   = dpiY / 96.0f * transform._22;
    const bool canCull = transform._12 == 0.0f && transform._21 == 0.0f && transform._11 > 0.0f && std::isfinite(scale) && scale > 0.0f;
    // Public DirectWrite overhang metrics include glyph ink, but not hinting or
    // strikethrough. Union with logical metrics, allow two target pixels plus
    // float cancellation, and draw all bounded chunks for non-axis transforms.
    const auto padding         = canCull ? 2.0f / scale + 0.125f : 0.0f;
    const bool referenceRaster = state.referenceForGeometry || static_cast<bool>(state.reference);
    if (referenceRaster)
    {
        if (! state.reference)
        {
            Projection joined{
                std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
            JoinBlocks(joined, state.projection.Blocks(), 0u, state.projection.Blocks().size(), state.projection.Mode());
            auto reference = std::make_shared<PreparedLayout>();
            reference->PrepareOwned(state.factory.get(), joined, state.options);
            state.reference = std::move(reference);
            ++state.last.wholeLayoutsPrepared;
            state.last.wholeLayoutSourceUnits = state.projection.SourceLength();
        }
        state.reference->Draw(target, brush, origin, links);
        return;
    }
    for (const auto& chunk : state.chunks)
    {
        if (! canCull || (chunk.geometry->drawingTop - padding < bottom && chunk.geometry->drawingBottom + padding > top))
        {
            auto local = links ? *links : LinkDrawing{};
            local.displayOrigin += chunk.displayStart;
            chunk.geometry->layout.Draw(target, brush, origin, links ? &local : nullptr);
        }
    }
}
} // namespace DxUi::RichText
