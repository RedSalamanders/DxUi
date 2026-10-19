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
    std::optional<MarkdownEmptyLine> sourceCaret;
    bool caretOnly{};
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
          glyphBoxes(0u),
          nodeElements(document->Syntax()->Nodes().size(), MarkdownNoNode),
          bounds(document->Elements().size()),
          sourceLines(0u) // Fallible size constructor: MSVC Debug's noexcept default allocates an iterator proxy.
    {
    }
    std::shared_ptr<const SemanticDocument> document;
    wil::com_ptr<IDWriteFactory> factory;
    LayoutOptions options;
    std::vector<std::unique_ptr<SemanticBox>> boxes;
    // Source caret boxes are in physical source order, while their zero-width
    // display anchors may coincide with or follow generated marker text. Keep
    // the glyph search index in display order instead of assuming both orders
    // are interchangeable.
    std::vector<std::size_t> glyphBoxes;
    std::vector<std::size_t> nodeElements;
    std::vector<std::optional<D2D1_RECT_F>> bounds;
    std::vector<std::pair<std::size_t, std::size_t>> sourceLines;
    float height{};
    const SemanticBox& BoxAt(std::size_t position) const
    {
        if (position > document->Map().Display().size() || boxes.empty())
            throw std::out_of_range("Invalid semantic display offset");
        if (glyphBoxes.empty())
            return *boxes.front();
        auto found = std::lower_bound(
            glyphBoxes.begin(), glyphBoxes.end(), position, [&](auto index, auto offset) { return boxes[index]->paragraph.display.last <= offset; });
        if (found == glyphBoxes.end())
            found = std::prev(glyphBoxes.end());
        else if (boxes[*found]->paragraph.display.first > position && found != glyphBoxes.begin())
            --found;
        return *boxes[*found];
    }
    bool Belongs(const SemanticBox& box, std::size_t source) const noexcept
    {
        if (box.sourceCaret)
        {
            const auto& line = *box.sourceCaret;
            return source == line.insertion || (source >= line.source.first && source < line.source.last);
        }
        const auto range = document->Syntax()->Nodes()[box.paragraph.syntaxNode].block.source;
        return source >= range.first && source <= range.last;
    }
    std::size_t SourceBox(std::size_t source, std::size_t site) const
    {
        if (! IsEditBoundary(document->Map().Source(), source))
            throw std::out_of_range("Invalid semantic source caret offset");
        if (site < boxes.size() && Belongs(*boxes[site], source))
            return site;
        for (std::size_t i = 0u; i < boxes.size(); ++i)
            if (boxes[i]->sourceCaret && Belongs(*boxes[i], source))
                return i;
        const auto* chosen = &BoxAt(document->Map().ToDisplay(source));
        const auto found   = std::find_if(boxes.begin(), boxes.end(), [&](const auto& box) { return box.get() == chosen; });
        return static_cast<std::size_t>(found - boxes.begin());
    }
    std::size_t NearestBox(float x, float y) const
    {
        if (! std::isfinite(x) || ! std::isfinite(y))
            throw std::invalid_argument("Invalid semantic hit coordinates");
        std::size_t chosen = 0u;
        double best        = std::numeric_limits<double>::max();
        for (std::size_t i = 0u; i < boxes.size(); ++i)
        {
            const auto& frame     = boxes[i]->frame;
            const float dy        = y < frame.top ? frame.top - y : y > frame.bottom ? y - frame.bottom : 0.0f;
            const float dx        = x < frame.left ? frame.left - x : x > frame.right ? x - frame.right : 0.0f;
            const double distance = static_cast<double>(dy) * 100000.0 + static_cast<double>(dx);
            if (distance < best)
            {
                chosen = i;
                best   = distance;
            }
        }
        return chosen;
    }
};
struct PreparedSemanticGeometry::Preparation
{
    enum class Phase
    {
        IndexElements,
        CountSites,
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
    Phase phase = Phase::IndexElements;
    std::size_t paragraph{}, rowStart{}, rowPlacement{}, rowTable = MarkdownNoNode, rowIndex{}, boundsIndex{};
    float y{}, rowHeight{};
    bool rowActive{};
    std::size_t emptyLine{}, plannedBoxes{};
    std::size_t lineBox{}, lineOffset{}, sourceLineStart{};
    std::size_t SourceFirst(const SemanticParagraph& content) const
    {
        const auto& nodes = state->document->Syntax()->Nodes();
        const auto& node  = nodes[content.syntaxNode];
        // Padded cells may be anchored after their row's EOL. Order every cell
        // by the physical row start, so an EOF/blank caret can never split a
        // table row between its real and generated cells.
        return content.table == MarkdownNoNode ? node.block.source.first : nodes[node.parent].block.source.first;
    }
    void FinishRow()
    {
        rowPlacement = rowStart;
        phase        = Phase::RowPlacement;
    }
    void LayoutOne()
    {
        auto& next            = *state;
        const auto paragraphs = next.document->Paragraphs();
        const auto emptyLines = next.document->Syntax()->EmptyLines();
        const bool atEnd      = paragraph == paragraphs.size() && emptyLine == emptyLines.size();
        if (atEnd)
        {
            if (rowActive)
            {
                FinishRow();
                return;
            }
            if (! next.boxes.empty())
            {
                next.height = std::max(next.options.fontSizeDip, y);
                phase       = Phase::SourceLines;
                return;
            }
        }
        SemanticParagraph content;
        std::optional<MarkdownEmptyLine> caretLine;
        bool caretOnly = false, consumeParagraph = false, consumeEmptyLine = false;
        if (atEnd)
        {
            // A definitions-only document still has a real source insertion
            // site even though it contributes neither a paragraph nor glyphs.
            caretLine = MarkdownEmptyLine{{0u, next.document->Map().Source().size()}, next.document->Map().Source().size(), 0u, 0u};
            caretOnly = true;
        }
        if (paragraph < paragraphs.size())
        {
            content          = paragraphs[paragraph];
            consumeParagraph = true;
        }
        if (emptyLine < emptyLines.size() && (paragraph == paragraphs.size() || emptyLines[emptyLine].source.first <= SourceFirst(content)))
        {
            caretLine        = emptyLines[emptyLine];
            consumeEmptyLine = true;
            // An empty list item's generated marker already owns this visual
            // line. Attach its insertion site instead of making a second row.
            consumeParagraph = paragraph < paragraphs.size() && content.syntaxNode == caretLine->owner;
            if (! consumeParagraph)
            {
                content            = {};
                content.syntaxNode = caretLine->owner;
                content.element    = next.nodeElements[caretLine->owner];
                content.indent     = caretLine->indent;
                const auto display = next.document->Map().ToDisplay(caretLine->insertion);
                content.display    = {display, display};
                caretOnly          = true;
            }
        }
        const bool table = content.table != MarkdownNoNode;
        if (rowActive && (! table || content.table != rowTable || content.row != rowIndex))
        {
            FinishRow();
            return;
        }
        if (next.boxes.size() == SemanticDocument::MaximumParagraphs)
            throw std::length_error("Semantic native paragraph/caret-site capacity exceeded");
        auto box         = std::make_unique<SemanticBox>();
        box->paragraph   = content;
        box->caretOnly   = caretOnly;
        box->sourceCaret = caretLine;
        const auto& node = next.document->Syntax()->Nodes()[content.syntaxNode];
        if (! box->sourceCaret && node.block.content.first == node.block.content.last)
        {
            box->sourceCaret = MarkdownEmptyLine{table ? node.block.content : node.block.source, node.block.content.first, content.syntaxNode, content.indent};
            if (table)
            {
                const auto row = next.document->Syntax()->Nodes()[node.parent].block.content;
                auto insertion = node.block.content.first;
                // M3's padded token is anchored at the full source row end,
                // which can include CRLF. Its visual insertion site is before
                // that EOL, never on the following physical source line.
                if (insertion == row.last)
                    while (insertion > row.first &&
                           (next.document->Map().Source()[insertion - 1u] == L'\r' || next.document->Map().Source()[insertion - 1u] == L'\n'))
                        --insertion;
                box->sourceCaret->source    = {insertion, insertion};
                box->sourceCaret->insertion = insertion;
            }
        }
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
        if (consumeParagraph)
            ++paragraph;
        if (consumeEmptyLine)
            ++emptyLine;
        Union(next.bounds[content.element], box->frame);
        if (! caretOnly)
            next.glyphBoxes.push_back(next.boxes.size());
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
            if (work.phase == Preparation::Phase::IndexElements)
            {
                const auto elements = work.state->document->Elements();
                if (work.boundsIndex == elements.size())
                {
                    work.boundsIndex = 0u;
                    work.phase       = Preparation::Phase::CountSites;
                    continue;
                }
                const auto index = work.boundsIndex++;
                if (elements[index].syntaxNode != MarkdownNoNode)
                    work.state->nodeElements[elements[index].syntaxNode] = index;
            }
            else if (work.phase == Preparation::Phase::CountSites)
            {
                // Preflight the same source-order merge, one metadata event at
                // a time. Overflow never spends thousands of SDK layout calls
                // before discovering that source fallback was required.
                const auto paragraphs = work.state->document->Paragraphs();
                const auto lines      = work.state->document->Syntax()->EmptyLines();
                if (work.paragraph == paragraphs.size() && work.emptyLine == lines.size())
                {
                    work.paragraph = work.emptyLine = 0u;
                    work.phase                      = Preparation::Phase::Layouts;
                    continue;
                }
                if (work.emptyLine < lines.size() &&
                    (work.paragraph == paragraphs.size() || lines[work.emptyLine].source.first <= work.SourceFirst(paragraphs[work.paragraph])))
                {
                    if (work.paragraph < paragraphs.size() && paragraphs[work.paragraph].syntaxNode == lines[work.emptyLine].owner)
                        ++work.paragraph;
                    ++work.emptyLine;
                }
                else
                    ++work.paragraph;
                if (++work.plannedBoxes > SemanticDocument::MaximumParagraphs)
                    throw std::length_error("Semantic native paragraph/caret-site admission exceeded");
            }
            else if (work.phase == Preparation::Phase::Layouts)
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
                    if (! map.Source().empty() && (map.Source().back() == L'\n' || map.Source().back() == L'\r') &&
                        prepared.sourceLines.back() != std::pair{map.Source().size(), map.Source().size()})
                        prepared.sourceLines.emplace_back(map.Source().size(), map.Source().size());
                    work.phase = Preparation::Phase::InlineBounds;
                    continue;
                }
                const auto& box = *prepared.boxes[work.lineBox];
                if (box.sourceCaret && box.paragraph.table == MarkdownNoNode)
                {
                    const auto& line = *box.sourceCaret;
                    // Keep hidden closing syntax and EOL with the preceding
                    // mapped line, rather than creating an EOL-only line unit.
                    if (! prepared.sourceLines.empty() && line.source.first > work.sourceLineStart)
                    {
                        prepared.sourceLines.back().second = line.source.first;
                        work.sourceLineStart               = line.source.first;
                    }
                    if (line.source.first == map.Source().size() && line.source.last == map.Source().size())
                    {
                        if (prepared.sourceLines.empty())
                            prepared.sourceLines.emplace_back(0u, map.Source().size());
                        else
                            prepared.sourceLines.back().second = map.Source().size();
                        // Finalization adds the single empty EOF unit below.
                    }
                    else if (line.source.last > work.sourceLineStart)
                        prepared.sourceLines.emplace_back(work.sourceLineStart, line.source.last);
                    work.sourceLineStart = std::max(work.sourceLineStart, line.source.last);
                    ++work.lineBox;
                    work.lineOffset = 0u;
                    continue;
                }
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
std::size_t PreparedSemanticGeometry::BoxCount() const noexcept
{
    return state_ ? state_->boxes.size() : 0u;
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
    const auto& state  = Current();
    const auto& chosen = *state.boxes[state.NearestBox(x, y)];
    auto hit           = chosen.layout.HitTest(x - chosen.x, y - chosen.y);
    hit.position += chosen.paragraph.display.first;
    hit.position = state.document->Map().SnapDisplayBoundary(hit.position, hit.trailing ? MappingAffinity::Trailing : MappingAffinity::Leading);
    return hit;
}
CaretGeometry PreparedSemanticGeometry::SourceCaret(std::size_t source, std::size_t site) const
{
    const auto& state  = Current();
    const auto& box    = *state.boxes[state.SourceBox(source, site)];
    const auto display = box.sourceCaret ? (box.caretOnly ? box.paragraph.display.first
                                            : state.document->Syntax()->Nodes()[box.paragraph.syntaxNode].block.kind == MarkdownBlockKind::ListItem
                                                ? box.paragraph.display.last
                                                : box.paragraph.display.first)
                                         : state.document->Map().ToDisplay(source);
    auto result        = box.layout.Caret(std::clamp(display, box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first);
    result.x += box.x;
    result.y += box.y;
    return result;
}
SourceCaretHit PreparedSemanticGeometry::HitTestSource(float x, float y) const
{
    const auto& state = Current();
    const auto index  = state.NearestBox(x, y);
    const auto& box   = *state.boxes[index];
    if (box.sourceCaret)
        return {box.sourceCaret->insertion, index};
    auto hit            = box.layout.HitTest(x - box.x, y - box.y);
    const auto affinity = hit.trailing ? MappingAffinity::Trailing : MappingAffinity::Leading;
    const auto display  = state.document->Map().SnapDisplayBoundary(box.paragraph.display.first + hit.position, affinity);
    return {state.document->Map().ToSource(display, affinity), index};
}
SourceCaretHit PreparedSemanticGeometry::StepSource(std::size_t source, std::size_t site, bool forwards) const
{
    const auto& state = Current();
    const auto index  = state.SourceBox(source, site);
    const auto& box   = *state.boxes[index];
    const auto& map   = state.document->Map();
    auto display      = std::clamp(map.ToDisplay(source), box.paragraph.display.first, box.paragraph.display.last);
    if (! box.sourceCaret)
        while (forwards ? display < box.paragraph.display.last : display > box.paragraph.display.first)
        {
            const auto local = display - box.paragraph.display.first;
            const auto next  = box.paragraph.display.first + (forwards ? box.layout.NextCluster(local) : box.layout.PreviousCluster(local));
            if (next == display)
                break;
            display           = map.SnapDisplayBoundary(next, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            const auto result = map.ToSource(display, forwards ? MappingAffinity::Trailing : MappingAffinity::Leading);
            // Generated marker anchors can lie ahead of a raw caret inside
            // opening syntax. Left must not follow them forward, nor Right
            // backward. Those anchors are not owned logical source stops.
            if (forwards ? result > source : result < source)
                return {result, index};
        }
    if ((forwards && index + 1u == state.boxes.size()) || (! forwards && ! index))
        return {source, index};
    const auto neighbor = forwards ? index + 1u : index - 1u;
    const auto& target  = *state.boxes[neighbor];
    if (target.sourceCaret)
        return {target.sourceCaret->insertion, neighbor};
    return {map.ToSource(forwards ? target.paragraph.display.first : target.paragraph.display.last,
                         forwards ? MappingAffinity::Leading : MappingAffinity::Trailing),
            neighbor};
}
SourceCaretHit PreparedSemanticGeometry::SourceLineBoundary(std::size_t source, std::size_t site, bool end) const
{
    const auto& state = Current();
    const auto index  = state.SourceBox(source, site);
    const auto& box   = *state.boxes[index];
    if (box.sourceCaret)
        return {box.sourceCaret->insertion, index};
    const auto& map  = state.document->Map();
    const auto local = std::clamp(map.ToDisplay(source), box.paragraph.display.first, box.paragraph.display.last) - box.paragraph.display.first;
    const auto boundary =
        map.SnapDisplayBoundary(box.paragraph.display.first + box.layout.LineBoundary(local, end), end ? MappingAffinity::Trailing : MappingAffinity::Leading);
    return {map.ToSource(boundary, end ? MappingAffinity::Trailing : MappingAffinity::Leading), index};
}
bool PreparedSemanticGeometry::IsEmptySourceSite(std::size_t source, std::size_t site) const
{
    const auto& state = Current();
    return state.boxes[state.SourceBox(source, site)]->sourceCaret.has_value();
}
bool PreparedSemanticGeometry::IsCellSourceSite(std::size_t source, std::size_t site) const
{
    const auto& state = Current();
    const auto& box   = *state.boxes[state.SourceBox(source, site)];
    return box.sourceCaret && box.paragraph.table != MarkdownNoNode;
}
std::pair<std::size_t, std::size_t> PreparedSemanticGeometry::VisibleSourceRange(float top, float bottom) const
{
    const auto& state = Current();
    if (! std::isfinite(top) || ! std::isfinite(bottom) || top > bottom)
        throw std::invalid_argument("Invalid semantic source viewport");
    const auto& map   = state.document->Map();
    std::size_t first = map.Source().size(), last = 0u;
    for (const auto& box : state.boxes)
        if (box->frame.bottom > top && box->frame.top < bottom)
        {
            if (box->sourceCaret)
            {
                first = std::min(first, box->sourceCaret->source.first);
                last  = std::max(last, box->sourceCaret->source.last);
            }
            else
            {
                const auto range = box->layout.VisibleRange(top - box->y, bottom - box->y);
                first            = std::min(first, map.ToSource(box->paragraph.display.first + range.first));
                last             = std::max(last, map.ToSource(box->paragraph.display.first + range.second, MappingAffinity::Trailing));
            }
        }
    return first <= last ? std::pair{first, last} : std::pair{std::size_t{0u}, std::size_t{0u}};
}
std::vector<D2D1_RECT_F> PreparedSemanticGeometry::RangeFromState(const State& state, std::size_t first, std::size_t last)
{
    if (first > last || last > state.document->Map().Display().size())
        throw std::out_of_range("Invalid semantic display range");
    std::vector<D2D1_RECT_F> result(0u);
    const auto initial = std::lower_bound(
        state.glyphBoxes.begin(), state.glyphBoxes.end(), first, [&](auto index, auto offset) { return state.boxes[index]->paragraph.display.last <= offset; });
    if (first != last)
        for (auto entry = initial; entry != state.glyphBoxes.end() && state.boxes[*entry]->paragraph.display.first < last; ++entry)
        {
            const auto& box  = *state.boxes[*entry];
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
