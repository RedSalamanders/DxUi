# Consistent Changes workflow across five projects

Status: COMPLETE - tooling and documentation scope, 2026-10-21

Scope: DxUi, RedSalamander, RedXe, RedPrism and RedTilde. This indexed cross-project plan owns the tooling-only change.
The durable contract is `Specs/Core/Core_Changes.md` in each repository.

- [x] Install identical instructions, validation, folding implementation and behavioral fixtures.
- [x] Connect existing validation/tooling entrypoints and PrePush where available.
- [x] Reconcile RedSalamander tooling inventory and normative consistency records.
- [x] Preserve notes, normalize legacy RedTilde fragments and fold every pending tracked fragment.
- [x] Run focused behavior/integration tests, repository validators and format checks; distinguish existing failures.
- [x] Verify shared-file identity, zero pending fragments and preservation of unrelated primary-checkout work.
- [x] Record validation and move this plan to Done.

Fold before final handoff and after every integration, even for one fragment. Validation stays read-only.
No native source, dependency, renderer or visual behavior changes; docs/gallery need no pixel update.
No native performance claim is made. Tooling fixtures qualify the changed behavior.
## Implemented behavior

The six shared files (Changes README, normative contract, validator, fold command, module and behavior runner)
are byte-identical in all five worktrees. Root AGENTS guidance is the same. Existing validation discovers the
common shape check and fixtures; existing PrePush entrypoints reject pending fragments before build/discovery.
RedPrism has no PrePush entrypoint, so its full test entrypoint enforces the same check. Validation never folds.
Folding is explicit before PrePush/final handoff and immediately after each integration, including one fragment.
The tooling performs no staging, commits, pushes, dependency updates or application behavior changes.

DxUi retains its old module import path as a forwarder. The measured harness identity, benchmark path rules and
paired overlay now include the new implementation; a test proves original baseline bytes are restored.
RedSalamander classifies the new tools, maintains exact README maps and its consistency ledger, runs fixtures in
its existing marked sandbox and refreshes the complete reviewed PR execution digest. Consumer API-revision advice
now points to CHANGELOG.md plus pending fragments. Existing release-note history was not migrated.

## Validation

| Project | Actual successful evidence |
| --- | --- |
| DxUi | 19 shared behavior cases; full validate.ps1 (all five validators and all tooling suites); format.ps1 -Check; paired overlay 31/31; scoped runner 34 cases. |
| RedSalamander | 19 shared behavior cases; 33/33 focused Pester cases (Changes, inventory, governance and API-note diagnostics); 8 scoped-policy cases; tool inventory and specification inventory with zero blocking findings; stable current PR execution digest. |
| RedXe | 19 shared behavior cases; complete independent tooling runner, including 30 scoped cases and four Python metadata tests; skill validation. |
| RedPrism | 19 shared behavior cases through validate-skills.ps1; all 15 skills validated. |
| RedTilde | 19 shared behavior cases; complete validate.ps1, including link/index, skill, pin/capability, build inventory, PowerShell syntax and performance-protocol fixtures. |

Across the five projects: 95 shared case executions passed, all changed PowerShell files parsed, and git diff --check
passed. The six common file hashes match. Every worktree passes Fold-Changelog.ps1 -Check with zero fragments.
The public final-runner case starts each real entrypoint in an isolated fixture and proves its pending-fragment
refusal occurs before build/test discovery. Additional RedSalamander coverage proves reparse refusal leaves the
target untouched. No native source/project/pin changed; native builds/runtime/platform/performance acceptance are
not claimed or rerun for this tooling-only slice. Docs/gallery were reviewed; no pixels or product usage changed.

The initial broader validation found two task defects: the RedSalamander README maps omitted new entries, and
DxUi's paired driver omitted the new module from its overlay. Both were fixed, with passing focused and final
validation. Initial native-format attempts outside DxUi were not qualified: RedSalamander has format-all.ps1 rather
than format.ps1; RedXe's formatter is mutating and has no -Check; RedPrism's existing native invocation failed with
StandardOutputEncoding; RedTilde's formatter requires an unrestored pinned source. These are unchanged native
formatter surfaces, with no C++ edits in this task. PowerShell syntax and changed-file whitespace checks passed.

## Folded content and isolation

Folded 108 inputs: 98 in DxUi (97 prior notes plus this change), seven in RedTilde (six prior notes plus this change),
and one workflow note in each of the other three projects. Independently compared every prior DxUi fragment's
complete text with the generated changelog and every prior RedTilde content line after format normalization.
No prior note text was lost. No unrelated primary-checkout fragment was folded.

Implementation was prepared and locally validated on ej/changes-workflow-2026-10-21 in isolated worktrees.
The user subsequently authorized commit, push and merge; Git and the pull requests own publication status:

| Project | Worktree |
| --- | --- |
| DxUi | C:\Users\eric\.codex\worktrees\changes-workflow\DxUi |
| RedSalamander | Z:\src\RedSalamander-worktrees\changes-workflow-2026-10-21 |
| RedXe | Z:\src\RedXe-worktrees\changes-workflow-2026-10-21 |
| RedPrism | Z:\src\RedPrism-worktrees\changes-workflow-2026-10-21 |
| RedTilde | Z:\src\RedTilde-worktrees\changes-workflow-2026-10-21 |

Other work continued in the primary checkouts during validation, including AGENTS updates and a RedSalamander
branch change. This task did not reset, switch, stash or write those primary checkouts. Before publication, the
concurrent drive-root output policy from each primary AGENTS.md was copied into its feature branch and verified
verbatim alongside the Changes guidance. Primary HEAD, branch, index, status and dirty/untracked file hashes were
recorded privately for preservation checks. Other primary work is excluded from these commits.
These local validations are not a remote CI or merge result.
