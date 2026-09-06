---
name: consumer-integration
description: Add or update a pinned DxUi dependency, MSBuild imports and application adapters in a consumer.
---

# Consumer integration

Read [the owning contract](../../../Specs/Build/Build_ToolchainAndConsumption.md) and [AGENTS.md](../../../AGENTS.md).

Use an exact revision lock and isolated output fingerprint. Do not reset or silently update the developer sibling checkout. Match headers, CRT, architecture and compiler. Import project targets rather than listing source files. Test lock mismatch, relocation and independent outputs; keep consumer-specific policy out of DxUi.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not prove remaining RedXe IME/AT bridges or later RedSalamander migration.

Consume only DxUi.lib (API revision 2, lock target DxUi). Restore pinned dependencies into the consumer output root before building. The public example must compile without private headers or consumer test flags.
