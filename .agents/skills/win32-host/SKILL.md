---
name: win32-host
description: Extract or revise the standalone HWND host, message handling, presentation and teardown; not the embedded RedXe renderer.
---

# Win32 host

Read [the owning contract](../../../Specs/Rendering/Rendering_Win32Host.md) and [AGENTS.md](../../../AGENTS.md).

Keep presentation and OS message ownership in the window adapter. Share controls with embedded mode. Preserve DPI, occlusion, focus and device-loss semantics. Remove RedSalamander global helpers through scoped services and test callback/drain lifetime.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; owned source awaiting dependency removal is not yet supported at runtime.
