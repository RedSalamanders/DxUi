---
name: consumer-integration
description: Add or update a pinned DxUi dependency, MSBuild imports and application adapters in a consumer. Use when a consumer adopts DxUi or moves its exact revision lock, changes how it restores, imports or links DxUi.lib, or wires the application-owned failure host and adapters before using DxUi controls.
---

# Consumer integration

Read [the owning contract](../../../Specs/Build/Build_ToolchainAndConsumption.md) and [AGENTS.md](../../../AGENTS.md).

Use an exact revision lock and isolated output fingerprint. Do not reset or silently update the developer sibling checkout. Match headers, CRT, architecture and compiler. Import project targets rather than listing source files. Test lock mismatch, relocation and independent outputs; keep consumer-specific policy out of DxUi.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not replace consumer product qualification or deferred native-platform/IME/AT coverage.

Consume only DxUi.lib at the API revision and commit the consumer's lock names (lock target DxUi; `main` is API revision 5, and an API 4 consumer stays on its API 4 pin or the `maintenance/api4` line until it moves), and call or import only the consumer interface `capabilities.json` lists (see the contract's compatibility section). Restore pinned dependencies into the consumer output root before building. The public example and frozen revision-4 and revision-5 API fixtures must compile without private headers or consumer test flags. Install and register the application-owned failure host before constructing controls or entering DxUi scopes; keep the host and module registrations alive until all workers and controls have stopped.
