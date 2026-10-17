# D2c retained rich-text preview

Before-source-change Release x64 Default baseline at clean 1c6a660a.
The existing synthetic library workload is unchanged and does not instantiate
RichTextEditor. before.json is unpaired baseline evidence, never non-regression
acceptance. The initial SkipBuild attempt was refused for stale attestation (observed tool
response; its redirected file was empty). The MSB4166 worker failure raw log is
retained separately from the successful diagnostic retry. Candidate functional,
resource, gallery and paired-comparison results will be recorded after execution.
No historical D2b performance warning is silently closed by this measurement.

## Exploratory failures retained

The first editor run found that the reusable layout rejected ControlHost's valid
empty DirectWrite locale. The implementation now accepts that setting and has a
named real-layout/RTL regression. The next run passed all nine new editor tests
and then failed an existing 33-control catalog assertion. The embedded exploratory
run exercised the new resources until its Escape fixture generated an unintended
double click at the same coordinates. The catalog assertions and fixture gesture
are updated before final qualification. These exploratory logs are not full-suite
or attested final-source passes. Builds attempted while code/test/design inputs
were still being edited failed attestation and are not counted as successful
qualified builds. A fresh frozen-input matrix follows.

A final source review added an explicit hidden-root Paint guard: native and
embedded hosts can paint a concrete control directly as root. The new tests
prepare that hidden root, verify its layout stays retired and its text pixels
vanish, then verify pixel-identical restoration without a new view surface. The
earlier Debug/Release 19-suite passes are preliminary; the final six-profile
build/native qualification is rerun for this guard and its regression tests.

The static component preview selector is scoped as .dx-field.dx-rich-editor so
the existing generic field rule cannot override its inset/height/alignment. This
stylesheet input is included before the final attested build pass; it changes
no native gallery renderer pixels. Earlier build attempts invalidated by source/
preview edits are not reused as final attestation.

## Final bounded-v0 qualification

[qualification.json](qualification.json) binds the exact frozen library fingerprint,
fixture hashes, six successful build attestations, executable/library/runtime hashes,
57 fresh ordinary native suite receipts and their raw logs. All 19 suites pass in
x64 Debug, Release and ASan Debug with zero capability skips. Each EditorControls
run completes 20 named rich-text scenarios (core plus retained editor). Embedded
passes 5,082 checks including real rich-text WARP pixels, shared views, no-allocation
warm composition, hidden-root/view/zero-size suspension, failed preparation, owner
retirement and device replacement. The ASan receipt includes an actual isolated
heap-use-after-free diagnosis; it is not inferred from compiler flags.

ARM64 Debug/Release/ASan Debug are cross-builds only. No native ARM64, foreground,
physical IME or assistive-technology qualification is claimed. Rich-text OS text
composition and rich UIA ranges are D2d and remain unimplemented. The original
concurrent checkout and all consumer pins are unchanged.

All five validators and the full tooling suite pass through validate.ps1, and
clang-format 22.1.3 passes. gallery.ps1 -PublishDocs regenerated five populated
themes for 34 controls plus the supplied-device example; all six images were
inspected. Raw suite receipts retain their original .json.txt member names and bytes in
[native-receipts.zip](native-receipts.zip), with each member hash in qualification.json.
The archive was verified against every original receipt before removing duplicate
text copies. Plain suite/build/tooling logs remain directly readable under Raw/.

## Serial Release diagnostic

[performance-summary.json](performance-summary.json) retains both post-change
comparisons against the original before.json. The first returns advice-required
for clean prepare p95 (0.0003 to 0.0004 ms), dirty frame median (1.6533 to 1.7528 ms)
and dirty composition CPU p95 (0.0251 to 0.0265 ms). The one quiet repeat returns
within-noise-budget. Neither result establishes the cause of the first warnings
or controlled paired acceptance. D4c remains open; no baseline was replaced.
This fixed Default fixture does not instantiate RichTextEditor and is not a new
editor stress benchmark. Normal test snapshots may overlap compilation and are
functional diagnostics, not the authoritative serial comparison here. Historic
D2b warning receipts remain unchanged.

| Scenario / metric | Before | First | Quiet repeat |
| --- | ---: | ---: | ---: |
| clean / fps | 2313.047903 | 2236.260972 | 2380.569789 |
| dirty / fps | 564.2497031 | 563.250972 | 587.7305373 |
| dirty / frameP50Ms | 1.6533 | 1.7528 | 1.6577 |
| dirty / privatePeakBytes | 30113792 | 29491200 | 29245440 |
| dirty / composeAllocations | 0 | 0 | 0 |
