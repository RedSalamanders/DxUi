# Approved automatic Start qualification, 11 October

The clean test-harness candidate is `02961a88321c75ccd39090873f48977206d6dfdb`, against frozen baseline
`869d74e35a7a85ba5af76f460630d313aa98648a`. The user approved the foreground test time and requested Start as
the no-click outcome. `test.ps1 -Interactive -InteractiveAutoStart` offers a visible 30-second countdown;
Cancel, Esc, close and interruption stop it. The ordinary manual mode keeps its Cancel default. Native receipts
record actual start action, mode and countdown independently of the request, rejecting missing, contradictory
or duplicate approval metadata. The desktop is checked again after confirmation and restored after execution.

| Qualification | Retained result |
| --- | --- |
| x64 [Debug](build-x64-Debug/README.md), [Release](build-x64-Release/README.md), [ASan Debug](build-x64-ASanDebug/README.md) builds | All three pass, complete actual v2 provenance |
| ARM64 [Debug](build-ARM64-Debug/README.md), [Release](build-ARM64-Release/README.md), [ASan Debug](build-ARM64-ASanDebug/README.md) builds | All three cross-builds pass; no native runtime claim |
| Noninteractive [Debug](nonfocus-Debug/README.md), [Release](nonfocus-Release/README.md), [ASan Debug](nonfocus-ASanDebug/README.md) | Nineteen complete suites each, 57 passes, zero native skips |
| Foreground [Debug](focus-Debug/README.md), [Release](focus-Release/README.md), [ASan Debug](focus-ASanDebug/README.md) | Four complete suites each, 12 passes, zero skips, verified restoration |
| Private lease/filter/watchdog checks | 6/6, 8/8 and 12/12 in each x64 profile; nine new private confirmation proofs required |
| Automatic-mode tooling | 32/32 cases, including ten malformed result variants and local/manual compatibility |
| [Default performance pair and controls](performance/README.md) | Original comparison advice-required; two fixed variability reports retained; acceptance open |

Every foreground run records clicked Start under the approved automatic mode (`started`, actual/requested
automatic policy true and countdown 30). The no-click expiry is separately exercised by native private-desktop
regressions; these foreground receipts are not relabelled `autostarted`. All four suites execute unfiltered,
with exact ordered canonical plan/executable/log bindings. The full actual post-run source/tool/SDK/dependency/
artifact state matches before acceptance. Foreground and keyboard focus remain unchanged; the pointer is restored.
Both ASan parents correctly diagnose their separate deliberate isolated heap use-after-free probes.

The six builds and 69 ordinary suite executions use compiled source fingerprint
`B0A180F7917AAE43B5F442C3D4E8D513B46F15A91E239D5EC9E3B18B367DAE6F`. The lead independently checks the
delegated findings and replays [the archived review](review/README.md). Exact original/snapshot/archive copy
bindings, complete logs, scope/build envelopes and current actual native provenance are retained. The x64 Debug
console-copy wrapper fails after its successful canonical build; the canonical MSBuild file log and explicit
wrapper exit remain separate. ARM64 evidence establishes cross-compilation only. Optional CMake evaluation in
the tooling triplet check is unavailable because CMake is absent from PATH; this does not imply a native skip.

The first production-wrapper reader accepts six malformed results. [Iteration evidence](iteration/README.md)
retains the red/green corrections; final tests reject ten corruptions. Independent review also strengthens the
external focus reviewer against duplicate policy/action fields and eight malformed full-suite plans, preserving
focused negative proofs and their explicit non-native scope. Earlier failed/canceled native attempts remain
retained in the parent packet and qualify their own source only.

The frozen before/candidate Default comparison flags ten timing/FPS metrics: clean FPS 2748.971 to 2358.171,
dirty FPS 666.189 to 569.974, both about 14% lower. Two candidate-only same-binary controls are declared before
execution and retain their complete raw rounds. Variability leaves the original flags unresolved; it does not
identify a cause, replace baseline-side controls, authorize a new baseline or qualify the seed policy. No frozen
judge, threshold, required-CI hold or exact resource budget changes. All native parent benchmarks are unpaired
observations. Native ARM64 foreground execution, physical IME/AT/touch, consumer adoption and required-CI
acceptance stay separately open; the original intermittent UIA/setup failure causes remain unproved.

Public controls, layout, theme, design-system tokens/previews and published gallery pixels are unchanged. The
documentation/gallery contract is reviewed; this private test-dialog workflow requires no gallery regeneration.
The domain contract and usage guidance document the approved mode. The complete production plan remains WIP
because its separate qualification and consumer obligations remain unfinished.

All 801 prior packet hashes match before additions. Their original seal and unchanged analytical bytes are
preserved in the review leaf. Each new profile leaf explains scenario/source/scope and binds immutable copies;
`SHA256SUMS` covers this subpacket and the parent seal covers the expanded inventory. Final repository validation
and formatting are run after reconciling the archive and prose; they supply no new native execution.
