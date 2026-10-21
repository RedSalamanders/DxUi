#include "DxUi.NativeFlowchart.h"
#include <algorithm>
#include <stdexcept>

namespace DxUi::RichText::Detail
{
namespace
{
bool Space(wchar_t c) noexcept
{
    return c == L' ' || c == L'\t';
}
bool Letter(wchar_t c) noexcept
{
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'_';
}
bool IdUnit(wchar_t c) noexcept
{
    return Letter(c) || (c >= L'0' && c <= L'9');
}
bool Lead(wchar_t c) noexcept
{
    return c >= 0xd800 && c <= 0xdbff;
}
bool Trail(wchar_t c) noexcept
{
    return c >= 0xdc00 && c <= 0xdfff;
}
} // namespace
struct NativeFlowchartParser::Preparation
{
    std::shared_ptr<FlowchartModel> model;
    std::size_t scan{}, first{}, depth{}, statements{};
    bool quote{}, escaped{}, pipe{}, comment{}, header{}, done{};
    explicit Preparation(std::wstring_view source, std::uint64_t revision) : model(std::make_shared<FlowchartModel>(source, revision))
    {
        if (source.size() > MaximumSourceUnits)
            Fail(FlowchartOutcome::Capacity, FlowchartIssue::SourceCapacity, {0u, source.size()});
    }
    std::wstring_view Text() const noexcept
    {
        return model->source;
    }
    bool Fail(FlowchartOutcome outcome, FlowchartIssue issue, MarkdownRange source) noexcept
    {
        model->outcome    = outcome;
        model->diagnostic = {issue, source};
        model->nodes.clear();
        model->edges.clear();
        done = true;
        return false;
    }
    bool Error(FlowchartIssue issue, std::size_t at, std::size_t end, FlowchartOutcome outcome = FlowchartOutcome::Malformed) noexcept
    {
        // Diagnostics are selectable source ranges. ASCII-only identifiers may
        // reject the first UTF-16 unit of a valid supplementary scalar; include
        // that whole scalar (and original CRLF pairs), never a split ACP.
        while (at && ! IsEditBoundary(Text(), at))
            --at;
        while (end < Text().size() && ! IsEditBoundary(Text(), end))
            ++end;
        return Fail(outcome, issue, {at, end});
    }
    void Skip(std::size_t& at, std::size_t end) const noexcept
    {
        while (at < end && Space(Text()[at]))
            ++at;
    }
    bool Label(std::size_t& at, std::size_t end, wchar_t close, std::wstring& value)
    {
        Skip(at, end);
        const auto start  = at;
        const bool quoted = at < end && Text()[at] == L'"';
        if (quoted)
            ++at;
        while (at < end)
        {
            auto c = Text()[at];
            if ((quoted && c == L'"') || (! quoted && c == close))
            {
                if (quoted)
                {
                    ++at;
                    Skip(at, end);
                }
                else
                    while (! value.empty() && Space(value.back()))
                        value.pop_back();
                if (at == end || Text()[at] != close)
                    return Error(FlowchartIssue::MissingLabelClose, start, at);
                ++at;
                return true;
            }
            const auto remaining = Text().substr(at, end - at);
            if (c == L'<' || c == L'>' || c == L'&' || c == L'`' || c == L'#' || remaining.starts_with(L"$$") || remaining.starts_with(L"fa:") ||
                remaining.starts_with(L"fab:") || remaining.starts_with(L"fas:") || remaining.starts_with(L"far:") || remaining.starts_with(L"fal:") ||
                remaining.starts_with(L"fad:") || remaining.starts_with(L"fat:") || remaining.starts_with(L"fass:"))
                return Error(FlowchartIssue::UnsupportedLabelMarkup, at, at + 1u, FlowchartOutcome::Unsupported);
            if (! quoted && (c == L'[' || c == L']' || c == L'(' || c == L')' || c == L'{' || c == L'}' || c == L';' || c == L'"'))
                return Error(FlowchartIssue::UnsupportedLabelMarkup, at, at + 1u, FlowchartOutcome::Unsupported);
            if (c == L'\\')
            {
                const auto escape = at++;
                if (at == end)
                    return Error(FlowchartIssue::InvalidEscape, escape, at);
                c = Text()[at];
                if (c == L'n')
                    c = L'\n';
                else if (c != L'"' && c != L'\\')
                    return Error(FlowchartIssue::InvalidEscape, escape, at + 1u);
            }
            if (c < L' ' && c != L'\n' && c != L'\t')
                return Error(FlowchartIssue::UnexpectedToken, at, at + 1u);
            if (value.size() == MaximumLabelUnits)
                return Error(FlowchartIssue::LabelCapacity, start, at + 1u, FlowchartOutcome::Capacity);
            value.push_back(c);
            ++at;
        }
        return Error(FlowchartIssue::MissingLabelClose, start, end);
    }
    bool Node(std::size_t& at, std::size_t end, std::size_t& index, bool target = false)
    {
        Skip(at, end);
        const auto start    = at;
        const bool terminal = model->family == NativeDiagramFamily::State && Text().substr(at, 3u) == L"[*]";
        if (! terminal && (at == end || ! Letter(Text()[at])))
            return Error(FlowchartIssue::InvalidIdentifier, at, std::min(at + 1u, end));
        at += terminal ? 3u : 1u;
        while (! terminal && at < end && IdUnit(Text()[at]))
            ++at;
        const auto idEnd = at;
        const auto id    = Text().substr(start, idEnd - start);
        if (id.size() > MaximumIdentifierUnits)
            return Error(FlowchartIssue::IdentifierCapacity, start, idEnd, FlowchartOutcome::Capacity);
        for (const auto reserved : {L"end", L"subgraph", L"direction", L"style", L"class", L"classDef", L"click", L"linkStyle"})
            if (id == reserved)
                return Error(FlowchartIssue::UnsupportedStatement, start, idEnd, FlowchartOutcome::Unsupported);
        FlowchartNode candidate;
        candidate.identifier = {start, idEnd};
        candidate.key        = terminal ? (target ? L"@end" : L"@start") : std::wstring(id);
        if (model->family == NativeDiagramFamily::State)
            candidate.shape = terminal ? FlowchartShape::Circle : FlowchartShape::Rounded;
        if (terminal)
            candidate.label = target ? L"◉" : L"●";
        Skip(at, end);
        bool explicitLabel = false;
        if (at < end && (Text()[at] == L'[' || Text()[at] == L'(' || Text()[at] == L'{'))
        {
            const auto open   = Text()[at++];
            const auto close  = open == L'[' ? L']' : (open == L'(' ? L')' : L'}');
            candidate.shape   = open == L'[' ? FlowchartShape::Rectangle : (open == L'(' ? FlowchartShape::Rounded : FlowchartShape::Diamond);
            const bool circle = open == L'(' && at < end && Text()[at] == L'(';
            if (circle)
            {
                ++at;
                candidate.shape = FlowchartShape::Circle;
            }
            // Other Mermaid shape openers must not turn into ordinary text.
            if (at < end && (Text()[at] == L'[' || Text()[at] == L'(' || Text()[at] == L'{' || Text()[at] == L'/' || Text()[at] == L'\\'))
                return Error(FlowchartIssue::UnsupportedShape, at - 1u, at + 1u, FlowchartOutcome::Unsupported);
            if (! Label(at, end, close, candidate.label))
                return false;
            if (circle && (at == end || Text()[at++] != L')'))
                return Error(FlowchartIssue::MissingLabelClose, start, at);
            explicitLabel = true;
        }
        else if (at < end && Text()[at] == L'@')
            return Error(FlowchartIssue::UnsupportedShape, at, at + 1u, FlowchartOutcome::Unsupported);
        candidate.declaration = {start, at};
        index                 = 0u;
        while (index < model->nodes.size())
        {
            if (model->nodes[index].key == candidate.key)
                break;
            ++index;
        }
        if (index == model->nodes.size())
        {
            if (index == MaximumNodes)
                return Error(FlowchartIssue::NodeCapacity, start, at, FlowchartOutcome::Capacity);
            if (! explicitLabel && ! terminal)
                candidate.label.assign(id);
            model->nodes.push_back(candidate);
        }
        else if (explicitLabel)
        {
            // A bare edge endpoint never resets a prior declaration's label.
            auto& node       = model->nodes[index];
            node.label       = candidate.label;
            node.shape       = candidate.shape;
            node.declaration = candidate.declaration;
        }
        return true;
    }
    bool Statement(std::size_t end)
    {
        auto at = first;
        Skip(at, end);
        while (end > at && Space(Text()[end - 1u]))
            --end;
        if (at == end)
            return true;
        if (++statements > MaximumStatements)
            return Error(FlowchartIssue::StatementCapacity, at, end, FlowchartOutcome::Capacity);
        if (! header)
        {
            const auto line = Text().substr(at, end - at);
            if (line == L"stateDiagram-v2" || line == L"stateDiagram" || line == L"classDiagram")
            {
                model->family    = line == L"classDiagram" ? NativeDiagramFamily::Class : NativeDiagramFamily::State;
                model->direction = FlowchartDirection::TopBottom;
                header           = true;
                return true;
            }
            const auto start = at;
            while (at < end && Letter(Text()[at]))
                ++at;
            const auto word = Text().substr(start, at - start);
            if (word != L"flowchart" && word != L"graph")
                return Error(FlowchartIssue::UnsupportedFamily, start, at, FlowchartOutcome::Unsupported);
            if (at == end || ! Space(Text()[at]))
                return Error(FlowchartIssue::InvalidDirection, at, end);
            Skip(at, end);
            const auto direction = Text().substr(at, end - at);
            if (direction == L"TB" || direction == L"TD")
                model->direction = FlowchartDirection::TopBottom;
            else if (direction == L"BT")
                model->direction = FlowchartDirection::BottomTop;
            else if (direction == L"LR")
                model->direction = FlowchartDirection::LeftRight;
            else if (direction == L"RL")
                model->direction = FlowchartDirection::RightLeft;
            else
                return Error(FlowchartIssue::InvalidDirection, at, end);
            header = true;
            return true;
        }
        if (model->family != NativeDiagramFamily::Flowchart && Text().substr(at, 10u) == L"direction ")
        {
            const auto direction = Text().substr(at + 10u, end - at - 10u);
            if (direction == L"TB" || direction == L"TD")
                model->direction = FlowchartDirection::TopBottom;
            else if (direction == L"BT")
                model->direction = FlowchartDirection::BottomTop;
            else if (direction == L"LR")
                model->direction = FlowchartDirection::LeftRight;
            else if (direction == L"RL")
                model->direction = FlowchartDirection::RightLeft;
            else
                return Error(FlowchartIssue::InvalidDirection, at, end);
            return true;
        }
        if (model->family == NativeDiagramFamily::State && Text().substr(at, 6u) == L"state ")
        {
            at += 6u;
            Skip(at, end);
            if (at < end && Text()[at] == L'"')
            {
                const auto firstLabel = ++at;
                while (at < end && Text()[at] != L'"')
                    ++at;
                if (at == end || at - firstLabel > MaximumLabelUnits)
                    return Error(FlowchartIssue::MissingLabelClose, firstLabel, at);
                const auto label = Text().substr(firstLabel, at - firstLabel);
                ++at;
                Skip(at, end);
                if (Text().substr(at, 3u) != L"as ")
                    return Error(FlowchartIssue::UnexpectedToken, at, end);
                at += 3u;
                std::size_t node{};
                if (! Node(at, end, node))
                    return false;
                Skip(at, end);
                if (at != end)
                    return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
                model->nodes[node].label.assign(label);
                return true;
            }
        }
        if (model->family == NativeDiagramFamily::Class && Text().substr(at, 6u) == L"class ")
            at += 6u;
        std::size_t previous{};
        if (! Node(at, end, previous))
            return false;
        while (true)
        {
            Skip(at, end);
            if (at == end)
                return true;
            if (model->family != NativeDiagramFamily::Flowchart && Text()[at] == L':')
            {
                ++at;
                Skip(at, end);
                auto& label = model->nodes[previous].label;
                if (label.size() + end - at + 1u > MaximumLabelUnits)
                    return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
                label.append(L"\n").append(Text().substr(at, end - at));
                return true;
            }
            FlowchartEdge edge;
            edge.from            = previous;
            const auto edgeStart = at;
            bool reverse{};
            if (model->family == NativeDiagramFamily::Class && (Text().substr(at, 4u) == L"<|--" || Text().substr(at, 4u) == L"--|>"))
            {
                reverse    = Text()[at] == L'<';
                edge.arrow = true;
                edge.label = L"inherits";
                at += 4u;
            }
            else
            {
                if (Text().substr(at, 3u) == L"-->")
                    edge.arrow = true;
                else if (model->family == NativeDiagramFamily::Class && Text().substr(at, 2u) == L"--")
                {
                    at -= 1u;
                }
                else if (Text().substr(at, 3u) != L"---")
                    return Error(FlowchartIssue::UnsupportedEdge, at, end, FlowchartOutcome::Unsupported);
                at += 3u;
            }
            // In Mermaid, ---oB/---xB are circle/cross edges, not IDs oB/xB.
            if (at < end && ((! edge.arrow && (Text()[at] == L'o' || Text()[at] == L'x')) || Text()[at] == L'>' || Text()[at] == L'-'))
            {
                return Error(FlowchartIssue::UnsupportedEdge, edgeStart, at + 1u, FlowchartOutcome::Unsupported);
            }
            Skip(at, end);
            if (at < end && Text()[at] == L'|')
            {
                ++at;
                if (! Label(at, end, L'|', edge.label))
                    return false;
                Skip(at, end);
            }
            if (at == end)
                return Error(FlowchartIssue::MissingTarget, edgeStart, end);
            if (! Node(at, end, edge.to, true))
                return false;
            const auto next = edge.to;
            Skip(at, end);
            if (model->family != NativeDiagramFamily::Flowchart && at < end && Text()[at] == L':')
            {
                ++at;
                Skip(at, end);
                if (end - at > MaximumLabelUnits)
                    return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
                edge.label.assign(Text().substr(at, end - at));
                at = end;
            }
            if (reverse)
                std::swap(edge.from, edge.to);
            edge.source = {edgeStart, at};
            if (model->edges.size() == MaximumEdges)
                return Error(FlowchartIssue::EdgeCapacity, edgeStart, at, FlowchartOutcome::Capacity);
            model->edges.push_back(edge);
            previous = next;
        }
    }
    void Step()
    {
        if (done)
            return;
        const auto text = Text();
        if (scan == text.size())
        {
            if (! comment && ! Statement(scan))
                return;
            if (! header)
            {
                Error(FlowchartIssue::MissingHeader, 0u, scan);
                return;
            }
            done = true;
            return;
        }
        const auto c       = text[scan];
        const bool newline = c == L'\r' || c == L'\n';
        if (! comment && ! quote && ! depth && ! pipe && c == L'%' && scan + 1u < text.size() && text[scan + 1u] == L'%')
        {
            if (scan + 2u < text.size() && text[scan + 2u] == L'{')
            {
                Error(FlowchartIssue::UnsupportedConfiguration, scan, scan + 3u, FlowchartOutcome::Unsupported);
                return;
            }
            if (! Statement(scan))
                return;
            comment = true;
        }
        if (newline || (! comment && ! quote && ! depth && c == L';'))
        {
            if (! comment && ! Statement(scan))
                return;
            ++scan;
            if (c == L'\r' && scan < text.size() && text[scan] == L'\n')
                ++scan;
            first   = scan;
            comment = quote = escaped = pipe = false;
            depth                            = 0u;
            return;
        }
        if (! comment)
        {
            if (quote)
            {
                if (escaped)
                    escaped = false;
                else if (c == L'\\')
                    escaped = true;
                else if (c == L'"')
                    quote = false;
            }
            else if (escaped)
                escaped = false;
            else if (c == L'\\' && (depth || pipe))
                escaped = true;
            else if (c == L'|')
                pipe = ! pipe;
            else if (c == L'"')
                quote = true;
            else if (c == L'[' || c == L'(' || c == L'{')
                ++depth;
            else if ((c == L']' || c == L')' || c == L'}') && depth)
                --depth;
        }
        ++scan;
    }
};
NativeFlowchartParser::NativeFlowchartParser() noexcept  = default;
NativeFlowchartParser::~NativeFlowchartParser() noexcept = default;
void NativeFlowchartParser::Begin(std::wstring_view source, std::uint64_t revision)
{
    if (source.size() > Document::MaximumUnits)
        throw std::length_error("Flowchart body exceeds Document admission");
    for (std::size_t i = 0u; i < source.size(); ++i)
    {
        if (Lead(source[i]))
        {
            if (++i == source.size() || ! Trail(source[i]))
                throw std::invalid_argument("Invalid flowchart UTF-16");
        }
        else if (Trail(source[i]) || ! source[i])
            throw std::invalid_argument("Invalid flowchart scalar");
    }
    const auto same = [&](const auto& result) { return result && result->revision == revision && result->source == source; };
    if (preparation_ && same(preparation_->model))
        return;
    if (! preparation_ && same(published_))
        return;
    auto next    = std::make_unique<Preparation>(source, revision);
    preparation_ = std::move(next);
}
MarkdownAdvance NativeFlowchartParser::Advance(std::size_t maximumWorkUnits)
{
    if (! preparation_)
        return {MarkdownPreparationStatus::Idle, 0u};
    std::size_t work{};
    try
    {
        while (work < maximumWorkUnits)
        {
            preparation_->Step();
            ++work;
            if (preparation_->done)
            {
                published_ = preparation_->model;
                preparation_.reset();
                return {MarkdownPreparationStatus::Complete, work};
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        preparation_.reset();
        throw;
    }
    catch (const std::length_error&)
    {
        preparation_.reset();
        throw;
    }
    return {MarkdownPreparationStatus::Pending, work};
}
bool NativeFlowchartParser::HasPendingPreparation() const noexcept
{
    return ! ! preparation_;
}
std::shared_ptr<const FlowchartModel> NativeFlowchartParser::Published() const noexcept
{
    return published_;
}
void NativeFlowchartParser::Cancel() noexcept
{
    preparation_.reset();
}
void NativeFlowchartParser::Clear() noexcept
{
    Cancel();
    published_.reset();
}
} // namespace DxUi::RichText::Detail
