#include "DxUi.RichSemanticGeometry.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <wil/resource.h>

namespace DxUi::RichText::Detail
{
namespace
{
struct SemanticBox
{
    PreparedLayout layout;
    SemanticParagraph paragraph;
    float x{}, y{}, width{}, height{};
    D2D1_RECT_F frame{};
};
bool SameOptions(const LayoutOptions& a, const LayoutOptions& b) noexcept
{
    return a.widthDip == b.widthDip && a.fontSizeDip == b.fontSizeDip && a.fontFamily == b.fontFamily && a.codeFontFamily == b.codeFontFamily &&
           a.locale == b.locale && a.readingDirection == b.readingDirection && a.textAlignment == b.textAlignment;
}
void Union(std::optional<D2D1_RECT_F>& bounds, const D2D1_RECT_F& rectangle) noexcept
{
    if (! bounds)
        bounds = rectangle;
    else
    {
        bounds->left   = std::min(bounds->left, rectangle.left);
        bounds->top    = std::min(bounds->top, rectangle.top);
        bounds->right  = std::max(bounds->right, rectangle.right);
        bounds->bottom = std::max(bounds->bottom, rectangle.bottom);
    }
}
float AddHeight(float y, float height)
{
    volatile float next = y + height;
    if (! std::isfinite(next) || next < y)
        throw std::overflow_error("Nonfinite semantic geometry extent");
    return next;
}
void IdentityProjection(const SemanticDocument& document, MarkdownRange range, Projection& projection)
{
    const auto text     = document.Map().Display().substr(range.first, range.last - range.first);
    projection.mode     = ViewMode::Source; // Private display identity, never raw model ACPs.
    projection.revision = document.Syntax()->Revision();
    projection.source.assign(text);
    projection.text.assign(text);
    projection.sourceToDisplay.resize(text.size() + 1u);
    projection.units.resize(text.size());
    for (std::size_t i = 0u; i <= text.size(); ++i)
    {
        projection.sourceToDisplay[i] = i;
        if (i < text.size())
            projection.units[i] = {i, i + 1u};
    }
    const auto runs  = document.Runs();
    const auto first = std::lower_bound(runs.begin(), runs.end(), range.first, [](const auto& run, auto offset) { return run.start + run.length <= offset; });
    for (auto run = first; run != runs.end() && run->start < range.last; ++run)
    {
        const auto begin = std::max(run->start, range.first), end = std::min(run->start + run->length, range.last);
        projection.runs.push_back({begin - range.first, end - begin, run->flags, run->heading});
    }
}
} // namespace
struct PreparedSemanticGeometry::State
{
    State(std::shared_ptr<const SemanticDocument> input, IDWriteFactory* writeFactory, const LayoutOptions& inputOptions)
        : document(std::move(input)),
          factory(writeFactory),
          options(inputOptions),
          boxes(0u),
          bounds(document->Elements().size())
    {
    }
    std::shared_ptr<const SemanticDocument> document;
    wil::com_ptr<IDWriteFactory> factory;
    LayoutOptions options;
    std::vector<std::unique_ptr<SemanticBox>> boxes;
    std::vector<std::optional<D2D1_RECT_F>> bounds;
    std::vector<std::pair<std::size_t, std::size_t>> sourceLines;
    float height{};
    const SemanticBox& BoxAt(std::size_t position) const
    {
        if (position > document->Map().Display().size() || boxes.empty())
            throw std::out_of_range("Invalid semantic display offset");
        auto found = std::lower_bound(boxes.begin(), boxes.end(), position, [](const auto& box, auto offset) { return box->paragraph.display.last <= offset; });
        if (found == boxes.end())
            found = std::prev(boxes.end());
        else if ((*found)->paragraph.display.first > position && found != boxes.begin())
            --found;
        return **found;
    }
};
struct PreparedSemanticGeometry::Preparation
{
    enum class Phase
    {
        Layouts,
        RowPlacement,
        SourceLines,
        InlineBounds,
        ParentBounds,
        Publish
    };
    explicit Preparation(std::shared_ptr<const SemanticDocument> document, IDWriteFactory* factory, const LayoutOptions& options)
        : state(std::make_unique<State>(std::move(document), factory, options))
    {
    }
    std::unique_ptr<State> state;
    Phase phase = Phase::Layouts;
    std::size_t paragraph{}, rowStart{}, rowPlacement{}, rowTable = MarkdownNoNode, rowIndex{}, boundsIndex{};
    float y{}, rowHeight{};
    bool rowActive{}, finalEmpty{};
    std::size_t lineBox{}, lineOffset{}, sourceLineStart{};
    void FinishRow()
    {
        rowPlacement = rowStart;
        phase        = Phase::RowPlacement;
    }
    void LayoutOne()
    {
        auto& next            = *state;
        const auto paragraphs = next.document->Paragraphs();
        if (paragraph == paragraphs.size())
        {
            if (rowActive)
            {
                FinishRow();
                return;
            }
            if (! paragraphs.empty() || finalEmpty)
            {
                next.height = std::max(next.options.fontSizeDip, y);
                phase       = Phase::SourceLines;
                return;
            }
        }
        SemanticParagraph content;
        if (paragraph < paragraphs.size())
            content = paragraphs[paragraph];
        else
            content.display = {0u, 0u};
        const bool table = content.table != MarkdownNoNode;
        if (rowActive && (! table || content.table != rowTable || content.row != rowIndex))
        {
            FinishRow();
            return;
        }
        auto box              = std::make_unique<SemanticBox>();
        box->paragraph        = content;
        const bool rtl        = next.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
        const float indent    = std::min(next.options.widthDip - 1.0f, static_cast<float>(std::min(content.indent, 64u)) * 16.0f);
        box->x                = rtl ? 0.0f : indent;
        box->y                = y;
        box->width            = next.options.widthDip - indent;
        LayoutOptions options = next.options;
        if (table)
        {
            if (! content.columns || content.column >= content.columns)
                throw std::logic_error("Invalid semantic table placement");
            if (! rowActive)
            {
                rowActive = true;
                rowStart  = next.boxes.size();
                rowTable  = content.table;
                rowIndex  = content.row;
                rowHeight = 0.0f;
            }
            const auto column     = rtl ? content.columns - content.column - 1u : content.column;
            const float cellWidth = box->width / static_cast<float>(content.columns);
            box->x += cellWidth * static_cast<float>(column) + 6.0f;
            box->y += 6.0f;
            box->width = std::max(1.0f, cellWidth - 12.0f);
            if (content.alignment == MarkdownAlignment::Center)
                options.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
            else if (content.alignment == MarkdownAlignment::Left)
                options.textAlignment = rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING;
            else if (content.alignment == MarkdownAlignment::Right)
                options.textAlignment = rtl ? DWRITE_TEXT_ALIGNMENT_LEADING : DWRITE_TEXT_ALIGNMENT_TRAILING;
        }
        options.widthDip = std::max(1.0f, box->width);
        Projection projection{
            std::wstring(0u, L' '), std::wstring(0u, L' '), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
        IdentityProjection(*next.document, content.display, projection);
        box->layout.PrepareSemanticOwned(next.factory.get(), projection, options);
        box->height = box->layout.Height();
        box->frame  = D2D1::RectF(box->x, box->y, box->x + box->width, box->y + box->height);
        if (table)
            rowHeight = std::max(rowHeight, box->height);
        else
            y = AddHeight(y, box->height + 8.0f);
        if (paragraph < paragraphs.size())
            ++paragraph;
        else
            finalEmpty = true;
        Union(next.bounds[content.element], box->frame);
        next.boxes.push_back(std::move(box));
    }
    void PlaceRowOne()
    {
        auto& next = *state;
        if (rowPlacement < next.boxes.size())
        {
            auto& box = *next.boxes[rowPlacement++];
            const float cellWidth =
                (next.options.widthDip - std::min(next.options.widthDip - 1.0f, static_cast<float>(std::min(box.paragraph.indent, 64u)) * 16.0f)) /
                static_cast<float>(box.paragraph.columns);
            box.frame                          = D2D1::RectF(box.x - 6.0f, y, box.x - 6.0f + cellWidth, y + rowHeight + 12.0f);
            next.bounds[box.paragraph.element] = box.frame;
            return;
        }
        y         = AddHeight(y, rowHeight + 12.0f);
        rowActive = false;
        phase     = Phase::Layouts;
    }
};
PreparedSemanticGeometry::PreparedSemanticGeometry() noexcept  = default;
PreparedSemanticGeometry::~PreparedSemanticGeometry() noexcept = default;
LayoutPreparationStatus PreparedSemanticGeometry::Begin(IDWriteFactory* factory, std::shared_ptr<const SemanticDocument> document, const LayoutOptions& options)
{
    if (! factory || ! document || ! std::isfinite(options.widthDip) || options.widthDip <= 0.0f || ! std::isfinite(options.fontSizeDip) ||
        options.fontSizeDip <= 0.0f)
        throw std::invalid_argument("Invalid semantic geometry request");
    const auto same = [&](const State& state) { return state.factory.get() == factory && state.document == document && SameOptions(state.options, options); };
    if (next_ && same(*next_->state))
        return LayoutPreparationStatus::Pending;
    if (state_ && same(*state_))
    {
        next_.reset();
        valid_ = true;
        return LayoutPreparationStatus::Complete;
    }
    auto next = std::make_unique<Preparation>(std::move(document), factory, options);
    next_.swap(next);
    return LayoutPreparationStatus::Pending;
}
LayoutAdvance PreparedSemanticGeometry::Advance(LayoutWorkBudget budget)
{
    if (! budget.maximumLayouts || ! budget.maximumBlockSteps)
        throw std::invalid_argument("Semantic layout budgets must be positive");
    if (! next_)
        return {};
    LayoutAdvance result{LayoutPreparationStatus::Pending};
    try
    {
        while (result.blockSteps < budget.maximumBlockSteps)
        {
            auto& work = *next_;
            if (work.phase == Preparation::Phase::Layouts)
            {
                if (result.layoutsPrepared == budget.maximumLayouts)
                    break;
                const auto oldCount = work.state->boxes.size();
                work.LayoutOne();
                if (work.state->boxes.size() != oldCount)
                    ++result.layoutsPrepared;
            }
            else if (work.phase == Preparation::Phase::RowPlacement)
                work.PlaceRowOne();
            else if (work.phase == Preparation::Phase::SourceLines)
            {
                auto& prepared  = *work.state;
                const auto& map = prepared.document->Map();
                if (work.lineBox == prepared.boxes.size())
                {
                    if (prepared.sourceLines.empty())
                        prepared.sourceLines.emplace_back(0u, map.Source().size());
                    else
                        prepared.sourceLines.back().second = map.Source().size();
                    if (! map.Source().empty() && (map.Source().back() == L'\n' || map.Source().back() == L'\r'))
                        prepared.sourceLines.emplace_back(map.Source().size(), map.Source().size());
                    work.phase = Preparation::Phase::InlineBounds;
                    continue;
                }
                const auto& box   = *prepared.boxes[work.lineBox];
                const auto length = box.paragraph.display.last - box.paragraph.display.first;
                auto end          = box.layout.LineBoundary(work.lineOffset, true);
                if (end < length && map.Display()[box.paragraph.display.first + end] == L'\n')
                    ++end;
                const auto snapped   = map.SnapDisplayBoundary(box.paragraph.display.first + end, MappingAffinity::Trailing);
                const auto sourceEnd = map.ToSource(snapped, MappingAffinity::Trailing);
                // Atomic objects can wrap over several SDK lines. One source
                // line unit owns the whole object; later overlapping fragments
                // must not create reversed or duplicate raw ACP intervals.
                if (sourceEnd > work.sourceLineStart)
                {
                    prepared.sourceLines.emplace_back(work.sourceLineStart, sourceEnd);
                    work.sourceLineStart = sourceEnd;
                }
                if (end >= length || end <= work.lineOffset)
                {
                    ++work.lineBox;
                    work.lineOffset = 0u;
                }
                else
                    work.lineOffset = end;
            }
            else if (work.phase == Preparation::Phase::InlineBounds)
            {
                const auto elements = work.state->document->Elements();
                if (work.boundsIndex == elements.size())
                {
                    work.phase = Preparation::Phase::ParentBounds;
                    continue;
                }
                const auto index    = work.boundsIndex++;
                const auto& element = elements[index];
                if (element.role == SemanticRole::Link || element.role == SemanticRole::Image || element.role == SemanticRole::Task)
                    for (const auto& rectangle : RangeFromState(*work.state, element.display.first, element.display.last))
                        Union(work.state->bounds[index], rectangle);
            }
            else if (work.phase == Preparation::Phase::ParentBounds)
            {
                if (! work.boundsIndex)
                {
                    work.phase = Preparation::Phase::Publish;
                    continue;
                }
                const auto index  = --work.boundsIndex;
                const auto parent = work.state->document->Elements()[index].parent;
                if (parent != MarkdownNoNode && work.state->bounds[index])
                    Union(work.state->bounds[parent], *work.state->bounds[index]);
            }
            else
            {
                state_ = std::move(work.state);
                next_.reset();
                valid_ = true;
                ++generation_;
                result.status = LayoutPreparationStatus::Complete;
                ++result.blockSteps;
                return result;
            }
            ++result.blockSteps;
        }
    }
    catch (const std::exception&)
    {
        next_.reset();
        throw;
    }
    return result;
}
void PreparedSemanticGeometry::Cancel() noexcept
{
    next_.reset();
}
void PreparedSemanticGeometry::Invalidate() noexcept
{
    Cancel();
    valid_ = false;
}
void PreparedSemanticGeometry::Clear() noexcept
{
    Cancel();
    state_.reset();
    valid_ = false;
}
bool PreparedSemanticGeometry::Ready() const noexcept
{
    return valid_ && static_cast<bool>(state_);
}
bool PreparedSemanticGeometry::Pending() const noexcept
{
    return static_cast<bool>(next_);
}
std::uint64_t PreparedSemanticGeometry::Generation() const noexcept
{
    return generation_;
}
std::uint64_t PreparedSemanticGeometry::Revision() const noexcept
{
    return state_ ? state_->document->Syntax()->Revision() : 0u;
}
float PreparedSemanticGeometry::Height() const noexcept
{
    return state_ ? state_->height : 0.0f;
}
const PreparedSemanticGeometry::State& PreparedSemanticGeometry::Current() const
{
    if (! Ready())
        throw std::logic_error("Semantic geometry is not current");
    return *state_;
}
const SemanticDocument& PreparedSemanticGeometry::Document() const
{
    return *Current().document;
}
CaretGeometry PreparedSemanticGeometry::Caret(std::size_t offset) const
{
    const auto& state = Current();
    const auto& box   = state.BoxAt(offset);
    auto result       = box.layout.Caret(std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first);
    result.x += box.x;
    result.y += box.y;
    return result;
}
Hit PreparedSemanticGeometry::HitTest(float x, float y) const
{
    const auto& state = Current();
    if (! std::isfinite(x) || ! std::isfinite(y))
        throw std::invalid_argument("Invalid semantic hit coordinates");
    const SemanticBox* chosen = nullptr;
    double best               = std::numeric_limits<double>::max();
    for (const auto& owned : state.boxes)
    {
        const auto& box       = *owned;
        const auto& frame     = box.frame;
        const float dy        = y < frame.top ? frame.top - y : y > frame.bottom ? y - frame.bottom : 0.0f;
        const float dx        = x < frame.left ? frame.left - x : x > frame.right ? x - frame.right : 0.0f;
        const double distance = static_cast<double>(dy) * 100000.0 + static_cast<double>(dx);
        if (! chosen || distance < best)
        {
            chosen = &box;
            best   = distance;
        }
    }
    auto hit = chosen->layout.HitTest(x - chosen->x, y - chosen->y);
    hit.position += chosen->paragraph.display.first;
    hit.position = state.document->Map().SnapDisplayBoundary(hit.position, hit.trailing ? MappingAffinity::Trailing : MappingAffinity::Leading);
    return hit;
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::RangeFromState(const State& state, std::size_t first, std::size_t last)
{
    if (first > last || last > state.document->Map().Display().size())
        throw std::out_of_range("Invalid semantic display range");
    std::vector<D2D1_RECT_F> result(0u);
    const auto initial =
        std::lower_bound(state.boxes.begin(), state.boxes.end(), first, [](const auto& box, auto offset) { return box->paragraph.display.last <= offset; });
    if (first != last)
        for (auto entry = initial; entry != state.boxes.end() && (*entry)->paragraph.display.first < last; ++entry)
        {
            const auto& box  = **entry;
            const auto begin = std::max(first, box.paragraph.display.first), end = std::min(last, box.paragraph.display.last);
            if (begin >= end)
                continue;
            for (auto rectangle : box.layout.Range(begin - box.paragraph.display.first, end - box.paragraph.display.first))
            {
                rectangle.left += box.x;
                rectangle.right += box.x;
                rectangle.top += box.y;
                rectangle.bottom += box.y;
                if (box.paragraph.table != MarkdownNoNode)
                {
                    rectangle.left   = std::max(rectangle.left, box.frame.left);
                    rectangle.right  = std::min(rectangle.right, box.frame.right);
                    rectangle.top    = std::max(rectangle.top, box.frame.top);
                    rectangle.bottom = std::min(rectangle.bottom, box.frame.bottom);
                }
                if (rectangle.right > rectangle.left && rectangle.bottom > rectangle.top)
                    result.push_back(rectangle);
            }
        }
    return std::vector<D2D1_RECT_F>(result);
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::Range(std::size_t first, std::size_t last) const
{
    return RangeFromState(Current(), first, last);
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::SourceRange(MarkdownRange source) const
{
    const auto& state = Current();
    std::vector<D2D1_RECT_F> result(0u);
    for (const auto fragment : state.document->Map().DisplayRanges(source))
        for (const auto& rectangle : RangeFromState(state, fragment.first, fragment.last))
            result.push_back(rectangle);
    return std::vector<D2D1_RECT_F>(result);
}
std::span<const std::pair<std::size_t, std::size_t>> PreparedSemanticGeometry::SourceLines() const
{
    return Current().sourceLines;
}
std::optional<D2D1_RECT_F> PreparedSemanticGeometry::ElementBounds(std::size_t element) const
{
    const auto& state = Current();
    if (element >= state.bounds.size())
        throw std::out_of_range("Invalid semantic element");
    return state.bounds[element];
}
std::size_t PreparedSemanticGeometry::PreviousCluster(std::size_t offset) const
{
    const auto& state   = Current();
    const auto& box     = state.BoxAt(offset);
    const auto local    = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto previous = local ? box.paragraph.display.first + box.layout.PreviousCluster(local) : offset ? offset - 1u : 0u;
    return state.document->Map().SnapDisplayBoundary(previous, MappingAffinity::Leading);
}
std::size_t PreparedSemanticGeometry::NextCluster(std::size_t offset) const
{
    const auto& state = Current();
    const auto& box   = state.BoxAt(offset);
    const auto local  = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto next   = local < box.paragraph.display.last - box.paragraph.display.first ? box.paragraph.display.first + box.layout.NextCluster(local)
                                                                                         : std::min(state.document->Map().Display().size(), offset + 1u);
    return state.document->Map().SnapDisplayBoundary(next, MappingAffinity::Trailing);
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::DeletionRange(std::size_t offset, bool backwards) const
{
    const auto& state   = Current();
    const auto leading  = state.document->Map().SnapDisplayBoundary(offset, MappingAffinity::Leading);
    const auto trailing = state.document->Map().SnapDisplayBoundary(offset, MappingAffinity::Trailing);
    if (leading != trailing)
        return {leading, trailing};
    return backwards ? std::pair{PreviousCluster(offset), offset} : std::pair{offset, NextCluster(offset)};
}
std::size_t PreparedSemanticGeometry::LineBoundary(std::size_t offset, bool end) const
{
    const auto& box  = Current().BoxAt(offset);
    const auto local = std::clamp(offset, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    return box.paragraph.display.first + box.layout.LineBoundary(local, end);
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::VisibleRange(float top, float bottom) const
{
    const auto& state = Current();
    if (! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom)
        throw std::invalid_argument("Invalid semantic viewport");
    std::size_t first = state.document->Map().Display().size(), last = 0u;
    for (const auto& box : state.boxes)
        if (box->frame.bottom > top && box->frame.top < bottom)
        {
            const auto range = box->layout.VisibleRange(top - box->y, bottom - box->y);
            first            = std::min(first, box->paragraph.display.first + range.first);
            last             = std::max(last, box->paragraph.display.first + range.second);
        }
    return first <= last ? std::pair{first, last} : std::pair{std::size_t{0u}, std::size_t{0u}};
}
void PreparedSemanticGeometry::Draw(ID2D1RenderTarget* target, ID2D1Brush* text, ID2D1Brush* decoration, D2D1_POINT_2F origin, float top, float bottom) const
{
    const auto& state = Current();
    if (! target || ! text || ! decoration || ! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom)
        throw std::invalid_argument("Invalid semantic raster request");
    const auto elements = state.document->Elements();
    for (std::size_t i = 0u; i < elements.size(); ++i)
    {
        const auto& bounds = state.bounds[i];
        if (! bounds || bounds->bottom <= top || bounds->top >= bottom)
            continue;
        const auto rectangle = D2D1::RectF(bounds->left + origin.x, bounds->top + origin.y, bounds->right + origin.x, bounds->bottom + origin.y);
        if (elements[i].role == SemanticRole::Cell || elements[i].role == SemanticRole::Code || elements[i].role == SemanticRole::Image)
            target->DrawRectangle(rectangle, decoration);
        else if (elements[i].role == SemanticRole::Quote)
        {
            const float x = state.options.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? rectangle.right + 8.0f : rectangle.left - 8.0f;
            target->DrawLine(D2D1::Point2F(x, rectangle.top), D2D1::Point2F(x, rectangle.bottom), decoration, 2.0f);
        }
    }
    for (const auto& box : state.boxes)
        if (box->frame.bottom > top && box->frame.top < bottom)
        {
            if (elements[box->paragraph.element].role == SemanticRole::Rule)
                target->DrawLine(D2D1::Point2F(origin.x + box->x, origin.y + box->y + box->height * .5f),
                                 D2D1::Point2F(origin.x + box->x + box->width, origin.y + box->y + box->height * .5f),
                                 decoration);
            else
            {
                if (box->paragraph.table != MarkdownNoNode)
                    target->PushAxisAlignedClip(
                        D2D1::RectF(origin.x + box->frame.left, origin.y + box->frame.top, origin.x + box->frame.right, origin.y + box->frame.bottom),
                        D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                const auto restoreClip = wil::scope_exit([&]() noexcept
                {
                    if (box->paragraph.table != MarkdownNoNode)
                        target->PopAxisAlignedClip();
                });
                box->layout.Draw(target, text, D2D1::Point2F(origin.x + box->x, origin.y + box->y));
            }
        }
}
} // namespace DxUi::RichText::Detail
