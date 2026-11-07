# Input-state continuity — scoped completion

Status: DONE for PRIOR-25/26/27 functional repairs.
Implementation: `a287710cc0230152a69da0c74496d654841a05b3` (API 5).

NumericStepper terminal/binding semantics, all-collapsed Grid keyboard expansion and innermost forwarded-drag
cancellation are implemented in canonical controls/native and embedded hosts. Maintained regressions cover
LTR/RTL, exact event counts, preview rollback, nested forwarding and callback retirement. Durable requirements
are merged into UI_ControlsAndLayout, UI_InputAndAccessibility and usage/hosting docs. No gallery pixels changed.

The [packet](../../../Measurements/InputStateContinuity_2026-11-07/README.md) records six zero-warning builds,
66 passing ordinary x64 suite executions, actual ASan detection and independently verified original archive
bytes. It retains baseline/first-candidate failures, a refused recompile and unchanged-object diagnostic proof.
Disk-admission stops remain explicit; only unexecuted profiles continued. Separate tooling deadlines keep their
original evidence and owner. Native ARM64, physical/IME/AT, paired performance, consumer adoption and broader
admission remain in the [single parent plan](../WIP/RemainingWork_2026-11-04.md); it is still active.
