#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>

namespace DxUi::RichText::Detail
{
// DxUiNativeFlowchart1 is an owned, deliberately limited Mermaid body grammar.
// This CPU model has no renderer, SDK object, host, worker or source mutation.
enum class FlowchartOutcome
{
    Prepared,
    Malformed,
    Unsupported,
    Capacity
};
enum class FlowchartDirection
{
    TopBottom,
    BottomTop,
    LeftRight,
    RightLeft
};
enum class NativeDiagramFamily
{
    Flowchart,
    State,
    Class,
    Sequence,
    Pie,
    EntityRelationship,
    Gantt,
    Mindmap
};
enum class ErMultiplicity
{
    None,
    One,
    ZeroOrOne,
    OneOrMany,
    ZeroOrMany
};
enum class FlowchartShape
{
    Rectangle,
    Rounded,
    Circle,
    Diamond,
    Stadium,
    Bar,
    Hexagon,
    Subroutine,
    DoubleCircle,
    Cylinder,
    Asymmetric,
    Parallelogram,
    ReverseParallelogram,
    Trapezoid,
    ReverseTrapezoid,
    Ellipse,
    Note
};
enum class FlowchartRelation
{
    Line,
    Arrow,
    Inheritance,
    Realization,
    Note,
    Aggregation,
    Composition
};
enum class FlowchartLineStyle
{
    Solid,
    Dotted,
    Thick,
    Invisible
};
enum FlowchartLabelStyleFlags : unsigned
{
    LabelBold      = 1u,
    LabelItalic    = 2u,
    LabelUnderline = 4u,
    LabelStrike    = 8u
};
struct FlowchartLabelStyle
{
    std::size_t first{}, last{};
    unsigned flags{};
};
struct FlowchartLabelStyles
{
    std::array<FlowchartLabelStyle, 32u> spans{};
    std::size_t count{};
};
inline constexpr std::uint32_t NoDiagramColor = 0x01000000u;
inline bool ValidDiagramColor(std::uint32_t color) noexcept
{
    return color <= NoDiagramColor;
}
inline bool ValidLabelStyles(const FlowchartLabelStyles& styles, std::size_t length, bool admitted) noexcept
{
    if (styles.count > styles.spans.size() || (! admitted && styles.count))
        return false;
    for (std::size_t index = 0u; index < styles.count; ++index)
    {
        const auto& span = styles.spans[index];
        if (span.first >= span.last || span.last > length || ! span.flags || (span.flags & ~(LabelBold | LabelItalic | LabelUnderline | LabelStrike)))
            return false;
    }
    return true;
}
enum class FlowchartIssue
{
    None,
    MissingHeader,
    UnsupportedFamily,
    InvalidDirection,
    UnsupportedConfiguration,
    UnsupportedStatement,
    InvalidIdentifier,
    UnsupportedShape,
    MissingLabelClose,
    InvalidEscape,
    UnsupportedLabelMarkup,
    MissingTarget,
    UnsupportedEdge,
    UnexpectedToken,
    SourceCapacity,
    NodeCapacity,
    EdgeCapacity,
    StatementCapacity,
    LabelCapacity,
    IdentifierCapacity,
    LinkRankCapacity
};
struct FlowchartDiagnostic
{
    FlowchartIssue issue{};
    MarkdownRange source;
};
enum class SequenceNotePlacement : std::uint8_t
{
    None,
    Left,
    Right,
    Over
};
struct FlowchartNode
{
    FlowchartNode() : label(0u, L' '), key(0u, L' '), ganttSection(0u, L' ')
    {
    }
    FlowchartNode(const FlowchartNode&)            = default;
    FlowchartNode& operator=(const FlowchartNode&) = default;
    MarkdownRange identifier, declaration;
    MarkdownRange valueSource;
    MarkdownRange ganttStartSource, ganttEndSource;
    double pieValue{};
    std::int64_t ganttStart{}, ganttEnd{};
    std::size_t mindParent{MarkdownNoNode};
    unsigned mindDepth{};
    std::wstring label;
    std::wstring key;
    std::wstring ganttSection;
    FlowchartLabelStyles styles;
    // Text offsets at which class member compartments begin. Alternating
    // attribute/method groups retain source order and have bounded separators.
    std::array<std::size_t, 16u> compartmentStarts{};
    std::size_t compartmentCount{};
    bool lastCompartmentMethod{};
    std::uint16_t groupMask{};
    std::size_t noteTarget{};
    bool noteLeft{};
    bool sequenceActor{};
    unsigned strokeWidth{1u};
    bool borderDashed{};
    std::uint32_t fillColor{NoDiagramColor}, strokeColor{NoDiagramColor}, textColor{NoDiagramColor};
    FlowchartShape shape{};
};
struct FlowchartGroup
{
    FlowchartGroup() : key(0u, L' '), title(0u, L' ')
    {
    }
    std::wstring key, title;
    MarkdownRange source;
    std::uint16_t parentMask{};
};
struct FlowchartEdge
{
    FlowchartEdge() : label(0u, L' '), fromCardinality(0u, L' '), toCardinality(0u, L' ')
    {
    }
    FlowchartEdge(const FlowchartEdge&)            = default;
    FlowchartEdge& operator=(const FlowchartEdge&) = default;
    std::size_t from{}, to{};
    MarkdownRange source;
    std::wstring label;
    std::wstring fromCardinality, toCardinality;
    FlowchartLabelStyles styles;
    FlowchartRelation relation{};
    FlowchartLineStyle style{};
    std::size_t minimumRanks{1u};
    bool arrow{};
    // Class relationship markers do not reverse the authored layout constraint.
    // For A <|-- B, A precedes B along direction while B inherits A.
    bool markerAtStart{};
    bool openArrow{}; // Sequence asynchronous messages use an open arrowhead.
    SequenceNotePlacement sequenceNote{};
    std::uint16_t sequenceNumber{};
    unsigned strokeWidth{}; // Zero preserves the edge syntax's normal/thick width.
    bool dashed{};
    bool dashSpecified{};
    std::uint32_t strokeColor{NoDiagramColor};
    ErMultiplicity erFrom{}, erTo{};
};
// Preparation-only derived text; the authored label and complete body remain unchanged.
inline std::wstring SequenceMessageText(const FlowchartEdge& edge)
{
    if (! edge.sequenceNumber)
        return std::wstring(edge.label);
    // MSVC Debug's format result uses a noexcept default string constructor.
    // Decimal conversion and the explicit return copy keep allocation fallible.
    auto text = std::to_wstring(edge.sequenceNumber);
    text.append(L". ").append(edge.label);
    return std::wstring(text);
}
struct FlowchartModel
{
    explicit FlowchartModel(std::wstring_view text, std::uint64_t value)
        : source(text),
          revision(value),
          accessibleTitle(0u, L' '),
          accessibleDescription(0u, L' '),
          pieTitle(0u, L' '),
          ganttTitle(0u, L' '),
          nodes(0u),
          edges(0u),
          groups(0u)
    {
    }
    std::wstring source;
    std::uint64_t revision{};
    std::wstring accessibleTitle, accessibleDescription;
    std::wstring pieTitle;
    std::wstring ganttTitle;
    FlowchartDirection direction{};
    NativeDiagramFamily family{};
    FlowchartOutcome outcome{};
    FlowchartDiagnostic diagnostic;
    bool pieShowData{};
    bool sequenceAutonumber{};
    // A rejected result exposes no partial graph. The exact body remains owned.
    std::vector<FlowchartNode> nodes;
    std::vector<FlowchartEdge> edges;
    std::vector<FlowchartGroup> groups;
};
inline bool ValidSequenceModel(const FlowchartModel& graph) noexcept
{
    if (graph.family != NativeDiagramFamily::Sequence)
        return true;
    if (graph.direction != FlowchartDirection::TopBottom || ! graph.groups.empty())
        return false;
    for (const auto& node : graph.nodes)
        if (node.shape != FlowchartShape::Rectangle || node.groupMask || node.compartmentCount || node.label.empty() ||
            node.identifier.first >= node.identifier.last || node.identifier.last > graph.source.size())
            return false;
    std::uint16_t messageNumber{};
    for (const auto& edge : graph.edges)
    {
        if (edge.from >= graph.nodes.size() || edge.to >= graph.nodes.size() ||
            edge.relation != (edge.arrow ? FlowchartRelation::Arrow : FlowchartRelation::Line) ||
            (edge.style != FlowchartLineStyle::Solid && edge.style != FlowchartLineStyle::Dotted) || edge.minimumRanks != 1u || edge.label.empty() ||
            ! edge.fromCardinality.empty() || ! edge.toCardinality.empty() || edge.erFrom != ErMultiplicity::None || edge.erTo != ErMultiplicity::None ||
            edge.source.first >= edge.source.last || edge.source.last > graph.source.size() || edge.sequenceNote > SequenceNotePlacement::Over ||
            (edge.openArrow && ! edge.arrow))
            return false;
        if (edge.sequenceNote != SequenceNotePlacement::None)
        {
            if (edge.arrow || edge.openArrow || edge.sequenceNumber || edge.style != FlowchartLineStyle::Solid ||
                (edge.sequenceNote != SequenceNotePlacement::Over && edge.from != edge.to))
                return false;
        }
        else if (edge.sequenceNumber != (graph.sequenceAutonumber ? ++messageNumber : 0u))
            return false;
    }
    return true;
}
inline bool ValidPieModel(const FlowchartModel& graph) noexcept
{
    if (graph.family != NativeDiagramFamily::Pie)
        return true;
    if (graph.direction != FlowchartDirection::TopBottom || graph.nodes.empty() || graph.nodes.size() > 16u || ! graph.groups.empty() ||
        ! graph.edges.empty() || graph.pieTitle.size() > 1024u)
        return false;
    double total{};
    for (const auto& node : graph.nodes)
    {
        if (node.shape != FlowchartShape::Rectangle || node.groupMask || node.compartmentCount || node.label.empty() ||
            node.identifier.first >= node.identifier.last || node.identifier.last > graph.source.size() || node.valueSource.first >= node.valueSource.last ||
            node.valueSource.last > graph.source.size() || ! std::isfinite(node.pieValue) || node.pieValue < 0.0)
            return false;
        const auto source = std::wstring_view(graph.source).substr(node.valueSource.first, node.valueSource.last - node.valueSource.first);
        double parsed{}, place = .1;
        unsigned integers{}, fractions{};
        bool dot{};
        for (const auto unit : source)
        {
            if (unit == L'.' && ! dot)
            {
                dot = true;
                continue;
            }
            if (unit < L'0' || unit > L'9')
                return false;
            if (dot)
            {
                if (++fractions > 2u)
                    return false;
                parsed += static_cast<double>(unit - L'0') * place;
                place *= .1;
            }
            else
            {
                if (++integers > 15u)
                    return false;
                parsed = parsed * 10.0 + static_cast<double>(unit - L'0');
            }
        }
        if (! integers || (dot && ! fractions) || parsed != node.pieValue)
            return false;
        total += node.pieValue;
    }
    for (std::size_t index = 0u; index < graph.nodes.size(); ++index)
        for (std::size_t other = index + 1u; other < graph.nodes.size(); ++other)
            if (graph.nodes[index].label == graph.nodes[other].label)
                return false;
    return std::isfinite(total) && total > 0.0;
}
inline std::wstring_view ErMultiplicityLabel(ErMultiplicity value) noexcept
{
    switch (value)
    {
        case ErMultiplicity::One: return L"1";
        case ErMultiplicity::ZeroOrOne: return L"0..1";
        case ErMultiplicity::OneOrMany: return L"1..*";
        case ErMultiplicity::ZeroOrMany: return L"0..*";
        case ErMultiplicity::None: return {};
    }
    return {};
}
inline bool ValidErModel(const FlowchartModel& graph) noexcept
{
    if (graph.family != NativeDiagramFamily::EntityRelationship)
        return true;
    if (graph.direction != FlowchartDirection::TopBottom || ! graph.groups.empty())
        return false;
    for (const auto& node : graph.nodes)
        if (node.shape != FlowchartShape::Rectangle || node.groupMask || node.label.empty() || node.identifier.first >= node.identifier.last ||
            node.identifier.last > graph.source.size())
            return false;
    for (const auto& edge : graph.edges)
        if (edge.from >= graph.nodes.size() || edge.to >= graph.nodes.size() || edge.relation != FlowchartRelation::Line || edge.arrow || edge.openArrow ||
            (edge.style != FlowchartLineStyle::Solid && edge.style != FlowchartLineStyle::Dotted) || edge.minimumRanks != 1u || edge.label.empty() ||
            ErMultiplicityLabel(edge.erFrom).empty() || ErMultiplicityLabel(edge.erTo).empty() || edge.fromCardinality != ErMultiplicityLabel(edge.erFrom) ||
            edge.toCardinality != ErMultiplicityLabel(edge.erTo) || edge.source.first >= edge.source.last || edge.source.last > graph.source.size())
            return false;
    return true;
}
inline bool ParseGanttDate(std::wstring_view value, std::int64_t& day) noexcept
{
    if (value.size() != 10u || value[4u] != L'-' || value[7u] != L'-')
        return false;
    const auto digit = [&](std::size_t index) noexcept { return value[index] >= L'0' && value[index] <= L'9'; };
    for (const auto index : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
        if (! digit(index))
            return false;
    const int year       = (value[0] - L'0') * 1000 + (value[1] - L'0') * 100 + (value[2] - L'0') * 10 + value[3] - L'0';
    const unsigned month = static_cast<unsigned>((value[5] - L'0') * 10 + value[6] - L'0');
    const unsigned date  = static_cast<unsigned>((value[8] - L'0') * 10 + value[9] - L'0');
    if (year < 1)
        return false;
    const std::chrono::year_month_day parsed{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{date}};
    if (! parsed.ok())
        return false;
    day = std::chrono::sys_days{parsed}.time_since_epoch().count();
    return true;
}
inline bool ParseGanttSpan(std::wstring_view start, std::wstring_view finish, std::int64_t& first, std::int64_t& last) noexcept
{
    if (! ParseGanttDate(start, first))
        return false;
    if (finish.size() >= 2u && finish.size() <= 4u && finish.back() == L'd')
    {
        unsigned duration{};
        for (const auto unit : finish.substr(0u, finish.size() - 1u))
        {
            if (unit < L'0' || unit > L'9')
                return false;
            duration = duration * 10u + static_cast<unsigned>(unit - L'0');
        }
        if (! duration || duration > 366u)
            return false;
        last = first + duration;
    }
    else if (! ParseGanttDate(finish, last))
        return false;
    return last > first && last - first <= 366;
}
inline bool ValidGanttModel(const FlowchartModel& graph) noexcept
{
    if (graph.family != NativeDiagramFamily::Gantt)
        return true;
    if (graph.direction != FlowchartDirection::TopBottom || graph.nodes.empty() || graph.nodes.size() > 32u || ! graph.groups.empty() ||
        ! graph.edges.empty() || graph.ganttTitle.size() > 1024u)
        return false;
    std::int64_t minimum{}, maximum{};
    bool firstNode = true;
    for (const auto& node : graph.nodes)
    {
        if (node.shape != FlowchartShape::Rectangle || node.groupMask || node.compartmentCount || node.label.empty() || node.ganttSection.size() > 1024u ||
            node.identifier.first >= node.identifier.last || node.identifier.last > graph.source.size() ||
            node.ganttStartSource.first >= node.ganttStartSource.last || node.ganttStartSource.last > graph.source.size() ||
            node.ganttEndSource.first >= node.ganttEndSource.last || node.ganttEndSource.last > graph.source.size())
            return false;
        std::int64_t first{}, last{};
        if (! ParseGanttSpan(std::wstring_view(graph.source).substr(node.ganttStartSource.first, node.ganttStartSource.last - node.ganttStartSource.first),
                             std::wstring_view(graph.source).substr(node.ganttEndSource.first, node.ganttEndSource.last - node.ganttEndSource.first),
                             first,
                             last) ||
            first != node.ganttStart || last != node.ganttEnd)
            return false;
        if (firstNode)
        {
            minimum   = first;
            maximum   = last;
            firstNode = false;
        }
        else
        {
            minimum = std::min(minimum, first);
            maximum = std::max(maximum, last);
        }
    }
    return maximum - minimum <= 366;
}
inline bool ValidMindmapModel(const FlowchartModel& graph) noexcept
{
    if (graph.family != NativeDiagramFamily::Mindmap)
        return true;
    if (graph.direction != FlowchartDirection::TopBottom || graph.nodes.empty() || graph.nodes.size() > 128u || ! graph.groups.empty() ||
        graph.edges.size() + 1u != graph.nodes.size())
        return false;
    for (std::size_t index = 0u; index < graph.nodes.size(); ++index)
    {
        const auto& node = graph.nodes[index];
        if ((node.shape != FlowchartShape::Rectangle && node.shape != FlowchartShape::Rounded && node.shape != FlowchartShape::Circle) || node.groupMask ||
            node.compartmentCount || node.label.empty() || node.identifier.first >= node.identifier.last || node.identifier.last > graph.source.size() ||
            (index == 0u ? node.mindParent != MarkdownNoNode || node.mindDepth != 0u
                         : node.mindParent >= index || node.mindDepth != graph.nodes[node.mindParent].mindDepth + 1u || node.mindDepth > 8u))
            return false;
        if (index)
        {
            const auto& edge = graph.edges[index - 1u];
            if (edge.from != node.mindParent || edge.to != index || edge.relation != FlowchartRelation::Line || edge.arrow || edge.openArrow ||
                edge.style != FlowchartLineStyle::Solid || edge.minimumRanks != 1u || ! edge.label.empty() || ! edge.fromCardinality.empty() ||
                ! edge.toCardinality.empty() || edge.erFrom != ErMultiplicity::None || edge.erTo != ErMultiplicity::None ||
                edge.source.first >= edge.source.last || edge.source.last > graph.source.size())
                return false;
        }
    }
    return true;
}
enum class FlowchartAdmission
{
    Accepted,
    InvalidSource,
    CapacityExceeded
};
class NativeFlowchartParser
{
public:
    static constexpr std::size_t MaximumSourceUnits = 16384u, MaximumNodes = 256u, MaximumEdges = 512u, MaximumStatements = 1024u, MaximumLabelUnits = 1024u,
                                 MaximumLinkRanks = 16u, MaximumIdentifierUnits = 128u;
    NativeFlowchartParser() noexcept;
    ~NativeFlowchartParser() noexcept;
    NativeFlowchartParser(const NativeFlowchartParser&)            = delete;
    NativeFlowchartParser& operator=(const NativeFlowchartParser&) = delete;
    // Invalid UTF-16/oversized Document admission returns its reason and leaves
    // the old request intact. A source-capacity result within Document admission
    // retains bounded exact source but builds no graph.
    FlowchartAdmission Begin(std::wstring_view source, std::uint64_t revision);
    // Scanning resumes per UTF-16 unit. One statement parse is indivisible:
    // at most 16K source units, a 256-node lookup table and 512 admitted edges.
    // Units are not elapsed time.
    // Failed Advance discards staging only; published readers remain immutable.
    MarkdownAdvance Advance(std::size_t maximumWorkUnits = 4096u);
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const FlowchartModel> Published() const noexcept;
    void Cancel() noexcept;
    void Clear() noexcept;

private:
    struct Preparation;
    std::unique_ptr<Preparation> preparation_;
    std::shared_ptr<const FlowchartModel> published_;
};
// Parser publication and both layout/paint services share model-only admission.
// Typography, factories and measurements remain service-specific view contracts.
inline bool ValidFlowchartModel(const FlowchartModel& graph) noexcept
{
    if (graph.outcome != FlowchartOutcome::Prepared || graph.source.size() > NativeFlowchartParser::MaximumSourceUnits ||
        graph.nodes.size() > NativeFlowchartParser::MaximumNodes || graph.edges.size() > NativeFlowchartParser::MaximumEdges || graph.groups.size() > 16u ||
        graph.family < NativeDiagramFamily::Flowchart || graph.family > NativeDiagramFamily::Mindmap || graph.direction < FlowchartDirection::TopBottom ||
        graph.direction > FlowchartDirection::RightLeft || ! ValidSequenceModel(graph) || ! ValidPieModel(graph) || ! ValidErModel(graph) ||
        ! ValidGanttModel(graph) || ! ValidMindmapModel(graph) || (graph.family != NativeDiagramFamily::Sequence && graph.sequenceAutonumber))
        return false;
    if (! graph.groups.empty() && graph.family != NativeDiagramFamily::Flowchart && graph.family != NativeDiagramFamily::State &&
        graph.family != NativeDiagramFamily::Class)
        return false;
    for (std::size_t index = 0u; index < graph.groups.size(); ++index)
        if (graph.groups[index].title.empty() || graph.groups[index].title.size() > NativeFlowchartParser::MaximumLabelUnits ||
            graph.groups[index].parentMask >= (1u << index))
            return false;
    for (const auto& node : graph.nodes)
    {
        if ((graph.family != NativeDiagramFamily::Sequence && node.sequenceActor) || node.strokeWidth < 1u || node.strokeWidth > 3u ||
            ! ValidDiagramColor(node.fillColor) || ! ValidDiagramColor(node.strokeColor) || ! ValidDiagramColor(node.textColor) ||
            ! ValidLabelStyles(node.styles, node.label.size(), graph.family == NativeDiagramFamily::Flowchart) ||
            (graph.family != NativeDiagramFamily::Flowchart && (node.strokeWidth != 1u || node.borderDashed || node.fillColor != NoDiagramColor ||
                                                                node.strokeColor != NoDiagramColor || node.textColor != NoDiagramColor)) ||
            node.label.size() > NativeFlowchartParser::MaximumLabelUnits || node.shape < FlowchartShape::Rectangle || node.shape > FlowchartShape::Note ||
            ((node.shape >= FlowchartShape::DoubleCircle && node.shape <= FlowchartShape::Ellipse) && graph.family != NativeDiagramFamily::Flowchart) ||
            node.groupMask >= (1u << graph.groups.size()) || node.compartmentCount > node.compartmentStarts.size() ||
            (node.compartmentCount && graph.family != NativeDiagramFamily::Class && graph.family != NativeDiagramFamily::EntityRelationship))
            return false;
        if (node.shape == FlowchartShape::Note &&
            ((graph.family != NativeDiagramFamily::State && graph.family != NativeDiagramFamily::Class) ||
             (node.noteTarget == MarkdownNoNode ? graph.family != NativeDiagramFamily::Class
                                                : node.noteTarget >= graph.nodes.size() || graph.nodes[node.noteTarget].shape == FlowchartShape::Note)))
            return false;
        for (std::size_t group = 0u; group < graph.groups.size(); ++group)
            if ((node.groupMask & (1u << group)) && (node.groupMask & graph.groups[group].parentMask) != graph.groups[group].parentMask)
                return false;
        for (std::size_t index = 0u; index < node.compartmentCount; ++index)
            if (! node.compartmentStarts[index] || node.compartmentStarts[index] >= node.label.size() ||
                node.label[node.compartmentStarts[index] - 1u] != L'\n' || (index && node.compartmentStarts[index] <= node.compartmentStarts[index - 1u]))
                return false;
    }
    for (const auto& edge : graph.edges)
        if ((graph.family != NativeDiagramFamily::Sequence && (edge.sequenceNote != SequenceNotePlacement::None || edge.sequenceNumber)) ||
            edge.from >= graph.nodes.size() || edge.to >= graph.nodes.size() || edge.label.size() > NativeFlowchartParser::MaximumLabelUnits ||
            edge.fromCardinality.size() > 32u || edge.toCardinality.size() > 32u ||
            ((! edge.fromCardinality.empty() || ! edge.toCardinality.empty()) && graph.family != NativeDiagramFamily::Class &&
             graph.family != NativeDiagramFamily::EntityRelationship) ||
            ! ValidLabelStyles(edge.styles, edge.label.size(), graph.family == NativeDiagramFamily::Flowchart) || edge.strokeWidth > 3u ||
            ! ValidDiagramColor(edge.strokeColor) ||
            (graph.family != NativeDiagramFamily::Flowchart && (edge.strokeWidth || edge.dashed || edge.dashSpecified || edge.strokeColor != NoDiagramColor)) ||
            (edge.dashed && ! edge.dashSpecified) || edge.minimumRanks == 0u || edge.minimumRanks > NativeFlowchartParser::MaximumLinkRanks ||
            edge.relation < FlowchartRelation::Line || edge.relation > FlowchartRelation::Composition || edge.style < FlowchartLineStyle::Solid ||
            edge.style > FlowchartLineStyle::Invisible || (edge.openArrow && (graph.family != NativeDiagramFamily::Sequence || ! edge.arrow)) ||
            (edge.markerAtStart && (graph.family != NativeDiagramFamily::Class || ! edge.arrow || edge.relation == FlowchartRelation::Arrow)) ||
            (graph.family != NativeDiagramFamily::EntityRelationship && (edge.erFrom != ErMultiplicity::None || edge.erTo != ErMultiplicity::None)) ||
            (edge.style == FlowchartLineStyle::Invisible && (edge.arrow || ! edge.label.empty())) ||
            edge.arrow != (edge.relation != FlowchartRelation::Line && edge.relation != FlowchartRelation::Note) ||
            (edge.relation == FlowchartRelation::Note && (graph.nodes[edge.to].shape != FlowchartShape::Note || graph.nodes[edge.to].noteTarget != edge.from)))
            return false;
    return true;
}
} // namespace DxUi::RichText::Detail
