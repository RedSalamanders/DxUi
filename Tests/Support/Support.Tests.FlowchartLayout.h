#pragma once
#include "Controls/DxUi.NativeFlowchartLayout.h"
#include "Support.Tests.NativeFlowchartCorpus.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

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
    const bool cardinality = std::ranges::any_of(graph->edges, [](const auto& edge) { return ! edge.fromCardinality.empty() || ! edge.toCardinality.empty(); });
    const std::vector<FlowchartSize> endpointLabels(cardinality ? graph->edges.size() : 0u, {16.0, 12.0});
    const std::vector<FlowchartSize> groups(graph->family == NativeDiagramFamily::Pie ? 1u : graph->groups.size(), {16.0, 12.0});
    for (std::size_t index = 0u; index < edges.size(); ++index)
        if (! graph->edges[index].label.empty())
            edges[index] = {20.0, 12.0};
    NativeFlowchartLayout layout;
    layout.Begin(std::move(graph), nodes, edges, endpointLabels, endpointLabels, groups);
    return Complete(layout, check, budget);
}
inline bool Near(double a, double b) noexcept
{
    return std::abs(a - b) < 0.000001;
}
template <typename Check> inline void Geometry(Check&& check)
{
    // Hand-calculated two-rank oracle: 40x32 boxes, 16-DIP endpoint
    // corridors, 24-DIP rank separation, 8-DIP margins and one direct route.
    const auto graph = Graph(L"graph TD;A-->B");
    const auto plan  = Place(graph, check);
    check(plan->issue == FlowchartLayoutIssue::None && plan->nodes.size() == 2u && plan->edges.size() == 1u && Near(plan->extent.width, 56.0) &&
              Near(plan->extent.height, 136.0),
          "two-node graph has independently calculated total extents");
    check(Near(plan->nodes[0u].bounds.x, 8.0) && Near(plan->nodes[0u].bounds.y, 8.0) && Near(plan->nodes[0u].bounds.width, 40.0) &&
              Near(plan->nodes[0u].bounds.height, 32.0) && Near(plan->nodes[1u].bounds.y, 96.0) && plan->nodes[1u].rank == 1u,
          "two-node graph has exact independent shape placement and longest-path ranks");
    constexpr FlowchartPoint expected[] = {{28.0, 40.0}, {28.0, 56.0}, {28.0, 56.0}, {28.0, 80.0}, {28.0, 80.0}, {28.0, 96.0}};
    for (std::size_t index = 0u; index < std::size(expected); ++index)
        check(Near(plan->edges[0u].path[index].x, expected[index].x) && Near(plan->edges[0u].path[index].y, expected[index].y),
              "unambiguous adjacent ranks use the independent direct centerline oracle");
    check(plan->edges[0u].hasArrow && Near(plan->edges[0u].arrow[1u].x, 24.0) && Near(plan->edges[0u].arrow[1u].y, 88.0),
          "arrow head ends on the target shape boundary");
    const auto sequence = Graph(L"sequenceDiagram\nparticipant Alice as Client\nparticipant Bob\nBob->>Alice: request\nAlice-->>Bob: reply\nBob-)Bob: self\n");
    const auto timeline = Place(sequence, check);
    check(sequence->outcome == FlowchartOutcome::Prepared && sequence->family == NativeDiagramFamily::Sequence && sequence->nodes.size() == 2u &&
              sequence->nodes[0u].label == L"Client" && sequence->edges.size() == 3u && sequence->edges[0u].from == 1u && sequence->edges[0u].to == 0u &&
              sequence->edges[1u].style == FlowchartLineStyle::Dotted && sequence->edges[2u].openArrow,
          "sequence model retains participant and message order, aliases and message kinds");
    check(timeline->issue == FlowchartLayoutIssue::None && timeline->nodes.size() == 2u && timeline->edges.size() == 3u &&
              timeline->nodes[0u].bounds.x < timeline->nodes[1u].bounds.x && timeline->edges[0u].path[0u].y < timeline->edges[1u].path[0u].y &&
              timeline->edges[1u].path[0u].y < timeline->edges[2u].path[0u].y && timeline->edges[2u].route == FlowchartRoute::Self &&
              timeline->edges[2u].openArrow && timeline->edges[0u].arrow[1u].x > timeline->edges[0u].arrow[0u].x &&
              timeline->edges[1u].arrow[1u].x < timeline->edges[1u].arrow[0u].x,
          "sequence layout uses ordered lifelines, correctly directed messages and a self-message loop");
    for (const auto invalid : {L"sequenceDiagram\nAlice->>Bob: x\nloop later\n", L"sequenceDiagram\nAlice->>Bob\n", L"sequenceDiagram\nactor Alice\n"})
    {
        const auto rejected = Graph(invalid);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == invalid && rejected->nodes.empty() && rejected->edges.empty(),
              "unsupported sequence constructs retain exact source without partial events");
    }
    const auto pie = Graph(L"pie showData title Inventory\n\"One\": 25\n\"Three\": 75\n\"Zero\": 0\n");
    const auto piePlan = Place(pie, check);
    check(pie->outcome == FlowchartOutcome::Prepared && pie->family == NativeDiagramFamily::Pie && pie->pieShowData &&
              pie->pieTitle == L"Inventory" && pie->nodes.size() == 3u && pie->nodes[0u].pieValue == 25.0 && pie->nodes[1u].pieValue == 75.0,
          "pie model retains title, numeric values, showData and source order");
    check(piePlan->issue == FlowchartLayoutIssue::None && piePlan->nodes.size() == 3u && Near(piePlan->nodes[0u].pieStart, 0.0) &&
              Near(piePlan->nodes[0u].pieEnd, 1.5707963267948966) && Near(piePlan->nodes[1u].pieEnd, 6.283185307179586) &&
              Near(piePlan->nodes[2u].pieStart, piePlan->nodes[2u].pieEnd) && piePlan->pieCircle.width == piePlan->pieCircle.height,
          "pie sectors have independently calculated quarter, three-quarter and zero angles");
    for (const auto invalid : {L"pie\n\"x\": -1\n", L"pie\n\"x\": 1.234\n", L"pie\n\"x\": 0\n", L"pie\n\"x\": 1\n\"x\": 2\n"})
    {
        const auto rejected = Graph(invalid);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == invalid && rejected->nodes.empty(),
              "invalid pie values retain exact source without partial slices");
    }
    const auto er = Graph(L"erDiagram\nCUSTOMER ||--o{ ORDER : places\nCUSTOMER {\nstring name\nint id PK\n}\n");
    const auto loneEntity = Graph(L"erDiagram\nCUSTOMER\n");
    check(loneEntity->outcome == FlowchartOutcome::Prepared && loneEntity->nodes.size() == 1u && loneEntity->edges.empty(),
          "standalone ER entities retain their source-owned identifier");
    const auto erPlan = Place(er, check);
    check(er->outcome == FlowchartOutcome::Prepared && er->family == NativeDiagramFamily::EntityRelationship && er->nodes.size() == 2u &&
              er->nodes[0u].label == L"CUSTOMER\nstring name\nint id PK" && er->nodes[0u].compartmentCount == 1u && er->edges.size() == 1u &&
              er->edges[0u].erFrom == ErMultiplicity::One && er->edges[0u].erTo == ErMultiplicity::ZeroOrMany &&
              er->edges[0u].fromCardinality == L"1" && er->edges[0u].toCardinality == L"0..*",
          "ER model retains typed entities, attributes and both endpoint multiplicities");
    check(erPlan->issue == FlowchartLayoutIssue::None && erPlan->edges.size() == 1u && erPlan->edges[0u].erFrom == ErMultiplicity::One &&
              erPlan->edges[0u].erTo == ErMultiplicity::ZeroOrMany, "ER endpoint multiplicities reach native geometry");
    for (const auto& [source, left, right] :
         {std::tuple{L"erDiagram\nA o|..|{ B : owns\n", ErMultiplicity::ZeroOrOne, ErMultiplicity::OneOrMany},
          std::tuple{L"erDiagram\nA }o..|o B : owns\n", ErMultiplicity::ZeroOrMany, ErMultiplicity::ZeroOrOne},
          std::tuple{L"erDiagram\nA }|..o{ B : owns\n", ErMultiplicity::OneOrMany, ErMultiplicity::ZeroOrMany}})
    {
        const auto relationship = Graph(source);
        check(relationship->outcome == FlowchartOutcome::Prepared && relationship->edges.size() == 1u && relationship->edges[0u].erFrom == left &&
                  relationship->edges[0u].erTo == right && relationship->edges[0u].style == FlowchartLineStyle::Dotted,
              "ER alternate multiplicity spellings and non-identifying relationships retain typed meanings");
    }
    for (const auto invalid : {L"erDiagram\nA -- B : x\n", L"erDiagram\nA ||--o{ B\n", L"erDiagram\nA {\nstring id\n"})
    {
        const auto rejected = Graph(invalid);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == invalid && rejected->nodes.empty(),
              "unsupported ER syntax retains exact source without partial entities");
    }
    for (const auto direction : {L"TD", L"BT", L"LR", L"RL"})
    {
        const auto pairs = Place(Graph(std::wstring(L"graph ") + direction + L";A;B;C;Z;Y;X;A-->X;B-->Y;C-->Z"), check);
        check(pairs->issue == FlowchartLayoutIssue::None && pairs->edges.size() == 3u, "three disconnected pairs are placed");
        const bool vertical = direction[0] == L'T' || direction[0] == L'B';
        for (std::size_t index = 0u; index < pairs->edges.size(); ++index)
        {
            const auto& edge = pairs->edges[index];
            for (const auto& point : edge.path)
                check(vertical ? Near(point.x, edge.path[0].x) : Near(point.y, edge.path[0].y),
                      "independent pair routes have straight, noncrossing centerlines");
            const auto& target = pairs->nodes[5u - index].bounds;
            check(vertical ? Near(edge.path.back().x, target.x + target.width / 2.0) : Near(edge.path.back().y, target.y + target.height / 2.0),
                  "reordered route still terminates at its original target");
        }
        for (std::size_t first = 0u; first < pairs->edges.size(); ++first)
            for (std::size_t second = first + 1u; second < pairs->edges.size(); ++second)
                check(vertical ? ! Near(pairs->edges[first].path[0].x, pairs->edges[second].path[0].x)
                               : ! Near(pairs->edges[first].path[0].y, pairs->edges[second].path[0].y),
                      "independent routes occupy distinct columns while preserving edge identity");
    }
    const auto labeledPairs = Place(Graph(L"graph TD;A;B;D;C;A-->|label|C;B-->D"), check);
    check(labeledPairs->edges[0].hasLabel && ! Near(labeledPairs->edges[0].path[1].x, labeledPairs->edges[0].path[2].x),
          "labeled pairs retain the exterior label lane");
    for (const auto& [direction, horizontal] : {std::pair{L"TB", false}, std::pair{L"BT", false}, std::pair{L"LR", true}, std::pair{L"RL", true}})
    {
        const auto forkGraph = Graph(L"stateDiagram-v2\ndirection " + std::wstring(direction) + L"\nstate F <<fork>>\nA --> F\nF --> B\n");
        const auto bars      = Place(forkGraph, check);
        const auto& bar      = bars->nodes[0u];
        check(bar.shape == FlowchartShape::Bar && (horizontal ? bar.bounds.height > bar.bounds.width : bar.bounds.width > bar.bounds.height),
              "fork bars stay perpendicular to the selected graph direction");
    }
    const auto choice = Place(Graph(L"stateDiagram-v2\nstate C <<choice>>\nA --> C\nC --> B : yes\n"), check);
    check(choice->nodes.size() == 3u && choice->nodes[0u].shape == FlowchartShape::Diamond && choice->edges.size() == 2u,
          "state choice pseudostates retain diamond geometry and both transitions");
    const auto styled = Place(Graph(L"flowchart LR\nA -.-> B\nB -.- C\nC ==> D\nD === E\nE ~~~ F\n"), check);
    check(styled->edges.size() == 5u && styled->edges[0u].style == FlowchartLineStyle::Dotted && styled->edges[1u].style == FlowchartLineStyle::Dotted &&
              styled->edges[2u].style == FlowchartLineStyle::Thick && styled->edges[3u].style == FlowchartLineStyle::Thick &&
              styled->edges[4u].style == FlowchartLineStyle::Invisible,
          "layout preserves all five edge styles, including the invisible ranking constraint");
    const auto spaced = Place(Graph(L"flowchart TD;A---->B;B-->C;A-->C"), check);
    check(spaced->issue == FlowchartLayoutIssue::None && spaced->nodes[0u].rank == 0u && spaced->nodes[1u].rank == 3u && spaced->nodes[2u].rank == 4u,
          "minimum link length participates in longest-path ranks");
    std::wstring excessive = L"flowchart TD;";
    for (unsigned i = 0u; i < 17u; ++i)
        excessive += L"N" + std::to_wstring(i) + L"----------------->N" + std::to_wstring(i + 1u) + L";";
    const auto beyondRanks = Place(Graph(excessive), check);
    check(beyondRanks->issue == FlowchartLayoutIssue::DiagramExtent && beyondRanks->nodes.empty() && beyondRanks->edges.empty(),
          "accumulated rank constraints beyond bounded geometry fail without partial placement");
    const auto relations = Place(Graph(L"classDiagram\nBase <|-- Derived\nDerived --> Client\nClient -- Contract\n"), check);
    check(relations->edges.size() == 3u && relations->edges[0u].relation == FlowchartRelation::Inheritance && relations->edges[0u].hasArrow &&
              relations->edges[1u].relation == FlowchartRelation::Arrow && relations->edges[1u].hasArrow &&
              relations->edges[2u].relation == FlowchartRelation::Line && ! relations->edges[2u].hasArrow,
          "class inheritance, directed association and plain association retain distinct edge kinds through layout");
    const auto genericClass = Graph(L"classDiagram\nclass A {\n+List~int~ values\n+find(Key~T~ key) Result~T~\n}\n");
    check(genericClass->outcome == FlowchartOutcome::Prepared && genericClass->nodes.size() == 1u &&
              genericClass->nodes[0u].label == L"A\n+List<int> values\n+find(Key<T> key) Result<T>" && genericClass->nodes[0u].compartmentCount == 2u,
          "bounded class generic notation becomes readable angle brackets in its original member compartments");
    for (const auto source : {L"classDiagram\nA : List~int\n", L"classDiagram\nA : List~~\n"})
    {
        const auto rejected = Graph(source);
        check(rejected->outcome == FlowchartOutcome::Unsupported && rejected->source == source && rejected->nodes.empty(),
              "unclosed or empty class generic notation retains exact source without partial models");
    }
    const auto standaloneNote = Graph(L"classDiagram\nnote \"A standalone note\"\n");
    const auto standalonePlan = Place(standaloneNote, check);
    check(standaloneNote->nodes.size() == 1u && standaloneNote->nodes[0].shape == FlowchartShape::Note &&
              standaloneNote->nodes[0].noteTarget == MarkdownNoNode && standaloneNote->nodes[0].label == L"A standalone note" &&
              standalonePlan->issue == FlowchartLayoutIssue::None && standalonePlan->nodes.size() == 1u,
          "standalone class notes retain their label and native note geometry");
    const auto attachedNote = Graph(L"classDiagram\nclass A\nnote for A \"Attached note\"\n");
    const auto attachedPlan = Place(attachedNote, check);
    check(attachedNote->nodes.size() == 2u && attachedNote->nodes[1].shape == FlowchartShape::Note && attachedNote->nodes[1].noteTarget == 0u &&
              attachedNote->edges.size() == 1u && attachedNote->edges[0].relation == FlowchartRelation::Note &&
              attachedPlan->issue == FlowchartLayoutIssue::None && attachedPlan->edges.size() == 1u,
          "attached class notes retain their target, dotted relation and native geometry");
    for (const auto source : {L"classDiagram\nnote \"unclosed\n", L"classDiagram\nnote for A \"\"\n", L"classDiagram\nnote \"x\" trailing\n"})
    {
        const auto rejected = Graph(source);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == source && rejected->nodes.empty() && rejected->edges.empty(),
              "malformed class notes retain exact source without partial graph publication");
    }
    const auto namespaced = Graph(L"classDiagram\nnamespace Domain {\nclass A\nclass B\nA --> B\n}\n");
    const std::vector<FlowchartSize> namespaceNodes(namespaced->nodes.size(), {16.0, 12.0}), namespaceEdges(namespaced->edges.size()),
        namespaceLabels(namespaced->groups.size(), {16.0, 12.0});
    NativeFlowchartLayout namespaceLayout;
    namespaceLayout.Begin(namespaced, namespaceNodes, namespaceEdges, {}, {}, namespaceLabels);
    const auto namespacedPlan = Complete(namespaceLayout, check);
    check(namespaced->groups.size() == 1u && namespaced->groups[0].title == L"Domain" && namespaced->nodes.size() == 2u &&
              namespaced->nodes[0].groupMask == 1u && namespaced->nodes[1].groupMask == 1u && namespacedPlan->issue == FlowchartLayoutIssue::None &&
              namespacedPlan->groups.size() == 1u,
          "class namespace owns its members and has bounded native group geometry");
    const auto grid = Graph(L"graph TD\nC1-->A2\nsubgraph One\nA1-->A2\nend\nsubgraph Two\nB1-->B2\nend\nsubgraph Three\nC1-->C2\nend\n");
    const std::vector<FlowchartSize> gridNodes(grid->nodes.size(), {16.0, 12.0}), gridEdges(grid->edges.size()), gridLabels(grid->groups.size(), {16.0, 12.0});
    NativeFlowchartLayout gridLayout;
    gridLayout.Begin(grid, gridNodes, gridEdges, {}, {}, gridLabels);
    const auto gridPlan = Complete(gridLayout, check);
    check(gridPlan->issue == FlowchartLayoutIssue::None && gridPlan->groups.size() == 3u && gridPlan->nodes.size() == 6u,
          "interleaved two-rank group grid is placed without group collisions");
    if (gridPlan->groups.size() == 3u)
        for (std::size_t group = 0u; group < gridPlan->groups.size(); ++group)
            for (std::size_t other = group + 1u; other < gridPlan->groups.size(); ++other)
                check(gridPlan->groups[group].bounds.x + gridPlan->groups[group].bounds.width <= gridPlan->groups[other].bounds.x ||
                          gridPlan->groups[other].bounds.x + gridPlan->groups[other].bounds.width <= gridPlan->groups[group].bounds.x,
                      "group columns have disjoint independently checked bounds");
    for (const auto source :
         {L"classDiagram\nnamespace Empty {\n}\n", L"classDiagram\nnamespace A {\nclass B\n", L"classDiagram\nnamespace 2Bad {\nclass A\n}\n"})
    {
        const auto rejected = Graph(source);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == source && rejected->groups.empty(),
              "invalid class namespace retains exact source without a partial model");
    }
    const auto shapes = Place(Graph(L"graph TD;A[box];B(round);C((circle));D{diamond}"), check);
    check(Near(shapes->nodes[0u].bounds.width, 40.0) && Near(shapes->nodes[1u].bounds.width, 40.0) && Near(shapes->nodes[2u].bounds.width, std::sqrt(2624.0)) &&
              Near(shapes->nodes[2u].bounds.height, std::sqrt(2624.0)) && Near(shapes->nodes[3u].bounds.width, 80.0) &&
              Near(shapes->nodes[3u].bounds.height, 64.0),
          "each supported shape contains the padded measured label");
    const auto hexagon = Place(Graph(L"flowchart TD;A{{decision}}-->B"), check);
    check(hexagon->issue == FlowchartLayoutIssue::None && hexagon->nodes[0u].shape == FlowchartShape::Hexagon &&
              hexagon->nodes[0u].bounds.width >= 2.0 * hexagon->nodes[0u].label.width &&
              Near(hexagon->edges[0u].path[0u].x, hexagon->nodes[0u].bounds.x + hexagon->nodes[0u].bounds.width / 2.0) &&
              Near(hexagon->edges[0u].path[0u].y, hexagon->nodes[0u].bounds.y + hexagon->nodes[0u].bounds.height),
          "hexagon encloses its label in its straight middle band and attaches at the bottom edge");
    const auto subroutine = Place(Graph(L"flowchart TD;A[[operation]]-->B"), check);
    check(subroutine->issue == FlowchartLayoutIssue::None && subroutine->nodes[0u].shape == FlowchartShape::Subroutine &&
              subroutine->nodes[0u].bounds.width >= subroutine->nodes[0u].label.width + 40.0,
          "subroutine label remains inside the inset vertical rules");
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
    const auto lanesGraph = Graph(L"graph TD;A-->|wide|B;A-->C;A-->D;B-->|back|A;C-->A");
    const std::vector<FlowchartSize> laneNodes(lanesGraph->nodes.size(), {16.0, 12.0});
    std::vector<FlowchartSize> laneLabels(lanesGraph->edges.size(), FlowchartSize{});
    laneLabels[0u] = {100.0, 12.0};
    laneLabels[3u] = {80.0, 12.0};
    NativeFlowchartLayout laneLayout;
    laneLayout.Begin(lanesGraph, laneNodes, laneLabels);
    const auto variableLanes = Complete(laneLayout, check);
    check(variableLanes->issue == FlowchartLayoutIssue::None && variableLanes->edges.size() == 5u &&
              Near(variableLanes->edges[1u].path[2u].x - variableLanes->edges[0u].path[2u].x, 108.0) &&
              Near(variableLanes->edges[2u].path[2u].x - variableLanes->edges[1u].path[2u].x, 16.0) &&
              variableLanes->edges[0u].label.x + variableLanes->edges[0u].label.width < variableLanes->edges[1u].path[2u].x &&
              variableLanes->edges[3u].label.x + variableLanes->edges[3u].label.width < variableLanes->nodes[0u].bounds.x,
          "wide labels reserve only their own lane, on both sides of the graph");
    const auto sharedLanes = Place(Graph(L"flowchart TD;A-->B;B-->C;C-->D;D-->E;E-->F;A-->C;D-->F;B-->D"), check);
    check(sharedLanes->issue == FlowchartLayoutIssue::None && sharedLanes->edges.size() == 8u &&
              Near(sharedLanes->edges[5u].path[2u].x, sharedLanes->edges[6u].path[2u].x) &&
              ! Near(sharedLanes->edges[5u].path[2u].x, sharedLanes->edges[7u].path[2u].x),
          "disjoint exterior rank intervals share a lane while intersecting intervals remain separate");
    const auto sharedBack = Place(Graph(L"flowchart TD;A-->B;B-->C;C-->D;B-->A;D-->C"), check);
    check(sharedBack->issue == FlowchartLayoutIssue::None && sharedBack->edges[3u].route == FlowchartRoute::Back &&
              sharedBack->edges[4u].route == FlowchartRoute::Back && Near(sharedBack->edges[3u].path[2u].x, sharedBack->edges[4u].path[2u].x),
          "disjoint back-edge intervals share a left-side lane");
    for (const auto direction : {std::wstring_view(L"BT"), std::wstring_view(L"LR"), std::wstring_view(L"RL")})
    {
        const auto oriented = Place(Graph(std::wstring(L"graph ") + std::wstring(direction) + L";A-->B"), check);
        const auto& first   = oriented->nodes[0u].bounds;
        const auto& last    = oriented->nodes[1u].bounds;
        check(Near(first.width, 40.0) && Near(first.height, 32.0) && Near(oriented->nodes[0u].label.width, 16.0) &&
                  Near(oriented->nodes[0u].label.height, 12.0),
              "orientation preserves horizontal label and shape dimensions");
        check(direction == L"BT" ? first.y > last.y : direction == L"LR" ? first.x < last.x : first.x > last.x, "each direction changes actual flow ordering");
        for (const auto point : oriented->edges[0u].path)
            check(direction == L"BT" ? Near(point.x, oriented->edges[0u].path[0u].x) : Near(point.y, oriented->edges[0u].path[0u].y),
                  "direct routes remain collinear in every reversed or horizontal orientation");
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
        if (example.number == 91u)
        {
            check(first->issue == FlowchartLayoutIssue::DiagramExtent && batched->issue == FlowchartLayoutIssue::DiagramExtent && first->nodes.empty() &&
                      first->edges.empty() && first->groups.empty() && batched->nodes.empty() && batched->edges.empty() && batched->groups.empty(),
                  "known overlapping reference subgraphs reject complete geometry identically for both budgets");
            ++admitted;
            continue;
        }
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
    check(admitted == 32u, "geometry covers every admitted original reference graph");
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
