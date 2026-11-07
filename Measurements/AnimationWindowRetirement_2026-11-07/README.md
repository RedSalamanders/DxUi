# Native animation window retirement

Implementation `06b8d5188d063d16aa27f91eaa1092e3cc136a55` (API 5) releases the module's cached animation
HWND when its last native host detaches. Siblings keep their subscriptions and warm idle cache. A timer callback
finishes before its dispatcher window is destroyed. A focus/capture retirement callback cannot rearm animation
on a detaching host. Embedded scheduling and the public ABI are unchanged.

The [qualification ledger](qualification.json) records six zero-warning builds, all 22 ordinary suites in each
x64 profile, actual ASan detection, and byte verification of original source, native images, failure EXE/PDB
closure and canonical run archives. ARM64 entries are cross-builds. The [counterexamples](counterexamples.json)
bind the same compiled public consumer objects before and after the fix. The original SDK leaves its HWND alive
and exits with the SDK fatal tag during thread teardown. The first candidate fixes ordinary retirement but
fails the separate reentrant blur oracle; the final guard passes both unchanged-object oracles.

The original RedTilde API-5 Recovery process passed all 19 cases before its late teardown failure. Its preserved
WER record and exact original EXE/PDB identify MissingHost in the dispatcher window procedure during TLS
destruction after reporter uninstallation. That diagnosis stays separate from unrelated historical failures.
The first SDK setup refusal, initial tooling dependency failure and first candidate remain retained.

Durable requirements belong to [native hosting](../../Specs/Rendering/Rendering_Win32Host.md),
[failure ownership](../../Specs/Core/Core_FailureHandling.md), [resources](../../Specs/Core/Core_PerformanceAndResources.md)
and [usage](../../docs/hosting.md). No default gallery pixels changed. Physical input, native ARM64 execution,
paired-performance acceptance, fresh hosted results and explicit consumer adoption remain source-specific
owners in the [active parent plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
