# Final x64 Release correctness

All 18 suites pass against the final disclosure-event implementation. The initial execution session
ended during ReadOnly after ten completed suites; the remaining eight ran with `-SkipBuild` against
the same binaries. Both logs, all suite receipts and both unpaired performance receipts are retained.
Menu records eight foreground capability skips; the other suites record zero. Cross-check executable
hashes in the receipts rather than inferring completion from the interrupted initial log.

The benchmark inside these correctness runs is unpaired and does not establish non-regression.
Use [the independent interleaved comparison](../interleaved-final/README.md) for paired evidence.
Native mixed-DPI consumer, real assistive technology and native ARM64 acceptance remain separate.
