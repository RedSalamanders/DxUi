# Repository tooling and CI: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the PowerShell tooling, tooling tests, `.ci/`,
`.github/` and the SDK packaging scripts changed in the window.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

Counts: the reviewer's header read P1 1, P2 8, P3 11 (20 findings); its own list held P1 1, P2 7, P3 12. After
verification there are 0 P0, 0 P1, 0 P2 and 17 P3 surviving findings (16 of the original 20 plus TOOL-21, the residual of
the refuted TOOL-01), and 4 refuted findings (TOOL-01, TOOL-07, TOOL-08, TOOL-16). Engine-owned items are listed
separately at the end of the findings and are not DxUi defects.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| TOOL-02 | P3 | Confirmed | Tooling tests depend on an unprovisioned, unverified, gitignored `.ci-engine` checkout | `Tools/tests/Test-NativeScope.ps1:176` | S |
| TOOL-03 | P3 | Plausible (risk) | CODEOWNERS leaves dependency and build-definition files unowned | `.github/CODEOWNERS:5` | S |
| TOOL-04 | P3 | Plausible | The SDK ZIP ships `DxUi.lib` without its PDB and the smoke consumer cannot notice | `Tools/New-SdkPackage.ps1:63` | M |
| TOOL-05 | P3 | Confirmed | The standalone public-header list in the external-consumer gate is hand-maintained and omits 12 of 29 headers | `test-consumer.ps1:72` | S |
| TOOL-06 | P3 | Plausible | A tooling test gives the must-succeed roster child a one-second budget | `Tools/tests/Test-ToolingTestRunner.ps1:31` | S |
| TOOL-09 | P3 | Confirmed | Dependabot cannot move the sealed workflow pins; the composite action pin already drifted | `.github/dependabot.yml:2` | S |
| TOOL-10 | P3 | Confirmed | `Submit-ChangelogFold.ps1`: mixed code provenance, unguarded temp parent, uneven cleanup | `Tools/Submit-ChangelogFold.ps1:123` | S |
| TOOL-11 | P3 | Confirmed | Folding into an empty Unreleased section drops the blank line before the next release heading | `Tools/Modules/Documentation/Changelog.psm1:254` | S |
| TOOL-12 | P3 | Confirmed | Documentation and skill drift after the CI rewrite (merges the doc halves of TOOL-01 and TOOL-07) | `validate.ps1:7` | S |
| TOOL-13 | P3 | Confirmed (in part) | Dead and duplicated adapter and legacy scope code kept as compatibility | `Tools/Invoke-ProjectCi.ps1:54` | M |
| TOOL-14 | P3 | Confirmed | The Static phase stops at the first failing validator | `Tools/Invoke-ProjectCi.ps1:50` | S |
| TOOL-15 | P3 | Confirmed (in part) | SDK package hygiene: notices not routed to qualification; working-tree copy (merges the residual of TOOL-08) | `Tools/New-SdkPackage.ps1:59` | M |
| TOOL-17 | P3 | Plausible | `normal-suites.json` records the requested suite list as "discovered" and "executed" | `test.ps1:335` | M |
| TOOL-18 | P3 | Plausible | `ConsumerUpdate` trusts the newest green `qualify.yml` dispatch run for a SHA | `Tools/ConsumerUpdate.psm1:22` | S |
| TOOL-19 | P3 | Plausible | `ParallelSuites.psm1` has no deadline, no tree kill and concatenates stdout before stderr | `Tools/ParallelSuites.psm1:42` | M |
| TOOL-20 | P3 | Confirmed | Fixture and test hygiene: foreign branches, leaked scratch, tests that accept both outcomes | `Tools/tests/Test-BuildConsole.ps1:6` | S |
| TOOL-21 | P3 | Confirmed | `Tools/tests/Test-AsanRuntime.ps1` runs in no CI path (residual of TOOL-01) | `test.ps1:93` | S |

## P3 findings

### TOOL-02 — Tooling tests depend on an unprovisioned, unverified, gitignored `.ci-engine` checkout

- **Severity** P3 (reviewer P2, verifier P3) · **Category** test-gap · **Verdict** Confirmed (two details of the original
  claim were wrong, corrected below) · **Confidence** high · **Effort** S · **Introduced** `a412f316`
- **Location**: `Tools/tests/Test-NativeScope.ps1:176-181`, `Tools/tests/Test-ScopedTesting.ps1:559-565`,
  `.gitignore:11-12`
- **Lead note (reproduced 2 November)**: running `validate.ps1` in the primary checkout at `6df17007` fails exactly
  here: `Test-NativeScope.ps1` ("the current caller is pinned and PR delegation has no native profiles") and
  `Test-ScopedTesting.ps1` both report "The common workflow or engine pin diverged: publish.yml" because the local
  `.ci-engine` is `fcf723f` (30 October) while `.ci/project.json` pins `e32cf9f9`. Every other validator and tooling
  test passed. `AGENTS.md` makes `validate.ps1` mandatory, so a stale engine blocks every developer's local gate until
  it is refreshed by hand; provisioning the engine at the pin (or skipping with an explicit not-run obligation) is the fix.
- **What**: Both tests `Import-Module "$repo/.ci-engine/Tools/Modules/Ci.Policy.psm1"` (and `Ci.Project.psm1`) and call
  `Test-CiProjectConfiguration` with a hand-built context hashtable. Both are in the `Invoke-ToolingTests.ps1` roster, so
  `test.ps1` (without `-SkipTooling`), `validate.ps1`, the Tooling scope of `Test-Changes.ps1` and the main x64
  ReleaseTest job depend on them. The hand-built context bypasses `New-CiProjectContext`, the only place that checks the
  engine checkout's `HEAD` equals `.ci/project.json` `engine.ref`. Nothing in the repository clones or verifies the
  checkout (`git grep ci-engine` finds only three workflows, `.gitignore` and these two tests). CI is correct:
  `native.yml` checks the engine out at `inputs.engine_ref` into `.ci-engine`.
- **Failure scenario**: (1) A new worktree or fresh clone has no `.ci-engine`: 115 of the 120 worktrees under
  `Z:\src\DxUi-worktrees` lack it (5 have it), and the roster fails at the top-level `Import-Module` with a bare "module
  not found" and no hint how to provision it. (2) The checkout in `Z:\src\DxUi\.ci-engine` is at `fcf723f`, an ancestor of
  the pin `e32cf9f9` and 20 commits behind. That engine iterates every file in its `Templates/` (including
  `publish.yml`, which DxUi no longer has) so `Test-CiProjectConfiguration` throws "common workflow or engine pin
  diverged: publish.yml": the tests fail loudly against the wrong engine instead of validating DxUi against the pinned
  one. This was derived by reading, not by running.
- **Fix**: Add a helper (for example `Get-CiEngineRoot` in `Tools/tests/TestSupport.psm1`) used by both tests that
  (a) throws an actionable message when `.ci-engine` is missing ("clone RedSalamanders/.github at the commit in
  `.ci/project.json` into `.ci-engine`") and (b) asserts `git -C .ci-engine rev-parse HEAD` equals `engine.ref` and the
  tracked tree is clean, mirroring the check in `New-CiProjectContext`. Optionally add `Tools/Initialize-CiEngine.ps1`
  that clones or fetches `engine.repository` at `engine.ref` into `.ci-engine` and asserts the pin (network use must be
  explicit). Document the step in `Tools/README.md` and `.agents/skills/build-dxui/SKILL.md`.
- **Regression test**: A tooling test that points the helper at (a) a missing directory and asserts the provisioning
  message, and (b) a temporary git repository whose `HEAD` differs from a fixture `engine.ref` and asserts the pin-mismatch
  message. Both fail at `6df17007` because no such helper exists.
- **Verifier notes**: The reviewer said no sibling worktree has `.ci-engine` (5 do) and that the stale clone makes the
  tests accept the wrong engine (it makes them throw at the missing `publish.yml`). Consequence is developer-environment
  friction only, hence P3.

### TOOL-03 — CODEOWNERS leaves dependency and build-definition files unowned

- **Severity** P3 (reviewer P2, verifier P3) · **Category** security · **Verdict** Plausible (a risk, not a defect) ·
  **Confidence** high on the file list, unknown on branch protection · **Effort** S
- **Location**: `.github/CODEOWNERS:5-14`, `Tools/tests/Test-CodeOwners.ps1:12`, `Specs/Build/Build_UnifiedCI.md:88`
- **What**: `CODEOWNERS` owns `/.github/ /.ci/ /Tests/ /Tools/ /Build/ /*.ps1 /Directory.Build.* /.agents/ /AGENTS.md`
  (plus four root performance scripts that `/*.ps1` already covers). That is exactly the surface
  `Build_UnifiedCI.md:88` defines ("`.github/`, `.ci/`, `Build/`, `Tools/`, `Tests/`, root PowerShell entrypoints,
  `Directory.Build.*`, and the manifest ... subject to the repository's independently configured review rules"), and
  `$protectedPath` in `Test-CodeOwners.ps1:12` encodes the same set, so the contract is met. Unowned and relevant to
  supply chain or compile behavior: `vcpkg.json` (`builtin-baseline`), `vcpkg-tool.json`, `vcpkg-configuration.json`,
  `DxUi.sln` and every `*.vcxproj`, `ThirdParty/**` (vendored headers compiled into the library), `.gitattributes`,
  `.clang-format`, `capabilities.json`, `LICENSE.txt`, `THIRD-PARTY-NOTICES.md`, `Specs/**`.
- **Failure scenario**: The Dependabot `vcpkg` ecosystem (`.github/dependabot.yml:13-18`) opens a PR that moves
  `builtin-baseline`; no code-owner review is requested for it. This already happened once: PR #140 (`7c5d1fe6`,
  merged 2026-10-30). Mitigations: `vcpkg.json` is a native-identity input so main qualification gates it, and Dependency
  Review runs on PRs. The repository has one owner who is also the author, and enforcement of code-owner review depends on
  rulesets that are not visible in the repository.
- **Fix**: Optional hardening, not a contract repair: add `/vcpkg*.json`, `/DxUi.sln`, `/src/DxUi.vcxproj`,
  `/ThirdParty/`, `/.gitattributes`, `/.clang-format`, `/capabilities.json` to `.github/CODEOWNERS`, extend
  `$protectedPath` in `Tools/tests/Test-CodeOwners.ps1:12` with the same set, and extend the sentence at
  `Specs/Build/Build_UnifiedCI.md:88` so the spec and the test agree. Decide first whether rulesets require code-owner
  review at all; if not, the file is advisory and this is moot.
- **Regression test**: In `Test-CodeOwners.ps1` add the new paths to `$protectedPath`; "every protected tracked path has
  an owner" fails for each at `6df17007` until the rules exist.
- **Verifier notes**: The CODEOWNERS file matches the documented contract exactly; the omissions are a design choice
  that is open to revision. Single maintainer, so the review value is mainly for bot and outside PRs.

### TOOL-04 — The SDK ZIP ships `DxUi.lib` without its PDB and the smoke consumer cannot notice

- **Severity** P3 (reviewer P2, verifier P3) · **Category** bug · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M · **Introduced** `a412f316`
- **Location**: `Tools/New-SdkPackage.ps1:63` (only `DxUi.lib` is copied), `:122-139` (smoke project never sets
  `GenerateDebugInformation`), `Specs/Build/Build_ConsoleAndPaths.md:21-22`
- **What**: Release keeps `/Zi` ProgramDatabase by spec ("Release retains ProgramDatabase"); `.build/x64/Release/DxUi.pdb`
  exists beside `DxUi.lib`, and nothing sets `/Z7`. A static library built with `/Zi` references the compiler PDB from each
  object. The ZIP carries the library, headers, props, policy, licenses and a sample, but no PDB, and `sdk-manifest.json`
  lists none. A consumer that links with `/DEBUG` would be expected to see LNK4099 (PDB not found) per object.
- **Failure scenario**: A consumer imports `DxUi.Sdk.props`, enables `GenerateDebugInformation`, and builds under its own
  zero-warning policy; the linker output contains LNK4099 for the DxUi objects while the receipt says `smokePassed: true`.
  Not verified: nobody linked such a consumer, and the exact LNK4099 behavior with `/GL` objects is unconfirmed.
- **Fix**: First run the smoke project once with `<Link><GenerateDebugInformation>true</GenerateDebugInformation></Link>`
  and check whether `Assert-NativeBuildDiagnostics` counts linker warnings and whether LNK4099 appears. If it does and an
  external debug-info consumer is expected, either ship `lib/<Platform>/Release/DxUi.pdb` in the ZIP (add it to
  `sdk-manifest.json` with its hash; it is then covered by `archiveSha256`) or build the production-only role with
  `/Z7` (this changes the documented Release `/Zi` policy in `Build_ConsoleAndPaths.md:22`, so it needs a spec edit).
  If no such consumer exists (SDK ZIPs are internal qualification artifacts, `Build_UnifiedCI.md:61-64`), record the
  decision in the SDK paragraph of `Build_UnifiedCI.md` and close.
- **Regression test**: A second smoke variant in `Tools/New-SdkPackage.ps1` (separate IntDir and OutDir, as the warning
  control uses) with debug information enabled that must reach zero warnings and zero errors; it fails at `6df17007` if
  LNK4099 appears.
- **Verifier notes**: No specification requires a PDB in the ZIP, and there is no external SDK consumer today.

### TOOL-05 — The standalone public-header list in the external-consumer gate is hand-maintained and omits 12 of 29 headers

- **Severity** P3 (reviewer P2, verifier P3) · **Category** test-gap · **Verdict** Confirmed (gap real, impact
  smaller than stated) · **Confidence** high on the list, medium on the consequence · **Effort** S
- **Location**: `test-consumer.ps1:71-76` (header list and generated one-include translation units),
  `Samples/SdkSmoke/Main.cpp:1-6`, `Specs/Build/Build_ToolchainAndConsumption.md:187-191`
- **What**: `test-consumer.ps1:72-73` compiles 17 headers (not 14 as first reported) as the first and only include of
  their own translation unit: DxUi, Typography, FocusRestore, PointerInput, AccessibilityTextUnits, NativeMenuInterop,
  FileDrop, RichDocument, RichMarkdown, RichTextLayout, MarkdownSyntax, RichSemantic, RichSemanticDocument,
  RichTextCommands, RichImage, RichMath, RichTextExport. `include/DxUi/` has 29 headers. Not compiled alone:
  Configuration, ControlCatalog, Diagnostics, Embedded, EmbeddedAccessibility, FrameRuntime, PreparedTreeAccessibility,
  RichAccessibilityText, RichCodeTokenizer, RichTextPresentation, TextInputServices, ThemeColors. `ControlCatalog.h` and
  `Diagnostics.h` are included by no other public header, sample or the SDK smoke.
- **Failure scenario**: A future edit uses a standard type in `Diagnostics.h` without its include; in-repo targets already
  include the header first, the external consumer never includes it alone, so the missing include is found only by a real
  consumer. Today the two named headers are self-contained by reading (`Diagnostics.h` includes `<cstdint>`,
  `<string_view>`, `<windows.h>`; `ControlCatalog.h` includes `DxUi.h`), so no failure exists yet.
- **Fix**: Build the header list in `test-consumer.ps1` from `Get-ChildItem include/DxUi/*.h` (sorted) instead of the
  literal, with an explicit exclusion list that carries a reason per header if any is not meant to stand alone. The
  contract currently promises standalone compilation only for the helper headers plus `RichMath.h` and `RichTextExport.h`
  (`Build_ToolchainAndConsumption.md:187-191`); update that paragraph to say every public header compiles alone.
- **Regression test**: A tooling test that parses the header list out of `test-consumer.ps1` (AST) or reads a shared data
  file and asserts equality with `git ls-files include/DxUi/*.h` minus the recorded exclusions. It fails at `6df17007`
  for the 12 headers above.
- **Verifier notes**: Spot-checked the omitted headers' include lists; none showed a missing include.

### TOOL-06 — A tooling test gives the must-succeed roster child a one-second budget

- **Severity** P3 (reviewer P2, verifier P3) · **Category** reliability · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S · **Introduced** `28958fc6`
- **Location**: `Tools/tests/Test-ToolingTestRunner.ps1:31`, `Tools/ToolingTestRunner.psm1:26`
- **What**: `Invoke-DxUiToolingTestRoster -Scripts @(slow.ps1, later.ps1) -TimeoutSeconds 1` applies one second to both
  children. The loop `while (-not $process.WaitForExit(1000) -and $timer.Elapsed.TotalSeconds -lt $TimeoutSeconds)` starts
  its clock at `Process.Start`, so `later.ps1` must start `pwsh -NoProfile -File ToolingTestChild.ps1`, run, and write
  `after-timeout.txt` within about a second or it is killed and the assertion fails although nothing is wrong.
- **Failure scenario**: On a cold or loaded hosted runner `pwsh` startup alone exceeds one second; the x64 ReleaseTest
  job (which runs the roster) fails and the whole native pipeline needs an explicit retry. No failure has been observed:
  the hosted x64 regression job for `6df17007` (run 41459331431) passed with this test in the roster.
- **Fix**: Give the script that must succeed its own generous budget: either a per-script timeout in the roster entry
  (preferred, one parameter change in `Invoke-DxUiToolingTestRoster`) or two roster calls, keeping one second only for the
  script that is meant to hang.
- **Regression test**: The existing case, restructured so the success script runs with a 60-second budget; optionally a
  variant whose child delays 1.5 seconds before writing and must still pass.
- **Verifier notes**: Mechanism confirmed by reading; severity lowered because of the hosted pass and the trivial fix.

### TOOL-09 — Dependabot cannot move the sealed workflow pins; the composite action pin already drifted

- **Severity** P3 (reviewer P3, verifier P3) · **Category** ci · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S · **Introduced** `a412f316`
- **Location**: `.github/dependabot.yml:2-12`, `.github/actions/setup-msvc/action.yml:15`,
  `.github/workflows/format.yml:43`
- **What**: `Test-CiProjectConfiguration` requires `ci.yml`, `format.yml`, `qualification.yml`, `qualify.yml` and
  `security.yml` to equal the engine templates byte for byte after `@ENGINE@` substitution (verified: all five do at
  HEAD). A Dependabot `github-actions` PR that bumps `actions/checkout` in any of them therefore always fails the Static
  check ("common workflow or engine pin diverged"). Dependabot with `directory: /` scans `.github/workflows`, not
  `.github/actions/*/action.yml`, so the composite action stays on `actions/upload-artifact@ea165f8d... # v4` while every
  workflow and the engine's `Policy/actions.json` use `cf430e030ddb... # v7.0.2`. The engine's `Test-Workflows.ps1` lints
  only `.github/workflows/*.yml`, so the drift is undetected. No Dependabot actions PR exists yet (only the vcpkg PR #140),
  so the red-by-construction PRs are inferred.
- **Failure scenario**: The first weekly actions PR is red by construction; the composite action never moves.
- **Fix**: In `.github/dependabot.yml` add `ignore: dependency-name: actions/*` under the github-actions entry (these pins
  move only through the engine) and bump the composite action pin by hand to the `Policy/actions.json` value. Keep the
  existing `RedSalamanders/.github` ignore.
- **Regression test**: A tooling test that compares every `uses:` SHA under `.github/**` (workflows and composite
  actions) with the engine `Policy/actions.json`; it fails at `6df17007` for the composite action. It needs the
  provisioning helper from TOOL-02.
- **Verifier notes**: Engine-side counterpart listed under "Engine findings to report upstream".

### TOOL-10 — `Submit-ChangelogFold.ps1`: mixed code provenance, unguarded temp parent, uneven cleanup

- **Severity** P3 (reviewer P3, verifier P3) · **Category** reliability · **Verdict** Confirmed (item 3 was overstated,
  corrected below) · **Confidence** high · **Effort** S
- **Location**: `Tools/Submit-ChangelogFold.ps1:123-125`, `:89-92`, `:82-84`, `:109-111`, `:97-104`, `:147-155`
- **What**: (1) The new-fold path runs `Fold-Changelog.ps1` from the temporary worktree of `origin/main` (`:123-124`) but
  `Test-Changelog.ps1` from the caller's checkout (`:125`); the review path uses the caller's copies for both
  (`:89-92`). The script states the first choice is deliberate; a mismatch makes the run refuse, and the fold PR's CI runs
  main's own `Test-Changelog`, so this is fail-safe, only inconsistent. (2) `-WorktreeParent` is only required to exist;
  an explicit `-WorktreeParent Z:\` creates `Z:\changelog-fold-xxxxxxxx` at a drive root, which `AGENTS.md` (Drive-root
  output policy) forbids for output (the default is the user's temp folder). (3) Under `-WhatIf`, `git worktree remove
  --force` still runs; when it fails the script does warn (`:149`), and the `Remove-Item -Recurse -Force` fallback at
  `:152` then honors `$WhatIfPreference`, printing a "What if:" line instead of deleting. The leftover is reported, only
  not removed. (4) The review path's cleanup (`:97-104`) compares `git worktree list --porcelain` output with exact
  strings and has no `Remove-Item` fallback, so a spelling mismatch (8.3 name, case) would leave a registered worktree.
- **Failure scenario**: `./Tools/Submit-ChangelogFold.ps1 -WhatIf -WorktreeParent Z:\` creates a checkout of `main` at
  the drive root, and after a locked-file removal failure leaves it there with only the git warning and a "What if" line.
- **Fix**: Reject a drive-root or reparse-point `-WorktreeParent`; call `Remove-Item -WhatIf:$false` in the fallback; share
  one `Remove-OwnedWorktree` helper between both paths comparing `[IO.Path]::GetFullPath` values with the platform
  comparer used at `:143-144`. Optionally run all three scripts from one root; if kept as is, document the choice in the
  script header.
- **Regression test**: Fixture in `Tools/Test-ChangelogWorkflow.ps1` where `git worktree remove` is stubbed to fail:
  assert the directory is deleted and a warning is printed, with and without `-WhatIf`; and a case that rejects a drive
  root.
- **Verifier notes**: Items 1, 2 and 4 confirmed by reading; item 3 does emit messages.

### TOOL-11 — Folding into an empty Unreleased section drops the blank line before the next release heading

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `Tools/Modules/Documentation/Changelog.psm1:5` (heading pattern), `:235-255` (`Merge-ChangelogFragments`),
  `:162` (release output)
- **What**: `UnreleasedHeadingPattern` consumes the heading line plus one following blank line, so after
  `Release-Changelog.ps1` (`## Unreleased\n\n## 1.1.0 - date\n\n...`) `$at` lands on the next `## ` heading. `$separator`
  is empty because `$Changelog[$at - 1]` is `\n`, and each entry is inserted as `- entry\n` directly before
  `## 1.1.0 - date`. The verifier replayed the exact regex and arithmetic on a literal string and got
  `## Unreleased\n\n- Next.\n## 1.1.0 - 2026-11-01\n\n- First.\n`. The exact-fold validator uses the same function, so
  tooling stays self-consistent and nothing flags it. Cosmetic: the blank line stays missing for every later fold.
- **Failure scenario**: The first fold after a release writes `- Next.` immediately above the release heading.
- **Fix**: In `Merge-ChangelogFragments`, when the text at `$at` begins with `## `, append `$newline` after the inserted
  entries (use the changelog's own line ending). The module header (`Changelog.psm1:1`) says it is kept identical in
  DxUi, RedSalamander, RedXe, RedPrism, RedTilde and RedMonitor, so mirror the change there or the copies drift.
- **Regression test**: `Tools/Test-ChangelogWorkflow.ps1`, after the existing "release cut" case (`:347-365`): fold one
  fragment into the cut result and assert the exact text
  `# Changelog\n\n## Unreleased\n\n- Next.\n\n## 1.1.0 - 2026-11-01\n\n- First...`;
  it fails at `6df17007`. Add the CRLF variant.
- **Verifier notes**: Reproduced with an ad hoc snippet over literal strings only.

### TOOL-12 — Documentation and skill drift after the CI rewrite

- **Severity** P3 (reviewer P3, verifier P3) · **Category** doc · **Verdict** Confirmed (wider than reported) ·
  **Confidence** high · **Effort** S · **Merged** the documentation halves of TOOL-01 and TOOL-07
- **Location**: see the list below
- **What**: The unified CI (`Tools/Invoke-ProjectCi.ps1`) runs the five `validate-*.ps1` scripts individually and passes
  the PR base SHA to `Test-Changelog -Base` (`:46,49`); it does not run `validate.ps1`, the tooling roster runs only in
  the main x64 ReleaseTest job (`:77`), no benchmark runs on PRs (`benchmark.yml` is `workflow_dispatch` only), and no push
  to main runs `Test-Changelog`. Stale statements:
  - `validate.ps1:7`, `Tools/README.md:8`, `test.ps1:28` (`.PARAMETER SkipTooling`), `docs/performance.md:444`: "CI's
    validation job runs" the script or the tooling tests.
  - `Tools/README.md:35` ("CI `HEAD^1`") and `Specs/Core/Core_Changes.md:29-31` (a normative contract: CI compares a PR's
    merge commit with `HEAD^1` and a push with `github.event.before`).
  - `Specs/Testing/Testing_Validation.md:333-337` (the independent Windows tooling job and ASan runtime check) and
    `:1178-1180` ("always-running Windows/Linux tooling remains covered"); `:276` ("PRs run compiler-free
    formatting/tooling/security") should say static validators.
  - `.agents/skills/performance-resources/SKILL.md:12` and `docs/performance.md:200-223` still say every PR that changes
    what the benchmark measures runs the hosted `paired-benchmark (pull request)` job and fails for a confirmed
    degradation. The owning contract is already correct (`Specs/Core/Core_PerformanceAndResources.md:441-445`, manual
    run, and `Specs/Build/Build_UnifiedCI.md:47`).
  - `.agents/skills/build-dxui/SKILL.md` does not mention `-ParallelSuites`, `-ProductionOnly`,
    `Tools/New-SdkPackage.ps1`, `Tools/Submit-ChangelogFold.ps1` or the `.ci-engine` prerequisite (TOOL-02).
- **Failure scenario**: A contributor believes PR CI runs `validate.ps1` or a PR benchmark, or that `Test-Changelog -Base`
  sees a merge commit; an agent trusting the performance skill skips its own before/after measurement.
- **Fix**: Rewrite each statement to match the current contract: PRs run the static subset (`Build_UnifiedCI.md:9-12`),
  tooling tests run locally (`validate.ps1`, PrePush) and in the main x64 ReleaseTest job, paired benchmarks are manual
  and acceptance stays a developer obligation (`AGENTS.md` Engineering). State in `validate.ps1` that the tooling step is
  not part of PR CI. Rewrite the `docs/performance.md` "Pull requests" bullet as the manual `benchmark.yml` description.
- **Regression test**: A `Tools/tests/Test-Docs.ps1` case that reads `validate.ps1`, `test.ps1`, `Tools/README.md`,
  `docs/performance.md`, `Specs/Core/Core_Changes.md` and `.agents/skills/performance-resources/SKILL.md` and asserts none
  contains "validation job" or "paired-benchmark (pull request)"; it fails at `6df17007`.
- **Verifier notes**: `docs/performance.md`, `Testing_Validation.md` and the `test.ps1` parameter text were found by the
  verifier; the reviewer listed only some of them.

### TOOL-13 — Dead and duplicated adapter and legacy scope code kept as compatibility

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification · **Verdict** Confirmed (in part: the
  engine-owned workflow items were refuted, see the engine section) · **Confidence** high · **Effort** M ·
  **Introduced** `a412f316`
- **Location**: `Tools/Invoke-ProjectCi.ps1:54,59,86,91,96`; `Tools/Commit-Gallery.ps1:12,21-26,43-51`;
  `Tools/ScopedTesting.psm1:332-405`; `Tools/NativeScope.psm1`; `Tools/Get-NativeScope.ps1`; `Tools/Test-CiGate.ps1`;
  `Test-Changes.ps1:77`; `Tests/test-scopes.json:639-651`
- **What**: In `Invoke-ProjectCi.ps1`, `$number` (`:54`) is never read; the second `if ($Phase -eq 'Qualification')`
  (`:91`) is unreachable because `:86` already returned; `evidence_directory` is written to `GITHUB_OUTPUT` twice
  (`:59`, `:96`). `Commit-Gallery.ps1 -PullRequest` needs `GITHUB_REPOSITORY` and `gh workflow run ci.yml` and has no
  hosted caller. `Get-ScopedPrCoverage` returns `@()` at `$profile.Count -eq 0` (`ScopedTesting.psm1:373`) and the engine
  forces `prCoverage` to stay empty, so `Get-ScopedPrCandidateScopes` (which calls `Get-NativeScope`, `:325`),
  `Get-ScopedPrContractDigest`, the keys `prContractClosure` and `prWorkflowDigest` and `NativeScope.psm1` are unreachable;
  the only caller is `Test-Changes.ps1:77`. `Test-CiGate.ps1` has no caller but its own test. The digest in
  `Tests/test-scopes.json:651` is verified by no live path, and its closure mode hashes raw bytes while `.gitattributes`
  pins EOL only for some extensions (latent, since nothing consumes it).
- **Failure scenario**: Maintenance cost and misleading coverage: hundreds of lines in `Test-ScopedTesting.ps1` and
  `Test-NativeScope.ps1` guard behavior the repository forbids.
- **Fix**: Delete the adapter-local dead code (`$number`, the unreachable branch, one of the two `evidence_directory`
  writes), `Commit-Gallery.ps1 -PullRequest`, the PR-coverage code path with its manifest keys, and the legacy scope
  modules `NativeScope.psm1`, `Get-NativeScope.ps1`, `Test-CiGate.ps1` with their tests and roster entries. Keep one test
  asserting `prCoverage` is empty (it already exists). Do not touch `.github/workflows/qualify.yml` or `ci.yml`.
- **Regression test**: After removal, `Tools/tests/Test-ScopedTesting.ps1` still asserts `prCoverage` is empty and
  `Tools/tests/Invoke-ToolingTests.ps1` still passes; add a parse-based check that `Invoke-ProjectCi.ps1` has no
  unreferenced local variable (or simply review). No new failing test is needed beyond the deletions.
- **Verifier notes**: `qualify.yml:17-20` (`profile` input) and `ci.yml:37,86` are byte-identical engine template text
  and cannot be edited in DxUi; reported upstream instead.

### TOOL-14 — The Static phase stops at the first failing validator

- **Severity** P3 (reviewer P3, verifier P3) · **Category** reliability · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `a412f316`
- **Location**: `Tools/Invoke-ProjectCi.ps1:46-51`
- **What**: `validate.ps1` ran every step and listed all failures (`Invoke-DxUiValidation`). The Static loop
  `foreach ($script in @('validate-skills.ps1', ...)) { Run $script }` goes through `Invoke-CiProjectScript`, which throws on
  the first nonzero exit, so a PR with a skill error and a spec error shows one at a time. The engine's Static allowlist
  does not admit `validate.ps1` for DxUi, so the fix must live in the adapter.
- **Failure scenario**: Two broken validators cost two pushes and two CI round trips.
- **Fix**: Wrap each `Run $script` in try/catch, collect the failing script names and messages, and throw once after the
  loop with the list. Move the loop into a small function (for example in a new `Tools/ProjectCi.psm1`) that takes the
  runner as a scriptblock so it can be tested without the engine.
- **Regression test**: A tooling test (next to `Test-Validation.ps1:320`) that calls the function with a runner that throws
  for two of five scripts and asserts the message names both and that the other three ran. It fails at `6df17007`
  because the loop aborts at the first failure.
- **Verifier notes**: `Test-CiProjectConfiguration`, workflow lint and `Test-Changelog` before the loop are also
  fail-fast; aggregation there is optional.

### TOOL-15 — SDK package hygiene: notices not routed to qualification; working-tree copy

- **Severity** P3 (reviewer P3, verifier P3) · **Category** reliability · **Verdict** Confirmed (in part) ·
  **Confidence** medium · **Effort** M · **Merged** the residual of TOOL-08 · **Introduced** `a412f316`
- **Location**: `Tools/New-SdkPackage.ps1:57-59,67-72,105-113`; `.ci/project.json` (`include`, `exclude`)
- **What**: (1) `THIRD-PARTY-NOTICES.md` (excluded by `**/*.md`) and `docs/licenses/*.txt` (excluded by `docs/**`) are
  copied into the ZIP (`:67-72`) but excluded from native routing, so a notice-only change retains the previous native
  status and no new qualified ZIP carries it until an unrelated code change. `Build_UnifiedCI.md` says shipped resources
  remain meaningful. (2) `Copy-Item -Recurse include` (`:59`) copies the working tree, not `git ls-files include`; the
  manifest's `source` is `git rev-parse HEAD` (`:106`) and `testsIncluded=$false` is a literal (`:107`). In CI this is
  covered: `New-CiProjectContext` requires a clean exact checkout at job start and `Write-CiProductionReceipt` calls
  `Assert-CiSourceFrozen` (HEAD equals the frozen commit, `git status --porcelain --untracked-files=normal` empty) after
  packaging; `-ProductionOnly` builds only the `DxUi` target (`build.ps1:189-192`) and the library has no test macros. A
  local run after an uncommitted header edit, or with a test-role `.build/x64/Release/DxUi.lib`, still yields a
  self-attested receipt. (3) The stage directory `.build/sdk/stage-*` is never removed.
- **Failure scenario**: A required notice update never reaches a built SDK until some code change triggers
  qualification; a local package run records `source=HEAD` for different bytes.
- **Fix**: Add `THIRD-PARTY-NOTICES.md` and `docs/licenses/**` to `include` in `.ci/project.json` (check that the engine
  schema allows it; these are shipped resources). In `New-SdkPackage.ps1`: refuse to run when
  `git status --porcelain --untracked-files=normal` is not empty, copy headers from `git ls-files include` rather than
  the directory, and delete the stage directory on success. Byte-reproducibility of the ZIP is not required (the receipt
  hashes the produced bytes), so do not add timestamp normalization.
- **Regression test**: A tooling test over a fixture repository with a modified header: the packager must refuse
  (cannot refuse today), plus a `Tools/tests/Test-Validation.ps1` style check that every file the packager copies from
  `docs/` or the root is covered by an `include` pattern or not excluded.
- **Verifier notes**: The original "receipt attests properties it never measures" claim (TOOL-08) was refuted for CI; only
  the local-run hygiene is carried here.

### TOOL-17 — `normal-suites.json` records the requested suite list as "discovered" and "executed"

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test-gap · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M
- **Location**: `test.ps1:331-336`
- **What**: `Publish-TestRunEvidenceReceipt` writes `discovered=@($Suites); executed=@($Suites)` with `countUnit='suite'`,
  taken from the argument list. It is reached only after `if ($failures.Count) { throw }` (`:331`), so `executed` implies
  every requested suite exited 0. The engine's `Get-CiSuiteEvidence` requires `discovered == executed > 0`, so any
  nonempty list satisfies it. Separate guards remain: names must exist in `Tests/test-scopes.json`, the native runner
  rejects unknown suite filters (`Tests/Controls/DxUi.Tests.Runner.cpp:617`), and `Tools/tests/Test-ScopedTesting.ps1`
  checks the default list covers every native scope. The contract states suite counts are suite counts
  (`Build_UnifiedCI.md`, receipts paragraph). No guard rejects a suite that registers zero tests and exits 0.
- **Failure scenario**: A suite whose registration is emptied by a build or macro change still exits 0 and counts as
  executed.
- **Fix**: Have the native runner print one machine-readable `suite=<name> tests=<n>` line (or write a small JSON) and build
  `executed` from the observed counts in `test.ps1`, failing the run when any `n` is 0. Keep the suite as the count unit.
- **Regression test**: A fixture suite log with `tests=0` must make `test.ps1`'s evidence step fail; it succeeds at
  `6df17007`.
- **Verifier notes**: Hardening; no current misreport was demonstrated.

### TOOL-18 — `ConsumerUpdate` trusts the newest green `qualify.yml` dispatch run for a SHA

- **Severity** P3 (reviewer P3, verifier P3) · **Category** ci · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S
- **Location**: `Tools/ConsumerUpdate.psm1:22-26`; engine `Tools/Invoke-Admission.ps1:45-46` and
  `Tools/Modules/Ci.Policy.psm1` (`Get-CiAdmissionDecision`)
- **What**: The advisory queries
  `actions/workflows/qualify.yml/runs?head_sha=<candidate>&branch=main&event=workflow_dispatch&per_page=1`
  and reports "update available" when the newest run is `completed/success`. A retained non-success outcome makes the run
  red (`Invoke-Admission.ps1:45-46` throws) and a retry that turns green is legitimately newer, so the reviewer's
  masking cases do not hold. One real hole: `Get-CiAdmissionDecision` returns `superseded` (job succeeds, no native work)
  when the dispatch input `commit` differs from the current head, and a workflow_dispatch run's `head_sha` is the ref tip
  at dispatch. A coordinator dispatch that races a push, or a manual dispatch with an older `commit`, can therefore end
  green with `head_sha` equal to the current tip although that tip was never qualified.
- **Failure scenario**: Main moves from X to Y between the coordinator's observation and its dispatch; the run (`commit=X`,
  `head_sha=Y`) ends green as superseded; a consumer's advisory reports Y as validated before Y's real qualification
  finishes or after it failed.
- **Fix**: Read the engine's `main-qualification` commit status for the candidate SHA (it is `success` only after the
  batch completed or a retained success) instead of workflow runs; keep the conservative "keep pinned" text on any
  other state or error. The function stays read-only and must not echo transport errors.
- **Regression test**: `Tools/tests/Test-ConsumerUpdate.ps1`: a stubbed `$Request` that returns a green workflow run for
  the candidate but a `failure` or `pending` status context must yield the "no successful validation" notice; it fails at
  `6df17007`.
- **Verifier notes**: Advisory only; the consumer separately verifies its pin, hence P3.

### TOOL-19 — `ParallelSuites.psm1` has no deadline, no tree kill and concatenates stdout before stderr

- **Severity** P3 (reviewer P3, verifier P3) · **Category** reliability · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M
- **Location**: `Tools/ParallelSuites.psm1:42-91` (no timeout), `:65` and `:98` (`Kill()` without tree), `:84-86`
  (stream concatenation)
- **What**: The scheduler waits on `WaitForExitAsync()` with no limit; the per-test native watchdog (300 s, exit 124)
  bounds a hung test but not a hang before it starts or during teardown. `Kill()` ends only the child, not a process
  tree. After an exit it drains with a 10 s limit (`:77`) and throws, leaving the other children to the `finally`. The
  merged log is the whole stdout then the whole stderr, not chronological, and `Get-SuiteSkips` attributes `SKIPPED:` to the
  last `[START]`, which is safe only because the ControlTests runner prints `[START]`, `[DONE]` and `SKIPPED:` all to
  stderr. Opt-in only: `-ParallelSuites` defaults to 1, the adapter never passes it, and foreground suites (the
  `InteractiveLease` runner prints `SKIPPED:` to stdout) never run in the parallel batch.
- **Failure scenario**: A future ordinary suite writes `SKIPPED:` to stdout; in parallel mode only, the skip is attributed
  to the wrong test.
- **Fix**: Add a `-TimeoutSeconds` parameter (default from the suite budget), use `Kill($true)` at both sites, and either
  timestamp-interleave the two streams or document the split and add a test that pins attribution.
- **Regression test**: `Tools/tests/Test-ParallelSuites.ps1`: a fixture child that prints `[START] A` to stderr and
  `SKIPPED: x` to stdout; assert the attribution matches serial mode (it differs at `6df17007`), and a child that hangs
  must be ended at the deadline with its tree.
- **Verifier notes**: Local opt-in path; the seeded scenario requires a future suite.

### TOOL-20 — Fixture and test hygiene: foreign branches, leaked scratch, tests that accept both outcomes

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test-gap · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `Tools/tests/Test-BuildConsole.ps1:6-10,38-40`; `test-consumer.ps1:11`;
  `Tools/tests/Test-ParallelSuites.ps1:77-84`; `Tools/tests/Test-MarkdownData.ps1` (bidi case)
- **What**: `Test-BuildConsole.ps1:6-10` carries a `bp28` / `RedSalamander.Perf` / `RedSalamander.vcxproj` branch
  that imports `Tools/Modules/Testing/TestSandbox.psm1`, which does not exist in DxUi, so it can never run; its scratch
  `.build/BuildConsoleTests/<guid>` is never removed; the long-input case (`:38-40`) accepts a
  `RegexMatchTimeoutException` as success. `test-consumer.ps1:11` creates `.build/consumers/<12 hex>` (a relocated clone plus
  restored dependencies) and never deletes it (retention for diagnostics is plausible, but undocumented).
  `Test-ParallelSuites.ps1:77-84` asserts strings in `test.ps1` instead of behavior. The bidi test in
  `Test-MarkdownData.ps1` reimplements the generator's algorithm and compares byte tables, so a shared misreading of the
  rule passes (it also pins four code points).
- **Failure scenario**: Local disk growth under `.build`; a regression that makes the diagnostics regex time out on every
  large log still passes `Test-BuildConsole`.
- **Fix**: Remove the foreign branch from `Test-BuildConsole.ps1`; clean owned scratch in a `finally` after a containment
  check (leaf matches a 32-hex name directly under `.build/BuildConsoleTests`); make the long-input case assert the
  timeout path is not taken on a runtime that supports non-backtracking, or assert either outcome explicitly with a
  comment; document or bound the `.build/consumers` retention; replace the string assertions in
  `Test-ParallelSuites.ps1` with a run of a fake `test.ps1` fixture.
- **Regression test**: The tests themselves; add an assertion that `Test-BuildConsole.ps1` leaves no directory behind.
- **Verifier notes**: None of these affects a gate.

### TOOL-21 — `Tools/tests/Test-AsanRuntime.ps1` runs in no CI path (residual of TOOL-01)

- **Severity** P3 (new, from the verifier's residual of reviewer P1) · **Category** ci · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `a412f316`
- **Location**: `test.ps1:92-93`, `Tools/Invoke-ProjectCi.ps1:77-78,82`, `Tools/tests/Invoke-ToolingTests.ps1:6`,
  `Tests/test-scopes.json:566-569`
- **What**: The old `ci.yml` ran `./Tools/tests/Test-AsanRuntime.ps1` on Windows (`windows-tooling`). It is not in the
  `Invoke-ToolingTests.ps1` roster and is invoked only from `test.ps1:93` under `if (-not $SkipTooling)`; the unified
  adapter always passes `-SkipTooling` (`Invoke-ProjectCi.ps1:78,82`) and runs only the roster (`:77`), so no PR, main or
  manual qualification job executes it. It is an MSBuild fixture for `Build/DxUi.AddressSanitizer.targets` (ARM64 runtime
  staging and the fail-closed missing-runtime case), so the contract sentence "Compiler fixtures run once on the x64
  native host" (`Build_UnifiedCI.md:65`) arguably covers it. It remains in the local Tooling scope
  (`Tests/test-scopes.json:566-569`), and ASan Debug itself is manual-only by contract (`Build_UnifiedCI.md:47`).
- **Failure scenario**: A regression in `Build/DxUi.AddressSanitizer.targets` is found only by a local run or a manual ASan
  qualification, never by automatic CI.
- **Fix**: In `Tools/Invoke-ProjectCi.ps1` add `Run 'Tools/tests/Test-AsanRuntime.ps1'` next to the x64 roster call
  (`:77`), so it executes once on the hosted x64 native host after `setup-msvc`; confirm it passes on the hosted image
  (it uses `Get-DxUiVisualStudioInstallation`). Keep it out of `Invoke-ToolingTests.ps1` so the roster stays portable.
  Update the `Tools/README.md` row for `tests/Test-AsanRuntime.ps1` accordingly. If the decision is that it stays local
  only, record that in `Specs/Build/Build_UnifiedCI.md` instead.
- **Regression test**: In `Tools/tests/Test-Validation.ps1`, in the case "complete local validation remains available while
  PRs select the compiler-free subset" (`:~317-328`), add an assertion that the adapter contains
  `Run 'Tools/tests/Test-AsanRuntime.ps1'` outside the Static block; it fails at `6df17007`.
- **Verifier notes**: This is the real remainder of the refuted TOOL-01.

## Engine findings to report upstream (RedSalamanders.CI)

These are in the sealed engine templates or the engine itself, pinned at `e32cf9f9b29f4e817decb09e1be44be7fc1a94de`. DxUi
cannot change them: `Test-CiProjectConfiguration` compares `ci.yml`, `format.yml`, `qualification.yml`, `qualify.yml` and
`security.yml` byte for byte with `Templates/*.yml`, and I verified all five are identical at HEAD.

1. (TOOL-16, P3, insider-only) `Templates/ci.yml` declares the `workflow_dispatch` input `pr_number` as `type: string` and
   uses it in `concurrency.group` (`ci-${{ ... || inputs.pr_number }}`) with `cancel-in-progress: true`. A dispatch with
   `pr_number: main-coordinator` shares the push coordinator's group and can cancel a coordinator in its five-minute wait;
   the dispatched run then fails in `resolve`. Fix: use `type: number` (as `Templates/format.yml` does) and validate
   `expected_head` as 40 hex in `Resolve-PrCheck.ps1`; add a check in the engine's `Tests/Test-Engine.ps1` that every
   dispatch input used in `concurrency` is `number` or `choice`. The `pull_request: branches: [main]` filter is intended
   (the manual writer accepts only main-targeting PRs).
2. (TOOL-13 part, P3) `Templates/qualify.yml` declares an input `profile` that is never forwarded to `native.yml`; the
   `Enable long Windows checkout paths` step with `if: runner.os == 'Windows'` appears in `Templates/ci.yml` (`resolve` and
   `report` jobs) and in `pr.yml` (`ci-gate`) on `ubuntu-24.04` jobs, where it can never run. Remove both from the templates
   and re-pin all six projects.
3. (TOOL-09 part, P3) The engine's `Tools/Test-Workflows.ps1` lints only `.github/workflows/*.yml`; add a Static check that
   every `uses:` in `.github/**` (workflows and composite actions) matches `Policy/actions.json`, and tell the projects
   whether Dependabot `github-actions` PRs against sealed templates are expected (they fail the template comparison).
4. (TOOL-01 enhancement, optional) The Static allowlist (`Invoke-CiProjectScript`) admits `validate.ps1` only for
   RedMonitor. If DxUi wants its portable tooling tests in PR checks, the engine must admit a dedicated script (for example
   a DxUi `Tools/tests/Invoke-PortableToolingTests.ps1`) and DxUi must reverse `Tools/tests/Test-Validation.ps1:324`. This is
   a design change, not a defect fix; the current split is stated in `Build_UnifiedCI.md:9-12`.
5. (Question 9 below) Add an engine regression test that `format.yml`'s formatter never reads `.clang-format` from the PR
   head.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| TOOL-01 | P1: PRs no longer run `Invoke-ToolingTests.ps1`; it first runs after merge, and the docs still say CI's PR job runs it | The PR/main split is the stated, tested contract, not a regression: `Specs/Build/Build_UnifiedCI.md:9-12` ("PRs run ... the reviewed compiler-free static subset ... Compiler-dependent tooling runs in main qualification") and `:65` ("Compiler fixtures run once on the x64 native host"); engine `Specs/CI.md:9` ("PR checks never compile, including compiler-dependent tooling fixtures") and `:19`; the engine allowlist admits `validate.ps1` only for RedMonitor (`Ci.Project.psm1`, `Invoke-CiProjectScript`); `Tools/tests/Test-Validation.ps1:324` asserts Static contains neither `Invoke-ToolingTests.ps1` nor `Run 'validate.ps1'` ("PRs cannot enter compiler fixtures"), and `Tools/tests/Test-ScopedTesting.ps1:568` asserts the adapter keeps `Invoke-ToolingTests.ps1` ("lost its main owner"). The roster is not compiler-free (`Test-PairedProjectProfiles.ps1:11,18`, `Test-ReleaseBuildPolicy.ps1:8-11`) and the engine PR job provisions no MSVC (`Test-NativeScope.ps1:185`). The hosted main x64 job ran the roster and passed for `6df17007` (run 41459331431). The proposed test would contradict `Test-Validation.ps1:324`. Residuals: TOOL-21 (`Test-AsanRuntime.ps1`) and TOOL-12 (stale docs) |
| TOOL-07 | P2: no automated performance-regression gate remains, and the performance contract and `AGENTS.md` still say PRs run one | Removal is policy: `Specs/Build/Build_UnifiedCI.md:47` and `Specs/Core/Core_PerformanceAndResources.md:441-445` ("explicit manual hosted x64 Release action ... Removing the PR/timer route does not waive required before/after performance acceptance"); the "Hosted paired gate" section is already current, and `AGENTS.md:44-47` makes no PR-benchmark claim (the baseline measurement is a developer obligation). Only `.agents/skills/performance-resources/SKILL.md:12` and `docs/performance.md:200-223` are stale: carried into TOOL-12 |
| TOOL-08 | P2: the SDK receipt attests `testsIncluded`, `TestModeDisabled`, `SmokePassed` without measuring them and binds the library to HEAD only by path | In CI the engine binds it: `Write-CiProductionReceipt` calls `Assert-CiSourceFrozen` (HEAD equals the frozen commit and `git status --porcelain --untracked-files=normal` is empty after packaging) and `New-CiProjectContext` requires a clean exact checkout at start; `SmokePassed $true` (`Tools/Invoke-ProjectCi.ps1:70`) is reached only after `Tools/New-SdkPackage.ps1` returned, and that script throws on any smoke, header-closure, hash or warning-control failure before `:183`; `build.ps1:189-192` builds `/t:DxUi` only and `src/DxUi.vcxproj` plus `include/` define no test macro. Production runs build fresh in the same job (`Invoke-ProjectCi.ps1:55-62`). Residual local-run hygiene: carried into TOOL-15 |
| TOOL-16 | P3: `ci.yml` accepts a free-text `pr_number` used in the concurrency group; stacked PRs lose checks | `.github/workflows/ci.yml` is byte-identical to the engine `Templates/ci.yml` (`type: string`, the concurrency expression and `branches: [main]` are template text), and `Test-CiProjectConfiguration` rejects any DxUi-side edit; the DxUi-side fix and test cannot exist. The substance is an engine issue: see "Engine findings to report upstream", item 1 |

## Area architecture and simplification proposals

All of these are tooling changes; none changes library production code or the shipped SDK contents.

1. **One process-supervision helper.** At least seven near-identical "start redirected child, drain pipes, enforce a
   timeout, kill, dispose" implementations: `Tools/ParallelSuites.psm1`, `Tools/ToolingTestRunner.psm1`,
   `.github/actions/setup-msvc/Setup.ps1`, `Tools/PerformanceProfiling.psm1` (`Invoke-OwnedProfilingProcess`),
   `Tools/ConsumerUpdate.psm1` (`Invoke-DxUiOwnedProcess`), `Tools/Modules/Documentation/Changelog.psm1`
   (`Read-ChangelogGit`), `Tools/Test-ChangelogWorkflow.ps1` (`Invoke-FixtureProcess`). They differ in tree kill, bounded
   drain and strict decoding. One tested `Tools/OwnedProcess.psm1` removes most of TOOL-19's class. Risk: medium (the
   Changelog module is shared across six repositories, so keep its copy out of scope). Effort L. Production: none.
2. **Provision the engine like a dependency** (TOOL-02): `Tools/Initialize-CiEngine.ps1` plus a shared
   `Get-CiEngineRoot` for the two tests and any local `Invoke-ProjectCi.ps1 -Phase Static -DevelopmentEngine` dry run.
   Risk: low. Effort S. Production: none.
3. **Split `Tools/Invoke-ProjectCi.ps1` by phase** (`Invoke-StaticPhase`, `Invoke-ReleaseTestPhase`,
   `Invoke-ProductionPhase`, `Invoke-QualificationPhase`) and drop the unreachable branch; it is one linear script with early
   returns deciding four roles today, which also makes TOOL-14's aggregation testable. Risk: low, covered by
   `Test-NativeScope.ps1` and `Test-Validation.ps1` string checks (update them). Effort M. Production: none.
4. **Share the Changes workflow module.** `Changelog.psm1` is documented as identical in six repositories
   (`Changelog.psm1:1`); copies drift silently (TOOL-11 must be mirrored). Move it, `Test-ChangelogWorkflow.ps1` and the
   three entry scripts into the engine or a versioned shared module and vendor a pinned copy with a digest check. Risk:
   medium (cross-repository). Effort L. Production: none.
5. **Retire the pre-unified-CI surface** (TOOL-13): `NativeScope`, `Get-NativeScope`, `Test-CiGate`, `Get-BenchmarkScope`
   wiring, `Get-ScopedPrCoverage` and their tests, roughly 1,200 lines of modules and tests; keep one test asserting
   `prCoverage` stays empty. Risk: low. Effort M. Production: none.
6. **Test that the clang-format pin exists once or agrees.** The pin lives in `Tools/Install-ClangFormat.ps1` and in the
   engine's `Policy/clang-format.json` (both 22.1.3 with the same SHA-256 today). Add a tooling test that fails if they
   differ. Risk: none. Effort S. Production: none.
7. **Split `Tools/Test-ChangelogWorkflow.ps1`** (654 lines: fixture plumbing, 20 cases, process control). The generic
   harness (`Case`, `Put`, `Snapshot`, `Invoke-FixtureGit`, `Invoke-FixtureProcess`) duplicates
   `Tools/tests/TestSupport.psm1`; the reason (shared across repositories) is valid, so proposal 4 is the real fix.
   Risk: low. Effort M. Production: none.
8. **Rename or split `Tools/tests/Test-NativeScope.ps1`.** After proposal 5 the remaining half is engine-conformance
   checks; move them to a `Test-CiAdapter.ps1` so the name says what it checks. Risk: none. Effort S. Production: none.

## Test-quality observations

- Strong behavior tests: `Tools/Test-ChangelogWorkflow.ps1` (real Git fixtures, bare remote, byte comparisons, owned-child
  timeout), `Tools/tests/Test-BuildConsole.ps1` (imposter and reordered footers), `Tools/tests/Test-MarkdownData.ps1`
  (byte-for-byte regeneration against the committed header), `Tools/tests/Test-ParallelSuites.ps1` first three cases (real
  overlap barrier), `Tools/tests/Test-MsvcSetup.ps1` (a sentinel process proves only the owned tree is killed).
- Tests that assert implementation text and can pass while behavior is wrong: `Tools/tests/Test-ParallelSuites.ps1:77-84`
  (strings in `test.ps1`), the adapter case in `Tools/tests/Test-NativeScope.ps1:~195-203` (strings in
  `Invoke-ProjectCi.ps1`; it does not list `Invoke-ToolingTests.ps1` or `Test-AsanRuntime.ps1`, which
  `Tools/tests/Test-ScopedTesting.ps1:568` and `Tools/tests/Test-Validation.ps1:324-327` cover for the former only),
  `Tools/tests/Test-BenchmarkGate.ps1` (about 100 `.Contains` checks over `benchmark.yml`). They guard against accidental
  deletion, not wrong behavior.
- Correction to the reviewer: the Static phase is covered by a negative assertion (`Test-Validation.ps1:324`) and the
  five validator names (`:325`); what is not covered is aggregation of failures (TOOL-14) and any CI execution of
  `Test-AsanRuntime.ps1` (TOOL-21).
- Time-fragile: TOOL-06 (one-second budget); `Tools/tests/Test-MsvcSetup.ps1` relies on a 1.5 s installer budget and a
  child writing `child.txt` before sleeping, so a slow .NET start fails the read, not the product.
- Needs unprovisioned state: TOOL-02 (`.ci-engine`).
- Not in the roster and not run by the roster: `Test-AsanRuntime.ps1` (TOOL-21), `Test-MsvcSetup.ps1`,
  `Test-InteractiveLease.ps1`, `Test-TestFilter.ps1`, `Test-TestWatchdog.ps1` (the last three are native-runner checks run
  by `test.ps1`; `Test-MsvcSetup.ps1` was not traced).
- `Tools/tests/Test-ToolingTestRunner.ps1` covers exit-code aggregation, stale `LASTEXITCODE`, timeout and module
  isolation; it does not cover a child that writes more than a pipe buffer to stderr before exiting nonzero (the code
  uses async reads; a test would pin that).

## Questions for the developer

1. Is it intended that `Invoke-ToolingTests.ps1` is not part of PR checks? Recommended answer: yes, per
   `Build_UnifiedCI.md:9-12` and `Test-Validation.ps1:324` (false premise — see verifier); correct the docs (TOOL-12) and
   decide whether `Test-AsanRuntime.ps1` joins the x64 ReleaseTest phase (TOOL-21).
2. Who enforces the `AGENTS.md` rule to measure a retained baseline for performance-relevant changes now that no workflow
   compares anything automatically? Recommended answer: the developer, by contract
   (`Core_PerformanceAndResources.md:441-445`); fix the stale skill and `docs/performance.md` text (TOOL-12).
   Optionally have `Test-Changes.ps1 -Explain` print "dispatch benchmark.yml" when `Get-BenchmarkScopeRules` matches the
   changed paths.
3. How should a fresh clone or worktree obtain `.ci-engine` at the pin, and should `test.ps1` fail or skip the two
   dependent tests? Recommended answer: provision with a helper and fail with an actionable message (TOOL-02); do not skip,
   because the main CI job checks the engine out and the tests are meant to run.
4. Do external consumers of the SDK link with debug information, and how do they get `DxUi.pdb`? Recommended answer:
   there is no external SDK consumer while the ZIPs are internal; run the debug-info smoke variant once and decide
   (TOOL-04).
5. What backs `testsIncluded=false` and `TestModeDisabled`? Recommended answer: structure and the engine, not a property:
   `-ProductionOnly` builds only the `DxUi` library target, which has no test macros, and the engine asserts a frozen,
   clean source (false premise in part — see verifier); local runs gain a clean-tree refusal (TOOL-15).
6. Are code-owner reviews enforced on `main`, and what is the second reviewer when the only owner is also the author?
   Recommended answer: record the ruleset in `Build_UnifiedCI.md`; with one maintainer the file is advisory for bot and
   outside PRs (TOOL-03).
7. `Setup.ps1` provisions from the floating `aka.ms/vs/18/insiders` channel and accepts any `14.52.x`; the SDK props demand
   the exact recorded `VCToolsVersion` for LTCG objects. What happens when the channel moves between qualification and a
   consumer's build? Recommended answer: the exact version is already recorded in `sdk-manifest.json` and enforced by
   `DxUi.Sdk.props` (`ValidateDxUiSdkProfile`); adding it to the ZIP name is optional. Not examined further by the
   verifier.
8. Is deleting someone else's pending changelog fragment (a "withdrawal") intended to be possible in any PR without trace?
   Recommended answer: yes, by design (`Specs/Core/Core_Changes.md`: withdrawn fragments are accepted for review);
   add an explicit marker only if audit trail is wanted.
9. Does `format.yml` (write token persisted in `.git/config`, PR head checked out by the engine) rely on the engine never
   reading `.clang-format` from the PR head? Recommended answer: add the engine regression test (engine item 5).
10. `-ParallelSuites` is not used by the adapter. Is it meant to be? Recommended answer: keep it local and opt-in; do not
    wire it into CI until TOOL-19 (deadline, tree kill, stream attribution) is fixed and suites sensitive to CPU
    oversubscription (Animation, Rendering) are classified.

## Coverage

Legend: full = reviewed fully; diff + touched = diff plus the full touched functions; diff = diff only; skimmed = diff read,
bodies not traced; skipped = not reviewed (reason given).

| File | Depth |
| --- | --- |
| `.ci/project.json` | full (include/exclude reasoning checked against native and SDK inputs; TOOL-15) |
| `.github/CODEOWNERS` | full |
| `.github/actions/setup-msvc/Setup.ps1` | full (no defect; signature, timeout and tree-kill logic read) |
| `.github/actions/setup-msvc/action.yml` | full (unchanged, pins compared; TOOL-09) |
| `.github/dependabot.yml` | full |
| `.github/workflows/benchmark.yml` | full (env-only inputs, read-only token) |
| `.github/workflows/ci.yml` | full (old version diffed; byte-identical to the engine template) |
| `.github/workflows/format.yml` | full (write-token scope reviewed) |
| `.github/workflows/gallery.yml` | diff (deleted; old file read) |
| `.github/workflows/qualification.yml`, `qualify.yml`, `security.yml` | full (byte-identical to engine templates) |
| `Test-Changes.ps1` | diff + touched (PrePush changelog block) |
| `build.ps1` | diff + touched (whole main flow read) |
| `test.ps1` | diff + touched (line 50 to end) |
| `test-consumer.ps1` | diff + touched (header and changed regions) |
| `gallery.ps1` | diff (one text line) |
| `performance-profile.ps1` | diff (parameter additions) |
| `Tools/Changelog.psm1` | full (shim) |
| `Tools/Modules/Documentation/Changelog.psm1` | full (TOOL-11) |
| `Tools/Fold-Changelog.ps1`, `Tools/Release-Changelog.ps1`, `Tools/Test-Changelog.ps1` | full |
| `Tools/Submit-ChangelogFold.ps1` | full (TOOL-10) |
| `Tools/Test-ChangelogWorkflow.ps1` | diff + touched (harness, submission, release, junction, PrePush and timeout cases; the first ~15 small cases skimmed) |
| `Tools/Commit-Gallery.ps1` | diff + touched (TOOL-13) |
| `Tools/ConsumerQualification.psm1` | diff + touched (snapshot, retry and native-module functions) |
| `Tools/ConsumerUpdate.psm1` | diff + touched (TOOL-18) |
| `Tools/Invoke-ProjectCi.ps1` | full (TOOL-13, 14, 21) |
| `Tools/New-SdkPackage.ps1` | full (TOOL-04, 15) |
| `Tools/ParallelSuites.psm1` | full (TOOL-19) |
| `Tools/ToolingTestRunner.psm1`, `Tools/ToolingTestChild.ps1` | full |
| `Tools/TestRunEvidence.psm1` | diff + touched (native `Pin`/`Identity` and finalizer) |
| `Tools/ScopedTesting.psm1` | diff + touched (PR contract digest and coverage; TOOL-13) |
| `Tools/Validation.psm1` | diff + touched (rich-text matrix check, dependency pins, include roots, `Invoke-DxUiValidation`) |
| `Tools/PerformancePolicy.psm1` | diff + touched (tracking-log path rule traced through `Resolve-PerformanceInputPath`) |
| `Tools/PerformanceProfiling.psm1` | diff (v3 boundary assertions and drain timing; bodies not traced) |
| `Tools/PerformanceComparison.psm1`, `Tools/PerformanceAcceptancePolicy.v1.json`, `Tools/PairedRun.psm1` | diff (fingerprint paths verified to exist) |
| `Tools/Generate-MetadataCorpus.ps1`, `Generate-NativeMathSymbols.ps1`, `Generate-NativeMathTestData.ps1` | full |
| `Tools/Generate-MarkdownUnicodeData.ps1`, `Generate-NativeFlowchartTestData.ps1` | diff |
| `Tools/Get-NativeScope.ps1`, `Tools/NativeScope.psm1`, `Tools/Test-CiGate.ps1` | diff (comment-only changes) |
| `Tools/README.md` | diff (read for drift; TOOL-12) |
| `Build/BuildConsole.psm1` (outside the pathspec, tooling gate) | full |
| `Build/NativeBuildPolicy.targets`, `Build/PinnedDxUiPolicy.targets` | skimmed (PDB, ICF and warning items only) |
| `Tests/test-scopes.json` | diff (reformatting and PR-coverage removal) |
| `Tools/tests/Invoke-ToolingTests.ps1` | full (roster checked against `Tools/tests/`) |
| `Tools/tests/Test-ToolingTestRunner.ps1`, `Test-ParallelSuites.ps1`, `Test-BuildConsole.ps1` | full (TOOL-06, 20) |
| `Tools/tests/Test-NativeScope.ps1`, `Test-BenchmarkGate.ps1` | diff (removed workflow-text tests inspected; TOOL-02, 07) |
| `Tools/tests/Test-MarkdownData.ps1`, `Test-MsvcSetup.ps1`, `Test-ReleaseBuildPolicy.ps1` | diff |
| `Tools/tests/Test-CodeOwners.ps1`, `Test-ConsumerUpdate.ps1`, `Test-MsvcSetupEntrypoints.ps1`, `Test-BenchmarkHost.ps1` | diff |
| `Tools/tests/Test-ConsumerQualification.ps1` (+489), `Test-Docs.ps1`, `Test-Validation.ps1`, `Test-PairedProjectProfiles.ps1`, `Test-PairedRun.ps1`, `Test-PerformancePolicy.ps1`, `Test-PerformanceProfiling.ps1`, `Test-RunEvidence.ps1`, `Test-ScopedTesting.ps1` | skimmed (diff and assertions counted, cases not traced; not independently validated by the reviewer; the verifier read the cited assertions in `Test-Validation.ps1`, `Test-ScopedTesting.ps1`) |
| `Tools/tests/fixtures/HostedPairedGate-2026-10-01/README.md` | skipped (one link text change) |
| `.gitattributes`, `.gitignore` | diff (EOL rules used for TOOL-13; ignore entries for the lease and engine checkout) |

### Checked and holds

These were examined by the reviewer and not contradicted by the verifier; the next reader need not redo them.

- No `pull_request_target` and no `workflow_run` trigger anywhere under `.github/`; the sealed workflows are
  byte-identical to the engine templates.
- No `${{ ... }}` interpolation of event data into a `run:` block; every user-controlled input (`pr_number`,
  `expected_head`, `benchmark_*`) reaches a script through `env:`.
- All third-party actions are pinned by full SHA and the SHAs match their stated tags (`actions/checkout` v7.0.1,
  `actions/upload-artifact` v7.0.2 and v4.6.2); the one drift is TOOL-09.
- Job permissions are narrow: `contents: read` by default; writes only in `format.yml`, `qualify.yml`, and the `resolve`,
  `report` (checks) and `coordinate` jobs of `ci.yml`.
- The engine pin in `.ci/project.json` equals every caller's `uses:` ref and `engine_ref:` (11 places, all `e32cf9f9...`).
- The zero-warning gate fails closed: `Get-NativeBuildDiagnostics` (`Build/BuildConsole.psm1:4-55`) rejects a missing log,
  an empty log, a truncated footer, a reordered footer and a `Build FAILED.` tail, and `Tools/tests/Test-BuildConsole.ps1`
  covers each.
- Adapter commands cannot silently pass: `Invoke-CiProjectScript` runs every command as `pwsh -File` and throws on a nonzero
  exit; `Get-CiSuiteEvidence` rejects zero discovery and a filtered run.
- Destructive operations in non-test tooling are narrow: one fallback `Remove-Item -Recurse` on a worktree path this
  process created (`Tools/Submit-ChangelogFold.ps1:152`), `[IO.File]::Delete` of validated fragment and stage paths, one
  single-file `Remove-Item` of a build receipt. No `Stop-Process`; every `Kill` targets a handle this process started.
- `Tools/Submit-ChangelogFold.ps1` cannot loop (Actions never fold; a fold PR touches only `CHANGELOG.md` and
  `Changes/*.md`, which `.ci/project.json` excludes from native routing) and never force-pushes; it refuses a remote branch
  that is not exactly one commit on the selected head with only `M CHANGELOG.md` / `D Changes/x.md` changes.
- Hosted evidence at `6df17007`: the x64 regression job of the `Qualify main Release` run 41459331431 completed
  successfully (it runs the full tooling roster); its ARM64 job was still running when checked.
