# Markdown interruption and scan bounds

Status: DONE — implementation and scoped local native qualification.
Implementation: `0ffaa1a6ae43f074f60e419eaa5becc5bc3fe0b3`, API 4.

- [x] Reproduce MD-03/05/06/07 with independent block/source assertions and hostile doubling fixtures.
- [x] Fix display-fence interruption in lazy containers and tables without losing source.
- [x] Bound HTML terminator searches, link nesting and failed table-reference admission.
- [x] Preserve original instrumented failures and distinct storage/tooling attempts.
- [x] Complete all six zero-warning builds, 57 noninteractive suite executions and actual ASan detection.
- [x] Verify original source/native/run archives; regenerate and review all six native gallery sheets.
- [x] Update normative bounds, capabilities, usage documentation and long-checkout regression fixtures.

The [source-bound packet](../../../Measurements/MarkdownScanBounds_2026-11-06/README.md) records 594 actual
compiled inputs, native identities and retained failed attempts. Logical-work bounds are independent of timing
and do not establish performance-policy acceptance. ARM64 cross-builds do not establish native execution.
Other editing, rendering, API and physical/qualification work retains its owner in the
[single active plan](../WIP/RemainingWork_2026-11-04.md).
