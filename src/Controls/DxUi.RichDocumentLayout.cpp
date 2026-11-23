#include "DxUi.PreparedLayoutAccess.h"
#include "DxUi.RichLayoutOptions.h"
#include "Support/WorkCounters.h"
#include <DxUi/Failure.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <wil/resource.h>

namespace DxUi
{
// DxUi.RetainedCaches.h: the incremental-versus-fresh oracle's fresh scene prepares without reuse.
[[nodiscard]] bool IsFreshPreparationForTest() noexcept;
} // namespace DxUi
namespace DxUi::RichText
{
namespace
{
// Reserve the final slot for one whole-layout compatibility resource.
constexpr std::size_t TargetChunkUnits = 1024u, MaximumChunks = 127u;
bool SameStyles(const std::vector<ProjectionBlock>& blocks, const Projection& right) noexcept
{
    // Compare the exact joined display/runs without materializing source, maps
    // or a whole Projection merely to reject a compatibility-cache candidate.
    std::size_t displayStart = 0u, runIndex = 0u;
    std::optional<StyleRun> joined;
    const auto flush = [&]() noexcept
    {
        if (! joined)
            return true;
        if (runIndex == right.runs.size())
            return false;
        const auto& expected = right.runs[runIndex++];
        return joined->start == expected.start && joined->length == expected.length && joined->flags == expected.flags && joined->heading == expected.heading;
    };
    for (const auto& item : blocks)
    {
        const auto& block = *item.content;
        if (block.text.size() > right.text.size() - displayStart || std::wstring_view(right.text).substr(displayStart, block.text.size()) != block.text)
            return false;
        for (const auto& run : block.runs)
        {
            const StyleRun next{displayStart + run.start, run.length, run.flags, run.heading};
            if (joined && joined->start + joined->length == next.start && joined->flags == next.flags && joined->heading == next.heading)
                joined->length += next.length;
            else
            {
                if (! flush())
                    return false;
                joined = next;
            }
        }
        displayStart += block.text.size();
    }
    return displayStart == right.text.size() && flush() && runIndex == right.runs.size();
}
bool SameOptions(const LayoutOptions& left, const LayoutOptions& right) noexcept
{
    return Detail::SameLayoutOptions(left, right, Detail::LayoutOptionsUse::Text);
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
    // Laid out at its own origin 0 (W7.3): every coordinate is chunk-local and the document adds the chunk's placement.
    PreparedLayout layout;
    // A nonfinal chunk's terminal LF creates a phantom empty line that the next chunk's first line replaces; its
    // continuation height excludes that line. A final chunk contributes its whole layout height.
    // Sums of the layout's line heights in double precision, so a placement is the exact sum of every line before it.
    double continuationHeight{}, finalHeight{};
    float drawingTop{}, drawingBottom{};
    bool emptyDisplay{};
    [[nodiscard]] double HeightAt(bool final) const noexcept
    {
        return final ? finalHeight : continuationHeight;
    }
};
struct ChunkPlacement
{
    std::size_t sourceStart{}, displayStart{}, firstBlock{}, blockCount{};
    // top: the double-precision sum of every line height before the chunk; y: its single-precision value.
    double top{};
    float y{}, height{};
    // Null for an estimated placeholder of a partial publication: nothing reads geometry from it.
    std::shared_ptr<const GeometryChunk> geometry;
    // A partial (viewport-first) publication's stand-in for a chunk not yet laid again: the previous geometry of the same
    // unchanged blocks, laid with other options or before the current preparation. It may stand in again, never be reused.
    bool stale{};
};
// One chunk of a preparation's plan, in document order. Exactly one of geometry (reused, or laid by this preparation)
// and standIn may be set before laying; laying sets geometry.
struct PlannedChunk
{
    std::size_t first{}, end{}, units{};
    std::shared_ptr<const GeometryChunk> geometry;
    const ChunkPlacement* standIn{};
    bool reused{};
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
    Detail::PreparedLayoutAccess::DocumentLineSpacing spacing;
    wil::com_ptr<IDWriteFactory> factory;
    std::vector<ChunkPlacement> chunks;
    std::shared_ptr<const PreparedLayout> reference;
    bool referenceForGeometry{};
    // A partial publication: some chunks are stale stand-ins or estimated placeholders. It is not Ready(); only the
    // library's viewport drawing (Detail::PreparedDocumentLayoutAccess) reads it, and only its laid chunks.
    bool partial{};
    float height{}, width{};
    LayoutPreparation last;
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
        Match,
        Plan,
        Lay,
        Place,
        Reference,
        Publish
    };
    Preparation() : next(std::make_unique<State>()), matched(0u), plan(0u), forcedStarts(0u)
    {
    }
    std::unique_ptr<State> next;
    // The publication this preparation started from, once a viewport-first publication replaced it. The raw pointers
    // below borrow its immutable chunks until this preparation publishes or is cancelled.
    std::unique_ptr<State> previous;
    SourceChangeRange change;
    // Per block: the previous chunk whose blocks start here unchanged. Reused when the options are compatible, a
    // viewport-first stand-in otherwise.
    std::vector<const ChunkPlacement*> matched;
    std::vector<PlannedChunk> plan;
    std::vector<std::size_t> forcedStarts;
    const ChunkPlacement* scanning{};
    std::size_t scanBlock{}, scanLocal{}, first{}, end{}, units{}, displayUnits{}, matchedChunks{};
    // The owner's current viewport, given to each advance through Detail::PreparedDocumentLayoutAccess. Without one,
    // chunks are laid in document order and only the complete result is published.
    Detail::DocumentViewport viewport;
    // The previous publication's height per source unit: the placeholder estimate until a chunk's height is known.
    double fallbackPerUnit{};
    Phase phase = Phase::Plan;
    bool compatible{}, matching{}, compact{}, exactPlacement = true, fresh{};
    // publishedOnce: a partial publication of this preparation happened; windowCurrent: the latest one shows every chunk
    // meeting the current viewport window.
    bool publishedOnce{}, windowCurrent{};

    [[nodiscard]] const State& Previous(const State& current) const noexcept
    {
        return previous ? *previous : current;
    }
    const ChunkPlacement* Candidate(const State& previousState, std::size_t block) const
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
        const auto found =
            std::lower_bound(previousState.chunks.begin(), previousState.chunks.end(), oldStart, [](const ChunkPlacement& chunk, std::size_t offset) {
            return chunk.sourceStart < offset;
        });
        // A placeholder has no geometry to stand in; a stale stand-in may stand in again (never reused, see Plan).
        if (found == previousState.chunks.end() || found->sourceStart != oldStart || found->blockCount > blocks.size() - block || ! found->geometry ||
            found->geometry->emptyDisplay)
            return nullptr;
        const auto oldLength = found->geometry->layout.Content().source.size();
        if (change.kind != SourceChangeKind::Unchanged && oldStart < change.begin && oldLength > change.begin - oldStart)
            return nullptr;
        return &*found;
    }
    const ChunkPlacement* Matched(std::size_t block) const noexcept
    {
        return ! compact && block < matched.size() ? matched[block] : nullptr;
    }
    // An edited range of an incremental preparation grows up to twice the target, to the next unchanged chunk, so an
    // edit re-lays the chunk that contains it instead of splitting off a remainder with no previous geometry to show.
    [[nodiscard]] std::size_t GrowthLimit() const noexcept
    {
        return matching && compatible && matchedChunks && ! compact ? 2u * TargetChunkUnits : TargetChunkUnits;
    }
    // Source units an entry covers, from the block starts.
    [[nodiscard]] std::size_t EntryUnits(const PlannedChunk& entry) const noexcept
    {
        const auto& blocks = next->projection.Blocks();
        const auto stop    = entry.end < blocks.size() ? blocks[entry.end].sourceStart : next->source.Length();
        return stop - blocks[entry.first].sourceStart;
    }
    // Estimate rule (W7.3): a chunk not laid and without a stand-in is as tall as its source units times the height per
    // unit of the plan's chunks whose heights are known (laid, reused or stand-in); before any is known, the previous
    // publication's height per unit; without one, 0.
    [[nodiscard]] double PerUnit() const noexcept
    {
        double height = 0.0, known = 0.0;
        for (std::size_t index = 0u; index < plan.size(); ++index)
            if (const auto* geometry = plan[index].geometry ? plan[index].geometry.get() : plan[index].standIn ? plan[index].standIn->geometry.get() : nullptr)
            {
                height += geometry->HeightAt(index + 1u == plan.size());
                known += static_cast<double>(EntryUnits(plan[index]));
            }
        return known > 0.0 ? height / known : fallbackPerUnit;
    }
    [[nodiscard]] double EstimatedHeight(std::size_t index, double perUnit) const noexcept
    {
        const auto& entry = plan[index];
        if (const auto* geometry = entry.geometry ? entry.geometry.get() : entry.standIn ? entry.standIn->geometry.get() : nullptr)
            return geometry->HeightAt(index + 1u == plan.size());
        return perUnit * static_cast<double>(EntryUnits(entry));
    }
    // The viewport with one viewport height of overscan above and below, centered on the focus when one is given.
    [[nodiscard]] std::pair<double, double> Window(double perUnit) const noexcept
    {
        auto top = static_cast<double>(viewport.top), bottom = static_cast<double>(viewport.bottom);
        if (viewport.focus != Detail::DocumentViewport::NoFocus)
        {
            const auto& blocks = next->projection.Blocks();
            double y           = 0.0;
            for (std::size_t index = 0u; index < plan.size(); ++index)
            {
                const auto height  = EstimatedHeight(index, perUnit);
                const auto start   = blocks[plan[index].first].sourceStart;
                const auto covered = EntryUnits(plan[index]);
                if (viewport.focus < start + covered || index + 1u == plan.size())
                {
                    y += covered
                             ? height * static_cast<double>(std::min(viewport.focus - std::min(viewport.focus, start), covered)) / static_cast<double>(covered)
                             : 0.0;
                    break;
                }
                y += height;
            }
            const auto half = (bottom - top) * 0.5;
            top             = y - half;
            bottom          = y + half;
        }
        const auto overscan = bottom - top;
        return {top - overscan, bottom + overscan};
    }
    [[nodiscard]] bool ForcedCut(std::size_t block) const noexcept
    {
        const auto start = next->projection.Blocks()[block].sourceStart;
        return std::binary_search(forcedStarts.begin(), forcedStarts.end(), start);
    }
};
#pragma warning(pop)
PreparedDocumentLayout::PreparedDocumentLayout() noexcept  = default;
PreparedDocumentLayout::~PreparedDocumentLayout() noexcept = default;
const PreparedDocumentLayout::State& PreparedDocumentLayout::Prepared() const
{
    if (! Ready())
        Failure::FailContract(E_INVALIDARG);
    return *state_;
}
bool PreparedDocumentLayout::Ready() const noexcept
{
    // A partial (viewport-first) publication is never Ready: no query answers from stale or estimated geometry.
    return ready_ && static_cast<bool>(state_) && ! state_->partial;
}
float PreparedDocumentLayout::Height() const noexcept
{
    return Ready() ? state_->height : 0.0f;
}
float PreparedDocumentLayout::Width() const noexcept
{
    return Ready() ? state_->width : 0.0f;
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
    if (const auto status = TryPrepare(factory, std::move(source), revision, mode, options); FAILED(status))
        Failure::FailContract(status);
}
HRESULT PreparedDocumentLayout::TryPrepare(IDWriteFactory* factory, SourceSnapshot source, std::uint64_t revision, ViewMode mode, const LayoutOptions& options)
{
    const Failure::Scope failureScope;
    const auto admitted = BeginPreparation(factory, std::move(source), revision, mode, options);
    if (admitted == LayoutPreparationStatus::Complete)
        return S_OK;
    if (admitted == LayoutPreparationStatus::Failed)
        return result_;
    const LayoutWorkBudget unlimited{(std::numeric_limits<std::size_t>::max)(), (std::numeric_limits<std::size_t>::max)()};
    for (;;)
    {
        const auto advance = AdvancePreparation(unlimited);
        if (advance.status == LayoutPreparationStatus::Failed)
            return advance.result;
        if (advance.status != LayoutPreparationStatus::Pending)
            return S_OK;
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
    const Failure::Scope failureScope;
    result_ = S_OK;
    if (! factory || (mode != ViewMode::Source && mode != ViewMode::Rich))
    {
        result_ = E_INVALIDARG;
        return LayoutPreparationStatus::Failed;
    }
    // Retain compatible pending work so the host can resume preparation in
    // bounded steps on the thread that owns the layout factory.
    if (pending_ && pending_->next->factory.get() == factory && pending_->next->projection.Mode() == mode &&
        pending_->next->projection.Revision() == revision && SameOptions(pending_->next->options, options) &&
        source.ChangedRangeFrom(pending_->next->source).kind == SourceChangeKind::Unchanged)
        return LayoutPreparationStatus::Pending;
    // The incremental-versus-fresh oracle's fresh scene prepares without any previous publication.
    const bool fresh      = IsFreshPreparationForTest();
    const bool sameEngine = ! fresh && state_ && state_->factory.get() == factory && state_->projection.Mode() == mode;
    bool compatible       = sameEngine && SameOptions(state_->options, options);
    const auto change     = state_ ? source.ChangedRangeFrom(state_->source) : SourceChangeRange{};
    if (compatible && change.kind == SourceChangeKind::Unchanged && ! state_->partial)
    {
        if (const auto status = state_->projection.TryPrepare(source, revision, mode); status != SourceResult::Success)
        {
            result_ = status == SourceResult::CapacityExceeded ? E_BOUNDS : E_INVALIDARG;
            return LayoutPreparationStatus::Failed;
        }
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
    if (const auto status = next.projection.TryPrepare(source, revision, mode); status != SourceResult::Success)
    {
        result_ = status == SourceResult::CapacityExceeded ? E_BOUNDS : E_INVALIDARG;
        return LayoutPreparationStatus::Failed;
    }
    next.source  = std::move(source);
    next.options = options;
    next.factory = factory;
    // Uniform line spacing (an explicit line height or any script run) depends on the whole document's runs, as in one
    // whole layout. A change of that input re-lays every chunk.
    next.spacing = {options.fontSizeDip, false};
    for (const auto& block : next.projection.Blocks())
        Detail::PreparedLayoutAccess::AccumulateLineSpacing(next.spacing, block.content->runs, options);
    if (options.lineHeightMultiplier <= 0.0f && ! next.spacing.scripts)
        next.spacing.size = options.fontSizeDip; // natural spacing: the largest run does not space other lines
    compatible          = compatible && state_->spacing == next.spacing;
    request->compatible = compatible;
    request->change     = change;
    request->fresh      = fresh;
    if (state_ && state_->height > 0.0f && state_->projection.SourceLength())
        request->fallbackPerUnit = static_cast<double>(state_->height) / static_cast<double>(state_->projection.SourceLength());
    if (sameEngine && change.kind != SourceChangeKind::FullReplacement && ! state_->chunks.empty())
    {
        // Pair unchanged blocks with the previous chunks: reused when compatible, viewport-first stand-ins otherwise.
        request->matched.resize(next.projection.Blocks().size(), nullptr);
        request->matching = true;
        request->phase    = Preparation::Phase::Match;
    }
#if DXUI_ENABLE_DIAGNOSTICS
    if (fresh && Detail::testRichRecordChunkPlan && Detail::testRichPublishedSourceLength == next.source.Length())
        request->forcedStarts = Detail::testRichPublishedChunkStarts;
#endif
    // Replacing a pending request is atomic even if cloning/indexing/options fail.
    pending_.swap(request);
    return LayoutPreparationStatus::Pending;
}
LayoutAdvance PreparedDocumentLayout::AdvancePreparation(const LayoutWorkBudget& budget)
{
    const Failure::Scope failureScope;
    if (! pending_)
        return {};
    LayoutAdvance result;
    // At most one viewport publication per advance: a later one in the same advance would be seen by no frame.
    bool publishedThisAdvance = false;
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps)
    {
        result.status = LayoutPreparationStatus::Failed;
        result.result = E_INVALIDARG;
        return result;
    }
    const auto fail = [&](HRESULT error)
    {
        result_       = error;
        result.status = LayoutPreparationStatus::Failed;
        result.result = error;
        return result;
    };
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
    auto& work         = DxUi::Detail::Work().rich;
    // Places the plan in document order: each chunk at the double-precision sum of the heights before it (W7.3). A chunk
    // not laid yet shows its stand-in (stale) or, without one, is an estimated placeholder with no geometry.
    const auto place = [&](State& target, double perUnit)
    {
        target.chunks.clear();
        target.chunks.reserve(request.plan.size());
        target.height = target.width = 0.0f;
        target.partial               = false;
        double running               = 0.0;
        for (std::size_t index = 0u; index < request.plan.size(); ++index)
        {
            const auto& entry   = request.plan[index];
            const bool final    = index + 1u == request.plan.size();
            const bool stale    = ! entry.geometry && entry.standIn;
            const auto geometry = entry.geometry ? entry.geometry : entry.standIn ? entry.standIn->geometry : std::shared_ptr<const GeometryChunk>{};
            const auto height   = geometry ? geometry->HeightAt(final) : perUnit * static_cast<double>(request.EntryUnits(entry));
            target.partial      = target.partial || ! entry.geometry;
            auto top            = running;
#if DXUI_ENABLE_DIAGNOSTICS
            if (! request.fresh && index && Detail::testRichChunkOffsetError != 0.0f)
                top += Detail::testRichChunkOffsetError;
#endif
            if (geometry)
                target.width = std::max(target.width, geometry->layout.HorizontalTextBounds().second);
            target.chunks.push_back({blocks[entry.first].sourceStart,
                                     blocks[entry.first].displayStart,
                                     entry.first,
                                     entry.end - entry.first,
                                     top,
                                     static_cast<float>(top),
                                     static_cast<float>(height),
                                     geometry,
                                     stale});
            running += height;
        }
        target.height = static_cast<float>(running);
        return std::isfinite(target.height);
    };
    for (;;)
    {
        switch (request.phase)
        {
            case Preparation::Phase::Match:
            {
                if (request.scanBlock == blocks.size())
                {
                    request.phase = Preparation::Phase::Plan;
                    continue;
                }
                if (result.blockSteps == budget.maximumBlockSteps)
                    return leave(LayoutPreparationStatus::Pending);
                ++result.blockSteps;
                const auto& previous = request.Previous(*state_);
                if (! request.scanning)
                    request.scanning = request.Candidate(previous, request.scanBlock);
                if (! request.scanning)
                {
                    ++request.scanBlock;
                    continue;
                }
                const auto& old       = *request.scanning;
                const auto& oldBlocks = previous.projection.Blocks();
                if (blocks[request.scanBlock + request.scanLocal].content->source != oldBlocks[old.firstBlock + request.scanLocal].content->source)
                {
                    ++request.scanBlock;
                    request.scanLocal = 0u;
                    request.scanning  = nullptr;
                }
                else if (++request.scanLocal == old.blockCount)
                {
                    request.matched[request.scanBlock] = request.scanning;
                    ++request.matchedChunks;
                    request.scanBlock += old.blockCount;
                    request.scanLocal = 0u;
                    request.scanning  = nullptr;
                }
                continue;
            }
            case Preparation::Phase::Plan:
            {
                if (request.first == blocks.size())
                {
                    request.phase = Preparation::Phase::Lay;
                    continue;
                }
                if (request.plan.size() == MaximumChunks)
                {
                    if (request.compact)
                        return fail(E_BOUNDS);
                    if (result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    // Discard only the plan; nothing has been laid yet.
                    request.compact = true;
                    request.first = request.end = request.units = request.displayUnits = 0u;
                    request.plan.clear();
                    request.forcedStarts.clear();
                    continue;
                }
                const auto* old = request.forcedStarts.empty() ? request.Matched(request.first) : nullptr;
                // A stale stand-in (from a partial publication) was laid with other options or before an edit: it stands in
                // for its blocks while they are laid again, but is never reused.
                if (request.end == request.first && old && request.compatible && ! old->stale)
                {
                    if (result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    request.plan.push_back({request.first, request.first + old->blockCount, 0u, old->geometry, nullptr, true});
                    request.first += old->blockCount;
                    request.end = request.first;
                    continue;
                }
                const auto* standIn = request.compatible && old && ! old->stale ? nullptr : old;
                for (;;)
                {
                    const auto at = request.end;
                    const bool take =
                        at < blocks.size() && (at == request.first              ? true
                                               : ! request.forcedStarts.empty() ? ! request.ForcedCut(at)
                                               : standIn ? at < request.first + standIn->blockCount
                                                         : request.displayUnits == 0u || (request.units < request.GrowthLimit() && ! request.Matched(at)));
                    if (! take)
                        break;
                    if (result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    request.units += blocks[at].content->source.size();
                    request.displayUnits += blocks[at].content->text.size();
                    ++request.end;
                }
                request.plan.push_back({request.first, request.end, request.units, nullptr, standIn, false});
                request.first = request.end;
                request.units = request.displayUnits = 0u;
                continue;
            }
            case Preparation::Phase::Lay:
            {
                // Choose the next chunk to lay. With a viewport: the first chunk not laid whose estimated placement meets
                // the viewport window; when none is left, publish the window (viewport first) with stand-ins and
                // placeholders for the rest, then lay the rest in document order. The walk over at most 127 planned chunks
                // is one block step; the viewport is read again on every advance, so a scroll moves the window.
                if (result.blockSteps == budget.maximumBlockSteps)
                    return leave(LayoutPreparationStatus::Pending);
                ++result.blockSteps;
                const auto perUnit  = request.PerUnit();
                const bool windowed = request.viewport.active && ! request.fresh;
                const auto none     = request.plan.size();
                std::size_t chosen = none, firstUnlaid = none;
                if (windowed)
                {
                    const auto window = request.Window(perUnit);
                    double top        = 0.0;
                    for (std::size_t index = 0u; index < request.plan.size(); ++index)
                    {
                        const auto height = request.EstimatedHeight(index, perUnit);
                        if (! request.plan[index].geometry)
                        {
                            if (firstUnlaid == none)
                                firstUnlaid = index;
                            if (chosen == none && top <= window.second && top + height >= window.first)
                                chosen = index;
                        }
                        top += height;
                    }
                }
                else
                    for (std::size_t index = 0u; index < request.plan.size() && firstUnlaid == none; ++index)
                        if (! request.plan[index].geometry)
                            firstUnlaid = index;
                const bool inWindow = chosen != none;
                if (! inWindow)
                {
                    if (firstUnlaid == none)
                    {
                        request.phase = Preparation::Phase::Place;
                        continue;
                    }
                    if (windowed && ! request.windowCurrent && ! publishedThisAdvance)
                    {
                        // Viewport first: every chunk meeting the window is laid. Publish it now; every other chunk has a
                        // stand-in or an estimated placeholder, so no chunk outside the window is laid before this.
                        auto interim        = std::make_unique<State>();
                        interim->projection = next.projection.Clone();
                        interim->source     = next.source;
                        interim->options    = next.options;
                        interim->spacing    = next.spacing;
                        interim->factory    = next.factory;
                        interim->last       = next.last;
                        if (! place(*interim, perUnit))
                            return fail(E_BOUNDS);
                        // The previous publication stays alive: the plan borrows its chunks.
                        if (! request.previous)
                            request.previous = std::move(state_);
                        state_ = std::move(interim);
                        ready_ = true;
                        ++generation_;
                        request.publishedOnce = request.windowCurrent = publishedThisAdvance = true;
                        ++work.viewportPublications;
                        // The advance goes on within its budget: a document whose rest fits this turn completes in it,
                        // so a first frame is never withheld for an extra turn only because the window was published. A
                        // window chunk found later in this advance is published by the next advance.
                        continue;
                    }
                    chosen = firstUnlaid;
                }
                if (result.layoutsPrepared == budget.maximumLayouts)
                    return leave(LayoutPreparationStatus::Pending);
                auto& entry   = request.plan[chosen];
                auto geometry = std::make_shared<GeometryChunk>();
                Projection joined{
                    std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
                JoinBlocks(joined, blocks, entry.first, entry.end, next.projection.Mode());
                if (const auto status = Detail::PreparedLayoutAccess::Prepare(
                        geometry->layout, next.factory.get(), joined, next.options, false, false, nullptr, 0u, {}, false, &next.spacing);
                    FAILED(status))
                    return fail(status);
                ++result.layoutsPrepared;
                const auto drawingBounds = geometry->layout.DrawingVerticalBounds();
                geometry->drawingTop     = drawingBounds.first;
                geometry->drawingBottom  = drawingBounds.second;
                geometry->finalHeight    = 0.0;
                const auto& text         = geometry->layout.Content().text;
                geometry->emptyDisplay   = text.empty();
                const auto lineMetrics   = geometry->layout.LineMetrics();
                const auto count         = ! text.empty() && text.back() == L'\n' && ! lineMetrics.empty() ? lineMetrics.size() - 1u : lineMetrics.size();
                for (std::size_t line = 0u; line < lineMetrics.size(); ++line)
                {
                    if (line < count)
                        geometry->continuationHeight += lineMetrics[line].height;
                    geometry->finalHeight += lineMetrics[line].height;
                }
                if (! std::isfinite(geometry->finalHeight) || ! std::isfinite(geometry->continuationHeight))
                    return fail(E_BOUNDS);
                // Counted from the request's own viewport, whatever the call: by construction this stays 0.
                if (windowed && ! inWindow && ! request.publishedOnce)
                    ++work.offscreenChunksLaidFirst;
                if (inWindow)
                    request.windowCurrent = false; // a new window chunk: publish the window again once it is complete
                entry.geometry = std::move(geometry);
                next.last.preparedSourceUnits += request.EntryUnits(entry);
                ++next.last.preparedChunks;
                ++work.chunksLaid;
                continue;
            }
            case Preparation::Phase::Place:
            {
                if (result.blockSteps == budget.maximumBlockSteps)
                    return leave(LayoutPreparationStatus::Pending);
                ++result.blockSteps;
                for (const auto& entry : request.plan)
                    if (entry.reused)
                        ++next.last.reusedChunks;
                if (! place(next, 0.0))
                    return fail(E_BOUNDS);
                // An empty displayed suffix would contribute its own empty line, which one whole layout does not have.
                request.exactPlacement = request.plan.size() < 2u || ! request.plan.back().geometry->emptyDisplay;
                request.phase          = Preparation::Phase::Reference;
                continue;
            }
            case Preparation::Phase::Reference:
            {
                const State* previous = request.previous ? request.previous.get() : state_.get();
                if (! request.exactPlacement)
                {
                    if (result.layoutsPrepared == budget.maximumLayouts || result.blockSteps == budget.maximumBlockSteps)
                        return leave(LayoutPreparationStatus::Pending);
                    ++result.blockSteps;
                    // One explicit compatibility work unit, not a hard CPU-time
                    // guarantee. Joining is needed only for a new whole layout.
                    if (request.compatible && previous && previous->reference && SameStyles(blocks, previous->reference->Content()))
                    {
                        next.reference = previous->reference;
                        ++next.last.wholeLayoutsReused;
                    }
                    else
                    {
                        Projection joined{std::wstring(0u, L'\0'),
                                          std::wstring(0u, L'\0'),
                                          std::vector<std::size_t>(0u),
                                          std::vector<SourceUnit>(0u),
                                          std::vector<StyleRun>(0u)};
                        JoinBlocks(joined, blocks, 0u, blocks.size(), next.projection.Mode());
                        auto reference = std::make_shared<PreparedLayout>();
                        if (const auto status = reference->PrepareOwned(next.factory.get(), joined, next.options); FAILED(status))
                            return fail(status);
                        next.reference = std::move(reference);
                        ++result.layoutsPrepared;
                        ++next.last.wholeLayoutsPrepared;
                        ++work.wholeLayoutsLaid;
                        next.last.wholeLayoutSourceUnits = next.source.Length();
                    }
                    next.referenceForGeometry = true;
                    next.height               = next.reference->Height();
                    next.width                = next.reference->HorizontalTextBounds().second;
                    next.chunks.clear();
                }
                request.phase = Preparation::Phase::Publish;
                continue;
            }
            case Preparation::Phase::Publish:
            {
#if DXUI_ENABLE_DIAGNOSTICS
                if (Detail::testRichRecordChunkPlan && ! request.fresh)
                {
                    Detail::testRichPublishedChunkStarts.clear();
                    for (const auto& chunk : next.chunks)
                        Detail::testRichPublishedChunkStarts.push_back(chunk.sourceStart);
                    Detail::testRichPublishedSourceLength = next.source.Length();
                }
#endif
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
    std::size_t result{};
    if (const auto status = TrySourceBoundary(position, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TrySourceBoundary(std::size_t position, std::size_t& output) const
{
    if (! Ready())
        return E_UNEXPECTED;
    const auto& projection = state_->projection;
    if (position > projection.DisplayLength())
        return E_INVALIDARG;
    std::size_t first = 0u, end = projection.SourceLength();
    while (first < end)
    {
        const auto middle = first + (end - first) / 2u;
        if (projection.ToDisplay(middle) < position)
            first = middle + 1u;
        else
            end = middle;
    }
    output = first;
    return S_OK;
}
std::size_t PreparedDocumentLayout::ToDisplay(std::size_t position) const
{
    std::size_t result{};
    if (const auto status = TryToDisplay(position, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryToDisplay(std::size_t position, std::size_t& output) const
{
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.SourceLength())
        return E_INVALIDARG;
    output = state_->projection.ToDisplay(position);
    return S_OK;
}
std::size_t PreparedDocumentLayout::ToSource(std::size_t position, bool trailing) const
{
    std::size_t result{};
    if (const auto status = TryToSource(position, trailing, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryToSource(std::size_t position, bool trailing, std::size_t& output) const
{
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.DisplayLength())
        return E_INVALIDARG;
    output = state_->projection.ToSource(position, trailing);
    return S_OK;
}
std::wstring PreparedDocumentLayout::DisplayText() const
{
    return Prepared().projection.DisplayText();
}
std::size_t PreparedDocumentLayout::ChunkAt(std::size_t position) const
{
    const auto& state = Prepared();
    if (position > state.projection.DisplayLength())
        Failure::FailContract(E_INVALIDARG);
    const auto found = std::upper_bound(
        state.chunks.begin(), state.chunks.end(), position, [](std::size_t offset, const ChunkPlacement& chunk) { return offset < chunk.displayStart; });
    return found == state.chunks.begin() ? 0u : static_cast<std::size_t>(found - state.chunks.begin() - 1);
}
HRESULT PreparedDocumentLayout::TryCaret(std::size_t position, CaretGeometry& result) const
{
    return TryCaret(position, CaretAffinity::Downstream, result);
}
HRESULT PreparedDocumentLayout::TryCaret(std::size_t position, CaretAffinity affinity, CaretGeometry& result) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    const auto& state = *state_;
    if (position > state.projection.DisplayLength())
        return E_INVALIDARG;
    if (state.referenceForGeometry)
        return state.reference->TryCaret(position, affinity, result);
    // Chunks end at physical source lines, so a chunk start is never a soft wrap
    // and both affinities resolve inside the chunk that contains the position.
    const auto& chunk = state.chunks[ChunkAt(position)];
    CaretGeometry local{};
    if (const auto status = chunk.geometry->layout.TryCaret(position - chunk.displayStart, affinity, local); FAILED(status))
        return status;
    local.y = static_cast<float>(chunk.top + local.y);
    result  = local;
    return S_OK;
}
CaretGeometry PreparedDocumentLayout::Caret(std::size_t position) const
{
    CaretGeometry result;
    if (const auto status = TryCaret(position, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
CaretGeometry PreparedDocumentLayout::Caret(std::size_t position, CaretAffinity affinity) const
{
    CaretGeometry result;
    if (const auto status = TryCaret(position, affinity, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
Hit PreparedDocumentLayout::HitTest(float x, float y) const
{
    Hit result{};
    if (const auto status = TryHitTest(x, y, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryHitTest(float x, float y, Hit& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    const auto& state = *state_;
    if (! std::isfinite(x) || ! std::isfinite(y))
        return E_INVALIDARG;
    if (state.referenceForGeometry)
        return state.reference->TryHitTest(x, y, output);
    const auto found  = std::upper_bound(state.chunks.begin(), state.chunks.end(), y, [](float top, const ChunkPlacement& chunk) { return top < chunk.y; });
    const auto& chunk = found == state.chunks.begin() ? state.chunks.front() : *std::prev(found);
    Hit hit{};
    if (const auto status = chunk.geometry->layout.TryHitTest(x, static_cast<float>(static_cast<double>(y) - chunk.top), hit); FAILED(status))
        return status;
    hit.position += chunk.displayStart;
    output = hit;
    return S_OK;
}
std::vector<D2D1_RECT_F> PreparedDocumentLayout::Range(std::size_t first, std::size_t last) const
{
    const Failure::Scope failureScope;
    std::vector<D2D1_RECT_F> result;
    if (const auto status = TryRange(first, last, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    const auto& state = *state_;
    if (first > last || last > state.projection.DisplayLength())
        return E_INVALIDARG;
    if (state.referenceForGeometry)
        return state.reference->TryRange(first, last, output);
    std::vector<D2D1_RECT_F> rectangles(0u);
    for (auto index = first == last ? state.chunks.size() : ChunkAt(first); index < state.chunks.size(); ++index)
    {
        const auto& chunk = state.chunks[index];
        if (chunk.displayStart >= last)
            break;
        const auto end = std::min(last - chunk.displayStart, chunk.geometry->layout.Content().text.size());
        std::vector<D2D1_RECT_F> local;
        if (const auto status = chunk.geometry->layout.TryRange(first > chunk.displayStart ? first - chunk.displayStart : 0u, end, local); FAILED(status))
            return status;
        for (auto& rectangle : local)
        {
            rectangle.top    = static_cast<float>(chunk.top + rectangle.top);
            rectangle.bottom = static_cast<float>(chunk.top + rectangle.bottom);
        }
        rectangles.insert(rectangles.end(), local.begin(), local.end());
    }
    output.swap(rectangles);
    return S_OK;
}
std::size_t PreparedDocumentLayout::PreviousCluster(std::size_t position) const
{
    std::size_t result{};
    if (const auto status = TryPreviousCluster(position, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryPreviousCluster(std::size_t position, std::size_t& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.DisplayLength())
        return E_INVALIDARG;
    if (state_->referenceForGeometry)
        return state_->reference->TryPreviousCluster(position, output);
    auto index         = ChunkAt(position);
    const auto& chunks = state_->chunks;
    if (index && position == chunks[index].displayStart)
        --index;
    const auto& chunk = chunks[index];
    std::size_t local{};
    if (const auto status = chunk.geometry->layout.TryPreviousCluster(position - chunk.displayStart, local); FAILED(status))
        return status;
    output = chunk.displayStart + local;
    return S_OK;
}
std::size_t PreparedDocumentLayout::NextCluster(std::size_t position) const
{
    std::size_t result{};
    if (const auto status = TryNextCluster(position, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryNextCluster(std::size_t position, std::size_t& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.DisplayLength())
        return E_INVALIDARG;
    if (state_->referenceForGeometry)
        return state_->reference->TryNextCluster(position, output);
    const auto& chunk = state_->chunks[ChunkAt(position)];
    std::size_t local{};
    if (const auto status = chunk.geometry->layout.TryNextCluster(position - chunk.displayStart, local); FAILED(status))
        return status;
    output = chunk.displayStart + local;
    return S_OK;
}
std::pair<std::size_t, std::size_t> PreparedDocumentLayout::DeletionRange(std::size_t position, bool backwards) const
{
    std::pair<std::size_t, std::size_t> result{};
    if (const auto status = TryDeletionRange(position, backwards, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryDeletionRange(std::size_t position, bool backwards, std::pair<std::size_t, std::size_t>& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.DisplayLength())
        return E_INVALIDARG;
    if (state_->referenceForGeometry)
        return state_->reference->TryDeletionRange(position, backwards, output);
    auto index         = ChunkAt(position);
    const auto& chunks = state_->chunks;
    if (backwards && index && position == chunks[index].displayStart)
        --index;
    const auto& chunk = chunks[index];
    std::pair<std::size_t, std::size_t> range{};
    if (const auto status = chunk.geometry->layout.TryDeletionRange(position - chunk.displayStart, backwards, range); FAILED(status))
        return status;
    output = {chunk.displayStart + range.first, chunk.displayStart + range.second};
    return S_OK;
}
std::size_t PreparedDocumentLayout::LineBoundary(std::size_t position, bool end) const
{
    std::size_t result{};
    if (const auto status = TryLineBoundary(position, end, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryLineBoundary(std::size_t position, bool end, std::size_t& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    if (position > state_->projection.DisplayLength())
        return E_INVALIDARG;
    if (state_->referenceForGeometry)
        return state_->reference->TryLineBoundary(position, end, output);
    const auto& chunk = state_->chunks[ChunkAt(position)];
    std::size_t local{};
    if (const auto status = chunk.geometry->layout.TryLineBoundary(position - chunk.displayStart, end, local); FAILED(status))
        return status;
    output = chunk.displayStart + local;
    return S_OK;
}
std::pair<std::size_t, std::size_t> PreparedDocumentLayout::VisibleRange(float top, float bottom) const
{
    std::pair<std::size_t, std::size_t> result{};
    if (const auto status = TryVisibleRange(top, bottom, result); FAILED(status))
        Failure::FailContract(status);
    return result;
}
HRESULT PreparedDocumentLayout::TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    const auto& state = *state_;
    if (! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        return E_INVALIDARG;
    if (state.referenceForGeometry)
        return state.reference->TryVisibleRange(top, bottom, output);
    std::optional<std::size_t> first;
    std::size_t last = 0u;
    for (const auto& chunk : state.chunks)
    {
        if (chunk.y >= bottom)
            break;
        if (chunk.y + chunk.height <= top)
            continue;
        // The chunk's lines in document coordinates (its placement plus each line's chunk-local caret), as a whole
        // layout's VisibleRange tests them.
        const auto& layout     = chunk.geometry->layout;
        const auto chunkBottom = std::min(bottom, chunk.y + chunk.height);
        std::size_t lineStart  = 0u;
        std::optional<std::size_t> chunkFirst;
        std::size_t chunkLast  = 0u;
        const auto lineMetrics = layout.LineMetrics();
        // Line starts plus the final end offset, as PreparedLayout::VisibleRange walks them.
        for (std::size_t line = 0u; line <= lineMetrics.size(); ++line)
        {
            CaretGeometry caret{};
            if (const auto status = layout.TryCaret(lineStart, caret); FAILED(status))
                return status;
            const auto lineTop = static_cast<float>(chunk.top + caret.y);
            if (lineTop >= chunkBottom)
                break;
            const auto lineEnd = line < lineMetrics.size() ? lineStart + lineMetrics[line].length : layout.Content().text.size();
            // Both edges rounded once from the double-precision placement, as the caret reports them: a line ending where the
            // next begins is not visible from that next line's top.
            if (static_cast<float>(chunk.top + caret.y + caret.height) > top)
            {
                if (! chunkFirst)
                    chunkFirst = lineStart;
                chunkLast = line < lineMetrics.size() ? lineEnd : layout.Content().text.size();
            }
            lineStart = lineEnd;
        }
        const std::pair<std::size_t, std::size_t> range{chunkFirst.value_or(0u), chunkLast};
        // A collapsed range at the final display ACP can be a visible empty
        // line/end caret. Only {0,0} for a nonempty layout means no line here.
        if (range.first == range.second && range.first != chunk.geometry->layout.Content().text.size())
            continue;
        if (! first)
            first = chunk.displayStart + range.first;
        last = chunk.displayStart + range.second;
    }
    output = {first.value_or(0u), last};
    return S_OK;
}
void PreparedDocumentLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom, const LinkDrawing* links) const
{
    if (const auto status = TryDraw(target, brush, origin, top, bottom, links); FAILED(status))
        Failure::FailContract(status);
}
HRESULT PreparedDocumentLayout::TryDraw(
    ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom, const LinkDrawing* links) const
{
    const Failure::Scope failureScope;
    if (! Ready())
        return E_UNEXPECTED;
    const auto& state = *state_;
    if (! target || ! brush || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        return E_INVALIDARG;
    D2D1_MATRIX_3X2_F transform{};
    target->GetTransform(&transform);
    float dpiX = 0.0f, dpiY = 0.0f;
    target->GetDpi(&dpiX, &dpiY);
    const auto scale   = dpiY / 96.0f * transform._22;
    const bool canCull = transform._12 == 0.0f && transform._21 == 0.0f && transform._11 > 0.0f && std::isfinite(scale) && scale > 0.0f;
    // Public DirectWrite overhang metrics include glyph ink, but not hinting or
    // strikethrough. Union with logical metrics, allow two target pixels plus
    // float cancellation, and draw all bounded chunks for non-axis transforms.
    const auto padding = canCull ? 2.0f / scale + 0.125f : 0.0f;
    // Reference geometry is prepared before publication. Rasterization never
    // constructs layouts, joins projections or changes preparation statistics.
    if (state.reference)
    {
        return state.reference->TryDraw(target, brush, origin, links);
    }
    for (const auto& chunk : state.chunks)
    {
        // Chunk drawing bounds are chunk-local; the chunk is drawn at its placement. Direct2D snaps each glyph run's
        // baseline in device space from that final origin, so a chunk draws the same pixels wherever its placement puts
        // it for a given document origin.
        if (! canCull || (chunk.y + chunk.geometry->drawingTop - padding < bottom && chunk.y + chunk.geometry->drawingBottom + padding > top))
        {
            auto local = links ? *links : LinkDrawing{};
            local.displayOrigin += chunk.displayStart;
            const auto placed = D2D1::Point2F(origin.x, origin.y + chunk.y);
            if (const auto status = chunk.geometry->layout.TryDraw(target, brush, placed, links ? &local : nullptr); FAILED(status))
                return status;
        }
    }
    return S_OK;
}
LayoutAdvance Detail::PreparedDocumentLayoutAccess::Advance(PreparedDocumentLayout& layout, const LayoutWorkBudget& budget, const DocumentViewport& viewport)
{
    // The viewport is the owner's current one: read again on every advance.
    if (layout.pending_)
        layout.pending_->viewport = viewport;
    return layout.AdvancePreparation(budget);
}
bool Detail::PreparedDocumentLayoutAccess::Partial(const PreparedDocumentLayout& layout) noexcept
{
    return layout.ready_ && layout.state_ && layout.state_->partial;
}
HRESULT Detail::PreparedDocumentLayoutAccess::DrawPartial(
    const PreparedDocumentLayout& layout, ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, float top, float bottom)
{
    const Failure::Scope failureScope;
    if (! Partial(layout))
        return E_UNEXPECTED;
    if (! target || ! brush || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        return E_INVALIDARG;
    // Only chunks laid for this publication draw; stale stand-ins and placeholders are outside the viewport by
    // construction and draw nothing even if a later scroll brings them in before completion.
    for (const auto& chunk : layout.state_->chunks)
    {
        if (! chunk.geometry || chunk.stale)
            continue;
        if (chunk.y + chunk.geometry->drawingTop >= bottom || chunk.y + chunk.geometry->drawingBottom <= top)
            continue;
        if (const auto status = chunk.geometry->layout.TryDraw(target, brush, D2D1::Point2F(origin.x, origin.y + chunk.y)); FAILED(status))
            return status;
    }
    return S_OK;
}
HRESULT Detail::PreparedDocumentLayoutAccess::PartialSourceTop(const PreparedDocumentLayout& layout, std::size_t source, float& y)
{
    const Failure::Scope failureScope;
    if (! Partial(layout))
        return E_UNEXPECTED;
    const auto& state = *layout.state_;
    if (source > state.projection.SourceLength())
        return E_INVALIDARG;
    const auto display = state.projection.ToDisplay(source);
    const auto found   = std::upper_bound(
        state.chunks.begin(), state.chunks.end(), display, [](std::size_t offset, const ChunkPlacement& chunk) { return offset < chunk.displayStart; });
    if (found == state.chunks.begin())
        return E_PENDING;
    const auto& chunk = *std::prev(found);
    // Only a laid chunk answers; a stand-in or placeholder is pending.
    if (! chunk.geometry || chunk.stale)
        return E_PENDING;
    CaretGeometry caret{};
    if (const auto status = chunk.geometry->layout.TryCaret(display - chunk.displayStart, caret); FAILED(status))
        return status;
    y = static_cast<float>(chunk.top + caret.y);
    return S_OK;
}
std::vector<std::size_t> Detail::PreparedDocumentLayoutAccess::ChunkDisplayStarts(const PreparedDocumentLayout& layout)
{
    std::vector<std::size_t> starts;
    if (layout.state_ && ! layout.state_->referenceForGeometry)
        for (const auto& chunk : layout.state_->chunks)
            starts.push_back(chunk.displayStart);
    return starts;
}
} // namespace DxUi::RichText
