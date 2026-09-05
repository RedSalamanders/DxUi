---
name: performance-resources
description: Design or validate DxUi CPU, allocation, cache, surface, wake-up and lifetime behavior.
---

# Performance resources

Read [the owning contract](../../../Specs/Core/Core_PerformanceAndResources.md) and [AGENTS.md](../../../AGENTS.md).

Measure dirty preparation separately from composition. Share immutable resources per device generation. Bound queue/cache/surface growth and replacement peaks. Clean/hidden states add no periodic work. Use source/configuration/fixture receipts; do not substitute draw counts for total cost.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; a reference import is not a supported implementation.
