# Measurement archive repair (21 October 2026)

Status: completed 21 October 2026 for evidence placement, preservation and repository validation.

Scope: restore the reviewed-measurement boundary for the 13 October main qualification packet, preserve its
raw evidence, and remove redundant source folders. Library behavior and qualification acceptance are unchanged.

- [x] Inspect the 541 metadata and 15 path-length findings and the packet's existing publication/privacy decision.
- [x] Inventory and hash all originals; copy to an explicitly authorized project archive and verify every byte.
- [x] Retain an auditable public inventory and explanation; remove only verified source copies and empty directories.
- [x] Clarify the documentation contract, validate the affected scope and run the complete validation and format gates.
- [x] Record preservation and cleanup results, then move this plan to Done.

The raw packet contains CI suite/child/status receipts, local successes, the original timeout, and diagnostic pilot
reports. These are useful historical evidence, but are not all standalone measurement-schema receipts. Preserve
their original bytes and paths privately rather than inventing workload ownership or benchmark-input hashes.
The existing public summary and all performance/foreground/platform/consumer HOLDs remain authoritative.
No runtime, visual or gallery pixel change is involved; native suites and gallery regeneration are unnecessary.

## Preservation and cleanup

Private archive `dxui-main-e0260dde-raw-20261021` retains 396 originals (22,529,203 bytes), including the original
README, all successes, the timeout, CI receipts, captures and diagnostic pilot. Every original source/archive
length and SHA-256 matched before removal; an independent reread verified all 396 archived originals afterward.
The [public packet](../../../Measurements/Performance/2026-10-13-main-qualification/README.md) now contains its
summary and a CSV inventory of 395 raw files (22,525,454 bytes); all entries match the private inventory.
The CSV's SHA-256 is `F7D63433C33D5FA190557EE5232F0B199790A55C7665082C635378B50F464AB2`.

Removed only those 395 verified redundant source copies and 25 empty source directories. No junctions or registered
worktrees were inside the packet, no executable was running there, and unrelated tracked edits were verified
unchanged. The longest remaining packet path is 77 characters. No new drive-root directory was created.
Raw receipts were never edited; the separate historical archive referenced in the original summary was not reverified.

## Validation

- `Test-Changes.ps1 -BaseRef HEAD -Explain`: Tooling only; no native input changed.
- `validate.ps1`: all five validators and the complete 18-script tooling suite passed, exit zero. The 541 measurement
  metadata and 15 path-length findings are resolved without changing validators or fabricating receipt metadata.
- `format.ps1 -Check` and `git diff --check`: passed.
- Final plan/index closeout: rechecked with the specification, dependency and skill validators.

The documentation contract and Measurements guidance now state the distinction between genuine reviewed receipts
and private raw archives. Local audit ledger `measurement-repair-20261021` retains inventories, preservation script,
pre-change Git state, independent verification and full validation logs. The existing library/consumer acceptance
HOLDs remain owned by their existing plans.
