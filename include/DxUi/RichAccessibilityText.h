#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace DxUi::RichText
{
enum class AccessibilityTextId
{
    EnterSource,
    ExitSource,
    EditingEquation,
    Equation,
    EquationViewFallback,
    EquationCapacity,
    UnsupportedEquation,
    FootnoteName,
    GoToFootnote,
    ReturnToReference,
    ReturnToReferenceHelp,
    TooManyDiagrams,
    DiagramOptions,
    UnclosedDiagram,
    DiagramSourceCapacity,
    DiagramAlternativeCapacity,
    UnsupportedDiagram,
    DiagramCapacity,
    MalformedDiagram,
    EmptyDiagram,
    NativeObjectCapacity,
    StateDiagramHeader,
    ClassDiagramHeader,
    FlowchartHeader,
    DirectionTopBottom,
    DirectionBottomTop,
    DirectionLeftRight,
    DirectionRightLeft,
    DiagramNodes,
    DiagramGroups,
    DiagramConnections,
    DiagramNote,
    DiagramImplements,
    DiagramInherits,
    DiagramPartOf,
    DiagramAggregatedBy,
    StateDiagramHelp,
    ClassDiagramHelp,
    FlowchartHelp,
    EditingBlock,
    DiagramGeometryCapacity,
    UnclosedYaml,
    UnclosedToml,
    InvalidToml,
    TomlCapacity,
    InvalidYaml,
    YamlDuplicateKey,
    YamlUnresolvedAlias,
    YamlCapacity,
    SequenceDiagramHeader,
    SequenceDiagramHelp,
    SequenceSends,
    SequenceAsyncSends,
    SequenceTo,
    SequenceDottedEnd,
    StandaloneDiagramNote,
    EmptyPie,
    EmptyGantt,
    EmptyMindmap,
    EmptySequence,
    EmptyEntityRelationship,
    PieHeader,
    PieSlices,
    PieHelp,
    EntityRelationshipHeader,
    EntityRelationshipHelp,
    IdentifyingRelationship,
    NonIdentifyingRelationship,
    GanttHeader,
    GanttTasks,
    GanttHelp,
    MindmapHeader,
    MindmapHelp,
    UnclosedDisplayMath,
    UnclosedMathFence,
    Count
};
struct AccessibilityTextOverride
{
    AccessibilityTextId id;
    std::wstring_view text; // Borrowed only while constructing the immutable dictionary.
};
// Callback-free configuration that can be shared by preparation and immutable UIA
// snapshots. Entries own their copied strings. Null dictionaries retain English.
class AccessibilityLabels final
{
public:
    static constexpr std::size_t MaximumTextUnits = 1024u;
    AccessibilityLabels() noexcept                = default;
    explicit AccessibilityLabels(std::span<const AccessibilityTextOverride> entries);
    AccessibilityLabels(const AccessibilityLabels&)            = delete;
    AccessibilityLabels& operator=(const AccessibilityLabels&) = delete;
    AccessibilityLabels(AccessibilityLabels&&)                 = delete;
    AccessibilityLabels& operator=(AccessibilityLabels&&)      = delete;
    [[nodiscard]] std::wstring_view Get(AccessibilityTextId id) const noexcept;
    [[nodiscard]] static std::wstring_view Default(AccessibilityTextId id) noexcept;
    [[nodiscard]] static std::wstring_view Text(const AccessibilityLabels* labels, AccessibilityTextId id) noexcept;
    // Footnote labels require {0}; return-reference labels also require {1}.
    // Tokens may reorder, must each occur once and accept no format directives.
    [[nodiscard]] static std::wstring Number(const AccessibilityLabels* labels, AccessibilityTextId id, std::size_t number, std::size_t second = 0u);

private:
    std::array<std::optional<std::wstring>, static_cast<std::size_t>(AccessibilityTextId::Count)> values_{};
};
} // namespace DxUi::RichText
