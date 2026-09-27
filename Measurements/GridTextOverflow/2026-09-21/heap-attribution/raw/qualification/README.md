# Diagnostic harness qualification

With original V11 production restored, Grid/Embedded/Rendering pass in x64
Debug, Release and ASan Debug (nine suite passes, zero skips), including the
ASan detection probe. All three ARM64 configurations cross-build only.
Suite/probe receipts retain their original JSON bytes as `.receipt.txt`.
Performance JSON files are unchanged linked common-fixture receipts and are
explicitly unpaired. They do not resolve the resource acceptance gate.
See the [parent investigation](../../README.md) for source/fixture ownership,
matched diagnostic comparisons, rejected experiments and remaining gates.
