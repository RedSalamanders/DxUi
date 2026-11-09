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
