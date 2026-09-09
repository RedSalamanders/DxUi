# Alternating pairs on a fixed CPU subset

All 32 raw receipts compare unchanged d192e47 baseline and ecad707 candidate
executables on the same DxUi complex WARP fixture. Exact binary/source/workload and
environment identities are in each receipt. The launcher gave every child the
same eight logical processors; this series must be compared within that condition.
No machine-wide power policy, priority, baseline or threshold changed.

Additional launcher context, retained verbatim from the generated context receipt:

```json
{
  "startedUtc": "2026-09-09T15:51:17.5565779Z",
  "completedUtc": "2026-09-09T15:52:18.5331122Z",
  "originalAffinity": 4294967295,
  "measurementAffinity": 255,
  "logicalProcessors": 8,
  "ordering": "Odd pairs baseline/candidate; even pairs candidate/baseline",
  "notes": "Same existing executable/fixture and unchanged thresholds. Child processes inherit the CPU subset. No machine-wide power or priority changes."
}
```

The source launcher/context and descriptive aggregate JSON remain under
`.build/reports/I19-ecad707-cpu-subset` and the I19 governed scratch directory.
All sixteen paired and fourteen repeated-baseline comparisons are retained here.
Five of seven baseline-only comparisons exceed bands in each profile; eleven of
sixteen candidate pairs require investigation. No failing comparison is discarded.

Descriptive medians of the eight per-run medians show Debug dirty composition
+0.61% and Release +0.00%; the earlier +7.9–11.0% Debug signal does not repeat at
that magnitude. These aggregate descriptions do not replace the per-pair verdicts.
Composition allocations, surfaces and hidden work show no growth. Acceptance
remains open pending a stable fixture; see [the qualification report](../README.md).
