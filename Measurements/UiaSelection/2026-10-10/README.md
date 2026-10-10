# Bounded UIA selection-event investigation

The archived ASan Debug failure heard an extra `Selected Ligne 3` notification while republishing an unchanged
Grid selection. Its [original log](../../Layout/2026-10-10/first-Accessibility-ASanDebug.log) and same-binary passing
isolation remain unchanged. The original cause is unproven.

This follow-up preserves the expected event payloads, per-step deduplication and 500 ms quiet interval. It fixes
the client helper's discarded quiescence timeout and records a bounded callback-entry/completion timeline plus
provider raise-attempt timeline for the exact Grid test. An unchanged republish must make zero provider attempts
and yield no client-observed selection events. Provider attempts are not delivery success or payload identity.
The assertion uses the same frozen event cutoff as its diagnostics, before a failure's tree query pumps messages.

`Invoke-Reproduction.ps1` declares ten isolated ASan Debug processes, then one process with the preceding two Tree
tests and the Grid test. It runs through the nonactivating native harness, retains each failure/outlier and stops
at that inventory. Plan/completion and child metadata use `.receipt.txt`; logs remain their original UTF-8 bytes.
The before/after v2 identity binds the source, actual build inputs/tools/dependencies and artifacts; the control
executable receives an additional explicit SHA-256. These filtered tests supplement required profile validation.

The documentation/gallery and design-system contracts were reviewed. These test/reporting changes have no visible
control change, so the existing published captures and design-system guidance remain applicable. Physical input,
assistive-technology acceptance and native ARM64 execution remain separate gates.
