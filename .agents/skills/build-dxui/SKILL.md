---
name: build-dxui
description: Build, test or diagnose the standalone DxUi libraries and consumer outputs; use for toolchain/configuration failures.
---

# Build dxui

Read [the owning contract](../../../Specs/Build/Build_ToolchainAndConsumption.md) and [AGENTS.md](../../../AGENTS.md).

Use root build.ps1 and test.ps1. Select x64/ARM64 and Debug/Release explicitly. Discover Visual Studio with prerelease support. Read the log for the first compiler error. Foundation is the only built target until capabilities changes. Cross-build receipts must not imply native execution.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; owned source awaiting dependency removal is not yet supported at runtime.
