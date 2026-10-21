# Drive-root artifact cleanup

Status: DONE, 2026-10-21, for filesystem maintenance and the focused ReFS harness repair.
Cross-repository maintenance authorized by the user. Repository-wide qualification is separate.
Owning guidance: each repository's `AGENTS.md`; ReFS behavior: RedSalamander's
`Specs/Testing/Testing_SelfTests.md`. No product or performance qualification is implied.

- [x] Inventory the named root folders, drive capacity, repository state and active references.
- [x] Obtain explicit authorization for D:/RedPrism.Perf, V:/RedPrism.Perf, S:/RedPrism.Perf,
  D:/DxUi.Perf, E:/DxUi.Perf and S:/DxUi.Perf.
- [x] Add the exact-path, explicit-authorization drive-root output rule to the five primary repositories.
- [x] Preserve complete folders through verified same-volume moves; retain original failures and recovery bundles.
- [x] Repair affected junctions and registered Git worktree metadata; remove the empty ReFS remnant.
- [x] Fix the ReFS self-test to use its acquired sandbox and skip unsuitable filesystems; add regression coverage.
- [x] Run scoped validation, retain all failures and qualification limits, verify the final filesystem, and close this maintenance plan.

Private maintenance receipts live in the existing Z:/src workspace under
DxUi-maintenance/drive-root-cleanup-20261021. The relocation map records every exact source and destination.
Complete no-follow manifests, directory file identities and post-rename comparisons establish preservation;
the procedure never recursively deletes an evidence tree and does not rewrite historical logs.

The additionally discovered D:/DxUiQualificationArchives and Z:/CodexArtifacts contain the same development
artifacts and are included. Preserve dirty primary checkouts, unrelated branches, applications and original
qualification verdicts. Physical paths change; historical receipts do not automatically qualify relocated builds.

## Completed maintenance

All 25 original development root entries are absent: the 23 unique named folders plus the additionally
discovered DxUi qualification archive and Codex artifacts. The 24 payload folders retain 2,523,006 regular
files, about 1.70 TiB in logical file sizes. Before/after complete no-follow metadata manifests match.
Twenty-three folders were renamed directly. Windows refused directory renames in the remaining RedPrism
evidence tree, so every payload file was renamed individually with its original 128-bit file identity,
size, timestamp and attributes checked. Directory timestamps, attributes, DACL, owner and group were
preserved; the 13,191 original directory containers were removed only after verification and only when
empty. Original failed attempts remain recorded. No payload file was copied or discarded.

The repair restores 22 live junctions, one registered detached Git worktree and two mutable latest-path
pointers. Four recreated evidence directory identities are bound to the preserved per-file journal and
matching whole-tree manifest. Original pointer contents are backed up. All eight recovery bundles retain
their original SHA-256 and pass Git bundle verification. Five primary AGENTS files and the retained active
DxUi main/RedPrism P1 worktrees carry the explicit exact-path authorization rule. Old paths in historical
receipts remain historical; each new approved root contains the relocation map and a handoff.

## Validation and limits

RedSalamander's ReFS case now acquires its own marked Compare scratch directory. The volume-discovery
helper is removed. A source-contract negative control fails against the original escape; 211 current
harness source contracts and 78 specification/tool-inventory checks pass. The specification inventory has
zero blocking findings. The current full x64 Debug solution builds. Fresh checks against that attested
build pass on ReFS and record the expected selected-non-ReFS skip on NTFS; neither recreates the forbidden
root, which was removed before these checks. The NTFS controller's skip exit remains explicit rather than
being promoted to a passing native case. The unchanged WatchCallback definition was moved before its owner
to repair a current-toolchain incomplete-type compilation failure; original build failures are retained.

DxUi formatting and tooling pass. Its full validation remains failed on the pre-existing untracked
measurement metadata and 15 consumer path-length findings in the initial dirty checkout; those valuable
records were not rewritten or discarded. Existing performance, native-platform, foreground and consumer
qualification holds remain owned by their existing ledgers. This maintenance closeout does not restamp them.
