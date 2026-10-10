# Accessibility scheduling paired study run

The original statistical interpretation below is superseded by the
[allocation correction](../../../RandomizationDesignCorrection50/README.md): this run constrained the global order
counts to six of each pattern. Correct restricted-design raw p is 0.0021645021645 and six-outcome Holm-adjusted p is
0.0129870129870. Original `study-result.json` and logs remain unchanged. Measured deltas and the accepted first-query
tradeoff remain unchanged; this run cannot seed the independent-order qualification policy.

This directory contains one exploratory paired timing run for `TestAccessibilityPublicationMutationBatchRetainsFinalStateAndReportsCost`. It executes only the `Accessibility` suite with `--no-activate`, the exact named test, and a 300-second test watchdog. Each of 12 independent randomized ABBA/BAAB blocks has two baseline and two candidate processes. Each process must emit seven records for each of the two benchmark scenarios; process medians form the block-level log effects.

The schedule seed and exact order are in `study-result.json`; 48 serial runner invocations are retained as raw `.log` files and individually SHA-256 hashed there. Timing analysis covers the operation, first retained-provider query, and total microseconds for both scenarios, with exact two-sided sign-flip p-values and Holm correction over six timing outcomes. Snapshot-build counters are reported separately without statistical inference.

This is descriptive/explanatory evidence only. It is not a policy acceptance, pass/fail gate, rebaseline, or platform/consumer qualification. A failed test, invalid record, missing identity, or changed pre/post identity leaves the study incomplete or inconclusive.
