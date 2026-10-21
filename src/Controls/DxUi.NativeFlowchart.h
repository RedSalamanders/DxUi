#pragma once
#include <DxUi/MarkdownSyntax.h>

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
enum class FlowchartShape
{
    Rectangle,
    Rounded,
    Circle,
    Diamond
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
    IdentifierCapacity
};
struct FlowchartDiagnostic
{
    FlowchartIssue issue{};
    MarkdownRange source;
};
struct FlowchartNode
{
    FlowchartNode() : label(0u, L' ')
    {
    }
    FlowchartNode(const FlowchartNode&)            = default;
    FlowchartNode& operator=(const FlowchartNode&) = default;
    MarkdownRange identifier, declaration;
    std::wstring label;
    FlowchartShape shape{};
};
struct FlowchartEdge
{
    FlowchartEdge() : label(0u, L' ')
    {
    }
    FlowchartEdge(const FlowchartEdge&)            = default;
    FlowchartEdge& operator=(const FlowchartEdge&) = default;
    std::size_t from{}, to{};
    MarkdownRange source;
    std::wstring label;
    bool arrow{};
};
struct FlowchartModel
{
    explicit FlowchartModel(std::wstring_view text, std::uint64_t value) : source(text), revision(value), nodes(0u), edges(0u)
    {
    }
    std::wstring source;
    std::uint64_t revision{};
    FlowchartDirection direction{};
    FlowchartOutcome outcome{};
    FlowchartDiagnostic diagnostic;
    // A rejected result exposes no partial graph. The exact body remains owned.
    std::vector<FlowchartNode> nodes;
    std::vector<FlowchartEdge> edges;
};
class NativeFlowchartParser
{
public:
    static constexpr std::size_t MaximumSourceUnits = 16384u, MaximumNodes = 256u, MaximumEdges = 512u, MaximumStatements = 1024u, MaximumLabelUnits = 1024u,
                                 MaximumIdentifierUnits = 128u;
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
