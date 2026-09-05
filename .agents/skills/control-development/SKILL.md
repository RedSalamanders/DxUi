---
name: control-development
description: Implement or revise shared retained controls, their state and layout; not application-specific AV or file-manager behavior.
---

# Control development

Read [the owning contract](../../../Specs/UI/UI_ControlsAndLayout.md) and [AGENTS.md](../../../AGENTS.md).

Edit the canonical controls in src/Controls and tests in Tests/Controls in place. Preserve preview/commit/cancel, disabled and acknowledged state. Replace concrete host dependencies through shared services. Preserve source attribution and update the dependency and migration ledgers; never create a second source tree. Cover behavior and bounds, not just appearance.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not prove pending RedXe bridges or later RedSalamander migration.

Update ControlCatalog and the all-control gallery when adding a concrete control. Run its interaction tests and generate the themed sheets with gallery.ps1.
