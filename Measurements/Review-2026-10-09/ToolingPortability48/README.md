# Tooling portability and first-candidate CI findings

These DxUi-only raw records retain the independently checked failures and fixture repairs after draft PR #70's
first candidate, `ffc64c8947301dfaf2b1e295968a1cc6253bd413`. `verification-manifest.txt` hashes every copied log;
Git attributes preserve their bytes. They do not establish final-candidate native or performance qualification.

Complete Windows validation 47 passed all five validators and every tooling suite with the repaired
PerformancePolicy and BenchmarkGate fixtures. Complete Linux validation 48 also passed, including 8/8
PerformancePolicy, 44/44 BenchmarkGate and 34 scoped-testing cases. Linux used an isolated clone of `ffc64c8`
with only those two repaired fixture files, native `/tmp` storage and a Linux-only PATH. Portable official
PowerShell 7.6.6's downloaded package was independently checked against SHA-256
`ddbc4a2d113bbd46d283cfedcbcd117a70caefd7673f41f2b4e0000badf103bc` before execution.

The earlier Linux 46 attempt is retained as a failure: WSL's Windows-mounted filesystem denied Git-object
fixture copying, and an inherited Windows `powershell.exe` could not parse Linux-path source files. The native
Linux rerun resolves those environment conditions; the Docs and VcpkgTriplet production tools and tests were
not changed. Linux's unavailable Windows PowerShell 5.1 parser and optional CMake evaluation remain explicitly
reported in its log; Windows validation exercises the Windows parser. Linux tooling says nothing about native
Windows C++ or ARM64 runtime support.

The original Ubuntu CI validation failure exposed Windows-specific fixture paths and a hardcoded legacy base
judge version. The repairs use native temporary roots and derive the base version from its immutable Git
source, with a new mismatched-version rejection assertion. Production provenance checks, judge source,
acceptance thresholds and the unqualified policy remain unchanged.

The three first-candidate x64 native CI logs passed all nineteen noninteractive scopes and Menu with zero
capability skips, then failed the same `TestNativeTextInputBackendEditMessagesFallBackWithoutTextInput` assertion.
The expected caption handling was stale: the contract deliberately leaves HWND title messages to the
application window procedure. The corrected fixture checks an actual edit-message fallback, caption decline
and the real HWND default route; a WindowHost regression additionally checks all three application/default
caption routes with and without logical editor focus. Final-source CI must execute that correction.

The corrected source passed the complete noninteractive x64 Debug WindowHost suite in iteration 48 with zero
skips, plus its fresh canonical build, five validators and clang-format 22.1.3 check. The lead independently
recomputed the current build/run receipt identities and verified the executable hash. The raw parent log and
`caption-qualification-48.zip` preserve that scoped result and its source identity. The sealed archive contains
the original native report, scoped run/build receipts and manifest; it is not whole-repository qualification.

The three ARM64 first-candidate CI logs passed the nineteen noninteractive suites with zero skips but failed
the first Menu frame assertion and application TSF activation assertion. ARM64 ASan Menu additionally exited
with `0xC0000409`. These failures do not prove an unavailable desktop or authorize skips. Additional diagnostics
record popup/activation state and reuse the lease's read-only desktop probe in verified hosted foreground runs.
They preserve existing execution and qualification requirements. These additions supersede iteration 48's
exact source identity. The whole Debug WindowHost suite passed again after their C++ changes compiled; complete
Windows validation 49 then passed with the new PowerShell authorization-guard fixture (18/18 cases). Final-source
CI must still qualify all six native profiles and the full foreground suites.
