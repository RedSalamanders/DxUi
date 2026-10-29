# Stationary submenu hover

Status: complete for F-source functional qualification. Historical red cases and the unassigned d4 Info-row cause remain preserved.

## Completed criteria

- Popup creation over an enabled submenu row with a stationary pointer starts the ordinary cascade delay after the popup is shown; another pointer movement is not required. Keyboard-created selection, disabled rows and leaf rows do not start that pointer timer.
- Keyboard navigation cancels pending pointer cascade timers throughout the popup chain while retaining AltGr behavior. The tested case verifies that a previously queued timer delivery does not fire after keyboard cancellation while no new pointer timer is armed; it does not establish timer-ID reuse behavior after rearming.
- The original Info-row assertion, 80 ms bound and readiness/foreground/owned-pointer preconditions remain. F suite receipts cover the Info-row and keyboard/timer cases; six hosted native profiles and the full local functional matrix passed.
- All six build profiles, 57 zero-skip ordinary x64 executions, the expected ASan probe and 12 local foreground outcomes passed. Shared performance acceptance is not claimed.

The durable contract is in [UI input and accessibility](../../../Specs/UI/UI_InputAndAccessibility.md). The previous failure timeline and unassigned cause are in [the qualification packet](../../../Measurements/QualificationReliability_2026-10-28/README.md).
