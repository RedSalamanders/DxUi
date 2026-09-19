# Localized layout — x64 AddressSanitizer qualification

The complete ASan Debug invocation passed the deliberate sanitizer detection probe and the first
16 functional suites (Foundation through Animation). Menu records nine foreground-capability
skips; the other completed suites record zero skips. The execution session disappeared during
Accessibility at 12:18 UTC; no final Accessibility receipt was written. Its partial log is retained
as `Accessibility-interrupted.log`. Old September 12 receipts are not reused as current evidence.

The unfinished Accessibility and WindowHost suites were resumed with the same built binaries
using `test.ps1 -Configuration 'ASan Debug' -Platform x64 -SkipBuild -Suites Accessibility,WindowHost`.
The full invocation and resume logs are retained under
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-full-asan.log` and `dxui-asan-resume.log`.
Both resumed suites passed with zero skips; their current receipts/logs and separate common benchmark
are retained. Together the two invocations cover all 18 suites, with the nine Menu skips still explicit.
No sanitizer diagnostic occurred in the functional suites. ARM64 execution and consumer acceptance
are separate; this is native x64 qualification only.
