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
    bool Node(std::size_t& at, std::size_t end, std::size_t& index)
    {
        Skip(at, end);
        const auto start = at;
        if (at == end || ! Letter(Text()[at]))
            return Error(FlowchartIssue::InvalidIdentifier, at, std::min(at + 1u, end));
        ++at;
        while (at < end && IdUnit(Text()[at]))
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
            const auto range = model->nodes[index].identifier;
            if (Text().substr(range.first, range.last - range.first) == id)
                break;
            ++index;
        }
        if (index == model->nodes.size())
        {
            if (index == MaximumNodes)
                return Error(FlowchartIssue::NodeCapacity, start, at, FlowchartOutcome::Capacity);
            if (! explicitLabel)
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
        std::size_t previous{};
        if (! Node(at, end, previous))
            return false;
        while (true)
        {
            Skip(at, end);
            if (at == end)
                return true;
            FlowchartEdge edge;
            edge.from            = previous;
            const auto edgeStart = at;
            if (Text().substr(at, 3u) == L"-->")
                edge.arrow = true;
            else if (Text().substr(at, 3u) != L"---")
                return Error(FlowchartIssue::UnsupportedEdge, at, end, FlowchartOutcome::Unsupported);
            at += 3u;
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
            if (! Node(at, end, edge.to))
                return false;
            edge.source = {edgeStart, at};
            if (model->edges.size() == MaximumEdges)
                return Error(FlowchartIssue::EdgeCapacity, edgeStart, at, FlowchartOutcome::Capacity);
            model->edges.push_back(edge);
            previous = edge.to;
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
