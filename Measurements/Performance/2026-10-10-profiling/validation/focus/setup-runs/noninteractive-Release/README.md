# Corrected diagnostic binaries: noninteractive validation

This snapshot retains one affected x64 Release run on commit `5763d57e` with the corrected test-only focus
setup inputs present in the dirty working tree. It is separate from the earlier clean-head receipts. All nineteen
unfiltered native noninteractive suites pass with zero skips; the canonical entrypoints check actual source,
dependency, toolchain and artifact bytes. The root reviewer checks current scope/build/tooling digests, each complete
log, current executable hashes and all actual post-native provenance fields against the parent performance report.

The parent FPS/memory report is unpaired. It establishes no performance non-regression, stable control policy or
consumer qualification. Native foreground suites, ARM64 execution and physical IME/AT acceptance remain separate.

`copy.receipt.txt` records the original generated files and exact bytes at capture time. `archive-copy.receipt.txt`
binds those immutable external copies to this repository archive. Generated originals may be replaced by later runs;
the retained bytes are not rewritten. The private reviewer source is historical evidence, not a portable runner.

MenuExitLifetime deliberately exits after its `[EXIT]` marker; its successful process exit and valid current scoped receipt establish that boundary without requiring the ordinary runner completion line.
