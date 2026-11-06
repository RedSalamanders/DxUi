# Markdown command ownership — completed scope

Status: complete for CMD-01/02/03/04/05 implementation and local qualification.

Task edits use the actual first paragraph rather than an empty item-opening line.
Sibling moves decline shared parent-marker ownership. Paragraph moves preserve container
prefixes and decline structural reinterpretation. Footnote labels reject an unmatched
trailing escape. The unreachable empty-math check is removed while valid blank display
math remains supported.

The normative contract lives in [UI controls](../../UI/UI_ControlsAndLayout.md) and
[usage documentation](../../../docs/rich-text.md). Exact source, independent baseline
failures, six zero-warning builds and 66 passing ordinary x64 suite executions are
recorded in the [qualification packet](../../../Measurements/CommandOwnership_2026-11-06/README.md).

Implementation: `c59094f5b9b49c0081164cca95473cac78e57001`, API 5. This completion does
not close EDIT-03/06/09/10, foreground/physical input, native ARM64 execution, paired
performance or consumer adoption. Those remain in the [single current plan](../WIP/RemainingWork_2026-11-04.md).
