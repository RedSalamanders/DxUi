# I61 local native validation — 2026-11-09

This is evidence for the organization testing-acceleration plan owned by RedSalamanders/.github, not complete DxUi or organization qualification.

## Source and command identity

The measured source is parent `0d12c464638ddc41f48d521a4a598a50870117d2` plus the patch inside [candidate-source.zip](candidate-source.zip). [source-identity.json](source-identity.json) binds the exact archived patch bytes and the built Control/Embedded executables. The subsequent CI engine-pin adoption is separate from this measured source. Complete original run receipts, native artifacts and matching EXE/PDB archives remain under `.build/evidence/test-runs/` at the exact run IDs in [scheduling-comparison.json](scheduling-comparison.json).

Build: `./build.ps1 -Configuration Release -Platform x64` passed with zero warnings/errors. Run the same non-foreground suite set with:

```powershell
./test.ps1 -Full -Configuration Release -Platform x64 -SkipBuild -SkipTooling -ParallelSuites 1
./test.ps1 -Full -Configuration Release -Platform x64 -SkipBuild -SkipTooling -ParallelSuites 2
```

Both runs passed all 19 suites with zero capability skips, identical binary hashes and identical ordered sequences for 1,726 named control cases. Foundation/Embedded are unchanged-binary full invocations; their aggregate checks are retained separately. The serial run took 441,376.81 ms; two workers took 335,173.65 ms: 24.06% less elapsed time, or 1.32x. This is one full pair, not a median, p95 or the plan's 3x/2x acceptance. Performance and harness checks preceded each batch serially. No foreground suite was requested. ARM64, Debug/ASan, consumer/SDK and foreground qualification are separate and remain open.

## Exact local preflight reuse

[preflight-measurements.json](preflight-measurements.json) records two alternating warm/fresh pairs against the same native bytes. Warm receipt validation took 1,029.83 and 920.15 ms; forced filter/watchdog verification took 23,262.95 and 23,178.77 ms. Every forced run passed 8 filter and 12 watchdog controls. These timings isolate the prerequisite, not the complete test run. Full and hosted execution remain fresh. Source/dependency/binary/host/environment changes, instrumentation, expiry, failure and mutation are independent refusal controls in the scoped-tooling tests.

## Preserved failing baseline and repair

The [prior serial attempt](prior-failed-serial.json) failed in Embedded (`0xc0000409`) and remains failed. Original run: `20261109T130229552Z-81776-d3ff2e9c1cd84dbeb130432cab0e8b22`. Its archived `ordinary.record` and matching PDB resolve `PreparedLayoutAccess::Prepare` at `DxUi.RichTextLayout.cpp:447`, reached from `TestRichDocumentGeometryMaximumStorageBudget`. DirectWrite's overhang query raised an internally handled C++ exception outside the preceding GetMetrics allowance. The repair applies the existing per-call native boundary to GetOverhangMetrics, preserving HRESULT refusal and observation of SDK work/reentrant scopes. The subsequent complete Embedded and 19-suite runs above passed. A passing retry does not replace the original failure.

Hosted normal concurrency remains one until native architecture comparison is accepted. Current local and main qualification coverage has not been removed.

## Selected native target union

[selected-builds.json](selected-builds.json) and [selected-builds-source.zip](selected-builds-source.zip) retain
the S2 source patch, input hashes, requested/executed targets, cold output hashes, observed results and refusal controls.
The cold x64 Release operation built exactly DxUi, FoundationTests and EmbeddedTests in 148 seconds with zero
warnings/errors. It produced only their six library/executable/symbol files, with no ControlTests, sample or lease
image. Foundation and Embedded then passed from those cold outputs; Embedded observed 583,358 checks and both
suite processes exited zero. The cold orchestration preceded the final solution-mapping/output guard additions;
native C++ inputs were unchanged, and the default-output path subsequently exercised the final guards.

The default-output selected operation passed in 192 seconds including canonical pre/post attestation. Direct
test.ps1 and the public affected entrypoint each admitted Foundation with zero capability skips and retained
ordinary unpaired performance observations. A matching public follow-up printed REUSED Foundation in about
9.4 seconds, with no native suite execution. Actual Full and Grid requests using this selected proof both refused
before native run evidence started. These are subset acceptance and identity costs, not Full/profile qualification
or a controlled full-versus-selected speed ratio. Other architecture/configuration, native full and runtime
deployment acceptance remain open.

Hosted first-wave source 5f65003cd94fdd45bfd47255b17d5a5eac498b11 failed its x64 tooling prerequisite in run
[42631977298](https://github.com/RedSalamanders/DxUi/actions/runs/42631977298): the same two stale watchdog-call
assertions found by the local full validator. Its raw job log is preserved in `.build/i61-hosted-first-wave-x64.log`.
The earlier failure remains failed; the corrected 9/9 and 48/48 assertions are in follow-up source d462a6ff and
the S2 candidate. Hosted acceptance requires a new run of the integrated follow-up source.

## Affected iteration follow-up

The profile iteration baseline has 41 focused tooling cases passing, including default no-work, later-edit scope
selection, staging already-tested bytes, distinct staged content behind unchanged working bytes, cumulative
Full/PrePush/foreground obligations, changed native context, failure revocation, ordinal path identity, expiry,
malformed state and separate-process profile exclusion. The real canonical x64 Release compiler/SDK/dependency
context resolved successfully and matched across two scans. Diagnostic scans of source/index, native context and
all profile artifacts took 23,850.0 and 19,808.8 ms; native context accounted for 15,550.4 and 15,448.3 ms.
These scans measure identity cost only, not an executed native affected/no-work campaign or a speedup.
Raw local diagnostics remain in `.build/i61-iteration-identity-measurements.json`.

Complete `validate.ps1` executed all validators and tooling scripts. It exposed two stale source-contract assertions
expecting a direct watchdog call after the first-wave helper extraction. They now check the canonical preflight's
ordering, retaining actual child invocation/failure controls in the scoped tests. Focused reruns passed 9/9 suite
failure and 48/48 interactive-mode cases; the full run's original failure remains recorded separately in
`.build/i61-iteration-validation.log`. Formatting, spec/skill checks, changelog checks and the final 41-case scoped
run passed. Hosted review and native final-head/profile acceptance remain separate.

## Release library input handoff

[sdk-library-inputs.json](sdk-library-inputs.json) records an actual x64 Release diagnostic at committed source
754cd827ffc1757f2a36b4972ecb4e67ccf7a498. The governed production-only build completed in 159,241 ms with zero
warnings/errors. Export verified complete v2 inputs and copied the library/PDB in 33,807 ms. Import recomputed
inputs, verified/restored the original bytes and omitted library compilation in 31,744 ms. The source fallback
callback deliberately throws if invoked in this exact-hit diagnostic; it was not called.

Export plus import cost 65,551 ms versus the 159,241 ms source build in this single local diagnostic: 58.8%
less elapsed time for these library-input stages. This excludes hosted artifact upload/download, runner setup
and dependency restore, and is not an overall pipeline speedup or a controlled multi-pair performance result.
The original library/PDB hashes and complete native identity remain in the record; no test/Full receipt is reused.

The extracted SDK consumer's real compilation/link/run and negative controls passed in 76,386 ms: 243 headers
stayed inside the extracted SDK or selected toolchain; license closure, exclusive producer-PDB isolation,
missing packaged PDB and first-party warning controls all passed. The one deliberate warning is confined to
the expected rejection control. Ordinary native compilation retained zero warnings/errors.
The diagnostic's numeric local run/attempt metadata is synthetic. It proves local input verification/routing,
not authenticated hosted provenance or ARM64 acceptance. Both hosted regression architectures and package
qualification remain required on the final integrated source.

The broader validator exposed two integration controls after this diagnostic: SDK build-helper changes needed
benchmark-scope ownership, and the native workflow assertion needed to admit only the optional current-attempt
SDK download's source fallback. Required build/test/package failures remain fatal, with seeded refusal controls.
Those corrections and later evidence documentation have separate source identities; they do not relabel this
earlier native run as final-head qualification. Original logs remain `.build/i61-sdk-native-{build,package,diagnostic}.log`
and `.build/i61-sdk-library-validate.log`.

## Complete native metadata queries

[metadata-queries.json](metadata-queries.json) compares the original 200-property batch evaluator at ef3ecaf0
with the response-file candidate identified by its exact module SHA-256. All 1,105 property values and the
complete normalized profile match for both library and benchmark projects in x64/ARM64 Debug, Release and
ASan Debug: twelve comparisons, with alternating original/candidate order. These are installed-MSBuild
evaluations on an x64 machine, including cross-target ARM64 metadata; they do not compile or run ARM64 code.

Across this single diagnostic matrix, total library evaluation fell from 33,337 to 25,256 ms (24.2%); benchmark
evaluation fell from 17,050 to 10,799 ms (36.7%). Startup/cache state varies and other tooling was active, so
these phase observations do not establish a controlled multi-pair or overall pipeline speedup. No native input,
property, profile, toolchain or byte check was removed. The real profile/include-resolution regressions also pass.
Independent query controls compare 1,502 properties beyond Windows command-line limits against original batches,
exercise Unicode and item-dependent values, forbid target execution and reject malformed/missing output,
nonzero/failed process launches, switch injection, duplicate names and foreign output paths.
