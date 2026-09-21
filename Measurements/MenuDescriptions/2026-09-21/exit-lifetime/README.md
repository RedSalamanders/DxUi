# Active asynchronous menu at CRT exit

A separate process probe deliberately calls `std::exit(0)` with a stack-owned
owner HWND and an active captured asynchronous menu. It uses the normal
no-activation guard. This exercises thread-local controller destruction before
stack owner cleanup can dismiss the menu normally.

Retained described source `356006a` with the same new test fails under x64 ASan
Debug: `ControlHost::Detach` releases capture, `WM_CAPTURECHANGED` re-enters
`FinalizeAsyncMenuController`, and the ongoing popup destructor subsequently
reads its freed host in `DeactivateNativeTextInputSession`. The baseline has no
production diagnostic edits; its raw ASan trace is retained.

The candidate destructor first closes finalization, ends the owned interaction,
and destroys the popup chain while controller members remain valid. It does not
invoke application completion callbacks during CRT fallback cleanup. The focused
x64 ASan Debug run passes MenuExitLifetime, NewControls, Accessibility and
WindowHost, all with zero skips. The raw suite receipts retain executable hashes
and the linked common-UI benchmark. Other configurations and final qualification
are pending; this is not an additional resource acceptance claim.

Docs/hosting contract reviewed and updated. This lifetime correction changes no
layout, colors, glyphs or gallery scenes, so the existing reviewed gallery remains
applicable. The separate local 1×1 positioning failure is unresolved and is not
explained or fixed by this correction.

Full logs and build records:
`C:/RedSalamander.Perf/evidence/i26-ui/menu-reflow-local-20260921/`.
