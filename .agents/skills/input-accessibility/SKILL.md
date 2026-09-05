---
name: input-accessibility
description: Implement or verify pointer capture, keyboard, text/IME and UI Automation across DxUi hosts.
---

# Input accessibility

Read [the owning contract](../../../Specs/UI/UI_InputAndAccessibility.md) and [AGENTS.md](../../../AGENTS.md).

Cover capture loss and gesture cancellation before commit. Text requires composition and OS focus ownership, not only key events. UIA providers must survive or disconnect safely on control detach. Hidden/modal-background controls leave navigation. Test real IME, touch and assistive technology in addition to synthetic events.

Validate changed guidance with `validate-skills.ps1` and `validate-specs.ps1`. For source/build work run the affected
`test.ps1` configurations and the additional validation named by the contract. Supported capabilities are recorded
in `capabilities.json`; library tests do not prove pending RedXe bridges or later RedSalamander migration.
