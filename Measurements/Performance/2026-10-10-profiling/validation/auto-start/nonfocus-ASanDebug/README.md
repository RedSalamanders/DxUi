# x64 ASan Debug noninteractive execution

Clean candidate `02961a88321c75ccd39090873f48977206d6dfdb` runs all nineteen affected noninteractive suites,
without a test filter, through `Test-Changes.ps1` against baseline `869d74e35a7a85ba5af76f460630d313aa98648a`.
Every complete suite passes with zero native capability skips. The five new approved automatic-start lease
cases each start and complete; filter, watchdog and private lease checks pass 8/8, 12/12 and 6/6.

`runner.log`, all native suite logs/receipts, scoped envelopes, the canonical parent benchmark and the complete
actual post-native state are retained. The root snapshot helper checks current v2 build, tool/SDK/dependency and
artifact identity before acceptance. The separate independent root replay checks the archived copy hashes,
ordered positive execution, new case completion and stored envelope digests.

The separate isolated AddressSanitizer probe returns expected exit 1 and diagnoses its deliberate heap use-after-free. All ordinary suites pass; the probe is not an ordinary suite failure.

The parent performance report is an unpaired observation. This leaf does not establish performance acceptance,
real foreground execution, native ARM64 support or consumer adoption. `archive-copy.receipt.txt` binds the
immutable snapshot files; later canonical runs may replace generated originals without changing these copies.
