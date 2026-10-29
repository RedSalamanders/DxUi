#pragma once
#include "Controls/DxUi.NativeFlowchartLayout.h"
#include "Support.Tests.NativeFlowchartCorpus.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace FlowchartLayoutTests
{
using namespace DxUi::RichText;
using namespace DxUi::RichText::Detail;
inline std::shared_ptr<const FlowchartModel> Graph(std::wstring_view source)
{
    NativeFlowchartParser parser;
    parser.Begin(source, 7u);
    while (parser.HasPendingPreparation())
        parser.Advance(13u);
    return parser.Published();
}
template <typename Check> inline std::shared_ptr<const FlowchartLayoutPlan> Complete(NativeFlowchartLayout& layout, Check&& check, std::size_t budget = 1u)
{
    std::size_t total{};
    while (layout.HasPendingPreparation())
    {
        const auto result = layout.Advance(budget);
        check(result.workUnits > 0u && result.workUnits <= budget, "graph preparation respects each positive event allowance");
        total += result.workUnits;
        check(total < 20000u, "bounded graph preparation makes finite progress including cycles");
    }
    return layout.Published();
}
template <typename Check>
inline std::shared_ptr<const FlowchartLayoutPlan> Place(std::shared_ptr<const FlowchartModel> graph, Check&& check, std::size_t budget = 1u)
{
    const std::vector<FlowchartSize> nodes(graph->nodes.size(), {16.0, 12.0});
    std::vector<FlowchartSize> edges(graph->edges.size(), FlowchartSize{});
    for (std::size_t index = 0u; index < edges.size(); ++index)
        if (! graph->edges[index].label.empty())
            edges[index] = {20.0, 12.0};
    NativeFlowchartLayout layout;
    layout.Begin(std::move(graph), nodes, edges);
    return Complete(layout, check, budget);
}
inline bool Near(double a, double b) noexcept
{
    return std::abs(a - b) < 0.000001;
}
template <typename Check> inline void Geometry(Check&& check)
{
    // Hand-calculated two-rank oracle: 40x32 boxes, 16-DIP endpoint
    // corridors, 24-DIP rank separation, 8-DIP margins and one exterior lane.
    const auto graph = Graph(L"graph TD;A-->B");
    const auto plan  = Place(graph, check);
    check(plan->issue == FlowchartLayoutIssue::None && plan->nodes.size() == 2u && plan->edges.size() == 1u && Near(plan->extent.width, 64.0) &&
              Near(plan->extent.height, 136.0),
          "two-node graph has independently calculated total extents");
    check(Near(plan->nodes[0u].bounds.x, 8.0) && Near(plan->nodes[0u].bounds.y, 8.0) && Near(plan->nodes[0u].bounds.width, 40.0) &&
              Near(plan->nodes[0u].bounds.height, 32.0) && Near(plan->nodes[1u].bounds.y, 96.0) && plan->nodes[1u].rank == 1u,
          "two-node graph has exact independent shape placement and longest-path ranks");
    constexpr FlowchartPoint expected[] = {{28.0, 40.0}, {28.0, 56.0}, {56.0, 56.0}, {56.0, 80.0}, {28.0, 80.0}, {28.0, 96.0}};
    for (std::size_t index = 0u; index < std::size(expected); ++index)
        check(Near(plan->edges[0u].path[index].x, expected[index].x) && Near(plan->edges[0u].path[index].y, expected[index].y),
              "forward edge follows the independent six-point corridor oracle");
    check(plan->edges[0u].hasArrow && Near(plan->edges[0u].arrow[1u].x, 24.0) && Near(plan->edges[0u].arrow[1u].y, 88.0),
          "arrow head ends on the target shape boundary");
    const auto relations = Place(Graph(L"classDiagram\nBase <|-- Derived\nDerived --> Client\nClient -- Contract\n"), check);
    check(relations->edges.size() == 3u && relations->edges[0u].relation == FlowchartRelation::Inheritance && relations->edges[0u].hasArrow &&
              relations->edges[1u].relation == FlowchartRelation::Arrow && relations->edges[1u].hasArrow &&
              relations->edges[2u].relation == FlowchartRelation::Line && ! relations->edges[2u].hasArrow,
          "class inheritance, directed association and plain association retain distinct edge kinds through layout");
    const auto shapes = Place(Graph(L"graph TD;A[box];B(round);C((circle));D{diamond}"), check);
    check(Near(shapes->nodes[0u].bounds.width, 40.0) && Near(shapes->nodes[1u].bounds.width, 40.0) && Near(shapes->nodes[2u].bounds.width, std::sqrt(2624.0)) &&
              Near(shapes->nodes[2u].bounds.height, std::sqrt(2624.0)) && Near(shapes->nodes[3u].bounds.width, 80.0) &&
              Near(shapes->nodes[3u].bounds.height, 64.0),
          "each supported shape contains the padded measured label");
    const auto stadium           = Place(Graph(L"flowchart LR;A([capsule label])-->B([target])"), check);
    const auto& capsule          = stadium->nodes[0u];
    const auto& port             = stadium->edges[0u].path[0u];
    const auto& target           = stadium->nodes[1u].bounds;
    const auto& targetPort       = stadium->edges[0u].path[5u];
    const auto radius            = capsule.bounds.height / 2.0;
    const auto textHalfHeight    = capsule.label.height / 2.0;
    const auto straightHalfWidth = (capsule.bounds.width - capsule.bounds.height) / 2.0;
    check(capsule.shape == FlowchartShape::Stadium && capsule.bounds.width >= capsule.label.width + capsule.bounds.height &&
              straightHalfWidth >= capsule.label.width / 2.0 && textHalfHeight < radius,
          "stadium label rectangle stays inside its capsule straight band with shape-aware clearance");
    check(Near(port.x, capsule.bounds.x + capsule.bounds.width) && Near(port.y, capsule.bounds.y + capsule.bounds.height / 2.0) &&
              Near(targetPort.x, target.x) && Near(targetPort.y, target.y + target.height / 2.0) && radius > 0.0,
          "left-to-right edge ports land on the true capsule end boundaries");
    const auto cycles = Place(Graph(L"graph TD;A-->B;B-->A;A-->A;A-->B"), check);
    check(cycles->edges[0u].route == FlowchartRoute::Forward && cycles->edges[1u].route == FlowchartRoute::Back &&
              cycles->edges[2u].route == FlowchartRoute::Self && cycles->edges[3u].route == FlowchartRoute::Forward &&
              cycles->edges[0u].path[2u].x != cycles->edges[3u].path[2u].x,
          "stable DFS distinguishes cycles and self loops while parallel edges own separate lanes");
    for (const auto direction : {std::wstring_view(L"BT"), std::wstring_view(L"LR"), std::wstring_view(L"RL")})
    {
        const auto oriented = Place(Graph(std::wstring(L"graph ") + std::wstring(direction) + L";A-->B"), check);
        const auto& first   = oriented->nodes[0u].bounds;
        const auto& last    = oriented->nodes[1u].bounds;
        check(Near(first.width, 40.0) && Near(first.height, 32.0) && Near(oriented->nodes[0u].label.width, 16.0) &&
                  Near(oriented->nodes[0u].label.height, 12.0),
              "orientation preserves horizontal label and shape dimensions");
        check(direction == L"BT" ? first.y > last.y : direction == L"LR" ? first.x < last.x : first.x > last.x, "each direction changes actual flow ordering");
    }
}
inline bool InteriorSegment(FlowchartPoint first, FlowchartPoint last, FlowchartRect rect) noexcept
{
    if (Near(first.x, last.x))
        return first.x > rect.x && first.x < rect.x + rect.width && std::max(first.y, last.y) > rect.y && std::min(first.y, last.y) < rect.y + rect.height;
    return first.y > rect.y && first.y < rect.y + rect.height && std::max(first.x, last.x) > rect.x && std::min(first.x, last.x) < rect.x + rect.width;
}
template <typename Check> inline void Invariants(const FlowchartLayoutPlan& plan, Check&& check)
{
    check(plan.issue == FlowchartLayoutIssue::None && plan.extent.width > 0.0 && plan.extent.height > 0.0 &&
              plan.extent.width <= NativeFlowchartLayout::MaximumExtent && plan.extent.height <= NativeFlowchartLayout::MaximumExtent,
          "admitted graph has bounded positive geometry");
    const auto inside = [&](FlowchartRect rect)
    {
        check(rect.x >= 0.0 && rect.y >= 0.0 && rect.x + rect.width <= plan.extent.width && rect.y + rect.height <= plan.extent.height,
              "shape/label is inside complete retained bounds");
    };
    for (std::size_t index = 0u; index < plan.nodes.size(); ++index)
    {
        const auto& bounds = plan.nodes[index].bounds;
        inside(bounds);
        inside(plan.nodes[index].label);
        const auto& label = plan.nodes[index].label;
        check(label.x >= bounds.x && label.y >= bounds.y && label.x + label.width <= bounds.x + bounds.width &&
                  label.y + label.height <= bounds.y + bounds.height,
              "horizontal measured label is contained by its own shape bounds");
        const auto relativeX = label.width / bounds.width, relativeY = label.height / bounds.height;
        if (plan.nodes[index].shape == FlowchartShape::Circle)
            check(relativeX * relativeX + relativeY * relativeY <= 1.0, "all label corners lie inside the circle");
        if (plan.nodes[index].shape == FlowchartShape::Diamond)
            check(relativeX + relativeY <= 1.0, "all label corners lie inside the diamond");
        for (std::size_t other = 0u; other < index; ++other)
        {
            const auto& previous = plan.nodes[other].bounds;
            check(bounds.x >= previous.x + previous.width || bounds.x + bounds.width <= previous.x || bounds.y >= previous.y + previous.height ||
                      bounds.y + bounds.height <= previous.y,
                  "ranked graph shape rectangles never overlap");
        }
    }
    for (std::size_t edgeIndex = 0u; edgeIndex < plan.edges.size(); ++edgeIndex)
    {
        const auto& edge = plan.edges[edgeIndex];
        if (edge.hasLabel)
        {
            inside(edge.label);
            for (const auto& node : plan.nodes)
                check(edge.label.x >= node.bounds.x + node.bounds.width || edge.label.x + edge.label.width <= node.bounds.x ||
                          edge.label.y >= node.bounds.y + node.bounds.height || edge.label.y + edge.label.height <= node.bounds.y,
                      "exterior edge labels cannot overlap a node");
            for (std::size_t previous = 0u; previous < edgeIndex; ++previous)
                if (plan.edges[previous].hasLabel)
                {
                    const auto& other = plan.edges[previous].label;
                    check(edge.label.x >= other.x + other.width || edge.label.x + edge.label.width <= other.x || edge.label.y >= other.y + other.height ||
                              edge.label.y + edge.label.height <= other.y,
                          "distinct exterior lanes prevent edge-label overlap");
                }
        }
        for (std::size_t index = 1u; index < edge.path.size(); ++index)
        {
            const auto first = edge.path[index - 1u], last = edge.path[index];
            check(Near(first.x, last.x) || Near(first.y, last.y), "every retained route segment is orthogonal");
            check(first.x >= 0.0 && first.y >= 0.0 && first.x <= plan.extent.width && first.y <= plan.extent.height, "routes remain inside retained bounds");
            for (const auto& node : plan.nodes)
                check(! InteriorSegment(first, last, node.bounds), "edge routes never enter any node rectangle, including skipped ranks");
        }
    }
}
template <typename Check> inline void Corpus(Check&& check)
{
    std::size_t admitted{};
    for (const auto& example : NativeFlowchartCorpus::flowcharts)
    {
        if (example.outcome != FlowchartOutcome::Prepared)
            continue;
        const auto graph = Graph(example.source);
        const auto first = Place(graph, check, 1u), batched = Place(graph, check, 113u);
        Invariants(*first, check);
        check(first->nodes.size() == batched->nodes.size() && first->edges.size() == batched->edges.size() &&
                  Near(first->extent.width, batched->extent.width) && Near(first->extent.height, batched->extent.height),
              "one-event and batched graph requests have deterministic extents and topology");
        for (std::size_t index = 0u; index < first->edges.size(); ++index)
            for (std::size_t point = 0u; point < first->edges[index].path.size(); ++point)
                check(Near(first->edges[index].path[point].x, batched->edges[index].path[point].x) &&
                          Near(first->edges[index].path[point].y, batched->edges[index].path[point].y),
                      "budget choice cannot change edge routing");
        ++admitted;
    }
    check(admitted == 16u, "geometry covers every admitted original reference graph");
    const auto fanoutGraph = Graph(L"flowchart LR;A & B --> C & D --> E");
    const auto fanoutPlan  = Place(fanoutGraph, check, 7u);
    check(fanoutGraph->outcome == FlowchartOutcome::Prepared && fanoutPlan->nodes.size() == 5u && fanoutPlan->edges.size() == 6u,
          "grouped endpoint cross-products reach the existing bounded router with deterministic topology");
    Invariants(*fanoutPlan, check);
    for (const auto& edge : fanoutPlan->edges)
    {
        check(std::isfinite(edge.path.front().x) && std::isfinite(edge.path.front().y) && std::isfinite(edge.path.back().x) &&
                  std::isfinite(edge.path.back().y),
              "fan-out routing emits finite ports for every expanded edge");
        check(edge.path.front().x <= fanoutPlan->extent.width && edge.path.front().y <= fanoutPlan->extent.height &&
                  edge.path.back().x <= fanoutPlan->extent.width && edge.path.back().y <= fanoutPlan->extent.height,
              "fan-out ports remain within the finite diagram resource envelope");
    }
    for (const auto source : {L"graph TD;A-->B;B-->C;A-->C;C-->A;B-->B;D", L"graph RL;A-->|yes|B;B-->C;C-->|again|A;A-->A"})
        Invariants(*Place(Graph(source), check), check);
    std::wstring chain = L"graph TD;";
    for (std::size_t index = 1u; index < NativeFlowchartParser::MaximumNodes; ++index)
        chain += L"N" + std::to_wstring(index - 1u) + L"-->N" + std::to_wstring(index) + L";";
    const auto maximum = Place(Graph(chain), check, 113u);
    check(maximum->nodes.size() == 256u && maximum->nodes.back().rank == 255u, "maximum-depth admitted graph uses bounded iterative traversal");
    Invariants(*maximum, check);
}
template <typename Check> inline void Admission(Check&& check)
{
    auto graph = Graph(L"graph TD;A-->B");
    std::vector<FlowchartSize> nodes(2u, {16.0, 12.0}), edges(1u, FlowchartSize{});
    NativeFlowchartLayout layout;
    layout.Begin(graph, nodes, edges);
    const auto previous = Complete(layout, check);
    layout.Begin(graph, nodes, edges);
    check(layout.Advance(0u).workUnits == 0u && layout.HasPendingPreparation() && layout.Published() == previous,
          "zero budget preserves pending geometry and previous publication");
    nodes[0u].width = std::numeric_limits<double>::quiet_NaN();
    bool invalid{};
    try
    {
        layout.Begin(graph, nodes, edges);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    check(invalid && layout.HasPendingPreparation() && layout.Published() == previous, "invalid measurements cannot replace admitted pending geometry");
    for (const auto bad : {-1.0, std::numeric_limits<double>::infinity(), NativeFlowchartLayout::MaximumExtent + 1.0})
    {
        nodes[0u].width = bad;
        invalid         = false;
        try
        {
            layout.Begin(graph, nodes, edges);
        }
        catch (const std::invalid_argument&)
        {
            invalid = true;
        }
        check(invalid && layout.HasPendingPreparation() && layout.Published() == previous,
              "negative, infinite and over-admission metrics preserve the previous request");
    }
    nodes[0u].width = 16.0;
    invalid         = false;
    try
    {
        layout.Begin(graph, std::span<const FlowchartSize>(nodes).first(1u), edges);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    check(invalid && layout.HasPendingPreparation(), "measurement cardinality must match the complete graph");
    auto broken = std::make_shared<FlowchartModel>(L"invalid caller", 0u);
    broken->nodes.emplace_back();
    broken->edges.emplace_back();
    broken->edges[0u].to                           = 1u;
    const std::array<FlowchartSize, 1u> oneInvalid = {{{16.0, 12.0}}};
    invalid                                        = false;
    try
    {
        layout.Begin(broken, oneInvalid, oneInvalid);
    }
    catch (const std::invalid_argument&)
    {
        invalid = true;
    }
    check(invalid && layout.HasPendingPreparation(), "invalid edge endpoints are rejected before indexing fixed staging arrays");
    layout.Advance(3u);
    layout.Cancel();
    check(! layout.HasPendingPreparation() && layout.Published() == previous, "cancellation retires partial graph geometry only");
    layout.Begin(graph, nodes, edges);
    auto replacement                        = Graph(L"graph LR;New");
    const std::array<FlowchartSize, 1u> one = {{{20.0, 12.0}}};
    layout.Begin(replacement, one, {});
    check(Complete(layout, check)->nodes.size() == 1u && previous->nodes.size() == 2u,
          "replacement request cannot publish stale two-node staging or mutate held plans");
    nodes[0u].width = NativeFlowchartLayout::MaximumExtent;
    layout.Begin(graph, nodes, edges);
    const auto capacity = Complete(layout, check);
    check(capacity->issue == FlowchartLayoutIssue::NodeExtent && capacity->nodes.empty() && capacity->edges.empty() && capacity->extent.width == 0.0,
          "node extent failure exposes no partial renderable graph");
    nodes[0u].width                          = 16.0;
    auto loop                                = Graph(L"graph TD;A-->|large|A");
    const std::array<FlowchartSize, 1u> huge = {{{65500.0, 65500.0}}};
    layout.Begin(loop, one, huge);
    const auto envelope = Complete(layout, check);
    check(envelope->issue == FlowchartLayoutIssue::DiagramExtent && envelope->nodes.empty() && envelope->edges.empty(),
          "aggregate edge labels and lanes obey the independent diagram extent limit");
    std::weak_ptr<const FlowchartModel> released = replacement;
    layout.Begin(replacement, one, {});
    const auto detached = Complete(layout, check);
    replacement.reset();
    check(released.expired() && detached->nodes.size() == 1u, "published geometry owns no source or graph and survives input retirement");
    layout.Clear();
    check(! layout.Published() && ! layout.HasPendingPreparation() && detached->nodes.size() == 1u &&
              layout.Advance().status == MarkdownPreparationStatus::Idle,
          "clear and idle preserve independently retained plans");
    const auto empty = Place(Graph(L"graph TD"), check);
    check(empty->issue == FlowchartLayoutIssue::None && empty->nodes.empty() && empty->edges.empty() && Near(empty->extent.width, 16.0) &&
              Near(empty->extent.height, 16.0),
          "an admitted empty graph has finite explicit empty bounds");
}
} // namespace FlowchartLayoutTests
