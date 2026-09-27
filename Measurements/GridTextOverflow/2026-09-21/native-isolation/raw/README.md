# Raw native-isolation evidence

These bytes belong to the [rejected grid experiments](../README.md). `provenance.txt`
lists the 47 external source files and SHA-256 digests. Benchmark `.json` files
retain their actual fixture/source/executable fingerprints; test receipts retain
their original bytes as `.receipt.txt` to distinguish them from performance receipts.

`no-draw.patch` and `no-trimming.patch` intentionally produce incomplete output.
`bounded-inline-newline-failed.patch` fails the visible-ellipsis pixel assertion.
`bounded-marker.patch` fixes that failure but does not improve retention resources.
All patches apply to the V11 grid source and were reverted after diagnosis.

Run `py -3 summarize.py` here to reproduce `summary.txt` from the raw reports.
The paired original/V11 paced reports remain in the adjacent heap-attribution
packet; they are not replaced by the short control or the restored unpaired test
benchmark. Common benchmark receipts referenced by all three test entrypoints
are copied here, alongside their original comparison status.
