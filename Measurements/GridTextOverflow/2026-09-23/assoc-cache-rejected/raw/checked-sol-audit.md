# DxUi warm32 rebuilt evidence audit

Evidence root: `C:\RedSalamander.Perf\evidence\i26-ui\grid-cache-associative-20260923`. These findings concern the two `warm32-*-rebuilt` directories only. The associative candidate remains unadopted.

## Source and rebuild identity

Both `build.log` files specify `Target: Rebuild`, compile `DxUi.Grid.cpp`, and exit 0. Both archived `source` folders match their `inputs.json` hashes. The shared test fixture hash is `45319fbae7f07da183d308c39bb262e6613e407a070657d254fa44353deb4151`; helper hash is `e53ecdefc7df43aca6bc0b0b4c6ab0c2eb0a3f899ac8649cccf64cf147ae65e8`; benchmark hash is `8195a01e2ac04b547977907b5d9f27a1cc122d9be0a08fefacf1dc01d6b3fc89`. Direct-map Grid/header hashes: `02ab680af45e1e7f1415d865eb6f01bbe9bc35812a2235feb0990382547936c1` / `e38c5d4ca4e9c6b2a48ceee3b718e015a40947450a5eb447da5d4165f9847236`. Associative Grid/header hashes: `9d46427c3da43e5eef08bc754f93332214bb947b4dbcff9eaaea827dfb6404ab` / `fce09d59861033b7d50382dc3c68a292ad63a13dc44db8509eb9689281d7e83f`. The top-level `warm-candidate-source` folder has older Rendering/helper hashes; use the rebuilt receipt `source` folders for exact test provenance.

## Functional receipts

- Original direct-map: `tests-exit.txt=1` as expected. `test.ps1` measured performance, then Grid and Embedded passed with zero skips. Rendering failed the first warm assertion after `hits=28 misses=36 layout creations=72`; it did not reach the later no-miss/pixel-equality assertions. The rebuilt original is a valid failing policy witness, not a passing full suite.
- Associative: `tests-exit.txt=1`. `test.ps1` stopped at paired performance comparison before functional suites. `stale-copied-receipts` contains wrongly copied original results and is invalid for candidate qualification. Separate `functional-results.json` records direct native Grid, Embedded, Rendering exits 0 with zero skips; Rendering reports `hits=64 misses=0 layout creations=0` and full pass. These are supplementary functional results only. Candidate performance executable SHA-256 `2b1ded359ea87b0b58bbfc4aee535227ffa4e0665e034260afd0fe9c9b26aa5a` matches supplementary Embedded executable.

## Paired performance

Same `dxui-complex-ui-v2` WARP fixture: 1280x720, 96 DPI, 83 controls, 1000 rows, 5 rounds of 40 frames; both receipts share commit `0ae83620...` but are dirty distinct source fingerprints. Candidate comparison status is `advice-required`. Median dirty private bytes rose 28,643,328 -> 29,659,136 (+1,015,808, +3.546%); dirty private peak rose 28,717,056 -> 29,683,712 (+966,656, +3.366%). Clean prepare p95 rose 0.0003 -> 0.0004 ms and is mechanically marked regressed despite its tiny absolute size. Clean FPS 2,016.92 -> 2,356.64 and dirty FPS 463.50 -> 493.23 improved; C++/composition allocations and surface bytes remain unchanged. These improvements do not waive the memory gate. Runner explicitly asks for quiet-fixture repeat and developer advice before accepting a confirmed regression.

## Limits

The warm fixture is 16 rows x 2 distinct multiline columns, all fitting the declared bounds. The capture helper can repaint both through `RedrawWindow` and `DebugCaptureBitmap`, so 64 cache events are not 64 distinct cells. The assertion `hits>=32` is a coarse repaint witness; original failed there at 28 hits, while its 36 misses establish direct-map churn in the log. The pixel-equality assertion is exercised only by the passing candidate. No allocation attribution or unrun platform/configuration qualification follows from these receipts.
