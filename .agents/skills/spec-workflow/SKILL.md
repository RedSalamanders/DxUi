---
name: spec-workflow
description: Create or reconcile DxUi normative contracts and indexed WIP/Done plans.
---

# Spec workflow

Read [the owning contract](../../../Specs/README.md) and [AGENTS.md](../../../AGENTS.md).

Read Specs/README.md. State implemented support separately from intended acceptance contracts. Plans own sequencing, not duplicate durable requirements. Validate links and indexes. Close a plan only when implementation, tests and normative updates are complete; keep separately deferred consumer/platform scope on HOLD.

Plan evidence anchors (`path:line`, `path:a-b`, a file link followed by `:a-b`, continuation `:a-b`) must resolve at `HEAD`:
`Tools/tests/Test-PlanAnchors.ps1` checks every WIP plan, the file, the line range and a symbol written directly
before the anchor. Write a file name without a directory only for `src/Controls/` files, give a continuation an
earlier file in its item or table row, and repair anchors when cited code moves; ticked items are skipped. The rule
is in [Testing_Validation.md](../../../Specs/Testing/Testing_Validation.md).

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not replace consumer product qualification or deferred native-platform/IME/AT coverage.
