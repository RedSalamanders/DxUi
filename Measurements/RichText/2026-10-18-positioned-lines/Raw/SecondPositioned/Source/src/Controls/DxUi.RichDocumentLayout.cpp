#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
// Reserve the final slot for one whole-layout compatibility resource.
constexpr std::size_t TargetChunkUnits = 1024u, MaximumChunks = 127u;
float AddCoordinate(float left, float right) noexcept
{
    // Match individual single-precision DirectWrite accumulation, rather than
    // letting an optimizer reassociate the compatibility test.
    volatile float result = left + right;
    return result;
}
bool SameStyles(const Projection& left, const Projection& right) noexcept
{
    return left.text == right.text && left.runs.size() == right.runs.size() &&
           std::equal(left.runs.begin(), left.runs.end(), right.runs.begin(), [](const StyleRun& a, const StyleRun& b) {
        return a.start == b.start && a.length == b.length && a.flags == b.flags && a.heading == b.heading;
    });
}
bool SameOptions(const LayoutOptions& left, const LayoutOptions& right) noexcept
{
    return left.widthDip == right.widthDip && left.fontSizeDip == right.fontSizeDip && left.fontFamily == right.fontFamily &&
           left.codeFontFamily == right.codeFontFamily && left.locale == right.locale && left.readingDirection == right.readingDirection;
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
    ready_ = false;
}
void PreparedDocumentLayout::Clear() noexcept
{
    ready_ = false;
    state_.reset();
    ++generation_;
}
void PreparedDocumentLayout::Prepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options)
{
    // Only public DirectWrite is used. Office's StepBackgroundRecalc/WaitForRecalc
    // evidence distinguishes invalidation, work and completion; it neither
    // establishes thread safety nor authorizes an embedded per-control timer.
    const bool compatible = state_ && state_->factory.get() == factory && state_->projection.Mode() == mode && SameOptions(state_->options, options);
    const auto change     = state_ ? source.ChangedRangeFrom(state_->source) : SourceChangeRange{};
    if (compatible && change.kind == SourceChangeKind::Unchanged)
    {
        state_->projection.Prepare(source, revision, mode);
        state_->source = std::move(source);
        state_->last   = {0u, state_->chunks.size(), 0u, 0u, state_->reference ? 1u : 0u, 0u};
        ready_         = true;
        ++generation_;
        return;
    }
    auto next = std::make_unique<State>();
    if (state_)
        next->projection = state_->projection.Clone();
    next->projection.Prepare(source, revision, mode);
    next->source        = source;
    next->options       = options;
    next->factory       = factory; // WIL retains the factory, preventing address reuse while chunks live.
    const auto& blocks  = next->projection.Blocks();
    const auto reusable = [&](std::size_t first) -> const ChunkPlacement*
    {
        if (! compatible || change.kind == SourceChangeKind::FullReplacement)
            return nullptr;
        const auto currentStart = blocks[first].sourceStart;
        std::size_t oldStart    = currentStart;
        if (change.kind != SourceChangeKind::Unchanged)
        {
            if (currentStart >= change.currentEnd)
                oldStart = currentStart - change.currentEnd + change.previousEnd;
            else if (currentStart >= change.begin)
                return nullptr;
        }
        const auto found = std::lower_bound(
            state_->chunks.begin(), state_->chunks.end(), oldStart, [](const ChunkPlacement& chunk, std::size_t offset) { return chunk.sourceStart < offset; });
        if (found == state_->chunks.end() || found->sourceStart != oldStart || found->blockCount > blocks.size() - first)
            return nullptr;
        const auto oldLength = found->geometry->layout.Content().source.size();
        if (change.kind != SourceChangeKind::Unchanged && oldStart < change.begin && oldLength > change.begin - oldStart)
            return nullptr;
        const auto& oldBlocks = state_->projection.Blocks();
        for (std::size_t local = 0u; local < found->blockCount; ++local)
            if (blocks[first + local].content->source != oldBlocks[found->firstBlock + local].content->source)
                return nullptr;
        return &*found;
    };
    const auto append = [&](std::size_t first, std::size_t end, std::shared_ptr<const GeometryChunk> geometry)
    {
        const auto& block = blocks[first];
        const auto bottom = end == blocks.size() ? geometry->layout.Height() : geometry->continuationBottom;
        const auto height = bottom - next->height;
        next->chunks.push_back({block.sourceStart, block.displayStart, first, end - first, next->height, height, std::move(geometry)});
        next->height = bottom;
        if (! std::isfinite(next->height))
            throw std::overflow_error("Rich geometry height overflow");
    };
    // One fallback compaction prevents repeated edits from fragmenting a view
    // into an unbounded COM graph. At most old+new bounded graphs coexist.
    for (bool compact = false;; compact = true)
    {
        next->chunks.clear();
        next->height            = 0.0f;
        next->last.reusedChunks = 0u;
        std::size_t first       = 0u;
        while (first < blocks.size() && next->chunks.size() < MaximumChunks)
        {
            if (! compact)
            {
                if (const auto* old = reusable(first); old && old->geometry->layout.VerticalOrigin() == next->height)
                {
                    append(first, first + old->blockCount, old->geometry);
                    first += old->blockCount;
                    ++next->last.reusedChunks;
                    continue;
                }
            }
            auto end   = first + 1u;
            auto units = blocks[first].content->source.size();
            while (end < blocks.size() && units < TargetChunkUnits && (compact || ! reusable(end)))
                units += blocks[end++].content->source.size();
            auto geometry = std::make_shared<GeometryChunk>();
            Projection joined{
                std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
            JoinBlocks(joined, blocks, first, end, mode);
            geometry->layout.PrepareOwned(factory, joined, options, next->height);
            const auto drawingBounds     = geometry->layout.DrawingVerticalBounds();
            geometry->drawingTop         = drawingBounds.first;
            geometry->drawingBottom      = drawingBounds.second;
            const auto& text             = geometry->layout.Content().text;
            geometry->continuationBottom = next->height;
            const auto lineMetrics       = geometry->layout.LineMetrics();
            const auto lineCount         = ! text.empty() && text.back() == L'\n' && ! lineMetrics.empty() ? lineMetrics.size() - 1u : lineMetrics.size();
            for (std::size_t line = 0u; line < lineCount; ++line)
                geometry->continuationBottom = AddCoordinate(geometry->continuationBottom, lineMetrics[line].height);
            append(first, end, std::move(geometry));
            next->last.preparedSourceUnits += units;
            ++next->last.preparedChunks;
            first = end;
        }
        if (first == blocks.size())
            break;
        if (compact)
            throw std::logic_error("Rich geometry chunk bound exceeded");
    }
    const bool exactPlacement = std::all_of(next->chunks.begin(), next->chunks.end(), [](const ChunkPlacement& chunk) {
        return chunk.geometry->layout.PositionedExactly() && chunk.geometry->layout.VerticalOrigin() == chunk.y;
    });
    if (! exactPlacement || (compatible && state_->reference))
    {
        Projection joined{
            std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
        JoinBlocks(joined, blocks, 0u, blocks.size(), mode);
        if (compatible && state_->reference && SameStyles(joined, state_->reference->Content()))
        {
            next->reference = state_->reference;
            ++next->last.wholeLayoutsReused;
        }
        else if (! exactPlacement)
        {
            auto reference = std::make_shared<PreparedLayout>();
            reference->PrepareOwned(factory, joined, options);
            next->reference = std::move(reference);
            ++next->last.wholeLayoutsPrepared;
            next->last.wholeLayoutSourceUnits = source.Length();
        }
    }
    if (! exactPlacement)
    {
        // Extremely small typography can dominate the hidden origin line.
        // Preserve the whole-layout contract if its measured prefix is inexact.
        next->referenceForGeometry = true;
        next->height               = next->reference->Height();
        next->chunks.clear();
    }
    // Nothing above touches the published source/geometry pair. Invalid input,
    // allocation and DirectWrite failures leave the prior pair unchanged.
    state_.swap(next);
    ready_ = true;
    ++generation_;
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
        return rectangles;
    for (auto index = ChunkAt(first); index < state.chunks.size(); ++index)
    {
        const auto& chunk = state.chunks[index];
        if (chunk.displayStart >= last)
            break;
        const auto end = std::min(last - chunk.displayStart, chunk.geometry->layout.Content().text.size());
        auto local     = chunk.geometry->layout.Range(first > chunk.displayStart ? first - chunk.displayStart : 0u, end);
        rectangles.insert(rectangles.end(), local.begin(), local.end());
    }
    return rectangles;
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
void PreparedDocumentLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom) const
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
        state.reference->Draw(target, brush, origin);
        return;
    }
    for (const auto& chunk : state.chunks)
    {
        if (! canCull || (chunk.geometry->drawingTop - padding < bottom && chunk.geometry->drawingBottom + padding > top))
            chunk.geometry->layout.Draw(target, brush, origin);
    }
}
} // namespace DxUi::RichText
