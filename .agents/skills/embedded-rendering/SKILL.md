---
name: embedded-rendering
description: Implement or inspect host-owned D3D11 integration, content preparation, cached surfaces and composition; not Win32 window presentation.
---

# Embedded rendering

Read [the owning contract](../../../Specs/Rendering/Rendering_EmbeddedD3D11.md) and [AGENTS.md](../../../AGENTS.md).

Separate event-driven preparation from allocation-free composition. Borrow the existing device/context; do not create a device, HWND or swap chain. Test dirty/clean/hidden behavior, two layout variants, hostile prior state, alpha and device loss with WARP. Record all texture and preparation costs.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; owned source awaiting dependency removal is not yet supported at runtime.
