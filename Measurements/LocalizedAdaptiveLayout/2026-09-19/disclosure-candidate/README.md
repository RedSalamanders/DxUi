# Disclosure accessibility — candidate

x64 Debug Accessibility, Embedded, Control, Rendering, Animation and WindowHost pass with zero
capability skips. These suite receipts/logs correspond to `dxui-disclosure-candidate-v5.log` in
`C:/RedSalamander.Perf/evidence/i26-ui`. The common benchmark is retained here as unpaired;
the separate Release process-memory investigation remains open.

The native regression covers collapsed semantic roots, state/property agreement, idempotent
requests, disabled rejection, clearing the pattern and callback-driven root replacement. Embedded
coverage verifies application acknowledgement after preparation, body visibility/focus recovery,
idempotence and provider disconnection after detach. It uses no application HWND or desktop focus.

Earlier intermediate failures are retained in `dxui-disclosure-candidate[-v2|-v3].log`: root-alias
lookup, stale acknowledged state and stale pattern removal, respectively. The final implementation
refreshes native snapshots in state setters and calls application callbacks outside the snapshot
mutex; it does not touch the control afterward. Embedded snapshots remain caller-published after
coherent preparation. The broader localized short-viewport scene was added afterward and needs its
own current run; these receipts must not be used to claim it has already passed.
