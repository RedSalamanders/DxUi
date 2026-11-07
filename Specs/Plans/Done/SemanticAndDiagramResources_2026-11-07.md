# Semantic and diagram resources — completed local scope

Status: complete for SEM-02/03/04/05 and FLOW-03/05 implementation and local qualification.

Unchanged image uploads and mathematics survive completion/animation snapshots. Authored
text determines paragraph direction, container headers retain inherited list markers,
and supplementary letters/digits share native, semantic and UIA word boundaries.
Final diagram notes cannot publish geometry crossed by an edge or its caption. Identical
styled edge captions share preparation within one bounded diagram generation.

The lasting contracts are [control layout](../../UI/UI_ControlsAndLayout.md),
[input and accessibility](../../UI/UI_InputAndAccessibility.md) and the
[rich-text guide](../../../docs/rich-text.md). Independent retained-baseline failures,
source/image hashes and the complete source-bound local matrix are in
[semantic resources](../../../Measurements/SemanticResources_2026-11-07/README.md) and
[diagram resources](../../../Measurements/DiagramResources_2026-11-07/README.md).

At `a42caaa0`, all six profiles build without warnings or errors, all 66 ordinary
native x64 suite executions pass without capability skips, and the real ASan detector
fires. All 683 original source inputs and retained native/run/failure-image members
are independently verified. The six harness gallery captures were regenerated from
that clean source and reviewed. ARM64 entries are cross-builds.

Native ARM64, physical input, consumer adoption and formal performance acceptance retain
their owners in the [single remaining plan](../WIP/RemainingWork_2026-11-04.md).
