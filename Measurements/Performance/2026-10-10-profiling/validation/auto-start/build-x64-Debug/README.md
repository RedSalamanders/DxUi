# x64 Debug build

The automatic-start test-harness candidate is clean commit `02961a88321c75ccd39090873f48977206d6dfdb`.
The canonical build passes. Its actual 26-field state, compiler/SDK/dependency/artifact checks and exact copied
raw receipts are retained in `root-review.receipt.txt`. Both build envelopes bind the captured bytes.

This leaf establishes a build. Native execution is retained separately in the noninteractive and focus leaves.

The console-copy wrapper failed after the successful build. `build.log` is the canonical MSBuild file log (zero warnings/errors); the root review records wrapper exit 1 separately from the successful build.

The source fingerprint is `B0A180F7917AAE43B5F442C3D4E8D513B46F15A91E239D5EC9E3B18B367DAE6F`.
`archive-copy.receipt.txt` binds each immutable external snapshot file to this leaf. Mutable generated receipt
paths describe capture time; subsequent canonical runs may replace those originals.
