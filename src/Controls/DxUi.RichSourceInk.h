#pragma once
#include <DxUi/RichTextLayout.h>
#include <algorithm>

namespace DxUi::RichText::Detail
{
// Presentation of admitted syntax only. All source/SDK geometry remains owned
// by the original projection. No parsing or allocation is performed by Draw.
class RichSourceInk final
{
public:
    RichSourceInk() : events_(0u), ranges_(0u)
    {
    }
    void Clear() noexcept
    {
        syntax_.reset();
        events_.clear();
        ranges_.clear();
        node_ = inline_ = event_ = previous_ = 0u;
        counts_                              = {};
        sorted_ = complete_ = overflow_ = false;
    }
    template <typename Map> bool Advance(const std::shared_ptr<const MarkdownSyntax>& syntax, Map&& map)
    {
        if (syntax_ != syntax)
        {
            Clear();
            syntax_ = syntax;
        }
        if (! syntax_ || complete_)
            return true;
        size_t work         = 0u;
        const auto& nodes   = syntax_->Nodes();
        const auto& inlines = syntax_->Inlines();
        while (node_ < nodes.size() && work++ < WorkPerTurn)
            AddNode(nodes[node_++]);
        while (node_ == nodes.size() && inline_ < inlines.size() && work++ < WorkPerTurn)
            AddInline(inlines[inline_++]);
        if (overflow_)
        {
            // Capacity fallback is the exact ordinary source view, never a
            // partially colored/stale revision or a repeated failed request.
            events_.clear();
            ranges_.clear();
            complete_ = true;
            return true;
        }
        if (node_ != nodes.size() || inline_ != inlines.size())
            return false;
        if (! sorted_)
        {
            // The event bound also bounds this one owner-thread ordering step.
            std::sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) { return a.position < b.position; });
            sorted_ = true;
        }
        work = 0u;
        while (event_ < events_.size() && work++ < WorkPerTurn)
        {
            const auto next = events_[event_].position;
            for (size_t role = counts_.size(); role > 0u; --role)
                if (counts_[role - 1u] != 0)
                {
                    Append({map(previous_), map(next)}, static_cast<TextInkRole>(role - 1u));
                    break;
                }
            previous_         = next;
            const auto& event = events_[event_++];
            counts_[static_cast<size_t>(event.role)] += event.delta;
        }
        complete_ = event_ == events_.size();
        return complete_;
    }
    std::span<const TextInkRange> Ranges() const noexcept
    {
        return complete_ ? std::span<const TextInkRange>(ranges_) : std::span<const TextInkRange>{};
    }
    bool Pending() const noexcept
    {
        return syntax_ && ! complete_;
    }
    static constexpr size_t WorkPerTurn = 256u;

private:
    struct Event
    {
        size_t position;
        TextInkRole role;
        int delta;
    };
    static constexpr size_t MaximumEvents = 65536u;
    std::shared_ptr<const MarkdownSyntax> syntax_;
    std::vector<Event> events_;
    std::vector<TextInkRange> ranges_;
    size_t node_{}, inline_{}, event_{}, previous_{};
    std::array<int, 3> counts_{};
    bool sorted_{}, complete_{}, overflow_{};
    void Add(MarkdownRange range, TextInkRole role)
    {
        if (range.first >= range.last || range.last > syntax_->Source().size() || overflow_)
            return;
        if (events_.size() > MaximumEvents - 2u)
        {
            overflow_ = true;
            return;
        }
        events_.push_back({range.first, role, 1});
        events_.push_back({range.last, role, -1});
    }
    void Append(MarkdownRange range, TextInkRole role)
    {
        if (range.first == range.last)
            return;
        if (! ranges_.empty() && ranges_.back().role == role && ranges_.back().display.last == range.first)
            ranges_.back().display.last = range.last;
        else
            ranges_.push_back({range, role});
    }
    void AddNode(const MarkdownNode& node)
    {
        const auto& block = node.block;
        switch (block.kind)
        {
            case MarkdownBlockKind::AtxHeading:
            case MarkdownBlockKind::SetextHeading:
            case MarkdownBlockKind::ListItem:
                Add({block.source.first, block.content.first}, TextInkRole::Accent);
                if (block.kind != MarkdownBlockKind::ListItem)
                    Add({block.content.last, block.source.last}, TextInkRole::Accent);
                break;
            case MarkdownBlockKind::BlockQuote:
            case MarkdownBlockKind::FencedCode:
            case MarkdownBlockKind::FrontMatter:
                Add({block.source.first, block.content.first}, TextInkRole::Subtle);
                Add({block.content.last, block.source.last}, TextInkRole::Subtle);
                break;
            case MarkdownBlockKind::ThematicBreak: Add(block.source, TextInkRole::Subtle); break;
            case MarkdownBlockKind::ReferenceDefinition:
                Add(block.source, TextInkRole::Subtle);
                Add(node.destination, TextInkRole::Link);
                break;
            default: break;
        }
    }
    void AddInline(const MarkdownInline& item)
    {
        const auto source = syntax_->Source();
        const auto range  = item.source;
        if (range.first >= range.last || range.last > source.size())
            return;
        switch (item.kind)
        {
            case MarkdownInlineKind::Code:
            case MarkdownInlineKind::Emphasis:
            case MarkdownInlineKind::Strong:
            case MarkdownInlineKind::Strikethrough:
            case MarkdownInlineKind::Math:
            {
                const auto marker = source[range.first];
                size_t count      = 0u;
                while (range.first + count < range.last && source[range.first + count] == marker)
                    ++count;
                Add({range.first, range.first + count}, TextInkRole::Subtle);
                if (range.last >= range.first + count * 2u)
                    Add({range.last - count, range.last}, TextInkRole::Subtle);
                break;
            }
            case MarkdownInlineKind::Link:
            case MarkdownInlineKind::Image:
            {
                const auto opener = item.kind == MarkdownInlineKind::Image ? 2u : 1u;
                size_t last       = std::min(range.last, range.first + opener);
                for (auto child = item.firstChild; child != MarkdownNoNode; child = syntax_->Inlines()[child].nextSibling)
                    last = std::max(last, syntax_->Inlines()[child].source.last);
                last = std::min(last, range.last);
                Add({range.first, std::min(range.last, range.first + opener)}, TextInkRole::Subtle);
                Add({std::min(last, range.last), range.last}, TextInkRole::Subtle);
                // Resolved destinations are not source coordinates. Only exact
                // raw spelling found after the label receives URL ink; escaped
                // or reference destinations retain ordinary exact source.
                if (! item.destination.empty())
                {
                    const auto raw = source.substr(last, range.last - last);
                    const auto at  = raw.find(item.destination);
                    if (at != raw.npos)
                        Add({last + at, last + at + item.destination.size()}, TextInkRole::Link);
                    else if (source[range.first] == L'<' && source[range.last - 1u] == L'>')
                        Add({range.first + 1u, range.last - 1u}, TextInkRole::Link);
                }
                break;
            }
            case MarkdownInlineKind::Html: Add(range, TextInkRole::Subtle); break;
            default: break;
        }
    }
};
} // namespace DxUi::RichText::Detail
