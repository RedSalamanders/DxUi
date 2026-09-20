# Final localized transitions: x64 ASan Debug

All 18 suites pass at commit `54e956eafa9ab0feeecefa40ba67a9f12b7439a3`, including the new localized invalidation, selected-detail and stacked-body clipping tests. The expected-failure ASan detection probe passes. Menu records nine foreground capability skips; the other seventeen suites record none. Raw reports, logs and performance output are retained. These correctness-run performance samples are unpaired and do not replace the retained paired library experiment. Native ARM64 CI qualification and consumer adoption are separate gates.
