# Editor input and viewport — completed scope

Status: complete for EDIT-03/06/09/10 implementation and local qualification.

Metadata-only composition/conversion updates repaint without source/history changes.
Read-only editing shortcuts yield to the application. Delimiter pairing respects Unicode
neighbors, traverses matching closers without history and wraps selections atomically.
Disclosure and view-only zoom preserve the visible source anchor while explicit caret
intent and replacement documents retain their normal precedence.

The lasting behavior belongs to [input](../../UI/UI_InputAndAccessibility.md) and
[editor usage](../../../docs/rich-text.md). The
[source-bound packet](../../../Measurements/EditorInputViewport_2026-11-07/README.md)
contains independent baseline failures, actual WARP pixels/anchors, six zero-warning
builds, 66 ordinary x64 suite executions and actual ASan detection at `efef569e`.

The [current plan](../WIP/RemainingWork_2026-11-04.md) retains other editor findings,
native ARM64 and physical input, performance policy and application adoption.
