# DxUi native UIA lifetime audit packet

Portable, content-addressed evidence bundle. `original-file-map.json.receipt.txt` maps each source-relative evidence path to its exact SHA-256 payload under `raw/`; raw basenames are short hashes. The mapped audit README explains scope, fixture metrics, source attestation and comparison limits. Run `pwsh -NoProfile -File ./Verify-Packet.ps1` to verify exact membership and all hashes.

This bundle captures the completed x64 matrix, prechange/repeat controls, tests-only red/focus-red evidence, and ARM64 dependency restore plus three completed cross-build logs. ARM64 builds do not establish runtime support. Native CI is still pending for commit `49c963f5751a6754e6b08e0362b96cfc7fc64414` (workflow run `36358394952`); it is not marked done here.

Repository packaging names the unchanged JSON map with the established .json.receipt.txt suffix: it is an archive index, not a benchmark measurement. Raw payloads and their original hashes are unchanged; the external original packet is retained.
