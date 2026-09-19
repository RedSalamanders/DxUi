# Disclosure accessibility — baseline

Before the disclosure provider change, the native x64 Debug test
`TestDisclosureButtonExpandCollapsePreservesAcknowledgedState` fails because a Button with
`SetDisclosureExpanded(false)` exposes no ExpandCollapse provider. The earlier action-flow and
multiline changes are present. `accessibility.log` and `accessibility-receipt.txt` retain the exact
failure and executable identity; `performance.json` records the common independent WARP fixture
before this UIA change. The comparison is unpaired and does not waive the open Release memory gate.

Command: `test.ps1 -Platform x64 -Configuration Debug -Suites Accessibility`.
Log: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-disclosure-baseline.log`.
The same test also requires idempotent state requests, property/pattern agreement, disabled-action
rejection, removal of the pattern after clearing disclosure, and safe callback-driven root replacement.
