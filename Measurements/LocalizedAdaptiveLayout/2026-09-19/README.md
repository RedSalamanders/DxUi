# Localized adaptive layout baseline

Pre-implementation Release baseline for the common independent `dxui-complex-ui-v2` WARP fixture.
Source: `b125edbf4cdd639d1cabd686f9dc3fe8aab1f374`; no library source changes. The dirty flag records
planning documentation. The receipt records exact compiled-input, executable and fixture hashes.

Command: `performance.ps1 -Configuration Release -Platform x64 -OutputPath Measurements/LocalizedAdaptiveLayout/2026-09-19/baseline-release.json`.
The command rebuilt the library and test executable; it did not reuse an unattested old binary.

- [Raw five-round receipt](baseline-release.json).
- [Unpaired comparison result](baseline-release.json.comparison.json).

This is a retained baseline, not a non-regression pass. It measures completed offscreen frames,
not presented desktop FPS or application behavior. The targeted localized-layout fixture and its
matching before/after measurements remain L0 work. Run candidates serially on the same fixture,
configuration and machine; preserve this original rather than silently rebaselining.
