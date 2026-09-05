---
name: control-development
description: Implement or revise shared retained controls, their state and layout; not application-specific AV or file-manager behavior.
---

# Control development

Read [the owning contract](../../../Specs/UI/UI_ControlsAndLayout.md) and [AGENTS.md](../../../AGENTS.md).

Check the imported control and tests before extracting a slice. Preserve preview/commit/cancel, disabled and acknowledged state. Replace concrete host dependencies through shared services. Keep the import immutable and update the migration ledger. Cover behavior and bounds, not just appearance.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; a reference import is not a supported implementation.
