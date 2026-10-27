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
    enum class Stage
    {
        Measure,
        Adjacency,
        Dfs,
        Indegree,
        ChooseRank,
        RankEdges,
        Ports,
        Rows,
        RowPositions,
        NodesPosition,
        Routes,
        TransformNodes,
        TransformEdges,
        NormalizeNodes,
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
    std::array<std::size_t, Nodes> head{}, tail{}, rank{}, indegree{}, before{}, after{}, color{};
    std::array<bool, Nodes> ranked{};
    std::array<std::size_t, Edges> next{}, startPort{}, endPort{}, lane{};
    std::array<DfsFrame, Nodes> stack{};
    std::array<double, Nodes> rowWidth{}, rowHeight{}, rowTop{}, rowCursor{};
    std::size_t cursor{}, roots{}, depth{}, rankedCount{}, active{}, activeEdge{}, maximumRank{}, leftLanes{}, rightLanes{};
    double bandWidth{}, bandLeft{}, rowY{Margin}, laneWidth{PortGap};
    double minX{}, minY{}, maxX{}, maxY{};
    FlowchartPoint shift;
    Stage stage{};
    bool horizontal{}, bounded{}, done{};

    Preparation(std::shared_ptr<const FlowchartModel> value, std::span<const FlowchartSize> nodes, std::span<const FlowchartSize> edges)
        : graph(std::move(value)),
          plan(std::make_shared<FlowchartLayoutPlan>())
    {
        std::copy(nodes.begin(), nodes.end(), nodeLabels.begin());
        std::copy(edges.begin(), edges.end(), edgeLabels.begin());
        head.fill(NoIndex);
        tail.fill(NoIndex);
        next.fill(NoIndex);
        plan->nodes.reserve(nodes.size());
        plan->edges.reserve(edges.size());
        horizontal = graph->direction == FlowchartDirection::LeftRight || graph->direction == FlowchartDirection::RightLeft;
    }
    void Fail(FlowchartLayoutIssue issue) noexcept
    {
        plan->issue  = issue;
        plan->extent = {};
        plan->nodes.clear();
        plan->edges.clear();
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
            Then(Stage::Adjacency);
            return;
        }
        const auto index = cursor++;
        auto width       = std::max(40.0, nodeLabels[index].width + 24.0);
        auto height      = std::max(32.0, nodeLabels[index].height + 16.0);
        const auto shape = graph->nodes[index].shape;
        if (shape == FlowchartShape::Circle)
            width = height = std::hypot(width, height);
        else if (shape == FlowchartShape::Diamond)
        {
            width *= 2.0;
            height *= 2.0;
        }
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
        plan->edges.emplace_back();
        if (! edge.label.empty())
            laneWidth = std::max(laneWidth, Cross(edgeLabels[index]) + Margin);
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
        const auto to = graph->edges[edge].to;
        rank[to]      = std::max(rank[to], rank[active] + 1u);
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
        const auto back  = plan->edges[index].route == FlowchartRoute::Back;
        startPort[index] = back ? ++before[rank[edge.from]] : ++after[rank[edge.from]];
        endPort[index]   = back ? ++after[rank[edge.to]] : ++before[rank[edge.to]];
        lane[index]      = back ? leftLanes++ : rightLanes++;
    }
    void Rows()
    {
        if (cursor == graph->nodes.size())
        {
            bandLeft = Margin + static_cast<double>(leftLanes) * laneWidth;
            if (bandLeft + bandWidth + static_cast<double>(rightLanes) * laneWidth + 2.0 * Margin > MaximumExtent)
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
            Then(Stage::Routes);
            return;
        }
        const auto index = cursor++;
        auto& node       = plan->nodes[index];
        node.rank        = rank[index];
        node.bounds.x    = rowCursor[node.rank];
        node.bounds.y    = rowTop[node.rank] + (rowHeight[node.rank] - node.bounds.height) / 2.0;
        rowCursor[node.rank] += node.bounds.width + NodeGap;
        node.label = {node.bounds.x + (node.bounds.width - Cross(nodeLabels[index])) / 2.0,
                      node.bounds.y + (node.bounds.height - Major(nodeLabels[index])) / 2.0,
                      Cross(nodeLabels[index]),
                      Major(nodeLabels[index])};
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
        const auto back  = placed.route == FlowchartRoute::Back;
        const auto start = back ? from.bounds.y : from.bounds.y + from.bounds.height;
        const auto end   = back ? to.bounds.y + to.bounds.height : to.bounds.y;
        const auto first = back ? rowTop[from.rank] - static_cast<double>(startPort[index]) * PortGap
                                : rowTop[from.rank] + rowHeight[from.rank] + static_cast<double>(startPort[index]) * PortGap;
        const auto last  = back ? rowTop[to.rank] + rowHeight[to.rank] + static_cast<double>(endPort[index]) * PortGap
                                : rowTop[to.rank] - static_cast<double>(endPort[index]) * PortGap;
        // Every edge has its own exterior lane, including parallel edges and
        // self loops. Horizontal corridors are outside complete rank rows;
        // skipped ranks therefore cannot put a line through a node interior.
        const auto cross = back ? Margin + static_cast<double>(leftLanes - 1u - lane[index]) * laneWidth
                                : bandLeft + bandWidth + Margin + static_cast<double>(lane[index]) * laneWidth;
        const auto fromX = from.bounds.x + from.bounds.width / 2.0, toX = to.bounds.x + to.bounds.width / 2.0;
        placed.path     = {{{fromX, start}, {fromX, first}, {cross, first}, {cross, last}, {toX, last}, {toX, end}}};
        placed.relation = edge.relation;
        placed.hasArrow = edge.arrow;
        const auto base = end + (back ? Margin : -Margin);
        placed.arrow    = {{{toX, end}, {toX - 4.0, base}, {toX + 4.0, base}}};
        placed.hasLabel = ! edge.label.empty();
        placed.label    = {cross + 4.0, (first + last - Major(edgeLabels[index])) / 2.0, Cross(edgeLabels[index]), Major(edgeLabels[index])};
    }
    void TransformNodes()
    {
        if (cursor == plan->nodes.size())
        {
            Then(Stage::TransformEdges);
            return;
        }
        auto& node  = plan->nodes[cursor++];
        node.bounds = Transform(node.bounds);
        node.label  = Transform(node.label);
        Include(node.bounds);
        Include(node.label);
    }
    void TransformEdges()
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
        edge.label = Transform(edge.label);
        if (edge.hasLabel)
            Include(edge.label);
    }
    void Step()
    {
        switch (stage)
        {
            case Stage::Measure: Measure(); break;
            case Stage::Adjacency: Adjacency(); break;
            case Stage::Dfs: Dfs(); break;
            case Stage::Indegree:
            case Stage::ChooseRank:
            case Stage::RankEdges: Rank(); break;
            case Stage::Ports: Ports(); break;
            case Stage::Rows: Rows(); break;
            case Stage::RowPositions: RowPositions(); break;
            case Stage::NodesPosition: NodesPosition(); break;
            case Stage::Routes: Routes(); break;
            case Stage::TransformNodes: TransformNodes(); break;
            case Stage::TransformEdges: TransformEdges(); break;
            case Stage::NormalizeNodes:
                if (cursor == plan->nodes.size())
                {
                    Then(Stage::NormalizeEdges);
                    break;
                }
                Shift(plan->nodes[cursor].bounds, shift);
                Shift(plan->nodes[cursor].label, shift);
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
                Shift(plan->edges[cursor].label, shift);
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
                                  std::span<const FlowchartSize> edgeLabels)
{
    if (! graph || graph->outcome != FlowchartOutcome::Prepared || graph->nodes.size() > NativeFlowchartParser::MaximumNodes ||
        graph->edges.size() > NativeFlowchartParser::MaximumEdges || nodeLabels.size() != graph->nodes.size() || edgeLabels.size() != graph->edges.size())
        throw std::invalid_argument("Flowchart layout needs a prepared bounded graph and exact label measurements");
    if (graph->direction < FlowchartDirection::TopBottom || graph->direction > FlowchartDirection::RightLeft)
        throw std::invalid_argument("Invalid flowchart layout direction");
    for (const auto& node : graph->nodes)
        if (node.shape < FlowchartShape::Rectangle || node.shape > FlowchartShape::Stadium)
            throw std::invalid_argument("Invalid flowchart layout shape");
    for (const auto& edge : graph->edges)
        if (edge.from >= graph->nodes.size() || edge.to >= graph->nodes.size() || edge.relation < FlowchartRelation::Line ||
            edge.relation > FlowchartRelation::Inheritance || edge.arrow != (edge.relation != FlowchartRelation::Line))
            throw std::invalid_argument("Invalid flowchart layout edge");
    for (const auto value : nodeLabels)
        if (! Valid(value))
            throw std::invalid_argument("Invalid flowchart node measurements");
    for (const auto value : edgeLabels)
        if (! Valid(value))
            throw std::invalid_argument("Invalid flowchart edge measurements");
    auto next    = std::make_unique<Preparation>(std::move(graph), nodeLabels, edgeLabels);
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
