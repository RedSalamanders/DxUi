# Native clipboard capacity evidence

Work in progress under [I19 library readiness](../../../Specs/Plans/WIP/SharedLibraryReadiness_2026-09-09.md).
Baseline source: d192e474e540adc2656e69b8e8150b0f7a05d63f before the native clipboard capacity fix.
The targeted fixture uses the production transport with private clipboard ownership callbacks and real
HGLOBAL storage; it never changes the desktop clipboard. Each round performs 50 writes and reads of
65,536 UTF-16 units after a warm transfer. Both versions allocate once per write and retain a measured
262,148-byte peak across old and replacement HGLOBAL storage. Five raw rounds are retained in the logs.
The pre-fix run includes the test seam, and fails the new native large-selection case after the first two
boundary cases. The candidate NativeTextInput suite passes in x64 Debug, including 100,000-unit transfers
and embedded overflow rejection.

Common-fixture baseline receipts were captured before implementation in Debug and Release. The original
Release run was disturbed and is retained beside its stable repeat; the repeat is the comparison baseline.
The pre-fix Debug repeat already showed higher process-private memory than the original baseline. The first
candidate benchmark is retained without claiming performance acceptance. Timing and process-memory variation
need matched follow-up; thresholds and baselines have not been relaxed. These measurements do not establish
consumer adoption, native ARM64 runtime, manual clipboard acceptance, or performance closeout.
