# focus-asan-failed

The first c2d05357 full ASan foreground run failed the IME setup; other suites and all restoration results remain retained.

The canonical report is the test runner's unpaired synthetic complex-UI observation in ASan Debug x64, at source c2d05357fc65156ba9a16f38bdc8d316fbd5f369. Its original source/tool/dependency/artifact/environment fields and raw bytes are retained. It establishes no paired non-regression or benchmark-policy acceptance. See the [integration explanation](../../README.md) for the fixture, archived attempts and remaining gates.
