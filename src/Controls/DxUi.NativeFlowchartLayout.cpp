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
    static constexpr auto Nodes = NativeFlowchartParser::MaximumNodes, Edges = NativeFlowchartParser::MaximumEdges;
    static constexpr auto RankWords = (Nodes + 63u) / 64u;
    enum class Stage
    {
        Measure,
        SequenceNodes,
        SequenceEdges,
        PieNodes,
        Adjacency,
        Dfs,
        Indegree,
        ChooseRank,
        RankEdges,
        Ports,
        Rows,
        RowPositions,
        NodesPosition,
        NotePositions,
        NoteCollision,
        GroupPositions,
        Routes,
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
    std::array<std::size_t, Nodes> groupColumns{};
    std::array<double, 16u> groupColumnWidth{};
    std::array<bool, Nodes> ranked{};
    std::array<std::size_t, Edges> next{}, startPort{}, endPort{};
    std::array<double, Edges> laneOffset{}, laneWidth{};
    std::array<bool, Edges> direct{};
    std::array<std::array<std::uint64_t, RankWords>, Edges> laneRanks{};
    std::array<double, Edges> laneSlotOffset{};
    std::array<bool, Edges> laneSlotBack{}, laneSlotReusable{};
    std::array<DfsFrame, Nodes> stack{};
    std::array<double, Nodes> rowWidth{}, rowHeight{}, rowTop{}, rowCursor{};
    std::size_t cursor{}, roots{}, depth{}, rankedCount{}, active{}, activeEdge{}, maximumRank{}, laneCount{};
    double bandWidth{}, bandLeft{}, rowY{Margin}, leftWidth{}, rightWidth{};
    double minX{}, minY{}, maxX{}, maxY{};
    double sequenceX{Margin}, sequenceY{}, sequenceWidth{}, sequenceHeader{}, sequenceGap{};
    double pieTotal{}, pieAngle{}, pieLegendY{}, pieWidth{}, pieDiameter{};
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
            if (graph->family == NativeDiagramFamily::Pie)
            {
                for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                    pieTotal += graph->nodes[index].pieValue;
                if (pieTotal <= 0.0 || ! std::isfinite(pieTotal)) { Fail(FlowchartLayoutIssue::DiagramExtent); return; }
                pieDiameter = 144.0;
                for (std::size_t index = 0u; index < graph->nodes.size(); ++index)
                    pieDiameter = std::max(pieDiameter, nodeLabels[index].height * 8.0);
                const auto title = groupLabels[0u];
                plan->pieTitle = {Margin, Margin, title.width, title.height};
                plan->pieCircle = {Margin, Margin + title.height + (title.height ? 12.0 : 0.0), pieDiameter, pieDiameter};
                pieLegendY = plan->pieCircle.y;
                pieWidth = std::max(Margin + title.width, Margin + pieDiameter);
                Then(Stage::PieNodes);
            }
            else if (graph->family == NativeDiagramFamily::Sequence)
            {
                sequenceGap = 48.0;
                for (std::size_t edge = 0u; edge < graph->edges.size(); ++edge)
                    sequenceGap = std::max(sequenceGap, edgeLabels[edge].width + 24.0);
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
        if (shape == FlowchartShape::Circle)
            width = height = std::hypot(width, height);
        else if (shape == FlowchartShape::Diamond)
        {
            width *= 2.0;
            height *= 2.0;
        }
        else if (shape == FlowchartShape::Hexagon)
            width *= 2.0; // The centered label fits inside the straight middle half.
        else if (shape == FlowchartShape::Subroutine)
            width = std::max(width, nodeLabels[index].width + 40.0);
        else if (shape == FlowchartShape::Stadium)
            // Keep the entire centered label rectangle inside the capsule's
            // straight middle band, independent of its measured text height.
            width = std::max(width, nodeLabels[index].width + height + 8.0);
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
            if (! Valid(plan->extent)) { Fail(FlowchartLayoutIssue::DiagramExtent); return; }
            Then(Stage::Finish);
            return;
        }
        const auto index = cursor++;
        auto& slice = plan->nodes[index];
        const auto label = nodeLabels[index];
        const auto x = plan->pieCircle.x + pieDiameter + 24.0;
        const auto legendHeight = std::max(12.0, label.height);
        slice.bounds = {x, pieLegendY + (legendHeight - 12.0) / 2.0, 12.0, 12.0};
        slice.label = {x + 20.0, pieLegendY + (legendHeight - label.height) / 2.0, label.width, label.height};
        slice.pieStart = pieAngle;
        pieAngle += graph->nodes[index].pieValue / pieTotal * 6.28318530717958647692;
        if (graph->nodes[index].pieValue > 0.0 && std::none_of(graph->nodes.begin() + static_cast<std::ptrdiff_t>(index + 1u), graph->nodes.end(),
                                                               [](const auto& next) { return next.pieValue > 0.0; }))
            pieAngle = 6.28318530717958647692;
        slice.pieEnd = std::min(pieAngle, 6.28318530717958647692);
        pieLegendY += legendHeight + 8.0;
        pieWidth = std::max(pieWidth, slice.label.x + slice.label.width);
        if (pieWidth + Margin > MaximumExtent || pieLegendY + Margin > MaximumExtent || pieDiameter > MaximumExtent)
            Fail(FlowchartLayoutIssue::DiagramExtent);
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
        const auto index = cursor++;
        auto& node       = plan->nodes[index];
        node.bounds.x    = sequenceX;
        node.bounds.y    = Margin + (sequenceHeader - node.bounds.height) / 2.0;
        node.label       = {node.bounds.x + (node.bounds.width - nodeLabels[index].width) / 2.0,
                            node.bounds.y + (node.bounds.height - nodeLabels[index].height) / 2.0,
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
        const auto label   = edgeLabels[index];
        const double lineY = sequenceY + label.height + 6.0;
        const bool self    = edge.from == edge.to;
        FlowchartPlacedEdge placed;
        placed.route      = self ? FlowchartRoute::Self : FlowchartRoute::Forward;
        placed.relation   = edge.relation;
        placed.style      = edge.style;
        placed.hasArrow   = true;
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
        const auto back  = plan->edges[index].route == FlowchartRoute::Back;
        startPort[index] = back ? ++before[rank[edge.from]] : ++after[rank[edge.from]];
        endPort[index]   = back ? ++after[rank[edge.to]] : ++before[rank[edge.to]];
        direct[index] =
            alignedPairs || (plan->edges[index].route == FlowchartRoute::Forward && edge.label.empty() && rank[edge.to] == rank[edge.from] + 1u &&
                             rowCount[rank[edge.from]] == 1u && rowCount[rank[edge.to]] == 1u && outgoing[edge.from] == 1u && incoming[edge.to] == 1u);
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
                    width += groupColumnWidth[group] + (group ? NodeGap : 0.0);
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
        const auto index   = cursor++;
        const auto row     = rank[index];
        const auto& bounds = plan->nodes[index].bounds;
        rowWidth[row] += (rowWidth[row] ? NodeGap : 0.0) + bounds.width;
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
        rowY           = rowTop[row] + rowHeight[row] + static_cast<double>(after[row]) * PortGap + RankGap;
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
            Then(hasNotes ? Stage::NotePositions : Stage::GroupPositions);
            return;
        }
        const auto position = cursor++;
        const auto index    = alignedGroups                                     ? groupColumns[position]
                              : alignedPairs && position >= graph->edges.size() ? pairTargets[position - graph->edges.size()]
                                                                                : position;
        auto& node          = plan->nodes[index];
        node.rank           = rank[index];
        node.bounds.x       = rowCursor[node.rank];
        node.bounds.y       = rowTop[node.rank] + (rowHeight[node.rank] - node.bounds.height) / 2.0;
        rowCursor[node.rank] += (alignedGroups ? groupColumnWidth[position % graph->groups.size()] : node.bounds.width) + NodeGap;
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
            Then(Stage::TransformNodes);
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
        const auto back  = placed.route == FlowchartRoute::Back;
        const auto start = back ? from.bounds.y : from.bounds.y + from.bounds.height;
        const auto end   = back ? to.bounds.y + to.bounds.height : to.bounds.y;
        const auto first = back ? rowTop[from.rank] - static_cast<double>(startPort[index]) * PortGap
                                : rowTop[from.rank] + rowHeight[from.rank] + static_cast<double>(startPort[index]) * PortGap;
        const auto last  = back ? rowTop[to.rank] + rowHeight[to.rank] + static_cast<double>(endPort[index]) * PortGap
                                : rowTop[to.rank] - static_cast<double>(endPort[index]) * PortGap;
        const auto fromX = from.bounds.x + from.bounds.width / 2.0, toX = to.bounds.x + to.bounds.width / 2.0;
        // Unambiguous adjacent single-node ranks share a centerline; every
        // other route retains its exterior lane and rank-row corridors.
        const auto cross       = direct[index] ? fromX
                                 : back        ? Margin + leftWidth - laneOffset[index] - laneWidth[index]
                                               : bandLeft + bandWidth + Margin + laneOffset[index];
        placed.path            = {{{fromX, start}, {fromX, first}, {cross, first}, {cross, last}, {toX, last}, {toX, end}}};
        placed.relation        = edge.relation;
        placed.style           = edge.style;
        placed.hasArrow        = edge.arrow;
        const auto base        = end + (back ? Margin : -Margin);
        placed.arrow           = {{{toX, end}, {toX - 4.0, base}, {toX + 4.0, base}}};
        const auto middle      = end + (back ? 6.0 : -6.0);
        const auto diamondTail = end + (back ? 12.0 : -12.0);
        placed.diamond         = {{{toX, end}, {toX - 4.0, middle}, {toX, diamondTail}, {toX + 4.0, middle}}};
        placed.hasLabel        = ! edge.label.empty();
        placed.label           = {cross + 4.0, (first + last - Major(edgeLabels[index])) / 2.0, Cross(edgeLabels[index]), Major(edgeLabels[index])};
        const auto fromSize = fromCardinalities[index], toSize = toCardinalities[index];
        placed.fromCardinality = {fromX + 4.0, start + (back ? -Major(fromSize) - 2.0 : 2.0), Cross(fromSize), Major(fromSize)};
        placed.toCardinality   = {toX + 4.0, end + (back ? 2.0 : -Major(toSize) - 2.0), Cross(toSize), Major(toSize)};
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
            case Stage::Adjacency: Adjacency(); break;
            case Stage::Dfs: Dfs(); break;
            case Stage::Indegree:
            case Stage::ChooseRank:
            case Stage::RankEdges: Rank(); break;
            case Stage::Ports: Ports(); break;
            case Stage::Rows: Rows(); break;
            case Stage::RowPositions: RowPositions(); break;
            case Stage::NodesPosition: NodesPosition(); break;
            case Stage::NotePositions: NotePositions(); break;
            case Stage::NoteCollision: NoteCollision(); break;
            case Stage::GroupPositions: GroupPositions(); break;
            case Stage::Routes: Routes(); break;
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
    if (graph->direction < FlowchartDirection::TopBottom || graph->direction > FlowchartDirection::RightLeft)
        throw std::invalid_argument("Invalid flowchart layout direction");
    if (! ValidSequenceModel(*graph))
        throw std::invalid_argument("Invalid sequence timeline model");
    if (! ValidPieModel(*graph))
        throw std::invalid_argument("Invalid pie chart topology");
    for (const auto& node : graph->nodes)
        if (node.shape < FlowchartShape::Rectangle || node.shape > FlowchartShape::Note ||
            (node.shape == FlowchartShape::Note &&
             ((graph->family != NativeDiagramFamily::State && graph->family != NativeDiagramFamily::Class) ||
              (node.noteTarget == MarkdownNoNode ? graph->family != NativeDiagramFamily::Class
                                                 : node.noteTarget >= graph->nodes.size() || graph->nodes[node.noteTarget].shape == FlowchartShape::Note))))
            throw std::invalid_argument("Invalid flowchart layout shape");
    for (const auto& edge : graph->edges)
        if (edge.from >= graph->nodes.size() || edge.to >= graph->nodes.size() || edge.minimumRanks == 0u ||
            edge.minimumRanks > NativeFlowchartParser::MaximumLinkRanks || edge.relation < FlowchartRelation::Line ||
            edge.relation > FlowchartRelation::Composition || edge.style < FlowchartLineStyle::Solid || edge.style > FlowchartLineStyle::Invisible ||
            (edge.openArrow && (graph->family != NativeDiagramFamily::Sequence || ! edge.arrow)) ||
            (edge.style == FlowchartLineStyle::Invisible && (edge.arrow || ! edge.label.empty())) ||
            edge.arrow != (edge.relation != FlowchartRelation::Line && edge.relation != FlowchartRelation::Note) ||
            (edge.relation == FlowchartRelation::Note &&
             (graph->nodes[edge.to].shape != FlowchartShape::Note || graph->nodes[edge.to].noteTarget != edge.from)))
            throw std::invalid_argument("Invalid flowchart layout edge");
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
    if (graph->groups.size() > 16u || groupLabels.size() != (graph->family == NativeDiagramFamily::Pie ? 1u : graph->groups.size()))
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
