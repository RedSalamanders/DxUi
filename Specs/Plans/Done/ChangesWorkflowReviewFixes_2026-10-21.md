# Changes workflow review fixes across five projects

Status: COMPLETE - shared tooling implementation and local regression scope

Scope: the shared tooling in DxUi, RedSalamander, RedXe, RedPrism and RedTilde. The current contract is
`Specs/Core/Core_Changes.md` in each repository. Original implementation history remains in the completed plan.

- [x] Reconcile the latest concurrent AGENTS heading layout without modifying primary checkouts.
- [x] Recognize tab-separated release headings when limiting duplicate detection to Unreleased.
- [x] Bound synthetic child/pipe waits and terminate an owned child tree before timeout cleanup.
- [x] Add independent regression fixtures and copy identical shared files to all five repositories.
- [x] Revalidate relevant tooling and fold fix fragments for reviewed publication.
- [x] Record local implementation evidence and prepare reviewed fix commits; Git/PRs and the private integration receipt own publication status.

Review identified two valid edge cases after initial PR creation. Three projects had already merged; those receive
follow-up feature branches. No native source, dependency pin, UI, screenshot or performance behavior changes.
Native platform/product qualification remains separate. GitHub account billing prevented RedSalamander jobs from
starting; this external failure is retained separately from successful local tooling checks.
## Findings and proof

A valid released heading with a tab separator was not a section boundary, so an identical released entry could
suppress publication of a new fragment under Unreleased. An isolated execution of the previous module produced
one occurrence of the released text; the corrected module produces the expected two. The shared suite independently
checks space and tab separators against literal changelog text.

A timed-out synthetic PowerShell child was previously disposed without termination. The suite now uses an owned
process helper with bounded process/pipe waits, kills its child tree on timeout and waits for exit before disposal.
The negative control launches a sleeping child, forces a short timeout and verifies that exact PID/creation identity
is gone before fixture cleanup. Its fallback cleanup preserves isolation if the implementation regresses.

The concurrent drive-root policy was reorganized into a top-level section in all five primary AGENTS files while
integration was in progress. All five feature files now match that latest layout and policy text, plus only the
intended Changes instructions. No primary file, index, HEAD or branch was changed by this task.

## Local validation

- Shared behavior suite: 21/21 per project, 105 passing executions across all five.
- DxUi: Docs 25/25 (including the shared suite), paired overlay 31/31, specification and skill validators.
- RedSalamander: 41/41 focused Pester cases with zero skips, including shared behavior, reparse refusal, API-note
  diagnostics, inventory/governance and scoped policy. The complete reviewed execution digest is refreshed.
- RedXe: 21/21 shared cases and all ten skills; its independent tooling CI passed for the initial implementation.
- RedPrism: its validator passes with 21/21 shared cases and all 15 skills.
- RedTilde: complete validate.ps1 passes, including 21/21 shared cases and existing policy/performance fixtures.
- All changed PowerShell sources parse; changed-file whitespace checks pass. Six common files remain byte-identical.
- One fix fragment was folded immediately in each repository; no pending fragments remain.

Original implementation PRs: DxUi #93 and RedXe #46 receive forward-only fix commits. RedSalamander #128,
RedPrism #5 and RedTilde #1 had merged before review findings arrived; follow-up feature branches carry the fixes.
Git and PRs own the eventual publication result, which is not asserted by this local implementation record.

RedSalamander hosted jobs could not start because GitHub reported an account payment/spending restriction.
That external failure is retained; local tooling success does not establish hosted native/CodeQL qualification.
Public CI is re-evaluated on the fix heads. No native source, build project, dependency pin, UI or renderer changed.
Docs/gallery were reviewed: this slice changes developer tooling and its contract, with no visual regeneration needed.
