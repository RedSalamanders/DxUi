---
name: modern-cpp-wil
description: Apply DxUi C++ ownership, Unicode, HRESULT, warning and WIL rules when writing native library code.
---

# Modern cpp wil

Read [the owning contract](../../../Specs/Core/Core_Architecture.md) and [AGENTS.md](../../../AGENTS.md).

Use the source-coordinated public API only within one module. Cross consumer-plugin boundaries through their COM/POD contracts. WIL owns Windows/COM resources; never manually destroy an owned handle. Keep callbacks noexcept with explicit error behavior. Run all required build configurations.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; a reference import is not a supported implementation.
