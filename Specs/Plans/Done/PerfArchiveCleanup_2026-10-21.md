# Verified .Perf archive and cache cleanup (21 October 2026)

Status: completed 21 October 2026 for verified redundancy removal and focused regression verification.

Scope: inspect existing project `.Perf` roots, remove only proven redundant data while preserving useful evidence,
and revalidate live junctions plus the focused RedSalamander ReFS sandbox case. No library behavior changes.

- [x] Inventory existing roots without following reparse points; record active processes, worktrees and junction targets.
- [x] Classify duplicate/obsolete candidates and verify retained equivalents before guarded removal.
- [x] Preserve an audit ledger, unique source/build/runtime evidence and active baselines/caches.
- [x] Verify junction targets/identities and exercise ReFS pass plus unsuitable-filesystem skip through the authorized runner.
- [x] Validate documentation closeout and record exact cleanup/retention results before moving this plan to Done.

Age, matching names and reproducibility assumptions alone do not establish redundancy. Original failed receipts and
distinct qualification snapshots remain evidence. Deletion must stay inside the exact reviewed directory, must not
follow junctions, and must not affect a running process or registered checkout. Use existing approved roots only.
The local audit ledger is `perf-redundancy-audit-20261021`. This maintenance changes no pixels or native library code;
gallery regeneration and unrelated full native suites are unnecessary. Product qualification HOLDs remain separate.

## Verified results

The no-follow inventory covered 13 roots, 3,298,560 files and 1,786,240,541,049 logical bytes, with zero scan errors.
All 1,649 encountered reparse points were indexed without walking their targets; archived test-fixture links were
left intact. Removal candidates were screened against these targets, all 22 repaired live junctions and registered
worktrees. Age or equal size alone never authorized deletion.

Removed 40 obsolete `ActualBuild`/`ActualLink` cache copies whose complete file sets matched retained
`BuildArchive`/`LinkArchive` counterparts, plus six duplicate top-level download ZIPs. Every file and alternate
stream matched before removal. Immediate open-handle hash guards protected both copies during deletion; all 679
retained counterparts were rehashed afterward. Removed 114 directories and 18,699,216,805 logical bytes
(17.415 GiB). Per-root consolidation manifests retain source-to-archive paths, original metadata and SHA-256.
The removal plan hash is `E4A0EDBF2346E90F1EA774C6EF659A41AF07AFC53B38E980AB80F092789CC6C7`.

Kept all 754 RedPrism qualification snapshots, distinct receipts, source/recovery archives, active output products
and dependency caches. Eleven otherwise duplicated downloads were retained because of a live target or registered
checkout overlap. Remaining possible overlaps are not asserted to be fully deduplicated.

All 22 live junctions retained their target identities. Reads through each alias resolved to the same file identity
as a direct target read. The two latest-path pointers and relocated worktree are checked separately in the ledger.

The receipt-gated x64 Debug runner executed the focused
`local_index_core_refs_probe_and_query_if_available` case without desktop activation. On ReFS it passed, runner
exit zero. On NTFS it produced the exact declared-capability skip, “Selected authorized test sandbox is not on ReFS.”
The filtered NTFS run deliberately has no passing cases, so its runner reports exit one, `ALL_TESTS_SKIPPED`;
this is retained as the expected negative control, never a green suite qualification. Neither run recreated the
forbidden legacy root. All 133 files (157,854 bytes) from six stale runs were copied and hash-verified before the
runner's normal stale-run sweep, and independently rechecked afterward. Fresh results and logs are retained in
the authorized evidence layout.

## Repository closeout

`Test-Changes.ps1 -BaseRef HEAD -Explain` selects Tooling only. The complete `validate.ps1` run passed all five
validators and all 18 tooling-test scripts, exit zero. `format.ps1 -Check` and `git diff --check` passed. The final
plan/index move was rechecked with the skill, specification and dependency validators. These checks add no new
native-platform, product or trusted performance qualification. No validator, native source or threshold changed.

All five repository HEADs remained unchanged. Live latest-path pointers still match their recorded hashes and
existing targets; the relocated detached worktree retains its recorded HEAD. No branch, worktree or recovery
bundle was removed, and no independently launched application was terminated. The existing drive-root policy
already owns the durable inventory/preservation/reference-verification requirements used by this cleanup.
