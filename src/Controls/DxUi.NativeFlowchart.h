#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <array>

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
    Sequence
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
struct FlowchartNode
{
    FlowchartNode() : label(0u, L' '), key(0u, L' ')
    {
    }
    FlowchartNode(const FlowchartNode&)            = default;
    FlowchartNode& operator=(const FlowchartNode&) = default;
    MarkdownRange identifier, declaration;
    std::wstring label;
    std::wstring key;
    // Text offsets at which class member compartments begin. Alternating
    // attribute/method groups retain source order and have bounded separators.
    std::array<std::size_t, 16u> compartmentStarts{};
    std::size_t compartmentCount{};
    bool lastCompartmentMethod{};
    std::uint16_t groupMask{};
    std::size_t noteTarget{};
    bool noteLeft{};
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
    FlowchartRelation relation{};
    FlowchartLineStyle style{};
    std::size_t minimumRanks{1u};
    bool arrow{};
    bool openArrow{}; // Sequence asynchronous messages use an open arrowhead.
};
struct FlowchartModel
{
    explicit FlowchartModel(std::wstring_view text, std::uint64_t value)
        : source(text),
          revision(value),
          accessibleTitle(0u, L' '),
          accessibleDescription(0u, L' '),
          nodes(0u),
          edges(0u),
          groups(0u)
    {
    }
    std::wstring source;
    std::uint64_t revision{};
    std::wstring accessibleTitle, accessibleDescription;
    FlowchartDirection direction{};
    NativeDiagramFamily family{};
    FlowchartOutcome outcome{};
    FlowchartDiagnostic diagnostic;
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
    for (const auto& edge : graph.edges)
        if (edge.from >= graph.nodes.size() || edge.to >= graph.nodes.size() || edge.relation != FlowchartRelation::Arrow || ! edge.arrow ||
            (edge.style != FlowchartLineStyle::Solid && edge.style != FlowchartLineStyle::Dotted) || edge.minimumRanks != 1u || edge.label.empty() ||
            ! edge.fromCardinality.empty() || ! edge.toCardinality.empty() || edge.source.first >= edge.source.last || edge.source.last > graph.source.size())
            return false;
    return true;
}
class NativeFlowchartParser
{
public:
    static constexpr std::size_t MaximumSourceUnits = 16384u, MaximumNodes = 256u, MaximumEdges = 512u, MaximumStatements = 1024u, MaximumLabelUnits = 1024u,
                                 MaximumLinkRanks = 16u, MaximumIdentifierUnits = 128u;
    NativeFlowchartParser() noexcept;
    ~NativeFlowchartParser() noexcept;
    NativeFlowchartParser(const NativeFlowchartParser&)            = delete;
    NativeFlowchartParser& operator=(const NativeFlowchartParser&) = delete;
    // Invalid UTF-16/oversized Document admission leaves the old request intact.
    // A source-capacity result retains bounded exact source but builds no graph.
    void Begin(std::wstring_view source, std::uint64_t revision);
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
} // namespace DxUi::RichText::Detail
