# Window hosting

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The WindowHost adapter owns HWND integration and its own swap-chain presentation only in window-host mode.
It uses the same retained control implementation as embedded mode, with a separate scheduling/presentation adapter.
Device loss, resize, zero-size suspension, DPI, focus and teardown have explicit contracts and regression tests.
Its message/timer/animation resources stop or quiesce when hidden and are destroyed by their owning runtime.

The imported WindowHost is a reference, not a standalone supported binary. It currently depends on RedSalamander
helpers, messages and animation dispatch. D1 removes those dependencies and migrates the existing window-host
tests before advertising support. RedSalamander's application migration remains a later independent plan.
