#include "DxUi.NativeFlowchartLayout.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace DxUi::RichText::Detail
{
namespace
{
constexpr auto NoIndex  = MarkdownNoNode;
constexpr double Margin = 8.0, NodeGap = 24.0, RankGap = 24.0, PortGap = 16.0;
bool Valid(FlowchartSize value) noexcept
{
    return std::isfinite(value.width) && std::isfinite(value.height) && value.width >= 0.0 && value.height >= 0.0 &&
           value.width <= NativeFlowchartLayout::MaximumExtent && value.height <= NativeFlowchartLayout::MaximumExtent;
}
void Shift(FlowchartPoint& point, FlowchartPoint delta) noexcept
{
    point.x += delta.x;
    point.y += delta.y;
}
void Shift(FlowchartRect& rect, FlowchartPoint delta) noexcept
{
    rect.x += delta.x;
    rect.y += delta.y;
}
} // namespace
struct NativeFlowchartLayout::Preparation
{
    Preparation(const Preparation&)            = delete;
    Preparation& operator=(const Preparation&) = delete;
    Preparation(Preparation&&)                 = delete;
    Preparation& operator=(Preparation&&)      = delete;
    static constexpr auto Nodes = NativeFlowchartParser::MaximumNodes, Edges = NativeFlowchartParser::MaximumEdges;
    static constexpr auto RankWords = (Nodes + 63u) / 64u;
    enum class Stage
    {
        Measure,
        SequenceNodes,
        SequenceEdges,
        PieNodes,
        GanttNodes,
        MindNodes,
        MindEdges,
        Adjacency,
        Dfs,
        Indegree,
        ChooseRank,
        RankEdges,
        OrderInitialize,
        OrderAccumulate,
        OrderSort,
        OrderPosition,
        OrderCompare,
        Ports,
        Rows,
        RowPositions,
        NodesPosition,
        NotePositions,
        NoteCollision,
        GroupPositions,
        Routes,
        RouteCompare,
        RestoreNodes,
        TransformNodes,
        TransformGroups,
        TransformEdges,
        CardinalityCheck,
        NormalizeNodes,
        NormalizeGroups,
        NormalizeEdges,
        Finish
    };
    struct DfsFrame
    {
        std::size_t node{}, edge{};
    };
    std::shared_ptr<const FlowchartModel> graph;
    std::shared_ptr<FlowchartLayoutPlan> plan;
    std::array<FlowchartSize, Nodes> nodeLabels{};
    std::array<FlowchartSize, Edges> edgeLabels{};
    std::array<FlowchartSize, Edges> fromCardinalities{}, toCardinalities{};
    std::array<FlowchartSize, 16u> groupLabels{};
    std::array<std::size_t, Nodes> head{}, tail{}, rank{}, indegree{}, before{}, after{}, color{}, incoming{}, outgoing{}, rowCount{};
    std::array<std::size_t, Nodes> pairTargets{};
    // Candidate ordering is private to staging; model identities never move.
    std::array<std::size_t, Nodes> orderedNodes{}, orderPosition{}, originalPosition{}, neighborCount{};
    std::array<double, Nodes> neighborSum{}, originalX{};
    std::array<std::array<FlowchartPoint, 6u>, Edges> originalPaths{};
    std::size_t originalRouteCrossings{}, candidateRouteCrossings{};
    bool restoringRoutes{};
    std::size_t orderSweep{}, originalCrossings{}, candidateCrossings{};
    bool useOrdering{};
    std::array<std::size_t, Nodes> groupColumns{};
    std::array<int, Nodes> mindSide{};
    std::array<double, 16u> groupColumnWidth{};
    std::array<bool, Nodes> ranked{};
    std::array<std::size_t, Edges> next{}, startPort{}, endPort{};
    struct AttachmentSide
    {
        double width{}, labelWidth{};
        std::size_t count{};
    };
    struct RelationshipAttachments
    {
        std::array<std::array<AttachmentSide, 2u>, Nodes> sides{};
        std::array<double, Edges> from{}, to{};
    };
    // Ordinary flowcharts allocate no relationship-only staging. This bounded
    // state is retired with preparation, never retained by published geometry.
    std::unique_ptr<RelationshipAttachments> relationshipAttachments;
    std::array<double, Edges> laneOffset{}, laneWidth{};
    std::array<bool, Edges> direct{};
    std::array<std::array<std::uint64_t, RankWords>, Edges> laneRanks{};
    std::array<double, Edges> laneSlotOffset{};
    std::array<bool, Edges> laneSlotBack{}, laneSlotReusable{};
    std::array<DfsFrame, Nodes> stack{};
    std::array<double, Nodes> rowWidth{}, rowHeight{}, rowTop{}, rowCursor{}, rowLabelGap{};
    std::size_t cursor{}, roots{}, depth{}, rankedCount{}, active{}, activeEdge{}, maximumRank{}, laneCount{};
    double bandWidth{}, bandLeft{}, rowY{Margin}, leftWidth{}, rightWidth{}, nodeGap{NodeGap};
    double minX{}, minY{}, maxX{}, maxY{};
    double sequenceX{Margin}, sequenceY{}, sequenceWidth{}, sequenceHeader{}, sequenceGap{};
    bool sequenceActors{};
    double pieTotal{}, pieAngle{}, pieLegendY{}, pieWidth{}, pieDiameter{};
    double ganttY{}, ganttLabelWidth{}, ganttScale{};
    std::int64_t ganttMinimum{}, ganttMaximum{};
    double mindWidth{}, mindHeight{}, mindColumn{}, mindRow{}, mindCenterX{}, mindCenterY{};
    FlowchartPoint shift;
    Stage stage{};
    bool horizontal{}, bounded{}, done{}, hasCardinality{}, hasNotes{}, alignedPairs{}, alignedGroups{};

    Preparation(std::shared_ptr<const FlowchartModel> value,
                std::span<const FlowchartSize> nodes,
                std::span<const FlowchartSize> edges,
                std::span<const FlowchartSize> from,
                std::span<const FlowchartSize> to,
                std::span<const FlowchartSize> groups)
        : graph(std::move(value)),
          plan(std::make_shared<FlowchartLayoutPlan>())
    {
        std::copy(nodes.begin(), nodes.end(), nodeLabels.begin());
        std::copy(edges.begin(), edges.end(), edgeLabels.begin());
        std::copy(from.begin(), from.end(), fromCardinalities.begin());
        std::copy(to.begin(), to.end(), toCardinalities.begin());
        std::copy(groups.begin(), groups.end(), groupLabels.begin());
        head.fill(NoIndex);
        tail.fill(NoIndex);
        next.fill(NoIndex);
        plan->nodes.reserve(nodes.size());
        plan->edges.reserve(edges.size());
        plan->groups.resize(graph->groups.size());
        for (const auto& edge : graph->edges)
            hasCardinality = hasCardinality || ! edge.fromCardinality.empty() || ! edge.toCardinality.empty();
        if (hasCardinality)
            relationshipAttachments = std::make_unique<RelationshipAttachments>();
        for (const auto& node : graph->nodes)
            hasNotes = hasNotes || node.shape == FlowchartShape::Note;
        horizontal = graph->direction == FlowchartDirection::LeftRight || graph->direction == FlowchartDirection::RightLeft;
    }
    void Fail(FlowchartLayoutIssue issue) noexcept
    {
        plan->issue  = issue;
        plan->extent = {};
        plan->nodes.clear();
        plan->edges.clear();
        plan->groups.clear();
        done = true;
    }
    void Then(Stage value) noexcept
    {
        stage  = value;
        cursor = 0u;
    }
    double Cross(FlowchartSize size) const noexcept
    {
        return horizontal ? size.height : size.width;
    }
    double Major(FlowchartSize size) const noexcept
    {
        return horizontal ? size.width : size.height;
    }
    FlowchartPoint Transform(FlowchartPoint p) const noexcept
    {
        switch (graph->direction)
        {
            case FlowchartDirection::BottomTop: return {p.x, -p.y};
            case FlowchartDirection::LeftRight: return {p.y, p.x};
            case FlowchartDirection::RightLeft: return {-p.y, p.x};
            case ::DxUi::RichText::Detail::FlowchartDirection::TopBottom:
            default: return p;
        }
    }
    FlowchartRect Transform(FlowchartRect rect) const noexcept
    {
        const auto first = Transform(FlowchartPoint{rect.x, rect.y});
        const auto last  = Transform(FlowchartPoint{rect.x + rect.width, rect.y + rect.height});
        return {std::min(first.x, last.x), std::min(first.y, last.y), std::abs(last.x - first.x), std::abs(last.y - first.y)};
    }
    void Include(FlowchartPoint p) noexcept
    {
        if (! bounded)
        {
            minX = maxX = p.x;
            minY = maxY = p.y;
            bounded     = true;
        }
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    void Include(FlowchartRect r) noexcept
    {
        Include(FlowchartPoint{r.x, r.y});
        Include(FlowchartPoint{r.x + r.width, r.y + r.height});
    }
    void Measure()
    {
        if (cursor == graph->nodes.size())
        {
            if (graph->family == NativeDiagramFamily::Mindmap)
            {
                double maximumWidth{}, maximumHeight{};
                for (const auto& node : plan->nodes)
                {
                    maximumWidth  = std::max(maximumWidth, node.bounds.width);
                    maximumHeight = std::max(maximumHeight, node.bounds.height);
                }
                unsigned leftDepth{}, rightDepth{}, branches{};
                for (std::size_t index = 1u; index < graph->nodes.size(); ++index)
                {
                    const auto& node = graph->nodes[index];
                    mindSide[index]  = node.mindDepth == 1u ? (++branches % 2u ? 1 : -1) : mindSide[node.mindParent];
                    if (mindSide[index] < 0)
                        leftDepth = std::max(leftDepth, node.mindDepth);
                    else
                        rightDepth = std::max(rightDepth, node.mindDepth);
                }
                mindColumn  = maximumWidth + 32.0;
                mindRow     = maximumHeight + 16.0;
                mindCenterX = Margin + static_cast<double>(leftDepth) * mindColumn + maximumWidth / 2.0;
                mindCenterY = Margin + maximumHeight / 2.0 + (graph->nodes.size() > 2u ? static_cast<double>(graph->nodes.size() - 2u) * mindRow / 2.0 : 0.0);
                mindWidth   = 2.0 * Margin + static_cast<double>(leftDepth + rightDepth) * mindColumn + maximumWidth;
                mindHeight  = 2.0 * Margin + maximumHeight + (graph->nodes.size() > 1u ? static_cast<double>(graph->nodes.size() - 2u) * mindRow : 0.0);
                if (mindWidth > MaximumExtent || mindHeight > MaximumExtent)
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
                Then(Stage::MindNodes);
            }
            else if (graph->family == NativeDiagramFamily::Gantt)
            {
                ganttMinimum = graph->nodes.front().ganttStart;
                ganttMaximum = graph->nodes.front().ganttEnd;
                for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                {
                    ganttMinimum    = std::min(ganttMinimum, graph->nodes[index].ganttStart);
                    ganttMaximum    = std::max(ganttMaximum, graph->nodes[index].ganttEnd);
                    ganttLabelWidth = std::max(ganttLabelWidth, nodeLabels[index].width);
                }
                const auto title   = groupLabels[0u];
                plan->ganttTitle   = {Margin, Margin, title.width, title.height};
                const double axisX = Margin + ganttLabelWidth + 24.0;
                const double axisY = Margin + title.height + (title.height ? 12.0 : 0.0) + 8.0;
                plan->ganttAxis    = {axisX, axisY, 320.0, 0.0};
                ganttScale         = 320.0 / static_cast<double>(ganttMaximum - ganttMinimum);
                ganttY             = axisY + 12.0;
                Then(Stage::GanttNodes);
            }
            else if (graph->family == NativeDiagramFamily::Pie)
            {
                for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                    pieTotal += graph->nodes[index].pieValue;
                if (pieTotal <= 0.0 || ! std::isfinite(pieTotal))
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
                pieDiameter = 144.0;
                for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                    pieDiameter = std::max(pieDiameter, nodeLabels[index].height * 8.0);
                const auto title = groupLabels[0u];
                plan->pieTitle   = {Margin, Margin, title.width, title.height};
                plan->pieCircle  = {Margin, Margin + title.height + (title.height ? 12.0 : 0.0), pieDiameter, pieDiameter};
                pieLegendY       = plan->pieCircle.y;
                pieWidth         = std::max(Margin + title.width, Margin + pieDiameter);
                Then(Stage::PieNodes);
            }
            else if (graph->family == NativeDiagramFamily::Sequence)
            {
                sequenceGap = 48.0;
                for (std::size_t edge = 0u; edge < graph->edges.size(); ++edge)
                {
                    const auto placement = graph->edges[edge].sequenceNote;
                    sequenceGap          = std::max(sequenceGap, edgeLabels[edge].width + (placement == SequenceNotePlacement::None ? 24.0 : 48.0));
                    if (placement == SequenceNotePlacement::Left || placement == SequenceNotePlacement::Over)
                        sequenceX = std::max(sequenceX, Margin + edgeLabels[edge].width + 32.0);
                }
                sequenceActors = std::ranges::any_of(graph->nodes, [](const auto& node) { return node.sequenceActor; });
                for (const auto& node : plan->nodes)
                    sequenceHeader = std::max(sequenceHeader, node.bounds.height);
                Then(Stage::SequenceNodes);
            }
            else
                Then(Stage::Adjacency);
            return;
        }
        const auto index = cursor++;
        auto width       = std::max(40.0, nodeLabels[index].width + 24.0);
        auto height      = std::max(32.0, nodeLabels[index].height + 16.0);
        const auto shape = graph->nodes[index].shape;
        if (shape == FlowchartShape::Bar)
        {
            // Canonical top-to-bottom coordinates rotate with the graph, so the bar remains perpendicular to the flow.
            plan->nodes.push_back({shape, 0u, {0.0, 0.0, 64.0, 8.0}, {}});
            return;
        }
        if (shape == FlowchartShape::Circle || shape == FlowchartShape::DoubleCircle)
            width = height = std::hypot(width, height) + (shape == FlowchartShape::DoubleCircle ? 8.0 : 0.0);
        else if (shape == FlowchartShape::Diamond)
        {
            width *= 2.0;
            height *= 2.0;
        }
        else if (shape == FlowchartShape::Hexagon)
            width *= 2.0; // The centered label fits inside the straight middle half.
        else if (shape == FlowchartShape::Asymmetric || shape == FlowchartShape::Parallelogram || shape == FlowchartShape::ReverseParallelogram ||
                 shape == FlowchartShape::Trapezoid || shape == FlowchartShape::ReverseTrapezoid)
            width *= 2.0; // The label stays within every slanted edge's central band.
        else if (shape == FlowchartShape::Ellipse)
        {
            width *= 1.5;
            height *= 1.5;
        }
        else if (shape == FlowchartShape::Subroutine)
            width = std::max(width, nodeLabels[index].width + 40.0);
        else if (shape == FlowchartShape::Cylinder)
            height += 16.0;
        else if (shape == FlowchartShape::Stadium)
            // Keep the entire centered label rectangle inside the capsule's
            // straight middle band, independent of its measured text height.
            width = std::max(width, nodeLabels[index].width + height + 8.0);
        if (graph->nodes[index].sequenceActor)
            height += SequenceActorHeight;
        if (width > MaximumExtent || height > MaximumExtent)
        {
            Fail(FlowchartLayoutIssue::NodeExtent);
            return;
        }
        plan->nodes.push_back({shape, 0u, {0.0, 0.0, horizontal ? height : width, horizontal ? width : height}, {}});
    }
    void PieNodes()
    {
        if (cursor == plan->nodes.size())
        {
            plan->extent = {pieWidth + Margin, std::max(plan->pieCircle.y + pieDiameter, pieLegendY) + Margin};
            if (! Valid(plan->extent))
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            Then(Stage::Finish);
            return;
        }
        const auto index        = cursor++;
        auto& slice             = plan->nodes[index];
        const auto label        = nodeLabels[index];
        const auto x            = plan->pieCircle.x + pieDiameter + 24.0;
        const auto legendHeight = std::max(12.0, label.height);
        slice.bounds            = {x, pieLegendY + (legendHeight - 12.0) / 2.0, 12.0, 12.0};
        slice.label             = {x + 20.0, pieLegendY + (legendHeight - label.height) / 2.0, label.width, label.height};
        slice.pieStart          = pieAngle;
        pieAngle += graph->nodes[index].pieValue / pieTotal * 6.28318530717958647692;
        if (graph->nodes[index].pieValue > 0.0 && std::none_of(graph->nodes.begin() + static_cast<std::ptrdiff_t>(index + 1u),
                                                               graph->nodes.end(),
                                                               [](const auto& next) { return next.pieValue > 0.0; }))
            pieAngle = 6.28318530717958647692;
        slice.pieEnd = std::min(pieAngle, 6.28318530717958647692);
        pieLegendY += legendHeight + 8.0;
        pieWidth = std::max(pieWidth, slice.label.x + slice.label.width);
        if (pieWidth + Margin > MaximumExtent || pieLegendY + Margin > MaximumExtent || pieDiameter > MaximumExtent)
            Fail(FlowchartLayoutIssue::DiagramExtent);
    }
    void GanttNodes()
    {
        if (cursor == plan->nodes.size())
        {
            plan->extent = {std::max(plan->ganttAxis.x + plan->ganttAxis.width, plan->ganttTitle.x + plan->ganttTitle.width) + Margin, ganttY + Margin};
            if (! Valid(plan->extent))
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            Then(Stage::Finish);
            return;
        }
        const auto index    = cursor++;
        auto& task          = plan->nodes[index];
        const auto label    = nodeLabels[index];
        const double height = std::max(20.0, label.height + 8.0);
        task.label          = {Margin, ganttY + (height - label.height) / 2.0, label.width, label.height};
        task.bounds         = {plan->ganttAxis.x + static_cast<double>(graph->nodes[index].ganttStart - ganttMinimum) * ganttScale,
                               ganttY + (height - 12.0) / 2.0,
                               static_cast<double>(graph->nodes[index].ganttEnd - graph->nodes[index].ganttStart) * ganttScale,
                               12.0};
        ganttY += height + 8.0;
        if (ganttY + Margin > MaximumExtent)
            Fail(FlowchartLayoutIssue::DiagramExtent);
    }
    void MindNodes()
    {
        if (cursor == plan->nodes.size())
        {
            Then(Stage::MindEdges);
            return;
        }
        const auto index   = cursor++;
        auto& node         = plan->nodes[index];
        const auto& model  = graph->nodes[index];
        const auto centerX = mindCenterX + static_cast<double>(mindSide[index]) * static_cast<double>(model.mindDepth) * mindColumn;
        const auto centerY = index == 0u ? mindCenterY : Margin + mindRow * static_cast<double>(index - 1u) + (mindRow - 16.0) / 2.0;
        node.bounds.x      = centerX - node.bounds.width / 2.0;
        node.bounds.y      = centerY - node.bounds.height / 2.0;
        node.label         = {node.bounds.x + (node.bounds.width - nodeLabels[index].width) / 2.0,
                              node.bounds.y + (node.bounds.height - nodeLabels[index].height) / 2.0,
                              nodeLabels[index].width,
                              nodeLabels[index].height};
    }
    void MindEdges()
    {
        if (cursor == graph->edges.size())
        {
            plan->extent = {mindWidth, mindHeight};
            Then(Stage::Finish);
            return;
        }
        const auto index = cursor++;
        const auto& edge = graph->edges[index];
        const auto& from = plan->nodes[edge.from].bounds;
        const auto& to   = plan->nodes[edge.to].bounds;
        const bool right = mindSide[edge.to] > 0;
        const FlowchartPoint first{right ? from.x + from.width : from.x, from.y + from.height / 2.0};
        const FlowchartPoint last{right ? to.x : to.x + to.width, to.y + to.height / 2.0};
        const auto middle = (first.x + last.x) / 2.0;
        FlowchartPlacedEdge placed;
        placed.relation = FlowchartRelation::Line;
        placed.path     = {{first, first, {middle, first.y}, {middle, last.y}, last, last}};
        plan->edges.push_back(placed);
    }
    void SequenceNodes()
    {
        if (cursor == plan->nodes.size())
        {
            sequenceWidth = sequenceX - (plan->nodes.empty() ? 0.0 : sequenceGap);
            sequenceY     = Margin + sequenceHeader + 24.0;
            Then(Stage::SequenceEdges);
            return;
        }
        const auto index    = cursor++;
        auto& node          = plan->nodes[index];
        node.bounds.x       = sequenceX;
        node.bounds.y       = Margin + (sequenceHeader - node.bounds.height) / (sequenceActors ? 1.0 : 2.0);
        const double symbol = graph->nodes[index].sequenceActor ? SequenceActorHeight : 0.0;
        node.label          = {node.bounds.x + (node.bounds.width - nodeLabels[index].width) / 2.0,
                               node.bounds.y + symbol + (node.bounds.height - symbol - nodeLabels[index].height) / 2.0,
                               nodeLabels[index].width,
                               nodeLabels[index].height};
        sequenceX += node.bounds.width + sequenceGap;
        if (sequenceX > MaximumExtent)
            Fail(FlowchartLayoutIssue::DiagramExtent);
    }
    void SequenceEdges()
    {
        if (cursor == graph->edges.size())
        {
            plan->extent = {sequenceWidth + Margin, sequenceY + Margin};
            if (! Valid(plan->extent))
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            Then(Stage::Finish);
            return;
        }
        const auto index   = cursor++;
        const auto& edge   = graph->edges[index];
        const auto& from   = plan->nodes[edge.from].bounds;
        const auto& to     = plan->nodes[edge.to].bounds;
        const double fromX = from.x + from.width / 2.0, toX = to.x + to.width / 2.0;
        const auto label = edgeLabels[index];
        if (edge.sequenceNote != SequenceNotePlacement::None)
        {
            const double width  = std::max(label.width + 16.0, edge.sequenceNote == SequenceNotePlacement::Over ? std::abs(toX - fromX) + 24.0 : 0.0);
            const double x      = edge.sequenceNote == SequenceNotePlacement::Left    ? fromX - 16.0 - width
                                  : edge.sequenceNote == SequenceNotePlacement::Right ? fromX + 16.0
                                                                                      : (fromX + toX - width) / 2.0;
            const double bottom = sequenceY + label.height + 16.0;
            if (x < Margin || x + width + Margin > MaximumExtent || bottom + Margin > MaximumExtent)
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            FlowchartPlacedEdge placed;
            placed.sequenceNote = true;
            placed.hasLabel     = true;
            placed.path         = {{{x, sequenceY}, {x + width, bottom}, {x + width, bottom}, {x + width, bottom}, {x + width, bottom}, {x + width, bottom}}};
            placed.label        = {x + (width - label.width) / 2.0, sequenceY + 8.0, label.width, label.height};
            sequenceWidth       = std::max(sequenceWidth, x + width);
            sequenceY           = bottom + 24.0;
            plan->edges.push_back(placed);
            return;
        }
        const double lineY = sequenceY + label.height + 6.0;
        const bool self    = edge.from == edge.to;
        FlowchartPlacedEdge placed;
        placed.route      = self ? FlowchartRoute::Self : FlowchartRoute::Forward;
        placed.relation   = edge.relation;
        placed.style      = edge.style;
        placed.hasArrow   = edge.arrow;
        placed.hasLabel   = true;
        placed.openArrow  = edge.openArrow;
        const double endY = self ? lineY + 20.0 : lineY;
        if (self)
            placed.path = {{{fromX, lineY}, {fromX + 24.0, lineY}, {fromX + 24.0, endY}, {toX, endY}, {toX, endY}, {toX, endY}}};
        else
            placed.path = {{{fromX, lineY}, {fromX, lineY}, {fromX, lineY}, {toX, lineY}, {toX, lineY}, {toX, lineY}}};
        const double arrowBase = toX + (self || toX < fromX ? 8.0 : -8.0);
        placed.arrow           = {{{toX, endY}, {arrowBase, endY - 4.0}, {arrowBase, endY + 4.0}}};
        const double labelX    = self ? fromX + 8.0 : (fromX + toX - label.width) / 2.0;
        placed.label           = {labelX, sequenceY, label.width, label.height};
        sequenceWidth          = std::max(sequenceWidth, std::max(labelX + label.width, self ? fromX + 24.0 : toX));
        sequenceY              = endY + 24.0;
        if (sequenceWidth + Margin > MaximumExtent || sequenceY + Margin > MaximumExtent)
        {
            Fail(FlowchartLayoutIssue::DiagramExtent);
            return;
        }
        plan->edges.push_back(placed);
    }
    void Adjacency()
    {
        if (cursor == graph->edges.size())
        {
            Then(Stage::Dfs);
            return;
        }
        const auto index = cursor++;
        const auto& edge = graph->edges[index];
        if (tail[edge.from] != NoIndex)
            next[tail[edge.from]] = index;
        else
            head[edge.from] = index;
        tail[edge.from] = index;
        ++outgoing[edge.from];
        ++incoming[edge.to];
        plan->edges.emplace_back();
    }
    void Dfs()
    {
        if (! depth)
        {
            if (roots == graph->nodes.size())
            {
                Then(Stage::Indegree);
                return;
            }
            const auto node = roots++;
            if (! color[node])
            {
                color[node]    = 1u;
                stack[depth++] = {node, head[node]};
            }
            return;
        }
        auto& frame = stack[depth - 1u];
        if (frame.edge == NoIndex)
        {
            color[frame.node] = 2u;
            --depth;
            return;
        }
        const auto edge = frame.edge;
        frame.edge      = next[edge];
        const auto to   = graph->edges[edge].to;
        if (to == frame.node)
            plan->edges[edge].route = FlowchartRoute::Self;
        else if (color[to] == 1u)
            plan->edges[edge].route = FlowchartRoute::Back;
        else if (! color[to])
        {
            color[to]      = 1u;
            stack[depth++] = {to, head[to]};
        }
    }
    void Rank()
    {
        if (stage == Stage::Indegree)
        {
            if (cursor == graph->edges.size())
            {
                Then(Stage::ChooseRank);
                return;
            }
            const auto index = cursor++;
            if (plan->edges[index].route == FlowchartRoute::Forward)
                ++indegree[graph->edges[index].to];
            return;
        }
        if (stage == Stage::ChooseRank)
        {
            if (rankedCount == graph->nodes.size())
            {
                for (std::size_t node = 0u; node < graph->nodes.size(); ++node)
                    ++rowCount[rank[node]];
                // A matching of two equal-width ranks can use parallel direct
                // routes after ordering targets by their source. Restrict this
                // to complete, isolated pairs so no unrelated route or group
                // can be crossed by the reordered nodes.
                const auto count = graph->edges.size();
                alignedPairs     = count > 1u && graph->nodes.size() == 2u * count && graph->groups.empty() && ! hasNotes && ! hasCardinality;
                for (std::size_t node = 0u; alignedPairs && node < graph->nodes.size(); ++node)
                    alignedPairs = rank[node] == (node < count ? 0u : 1u) && plan->nodes[node].bounds.width == plan->nodes[0].bounds.width &&
                                   (node < count ? outgoing[node] == 1u && incoming[node] == 0u : incoming[node] == 1u && outgoing[node] == 0u);
                for (std::size_t source = 0u; alignedPairs && source < count; ++source)
                {
                    const auto edge = head[source];
                    alignedPairs    = edge != NoIndex && graph->edges[edge].to >= count && graph->edges[edge].label.empty() &&
                                      plan->edges[edge].route == FlowchartRoute::Forward;
                    if (alignedPairs)
                        pairTargets[source] = graph->edges[edge].to;
                }
                // A complete two-rank grid with one member per
                // independent group can align its columns without merging
                // unrelated group bounds. Model indices remain unchanged.
                const auto groupCount = graph->groups.size();
                alignedGroups =
                    groupCount > 1u && graph->nodes.size() == 2u * groupCount && ! hasNotes && rowCount[0u] == groupCount && rowCount[1u] == groupCount;
                groupColumns.fill(NoIndex);
                for (std::size_t group = 0u; alignedGroups && group < groupCount; ++group)
                {
                    alignedGroups           = graph->groups[group].parentMask == 0u;
                    groupColumnWidth[group] = Cross(groupLabels[group]);
                }
                for (std::size_t node = 0u; alignedGroups && node < graph->nodes.size(); ++node)
                {
                    const auto mask = graph->nodes[node].groupMask;
                    std::size_t group{};
                    while (group < groupCount && mask != static_cast<std::uint16_t>(1u << group))
                        ++group;
                    alignedGroups = group < groupCount && rank[node] < 2u;
                    if (alignedGroups)
                    {
                        const auto slot         = rank[node] * groupCount + group;
                        alignedGroups           = groupColumns[slot] == NoIndex;
                        groupColumns[slot]      = node;
                        groupColumnWidth[group] = std::max(groupColumnWidth[group], plan->nodes[node].bounds.width);
                    }
                }
                if (graph->family == NativeDiagramFamily::Flowchart && graph->groups.empty() && ! hasNotes && ! alignedPairs && graph->edges.size() > 1u)
                    Then(Stage::OrderInitialize);
                else
                    Then(Stage::Ports);
                return;
            }
            for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                if (! ranked[index] && ! indegree[index])
                {
                    active        = index;
                    ranked[index] = true;
                    ++rankedCount;
                    activeEdge = head[index];
                    stage      = Stage::RankEdges;
                    return;
                }
            // DFS removes every gray back edge, so this is an internal invariant.
            throw std::logic_error("Flowchart DAG ranking made no progress");
        }
        if (activeEdge == NoIndex)
        {
            stage = Stage::ChooseRank;
            return;
        }
        const auto edge = activeEdge;
        activeEdge      = next[edge];
        if (plan->edges[edge].route != FlowchartRoute::Forward)
            return;
        const auto to     = graph->edges[edge].to;
        const auto length = graph->edges[edge].minimumRanks;
        if (length >= Nodes || rank[active] >= Nodes - length)
        {
            Fail(FlowchartLayoutIssue::DiagramExtent);
            return;
        }
        rank[to] = std::max(rank[to], rank[active] + length);
        --indegree[to];
    }
    // Two stable adjacent-rank barycenter sweeps are evaluated against the
    // original rank order. Each event handles one node or edge; bounded inner
    // comparisons never exceed the parser's node/edge ceilings. A candidate is
    // retained only when its independently counted inversions strictly decrease.
    void Order()
    {
        if (stage == Stage::OrderInitialize)
        {
            if (cursor == graph->nodes.size())
            {
                neighborSum.fill(0.0);
                neighborCount.fill(0u);
                Then(Stage::OrderAccumulate);
                return;
            }
            const auto node    = cursor++;
            orderedNodes[node] = node;
            std::size_t position{};
            for (std::size_t prior = 0u; prior < node; ++prior)
                position += rank[prior] == rank[node];
            originalPosition[node] = orderPosition[node] = position;
            return;
        }
        if (stage == Stage::OrderAccumulate)
        {
            if (cursor == graph->edges.size())
            {
                Then(Stage::OrderSort);
                return;
            }
            const auto edgeIndex = cursor++;
            const auto& edge     = graph->edges[edgeIndex];
            if (plan->edges[edgeIndex].route != FlowchartRoute::Forward || rank[edge.to] != rank[edge.from] + 1u)
                return;
            const auto node     = orderSweep == 0u ? edge.to : edge.from;
            const auto neighbor = orderSweep == 0u ? edge.from : edge.to;
            neighborSum[node] += static_cast<double>(orderPosition[neighbor]);
            ++neighborCount[node];
            return;
        }
        if (stage == Stage::OrderSort)
        {
            if (cursor == graph->nodes.size())
            {
                rowCount.fill(0u);
                Then(Stage::OrderPosition);
                return;
            }
            const auto current = cursor++;
            const auto node    = orderedNodes[current];
            const auto score   = [&](std::size_t index)
            { return neighborCount[index] ? neighborSum[index] / static_cast<double>(neighborCount[index]) : static_cast<double>(orderPosition[index]); };
            auto insertion = current;
            while (insertion)
            {
                const auto prior = orderedNodes[insertion - 1u];
                if (rank[prior] < rank[node] || (rank[prior] == rank[node] && score(prior) <= score(node)))
                    break;
                orderedNodes[insertion] = prior;
                --insertion;
            }
            orderedNodes[insertion] = node;
            return;
        }
        if (stage == Stage::OrderPosition)
        {
            if (cursor == graph->nodes.size())
            {
                if (++orderSweep < 2u)
                {
                    neighborSum.fill(0.0);
                    neighborCount.fill(0u);
                    Then(Stage::OrderAccumulate);
                }
                else
                    Then(Stage::OrderCompare);
                return;
            }
            const auto node     = orderedNodes[cursor++];
            orderPosition[node] = rowCount[rank[node]]++;
            return;
        }
        if (cursor == graph->edges.size())
        {
            useOrdering = candidateCrossings < originalCrossings;
            Then(Stage::Ports);
            return;
        }
        const auto index = cursor++;
        const auto& edge = graph->edges[index];
        if (plan->edges[index].route != FlowchartRoute::Forward || rank[edge.to] != rank[edge.from] + 1u)
            return;
        for (std::size_t prior = 0u; prior < index; ++prior)
        {
            const auto& other = graph->edges[prior];
            if (plan->edges[prior].route != FlowchartRoute::Forward || rank[other.from] != rank[edge.from] || rank[other.to] != rank[edge.to] ||
                edge.from == other.from || edge.to == other.to)
                continue;
            const auto crossed = [&](const auto& positions)
            { return (positions[edge.from] < positions[other.from]) != (positions[edge.to] < positions[other.to]); };
            originalCrossings += crossed(originalPosition);
            candidateCrossings += crossed(orderPosition);
        }
    }
    void Ports()
    {
        if (cursor == graph->edges.size())
        {
            Then(Stage::Rows);
            return;
        }
        const auto index = cursor++;
        const auto& edge = graph->edges[index];
        if (edge.relation == FlowchartRelation::Note)
            return;
        const auto back = plan->edges[index].route == FlowchartRoute::Back;
        if (hasCardinality)
        {
            const auto reserve = [&](std::size_t node, std::size_t side, FlowchartSize label)
            {
                auto& slots       = relationshipAttachments->sides[node][side];
                const auto width  = std::max(PortGap, Cross(label) + Margin);
                const auto offset = slots.width + width / 2.0;
                slots.width += width;
                slots.labelWidth = std::max(slots.labelWidth, Cross(label));
                ++slots.count;
                return offset;
            };
            relationshipAttachments->from[index] = reserve(edge.from, back ? 0u : 1u, fromCardinalities[index]);
            relationshipAttachments->to[index]   = reserve(edge.to, back ? 1u : 0u, toCardinalities[index]);
        }
        startPort[index]    = back ? ++before[rank[edge.from]] : ++after[rank[edge.from]];
        endPort[index]      = back ? ++after[rank[edge.to]] : ++before[rank[edge.to]];
        const auto parallel = std::ranges::count_if(graph->edges, [&](const auto& other) { return other.from == edge.from && other.to == edge.to; }) > 1;
        direct[index]       = alignedPairs || (! parallel && graph->groups.empty() && ! hasNotes && plan->edges[index].route == FlowchartRoute::Forward &&
                                               rank[edge.to] == rank[edge.from] + 1u);
        if (direct[index] && ! edge.label.empty())
        {
            // Work in the major axis: LR/RL labels need their measured width,
            // TB/BT labels need their height. Endpoint cardinalities keep their
            // own space beside the boxes instead of competing with the label.
            rowLabelGap[rank[edge.from]] = std::max(rowLabelGap[rank[edge.from]],
                                                    Major(edgeLabels[index]) + Major(fromCardinalities[index]) + Major(toCardinalities[index]) + 2.0 * Margin);
            // Adjacent fan labels lie halfway between their attachments. Their
            // centers therefore need twice the measured cross-axis space at
            // the sibling boxes. Keep box sizes independent of label lengths.
            nodeGap = std::max(nodeGap, 2.0 * Cross(edgeLabels[index]) + 2.0 * Margin);
        }
        if (direct[index])
            return;
        laneWidth[index] = edge.label.empty() ? PortGap : std::max(PortGap, Cross(edgeLabels[index]) + Margin);
        std::array<std::uint64_t, RankWords> occupied{};
        if (edge.label.empty())
        {
            const auto first = std::min(rank[edge.from], rank[edge.to]);
            const auto last  = std::max(rank[edge.from], rank[edge.to]);
            for (auto row = first; row <= last; ++row)
                occupied[row / 64u] |= std::uint64_t{1u} << (row % 64u);
            for (std::size_t slot = 0u; slot < laneCount; ++slot)
            {
                if (! laneSlotReusable[slot] || laneSlotBack[slot] != back)
                    continue;
                bool overlap{};
                for (std::size_t word = 0u; word < RankWords; ++word)
                    overlap = overlap || (laneRanks[slot][word] & occupied[word]) != 0u;
                if (overlap)
                    continue;
                for (std::size_t word = 0u; word < RankWords; ++word)
                    laneRanks[slot][word] |= occupied[word];
                laneOffset[index] = laneSlotOffset[slot];
                return;
            }
        }
        auto& sideWidth             = back ? leftWidth : rightWidth;
        laneOffset[index]           = sideWidth;
        laneSlotOffset[laneCount]   = sideWidth;
        laneSlotBack[laneCount]     = back;
        laneSlotReusable[laneCount] = edge.label.empty();
        laneRanks[laneCount++]      = occupied;
        sideWidth += laneWidth[index];
    }
    void Rows()
    {
        if (cursor == graph->nodes.size())
        {
            if (alignedGroups)
            {
                double width{};
                for (std::size_t group = 0u; group < graph->groups.size(); ++group)
                    width += groupColumnWidth[group] + (group ? nodeGap : 0.0);
                rowWidth[0u] = rowWidth[1u] = bandWidth = width;
            }
            bandLeft = Margin + leftWidth;
            if (bandLeft + bandWidth + rightWidth + 2.0 * Margin > MaximumExtent)
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            Then(Stage::RowPositions);
            return;
        }
        const auto index = cursor++;
        const auto row   = rank[index];
        auto& bounds     = plan->nodes[index].bounds;
        if (hasCardinality)
            for (const auto& side : relationshipAttachments->sides[index])
                if (side.count > 1u)
                    bounds.width = std::max(bounds.width, side.width + side.labelWidth + 2.0 * Margin);
        rowWidth[row] += (rowWidth[row] ? nodeGap : 0.0) + bounds.width;
        rowHeight[row] = std::max(rowHeight[row], bounds.height);
        bandWidth      = std::max(bandWidth, rowWidth[row]);
        maximumRank    = std::max(maximumRank, row);
    }
    void RowPositions()
    {
        if (cursor > maximumRank)
        {
            Then(Stage::NodesPosition);
            return;
        }
        const auto row = cursor++;
        rowTop[row]    = rowY + static_cast<double>(before[row]) * PortGap;
        rowY           = rowTop[row] + rowHeight[row] + static_cast<double>(after[row]) * PortGap + std::max(RankGap, rowLabelGap[row]);
        if (rowTop[row] + rowHeight[row] + Margin > MaximumExtent)
        {
            Fail(FlowchartLayoutIssue::DiagramExtent);
            return;
        }
        rowCursor[row] = bandLeft + (bandWidth - rowWidth[row]) / 2.0;
    }
    void NodesPosition()
    {
        if (cursor == plan->nodes.size())
        {
            // A single two-rank group with one external sibling cannot use
            // independently centered rows: the group envelope then covers the
            // sibling. Align the two members in a column beside that sibling.
            // Keep model order/indices intact and leave all other topologies
            // to the general collision-checked router.
            if (graph->groups.size() == 1u && graph->groups[0u].parentMask == 0u && graph->nodes.size() == 3u && maximumRank == 1u && ! hasNotes)
            {
                std::size_t outsider = NoIndex, upper = NoIndex, lower = NoIndex;
                for (std::size_t i = 0u; i < graph->nodes.size(); ++i)
                {
                    if (graph->nodes[i].groupMask == 0u && rank[i] == 0u && outsider == NoIndex)
                        outsider = i;
                    else if (graph->nodes[i].groupMask == 1u && rank[i] == 0u && upper == NoIndex)
                        upper = i;
                    else if (graph->nodes[i].groupMask == 1u && rank[i] == 1u && lower == NoIndex)
                        lower = i;
                }
                if (outsider != NoIndex && upper != NoIndex && lower != NoIndex)
                {
                    const auto column     = bandLeft + plan->nodes[outsider].bounds.width + nodeGap + 16.0;
                    const auto groupWidth = std::max({plan->nodes[upper].bounds.width, plan->nodes[lower].bounds.width, Cross(groupLabels[0u])});
                    if (column + groupWidth + rightWidth + Margin + 8.0 > MaximumExtent)
                    {
                        Fail(FlowchartLayoutIssue::DiagramExtent);
                        return;
                    }
                    for (const auto index : {outsider, upper, lower})
                    {
                        auto& node   = plan->nodes[index];
                        const auto x = index == outsider ? bandLeft : column;
                        node.label.x += x - node.bounds.x;
                        node.bounds.x = x;
                    }
                    bandWidth = column - bandLeft + groupWidth + 8.0;
                }
            }
            Then(hasNotes ? Stage::NotePositions : Stage::GroupPositions);
            return;
        }
        const auto position = cursor++;
        const auto index    = alignedGroups                                     ? groupColumns[position]
                              : alignedPairs && position >= graph->edges.size() ? pairTargets[position - graph->edges.size()]
                              : useOrdering                                     ? orderedNodes[position]
                                                                                : position;
        auto& node          = plan->nodes[index];
        if (useOrdering)
        {
            originalX[index] = bandLeft + (bandWidth - rowWidth[rank[index]]) / 2.0;
            for (std::size_t prior = 0u; prior < index; ++prior)
                if (rank[prior] == rank[index])
                    originalX[index] += plan->nodes[prior].bounds.width + nodeGap;
        }
        node.rank     = rank[index];
        node.bounds.x = rowCursor[node.rank];
        node.bounds.y = rowTop[node.rank] + (rowHeight[node.rank] - node.bounds.height) / 2.0;
        rowCursor[node.rank] += (alignedGroups ? groupColumnWidth[position % graph->groups.size()] : node.bounds.width) + nodeGap;
        node.label = {node.bounds.x + (node.bounds.width - Cross(nodeLabels[index])) / 2.0,
                      node.bounds.y + (node.bounds.height - Major(nodeLabels[index])) / 2.0,
                      Cross(nodeLabels[index]),
                      Major(nodeLabels[index])};
    }
    void NotePositions()
    {
        if (cursor == graph->nodes.size())
        {
            Then(Stage::NoteCollision);
            return;
        }
        const auto index   = cursor++;
        const auto& source = graph->nodes[index];
        if (source.shape != FlowchartShape::Note || source.noteTarget == MarkdownNoNode)
            return;
        auto& note         = plan->nodes[index];
        const auto& target = plan->nodes[source.noteTarget];
        if (! horizontal)
        {
            note.bounds.x = source.noteLeft ? target.bounds.x - note.bounds.width - 12.0 : target.bounds.x + target.bounds.width + 12.0;
            note.bounds.y = target.bounds.y + (target.bounds.height - note.bounds.height) / 2.0;
        }
        else
        {
            const bool forward = (graph->direction == FlowchartDirection::LeftRight) != source.noteLeft;
            note.bounds.y      = forward ? target.bounds.y + target.bounds.height + 12.0 : target.bounds.y - note.bounds.height - 12.0;
            note.bounds.x      = target.bounds.x + (target.bounds.width - note.bounds.width) / 2.0;
        }
        note.label = {note.bounds.x + (note.bounds.width - Cross(nodeLabels[index])) / 2.0,
                      note.bounds.y + (note.bounds.height - Major(nodeLabels[index])) / 2.0,
                      Cross(nodeLabels[index]),
                      Major(nodeLabels[index])};
    }
    void NoteCollision()
    {
        if (cursor == graph->nodes.size())
        {
            Then(Stage::GroupPositions);
            return;
        }
        const auto index = cursor++;
        if (graph->nodes[index].shape != FlowchartShape::Note)
            return;
        const auto& note = plan->nodes[index].bounds;
        for (std::size_t other = 0u; other < plan->nodes.size(); ++other)
        {
            if (other == index)
                continue;
            const auto& bounds = plan->nodes[other].bounds;
            if (note.x < bounds.x + bounds.width && bounds.x < note.x + note.width && note.y < bounds.y + bounds.height && bounds.y < note.y + note.height)
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
        }
    }
    void GroupPositions()
    {
        if (cursor == graph->groups.size())
        {
            Then(Stage::Routes);
            return;
        }
        const auto index = graph->groups.size() - ++cursor;
        const auto bit   = static_cast<std::uint16_t>(1u << index);
        double left{}, top{}, right{}, bottom{};
        bool any{};
        const auto include = [&](const FlowchartRect& rect)
        {
            if (! any)
            {
                left   = rect.x;
                top    = rect.y;
                right  = rect.x + rect.width;
                bottom = rect.y + rect.height;
                any    = true;
                return;
            }
            left   = std::min(left, rect.x);
            top    = std::min(top, rect.y);
            right  = std::max(right, rect.x + rect.width);
            bottom = std::max(bottom, rect.y + rect.height);
        };
        for (std::size_t node = 0u; node < graph->nodes.size(); ++node)
            if (graph->nodes[node].groupMask & bit)
                include(plan->nodes[node].bounds);
        for (std::size_t child = index + 1u; child < graph->groups.size(); ++child)
            if (graph->groups[child].parentMask & bit)
                include(plan->groups[child].bounds);
        if (! any)
        {
            Fail(FlowchartLayoutIssue::DiagramExtent);
            return;
        }
        const auto title = groupLabels[index];
        const auto width = std::max(right - left + 16.0, Cross(title) + 16.0);
        auto& result     = plan->groups[index];
        result.bounds    = {left - 8.0, top - Major(title) - 16.0, width, bottom - top + Major(title) + 24.0};
        result.label     = {result.bounds.x + 8.0, result.bounds.y + 4.0, Cross(title), Major(title)};
        for (std::size_t node = 0u; node < graph->nodes.size(); ++node)
        {
            const auto& other = plan->nodes[node].bounds;
            if (! (graph->nodes[node].groupMask & bit) && result.bounds.x < other.x + other.width && other.x < result.bounds.x + result.bounds.width &&
                result.bounds.y < other.y + other.height && other.y < result.bounds.y + result.bounds.height)
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
        }
        for (std::size_t other = index + 1u; other < graph->groups.size(); ++other)
        {
            const auto& bounds = plan->groups[other].bounds;
            if (! (graph->groups[other].parentMask & bit) && result.bounds.x < bounds.x + bounds.width && bounds.x < result.bounds.x + result.bounds.width &&
                result.bounds.y < bounds.y + bounds.height && bounds.y < result.bounds.y + result.bounds.height)
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
        }
    }
    void Routes()
    {
        if (cursor == plan->edges.size())
        {
            Then(useOrdering && ! restoringRoutes ? Stage::RouteCompare : Stage::TransformNodes);
            return;
        }
        const auto index = cursor++;
        auto& placed     = plan->edges[index];
        const auto& edge = graph->edges[index];
        const auto& from = plan->nodes[edge.from];
        const auto& to   = plan->nodes[edge.to];
        if (edge.relation == FlowchartRelation::Note)
        {
            const auto left = graph->nodes[edge.to].noteLeft;
            FlowchartPoint first{}, last{};
            if (! horizontal)
            {
                first = {left ? from.bounds.x : from.bounds.x + from.bounds.width, from.bounds.y + from.bounds.height / 2.0};
                last  = {left ? to.bounds.x + to.bounds.width : to.bounds.x, to.bounds.y + to.bounds.height / 2.0};
            }
            else
            {
                const bool forward = (graph->direction == FlowchartDirection::LeftRight) != left;
                first              = {from.bounds.x + from.bounds.width / 2.0, forward ? from.bounds.y + from.bounds.height : from.bounds.y};
                last               = {to.bounds.x + to.bounds.width / 2.0, forward ? to.bounds.y : to.bounds.y + to.bounds.height};
            }
            placed.path     = {first, first, first, last, last, last};
            placed.relation = edge.relation;
            placed.style    = edge.style;
            return;
        }
        const auto back       = placed.route == FlowchartRoute::Back;
        const auto start      = back ? from.bounds.y : from.bounds.y + from.bounds.height;
        const auto end        = back ? to.bounds.y + to.bounds.height : to.bounds.y;
        const auto first      = back ? rowTop[from.rank] - static_cast<double>(startPort[index]) * PortGap
                                     : rowTop[from.rank] + rowHeight[from.rank] + static_cast<double>(startPort[index]) * PortGap;
        const auto last       = back ? rowTop[to.rank] + rowHeight[to.rank] + static_cast<double>(endPort[index]) * PortGap
                                     : rowTop[to.rank] - static_cast<double>(endPort[index]) * PortGap;
        const auto attachment = [&](const auto& node, std::size_t nodeIndex, std::size_t sideIndex, bool source)
        {
            if (! relationshipAttachments)
                return node.bounds.x + node.bounds.width / 2.0;
            const auto& side  = relationshipAttachments->sides[nodeIndex][sideIndex];
            const auto offset = source ? relationshipAttachments->from[index] : relationshipAttachments->to[index];
            return node.bounds.x + (side.count > 1u ? (node.bounds.width - side.width) / 2.0 + offset : node.bounds.width / 2.0);
        };
        const auto fromX = attachment(from, edge.from, back ? 0u : 1u, true);
        const auto toX   = attachment(to, edge.to, back ? 1u : 0u, false);
        // Adjacent ranks have a measured, empty label corridor. Long/back and
        // parallel routes retain their separate exterior lanes.
        const auto cross       = direct[index] ? toX
                                 : back        ? Margin + leftWidth - laneOffset[index] - laneWidth[index]
                                               : bandLeft + bandWidth + Margin + laneOffset[index];
        placed.path            = {{{fromX, start}, {fromX, first}, {cross, first}, {cross, last}, {toX, last}, {toX, end}}};
        const auto labelMiddle = (first + last) / 2.0;
        if (direct[index] && ! edge.label.empty())
            placed.path = {{{fromX, start}, {fromX, labelMiddle}, {fromX, labelMiddle}, {toX, labelMiddle}, {toX, labelMiddle}, {toX, end}}};
        if (useOrdering && ! restoringRoutes)
        {
            auto& original     = originalPaths[index];
            original           = placed.path;
            const auto oldFrom = originalX[edge.from] + fromX - from.bounds.x;
            const auto oldTo   = originalX[edge.to] + toX - to.bounds.x;
            original[0u].x = original[1u].x = oldFrom;
            original[4u].x = original[5u].x = oldTo;
            if (direct[index])
            {
                original[2u].x = ! edge.label.empty() ? oldFrom : oldTo;
                original[3u].x = oldTo;
            }
        }
        placed.relation        = edge.relation;
        placed.style           = edge.style;
        placed.erFrom          = edge.erFrom;
        placed.erTo            = edge.erTo;
        placed.hasArrow        = edge.arrow;
        const auto base        = end + (back ? Margin : -Margin);
        placed.arrow           = {{{toX, end}, {toX - 4.0, base}, {toX + 4.0, base}}};
        const auto middle      = end + (back ? 6.0 : -6.0);
        const auto diamondTail = end + (back ? 12.0 : -12.0);
        placed.diamond         = {{{toX, end}, {toX - 4.0, middle}, {toX, diamondTail}, {toX + 4.0, middle}}};
        placed.hasLabel        = ! edge.label.empty();
        placed.label           = {direct[index] ? (fromX + toX - Cross(edgeLabels[index])) / 2.0 : cross + 4.0,
                                  labelMiddle - Major(edgeLabels[index]) / 2.0,
                                  Cross(edgeLabels[index]),
                                  Major(edgeLabels[index])};
        const auto fromSize = fromCardinalities[index], toSize = toCardinalities[index];
        placed.fromCardinality = {fromX + 4.0, start + (back ? -Major(fromSize) - 2.0 : 2.0), Cross(fromSize), Major(fromSize)};
        placed.toCardinality   = {toX + 4.0, end + (back ? 2.0 : -Major(toSize) - 2.0), Cross(toSize), Major(toSize)};
    }
    static std::size_t ProperCrossings(const std::array<FlowchartPoint, 6u>& first, const std::array<FlowchartPoint, 6u>& second) noexcept
    {
        std::size_t result{};
        for (std::size_t a = 1u; a < first.size(); ++a)
            for (std::size_t b = 1u; b < second.size(); ++b)
            {
                const auto p = first[a - 1u], q = first[a], r = second[b - 1u], s = second[b];
                const bool verticalA = p.x == q.x, verticalB = r.x == s.x;
                if (verticalA == verticalB)
                    continue;
                const auto v = verticalA ? p : r, ve = verticalA ? q : s;
                const auto h = verticalA ? r : p, he = verticalA ? s : q;
                result += v.x > std::min(h.x, he.x) && v.x < std::max(h.x, he.x) && h.y > std::min(v.y, ve.y) && h.y < std::max(v.y, ve.y);
            }
        return result;
    }
    void CompareRoutes()
    {
        if (cursor == plan->edges.size())
        {
            if (candidateRouteCrossings < originalRouteCrossings)
                Then(Stage::TransformNodes);
            else
            {
                restoringRoutes = true;
                Then(Stage::RestoreNodes);
            }
            return;
        }
        const auto index = cursor++;
        for (std::size_t prior = 0u; prior < index; ++prior)
        {
            originalRouteCrossings += ProperCrossings(originalPaths[index], originalPaths[prior]);
            candidateRouteCrossings += ProperCrossings(plan->edges[index].path, plan->edges[prior].path);
        }
    }
    void RestoreNodes()
    {
        if (cursor == plan->nodes.size())
        {
            Then(Stage::Routes);
            return;
        }
        const auto index = cursor++;
        auto& node       = plan->nodes[index];
        node.label.x += originalX[index] - node.bounds.x;
        node.bounds.x = originalX[index];
    }
    void TransformNodes()
    {
        if (cursor == plan->nodes.size())
        {
            Then(Stage::TransformGroups);
            return;
        }
        auto& node  = plan->nodes[cursor++];
        node.bounds = Transform(node.bounds);
        node.label  = Transform(node.label);
        Include(node.bounds);
        Include(node.label);
    }
    void TransformGroups()
    {
        if (cursor == plan->groups.size())
        {
            Then(Stage::TransformEdges);
            return;
        }
        auto& group  = plan->groups[cursor++];
        group.bounds = Transform(group.bounds);
        group.label  = Transform(group.label);
        Include(group.bounds);
        Include(group.label);
    }
    void TransformEdges()
    {
        if (cursor == plan->edges.size())
        {
            Then(Stage::CardinalityCheck);
            if (! hasCardinality && graph->groups.empty())
                cursor = plan->edges.size();
            return;
        }
        auto& edge = plan->edges[cursor++];
        for (auto& point : edge.path)
        {
            point = Transform(point);
            Include(point);
        }
        for (auto& point : edge.arrow)
        {
            point = Transform(point);
            if (edge.hasArrow)
                Include(point);
        }
        for (auto& point : edge.diamond)
        {
            point = Transform(point);
            if (edge.relation == FlowchartRelation::Aggregation || edge.relation == FlowchartRelation::Composition)
                Include(point);
        }
        if (horizontal)
        {
            const auto& sourceEdge = graph->edges[cursor - 1u];
            const auto inset       = [&](std::size_t nodeIndex, FlowchartPoint point)
            {
                const auto& node = plan->nodes[nodeIndex];
                if (node.shape < FlowchartShape::Parallelogram || node.shape > FlowchartShape::ReverseTrapezoid)
                    return 0.0;
                const auto amount = node.bounds.width * .125;
                return point.x <= node.bounds.x + node.bounds.width / 2.0 ? amount : -amount;
            };
            edge.path.front().x += inset(sourceEdge.from, edge.path.front());
            const auto targetInset = inset(sourceEdge.to, edge.path.back());
            edge.path.back().x += targetInset;
            if (edge.hasArrow)
                for (auto& point : edge.arrow)
                    point.x += targetInset;
            if (edge.relation == FlowchartRelation::Aggregation || edge.relation == FlowchartRelation::Composition)
                for (auto& point : edge.diamond)
                    point.x += targetInset;
        }
        edge.label = Transform(edge.label);
        if (edge.hasLabel)
            Include(edge.label);
        edge.fromCardinality = Transform(edge.fromCardinality);
        edge.toCardinality   = Transform(edge.toCardinality);
        if (! graph->edges[cursor - 1u].fromCardinality.empty())
            Include(edge.fromCardinality);
        if (! graph->edges[cursor - 1u].toCardinality.empty())
            Include(edge.toCardinality);
    }
    void CardinalityCheck()
    {
        if (cursor == plan->edges.size())
        {
            plan->extent = {bounded ? maxX - minX + 2.0 * Margin : 2.0 * Margin, bounded ? maxY - minY + 2.0 * Margin : 2.0 * Margin};
            if (! Valid(plan->extent))
            {
                Fail(FlowchartLayoutIssue::DiagramExtent);
                return;
            }
            shift = {Margin - minX, Margin - minY};
            Then(Stage::NormalizeNodes);
            return;
        }
        const auto current  = cursor++;
        const auto overlaps = [](const FlowchartRect& a, const FlowchartRect& b) noexcept
        { return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height; };
        if (plan->edges[current].hasLabel)
            for (const auto& group : plan->groups)
                if (overlaps(plan->edges[current].label, group.label))
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
        for (const bool from : {true, false})
        {
            const auto& cardinality = from ? graph->edges[current].fromCardinality : graph->edges[current].toCardinality;
            if (cardinality.empty())
                continue;
            const auto& bounds = from ? plan->edges[current].fromCardinality : plan->edges[current].toCardinality;
            for (const auto& group : plan->groups)
                if (overlaps(bounds, group.label))
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
            for (const auto& node : plan->nodes)
                if (overlaps(bounds, node.bounds))
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
            for (std::size_t other = 0u; other < plan->edges.size(); ++other)
            {
                const auto& candidate = plan->edges[other];
                const auto& source    = graph->edges[other];
                if ((candidate.hasLabel && overlaps(bounds, candidate.label)) ||
                    (! (other == current && from) && ! source.fromCardinality.empty() && overlaps(bounds, candidate.fromCardinality)) ||
                    (! (other == current && ! from) && ! source.toCardinality.empty() && overlaps(bounds, candidate.toCardinality)))
                {
                    Fail(FlowchartLayoutIssue::DiagramExtent);
                    return;
                }
            }
        }
    }
    void Step()
    {
        switch (stage)
        {
            case Stage::Measure: Measure(); break;
            case Stage::SequenceNodes: SequenceNodes(); break;
            case Stage::SequenceEdges: SequenceEdges(); break;
            case Stage::PieNodes: PieNodes(); break;
            case Stage::GanttNodes: GanttNodes(); break;
            case Stage::MindNodes: MindNodes(); break;
            case Stage::MindEdges: MindEdges(); break;
            case Stage::Adjacency: Adjacency(); break;
            case Stage::Dfs: Dfs(); break;
            case Stage::Indegree:
            case Stage::ChooseRank:
            case Stage::RankEdges: Rank(); break;
            case Stage::OrderInitialize:
            case Stage::OrderAccumulate:
            case Stage::OrderSort:
            case Stage::OrderPosition:
            case Stage::OrderCompare: Order(); break;
            case Stage::Ports: Ports(); break;
            case Stage::Rows: Rows(); break;
            case Stage::RowPositions: RowPositions(); break;
            case Stage::NodesPosition: NodesPosition(); break;
            case Stage::NotePositions: NotePositions(); break;
            case Stage::NoteCollision: NoteCollision(); break;
            case Stage::GroupPositions: GroupPositions(); break;
            case Stage::Routes: Routes(); break;
            case Stage::RouteCompare: CompareRoutes(); break;
            case Stage::RestoreNodes: RestoreNodes(); break;
            case Stage::TransformNodes: TransformNodes(); break;
            case Stage::TransformGroups: TransformGroups(); break;
            case Stage::TransformEdges: TransformEdges(); break;
            case Stage::CardinalityCheck: CardinalityCheck(); break;
            case Stage::NormalizeNodes:
                if (cursor == plan->nodes.size())
                {
                    Then(Stage::NormalizeGroups);
                    break;
                }
                Shift(plan->nodes[cursor].bounds, shift);
                Shift(plan->nodes[cursor].label, shift);
                ++cursor;
                break;
            case Stage::NormalizeGroups:
                if (cursor == plan->groups.size())
                {
                    Then(Stage::NormalizeEdges);
                    break;
                }
                Shift(plan->groups[cursor].bounds, shift);
                Shift(plan->groups[cursor].label, shift);
                ++cursor;
                break;
            case Stage::NormalizeEdges:
                if (cursor == plan->edges.size())
                {
                    Then(Stage::Finish);
                    break;
                }
                for (auto& point : plan->edges[cursor].path)
                    Shift(point, shift);
                for (auto& point : plan->edges[cursor].arrow)
                    Shift(point, shift);
                for (auto& point : plan->edges[cursor].diamond)
                    Shift(point, shift);
                Shift(plan->edges[cursor].label, shift);
                Shift(plan->edges[cursor].fromCardinality, shift);
                Shift(plan->edges[cursor].toCardinality, shift);
                ++cursor;
                break;
            case Stage::Finish: done = true; break;
        }
    }
};
NativeFlowchartLayout::NativeFlowchartLayout() noexcept  = default;
NativeFlowchartLayout::~NativeFlowchartLayout() noexcept = default;
void NativeFlowchartLayout::Begin(std::shared_ptr<const FlowchartModel> graph,
                                  std::span<const FlowchartSize> nodeLabels,
                                  std::span<const FlowchartSize> edgeLabels,
                                  std::span<const FlowchartSize> fromCardinalities,
                                  std::span<const FlowchartSize> toCardinalities,
                                  std::span<const FlowchartSize> groupLabels)
{
    if (! graph || graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() > NativeFlowchartParser::MaximumNodes ||
        graph->edges.size() > NativeFlowchartParser::MaximumEdges || nodeLabels.size() != graph->nodes.size() || edgeLabels.size() != graph->edges.size())
        throw std::invalid_argument("Flowchart layout needs a prepared bounded graph and exact label measurements");
    if (! ValidFlowchartModel(*graph))
        throw std::invalid_argument("Invalid native flowchart model");
    for (const auto value : nodeLabels)
        if (! Valid(value))
            throw std::invalid_argument("Invalid flowchart node measurements");
    for (const auto value : edgeLabels)
        if (! Valid(value))
            throw std::invalid_argument("Invalid flowchart edge measurements");
    if ((! fromCardinalities.empty() && fromCardinalities.size() != graph->edges.size()) ||
        (! toCardinalities.empty() && toCardinalities.size() != graph->edges.size()))
        throw std::invalid_argument("Invalid class cardinality measurements");
    for (const auto& edge : graph->edges)
        if ((! edge.fromCardinality.empty() && fromCardinalities.empty()) || (! edge.toCardinality.empty() && toCardinalities.empty()))
            throw std::invalid_argument("Missing class cardinality measurements");
    for (const auto& value : fromCardinalities)
        if (! Valid(value))
            throw std::invalid_argument("Invalid source cardinality measurement");
    for (const auto& value : toCardinalities)
        if (! Valid(value))
            throw std::invalid_argument("Invalid target cardinality measurement");
    if (graph->groups.size() > 16u ||
        groupLabels.size() != ((graph->family == NativeDiagramFamily::Pie || graph->family == NativeDiagramFamily::Gantt) ? 1u : graph->groups.size()))
        throw std::invalid_argument("Invalid flowchart group measurements");
    for (const auto& value : groupLabels)
        if (! Valid(value))
            throw std::invalid_argument("Invalid flowchart group label measurement");
    auto next    = std::make_unique<Preparation>(std::move(graph), nodeLabels, edgeLabels, fromCardinalities, toCardinalities, groupLabels);
    preparation_ = std::move(next);
}
MarkdownAdvance NativeFlowchartLayout::Advance(std::size_t maximumWorkUnits)
{
    if (! preparation_)
        return {MarkdownPreparationStatus::Idle, 0u};
    std::size_t work{};
    while (work < maximumWorkUnits)
    {
        preparation_->Step();
        ++work;
        if (preparation_->done)
        {
            published_ = preparation_->plan;
            preparation_.reset();
            return {MarkdownPreparationStatus::Complete, work};
        }
    }
    return {MarkdownPreparationStatus::Pending, work};
}
bool NativeFlowchartLayout::HasPendingPreparation() const noexcept
{
    return ! ! preparation_;
}
std::shared_ptr<const FlowchartLayoutPlan> NativeFlowchartLayout::Published() const noexcept
{
    return published_;
}
void NativeFlowchartLayout::Cancel() noexcept
{
    preparation_.reset();
}
void NativeFlowchartLayout::Clear() noexcept
{
    Cancel();
    published_.reset();
}
} // namespace DxUi::RichText::Detail
