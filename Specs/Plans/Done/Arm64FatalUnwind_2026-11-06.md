# ARM64 fatal-origin verifier and preflight retention

Status: DONE — implementation, tooling and source-bound local qualification.
Implementation: `3a5ae6b0ee109118572a3358f974a3ee41f264e5`, API 5.

- [x] Use the PAC-aware ARM64 walker and required target-attribute callback with explicit availability,
  native-machine and original process-identity admission. Preserve saved context/thread, original PE/PDB
  bindings and the exact named throw origin.
- [x] Preserve failed-preflight dumps, records and child output. Keep previous failures separate and stream
  the current original EXE/PDB files into a member-verified compressed archive.
- [x] Exercise 58 evidence fixtures, including a live independently named executable, wrong/redirected
  profiles, current/prior failure retention and actual compressed member identities.
- [x] Complete six zero-warning builds, all 66 ordinary noninteractive x64 suite executions and actual ASan
  detection. Verify the original 681-file snapshot and every retained native/run/failure-image archive member.
- [x] Retain the original storage refusals and output-process inspection refusal. Reverify four completed
  profiles before the exact remaining two-profile continuation; preserve both original progress ledgers.
- [x] Pass the complete compiler-free validators and formatting, and update authoritative validation and
  tool documentation.

The [packet](../../../Measurements/Arm64FatalUnwind_2026-11-06/README.md) preserves the scope and exact source.
The ARM64 entries here are cross-builds. Native ARM64 saved-origin execution and the complete hosted
test-before-production gate remain with [D-Q1](../WIP/RemainingWork_2026-11-04.md#d-q1), alongside its
separately owned physical/foreground gates. Earlier output-less hosted failures keep their unknown cause.
