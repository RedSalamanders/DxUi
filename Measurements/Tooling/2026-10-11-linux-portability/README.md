# Performance tooling portability, 11 October 2026

PR [#73](https://github.com/RedSalamanders/DxUi/pull/73) first ran at head
`cdb3be2cf68d6444de9e66559db44fcfd75c0781`, base `ec680e866a218b57000a1780018c00101a437f91`,
and actual synthetic merge `a7abc95f3cc3fee9eed3593009790220793d98b3` (tree
`3b6f892a6db4a68e6bd006ec27162e48551b893c`). Run `38163383545`, attempt 1, failed its Ubuntu
validation job `114540799271`: four performance-policy fixture cases and one profiling process-identity case.
The lead cancelled the superseded run after reading the failures. Its incomplete native, consumer and paired
jobs establish no candidate qualification. Raw API/log bytes and the actual checkout/parent/tree mapping are
retained at their own identities.

## Reproduction and repair

An isolated Linux-native Ubuntu WSL clone of the exact published head reproduces policy **29/33** and profiling
**101/102**, including the four bad native paths and the differing absolute process-creation timestamps.
The first path repair improves policy to **32/33** and exposes a remaining setup boundary before full tool
identity can become verifiable: the synthetic full-identity fixture still invokes Win32 DPI and ambient
SystemRoot/WARP probes. That failed intermediate result and a failed isolation probe remain separate.

The policy fixtures keep their Windows project XML while creating host-correct evaluated identities and full
paths that locate the owned fixture bytes. Rejection cases include host-absolute source-selected paths and
identity/full-path mismatches naming two existing owned files. The separate Windows-profile normalization
regression remains. Production policy code and provenance requirements are unchanged.
On non-Windows hosts the synthetic full-identity fixture substitutes exactly one native DPI expression in a
fixture-local copy of the identity function and supplies owned synthetic WARP bytes. All real validation,
tool/dependency hashing and identity assembly remain; removing WARP must fail the unchanged production guard.
Module-scoped calls exercise that copy, and the exact original function and absent/empty/valued SystemRoot
are restored. The test establishes synthetic assembly, not Windows environment qualification.

The profiling fixtures retain the PID plus native creation instance across processes. On Linux they use the
boot ID and exact boot-relative start tick from `/proc/<pid>/stat`; other hosts retain the exact UTC creation
timestamp. Reopening the same process must preserve identity; a different PID or creation instance must fail.
The original direct-child timeout, surviving descendant and bounded pipe-drain assertions remain. Cleanup
checks ownership before terminating its test helper. Production process control is unchanged.

The [kernel proc contract](https://www.kernel.org/doc/Documentation/filesystems/proc.txt) defines the start tick.
The [.NET implementation](https://github.com/dotnet/runtime/blob/main/src/libraries/System.Diagnostics.Process/src/System/Diagnostics/Process.Linux.cs)
reconstructs an absolute start time using a process-local cached boot-time estimate; the before-run records
the actual cross-process mismatch. This attribution concerns the fixture identity comparison, not a native
performance result or a process-control defect in the production runner.

Final focused policy results pass **33/33** on both hosts. The lead's full `validate.ps1` passes all five
validators and every tooling suite on Windows and Linux, including policy **33/33** and profiling **103/103**;
Windows `format.ps1 -Check` passes with clang-format 22.1.3. The two source hashes are recorded in each
root-validation receipt and match the archived after-source bytes. These dirty overlays bind published base
`cdb3be2` plus explicit source/document bytes; they are not relabelled as a future clean Git commit.

One first full WSL run fails because WSL's inherited Windows PATH discovers Windows PowerShell 5.1 and sends
it a Linux-native repository path. That raw failure remains. The successful rerun uses a recorded native Linux
PATH with no Windows parser bridge. CMake and the Windows 5.1 parser are unavailable there; their optional
branches are reported unavailable, while the Windows validation executes its installed-host branches. Hosted
Ubuntu has its own tools and must produce its own fresh result.

Final focused results and full validation are retained in `local/` and bound by `copy.receipt.txt`.
The lead independently checks the agent's path edits and the process-identity repair. Windows and Linux execute
the assertions on their respective hosts; WSL results do not substitute for fresh hosted Ubuntu CI. The official
PowerShell 7.6.6 Linux asset is verified against release SHA-256
`ddbc4a2d113bbd46d283cfedcbcd117a70caefd7673f41f2b4e0000badf103bc`; runtime metadata is retained,
and the external runtime installation is not a supported repository dependency.

## Provenance and limits

`local/` retains the original focused/full logs, overlay source hashes and drivers; `ci/` retains the failed run
metadata and exact API job-log stdout. `source/` retains before/after fixture bytes. API/metadata and archived
source payloads use `.receipt.txt`; they are not canonical native benchmark JSON. `copy.receipt.txt` binds each
copied original by length and SHA-256, and `SHA256SUMS` seals this packet. Earlier native integration receipts
remain under [UiComposition](../../UiComposition/2026-10-11-integration/README.md) at their own source/artifact
identities and are not relabelled as this tooling candidate.

These changes affect two portable tests, documentation and evidence. No control rendering, design-system
preview, native test, workflow budget, performance threshold or consumer pin changes. A fresh exact-head PR
run and independent native/consumer/paired evidence review remain required before remote integration. The
dated [integration decision](../../../Specs/Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage)
retains performance-policy HOLD and individually verified ARM64 refusal before input as unfinished qualification;
it never accepts correctness, sanitizer, provenance or deterministic resource failures or confirmed degradation.
