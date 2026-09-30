# Native UIA lifetime supplemental evidence packet

Portable, content-addressed supplement to the earlier 2026-09-28 packet. `original-file-map.json.receipt.txt` maps original evidence-relative paths to exact SHA-256 payloads under `raw/`. Run `pwsh -NoProfile -File ./Verify-Packet.ps1` to verify every payload and exact packet membership.

CI workflow 36358394952 completed successfully at `49c963f5751a6754e6b08e0362b96cfc7fc64414` with seven jobs, 122 suite receipts, 8 consumer receipts, and two positive ASan probes. The ARM64 Menu suite receipts retain nine interactive-desktop skips in each configuration; they are not claims that those cases executed. The earlier x64 audit was produced while the workflow was in progress; `run-completed.json`, the root verifications and the completed ARM64 audit establish the final CI state.

The short crossover retains eight 40-frame receipts with their original comparison sidecars and reviewed assessment. The long diagnostic retains eight 1,000-frame receipts, twelve paired/repeat comparisons, all logs, the stock comparator rejection checks, original source backups, driver sources and the restoration/build receipt. Comparator `advice-required` flags remain visible in the raw comparisons; packet assembly does not waive them or claim a performance pass. The long offscreen WARP fixture does not measure WindowHost UIA provider operations or presented FPS.

This packet contains receipts, logs, audit material and selected visual artifacts. Binaries, archives, dependency trees and node outputs are excluded. Source absolute paths in receipts are diagnostic provenance; the packet verifies bytes without requiring those paths to exist.

Repository packaging names the unchanged JSON map with the established .json.receipt.txt suffix: it is an archive index, not a benchmark measurement. Raw payloads and their original hashes are unchanged; the external original packet is retained.
