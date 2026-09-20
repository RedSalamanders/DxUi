# Disclosure — native property notifications

x64 Debug Accessibility and Embedded suites pass with zero skips after completing disclosure
state-change events. A real UI Automation MTA client subscribes to the native semantic root and
receives both acknowledged expansion and collapse. The test pumps the UI owner while the client
subscribes/unsubscribes and retains COM/event ownership through callbacks.

Native event emission uses the canonical root provider when the control is the collapsed semantic
root, preserving its runtime identity. The embedded change detector now includes disclosure state
in its unchanged-record predicate, so a state-only update is not discarded before event emission.
Existing idempotence, disabled-state, callback root replacement and detach checks remain passing.

Command: `test.ps1 -Configuration Debug -Platform x64 -Suites Accessibility,Embedded`.
Log: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-disclosure-events-v3.log`.
Earlier build diagnostics are retained alongside that log. The common benchmark is unpaired;
the prior full Debug/Release receipts predate this event correction and do not qualify it.
