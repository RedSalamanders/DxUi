#pragma once
#include "DxUi.NativeFlowchart.h"
#include <array>
#include <span>

namespace DxUi::RichText::Detail
{
struct FlowchartSize
{
    double width{}, height{};
};
struct FlowchartPoint
{
    double x{}, y{};
};
struct FlowchartRect
{
    double x{}, y{}, width{}, height{};
};
enum class FlowchartRoute
{
    Forward,
    Back,
    Self
};
enum class FlowchartLayoutIssue
{
    None,
    NodeExtent,
    DiagramExtent
};
struct FlowchartPlacedNode
{
    FlowchartShape shape{};
    std::size_t rank{};
    FlowchartRect bounds, label;
};
struct FlowchartPlacedEdge
{
    FlowchartRoute route{};
    // Six orthogonal vertices include the exact shape attachment points.
    // Crossings are allowed; routing does not promise crossing minimization.
    std::array<FlowchartPoint, 6u> path{};
    std::array<FlowchartPoint, 3u> arrow{};
    FlowchartRect label;
    bool hasArrow{}, hasLabel{};
};
struct FlowchartLayoutPlan
{
    FlowchartLayoutPlan() : nodes(0u), edges(0u)
    {
    }
    FlowchartLayoutIssue issue{};
    FlowchartSize extent;
    // Source-independent geometry: no source strings, graph ownership, device,
    // SDK layout, host, theme brush or raster resource is retained here.
    // Stable vector positions identify the corresponding model label slots.
    // A capacity result publishes no partially placed nodes or edges.
    std::vector<FlowchartPlacedNode> nodes;
    std::vector<FlowchartPlacedEdge> edges;
};
class NativeFlowchartLayout
{
public:
    static constexpr double MaximumExtent = 65536.0;
    NativeFlowchartLayout() noexcept;
    ~NativeFlowchartLayout() noexcept;
    NativeFlowchartLayout(const NativeFlowchartLayout&)            = delete;
    NativeFlowchartLayout& operator=(const NativeFlowchartLayout&) = delete;
    // Labels must already have been measured by the caller. This CPU service
    // creates no SDK layouts and cannot hide label shaping inside one work unit.
    // Invalid graph/metric input or failed admission preserves pending work.
    // Successful Begin replaces pending work; the old publication stays valid.
    void Begin(std::shared_ptr<const FlowchartModel> graph, std::span<const FlowchartSize> nodeLabels, std::span<const FlowchartSize> edgeLabels);
    // An event handles one node, edge, DFS transition or rank. A stable Kahn
    // choice scans at most 256 nodes. Units are not elapsed-time guarantees.
    // Begin copies at most 768 measurements into fixed bounded staging arrays.
    MarkdownAdvance Advance(std::size_t maximumWorkUnits = 256u);
    bool HasPendingPreparation() const noexcept;
    std::shared_ptr<const FlowchartLayoutPlan> Published() const noexcept;
    void Cancel() noexcept;
    void Clear() noexcept;

private:
    struct Preparation;
    std::unique_ptr<Preparation> preparation_;
    std::shared_ptr<const FlowchartLayoutPlan> published_;
};
} // namespace DxUi::RichText::Detail
