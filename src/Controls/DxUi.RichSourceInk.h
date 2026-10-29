#pragma once
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <exception>
#include <new>
#include <utility>

namespace DxUi::RichText::Detail
{
// Presentation of admitted syntax only. Each Advance shares one 256-step budget
// across node/marker walks, inline marker/link scans, merge ordering and event
// publication. No partial ranges escape; source/SDK geometry remains owned by
// the original projection. No parsing or allocation is performed by Draw.
class RichSourceInk final
{
public:
    RichSourceInk() : events_(0u), mergeBuffer_(0u), ranges_(0u)
    {
    }
    void Clear() noexcept
    {
        syntax_.reset();
        events_.clear();
        mergeBuffer_.clear();
        ranges_.clear();
        node_ = inline_ = event_ = previous_ = 0u;
        counts_                              = {};
        inlinePhase_                         = InlinePhase::None;
        childCursor_ = scanPosition_ = markerCount_ = linkLast_ = destinationFirst_ = destinationPosition_ = destinationDepth_ = 0u;
        destinationAngle_ = destinationEscaped_ = false;
        sortWidth_ = mergeStart_ = mergeLeft_ = mergeMiddle_ = mergeRight_ = mergeLeftCursor_ = mergeRightCursor_ = mergeOutput_ = 0u;
        mergeRunActive_ = mergePickReady_ = mergePickLeft_ = false;
        sorted_ = complete_ = overflow_ = false;
        lastWork_                       = 0u;
    }
    template <typename Map> bool Advance(const std::shared_ptr<const MarkdownSyntax>& syntax, Map&& map)
    {
        lastWork_ = 0u;
        try
        {
            return AdvanceCore(syntax, std::forward<Map>(map));
        }
        catch (const std::bad_alloc&)
        {
            Clear();
            throw;
        }
        catch (const std::exception&)
        {
            Clear();
            throw;
        }
    }
    std::span<const TextInkRange> Ranges() const noexcept
    {
        return complete_ ? std::span<const TextInkRange>(ranges_) : std::span<const TextInkRange>{};
    }
    bool Pending() const noexcept
    {
        return syntax_ && ! complete_;
    }
    size_t LastWork() const noexcept
    {
        return lastWork_;
    }
    static constexpr size_t WorkPerTurn = 256u;

private:
    enum class InlinePhase
    {
        None,
        MarkerRun,
        LinkChildren,
        LinkWhitespace,
        LinkBody,
        Finished
    };
    struct Event
    {
        size_t position;
        TextInkRole role;
        int delta;
    };
    static constexpr size_t MaximumEvents = 65536u;
    std::shared_ptr<const MarkdownSyntax> syntax_;
    std::vector<Event> events_;
    std::vector<Event> mergeBuffer_;
    std::vector<TextInkRange> ranges_;
    size_t node_{}, inline_{}, event_{}, previous_{};
    std::array<int, 3> counts_{};
    InlinePhase inlinePhase_{InlinePhase::None};
    size_t childCursor_{}, scanPosition_{}, markerCount_{}, linkLast_{}, destinationFirst_{}, destinationPosition_{}, destinationDepth_{};
    wchar_t marker_{};
    bool destinationAngle_{}, destinationEscaped_{};
    size_t sortWidth_{}, mergeStart_{}, mergeLeft_{}, mergeMiddle_{}, mergeRight_{}, mergeLeftCursor_{}, mergeRightCursor_{}, mergeOutput_{};
    bool mergeRunActive_{}, mergePickReady_{}, mergePickLeft_{};
    bool sorted_{}, complete_{}, overflow_{};
    size_t lastWork_{};

    template <typename Map> bool AdvanceCore(const std::shared_ptr<const MarkdownSyntax>& syntax, Map&& map)
    {
        if (syntax_ != syntax)
        {
            Clear();
            syntax_ = syntax;
        }
        if (! syntax_ || complete_)
            return true;
        const auto& nodes   = syntax_->Nodes();
        const auto& inlines = syntax_->Inlines();
        while (lastWork_ < WorkPerTurn)
        {
            if (overflow_)
            {
                // Capacity fallback is the exact ordinary source view, never a
                // partially colored/stale revision or a repeated failed request.
                events_.clear();
                mergeBuffer_.clear();
                ranges_.clear();
                complete_ = true;
                return true;
            }
            if (node_ < nodes.size())
            {
                AddNode(nodes[node_++]);
                ++lastWork_;
                continue;
            }
            if (inline_ < inlines.size())
            {
                const auto& item = inlines[inline_];
                if (inlinePhase_ == InlinePhase::None)
                {
                    StartInline(item);
                    ++lastWork_;
                    continue;
                }
                if (inlinePhase_ == InlinePhase::Finished)
                {
                    ++inline_;
                    inlinePhase_ = InlinePhase::None;
                    continue;
                }
                AdvanceInline(item);
                continue;
            }
            if (! sorted_)
            {
                AdvanceMerge();
                continue;
            }
            if (event_ < events_.size())
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
                ++lastWork_;
                continue;
            }
            complete_ = true;
            break;
        }
        if (node_ == nodes.size() && inline_ == inlines.size() && sorted_ && event_ == events_.size())
            complete_ = true;
        return complete_;
    }
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
                Add({block.source.first, block.content.first}, TextInkRole::Accent);
                Add({block.content.last, block.source.last}, TextInkRole::Accent);
                break;
            case MarkdownBlockKind::ListItem: Add(block.info, TextInkRole::Accent); break;
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
            case ::DxUi::RichText::MarkdownBlockKind::BlankLine:
            case ::DxUi::RichText::MarkdownBlockKind::Document:
            case ::DxUi::RichText::MarkdownBlockKind::FootnoteDefinition:
            case ::DxUi::RichText::MarkdownBlockKind::HtmlBlock:
            case ::DxUi::RichText::MarkdownBlockKind::IndentedCode:
            case ::DxUi::RichText::MarkdownBlockKind::List:
            case ::DxUi::RichText::MarkdownBlockKind::Paragraph:
            case ::DxUi::RichText::MarkdownBlockKind::SourceEditor:
            case ::DxUi::RichText::MarkdownBlockKind::Table:
            case ::DxUi::RichText::MarkdownBlockKind::TableCell:
            case ::DxUi::RichText::MarkdownBlockKind::TableRow:
            default: break;
        }
    }
    void StartInline(const MarkdownInline& item)
    {
        const auto source = syntax_->Source();
        const auto range  = item.source;
        if (range.first >= range.last || range.last > source.size())
        {
            inlinePhase_ = InlinePhase::Finished;
            return;
        }
        switch (item.kind)
        {
            case MarkdownInlineKind::Code:
            case MarkdownInlineKind::Emphasis:
            case MarkdownInlineKind::Strong:
            case MarkdownInlineKind::Strikethrough:
            case MarkdownInlineKind::Math:
                marker_       = source[range.first];
                markerCount_  = 0u;
                scanPosition_ = range.first;
                inlinePhase_  = InlinePhase::MarkerRun;
                break;
            case MarkdownInlineKind::Link:
            case MarkdownInlineKind::Image:
                linkLast_    = std::min(range.last, range.first + (item.kind == MarkdownInlineKind::Image ? 2u : 1u));
                childCursor_ = item.firstChild;
                inlinePhase_ = InlinePhase::LinkChildren;
                break;
            case MarkdownInlineKind::Html:
                Add(range, TextInkRole::Subtle);
                inlinePhase_ = InlinePhase::Finished;
                break;
            case ::DxUi::RichText::MarkdownInlineKind::FootnoteReference:
            case ::DxUi::RichText::MarkdownInlineKind::HardBreak:
            case ::DxUi::RichText::MarkdownInlineKind::SoftBreak:
            case ::DxUi::RichText::MarkdownInlineKind::Text:
            default: inlinePhase_ = InlinePhase::Finished; break;
        }
    }
    void FinishMarkerRun(const MarkdownInline& item)
    {
        if (markerCount_ != 0u)
        {
            Add({item.source.first, item.source.first + markerCount_}, TextInkRole::Subtle);
            if (item.source.last >= item.source.first + markerCount_ * 2u)
                Add({item.source.last - markerCount_, item.source.last}, TextInkRole::Subtle);
        }
        inlinePhase_ = InlinePhase::Finished;
    }
    void FinishLinkLabel(const MarkdownInline& item)
    {
        const auto range  = item.source;
        const auto source = syntax_->Source();
        linkLast_         = std::min(linkLast_, range.last);
        Add({range.first, std::min(range.last, range.first + (item.kind == MarkdownInlineKind::Image ? 2u : 1u))}, TextInkRole::Subtle);
        Add({linkLast_, range.last}, TextInkRole::Subtle);
        if (linkLast_ + 1u < range.last && source[linkLast_] == L']' && source[linkLast_ + 1u] == L'(')
        {
            destinationPosition_ = linkLast_ + 2u;
            inlinePhase_         = InlinePhase::LinkWhitespace;
            return;
        }
        if (source[range.first] == L'<' && source[range.last - 1u] == L'>')
            Add({range.first + 1u, range.last - 1u}, TextInkRole::Link);
        inlinePhase_ = InlinePhase::Finished;
    }
    void FinishLinkDestination(size_t end)
    {
        Add({destinationFirst_, end}, TextInkRole::Link);
        inlinePhase_ = InlinePhase::Finished;
    }
    void AdvanceInline(const MarkdownInline& item)
    {
        const auto source = syntax_->Source();
        switch (inlinePhase_)
        {
            case InlinePhase::MarkerRun:
                if (scanPosition_ < item.source.last && source[scanPosition_] == marker_)
                {
                    ++scanPosition_;
                    ++markerCount_;
                    ++lastWork_;
                }
                else
                    FinishMarkerRun(item);
                return;
            case InlinePhase::LinkChildren:
                if (childCursor_ != MarkdownNoNode)
                {
                    const auto& child = syntax_->Inlines()[childCursor_];
                    linkLast_         = std::max(linkLast_, child.source.last);
                    childCursor_      = child.nextSibling;
                    ++lastWork_;
                }
                else
                    FinishLinkLabel(item);
                return;
            case InlinePhase::LinkWhitespace:
                if (destinationPosition_ < item.source.last && (source[destinationPosition_] == L' ' || source[destinationPosition_] == L'\t' ||
                                                                source[destinationPosition_] == L'\r' || source[destinationPosition_] == L'\n'))
                {
                    ++destinationPosition_;
                    ++lastWork_;
                    return;
                }
                destinationAngle_    = destinationPosition_ < item.source.last && source[destinationPosition_] == L'<';
                destinationFirst_    = destinationPosition_ + (destinationAngle_ ? 1u : 0u);
                destinationPosition_ = destinationFirst_;
                destinationDepth_    = 0u;
                destinationEscaped_  = false;
                inlinePhase_         = InlinePhase::LinkBody;
                return;
            case InlinePhase::LinkBody:
                if (destinationPosition_ >= item.source.last)
                {
                    FinishLinkDestination(destinationPosition_);
                    return;
                }
                {
                    const auto value = source[destinationPosition_];
                    ++lastWork_;
                    if (destinationEscaped_)
                        destinationEscaped_ = false;
                    else if (value == L'\\' && destinationPosition_ + 1u < item.source.last)
                        destinationEscaped_ = true;
                    else if (destinationAngle_ && value == L'>')
                    {
                        FinishLinkDestination(destinationPosition_);
                        return;
                    }
                    else if (! destinationAngle_)
                    {
                        if ((value == L')' && destinationDepth_ == 0u) || value == L' ' || value == L'\t' || value == L'\r' || value == L'\n')
                        {
                            FinishLinkDestination(destinationPosition_);
                            return;
                        }
                        if (value == L'(')
                            ++destinationDepth_;
                        else if (value == L')' && destinationDepth_ > 0u)
                            --destinationDepth_;
                    }
                    ++destinationPosition_;
                }
                return;
            case ::DxUi::RichText::Detail::RichSourceInk::InlinePhase::Finished:
            case ::DxUi::RichText::Detail::RichSourceInk::InlinePhase::None:
            default: inlinePhase_ = InlinePhase::Finished; return;
        }
    }
    void AdvanceMerge()
    {
        const auto count = events_.size();
        if (count < 2u)
        {
            sorted_ = true;
            return;
        }
        if (sortWidth_ == 0u)
        {
            mergeBuffer_.reserve(count);
            sortWidth_ = 1u;
        }
        if (! mergeRunActive_)
        {
            if (mergeStart_ >= count)
            {
                events_.swap(mergeBuffer_);
                if (sortWidth_ >= count - sortWidth_)
                {
                    sorted_ = true;
                    return;
                }
                sortWidth_ *= 2u;
                mergeStart_ = 0u;
                mergeBuffer_.clear();
                return;
            }
            mergeLeft_        = mergeStart_;
            mergeMiddle_      = std::min(count, mergeLeft_ + sortWidth_);
            mergeRight_       = std::min(count, mergeMiddle_ + sortWidth_);
            mergeLeftCursor_  = mergeLeft_;
            mergeRightCursor_ = mergeMiddle_;
            mergeOutput_      = mergeLeft_;
            mergeRunActive_   = true;
        }
        if (mergePickReady_)
        {
            mergeBuffer_.push_back(events_[mergePickLeft_ ? mergeLeftCursor_++ : mergeRightCursor_++]);
            ++mergeOutput_;
            mergePickReady_ = false;
            ++lastWork_;
        }
        else if (mergeLeftCursor_ < mergeMiddle_ && mergeRightCursor_ < mergeRight_)
        {
            mergePickLeft_  = events_[mergeLeftCursor_].position <= events_[mergeRightCursor_].position;
            mergePickReady_ = true;
            ++lastWork_;
        }
        else if (mergeLeftCursor_ < mergeMiddle_ || mergeRightCursor_ < mergeRight_)
        {
            const bool left = mergeLeftCursor_ < mergeMiddle_;
            mergeBuffer_.push_back(events_[left ? mergeLeftCursor_++ : mergeRightCursor_++]);
            ++mergeOutput_;
            ++lastWork_;
        }
        if (mergeOutput_ == mergeRight_)
        {
            mergeStart_     = mergeRight_;
            mergeRunActive_ = false;
        }
    }
};
} // namespace DxUi::RichText::Detail
