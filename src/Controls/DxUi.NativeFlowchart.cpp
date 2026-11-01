#include "DxUi.NativeFlowchart.h"
#include "DxUi.MarkdownCharacterReference.h"
#include <algorithm>
#include <array>
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
    std::size_t scan{}, first{}, depth{}, statements{}, classBlockNode{}, classBlockStart{};
    std::array<bool, MaximumNodes> stateDescriptions{};
    std::array<std::uint8_t, MaximumNodes> pseudoKinds{};
    bool quote{}, escaped{}, pipe{}, comment{}, header{}, done{}, classBlock{};
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
    bool PlainTextUnit(std::size_t& at, std::size_t end, std::wstring& value, bool classMember = false, bool quoted = false)
    {
        const auto start         = at;
        auto c                   = Text()[at];
        const auto remaining     = Text().substr(at, end - at);
        const auto entityEnd     = c == L'&' ? remaining.find(L';') : std::wstring_view::npos;
        const bool encodedEntity = c == L'&' && entityEnd != std::wstring_view::npos &&
                                   remaining.substr(1u, entityEnd - 1u).find_first_of(L" \t\r\n|[]{}()") == std::wstring_view::npos;
        if (c == L'<' || c == L'>' || (c == L'&' && model->family != NativeDiagramFamily::Flowchart) || c == L'`' || c == L'#' || (classMember && c == L'~') ||
            remaining.starts_with(L"$$") || remaining.starts_with(L"fa:") || remaining.starts_with(L"fab:") || remaining.starts_with(L"fas:") ||
            remaining.starts_with(L"far:") || remaining.starts_with(L"fal:") || remaining.starts_with(L"fad:") || remaining.starts_with(L"fat:") ||
            remaining.starts_with(L"fass:") || encodedEntity)
            return Error(FlowchartIssue::UnsupportedLabelMarkup, at, at + 1u, FlowchartOutcome::Unsupported);
        if (quoted && c == L'\\')
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
        return true;
    }
    bool PlainText(std::size_t& at, std::size_t end, std::wstring& value, bool classMember = false, bool quoted = false)
    {
        while (at < end)
            if (! PlainTextUnit(at, end, value, classMember, quoted))
                return false;
        return true;
    }
    bool StateDescription(std::size_t node, std::wstring_view description, std::size_t firstUnit, std::size_t end)
    {
        if (description.empty())
            return true;
        if (model->nodes[node].shape == FlowchartShape::Diamond || model->nodes[node].shape == FlowchartShape::Bar)
            return Error(FlowchartIssue::UnsupportedStatement, firstUnit, end, FlowchartOutcome::Unsupported);
        auto& label = model->nodes[node].label;
        if (stateDescriptions[node])
        {
            if (label.size() == MaximumLabelUnits || description.size() > MaximumLabelUnits - label.size() - 1u)
                return Error(FlowchartIssue::LabelCapacity, firstUnit, end, FlowchartOutcome::Capacity);
            label.push_back(L'\n');
            label.append(description);
        }
        else
        {
            // The first state description replaces the implicit ID label;
            // later descriptions append even when the first equals that ID.
            label.assign(description);
            stateDescriptions[node] = true;
        }
        return true;
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
            if (model->family == NativeDiagramFamily::Flowchart && c == L'<')
            {
                const auto rest   = Text().substr(at, end - at);
                const auto length = rest.starts_with(L"<br>") ? 4u : rest.starts_with(L"<br/>") ? 5u : rest.starts_with(L"<br />") ? 6u : 0u;
                if (length)
                {
                    if (value.size() == MaximumLabelUnits)
                        return Error(FlowchartIssue::LabelCapacity, at, at + length, FlowchartOutcome::Capacity);
                    value.push_back(L'\n');
                    at += length;
                    continue;
                }
            }
            if (model->family == NativeDiagramFamily::Flowchart && c == L'#')
            {
                const auto startReference = at;
                auto finish               = at + 1u;
                while (finish < end && finish - at < 33u && Text()[finish] != L';')
                    ++finish;
                if (finish == end || Text()[finish] != L';')
                    return Error(FlowchartIssue::UnsupportedLabelMarkup, at, std::min(end, finish + 1u), FlowchartOutcome::Unsupported);
                std::array<wchar_t, 35u> spelling{};
                spelling[0u]        = L'&';
                const bool numeric  = at + 1u < finish && Text()[at + 1u] >= L'0' && Text()[at + 1u] <= L'9';
                const auto copyFrom = at + (numeric ? 0u : 1u);
                const auto length   = finish + 1u - copyFrom;
                std::copy_n(Text().data() + copyFrom, length, spelling.data() + 1u);
                MarkdownLexeme decoded;
                if (! DecodeMarkdownCharacterReference(std::wstring_view(spelling.data(), length + 1u), 0u, decoded) || decoded.source.last != length + 1u)
                    return Error(FlowchartIssue::UnsupportedLabelMarkup, startReference, finish + 1u, FlowchartOutcome::Unsupported);
                for (std::size_t i = 0u; i < decoded.valueLength; ++i)
                    if ((decoded.value[i] < L' ' && decoded.value[i] != L'\n' && decoded.value[i] != L'\t') ||
                        (decoded.value[i] >= 0x7f && decoded.value[i] <= 0x9f))
                        return Error(FlowchartIssue::UnsupportedLabelMarkup, startReference, finish + 1u, FlowchartOutcome::Unsupported);
                if (decoded.valueLength > MaximumLabelUnits - value.size())
                    return Error(FlowchartIssue::LabelCapacity, startReference, finish + 1u, FlowchartOutcome::Capacity);
                value.append(decoded.value.data(), decoded.valueLength);
                at = finish + 1u;
                continue;
            }
            if (! quoted && (c == L'[' || c == L']' || c == L'(' || c == L')' || c == L'{' || c == L'}' || c == L';' || c == L'"'))
                return Error(FlowchartIssue::UnsupportedLabelMarkup, at, at + 1u, FlowchartOutcome::Unsupported);
            if (! PlainTextUnit(at, end, value, false, quoted))
                return false;
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
        for (const auto reserved : {L"end",
                                    L"subgraph",
                                    L"direction",
                                    L"style",
                                    L"class",
                                    L"classDef",
                                    L"click",
                                    L"linkStyle",
                                    L"note",
                                    L"accTitle",
                                    L"accDescr",
                                    L"title",
                                    L"cssClass"})
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
        if (model->family != NativeDiagramFamily::Flowchart && ! terminal && at < end && (Text()[at] == L'[' || Text()[at] == L'(' || Text()[at] == L'{'))
            return Error(FlowchartIssue::UnsupportedShape, at, at + 1u, FlowchartOutcome::Unsupported);
        if (at < end && (Text()[at] == L'[' || Text()[at] == L'(' || Text()[at] == L'{'))
        {
            const auto open    = Text()[at++];
            const bool stadium = model->family == NativeDiagramFamily::Flowchart && open == L'(' && at < end && Text()[at] == L'[';
            const auto close   = stadium ? L']' : (open == L'[' ? L']' : (open == L'(' ? L')' : L'}'));
            candidate.shape    = stadium ? FlowchartShape::Stadium
                                         : (open == L'[' ? FlowchartShape::Rectangle : (open == L'(' ? FlowchartShape::Rounded : FlowchartShape::Diamond));
            if (stadium)
                ++at;
            const bool circle = ! stadium && open == L'(' && at < end && Text()[at] == L'(';
            if (circle)
            {
                ++at;
                candidate.shape = FlowchartShape::Circle;
            }
            // Other Mermaid shape openers must not turn into ordinary text.
            if (at < end && (Text()[at] == L'[' || Text()[at] == L'(' || Text()[at] == L'{' || Text()[at] == L'/' || Text()[at] == L'\\'))
                return Error(FlowchartIssue::UnsupportedShape, at - 1u, at + 1u, FlowchartOutcome::Unsupported);
            if (! circle && ! stadium && at < end && ((open == L'(' && Text()[at] == L'-') || (open == L'[' && Text()[at] == L'|')))
                return Error(FlowchartIssue::UnsupportedShape, at - 1u, at + 1u, FlowchartOutcome::Unsupported);
            if (! Label(at, end, close, candidate.label))
                return false;
            if (circle && (at == end || Text()[at++] != L')'))
                return Error(FlowchartIssue::MissingLabelClose, start, at);
            if (stadium && (at == end || Text()[at++] != L')'))
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
    bool NodeGroup(std::size_t& at, std::size_t end, std::vector<std::size_t>& group, bool target = false)
    {
        while (true)
        {
            Skip(at, end);
            if (at < end && Text()[at] == L'&')
                return Error(FlowchartIssue::UnexpectedToken, at, at + 1u);
            std::size_t node{};
            if (! Node(at, end, node, target))
                return false;
            group.push_back(node);
            Skip(at, end);
            if (at == end || Text()[at] != L'&')
                return true;
            const auto separator = at++;
            Skip(at, end);
            if (at == end || ! Letter(Text()[at]))
                return Error(FlowchartIssue::UnexpectedToken, separator, at);
        }
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
        if (classBlock)
        {
            if (Text().substr(at, end - at) == L"}")
            {
                classBlock = false;
                return true;
            }
            const auto memberStart = at;
            if (! Letter(Text()[at]) && Text()[at] != L'+' && Text()[at] != L'-')
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            const auto member = Text().substr(at, end - at);
            if (member.find_first_of(L"{}\";") != std::wstring_view::npos || member.starts_with(L"class ") || member.starts_with(L"direction ") ||
                member.starts_with(L"click ") || member.starts_with(L"style "))
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            auto& label = model->nodes[classBlockNode].label;
            if (label.size() == MaximumLabelUnits)
                return Error(FlowchartIssue::LabelCapacity, memberStart, end, FlowchartOutcome::Capacity);
            label.push_back(L'\n');
            return PlainText(at, end, label, true);
        }
        if (model->family != NativeDiagramFamily::Flowchart && Text().substr(at, 9u) == L"direction" && (at + 9u == end || Space(Text()[at + 9u])))
        {
            auto token = at + 9u;
            Skip(token, end);
            const auto directionStart = token;
            while (token < end && ! Space(Text()[token]))
                ++token;
            const auto direction = Text().substr(directionStart, token - directionStart);
            Skip(token, end);
            if (direction.empty() || token != end)
                return Error(FlowchartIssue::InvalidDirection, at, end);
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
        bool stateDeclaration = false;
        if (model->family == NativeDiagramFamily::State && Text().substr(at, 5u) == L"state" && at + 5u < end && Space(Text()[at + 5u]))
        {
            stateDeclaration = true;
            at += 5u;
            Skip(at, end);
            if (at < end && Text()[at] == L'"')
            {
                const auto firstLabel = ++at;
                while (at < end)
                {
                    if (Text()[at] == L'\\' && at + 1u < end)
                    {
                        at += 2u;
                        continue;
                    }
                    if (Text()[at] == L'"')
                        break;
                    ++at;
                }
                if (at == end)
                    return Error(FlowchartIssue::MissingLabelClose, firstLabel, at);
                if (at - firstLabel > MaximumLabelUnits)
                    return Error(FlowchartIssue::LabelCapacity, firstLabel, at, FlowchartOutcome::Capacity);
                std::wstring label(0u, L' ');
                auto labelAt = firstLabel;
                if (! PlainText(labelAt, at, label, false, true))
                    return false;
                ++at;
                Skip(at, end);
                if (Text().substr(at, 2u) != L"as" || (at + 2u < end && ! Space(Text()[at + 2u])))
                    return Error(FlowchartIssue::UnexpectedToken, at, end);
                at += 2u;
                Skip(at, end);
                std::size_t node{};
                if (! Node(at, end, node))
                    return false;
                Skip(at, end);
                if (at != end)
                    return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
                return StateDescription(node, label, firstLabel, end);
            }
        }
        if (stateDeclaration)
        {
            struct Pseudostate
            {
                std::wstring_view marker;
                FlowchartShape shape;
                std::uint8_t kind;
            };
            constexpr std::array pseudostates{Pseudostate{L"<<choice>>", FlowchartShape::Diamond, 1u},
                                              Pseudostate{L"<<fork>>", FlowchartShape::Bar, 2u},
                                              Pseudostate{L"<<join>>", FlowchartShape::Bar, 3u}};
            for (const auto& [marker, shape, kind] : pseudostates)
            {
                const auto markerAt = Text().find(marker, at);
                if (markerAt == std::wstring_view::npos || markerAt + marker.size() != end)
                    continue;
                std::size_t node{};
                if (! Node(at, markerAt, node))
                    return false;
                Skip(at, markerAt);
                if (at != markerAt || stateDescriptions[node] || (pseudoKinds[node] != 0u && pseudoKinds[node] != kind))
                    return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
                pseudoKinds[node]        = kind;
                model->nodes[node].shape = shape;
                model->nodes[node].label.clear();
                return true;
            }
        }
        if (model->family == NativeDiagramFamily::Class && Text().substr(at, 5u) == L"class" && at + 5u < end && Space(Text()[at + 5u]))
        {
            at += 5u;
            Skip(at, end);
            if (end > at && Text()[end - 1u] == L'{')
            {
                const auto blockStart = end - 1u;
                std::size_t node{};
                if (! Node(at, blockStart, node))
                    return false;
                Skip(at, blockStart);
                if (at != blockStart)
                    return Error(FlowchartIssue::UnsupportedStatement, at, blockStart, FlowchartOutcome::Unsupported);
                classBlockNode  = node;
                classBlockStart = blockStart;
                classBlock      = true;
                return true;
            }
        }
        // Explicit size construction may propagate Debug iterator-proxy
        // allocation failure to Advance's failure-atomic boundary.
        std::vector<std::size_t> previousGroup(0u);
        if (model->family == NativeDiagramFamily::Flowchart)
        {
            if (! NodeGroup(at, end, previousGroup))
                return false;
        }
        else
        {
            std::size_t previous{};
            if (! Node(at, end, previous))
                return false;
            previousGroup.push_back(previous);
        }
        while (true)
        {
            Skip(at, end);
            if (at == end)
                return true;
            if (Text().substr(at, 3u) == L":::")
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            if (model->family != NativeDiagramFamily::Flowchart && Text()[at] == L':')
            {
                ++at;
                Skip(at, end);
                if (model->family == NativeDiagramFamily::State)
                {
                    const auto descriptionStart = at;
                    std::wstring description(0u, L' ');
                    if (! PlainText(at, end, description))
                        return false;
                    return StateDescription(previousGroup.front(), description, descriptionStart, end);
                }
                auto& label = model->nodes[previousGroup.front()].label;
                if (label.size() == MaximumLabelUnits)
                    return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
                label.push_back(L'\n');
                return PlainText(at, end, label, model->family == NativeDiagramFamily::Class);
            }
            FlowchartEdge edge;
            const auto edgeStart = at;
            bool reverse{};
            const auto relationText = Text().substr(at, end - at);
            if (model->family == NativeDiagramFamily::Class &&
                (relationText.starts_with(L"--*") || relationText.starts_with(L"--o") || relationText.starts_with(L"..") || relationText.starts_with(L"<|..")))
                return Error(FlowchartIssue::UnsupportedEdge,
                             edgeStart,
                             std::min(end, at + (relationText.starts_with(L"<|..") ? 4u : 3u)),
                             FlowchartOutcome::Unsupported);
            bool inlineLabel   = false;
            const auto readEnd = [&](std::size_t start, FlowchartLineStyle style, std::size_t& finish, std::size_t& ranks, bool& arrow)
            {
                const wchar_t marker = style == FlowchartLineStyle::Dotted ? L'.' : style == FlowchartLineStyle::Thick ? L'=' : L'-';
                auto cursor          = start;
                while (cursor < end && Text()[cursor] == marker)
                    ++cursor;
                const auto count = cursor - start;
                if (style == FlowchartLineStyle::Dotted)
                {
                    if (! count || cursor == end || Text()[cursor] != L'-')
                        return false;
                    ++cursor;
                    ranks = count;
                }
                else
                {
                    if (count < 2u)
                        return false;
                    ranks = count - 1u;
                }
                arrow = cursor < end && Text()[cursor] == L'>';
                if (arrow)
                    ++cursor;
                else if (style != FlowchartLineStyle::Dotted)
                {
                    if (count < 3u)
                        return false;
                    --ranks;
                }
                finish = cursor;
                return true;
            };
            if (model->family == NativeDiagramFamily::Flowchart)
            {
                struct InlineOperator
                {
                    std::wstring_view prefix;
                    FlowchartLineStyle style;
                };
                constexpr std::array operators{InlineOperator{L"--", FlowchartLineStyle::Solid},
                                               InlineOperator{L"-.", FlowchartLineStyle::Dotted},
                                               InlineOperator{L"==", FlowchartLineStyle::Thick}};
                for (const auto& op : operators)
                {
                    const auto labelStart = at + op.prefix.size();
                    if (! relationText.starts_with(op.prefix) || labelStart >= end || ! Space(Text()[labelStart]))
                        continue;
                    auto begin = labelStart;
                    Skip(begin, end);
                    auto close = begin;
                    std::size_t finish{}, ranks{};
                    bool arrow{};
                    while (close < end && ! readEnd(close, op.style, finish, ranks, arrow))
                    {
                        if (close - begin > MaximumLabelUnits)
                            return Error(FlowchartIssue::LabelCapacity, begin, close, FlowchartOutcome::Capacity);
                        const wchar_t marker = op.style == FlowchartLineStyle::Dotted ? L'.' : op.style == FlowchartLineStyle::Thick ? L'=' : L'-';
                        if (Text()[close] == marker)
                            while (close < end && Text()[close] == marker)
                                ++close;
                        else
                            ++close;
                    }
                    auto labelEnd = close;
                    while (labelEnd > begin && Space(Text()[labelEnd - 1u]))
                        --labelEnd;
                    if (close == end || labelEnd == begin)
                        return Error(FlowchartIssue::UnsupportedEdge, edgeStart, end, FlowchartOutcome::Unsupported);
                    if (ranks > MaximumLinkRanks)
                        return Error(FlowchartIssue::LinkRankCapacity, close, finish, FlowchartOutcome::Capacity);
                    const auto markup = Text().substr(begin, labelEnd - begin).find_first_of(L"\"|");
                    if (markup != std::wstring_view::npos)
                        return Error(FlowchartIssue::UnsupportedLabelMarkup, begin + markup, begin + markup + 1u, FlowchartOutcome::Unsupported);
                    auto labelAt = begin;
                    if (! PlainText(labelAt, labelEnd, edge.label))
                        return false;
                    edge.arrow        = arrow;
                    edge.relation     = edge.arrow ? FlowchartRelation::Arrow : FlowchartRelation::Line;
                    edge.style        = op.style;
                    edge.minimumRanks = ranks;
                    at                = finish;
                    inlineLabel       = true;
                    break;
                }
            }
            if (inlineLabel)
            {
                // The operator and its bounded plain-text label are already consumed.
            }
            else if (model->family == NativeDiagramFamily::Class && (Text().substr(at, 4u) == L"<|--" || Text().substr(at, 4u) == L"--|>"))
            {
                reverse       = Text()[at] == L'<';
                edge.arrow    = true;
                edge.relation = FlowchartRelation::Inheritance;
                at += 4u;
            }
            else
            {
                std::size_t operatorLength{};
                std::size_t finish{}, ranks{};
                bool arrow{};
                const auto style        = Text().substr(at, 2u) == L"-."   ? FlowchartLineStyle::Dotted
                                          : at < end && Text()[at] == L'=' ? FlowchartLineStyle::Thick
                                                                           : FlowchartLineStyle::Solid;
                const auto closingStart = at + (style == FlowchartLineStyle::Dotted ? 1u : 0u);
                if (model->family == NativeDiagramFamily::Flowchart && readEnd(closingStart, style, finish, ranks, arrow))
                {
                    if (ranks > MaximumLinkRanks)
                        return Error(FlowchartIssue::LinkRankCapacity, at, finish, FlowchartOutcome::Capacity);
                    edge.arrow        = arrow;
                    edge.relation     = arrow ? FlowchartRelation::Arrow : FlowchartRelation::Line;
                    edge.style        = style;
                    edge.minimumRanks = ranks;
                    operatorLength    = finish - at;
                }
                else if (model->family == NativeDiagramFamily::Flowchart && Text().substr(at, 3u) == L"~~~")
                {
                    edge.style     = FlowchartLineStyle::Invisible;
                    operatorLength = 3u;
                }
                else if (Text().substr(at, 3u) == L"-->")
                {
                    edge.arrow     = true;
                    edge.relation  = FlowchartRelation::Arrow;
                    operatorLength = 3u;
                }
                else if (model->family == NativeDiagramFamily::Class && Text().substr(at, 2u) == L"--")
                {
                    edge.relation  = FlowchartRelation::Line;
                    operatorLength = 2u;
                }
                else if (Text().substr(at, 3u) == L"---")
                    operatorLength = 3u;
                else
                    return Error(FlowchartIssue::UnsupportedEdge, at, end, FlowchartOutcome::Unsupported);
                at += operatorLength;
            }
            // In Mermaid, ---oB/---xB are circle/cross edges, not IDs oB/xB.
            if (at < end && ((! edge.arrow && (Text()[at] == L'o' || Text()[at] == L'x')) || Text()[at] == L'>' || Text()[at] == L'-'))
            {
                return Error(FlowchartIssue::UnsupportedEdge, edgeStart, at + 1u, FlowchartOutcome::Unsupported);
            }
            Skip(at, end);
            if (at < end && Text()[at] == L'|')
            {
                if (model->family != NativeDiagramFamily::Flowchart)
                    return Error(FlowchartIssue::UnsupportedEdge, edgeStart, at + 1u, FlowchartOutcome::Unsupported);
                if (inlineLabel || edge.style == FlowchartLineStyle::Invisible)
                    return Error(FlowchartIssue::UnsupportedEdge, edgeStart, at + 1u, FlowchartOutcome::Unsupported);
                ++at;
                if (! Label(at, end, L'|', edge.label))
                    return false;
                Skip(at, end);
            }
            if (at == end)
                return Error(FlowchartIssue::MissingTarget, edgeStart, end);
            std::vector<std::size_t> nextGroup(0u);
            if (model->family == NativeDiagramFamily::Flowchart)
            {
                if (! NodeGroup(at, end, nextGroup, true))
                    return false;
            }
            else
            {
                std::size_t next{};
                if (! Node(at, end, next, true))
                    return false;
                nextGroup.push_back(next);
            }
            Skip(at, end);
            if (model->family != NativeDiagramFamily::Flowchart && at < end && Text()[at] == L':')
            {
                ++at;
                Skip(at, end);
                if (! PlainText(at, end, edge.label))
                    return false;
                at = end;
            }
            edge.source = {edgeStart, at};
            if (previousGroup.size() > MaximumEdges / nextGroup.size() || previousGroup.size() * nextGroup.size() > MaximumEdges - model->edges.size())
                return Error(FlowchartIssue::EdgeCapacity, edgeStart, at, FlowchartOutcome::Capacity);
            for (const auto from : previousGroup)
                for (const auto to : nextGroup)
                {
                    auto expanded = edge;
                    expanded.from = from;
                    expanded.to   = to;
                    if (reverse)
                        std::swap(expanded.from, expanded.to);
                    model->edges.push_back(std::move(expanded));
                }
            previousGroup = std::move(nextGroup);
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
            if (classBlock)
            {
                Error(FlowchartIssue::UnexpectedToken, classBlockStart, scan);
                return;
            }
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
        if (newline || (! comment && ! quote && ! depth && ! pipe && c == L';'))
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
