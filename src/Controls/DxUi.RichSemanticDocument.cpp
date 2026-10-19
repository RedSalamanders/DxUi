#include "DxUi.RichSemanticHtml.h"
#include <algorithm>
#include <atomic>
#include <optional>

namespace DxUi::RichText
{
SemanticDocument::SemanticDocument(std::shared_ptr<const MarkdownSyntax> syntax)
    : syntax_(std::move(syntax)),
      elements_(0u),
      paragraphs_(0u),
      runs_(0u),
      sourceRuns_(0u)
{
    static std::atomic<std::uint64_t> nextIdentity{1u};
    identity_ = nextIdentity.fetch_add(1u, std::memory_order_relaxed);
}

struct SemanticBuilder::Preparation
{
    struct Visit
    {
        std::size_t node{}, parent = MarkdownNoNode, element = MarkdownNoNode, paragraph = MarkdownNoNode;
        std::size_t table = MarkdownNoNode, row{}, column{}, columns{};
        unsigned flags{}, heading{}, indent{};
        bool inlineNode{}, close{};
    };
    explicit Preparation(std::shared_ptr<const MarkdownSyntax> syntax)
        : document(new SemanticDocument(std::move(syntax))),
          output(document->runs_),
          visits(0u),
          tails(0u),
          ordinals(document->syntax_->Nodes().size(), 0u)
    {
        visits.push_back({});
    }
    std::shared_ptr<SemanticDocument> document;
    Detail::SemanticOutput output;
    Detail::SemanticValue normalized;
    Detail::NativeHtmlPlan html;
    std::size_t htmlDisplayFirst{};
    std::vector<Visit> visits;
    std::vector<std::size_t> tails;
    std::vector<unsigned> ordinals;

    std::size_t Element(SemanticRole role, MarkdownRange source, std::size_t parent, std::uint64_t identity = 0u)
    {
        auto& elements = document->elements_;
        if (elements.size() == SemanticDocument::MaximumElements)
            throw std::length_error("Semantic element capacity exceeded");
        const auto index = elements.size();
        elements.emplace_back();
        tails.push_back(MarkdownNoNode);
        auto& element         = elements.back();
        element.role          = role;
        element.source        = source;
        element.display       = {output.text.size(), output.text.size()};
        element.parent        = parent;
        element.blockIdentity = identity;
        if (parent != MarkdownNoNode)
        {
            if (tails[parent] == MarkdownNoNode)
                elements[parent].firstChild = index;
            else
                elements[tails[parent]].nextSibling = index;
            tails[parent] = index;
        }
        return index;
    }
    static SemanticRole BlockRole(MarkdownBlockKind kind)
    {
        switch (kind)
        {
            case MarkdownBlockKind::Document: return SemanticRole::Document;
            case MarkdownBlockKind::BlockQuote: return SemanticRole::Quote;
            case MarkdownBlockKind::List: return SemanticRole::List;
            case MarkdownBlockKind::ListItem: return SemanticRole::ListItem;
            case MarkdownBlockKind::AtxHeading:
            case MarkdownBlockKind::SetextHeading: return SemanticRole::Heading;
            case MarkdownBlockKind::FencedCode:
            case MarkdownBlockKind::IndentedCode: return SemanticRole::Code;
            case MarkdownBlockKind::HtmlBlock: return SemanticRole::Html;
            case MarkdownBlockKind::Table: return SemanticRole::Table;
            case MarkdownBlockKind::TableRow: return SemanticRole::Row;
            case MarkdownBlockKind::TableCell: return SemanticRole::Cell;
            case MarkdownBlockKind::ThematicBreak: return SemanticRole::Rule;
            default: return SemanticRole::Paragraph;
        }
    }
    static bool Leaf(MarkdownBlockKind kind) noexcept
    {
        switch (kind)
        {
            case MarkdownBlockKind::Paragraph:
            case MarkdownBlockKind::AtxHeading:
            case MarkdownBlockKind::SetextHeading:
            case MarkdownBlockKind::FencedCode:
            case MarkdownBlockKind::IndentedCode:
            case MarkdownBlockKind::HtmlBlock:
            case MarkdownBlockKind::TableCell:
            case MarkdownBlockKind::ThematicBreak: return true;
            default: return false;
        }
    }
    void CloseElement(std::size_t index)
    {
        auto& element        = document->elements_[index];
        element.display.last = output.text.size();
        switch (element.role)
        {
            case SemanticRole::Paragraph:
            case SemanticRole::Heading:
            case SemanticRole::Code:
            case SemanticRole::Html:
            case SemanticRole::Cell:
            case SemanticRole::Link: element.name.assign(output.text, element.display.first, element.display.last - element.display.first); break;
            default: break;
        }
    }
    std::wstring ImageAlternative(const MarkdownInline& image) const
    {
        std::wstring result(0u, L' ');
        std::vector<std::size_t> pending(0u);
        if (image.firstChild != MarkdownNoNode)
            pending.push_back(image.firstChild);
        const auto& inlines = document->syntax_->Inlines();
        while (! pending.empty())
        {
            const auto index = pending.back();
            pending.pop_back();
            const auto& node = inlines[index];
            if (node.nextSibling != MarkdownNoNode)
                pending.push_back(node.nextSibling);
            if (node.firstChild != MarkdownNoNode)
                pending.push_back(node.firstChild);
            else if (node.kind == MarkdownInlineKind::SoftBreak || node.kind == MarkdownInlineKind::HardBreak)
                result.push_back(L' ');
            else
                result.append(node.value);
            if (result.size() > SourceDisplayMap::MaximumDisplayUnits)
                throw std::length_error("Semantic image alternative capacity exceeded");
        }
        return std::wstring(result);
    }
    void HtmlTag(const Detail::NativeHtmlTag& tag, std::size_t parent, unsigned flags, unsigned heading)
    {
        if (tag.kind == Detail::NativeHtmlKind::A)
        {
            if (tag.closing)
            {
                if (html.linkElement == MarkdownNoNode)
                    throw std::logic_error("Native HTML anchor close has no owner");
                CloseElement(html.linkElement);
                html.linkElement = MarkdownNoNode;
            }
            else
            {
                const auto element                       = Element(SemanticRole::Link, {tag.source.first, html.tags[tag.mate].source.last}, parent);
                document->elements_[element].destination = tag.destination;
                document->elements_[element].title       = tag.title;
                html.linkElement                         = element;
            }
        }
        html.flags   = tag.flagsAfter;
        html.heading = tag.headingAfter;
        if (tag.kind == Detail::NativeHtmlKind::Br)
            output.Emit(L"\n", tag.source, DisplaySpanKind::Transformed, flags | html.flags, html.heading ? html.heading : heading);
    }
    void HtmlBlock(const Visit& visit)
    {
        std::size_t at = 0u;
        for (const auto& tag : html.tags)
        {
            const auto first = std::max(at, html.bodyFirst), last = std::min(tag.first, html.bodyLast);
            if (first < last)
                Detail::NativeHtmlText(normalized, first, last, output, visit.flags | html.flags, html.heading ? html.heading : visit.heading);
            HtmlTag(tag, visit.element, visit.flags, visit.heading);
            at = tag.last;
        }
        const auto first = std::max(at, html.bodyFirst);
        if (first < html.bodyLast)
            Detail::NativeHtmlText(normalized, first, html.bodyLast, output, visit.flags | html.flags, html.heading ? html.heading : visit.heading);
    }
    void Inline(Visit visit)
    {
        const auto& node = document->syntax_->Inlines()[visit.node];
        if (visit.close)
        {
            CloseElement(visit.element);
            return;
        }
        if (node.nextSibling != MarkdownNoNode)
        {
            auto sibling = visit;
            sibling.node = node.nextSibling;
            visits.push_back(sibling);
        }
        auto markdownFlags = visit.flags;
        switch (node.kind)
        {
            case MarkdownInlineKind::Strong: markdownFlags |= Bold; break;
            case MarkdownInlineKind::Emphasis: markdownFlags |= Italic; break;
            case MarkdownInlineKind::Strikethrough: markdownFlags |= Strike; break;
            case MarkdownInlineKind::Link: markdownFlags |= Link; break;
            default: break;
        }
        if (node.kind == MarkdownInlineKind::Html && html.valid)
        {
            HtmlTag(html.At(node.source), visit.parent, markdownFlags, visit.heading);
            return;
        }
        const auto flags = markdownFlags | html.flags;
        auto parent      = visit.parent;
        if (node.kind == MarkdownInlineKind::Link || node.kind == MarkdownInlineKind::Image)
        {
            // A Markdown style container can cross an HTML anchor boundary.
            // Resolve its current native owner only when creating an element;
            // never capture a temporary HTML anchor in the style visit stack.
            const auto owner   = html.linkElement == MarkdownNoNode ? parent : html.linkElement;
            const auto element = Element(node.kind == MarkdownInlineKind::Link ? SemanticRole::Link : SemanticRole::Image, node.source, owner);
            document->elements_[element].destination = node.destination;
            document->elements_[element].title       = node.title;
            if (node.kind == MarkdownInlineKind::Image)
            {
                const auto alternative = ImageAlternative(node);
                std::wstring value(0u, L' ');
                value.append(L"[image");
                if (! alternative.empty())
                    value.append(L": ").append(alternative);
                value.push_back(L']');
                output.Emit(value, node.source, DisplaySpanKind::Object, flags, visit.heading);
                auto& image        = document->elements_[element];
                image.display.last = output.text.size();
                image.name         = alternative;
                return;
            }
            auto close    = visit;
            close.close   = true;
            close.element = element;
            visits.push_back(close);
            parent = element;
        }
        if (node.firstChild != MarkdownNoNode)
        {
            auto child   = visit;
            child.node   = node.firstChild;
            child.parent = parent;
            // Native HTML style is traversal state, not inherited Markdown
            // state: a closing tag inside an emphasis node must end its style.
            child.flags = markdownFlags;
            visits.push_back(child);
            return;
        }
        if (node.kind == MarkdownInlineKind::SoftBreak || node.kind == MarkdownInlineKind::HardBreak)
            output.Emit(node.kind == MarkdownInlineKind::SoftBreak ? L" " : L"\n", node.source, DisplaySpanKind::Transformed, flags, visit.heading);
        else if (! node.value.empty())
        {
            Detail::SemanticValue value;
            Detail::ResolveSemanticValue(*document->syntax_, normalized, node, value);
            output.Value(value, flags | ((node.kind == MarkdownInlineKind::Code || node.kind == MarkdownInlineKind::Html) ? Code : 0u), visit.heading);
        }
    }
    std::size_t Paragraph(const Visit& visit, std::size_t element)
    {
        if (document->paragraphs_.size() == SemanticDocument::MaximumParagraphs)
            throw std::length_error("Semantic paragraph capacity exceeded");
        const auto index = document->paragraphs_.size();
        document->paragraphs_.push_back({{output.text.size(), output.text.size()},
                                         element,
                                         visit.node,
                                         visit.table,
                                         visit.row,
                                         visit.column,
                                         visit.columns,
                                         visit.indent,
                                         visit.flags,
                                         visit.heading,
                                         document->syntax_->Nodes()[visit.node].alignment});
        return index;
    }
    static std::size_t FirstVisibleChild(const std::vector<MarkdownNode>& nodes, const MarkdownNode& owner)
    {
        auto child = owner.firstChild;
        while (child != MarkdownNoNode &&
               (nodes[child].block.kind == MarkdownBlockKind::ReferenceDefinition || nodes[child].block.kind == MarkdownBlockKind::BlankLine))
            child = nodes[child].nextSibling;
        return child;
    }
    static bool SeparateMarker(const std::vector<MarkdownNode>& nodes, const MarkdownNode& item)
    {
        const auto first = FirstVisibleChild(nodes, item);
        if (first == MarkdownNoNode)
            return true;
        const auto kind = nodes[first].block.kind;
        return kind == MarkdownBlockKind::List || kind == MarkdownBlockKind::Table || kind == MarkdownBlockKind::FencedCode ||
               kind == MarkdownBlockKind::IndentedCode || kind == MarkdownBlockKind::HtmlBlock || kind == MarkdownBlockKind::ThematicBreak;
    }
    void Marker(std::size_t itemIndex, std::size_t anchor, unsigned flags, unsigned heading)
    {
        const auto& nodes = document->syntax_->Nodes();
        const auto& item  = nodes[itemIndex];
        const auto& list  = nodes[item.parent];
        if (list.listMarker == L'.' || list.listMarker == L')')
        {
            std::array<wchar_t, 16u> digits{};
            auto end    = digits.data() + digits.size();
            end[-1]     = L' ';
            end[-2]     = list.listMarker;
            auto cursor = end - 2u;
            auto number = ordinals[itemIndex];
            do
            {
                *--cursor = static_cast<wchar_t>(L'0' + number % 10u);
                number /= 10u;
            } while (number);
            output.Generated(std::wstring_view(cursor, static_cast<std::size_t>(end - cursor)), anchor, flags, heading);
        }
        else
            output.Generated(L"\u2022 ", anchor, flags, heading);
    }
    void Prefix(const Visit& visit, std::size_t element)
    {
        const auto& nodes = document->syntax_->Nodes();
        auto child        = visit.node;
        for (auto parent = nodes[child].parent; parent != MarkdownNoNode; child = parent, parent = nodes[parent].parent)
        {
            const auto& owner = nodes[parent];
            if (FirstVisibleChild(nodes, owner) != child)
                break;
            if (owner.block.kind != MarkdownBlockKind::ListItem)
                continue;
            if (SeparateMarker(nodes, owner))
                break;
            const auto& leaf  = nodes[visit.node];
            const auto anchor = leaf.firstInline == MarkdownNoNode ? leaf.block.content.last : document->syntax_->Inlines()[leaf.firstInline].source.first;
            Marker(parent, anchor, visit.flags, visit.heading);
            if (owner.task != MarkdownTask::None && normalized.units.size() >= 3u)
            {
                const MarkdownRange marker{normalized.units[0u].source.first, normalized.units[2u].source.last};
                const auto task   = Element(SemanticRole::Task, marker, element, owner.identity);
                auto& target      = document->elements_[task];
                target.task       = owner.task;
                target.taskMarker = marker;
                output.Emit(owner.task == MarkdownTask::Checked ? L"\u2611" : L"\u2610", marker, DisplaySpanKind::Object, visit.flags, visit.heading);
                target.display.last = output.text.size();
                output.Generated(L" ", anchor, visit.flags, visit.heading);
            }
            break;
        }
    }
    void Block(Visit visit)
    {
        const auto& syntax = *document->syntax_;
        const auto& nodes  = syntax.Nodes();
        const auto& node   = nodes[visit.node];
        if (visit.close)
        {
            if (visit.paragraph != MarkdownNoNode)
            {
                auto& paragraph = document->paragraphs_[visit.paragraph];
                if (html.valid && output.text.size() == htmlDisplayFirst)
                {
                    paragraph.emptyInsertion = html.emptyInsertion;
                    output.Generated(L" ", html.emptyInsertion, visit.flags, visit.heading);
                }
                if (output.text.size() == paragraph.display.first)
                    output.Generated(L" ", node.block.content.first, visit.flags, visit.heading);
                paragraph.display.last = output.text.size();
                CloseElement(visit.element);
                if (output.text.empty() || output.text.back() != L'\n')
                    output.Generated(visit.table != MarkdownNoNode && visit.column + 1u < visit.columns ? L"\t" : L"\n", node.block.content.last);
            }
            else
                CloseElement(visit.element);
            return;
        }
        if (node.nextSibling != MarkdownNoNode)
        {
            auto sibling = visit;
            sibling.node = node.nextSibling;
            if (visit.table != MarkdownNoNode && nodes[visit.node].block.kind == MarkdownBlockKind::TableCell)
                ++sibling.column;
            visits.push_back(sibling);
        }
        if (node.block.kind == MarkdownBlockKind::BlankLine || node.block.kind == MarkdownBlockKind::ReferenceDefinition)
            return;
        visit.element                                  = Element(BlockRole(node.block.kind), node.block.source, visit.parent, node.identity);
        document->elements_[visit.element].syntaxNode  = visit.node;
        document->elements_[visit.element].heading     = node.block.heading;
        document->elements_[visit.element].alignment   = node.alignment;
        document->elements_[visit.element].tableHeader = node.tableHeader;
        document->elements_[visit.element].task        = node.task;
        if (node.block.kind == MarkdownBlockKind::ListItem || node.block.kind == MarkdownBlockKind::BlockQuote)
            ++visit.indent;
        visit.heading = node.block.heading;
        if (node.block.kind == MarkdownBlockKind::FencedCode || node.block.kind == MarkdownBlockKind::IndentedCode)
            visit.flags |= Code;
        if (node.tableHeader)
            visit.flags |= Bold;
        if (node.block.kind == MarkdownBlockKind::Table)
        {
            visit.table = visit.element;
            if (node.firstChild != MarkdownNoNode)
                for (auto cell = nodes[node.firstChild].firstChild; cell != MarkdownNoNode; cell = nodes[cell].nextSibling)
                    ++visit.columns;
            document->elements_[visit.element].columns = visit.columns;
        }
        if (node.block.kind == MarkdownBlockKind::TableRow && visit.table != MarkdownNoNode)
            visit.row = document->elements_[visit.table].rows++;
        if (node.block.kind == MarkdownBlockKind::List)
        {
            auto number = node.listStart;
            for (auto item = node.firstChild; item != MarkdownNoNode; item = nodes[item].nextSibling)
                ordinals[item] = number++;
        }
        if (node.block.kind == MarkdownBlockKind::ListItem && SeparateMarker(nodes, node))
        {
            const auto paragraph = Paragraph(visit, visit.element);
            Marker(visit.node, node.block.content.first, visit.flags, visit.heading);
            document->paragraphs_[paragraph].display.last = output.text.size();
            document->paragraphs_[paragraph].markerEnd    = output.text.size();
            document->paragraphs_[paragraph].markerOwner  = visit.node;
            output.Generated(L"\n", node.block.content.first);
        }
        auto close  = visit;
        close.close = true;
        if (Leaf(node.block.kind))
        {
            Detail::NormalizeSemanticLeaf(syntax, visit.node, normalized);
            if (node.block.kind == MarkdownBlockKind::HtmlBlock)
                html.Block(normalized);
            else if (node.block.kind != MarkdownBlockKind::FencedCode && node.block.kind != MarkdownBlockKind::IndentedCode)
                html.Inline(syntax, node, normalized);
            else
                html.Reset();
            document->elements_[visit.element].nativeHtml = html.valid;
            if (node.block.kind == MarkdownBlockKind::HtmlBlock)
            {
                if (! html.valid)
                    visit.flags |= Code;
                else if (html.wrapper)
                {
                    const auto kind = *html.wrapper;
                    if (Detail::HtmlHeading(kind))
                    {
                        visit.heading                              = Detail::HtmlHeading(kind);
                        document->elements_[visit.element].role    = SemanticRole::Heading;
                        document->elements_[visit.element].heading = visit.heading;
                    }
                    else if (kind == Detail::NativeHtmlKind::Pre)
                        document->elements_[visit.element].role = SemanticRole::Code;
                    else
                        document->elements_[visit.element].role = SemanticRole::Paragraph;
                }
            }
            close           = visit;
            close.close     = true;
            close.paragraph = Paragraph(visit, visit.element);
            Prefix(visit, visit.element);
            auto& paragraph = document->paragraphs_[close.paragraph];
            if (output.text.size() > paragraph.display.first)
            {
                paragraph.markerEnd = output.text.size();
                for (auto owner = node.parent; owner != MarkdownNoNode; owner = nodes[owner].parent)
                    if (nodes[owner].block.kind == MarkdownBlockKind::ListItem)
                    {
                        paragraph.markerOwner = owner;
                        break;
                    }
            }
            htmlDisplayFirst = output.text.size();
            if (html.present && ! html.valid)
            {
                Detail::SemanticValue literal;
                if (node.block.kind != MarkdownBlockKind::HtmlBlock && node.firstInline != MarkdownNoNode)
                    Detail::SliceSemanticValue(normalized, {syntax.Inlines()[node.firstInline].source.first, node.block.source.last}, literal);
                else
                    literal = normalized;
                output.Value(literal, visit.flags | Code, visit.heading);
            }
            else if (node.block.kind == MarkdownBlockKind::HtmlBlock)
                HtmlBlock(visit);
            else if (node.firstInline != MarkdownNoNode)
            {
                visits.push_back(close);
                auto inlineVisit       = visit;
                inlineVisit.inlineNode = true;
                inlineVisit.node       = node.firstInline;
                inlineVisit.parent     = visit.element;
                visits.push_back(inlineVisit);
                return;
            }
            else if (node.block.kind == MarkdownBlockKind::ThematicBreak)
                output.Emit(L"\ufffc", node.block.source, DisplaySpanKind::Object, visit.flags, visit.heading);
            else
                output.Value(normalized, visit.flags, visit.heading);
        }
        visits.push_back(close);
        if (! Leaf(node.block.kind) && node.firstChild != MarkdownNoNode)
        {
            auto child   = visit;
            child.node   = node.firstChild;
            child.parent = visit.element;
            child.column = 0u;
            visits.push_back(child);
        }
    }
    bool Step()
    {
        if (visits.empty())
        {
            document->map_ = SourceDisplayMap::Build(document->syntax_->Source(), output.text, output.spans);
            PrepareSourceFormats();
            return true;
        }
        auto visit = visits.back();
        visits.pop_back();
        if (visit.inlineNode)
            Inline(visit);
        else
            Block(visit);
        return false;
    }
    void PrepareSourceFormats()
    {
        // Runs are a sorted, complete display partition. Convert only primary
        // source owners, then fill hidden syntax with the unformatted style.
        // This happens once with map publication, never in UIA or composition.
        std::vector<StyleRun> mapped(0u);
        const auto runs = document->Runs();
        for (const auto& span : document->Map().Spans())
        {
            if (! span.primary || span.source.first == span.source.last)
                continue;
            auto run =
                std::lower_bound(runs.begin(), runs.end(), span.display.first, [](const auto& item, auto first) { return item.start + item.length <= first; });
            for (; run != runs.end() && run->start < span.display.last; ++run)
            {
                const bool exact = span.kind == DisplaySpanKind::Source;
                const auto first = exact ? span.source.first + std::max(run->start, span.display.first) - span.display.first : span.source.first;
                const auto last  = exact ? span.source.first + std::min(run->start + run->length, span.display.last) - span.display.first : span.source.last;
                if (first < last)
                    mapped.push_back({first, last - first, run->flags, run->heading});
            }
        }
        std::ranges::sort(mapped, {}, &StyleRun::start);
        auto& result      = document->sourceRuns_;
        const auto append = [&](std::size_t first, std::size_t last, unsigned flags, unsigned heading)
        {
            if (first == last)
                return;
            if (! result.empty() && result.back().start + result.back().length == first && result.back().flags == flags && result.back().heading == heading)
                result.back().length += last - first;
            else
                result.push_back({first, last - first, flags, heading});
        };
        std::size_t cursor = 0u;
        for (const auto& run : mapped)
        {
            if (run.start < cursor)
            {
                // A multi-scalar transformed object can have several identical
                // display runs. Conflicting attributes would be ambiguous.
                if (result.empty() || run.start < result.back().start || run.start + run.length > cursor || result.back().flags != run.flags ||
                    result.back().heading != run.heading)
                    throw std::logic_error("Ambiguous source format ownership");
                continue;
            }
            append(cursor, run.start, Regular, 0u);
            append(run.start, run.start + run.length, run.flags, run.heading);
            cursor = run.start + run.length;
        }
        append(cursor, document->Map().Source().size(), Regular, 0u);
    }
};

SemanticBuilder::SemanticBuilder() noexcept  = default;
SemanticBuilder::~SemanticBuilder() noexcept = default;
void SemanticBuilder::Begin(std::shared_ptr<const MarkdownSyntax> syntax)
{
    if (! syntax)
        throw std::invalid_argument("Semantic preparation requires syntax");
    if (! IsValidUtf16(syntax->Source()))
        throw std::invalid_argument("Invalid semantic source UTF-16");
    if (syntax->Source().size() > Document::MaximumUnits || syntax->Nodes().size() > SemanticDocument::MaximumElements ||
        syntax->Inlines().size() > SemanticDocument::MaximumElements - syntax->Nodes().size())
        throw std::length_error("Semantic source/node admission exceeded");
    if (next_ && next_->document->syntax_ == syntax)
        return;
    if (published_ && published_->syntax_ == syntax)
    {
        next_.reset();
        return;
    }
    auto next = std::make_unique<Preparation>(std::move(syntax));
    next_.swap(next);
}
MarkdownAdvance SemanticBuilder::Advance(std::size_t maximumEvents)
{
    if (! maximumEvents)
        throw std::invalid_argument("Semantic preparation budget must be positive");
    if (! next_)
        return {};
    std::size_t work = 0u;
    try
    {
        while (work < maximumEvents)
        {
            ++work;
            if (next_->Step())
            {
                published_ = next_->document;
                next_.reset();
                return {MarkdownPreparationStatus::Complete, work};
            }
        }
    }
    catch (const std::exception&)
    {
        next_.reset();
        throw;
    }
    return {MarkdownPreparationStatus::Pending, work};
}
void SemanticBuilder::Cancel() noexcept
{
    next_.reset();
}
void SemanticBuilder::Clear() noexcept
{
    next_.reset();
    published_.reset();
}
bool SemanticBuilder::HasPendingPreparation() const noexcept
{
    return static_cast<bool>(next_);
}
std::shared_ptr<const SemanticDocument> SemanticBuilder::Published() const noexcept
{
    return published_;
}
} // namespace DxUi::RichText
