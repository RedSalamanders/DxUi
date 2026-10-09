# Running the accessibility scheduling study

`Invoke-PairedSchedulingStudy.ps1` is a reusable, PowerShell-only measurement driver for the already-built x64 Debug
baseline and candidate named in `source-input-manifest.json`. That preserved setup receipt remains the default. Pass
`-SetupManifest <path>` to use a separately refreshed receipt after rebuilding both overlays; the driver never edits
either receipt. It performs no restore or build. Run it only after the owner has declared a quiet measurement window;
the script itself does not claim that the machine is isolated or that the result qualifies a release.

Before running, it verifies that the compiled source closure matches between the two roots except for exactly the
retained `src/Controls/DxUi.Accessibility.cpp`, the baseline-only test project registration, and its diagnostic adapter.
It checks the adapter and differing-file hashes from the selected setup receipt. Both roots must also have a valid
scoped x64 Debug build receipt whose build-input and artifact identities match their current `Get-ScopedBuildInputIdentity`
and `Get-ScopedArtifactIdentity` results. The build-input identity combines compiled repository source with the platform's
installed vcpkg triplet files and required status, package inventory, and ABI metadata. The selected receipt, study script, source closure, scoped build
receipts, and resolved tool identities are checked again after the run; metadata or compiled-input mutation makes the
study inconclusive.

The driver records `Get-PerformanceToolIdentities` for baseline and candidate before and after execution. An
unverifiable identity or a mismatch in the effective MSBuild/SDK/toolchain, dependency closure, harness, or environment
stops the study. The separate import inventory records where named PE imports can be resolved through the executable
directory, system directories, PATH, and Visual Studio redistributables; it is not evidence of which modules the process
actually loaded. The performance identity's resolved build/dependency hashes are the authoritative tool identity here.
Instrumentation flags reported by `Get-ScopedInstrumentation` must be inactive. Every benchmark record's
`uiaListening` value must remain the same across all 672 repetitions on both sides or the timing analysis is withheld.

The driver invokes only `TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost` from the
`Accessibility` suite with `--no-activate` and a 300-second test timeout. It imports the candidate's shared paired
schedule and statistical helper modules, requests twelve balanced randomized ABBA/BAAB blocks, and records the seed
and exact order. Each of the 48 serial processes must exit successfully, start and complete that exact test, and emit
fourteen valid JSON records: repetitions 0–6 for both `keystroke-batch` and `selection-batch`, each with 128 operations.
The test's assertions include checking the first retained-provider query after every mutation batch. Logs are written
directly to files by the child process; the driver's capture path does not pipe or transform runner stdout.

For each process and scenario, the driver takes medians across seven repetitions. It then calculates one candidate-over-
baseline log effect for each of the twelve independent blocks using the two within-block process medians per side. The
six outcomes are operation time, first-query time, and total time for each scenario. Their exact two-sided sign-flip
p-values are Holm-adjusted as one six-outcome family. Snapshot-build counts are summarized separately and receive no
statistical inference. The report retains all raw logs and hashes, schedule, per-process medians, block effects, and
pre/post source, fixture, binary, dependency, toolchain, runtime, and environment identities. The baseline-only link
adapters are disclosed from the unchanged setup receipt.

Outputs go beneath this folder's `Runs/<UTC timestamp>-seed-<seed>/`. An incomplete process, malformed or missing record,
test failure, unavailable identity, inconsistent UIA listening state, instrumentation, or pre/post identity drift
prevents a complete explanatory result; partial logs are retained for diagnosis. The script never changes the setup
receipt or any other source/build metadata.

This is exploratory paired evidence only. It does not run A/A calibration, qualify or migrate the performance policy,
make a pass/fail decision, change thresholds, or authorize a rebaseline. It also does not provide Release, ARM64,
ASan, consumer, or interactive qualification.

## Refreshing the baseline overlay

`Refresh-AccessibilitySchedulingBaseline.ps1` is a separate source-refresh/build-only phase for the named baseline
worktree. The candidate must already have a valid scoped x64 Debug build receipt. The helper copies candidate compiled
inputs into baseline, then checks exact input parity while preserving the retained pre-A1 library source and the
baseline-only test project/adapter. It refuses baseline-only stale compiled files instead of deleting them. It invokes
canonical `build.ps1 -Platform x64 -Configuration Debug -Rebuild` in baseline,
checks source/artifact identities around that build, and publishes the standard scoped build receipt required by
`Test-Changes.ps1 -SkipBuild`. It also writes a separate `build-only-attestation.json` whose outcome explicitly says
no test suite ran; the receipt is build evidence, not test evidence.

Only after these checks does it create the new
`source-input-manifest.refresh-2026-10-09-v2.json`. Before overlay, it requires matching toolchain, dependency, and
environment identities while allowing the old baseline harness identity to differ. After overlay and build, all four
resolved performance identities, including harness, must match. The original `source-input-manifest.json` and retained
`Accessibility.cpp.txt` are never changed. Pass the versioned manifest to the paired driver with `-SetupManifest`; do
not execute the refresh helper until the source owner has frozen the candidate and scheduled the baseline build.

The original inventory names Hostx64 compiler/linker files, while its hash-verified MSBuild log records Hostx86
invocations. The refresh preserves that original receipt, verifies its inventoried files remain unchanged and that
the retained log names the currently resolved compiler/linker paths, and records an explicit provenance erratum.
It does not infer historical Hostx86 binary hashes. A full rebuild prevents reuse of objects from that incomplete
historical attestation; current resolved tool, dependency, environment and harness identities must still match across
both roots before and after the rebuild.

The archived `Accessibility.cpp.txt` remains byte-identical to the original receipt. Its compiled baseline copy
needs exactly two disclosed call-site bridges after shared posted-payload hardening: the old `(lp, hwnd)` calls
become `(hwnd, msg, lp)` for provider creation and UI actions. The helper requires exactly one occurrence of each
original expression, records both transformations and the compiled-copy hash, and refuses any other baseline
source change. Scheduling, snapshot construction, provider queries and mutation logic are unchanged by these
bridges. They add the current payload's message identity check; this explanatory study does not qualify the old
implementation's full accessibility behavior.
