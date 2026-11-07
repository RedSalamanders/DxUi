# Source spacing and radio group names — completed local scope

Status: complete for RENDER-02 and A11Y-01 implementation and local qualification.

Uniform Source line spacing uses retained chunks after an edit. Sequential line
accumulation and baseline mapping before snapping preserve exact height, carets,
hit affinity, range geometry and native pixels, including fractional metrics and
skew. The 59,000-unit fixture prepares 1,062 units and retains 55 chunks after one
character changes. Mixed-style Rich v0 keeps its compatibility path.

RadioButtons Group names prefer the explicit accessible name, visible header and
associated label in that order. Native UIA Name events reflect effective changes;
unchanged headers emit none. Embedded publication preserves the same semantics,
provider identity, selection, navigation and detach behavior.

The lasting contracts belong to [layout](../../UI/UI_ControlsAndLayout.md),
[accessibility](../../UI/UI_InputAndAccessibility.md), the
[rich-text guide](../../../docs/rich-text.md) and [control guide](../../../docs/controls.md).
[Spacing evidence](../../../Measurements/SourceLineSpacing_2026-11-07/README.md)
and [radio naming evidence](../../../Measurements/RadioGroupNaming_2026-11-07/README.md)
retain independent baseline failures and original intermediate failures.

At `21f63d32`, all six profiles build without warnings or errors, all 66 ordinary
native x64 suite executions pass without skips and actual ASan detection fires.
All original source/native/run/failure-image members are independently verified.
The six deterministic gallery captures were regenerated from clean source and
reviewed. ARM64 entries are cross-builds. A11Y-02 semantic state events, physical
input/AT, consumer adoption, native ARM64 and formal performance acceptance retain
their owners in the [single remaining plan](../WIP/RemainingWork_2026-11-04.md).

The owned no-activation Menu cursor fixture also now places its popup above an
owned cover before checking actual pointer ownership. Its genuine foreground
execution remains a fresh hosted gate, not part of the 66 noninteractive runs.
