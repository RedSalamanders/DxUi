# Localized layout — complete x64 Debug suite run

All 18 requested suites returned zero; Menu retains nine explicit interactive-desktop capability
skips and the other 17 suites record zero skips. This run uses the final shared short-viewport
fixture (including the action-label bounds assertions), logical DPI transitions, disclosure
pattern/state/lifetime checks and multiline paint tests. It predates the additional native UIA
event notification/subscriber work, whose qualification is separate.

Command: `test.ps1 -Configuration Debug -Platform x64`.
Log: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-full-debug.log`.
Suite logs and receipts retain exact executable hashes. The common benchmark is unpaired here;
no Release performance or physical mixed-DPI acceptance is inferred.
