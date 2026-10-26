# DxUi native and input evidence — 26 October 2026

This packet combines hosted native profile receipts for source `47a9d25995f8dc84b5ca71a2ab75ba405ed8d74a` with a separate local interactive input run. Integration commit `6c3205cf34e2af9b248c5d507caba4db14bab6c4` has the same source tree (`3ee2e12647ca2e05113823eb172bb0facf879021`). The hosted run's measured merge was `c4b0731d3cef3d95eb607bc531bb0bfb8b3e9581`.

## Hosted native profiles

Six provider-bound native profiles completed: x64 and ARM64 Debug/Release, plus x64 and ARM64 AddressSanitizer Debug. Their archives contain 126 full-scope reports (21 per profile), 738 Menu test completions (123 per profile), and 12 snapshot API cases. Both AddressSanitizer probes detected their expected traps. The six `completed.json` manifests report 496 file streams in total. This count is the sum of manifest-declared streams; it is not the number of ZIP entries.

The provider archive digests and executed-binary hashes are listed in [summary.json](summary.json). The six native provider ZIPs retain no executable, DLL or PDB, so their binary hashes are receipt metadata rather than an independent PE rehash. ARM64 receipts do not provide a verified keyboard-focus capture (`foreground-minimized`); cursor state is reported separately. The x64 profiles report unchanged foreground, focus and cursor. The same workflow subsequently fails paired verdict publication because its base policy is unqualified; this native evidence is not an overall green CI result.

## Local input run

Four x64 Release suites completed with exit code zero and no skips: Menu, MenuResources, MenuResourceScaling, and NativeTextInput. Menu logged 123 named tests; NativeTextInput logged 172. The resource suites report suite completion but do not enumerate individual tests in their logs. The Menu run includes `TestMenuPopupAcrylicLightVisualBaseline` and `TestMenuPopupAcrylicDarkPointerParkingClearsHover`.

The local AutoStart lease completed with the configured 30-second confirmation, and recorded foreground, focus, and cursor unchanged at exit. Build was skipped only after the canonical read-only build-attestation gate passed. The run manifest contains 21 entries: 17 file streams, including 4 preserved prior suite receipts; those four are not new executions. The CSV provides safe logical labels, byte lengths, and SHA-256 values for all 17 streams. Raw logs, receipt bodies, images, and executables are not included.

The NativeTextInput suite is not physical IME evidence. The embedded performance report is unpaired, and this packet does not qualify performance or establish a performance improvement.

## Evidence references

[summary.json](summary.json) binds source/tree, hosted provider archive and binary digests, local executable/lease/build-attestation hashes, and the source hashes used for the canonical skip-build admission check. [payload-hashes.csv](payload-hashes.csv) lists only local stream hashes and lengths. The [lead replay](root-independent-replay.json) rehashes the six native ZIPs and all 17 local streams, checks every current suite/lease/binary binding, and independently counts the 123/172 named Menu/NativeTextInput completions. The provider archives and raw local payloads remain in their existing private evidence stores. [Paired performance](../paired-47a9/README.md) remains a separate, inconclusive gate.
