# Retained evidence: baseline-repeat

See the [scenario, source identities, results and limitations](../README.md).

Performance JSON retains its original benchmark/source hashes. Native suite receipts
are preserved byte-for-byte as *-receipt.txt, following the existing qualification
archive convention; they are test-execution receipts, not standalone performance measurements.
Unpaired, failed and superseded results remain explicit. No resource waiver or consumer adoption is implied.

**Archive pruning (2026-09-25):** 1 verbose logs and raw receipt copies (5 KB) under this
directory were removed from the main-line copy to bound repository size. Their byte-exact
originals remain at the same paths on `codex/menu-description-layout` commit `36a1c43`;
see the [series index](../README.md).
