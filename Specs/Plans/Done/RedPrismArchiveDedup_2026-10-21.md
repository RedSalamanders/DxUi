# RedPrism qualification archive content audit (21 October 2026)

Status: completed 21 October 2026 for content audit, eligible storage consolidation and focused verification.

Scope: compare all 754 retained qualification directories by actual content, preserve distinct run provenance,
and remove only verified redundant storage while preserving historical access and live references.

- [x] Capture a fresh no-follow inventory and content hashes; identify whole-archive and file-level duplicates.
- [x] Select and exercise a fail-closed consolidation method that preserves unique bytes, metadata records and paths.
- [x] Consolidate only verified eligible copies; retain unique or ambiguous evidence and record recoverable mappings.
- [x] Revalidate historical payload hashes, live junctions, pointers/worktrees and focused ReFS/NTFS behavior.
- [x] Explain evidence use, retain an audit report, validate guidance and close this plan.

Each run's source/build/scenario identity, logs, results, skips and failures remain distinct evidence even when
payload files repeat. A historical pass only applies under its recorded identity and coverage. Unpaired timing
does not establish a regression verdict. Cleanup must not rewrite outcomes or promote qualification HOLDs.

The local audit ledger is `redprism-archive-content-audit-20261021`. This maintenance changes no native code or
gallery pixels. Existing drive-root permission and no-follow/preservation rules remain authoritative.

## Content findings and preservation

All 754 directories were fully content-hashed: 1,035,715 paths and 1,114,898,954,702 bytes, zero read/reparse errors.
No two complete archive trees are identical. There are 143 groups of repeated source snapshots and 23,575 groups
of repeated file contents. Qualification manifests verified 961,232 entries in 1,452 manifests, with all completion
bindings intact. Vendor manifests inside recovered workspaces are payloads, not qualification manifests.

Consolidation preserves all 754 logical archive folders, every original path, original receipt/manifest bytes and
distinct outcomes. It replaced 9,382 eligible independent copies with read-only NTFS hard links to 4,165 retained
file objects. Removed copy size: 461,172,688,487 bytes. Observed D: free-space increase: 429.513 GiB.
Eligible payloads are at least 1 MiB, lie under verified qualification Sources/Evidence roots, and match actual
SHA-256, length, attributes, last-write time and owner/group/DACL; named streams, special storage attributes,
pre-existing sharing and live references prevent consolidation. Recovery bundles and Git stores stay separate.
Smaller copies and incompatible metadata remain independent; this is not complete byte-level deduplication.

Before every replacement, read/no-write leases protect the original and retained bytes; a durable journal records
metadata, hashes and a temporary rollback hard link. Atomic replacement is verified before the original physical
copy is released. Original per-path creation/access times, attributes and file IDs remain in the before index and
transaction journal. Shared file objects have one creation/access state; historical run provenance remains in the
unchanged receipts. Shared payloads are read-only: restore an independent writable copy before editing.

Nineteen safety fixtures pass, including distinct bytes, alternate streams/security, locked files, reparse/outside
paths, Unicode paths beyond MAX_PATH, read-only writes, copy-out independence, link limits, and rollback before/after
replacement. Earlier fixture failures remain retained, not rewritten as successes. No production payload changed
until these fixtures passed.

## Independent verification and references

After consolidation, every original path was checked against captured identity, size and metadata. All 9,382 aliases
match their intended retained file IDs, and the 4,165 retained objects were independently fully rehashed
(245,487,941,304 bytes); zero errors. All 78,027 directories remain; 3,413 affected parent timestamps were restored.
Unchanged objects retain their captured IDs/metadata; the original full content scan remains their byte record.

All 22 live junctions retain original target and probe-file identities and pass actual reads. Both live pointers
and the relocated worktree verify. All forbidden old roots remain absent. The receipt-gated focused x64 Debug ReFS
case passed, runner exit zero; NTFS produced its exact declared-capability skip, runner exit one with
`ALL_TESTS_SKIPPED`. It is the expected negative control, not passing suite qualification. Stale-run evidence was
preserved before the runner sweep; fresh results/logs remain in the authorized evidence layout.

## Closeout

The report explains how exact source/build/fixture identity, raw results, paired comparisons and recovery snapshots
support scoped checks, qualification and diagnosis. Duplicate payload copies add no test coverage; distinct runs
remain separate evidence. Shared-file use and original metadata lookup are documented beside the retained archives.
Compact transaction/recovery metadata is copied and hash-verified alongside the archive collection; the full ledger
and content index remain under the local maintenance audit ID above.

`Test-Changes.ps1 -BaseRef HEAD -Explain` selects Tooling only. The complete `validate.ps1` run passed all five
validators and all 18 tooling-test scripts, exit zero. `format.ps1 -Check` and `git diff --check` passed; the final
plan/index closeout was rechecked with skill, specification and dependency validators. All repository HEADs remain
unchanged; no native source, validator, threshold, consumer pin, worktree or qualification HOLD changed.

The publication review imports only this completed record, its Done index entry and folded changelog onto current
main. It independently rechecks all 40 compact audit metadata originals against their recorded SHA-256/lengths and
all 40 recovery copies, plus the frozen fixture helper/test hashes. The original full payload audit remains dated
evidence; publication does not repeat that terabyte-scale operation or infer fresh native/resource acceptance.
Current full repository validation and formatting pass; this documentation changes no gallery pixels.
