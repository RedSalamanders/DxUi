# Native diagnostic-fixture qualification

Source `1315b4fa5748ec34b146a659edf9a94a928cca57` completes
[CI 35659847692](https://github.com/RedSalamanders/DxUi/actions/runs/35659847692)
in all six native x64/ARM64 Debug, Release and ASan Debug profiles. This qualifies
the foreground classification of menu resource probes and the opt-in text-layout
isolation test; production behavior remains `d461095`.

The first x64 Release attempt timed out while subscribing to disclosure UIA events
(`ElementFromHandle`, `E_PENDING`, 3,000 ms). One job retry passes. Keep the failed
receipt/log in [text-layout isolation](../text-layout-isolation/README.md); this
classifies an intermittent failure, not a repaired library bug.

All nineteen regular suites pass per profile; x64 Release also passes both resource
diagnostics. Nine existing ARM64 Menu desktop-capability skips remain explicit per
profile, with zero x64 skips. All six external consumer checks and both ASan
annotation-disabled variants pass. The intentional ASan fault probes return exit 1
with detection confirmed, which is the expected diagnostic result.

Raw receipts and selected logs are byte-preserved under `raw`, bound by
`sha256.receipt.txt`. Complete artifacts remain at
`C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-1315b4f-final/`.
These native results do not waive resource regressions, prove physical mixed-DPI
consumer presentation, change a consumer pin or close its assistive-technology gate.

**Archive pruning (2026-09-25):** 168 verbose logs and raw receipt copies (2.54 MB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
