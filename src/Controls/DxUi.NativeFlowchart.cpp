#include "DxUi.NativeFlowchart.h"
#include "DxUi.MarkdownCharacterReference.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

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
    std::wstring noteBlockText;
    std::size_t scan{}, first{}, depth{}, statements{}, classBlockNode{}, classBlockStart{}, descriptionBlockStart{};
    std::size_t noteBlockStart{}, noteBlockTarget{};
    std::array<bool, MaximumNodes> stateDescriptions{};
    std::array<bool, MaximumNodes> classAnnotations{};
    std::array<std::uint8_t, MaximumNodes> pseudoKinds{};
    std::array<std::size_t, 4u> groupStack{};
    std::size_t groupDepth{};
    bool quote{}, escaped{}, pipe{}, comment{}, header{}, done{}, classBlock{}, titleSeen{}, descriptionSeen{}, descriptionBlock{}, descriptionHasLine{};
    bool noteBlock{}, noteBlockLeft{}, noteBlockHasLine{};
    explicit Preparation(std::wstring_view source, std::uint64_t revision) : model(std::make_shared<FlowchartModel>(source, revision)), noteBlockText(0u, L' ')
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
        model->groups.clear();
        model->accessibleTitle.clear();
        model->accessibleDescription.clear();
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
        const bool html =
            c == L'<' && (model->family != NativeDiagramFamily::Flowchart ||
                          (remaining.size() > 1u && (Letter(remaining[1u]) || remaining[1u] == L'/' || remaining[1u] == L'!' || remaining[1u] == L'?') &&
                           remaining.find(L'>') != std::wstring_view::npos));
        const bool icon = remaining.starts_with(L"fa:fa-") || remaining.starts_with(L"fab:fa-") || remaining.starts_with(L"fas:fa-") ||
                          remaining.starts_with(L"far:fa-") || remaining.starts_with(L"fal:fa-") || remaining.starts_with(L"fad:fa-") ||
                          remaining.starts_with(L"fat:fa-") || remaining.starts_with(L"fass:fa-");
        if (html || (c == L'>' && model->family != NativeDiagramFamily::Flowchart) || (c == L'&' && model->family != NativeDiagramFamily::Flowchart) ||
            c == L'`' || (c == L'#' && model->family != NativeDiagramFamily::Flowchart) || (classMember && c == L'~') || remaining.starts_with(L"$$") || icon ||
            encodedEntity)
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
    bool ClassAnnotation(std::size_t node, std::size_t start, std::size_t finish)
    {
        const auto marker = Text().substr(start, finish - start);
        if (marker.size() < 5u || marker.size() > 36u || ! marker.starts_with(L"<<") || ! marker.ends_with(L">>") || classAnnotations[node])
            return Error(FlowchartIssue::UnsupportedStatement, start, finish, FlowchartOutcome::Unsupported);
        for (const auto c : marker.substr(2u, marker.size() - 4u))
            if (! Letter(c))
                return Error(FlowchartIssue::UnsupportedStatement, start, finish, FlowchartOutcome::Unsupported);
        auto& label             = model->nodes[node].label;
        const auto renderedSize = marker.size() - 2u;
        if (renderedSize + 1u > MaximumLabelUnits - label.size())
            return Error(FlowchartIssue::LabelCapacity, start, finish, FlowchartOutcome::Capacity);
        std::wstring annotated(0u, L' ');
        annotated.reserve(renderedSize + 1u + label.size());
        annotated.push_back(L'«');
        annotated.append(marker.substr(2u, marker.size() - 4u));
        annotated.push_back(L'»');
        annotated.push_back(L'\n');
        annotated.append(label);
        label.swap(annotated);
        for (std::size_t index = 0u; index < model->nodes[node].compartmentCount; ++index)
            model->nodes[node].compartmentStarts[index] += renderedSize + 1u;
        classAnnotations[node] = true;
        return true;
    }
    bool ClassMember(std::size_t nodeIndex, std::size_t at, std::size_t end)
    {
        if (at == end)
            return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
        auto& node          = model->nodes[nodeIndex];
        const bool method   = Text().substr(at, end - at).find(L'(') != std::wstring_view::npos;
        const bool boundary = ! node.compartmentCount || method != node.lastCompartmentMethod;
        if (boundary && node.compartmentCount == node.compartmentStarts.size())
            return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
        if (node.label.size() == MaximumLabelUnits)
            return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
        node.label.push_back(L'\n');
        if (boundary)
            node.compartmentStarts[node.compartmentCount++] = node.label.size();
        node.lastCompartmentMethod = method;
        return PlainText(at, end, node.label, true);
    }
    bool Cardinality(std::size_t& at, std::size_t end, std::wstring& value)
    {
        if (at == end || Text()[at] != L'"')
            return true;
        const auto start = at++;
        while (at < end && Text()[at] != L'"')
        {
            const auto c = Text()[at];
            if (! (c >= L'0' && c <= L'9') && c != L'*' && c != L'.' && c != L'+' && c != L'-' && c != L'?' && c != L' ')
                return Error(FlowchartIssue::UnsupportedLabelMarkup, at, at + 1u, FlowchartOutcome::Unsupported);
            if (value.size() == 32u)
                return Error(FlowchartIssue::LabelCapacity, start, at + 1u, FlowchartOutcome::Capacity);
            value.push_back(c);
            ++at;
        }
        if (at == end || value.empty())
            return Error(FlowchartIssue::MissingLabelClose, start, at);
        ++at;
        Skip(at, end);
        return true;
    }
    bool CloseGroup(std::size_t at, std::size_t end)
    {
        if (! groupDepth)
            return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
        const auto groupIndex = groupStack[groupDepth - 1u];
        bool hasNode{};
        for (const auto& node : model->nodes)
            hasNode = hasNode || (node.groupMask & static_cast<std::uint16_t>(1u << groupIndex));
        if (! hasNode)
            return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
        model->groups[groupIndex].source.last = end;
        --groupDepth;
        return true;
    }
    bool AddStateNote(std::size_t target, bool left, std::size_t start, std::size_t end, std::wstring_view text)
    {
        if (text.empty())
            return Error(FlowchartIssue::UnsupportedStatement, start, end, FlowchartOutcome::Unsupported);
        if (model->nodes.size() == MaximumNodes)
            return Error(FlowchartIssue::NodeCapacity, start, end, FlowchartOutcome::Capacity);
        if (model->edges.size() == MaximumEdges)
            return Error(FlowchartIssue::EdgeCapacity, start, end, FlowchartOutcome::Capacity);
        FlowchartNode note;
        note.key.assign(L"@note");
        note.key.append(std::to_wstring(model->nodes.size()));
        note.identifier  = {start, start + 4u};
        note.declaration = {start, end};
        note.shape       = FlowchartShape::Note;
        note.noteTarget  = target;
        note.noteLeft    = left;
        note.groupMask   = model->nodes[target].groupMask;
        note.label.assign(text);
        const auto noteIndex = model->nodes.size();
        model->nodes.push_back(note);
        FlowchartEdge edge;
        edge.from     = target;
        edge.to       = noteIndex;
        edge.source   = {start, end};
        edge.relation = FlowchartRelation::Note;
        edge.style    = FlowchartLineStyle::Dotted;
        model->edges.push_back(edge);
        return true;
    }
    bool NoteLine(std::size_t at, std::size_t end)
    {
        if (noteBlockHasLine)
        {
            if (noteBlockText.size() == MaximumLabelUnits)
                return Error(FlowchartIssue::LabelCapacity, at, end, FlowchartOutcome::Capacity);
            noteBlockText.push_back(L'\n');
        }
        noteBlockHasLine = true;
        return PlainText(at, end, noteBlockText);
    }
    bool OpenGroup(std::wstring_view key, std::wstring_view title, std::size_t keyStart, std::size_t start, std::size_t end)
    {
        if (groupDepth == groupStack.size() || model->groups.size() == 16u)
            return Error(FlowchartIssue::NodeCapacity, start, end, FlowchartOutcome::Capacity);
        for (const auto& group : model->groups)
            if (group.key == key)
                return Error(FlowchartIssue::UnsupportedStatement, keyStart, keyStart + key.size(), FlowchartOutcome::Unsupported);
        for (const auto& node : model->nodes)
            if (node.key == key)
                return Error(FlowchartIssue::UnsupportedStatement, keyStart, keyStart + key.size(), FlowchartOutcome::Unsupported);
        FlowchartGroup group;
        group.key.assign(key);
        group.title.assign(title);
        group.source.first = start;
        for (std::size_t depthIndex = 0u; depthIndex < groupDepth; ++depthIndex)
            group.parentMask |= static_cast<std::uint16_t>(1u << groupStack[depthIndex]);
        groupStack[groupDepth++] = model->groups.size();
        model->groups.push_back(group);
        return true;
    }
    bool DescriptionLine(std::size_t start, std::size_t end)
    {
        auto& value       = model->accessibleDescription;
        const auto length = end - start;
        if (length + (descriptionHasLine ? 1u : 0u) > MaximumLabelUnits - value.size())
            return Error(FlowchartIssue::LabelCapacity, start, end, FlowchartOutcome::Capacity);
        for (std::size_t position = start; position < end; ++position)
            if (Text()[position] < L' ' && Text()[position] != L'\t')
                return Error(FlowchartIssue::UnsupportedStatement, position, position + 1u, FlowchartOutcome::Unsupported);
        if (descriptionHasLine)
            value.push_back(L'\n');
        value.append(Text().substr(start, length));
        descriptionHasLine = true;
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
                while (finish < end && finish - at < 33u && (Letter(Text()[finish]) || (Text()[finish] >= L'0' && Text()[finish] <= L'9')))
                    ++finish;
                if (finish == at + 1u || finish == end || Text()[finish] != L';')
                {
                    if (value.size() == MaximumLabelUnits)
                        return Error(FlowchartIssue::LabelCapacity, at, at + 1u, FlowchartOutcome::Capacity);
                    value.push_back(L'#');
                    ++at;
                    continue;
                }
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
        for (const auto& group : model->groups)
            if (group.key == id)
                return Error(FlowchartIssue::UnsupportedStatement, start, idEnd, FlowchartOutcome::Unsupported);
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
        if (terminal && groupDepth)
        {
            candidate.key.push_back(L'#');
            for (std::size_t depthIndex = 0u; depthIndex < groupDepth; ++depthIndex)
                candidate.key.push_back(static_cast<wchar_t>(L'A' + groupStack[depthIndex]));
        }
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
            const bool circle     = ! stadium && open == L'(' && at < end && Text()[at] == L'(';
            const bool hexagon    = open == L'{' && at < end && Text()[at] == L'{';
            const bool subroutine = open == L'[' && at < end && Text()[at] == L'[';
            if (circle)
            {
                ++at;
                candidate.shape = FlowchartShape::Circle;
            }
            if (hexagon)
            {
                ++at;
                candidate.shape = FlowchartShape::Hexagon;
            }
            if (subroutine)
            {
                ++at;
                candidate.shape = FlowchartShape::Subroutine;
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
            if (hexagon && (at == end || Text()[at++] != L'}'))
                return Error(FlowchartIssue::MissingLabelClose, start, at);
            if (subroutine && (at == end || Text()[at++] != L']'))
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
        std::uint16_t activeGroups{};
        for (std::size_t depthIndex = 0u; depthIndex < groupDepth; ++depthIndex)
            activeGroups |= static_cast<std::uint16_t>(1u << groupStack[depthIndex]);
        auto& membership = model->nodes[index].groupMask;
        if (activeGroups && membership && (activeGroups & membership) != activeGroups && (activeGroups & membership) != membership)
            return Error(FlowchartIssue::UnsupportedStatement, start, idEnd, FlowchartOutcome::Unsupported);
        membership |= activeGroups;
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
        if (at == end && noteBlock)
            return NoteLine(at, end);
        if (at == end && descriptionBlock)
            return DescriptionLine(at, end);
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
        if (descriptionBlock)
        {
            if (Text().substr(at, end - at) == L"}")
            {
                descriptionBlock = false;
                return true;
            }
            return DescriptionLine(at, end);
        }
        const auto statement = Text().substr(at, end - at);
        if (noteBlock)
        {
            if (statement != L"end note")
                return NoteLine(at, end);
            noteBlock = false;
            return AddStateNote(noteBlockTarget, noteBlockLeft, noteBlockStart, end, noteBlockText);
        }
        if (model->family == NativeDiagramFamily::Flowchart && statement == L"end")
            return CloseGroup(at, end);
        if (model->family == NativeDiagramFamily::State && statement == L"}")
            return CloseGroup(at, end);
        if (model->family == NativeDiagramFamily::Flowchart && statement.starts_with(L"subgraph") && statement.size() > 8u && Space(statement[8u]))
        {
            if (groupDepth == groupStack.size() || model->groups.size() == 16u)
                return Error(FlowchartIssue::NodeCapacity, at, end, FlowchartOutcome::Capacity);
            const auto groupStart = at;
            at += 8u;
            Skip(at, end);
            const auto keyStart = at;
            if (at == end || ! Letter(Text()[at]))
                return Error(FlowchartIssue::InvalidIdentifier, at, end);
            while (at < end && (Letter(Text()[at]) || (Text()[at] >= L'0' && Text()[at] <= L'9') || Text()[at] == L'_'))
                ++at;
            if (at - keyStart > MaximumIdentifierUnits)
                return Error(FlowchartIssue::IdentifierCapacity, keyStart, at, FlowchartOutcome::Capacity);
            const auto key = Text().substr(keyStart, at - keyStart);
            std::wstring title(0u, L' ');
            Skip(at, end);
            if (at < end && Text()[at] == L'[')
            {
                ++at;
                if (! Label(at, end, L']', title))
                    return false;
                Skip(at, end);
            }
            else
                title.assign(key);
            if (at != end || title.empty())
                return Error(FlowchartIssue::UnsupportedStatement, groupStart, end, FlowchartOutcome::Unsupported);
            return OpenGroup(key, title, keyStart, groupStart, end);
        }
        if (! classBlock && (statement == L"accDescr {" || statement == L"accDescr{"))
        {
            if (descriptionSeen)
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            descriptionSeen       = true;
            descriptionBlock      = true;
            descriptionBlockStart = at;
            return true;
        }
        for (const auto& [prefix, title] : {std::pair{std::wstring_view(L"accTitle:"), true}, std::pair{std::wstring_view(L"accDescr:"), false}})
            if (! classBlock && statement.starts_with(prefix))
            {
                auto& seen = title ? titleSeen : descriptionSeen;
                if (seen)
                    return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
                auto valueStart = at + prefix.size();
                Skip(valueStart, end);
                if (valueStart == end)
                    return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
                if (end - valueStart > MaximumLabelUnits)
                    return Error(FlowchartIssue::LabelCapacity, valueStart, end, FlowchartOutcome::Capacity);
                auto& value = title ? model->accessibleTitle : model->accessibleDescription;
                for (auto position = valueStart; position < end; ++position)
                    if (Text()[position] < L' ' && Text()[position] != L'\t')
                        return Error(FlowchartIssue::UnsupportedStatement, position, position + 1u, FlowchartOutcome::Unsupported);
                value.assign(Text().substr(valueStart, end - valueStart));
                seen = true;
                return true;
            }
        if (classBlock)
        {
            if (Text().substr(at, end - at) == L"}")
            {
                classBlock = false;
                return true;
            }
            if (Text().substr(at, 2u) == L"<<")
                return ClassAnnotation(classBlockNode, at, end);
            const auto memberStart = at;
            if (! Letter(Text()[at]) && Text()[at] != L'+' && Text()[at] != L'-')
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            const auto member = Text().substr(at, end - at);
            if (member.find_first_of(L"{}\";") != std::wstring_view::npos || member.starts_with(L"class ") || member.starts_with(L"direction ") ||
                member.starts_with(L"click ") || member.starts_with(L"style "))
                return Error(FlowchartIssue::UnsupportedStatement, at, end, FlowchartOutcome::Unsupported);
            return ClassMember(classBlockNode, memberStart, end);
        }
        if (model->family == NativeDiagramFamily::Class && Text().substr(at, 2u) == L"<<")
        {
            const auto start = at;
            const auto close = Text().find(L">>", at + 2u);
            if (close == std::wstring_view::npos || close + 2u >= end || ! Space(Text()[close + 2u]))
                return Error(FlowchartIssue::UnsupportedStatement, start, end, FlowchartOutcome::Unsupported);
            at = close + 2u;
            Skip(at, end);
            const auto before = model->nodes.size();
            std::size_t node{};
            if (! Node(at, end, node))
                return false;
            Skip(at, end);
            if (at != end || node == before)
                return Error(FlowchartIssue::UnsupportedStatement, start, end, FlowchartOutcome::Unsupported);
            return ClassAnnotation(node, start, close + 2u);
        }
        if (model->family == NativeDiagramFamily::Class && statement.starts_with(L"note "))
        {
            const auto start = at;
            at += 5u;
            std::size_t target = MarkdownNoNode;
            if (Text().substr(at, 4u) == L"for ")
            {
                at += 4u;
                if (! Node(at, end, target)) return false;
                Skip(at, end);
            }
            if (at == end || Text()[at++] != L'"')
                return Error(FlowchartIssue::UnsupportedStatement, start, end, FlowchartOutcome::Unsupported);
            const auto labelStart = at;
            while (at < end && Text()[at] != L'"')
            {
                if (Text()[at] == L'\\' && at + 1u < end) ++at;
                ++at;
            }
            if (at == end) return Error(FlowchartIssue::MissingLabelClose, labelStart, at);
            std::wstring text(0u, L' ');
            auto textAt = labelStart;
            if (! PlainText(textAt, at, text, false, true)) return false;
            ++at;
            Skip(at, end);
            if (at != end || text.empty()) return Error(FlowchartIssue::UnsupportedStatement, start, end, FlowchartOutcome::Unsupported);
            if (target != MarkdownNoNode) return AddStateNote(target, true, start, end, text);
            if (model->nodes.size() == MaximumNodes) return Error(FlowchartIssue::NodeCapacity, start, end, FlowchartOutcome::Capacity);
            FlowchartNode note;
            note.key.assign(L"@note");
            note.key.append(std::to_wstring(model->nodes.size()));
            note.identifier = {start, start + 4u};
            note.declaration = {start, end};
            note.shape = FlowchartShape::Note;
            note.noteTarget = MarkdownNoNode;
            note.label.swap(text);
            model->nodes.push_back(std::move(note));
            return true;
        }
        if (model->family == NativeDiagramFamily::State && statement.starts_with(L"state") && statement.size() > 5u && Space(statement[5u]) &&
            statement.ends_with(L"{"))
        {
            const auto groupStart = at;
            at += 5u;
            Skip(at, end);
            std::wstring title(0u, L' ');
            if (at < end && Text()[at] == L'"')
            {
                const auto firstTitle = ++at;
                while (at < end && Text()[at] != L'"')
                {
                    if (Text()[at] == L'\\' && at + 1u < end)
                        ++at;
                    ++at;
                }
                if (at == end)
                    return Error(FlowchartIssue::MissingLabelClose, firstTitle, at);
                auto titleCursor = firstTitle;
                if (! PlainText(titleCursor, at, title, false, true))
                    return false;
                ++at;
                Skip(at, end);
                if (Text().substr(at, 2u) != L"as" || at + 2u >= end || ! Space(Text()[at + 2u]))
                    return Error(FlowchartIssue::UnexpectedToken, at, end);
                at += 2u;
                Skip(at, end);
            }
            const auto keyStart = at;
            if (at == end || ! Letter(Text()[at]))
                return Error(FlowchartIssue::InvalidIdentifier, at, end);
            while (at < end && IdUnit(Text()[at]))
                ++at;
            if (at - keyStart > MaximumIdentifierUnits)
                return Error(FlowchartIssue::IdentifierCapacity, keyStart, at, FlowchartOutcome::Capacity);
            const auto key = Text().substr(keyStart, at - keyStart);
            if (title.empty())
                title.assign(key);
            Skip(at, end);
            if (at + 1u != end || Text()[at] != L'{')
                return Error(FlowchartIssue::UnsupportedStatement, groupStart, end, FlowchartOutcome::Unsupported);
            return OpenGroup(key, title, keyStart, groupStart, end);
        }
        if (model->family == NativeDiagramFamily::State && (statement.starts_with(L"note right of ") || statement.starts_with(L"note left of ")))
        {
            const auto noteStart = at;
            const bool left      = statement.starts_with(L"note left of ");
            at += left ? 13u : 14u;
            std::size_t target{};
            if (! Node(at, end, target))
                return false;
            Skip(at, end);
            if (at == end)
            {
                noteBlock        = true;
                noteBlockLeft    = left;
                noteBlockStart   = noteStart;
                noteBlockTarget  = target;
                noteBlockHasLine = false;
                noteBlockText.clear();
                return true;
            }
            if (Text()[at++] != L':')
                return Error(FlowchartIssue::UnsupportedStatement, noteStart, end, FlowchartOutcome::Unsupported);
            Skip(at, end);
            if (at == end)
                return Error(FlowchartIssue::UnsupportedStatement, noteStart, end, FlowchartOutcome::Unsupported);
            std::wstring text(0u, L' ');
            if (! PlainText(at, end, text))
                return false;
            return AddStateNote(target, left, noteStart, end, text);
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
                return ClassMember(previousGroup.front(), at, end);
            }
            FlowchartEdge edge;
            const auto edgeStart = at;
            if (model->family == NativeDiagramFamily::Class && ! Cardinality(at, end, edge.fromCardinality))
                return false;
            bool reverse{};
            const auto relationText = Text().substr(at, end - at);
            bool inlineLabel        = false;
            const auto readEnd      = [&](std::size_t start, FlowchartLineStyle style, std::size_t& finish, std::size_t& ranks, bool& arrow)
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
            else if (model->family == NativeDiagramFamily::Class &&
                     (relationText.starts_with(L"*--") || relationText.starts_with(L"o--") || relationText.starts_with(L"--*") ||
                      relationText.starts_with(L"--o") || relationText.starts_with(L"*..") || relationText.starts_with(L"o..") ||
                      relationText.starts_with(L"..*") || relationText.starts_with(L"..o")))
            {
                reverse           = relationText[0] == L'*' || relationText[0] == L'o';
                const auto marker = relationText[reverse ? 0u : 2u];
                edge.arrow        = true;
                edge.relation     = marker == L'*' ? FlowchartRelation::Composition : FlowchartRelation::Aggregation;
                edge.style        = relationText[1u] == L'.' ? FlowchartLineStyle::Dotted : FlowchartLineStyle::Solid;
                at += 3u;
            }
            else if (model->family == NativeDiagramFamily::Class && (Text().substr(at, 4u) == L"<|--" || Text().substr(at, 4u) == L"--|>" ||
                                                                     Text().substr(at, 4u) == L"<|.." || Text().substr(at, 4u) == L"..|>"))
            {
                reverse       = Text()[at] == L'<';
                edge.arrow    = true;
                edge.style    = Text()[at + (reverse ? 2u : 0u)] == L'.' ? FlowchartLineStyle::Dotted : FlowchartLineStyle::Solid;
                edge.relation = edge.style == FlowchartLineStyle::Dotted ? FlowchartRelation::Realization : FlowchartRelation::Inheritance;
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
                else if (model->family == NativeDiagramFamily::Class && Text().substr(at, 2u) == L"..")
                {
                    edge.relation  = FlowchartRelation::Line;
                    edge.style     = FlowchartLineStyle::Dotted;
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
            if (model->family == NativeDiagramFamily::Class && ! Cardinality(at, end, edge.toCardinality))
                return false;
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
                    {
                        std::swap(expanded.from, expanded.to);
                        expanded.fromCardinality.swap(expanded.toCardinality);
                    }
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
            if (classBlock || descriptionBlock || noteBlock || groupDepth)
            {
                const auto start = noteBlock          ? noteBlockStart
                                   : descriptionBlock ? descriptionBlockStart
                                   : classBlock       ? classBlockStart
                                                      : model->groups[groupStack[groupDepth - 1u]].source.first;
                Error(FlowchartIssue::UnexpectedToken, start, scan);
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
        if (! descriptionBlock && ! noteBlock && ! comment && ! quote && ! depth && ! pipe && c == L'%' && scan + 1u < text.size() && text[scan + 1u] == L'%')
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
        if (newline || (! descriptionBlock && ! noteBlock && ! comment && ! quote && ! depth && ! pipe && c == L';'))
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
