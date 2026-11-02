# Documentation, specifications and process: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: whether what the repository says is true,
consistent, usable and maintainable: root and `docs/` pages, `Specs/` contracts, plan folders, `capabilities.json`,
`Changes/` and `CHANGELOG.md`, `Measurements/` evidence and its privacy, third-party notices and the SDK packager.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

Result: one P1 (the SDK ZIP and every linked product ship compiled Unicode and WHATWG data without their license
notices), eight P2 and fourteen P3. No whole finding was refuted; sub-claims that were wrong are listed under Refuted.
The capability table, the numeric limits behind it, the C++ examples in `docs/` and every documented script parameter
were found accurate by the reviewer; the defects are stale present-tense prose left by the plan migration and the
unified-CI migration, evidence that a reader cannot follow or reproduce, and repository hygiene. Verifier changes:
DOCS-07 raised to P1; DOCS-03, 08, 09, 10, 13 and 15 lowered to P3.

Context for readers: the live remote `main` is `5fc4d4c3` (PR #160, after the review HEAD `6df17007`) and already adds
`docs/upgrade.md`, which partly overtakes DOCS-16. The local remote-tracking refs are not stale
(`git ls-remote origin` and `git branch -r` agree). Nothing else between `6df17007` and `5fc4d4c3` touches a cited
location.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| DOCS-07 | P1 | Confirmed | Unicode and WHATWG license notices missing from `THIRD-PARTY-NOTICES.md` and the SDK ZIP | `Tools/New-SdkPackage.ps1:66` | S |
| DOCS-01 | P2 | Confirmed | API3, API4 and SDK qualification subjects are commits that exist on no remote | `Measurements/WipDelivery_2026-11-02/README.md:9` | S |
| DOCS-02 | P2 | Confirmed | Stale "in development" and "in progress" statements contradict implemented capabilities | `docs/rich-text.md:1168` | S |
| DOCS-04 | P2 | Confirmed | 27 evidence links land on a README that contains none of the cited packets | `Specs/UI/UI_ControlsAndLayout.md:1590` | M |
| DOCS-05 | P2 | Confirmed | Done plans keep about 400 unchecked boxes that contradict their own closeout | `Specs/Plans/Done/ProductionReview_2026-10-05.md:163` | M |
| DOCS-06 | P2 | Confirmed | CI description is stale after the unified-CI migration | `README.md:61` | S |
| DOCS-11 | P2 | Confirmed | JSON receipts stored as `*.json.receipt.txt` bypass the receipt validator; catalog is not machine-verified | `Tools/Validation.psm1:162` | M |
| DOCS-12 | P2 | Confirmed | Public receipts carry the account name, hostname and a whole-machine process snapshot | `Measurements/WipDelivery_2026-11-02/raw/performance/semantic/delivery-01/preflight.json.receipt.txt` | M |
| DOCS-14 | P2 | Confirmed | Evidence is 90% of the tracked tree; one packet is 79% of the window's added lines | `Measurements/` | L |
| DOCS-03 | P3 | Confirmed | Docs and contracts call Done plans "active" and delegate ownership to them | `docs/rich-text.md:346` | S |
| DOCS-08 | P3 | Confirmed | `Measurements/README.md` omits the newest packet and nine other folders | `Measurements/README.md` | S |
| DOCS-09 | P3 | Confirmed | `ThirdParty/Markdown/README.md` describes 3 limited profiles and 18 unsupported families; the matrix has 8 and 13 | `ThirdParty/Markdown/README.md:20` | S |
| DOCS-10 | P3 | Confirmed | Spec authority page and contract headers have drifted | `Specs/README.md:35` | S |
| DOCS-13 | P3 | Confirmed | 413 unfolded fragments, 364 dated the same day, will fold in reverse-alphabetical order | `Changes/` | M |
| DOCS-15 | P3 | Plausible | `UI_ControlsAndLayout.md` is a 2,864-line contract with one 1,528-line section | `Specs/UI/UI_ControlsAndLayout.md:960` | L |
| DOCS-16 | P3 | Plausible | No pin guidance for the pre-adoption conditions (partly overtaken by PR #160) | `docs/getting-started.md:24` | S |
| DOCS-17 | P3 | Plausible | `capabilities.json` has a stale identifier and lacks two pending ids | `capabilities.json:116` | S |
| DOCS-18 | P3 | Plausible | Design-system RichTextEditor guideline and `docs/images` captures lag the window | `Specs/DesignSystem/components/RichTextEditor/README.md:73` | M |
| DOCS-19 | P3 | Confirmed | `docs/rich-text.md` is a reference, not a usage guide | `docs/rich-text.md:1635` | L |
| DOCS-20 | P3 | Confirmed | `FrameUpdateAccounting` plan has no status line and a stale open item | `Specs/Plans/WIP/FrameUpdateAccounting_2026-10-26.md:16` | S |
| DOCS-21 | P3 | Plausible | Closeout says repairs are "covered" where the packet's own draft says the test is not located | `Specs/Plans/Done/README.md:74` | M |
| DOCS-22 | P3 | Plausible | ARM64 status is described four different ways | `Specs/Build/Build_UnifiedCI.md:29` | S |
| DOCS-23 | P3 | Plausible (one sub-claim refuted) | Small documentation defects | `Specs/Core/Core_Documentation.md:40` | S |

## P1 findings

### DOCS-07 — Unicode and WHATWG license notices missing from `THIRD-PARTY-NOTICES.md` and the SDK ZIP

- **Severity** P1 (reviewer P2, verifier P1) · **Category** repo-hygiene (legal notice) · **Verdict** Confirmed ·
  **Confidence** high on the omission, medium on legal necessity for the WHATWG table · **Effort** S · **Introduced**
  `c1a77e42` (2026-10-21, before the window; the gap exists at HEAD; `b472bea1`, 2026-11-01, added more Unicode data
  without extending the notices)
- **Location**: `THIRD-PARTY-NOTICES.md` (whole file; it ends "New dependencies or assets must extend this
  inventory"); `Tools/New-SdkPackage.ps1:66-72`; `docs/README.md:26`; `src/Controls/DxUi.TextElementsData.h:4`;
  `src/Controls/DxUi.MarkdownUnicodeData.h:1`; `src/Controls/DxUi.MarkdownEntityData.h:4-5`;
  `src/Controls/DxUi.RichBidi.h`; `ThirdParty/Unicode/17.0.0/LICENSE.txt`;
  `ThirdParty/Markdown/whatwg-2026-10-18/LICENSE.txt`
- **What**: `DxUi.lib` contains tables generated from third-party data: Unicode 17.0.0 (GraphemeBreakProperty,
  DerivedCoreProperties, emoji-data, DerivedBidiClass, CaseFolding, UnicodeData) in `DxUi.TextElementsData.h` and
  `DxUi.MarkdownUnicodeData.h`, and the 2,125-name WHATWG `entities.json` table in `DxUi.MarkdownEntityData.h`. The
  generated headers cite the in-tree license files, which only a source clone contains. `THIRD-PARTY-NOTICES.md` lists
  WIL, toml++, KaTeX and rapidyaml only, and `Tools/New-SdkPackage.ps1:66-72` copies `LICENSE.txt`,
  `THIRD-PARTY-NOTICES.md`, WIL, KaTeX, toml++ and rapidyaml (+bundled) into the ZIP's `licenses/` folder: no Unicode
  License V3 text and no WHATWG CC-BY-4.0/BSD-3-Clause text. Unicode License V3 permits redistribution of "Data Files
  or Software" only if the notice appears with all copies or in associated documentation; BSD-3-Clause clause 2
  requires binary redistributions to reproduce the notice; CC-BY-4.0 requires attribution. Commit `803091f5` ("Include
  KaTeX symbol notice in SDK") shows the set was extended once, so this is an omission, not a decision. No validator
  or tooling test compares the SDK license set with the data compiled in (searched `Tools/tests`, `validate*.ps1`,
  `.github`). Additionally the copied `THIRD-PARTY-NOTICES.md` keeps repository-relative links
  (`docs/licenses/WIL.txt`) that do not exist in the ZIP, where the files are renamed (`licenses/WIL-LICENSE.txt`).
- **Failure scenario**: an SDK ZIP built by `Tools/New-SdkPackage.ps1` is handed to a consumer or an application that
  links `DxUi.lib` ships (RedSalamander, RedXe, RedPrism, RedTilde). The product embeds a modified copy of the Unicode
  data files and of the WHATWG entity table; the only notice set it received omits both licenses, so the product's own
  third-party notice page cannot carry them either. Today SDK ZIPs are internal artifacts (no publication workflow),
  so no external redistribution has occurred; the first one that leaves the owner's control is non-compliant.
- **Fix**:
  1. Copy the two license texts byte for byte into `docs/licenses/unicode-v3.txt` (from
     `ThirdParty/Unicode/17.0.0/LICENSE.txt`) and `docs/licenses/whatwg-entities.txt` (from
     `ThirdParty/Markdown/whatwg-2026-10-18/LICENSE.txt`), next to the existing license files.
  2. Add two entries to `THIRD-PARTY-NOTICES.md` in the existing style: Unicode data (Unicode License V3, Copyright
     1991-2026 Unicode, Inc.; derived range tables in the three headers named above) and WHATWG entity names
     (Copyright WHATWG; CC-BY-4.0 with BSD-3-Clause for source-code portions). Add one sentence that the CommonMark,
     GFM, toml-test, yaml-test-suite, Mermaid and KaTeX documentation corpora under `ThirdParty/` and
     `docs/licenses/toml-test.txt` and `yaml-test-suite.txt` are test inputs and are not distributed.
  3. In `Tools/New-SdkPackage.ps1` copy both files into `licenses/` (for example `Unicode-LICENSE.txt` and
     `WHATWG-LICENSE.txt`) and stage the notices file with its relative `docs/licenses/` links rewritten to the
     renamed `licenses/` files, so every link in the shipped copy resolves inside the ZIP.
  4. Correct `docs/README.md:26` to say the notices cover WIL, toml++, rapidyaml, KaTeX, the Unicode data and the
     WHATWG entity table.
  5. Add a gate in `Test-DxUiDependencies` (`Tools/Validation.psm1:477`): every header under `src/` or `include/`
     whose first 15 lines name a `ThirdParty/**/LICENSE*` path must have that license named in
     `THIRD-PARTY-NOTICES.md` and a file in `docs/licenses/`; and the license file names staged by
     `New-SdkPackage.ps1` must include every `docs/licenses/` file that the notices file marks as shipped.
  6. Tell the consumers: `docs/upgrade.md` (live on `origin/main` since PR #160) should state that applications
     linking `DxUi.lib` carry the notices of `licenses/` in their own distribution.
- **Regression test**: `Tools/tests/Test-Validation.ps1`, fixture style of `Set-DependencyFixture`: add a header whose
  comment cites `ThirdParty/Unicode/17.0.0/LICENSE.txt` with a notices file that does not mention Unicode, assert
  `Test-DxUiDependencies` reports a missing-notice failure; add the entry and the `docs/licenses` file, assert it
  passes. A second case parses the `licenses/` copy list of `Tools/New-SdkPackage.ps1` and asserts both new names are
  present. The extracted-ZIP smoke already run by `Tools/New-SdkPackage.ps1` should assert
  `licenses/Unicode-LICENSE.txt` and `licenses/WHATWG-LICENSE.txt` exist and that every relative link in
  `licenses/THIRD-PARTY-NOTICES.md` resolves. Both fail at `6df17007`.
- **Verifier notes**: confirmed from the file contents and the packager text. Raised to P1 because it is a
  license-condition defect in the release artifact and a prerequisite of the first external SDK or application
  release; the fix is two file copies and a paragraph. Whether a name-to-code-point table is copyrightable is
  arguable, but the repository itself elected the CC-BY/BSD terms; counsel confirmation is advisable but not required
  to justify the fix.

## P2 findings

### DOCS-01 — API3, API4 and SDK qualification subjects are commits that exist on no remote

- **Decision (2 November, D10)**: resolved for publication. `a5b3ac81`, `a8cb8e1d` and `68f6b15e` are now on `origin`
  under their original branch names, so the evidence is reproducible from the remote; see the plan's D10 entry.
- **Severity** P2 (reviewer P2, verifier P2; escalate to P1 if RedPrism or RedXe are to adopt on this evidence) ·
  **Category** claim · **Verdict** Confirmed · **Confidence** high · **Effort** S
- **Location**: `Measurements/WipDelivery_2026-11-02/README.md:9-10`;
  `Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md:30-31`; `Specs/Plans/Done/DeliveryReview_2026-10-31.md:5`;
  `Specs/Plans/Done/README.md:80`
- **What**: The packet states that API3 compatibility source `a5b3ac81` and API4 source `a8cb8e1d` each "passed six
  builds, 15 ordinary and nine foreground executions", and that production SDK source `68f6b15e` passed extracted
  consumer smoke and the 391-file header closure. None of the three is an ancestor of `6df17007`.
  `git branch -r --contains` returns nothing for any of them, and none of the 159 `refs/pull/*/head` SHAs on the live
  remote has them as an ancestor. They live only on local branches `ej/dxui-compat-redprism-menu-20261101` (14 commits
  ahead of merge-base `8e899407`), `ej/dxui-compat-redxe-menu-20261101` (11 ahead of `fa4da2cb`) and
  `ej/dxui-delivery-sdk-20261102` (1 ahead, 7 behind main); each is a linked worktree under the private
  `DxUi-worktrees` folder. `git ls-remote --heads origin` lists only the 28 October
  `ej/wall-compat-{redprism,redsalamander,redxe}-20261028` heads, which are strict ancestors of `a5b3ac81` and
  `a8cb8e1d`, so the qualified compat sources are newer than anything a consumer can fetch. `68f6b15e` has the same
  author, date and subject as main's `21a74896` but a different tree; `src/` and `include/` are identical to HEAD, the
  difference is the delivery packet, gallery PNGs and a few docs.
- **Failure scenario**: RedPrism or RedXe reads the packet, concludes API3/API4 compatibility is qualified at those
  SHAs, and tries `git fetch` of the SHA or a branch: nothing exists. 135 of the 145 local branches have no upstream,
  so the only copy of the qualified compat sources is this workstation.
- **Fix**: Push the compat branches (or tag the commits, for example `qualified/api3-a5b3ac81`) before the packet is
  cited; add a "Subject branches" line to the packet README with branch, full 40-character SHA and the observed result
  of `git branch -r --contains`. For the SDK, either push `ej/dxui-delivery-sdk-20261102` or re-bind the SDK receipt
  prose to `21a74896` and state that `68f6b15e` differs only outside `src/`, `include/` and `Tests/`. Add a check to
  `Test-PrePush.ps1` (not to the offline validators): every 40-hex SHA in a changed `Measurements/**/README.md` must
  be reachable from HEAD or from some `origin/*` ref, with an explicit offline skip that reports itself as skipped.
- **Regression test**: a tooling test with a throw-away git repository (pattern of `Tools/tests/Test-Validation.ps1`
  fixtures): a README citing a SHA that exists only on an unpushed branch makes the new check fail; the same SHA after
  `git update-ref refs/remotes/origin/x` passes; an abbreviated or nonexistent SHA fails.
- **Verifier notes**: not stale. The live remote `main` is `5fc4d4c3` and equals the local `origin/main`; the three
  commits are still unreachable from it. Packet SHAs also appear in `Specs/Plans/Done/README.md:80`.

### DOCS-02 — Stale "in development" and "in progress" statements contradict implemented capabilities

- **Severity** P2 (reviewer P2, verifier P2) · **Category** claim · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location** (all at HEAD): `docs/rich-text.md:1168-1169` ("math/diagram rendering and metadata validation remain in
  development"), `:431` ("in development under M6"), `:561-566` ("in-progress drop-at-caret implementation ... remain
  pending"); `Specs/UI/UI_ControlsAndLayout.md:2373-2374` ("Footer projection and navigation are in development");
  `Specs/UI/UI_InputAndAccessibility.md:1253-1254` ("integration in development under M5a"); `README.md:36-38`
  ("Complete semantic rendering/editing and the accepted math/diagram extensions remain in progress");
  `docs/controls.md:398,404,406` (flowchart/"state/class" only, "remain under the active plan");
  `Measurements/RichText/README.md:17-19` ("File drag-and-drop remains a spec and test plan" under implemented scope)
- **What**: `Specs/Core/Core_Documentation.md` says the capability table is an exact projection of `capabilities.json`
  and that docs distinguish implemented behavior from pending. The tables are right (`docs/rich-text.md:28-37`,
  `capabilities.json:177` list native math, flowchart, state, class, sequence, pie, ER, Gantt, mind map, footnotes,
  file drop and metadata validation as Limited or Supported); the surrounding prose still says the same features are
  in development. M6, the file-drop plan and footnote preview are all in Done or in `capabilities.json` `supported`.
- **Failure scenario**: an application developer reads `docs/rich-text.md:1168` and avoids math/diagram or metadata
  validation that the capability table two screens above lists as available; or a reviewer of
  `UI_ControlsAndLayout.md:2373` concludes footnote navigation is unimplemented and re-opens finished work.
- **Fix**: Replace each phrase with the capability-table wording ("Limited; physical IME/AT and performance acceptance
  remain open") or delete the sentence. In `docs/controls.md:398,406` name the full Mermaid family set or point to the
  table. Drop the drag-and-drop sentence from `Measurements/RichText/README.md:17-19`. Add a doc-lint to
  `Test-DxUiDocs` (`Tools/Validation.psm1:286`, running on `Get-MarkdownProse` so code blocks are excluded) over
  `README.md`, `docs/*.md` and `Specs/{Core,UI,Rendering,Build,Testing}/*.md`: fail on `in development`,
  `in-progress`, `under M\d`, `remain(s)? in progress` and `active (qualification )?plan`, with an explicit per-line
  allow marker for legitimate uses. Do not apply it to WIP or Done plan bodies (historical).
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture: a docs page containing "remains in development" fails
  `Test-DxUiDocs`; the same text inside a fenced block or after the allow marker passes. Run against the repository,
  it fails at `6df17007` for the locations above.
- **Verifier notes**: every quoted line reproduced at HEAD.

### DOCS-04 — 27 evidence links land on a README that contains none of the cited packets

- **Severity** P2 (reviewer P2, verifier P2) · **Category** link (claim not backed in the tree) · **Verdict**
  Confirmed · **Confidence** high · **Effort** M
- **Location**: `docs/rich-text.md:9,409,566,569,570,1008,1131,1133,1191,1247,1260,1286,1412,1480` (14);
  `Specs/UI/UI_ControlsAndLayout.md:1590` and four more; `Specs/UI/UI_InputAndAccessibility.md` (1);
  `Specs/Core/Core_PerformanceAndResources.md` (5); `docs/performance.md` (1); `README.md:38`
- **What**: Examples: "The sealed M3 qualification retains 57 fresh complete ordinary x64 scope/profile runs, six root
  builds, 12 isolated probes, 324 exact raster pairs" (`docs/rich-text.md:1191-1193`); "The bounded M4f implementation
  is functionally qualified" (`Specs/UI/UI_ControlsAndLayout.md:1590`, a normative contract). Both link to
  `Measurements/RichText/README.md`, which contains no `M2`, `M3`, `M4e` or `M4f` (and neither does
  `Measurements/RichText/2026-11-01-usage-history.md`), and which states that raw logs, receipts and replay packets
  are private and that 19,074 raw files were moved to an owner-retained archive (`richtext-details-8fe2d5f5.zip`,
  lines 165-176). The history file does repeat the "57 ordinary x64 suite runs" figure but not the packet names; "324
  exact raster pairs" appears nowhere in the tree.
- **Failure scenario**: a contract reader follows "functionally qualified" to its evidence and finds a README with no
  such packet and an archive that is not in the repository; the claim cannot be audited.
- **Fix**: In usage docs, drop packet-level claims (counts of runs and raster pairs) and keep one sentence plus a link
  to `Measurements/RichText/2026-11-01-usage-history.md`. In normative specs replace "functionally qualified" with a
  statement of which tests exist (name the test file or suite) and link the real file. Where a packet claim must stay,
  add a heading per packet to `Measurements/RichText/README.md` and link the anchor. Add a link-text rule to
  `Test-DxUiDocs`: a link whose text or sentence names a milestone code `M\d+[a-z]?` must target a file that contains
  that code.
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture: a spec sentence "The M4f implementation is qualified"
  whose word "qualified" links to a fixture README that lacks `M4f` fails; adding an `M4f` heading and linking that
  anchor passes. The rule fails at `6df17007` for the 27 links.
- **Verifier notes**: link count re-derived (14 + 5 + 5 + 1 + 1 + 1 = 27). The existing link check (`Test-DxUiSpecs`,
  `Tools/Validation.psm1:329`) strips the fragment (`Split('#', 2)[0]`), so it cannot catch this.

### DOCS-05 — Done plans keep about 400 unchecked boxes that contradict their own closeout

- **Severity** P2 (reviewer P2, verifier P2) · **Category** process · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M
- **Location**: unchecked `- [ ]` counts in `Specs/Plans/Done/*.md` at HEAD: ProductionReview_2026-10-05 118,
  CodeReview_2026-10-28 54, RichTextAudit_2026-10-21 54, CodeReview_2026-10-31 42, RichTextEditor_2026-10-17 35,
  CodeReview_2026-10-10 32, DeliveryReview_2026-10-31 11, Msvc1452ReleaseOptimization_2026-10-23 10, WipFolderQuality
  7, SharedLibraryReadiness 7, RichTextFileDrop 6 and nine more with 2-4 each: 400 in 20 plans, 413 with the 13 in
  `Specs/Plans/Done/RichTextAudit_2026-10-21/RedTildeCompletion_2026-10-24.md`
- **What**: `Specs/Plans/Done/ProductionReview_2026-10-05.md:29-30` says its checkboxes "remain source-specific
  records, not a second current qualification queue", while `:163-164` says they "remain open until their associated
  qualification obligations finish" and `:192-207` lists `[ ] Q1/Q2 ...` as open.
  `RichTextEditor_2026-10-17.md:3107-3111` leaves `[ ] M5c` (native math) and `[ ] M5d` (Mermaid flowchart) open
  although both are implemented (`src/Controls/DxUi.NativeMath.cpp`, `DxUi.NativeFlowchart.cpp`), and `:3356-3358`
  says drag-and-drop "implementation/runtime tests have not started" although `include/DxUi/FileDrop.h` exists.
  `CodeReview_2026-10-31.md:89,194` still says Grid and Tree do not override `OnBoundsChanged`;
  `src/Controls/DxUi.Grid.cpp:6490` and `src/Controls/DxUi.Tree.cpp:2961` do. `CodeReview_2026-10-28.md` from line 81
  lists 54 `[ ] R28-nn ... [confirmed]` defects with no per-row disposition; the real dispositions are in
  `Measurements/WipDelivery_2026-11-02/raw/historical/historical-per-id.md.txt` (45 rows still say "PARTIAL TRACE").
  `AGENTS.md` and `Specs/README.md` require completing or handing off before moving a plan to Done.
- **Failure scenario**: a maintainer greps Done plans for open `[confirmed]` defects to find work, finds 54 R28 items
  and reopens fixed code; or concludes M5c/M5d are not implemented.
- **Fix**: One mechanical pass that converts every box in a Done plan to `[x]` (done), `[-]` (rejected, with the
  packet id) or `[>]` (handed to
  `Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#shared-qualification-ownership-16-october-2026`), generated
  from the existing per-ID tables; document the three marks in `Specs/README.md`. Delete the contradicting sentence at
  `ProductionReview_2026-10-05.md:163-164`. Add a `Test-DxUiSpecs` rule (`Tools/Validation.psm1:329`, using
  `Get-MarkdownProse`): a file under `Specs/Plans/Done/` may contain no `- [ ]`. Apply the rule in the same change as
  the pass, since 413 current violations would otherwise fail the build; if the pass is deferred, add the two-line
  banner of `ProductionReview_2026-10-05.md:29-30` to every affected Done plan first.
- **Regression test**: `Tools/tests/Test-Validation.ps1` fixture: a `Specs/Plans/Done/X.md` with `- [ ] item` fails
  `Test-DxUiSpecs`; with `- [x]`, `- [-]` or `- [>]` it passes; a `- [ ]` inside a fenced block passes.
- **Verifier notes**: counts re-derived with a loop over the 20 files. Corrections to the report: 23 Done files carry
  "Status: DONE" (13 also carry "Historical pre-closeout"), the window added 26 Done plan files (16 renamed from WIP,
  10 added directly); the substance is unaffected.

### DOCS-06 — CI description is stale after the unified-CI migration

- **Severity** P2 (reviewer P2, verifier P2) · **Category** claim · **Verdict** Confirmed · **Confidence** high for
  `README.md` and `docs/performance.md`, medium for `Testing_Validation.md` · **Effort** S
- **Location**: `README.md:61-62`; `docs/performance.md:201-203` and `:418-420`;
  `Specs/Testing/Testing_Validation.md:313-322` and `:454-455`
- **What**: `README.md:61` says native x64/ARM64 Debug/Release/ASan Debug CI produces per-suite receipts.
  `Specs/Build/Build_UnifiedCI.md:29,47-48` says automatic main qualification executes native Release on x64 and ARM64
  and that Debug, ASan and paired benchmarks run through explicit manual qualification with no scheduled native
  builds; `Build_UnifiedCI.md:5` says older references to automatic native PR matrices are superseded.
  `.github/workflows/ci.yml` is an `ubuntu-24.04` coordinator with 5-minute jobs; `windows-11-vs2026-arm` appears
  nowhere in `.github/` or `.ci/`; only `.github/workflows/benchmark.yml` has a 240-minute limit.
  `docs/performance.md:201-203` describes a 180/240-minute `ci.yml` study and `:418-420` native ARM64 runs "configured
  in CI on `windows-11-vs2026-arm`". `Testing_Validation.md:313-322` lists per-profile ceilings (x64 90/105, ARM64
  150/155/165 minutes) for six profiles.
- **Failure scenario**: a developer opens a PR expecting the README's six-profile native matrix, or a maintainer
  debugs a `ci.yml` timeout that is not configured.
- **Fix**: Rewrite the three passages in terms of `Build_UnifiedCI.md` (automatic = Release x64 and ARM64; Debug and
  ASan = manual dispatch; hosted ceilings live in the shared engine pin). Drop workflow file names, runner labels and
  minute counts from usage docs. State in the README that the six-profile matrix is the local and manual qualification
  requirement. For `Testing_Validation.md` keep the ceilings only if the shared engine still enforces them; otherwise
  say they apply to local and manual runs.
- **Regression test**: `Tools/tests/Test-Docs.ps1`: every `.github/workflows/*.yml` name and runner label mentioned in
  `README.md` or `docs/*.md` must exist in `.github/workflows/` or `.ci/project.json`; fixture mentioning
  `windows-11-vs2026-arm` with no such label fails, and fails for the repository at `6df17007`.
- **Verifier notes**: confirmed against the workflow files. The "required native matrix" sentence at
  `Testing_Validation.md:454-455` is defensible as the manual requirement; the other citations are not.

### DOCS-11 — JSON receipts stored as `*.json.receipt.txt` bypass the receipt validator; catalog is not machine-verified

- **Severity** P2 (reviewer P2, verifier P2) · **Category** process · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M
- **Location**: `Tools/Validation.psm1:162-193` (`Test-DxUiMeasurements` enumerates `*.json` only);
  `Measurements/WipDelivery_2026-11-02/` (912 `*.json.receipt.txt`, 0 `*.json`); `.gitattributes:73`
- **What**: `Specs/Core/Core_Documentation.md` requires each reviewed benchmark JSON receipt to carry its real DxUi
  workload owner, fixture-input hashes and a neighboring scenario explanation. The validator sees `*.json` only; the
  packet stores its 144 paired reports, semantic study and manifests as `.json.receipt.txt`. No script, test or
  workflow references `delivery-manifest.receipt.txt` or `raw-receipt-catalog.receipt.txt` (searched `Tools`, `Tests`,
  `.github`, `validate*.ps1`). The reviewer recomputed all 1,203 catalog entries (SHA-256 and byte length, none
  missing or extra) and found the packet sound today, but nothing keeps it so; the path-shortening repair in the
  window moved 434 files.
- **Failure scenario**: a later edit or a Git normalization (`.gitattributes:73` protects only this one packet)
  changes a receipt byte; no validator notices until a person re-hashes.
- **Fix**: Add `Test-DxUiReceiptPackets` to `Tools/Validation.psm1`, called from `Test-DxUiSpecs`: for every
  `*receipt.txt` catalog or manifest in a packet, recompute SHA-256 and length of each listed path and fail on
  mismatch, missing or unlisted files under `raw/`; parse every `*.json.receipt.txt` as JSON and apply the same
  `workloadOwner` and `benchmarkInputs` rules as `Test-DxUiMeasurements`. Pitfall: that function requires a
  `README.md` next to each receipt; for packets accept the nearest ancestor README inside the packet, otherwise every
  raw subfolder fails. About 1,200 files and 22 MB hash in seconds.
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture packet: a catalog entry with a wrong hash fails; a
  `x.json.receipt.txt` without `benchmarkInputs` fails with "Missing fixture input hashes"; an unlisted file under
  `raw/` fails; the corrected packet passes.
- **Verifier notes**: correction to the report: `*.json.receipt.txt` files in all of `Measurements/` number 1,076, of
  which 912 are in this packet, so only 164 are earlier files (not 1,076). The 1,203-hash recomputation was not
  repeated.

### DOCS-12 — Public receipts carry the account name, hostname and a whole-machine process snapshot

- **Decision (2 November, D9)**: redact going forward **and** rewrite Git history to remove the existing identity
  data; the procedure, verification and consumer re-pin steps are in the plan's "History rewrite (D9)" section.
- **Severity** P2 (reviewer P2, verifier P2; deliberately not P1, see verifier notes) · **Category** repo-hygiene
  (privacy) · **Verdict** Confirmed · **Confidence** high on presence, medium on policy intent · **Effort** M
- **Location**: `Measurements/WipDelivery_2026-11-02/raw/performance/semantic/delivery-01/preflight.json.receipt.txt`
  (915 process records); `Measurements/WipDelivery_2026-11-02/raw/receipts/relocated-consumer/*/stdout.log:25`;
  `Measurements/WipDelivery_2026-11-02/raw-receipt-catalog.receipt.txt` (private worktree paths in 264 packet files);
  `Tools/PerformancePolicy.psm1:1035` and `performance-paired.ps1:383` (write `machine` from
  `[Environment]::MachineName`); `Tools/PerformanceComparison.psm1:7,212` (`machine` is a comparison key);
  `Specs/Core/Core_Documentation.md:56-57`; `Specs/Testing/Testing_Validation.md:12`
- **What**: Tracked files at HEAD: the workstation hostname appears as the receipt field `machine` in 1,152
  `Measurements/` files (147 in the 2 November packet, none outside `Measurements/`); the developer's Windows account
  name appears in a profile path in 963 `Measurements/` files (Performance 444, UiComposition 217, Layout 119,
  CiBudget 104, others 79; 5 in the packet) and in two Done plans. The preflight snapshot lists every running process
  on the workstation (`ProcessId`, `ParentProcessId`, `Name`, `ExecutablePath`, `CreationDate`) with 221 executable
  paths under the user profile; two `intent.json.receipt.txt` files hold full command lines that contain only suite
  names. The data predates the window for the most part; the window added the process snapshot and the packet's 147 +
  5 files. A sweep of all tracked non-ThirdParty files found no token, key or credential shapes, no environment dumps,
  only GitHub-masked `AUTHORIZATION: basic ***` headers and `GITHUB_TOKEN Permissions` headings, and the developer's
  own e-mail address in 16 older CI API dumps (already public in commit metadata). `Core_Documentation.md:56-57` says
  private machine diagnostics stay in generated output or an authorized private archive; the process snapshot
  contradicts it.
- **Failure scenario**: anyone cloning the public repository learns the workstation's name, the account name, the
  installed and running software of the machine on 31 October, and the private worktree layout; none of that is needed
  to replay a benchmark.
- **Fix**: (1) Write `machine` as a stable alias at write time, for example a `machineId` of the first 12 hex digits
  of the SHA-256 of the machine name, and let `Tools/PerformanceComparison.psm1:212` compare `machineId` when present
  and `machine` for legacy receipts; version the receipt schema so old and new pairs are not mixed silently. (2) The
  semantic-study collector that wrote `processSnapshot` is not in the repository (the probe tooling is "private and
  frozen" per the packet README), so redact on publication: replace `preflight.json.receipt.txt` by a derivative
  holding the process count and the names of processes that matter (compilers, browsers, DxUi), and give the catalog
  entry an explicit "withheld: privacy" disposition with the original SHA-256 and size. (3) Add a generic validator
  rule (no hard-coded names): in files added or changed under `Measurements/` fail on a path matching
  `[A-Za-z]:[\\/]+Users[\\/]+[^\\/"\s<]+` and on a JSON array key `processSnapshot` or an `ExecutablePath` array
  longer than 20 entries; historical files are listed in a baseline file so the rule applies to new evidence only. (4)
  Do not rewrite the 1,152 + 963 historical hash-pinned files; record that decision in
  `Specs/Core/Core_Documentation.md`.
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture: a new `Measurements/X/r.json` containing
  `C:\\Users\\alice\\AppData` fails, with `C:\\Users\\<user>\\` it passes, and a file with 30 `ExecutablePath` entries
  fails. A comparison test in `Tools/tests/Test-PerformanceComparison.ps1`: two receipts with the same `machineId` and
  different raw names compare; with different `machineId` they refuse.
- **Verifier notes**: kept at P2. The exposed data is a first-name profile folder (equal to the public commit
  identity), a short workstation name, install paths and a process list; no credential or contact detail beyond the
  already-public commit e-mail. It is broad (about 2,100 files) and violates the repository's own rule, so it is a
  hygiene defect to stop growing, with an explicit owner decision on history. The report's counts (372 files with
  `C:\Users\` paths, 4 packet files) differ from the verified counts above because the path forms differ (escaped JSON
  slashes, forward slashes); the verified counts are used here.

### DOCS-14 — Evidence is 90% of the tracked tree; one packet is 79% of the window's added lines

- **Severity** P2 (reviewer P2, verifier P2) · **Category** repo-hygiene · **Verdict** Confirmed · **Confidence** high
  on numbers, medium on remedy · **Effort** L
- **Location**: `Measurements/` (11,380 of 12,624 tracked files; 330.3 of 367.3 MiB);
  `Measurements/WipDelivery_2026-11-02` (1,208 files, 22.1 MiB); 65 `.zip`/`.tar` files under `Measurements/` (104.9
  MiB); `Test-PrePush.ps1`
- **What**: By tracked bytes `Measurements/` is 89.9%, `ThirdParty/` 2.9% (10.6 MiB), `Tests/` 2.9%, `src/` 1.4%. By
  added lines the packet is 503,065 of 639,740 lines in the window (78.6%); PR #159 is 729 commits, 1,953 files,
  +620,591 lines and was reviewed only by a self-authored closeout. The window itself added about 14 MiB of pack
  objects, so history growth is modest, but every consumer that clones or pins DxUi checks out 330 MiB of historical
  evidence. `AGENTS.md` and `Core_Documentation.md` say raw status reports stay outside Git, and
  `Measurements/RichText/README.md:165-176` already moved 19,074 files to an out-of-tree archive, so the precedent
  exists.
- **Failure scenario**: every consumer restore that resolves a clean source checkout of the pin clones and checks out
  the whole evidence tree; a future delivery PR of this shape cannot be reviewed as a unit.
- **Fix**: Keep in Git the summary README, the manifest (hash, original path, byte count) and the few receipts a
  reader must open; publish raw trees as a GitHub release asset or an orphan `evidence/*` branch, with the manifest
  pinning the asset SHA-256. Add no new `.zip` or `.tar`. Add a soft limit (for example 5 MB added under
  `Measurements/` per PR) to `Test-PrePush.ps1` with an explicit, recorded override. Split future delivery PRs into
  contracts and docs, source and tests, evidence.
- **Regression test**: a case in the `Test-PrePush.ps1` tooling coverage: a fixture diff adding 6 MB under
  `Measurements/` fails without the override and passes with it; a diff adding a `.zip` fails.
- **Verifier notes**: all counts re-derived from `git ls-tree -r -l HEAD`; the "dirstat by file count" versus "by
  lines" correction in the report is right.

## P3 findings

### DOCS-03 — Docs and contracts call Done plans "active" and delegate ownership to them

- **Severity** P3 (reviewer P2, verifier P3: label wording) · **Category** link · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `docs/rich-text.md:346` and `:769` (link text "active plan" and "active qualification plan" targeting
  `Specs/Plans/Done/RichTextEditor_2026-10-17.md`), `:561-562`; `docs/controls.md:404`;
  `Specs/UI/UI_ControlsAndLayout.md:2546` (targeting `Specs/Plans/Done/SubtleMarkdownPresentation_2026-10-27.md`);
  `docs/samples.md:42` (targeting `Specs/Plans/Done/RichTextFileDrop_2026-10-20.md`)
- **What**: `Specs/README.md:7-8` says WIP plans sequence unfinished work and Done plans are historical. The window
  moved the plans and updated their paths but left the "active" labels and the ownership phrasing. The only active
  qualification owner is `Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md`.
- **Failure scenario**: a reader looking for the remaining gates of rich text follows "active plan" into a closed
  plan.
- **Fix**: Point "remaining gates" prose at
  `Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#shared-qualification-ownership-16-october-2026`; keep Done
  links only as history ("recorded in"). The doc-lint of DOCS-02 already covers the phrase.
- **Regression test**: same fixture as DOCS-02 plus a rule that a link whose text contains "active" must target
  `Specs/Plans/WIP/`; fails for the five locations.
- **Verifier notes**: lowered from P2: label and link wording, no behavioral claim beyond the label is wrong.

### DOCS-08 — `Measurements/README.md` omits the newest packet and nine other folders

- **Severity** P3 (reviewer P2, verifier P3) · **Category** link / process · **Verdict** Confirmed · **Confidence**
  high · **Effort** S
- **Location**: `Measurements/README.md` (278 lines; first entry is the 29 October PR134 comparison)
- **What**: The root `README.md:25` and `Core_Documentation.md` send readers to this index for reviewed receipts. Ten
  of the 41 top-level folders are not mentioned: `BranchIntegration`, `ControlDiagnostics_2026-10-28`,
  `LocalizedAdaptiveLayout`, `MenuPointerFixture_2026-10-26`, `ReliabilityAndFollowUps`, `Review-2026-10-09`,
  `ReviewFollowUps`, `Tooling`, `ViewerSpace` and `WipDelivery_2026-11-02` (the packet that every Done plan header and
  `ConsumerUiQualification` cite). 29 of 41 folders have no `README.md` of their own.
- **Failure scenario**: a reader following the documented index cannot find the delivery evidence.
- **Fix**: Add a newest-first entry for `WipDelivery_2026-11-02` (sources, what passed, what is open) and one line per
  other folder. Add a rule to `Test-DxUiMeasurements` (`Tools/Validation.psm1:162`): every immediate subfolder of
  `Measurements/` must be named in `Measurements/README.md`. Apply it in the same change as the entries.
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture with an unlisted `Measurements/New/` folder fails; listing
  it passes.
- **Verifier notes**: exactly the ten folders named were absent; lowered from P2 as a navigation gap.

### DOCS-09 — `ThirdParty/Markdown/README.md` describes 3 limited profiles and 18 unsupported families; the matrix has 8 and 13

- **Severity** P3 (reviewer P2, verifier P3) · **Category** contract · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** stale since the sequence, state, class, pie, ER, Gantt and mind-map profiles landed;
  the file was last edited by `f22f5237` (2 November) without reconciling
- **Location**: `ThirdParty/Markdown/README.md:18-27`; `ThirdParty/Markdown/mermaid-11.4.1/dxui-family-matrix.json`
- **What**: The README says the matrix accounts for 22 documents as "the limited flowchart, state and class profiles,
  the example collection, and 18 unsupported diagram families" and that "the remaining 18 family examples fall back as
  unsupported families". The matrix has 22 rows: 8 `limited-*` (class, er, flowchart, gantt, mindmap, pie, sequence,
  state), 1 `example-collection` and 13 `unsupported-family`.
- **Failure scenario**: a contributor sizing remaining Mermaid work uses the README and plans for 18 unsupported
  families.
- **Fix**: Regenerate the paragraph from the matrix (counts and names) and have `Tools/tests/Test-MarkdownData.ps1`
  assert the numbers it states.
- **Regression test**: `Tools/tests/Test-MarkdownData.ps1`: change one matrix row from `unsupported-family` to a
  `limited-*` value in a fixture copy and assert the README-count check fails; fails at `6df17007` (3/18 versus 8/13).
- **Verifier notes**: counts verified from the JSON; vendored-data README, lowered from P2.

### DOCS-10 — Spec authority page and contract headers have drifted

- **Severity** P3 (reviewer P2, verifier P3) · **Category** process · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `Specs/README.md:11-23,35`; `Specs/UI/UI_ControlsAndLayout.md:39,82`;
  `Specs/UI/UI_InputAndAccessibility.md:115-116`; `Specs/Rendering/Rendering_EmbeddedD3D11.md:28-29`;
  `Specs/Core/Core_PerformanceAndResources.md:134-135`; `Specs/Build/Build_UnifiedCI.md`
- **What**: `Specs/README.md:35` says "Run the three root validation scripts" while `AGENTS.md` and `validate.ps1`
  have five (`validate-build-matrix`, `-dependencies`, `-skills`, `-specs`, `-test-port`). The status-label list (SPEC
  READY, ACTIVE, HOLD) has no DONE although 23 Done plans carry "Status: DONE". The contract table lists 11 documents
  and omits `Build_UnifiedCI.md` and `Testing_RunEvidence.md` (`Core_Changes.md` and `Build_ConsoleAndPaths.md` appear
  only in prose). `Status:` and `Last reviewed:` sit far from the top (UI_ControlsAndLayout 39 and 82,
  UI_InputAndAccessibility 115, Rendering_EmbeddedD3D11 28, Core_PerformanceAndResources 134); `Build_UnifiedCI.md`
  has neither. Several stamps are older than the last edit (Build_ToolchainAndConsumption 2026-10-01 versus
  2026-11-02, Testing_Validation 10-25 versus 11-02, UI_ControlsAndLayout 10-25 versus 11-02, and others).
  `Specs/README.md:36` says generated measurement reports never live in Specs, but
  `Specs/Plans/Done/CodeReview_2026-10-10/*.json` and `Specs/Plans/Done/SharedLibraryReadiness/*.json` are receipts.
- **Failure scenario**: a contributor following the authority page runs three validators and misses two; a reader
  trusts a "Last reviewed" date that predates the latest edit.
- **Fix**: Change "three" to "five" and point at `validate.ps1`; add DONE to the label list; add `Build_UnifiedCI.md`,
  `Testing_RunEvidence.md`, `Core_Changes.md` and `Build_ConsoleAndPaths.md` to the contract table; move the `Status:`
  block to line 3 of each contract (UI_ControlsAndLayout currently has 38 lines of RichTextEditor prose first); give
  `Build_UnifiedCI.md` a `Status:` line; delete the `Last reviewed:` stamps (no tooling reads them; git history is the
  review record). Add a `Test-DxUiSpecs` rule requiring `Status:` within the first 6 lines of each contract in the
  required list.
- **Regression test**: `Tools/tests/Test-Validation.ps1` fixture: a contract with `Status:` at line 12 fails; at line
  3 passes; a contract table missing a file under `Specs/{Build,Core,Rendering,Testing,UI}/` fails.
- **Verifier notes**: `Testing_RunEvidence.md` has `Status:` at line 3 (its only defect is the missing table row);
  lowered from P2.

### DOCS-13 — 413 unfolded fragments, 364 dated the same day, will fold in reverse-alphabetical order

- **Severity** P3 (reviewer P2, verifier P3) · **Category** process · **Verdict** Confirmed · **Confidence** high on
  counts, medium on the "no validation wording" heuristic · **Effort** M
- **Location**: `Changes/` (413 fragments plus README); `Specs/Core/Core_Changes.md:13-17,36`; `Changes/README.md:20`;
  `CHANGELOG.md`
- **What**: 413 fragments: 2 dated 2026-10-31, 364 dated 2026-11-01, 47 dated 2026-11-02, all through PR #159.
  Structure is sound (names, dates, one bullet each). `Core_Changes.md:13-17` asks fragments to "Describe the
  behavior, relevant validation and material remaining gates"; 214 of 413 contain none of test, pass, qualif, pending,
  remain, gate, validat, probe, verif, regress, coverage or check (for example `Changes/2026-11-01-class-blocks.md` is
  one clause). 178 topics start with `rich` and 78 with `math`, micro-steps of two feature streams. The fold writes
  "descending ordinal filename order (newest date first, topic breaking ties)", so 364 same-day entries sort
  reverse-alphabetically, not chronologically. The fold roughly doubles `CHANGELOG.md` (384 entries, 1,750 lines, 238
  KB, one `## Unreleased` heading) and no release heading has ever been cut.
- **Failure scenario**: the next fold PR adds 413 entries in an order that reads as noise, to a changelog that no
  consumer can scan for what changed in a pin.
- **Fix**: Before the fold, consolidate the pending fragments into one per user-visible capability with a validation
  or gate sentence; add a size guard to `Tools/Submit-ChangelogFold.ps1` (refuse more than N entries without
  `-Force`); document dating by merge day; cut a version heading when the first consumer pin is announced. Do not add
  a keyword rule for "validation"; the heuristic is too weak to be a gate, so make the policy sentence explicit ("or:
  no behavior change") and review it by hand.
- **Regression test**: `Tools/Test-ChangelogWorkflow.ps1` fixture: a fold of more than N fragments without `-Force` is
  refused; a fold of N passes and keeps the documented order.
- **Verifier notes**: counts re-derived (413, 2/364/47, 214, 384 entries, 1,750 lines); lowered from P2 because the
  process rules are met and the heuristic is weak.

### DOCS-15 — `UI_ControlsAndLayout.md` is a 2,864-line contract with one 1,528-line section

- **Severity** P3 (reviewer P2, verifier P3) · **Category** readability / contract · **Verdict** Plausible ·
  **Confidence** high on size · **Effort** L
- **Location**: `Specs/UI/UI_ControlsAndLayout.md` (`## Retained RichTextEditor v0 preview (D2c)` at line 960 spans to
  2487, 53% of the file; line 964 is 4,415 characters; line 2603 is 2,717); also
  `Specs/UI/UI_InputAndAccessibility.md` (1,585 lines), `Specs/Testing/Testing_Validation.md` (1,202),
  `Specs/Core/Core_PerformanceAndResources.md` (1,073)
- **What**: The document titled "Controls and layout" opens with 38 lines of RichTextEditor ink requirements before
  its own `Status:` line, has 34,067 words and no table of contents. The large section contains 14 `###` subsections
  whose titles carry plan codes (M4a/b, M4c, M4d, M4e, M5a). The normative files no longer contain commit hashes or PR
  numbers, so the problem is structure, not history.
- **Failure scenario**: a contributor changing the footnote grammar needs lines 2369-2487 but must find them by
  searching milestone codes; edits to the middle of a 1,528-line section cause merge conflicts between unrelated
  rich-text features.
- **Fix**: Split into `Specs/UI/UI_ControlsAndLayout.md` (intrinsic measurement, flex, forms, popups: lines 108-809)
  and a `Specs/UI/RichText/` folder with one contract per feature (`Source.md`, `Markdown.md`, `SemanticRendering.md`,
  `Commands.md`, `Accessibility.md`, `Math.md`, `Diagrams.md`, `Images.md`, `Metadata.md`, `Footnotes.md`,
  `Presentation.md`). Each starts with a 5-line block (Status, Implemented with a capability id, Limits table, Open
  gates linking `ConsumerUiQualification`). Replace milestone codes by feature names, express the roughly 40 parser
  limits of line 964 as a table, add the new files to the `Specs/README.md` table and the `Test-DxUiSpecs` required
  list. Pitfall: `capabilities.json` and `Test-DxUiRichTextDocumentation` depend on heading anchors; rename in one
  change with a script that rewrites links.
- **Regression test**: `Test-DxUiSpecs` rule: no contract file over 1,500 lines; every file under `Specs/UI/RichText/`
  appears in the `Specs/README.md` table.
- **Verifier notes**: the report's "7 headings" counts only `##`; the big section has 14 `###` subsections, so the "no
  structure" framing is overstated. Size and readability findings stand.

### DOCS-16 — No pin guidance for the pre-adoption conditions (partly overtaken by PR #160)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** process · **Verdict** Plausible · **Confidence** high at
  HEAD · **Effort** S
- **Location**: `CHANGELOG.md:5`; `docs/getting-started.md:24`; `vcpkg.json` (`version-string: 0.1.0`);
  `docs/hosting.md`
- **What**: At `6df17007` no consumer-facing page says which commit is the recommended pin or lists the known
  pre-adoption conditions (DX7 correlated character receipt repair, API revision 4 without binary compatibility), and
  no release heading exists although `Tools/Release-Changelog.ps1` does. PR #160 (live `origin/main`, `5fc4d4c3`)
  added `docs/upgrade.md`, which covers pin selection ("final reviewed main commit including this guide"), the
  rebuild-the-closure rule and per-consumer entrypoints. It still does not mention DX7 (`grep -c DX7` is 0), names no
  tag, and does not say what the notices obligation is (DOCS-07).
- **Failure scenario**: a consumer adopts the pin without learning that the character-receipt transport is a
  pre-adoption repair condition.
- **Fix**: Add the DX7 condition and a pointer to the open gates to `docs/upgrade.md`; cut a version heading or tag
  when the pin is announced; keep one page as the single answer to "which commit do I take?".
- **Regression test**: `Tools/tests/Test-Docs.ps1`: `docs/upgrade.md` must name every id in `capabilities.json`
  `pending` that is marked as a pre-adoption condition (add such a marker field first).
- **Verifier notes**: the original proposal of a new `docs/pinning.md` is superseded by `docs/upgrade.md`.

### DOCS-17 — `capabilities.json` has a stale identifier and lacks two pending ids

- **Severity** P3 (reviewer P3, verifier P3) · **Category** claim · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S
- **Location**: `capabilities.json:116,177` (`bounded-native-mermaid-state-class-preview`);
  `capabilities.json:141-153` (`pending`); `capabilities.json:11,18,51,58` (`consumerInterface.mandatoryParameters`
  and `modules`)
- **What**: The id says "state-class" but the row's limits list flowchart, state, class, sequence, pie, ER, Gantt and
  mind map. `pending` already contains `rich-text-editor`, `rich-text-markdown-extension-conformance` and
  `direct-source-ink-renderer-performance-qualification`, but nothing for physical IME/assistive-technology
  qualification or the DX7 pre-adoption condition. `mandatoryParameters` duplicates part of `modules` with different
  parameter lists (`DisableStlAnnotations` only in the latter).
- **Failure scenario**: tooling that keys on the id mislabels the capability; a consumer reading `pending` believes
  IME/AT is covered by the generic rich-text id.
- **Fix**: Rename the id (a coordinated edit with `docs/rich-text.md` and its validator), add pending ids
  `physical-ime-assistive-technology-qualification` and `correlated-character-receipt-transport`, and add a doc line
  stating the relationship of the two parameter lists.
- **Regression test**: `Tools/tests/Test-Docs.ps1` fixture: each `pending` id must be referenced from at least one
  docs or Specs page; passes once the pages cite the new ids.
- **Verifier notes**: the report's claim that no performance pending id exists is overstated (see the existing
  `direct-source-ink-renderer-performance-qualification`).

### DOCS-18 — Design-system RichTextEditor guideline and `docs/images` captures lag the window

- **Severity** P3 (reviewer P3, verifier P3) · **Category** process · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M
- **Location**: `Specs/DesignSystem/components/RichTextEditor/README.md:73`;
  `Specs/DesignSystem/components/RichTextEditor/preview.html:24`; `docs/images/rich-text-local-images-dark.png` and
  `-light.png` (last commit `c1a77e42`, 2026-10-21); `docs/gallery/generation.json`
- **What**: The window added sequence, pie, ER, Gantt and mind-map profiles, class annotations and compartments,
  authored image presentation, file drop and footnotes. The RichTextEditor guideline still says "native in-document
  math and state/class diagrams" and only one line of `Specs/DesignSystem` changed. `AGENTS.md` and `CONTRIBUTING.md`
  require visual changes to update the design-system guideline and preview in the same change. Gallery sheets are
  regenerated correctly (`generation.json`: `sourceCommit` `9909da10`, clean, 35 controls), but `docs/images/*` has no
  generation record although `docs/rich-text.md:1311-1312` names the command.
- **Failure scenario**: a theme author sizing the new diagram objects finds no guidance.
- **Fix**: Add the diagram families, footnote and metadata states to the guideline and preview, or record that none
  are control chrome changes; write `docs/images/generation.json` (command, commit, hashes) and have `Test-DxUiDocs`
  check the two PNG hashes.
- **Regression test**: `Tools/tests/Test-Docs.ps1`: modifying a PNG byte without updating
  `docs/images/generation.json` fails.
- **Verifier notes**: not visually verified; the PNGs were not opened.

### DOCS-19 — `docs/rich-text.md` is a reference, not a usage guide

- **Severity** P3 (reviewer P3, verifier P3) · **Category** readability / simplification · **Verdict** Confirmed ·
  **Confidence** high · **Effort** L
- **Location**: `docs/rich-text.md` (1,987 lines, 15 `##` sections, a 4,746-character line at 1635); headings carry
  plan codes (D2c, D2d, M3, M4d, M4e, M4f, M5b); `capabilities.json` anchors depend on them
- **What**: The capability table (lines 17-39) is good and machine-checked. Below it the usage doc still carries
  qualification history ("57 ordinary x64 scope passes", "324 exact raster pairs") although `Core_Documentation.md`
  places history in `Measurements/RichText`; `Measurements/RichText/2026-11-01-usage-history.md` already moved part of
  it. The first runnable snippet is at line 478, the math command inventory is one 4.7k-character paragraph, and the
  six C++ examples are far apart.
- **Failure scenario**: a new user needs 480 lines to see how to create an editor.
- **Fix**: (1) a 40-line quick start with the editor snippet of line 478; (2) the capability table; (3) one short page
  per feature (Markdown, Images, Metadata, Footnotes, Math, Diagrams, Export, Accessibility) with Syntax, API, Limits
  table and Open gates; (4) milestone-free headings, with a one-time anchor update in `capabilities.json` and
  `Test-DxUiRichTextDocumentation`; (5) delete the remaining qualification paragraphs listed in DOCS-04.
- **Regression test**: `Tools/tests/Test-Docs.ps1`: no heading in `docs/*.md` matches `\((D|M)\d+[a-z]?[^)]*\)`; the
  first fenced C++ block of `docs/rich-text.md` appears before line 120.
- **Verifier notes**: structure counts verified.

### DOCS-20 — `FrameUpdateAccounting` plan has no status line and a stale open item

- **Severity** P3 (reviewer P3, verifier P3) · **Category** process · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `Specs/Plans/WIP/FrameUpdateAccounting_2026-10-26.md:1-16`; `Specs/Plans/WIP/README.md:6`;
  `Specs/README.md:18-23`
- **What**: `Specs/README.md` says current plan status lines use the labels ACTIVE, SPEC READY and HOLD. The plan
  starts at "Current evidence checkpoint" with no status line (only the WIP index row carries **ACTIVE**). Line 16 is
  the unchecked item "Fold changelog, publish source/test evidence, verify the remote merge and move this plan to Done when
  its required gates close" while `CHANGELOG.md:103` already carries the entry and PR #112 is merged. Implementation and tests exist
  (`Tests/Controls/DxUi.Tests.Rendering.cpp:2682`). `AGENTS.md` says not to leave a completed plan in WIP; this plan
  remains only for performance acceptance that `ConsumerUiQualification` already owns.
- **Failure scenario**: the WIP index reports an ACTIVE plan whose work is finished.
- **Fix**: Add a status line such as "ACTIVE; metric correction implemented and hosted-qualified, shared performance
  acceptance owned by ConsumerUiQualification" in the form the other WIP plans use, and tick or delete line 16; or move the plan to Done with a hand-off line and remove the WIP row.
- **Regression test**: `Test-DxUiSpecs` rule: every WIP plan has a `Status` line in its first 6 lines; fails for this
  plan at `6df17007`.
- **Verifier notes**: confirmed; the changelog entry exists at the cited line.

### DOCS-21 — Closeout says repairs are "covered" where the packet's own draft says the test is not located

- **Severity** P3 (reviewer P3, verifier P3) · **Category** claim · **Verdict** Plausible · **Confidence** medium ·
  **Effort** M
- **Location**: `Specs/Plans/Done/README.md:74`; `Specs/Plans/Done/CodeReview_2026-10-28.md:5`;
  `Measurements/WipDelivery_2026-11-02/raw/historical/historical-per-id.md.txt:7,91`
- **What**: Both closeouts say accepted repairs are "covered by the six-build/75-suite R27 cohort". For R28-01 (Grid
  and Tree do not re-clamp scroll offsets on growth) the per-ID inventory says "PARTIAL TRACE ... directed
  grow-viewport test not yet located, so source fix only", and 45 rows carry that status. The verifier found no
  scroll-clamp-on-grow test in `Tests/Controls/DxUi.Tests.Grid.cpp` or `DxUi.Tests.Tree.cpp` (the Tree tests at
  `:145-182` only set root bounds before a click). A passing suite proves the suite passes, not that a fix is covered.
- **Failure scenario**: a regression in `Grid::OnBoundsChanged` (`src/Controls/DxUi.Grid.cpp:6490`) passes all suites.
- **Fix**: Add the directed regression tests (R28-01 first), or reword the closeout as "integrated; direct test exists
  for N of M IDs" listing the source-only IDs; promote the final disposition table to a first-class file in the
  packet.
- **Regression test**: `Tests/Controls/DxUi.Tests.Tree.cpp` and `DxUi.Tests.Grid.cpp`: scroll to the end of a content
  taller than the viewport, enlarge the control with `SetBounds` so the content fits, assert the scroll offset is 0
  and the first row is reachable; both fail without `OnBoundsChanged`.
- **Verifier notes**: absence of a test was established by name search and reading, not exhaustively.

### DOCS-22 — ARM64 status is described four different ways

- **Severity** P3 (reviewer P3, verifier P3) · **Category** contract · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S
- **Location**: `Measurements/WipDelivery_2026-11-02/README.md:29` and
  `Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md:44` ("Native ARM64 execution is explicitly deferred by the
  user"); `Specs/Build/Build_UnifiedCI.md:29` ("Both native x64 and native ARM64 execute"); `capabilities.json`
  pending `native-arm64-runtime-qualification`; `Specs/Plans/WIP/FrameUpdateAccounting_2026-10-26.md:11`;
  `docs/getting-started.md:21`
- **What**: A consumer cannot tell whether native ARM64 runtime is unqualified (capabilities), deferred (WIP) or
  executed on every main push (CI contract). The statements are reconcilable (local delivery run versus hosted run
  versus ARM64 foreground and physical input) but no document says so.
- **Failure scenario**: a consumer plans ARM64 adoption from the wrong one of the four statements.
- **Fix**: One paragraph in `Specs/Core/Core_Documentation.md` or `docs/performance.md#arm64-evidence`: hosted native
  ARM64 functional runs exist for Release on main; local ARM64 execution and ARM64 foreground and physical input are
  deferred; therefore the capability stays pending. Link it from the five places.
- **Regression test**: none practical beyond `Test-DxUiDocs` link checks for the new anchor.
- **Verifier notes**: statements verified at the cited lines.

### DOCS-23 — Small documentation defects

- **Severity** P3 (reviewer P3, verifier P3) · **Category** readability · **Verdict** Plausible (one sub-claim
  refuted) · **Confidence** high · **Effort** S
- **Location / What / Fix** (one line each):
  - `Specs/Plans/WIP/README.md:12-17` ends with a hand-picked list of five "Completed records" and a pointer to
    another repository's plan; replace by a link to `../Done/README.md`.
  - `Specs/Plans/Done/README.md` mixes blank-line-separated and consecutive entries, is not chronological, and the
    window appended a block without a heading; add a heading and order it.
  - `Specs/Core/Core_Documentation.md:40` names the CI artifact `docs-gallery-x64-Release`, a string that exists
    nowhere in `.github/`, `.ci/` or `Tools/` (it may live in the external engine; unverifiable here); verify against
    the shared engine pin or remove it.
  - `docs/performance.md` opens (lines 1-113) with receipt-attestation internals before the first "Run and compare"
    section at line 114 and holds 14 dated records; move the attestation text to
    `Specs/Core/Core_PerformanceAndResources.md`.
  - `.github/CODEOWNERS` protects `.github`, `.ci`, `Tests`, `Tools`, `Build`, scripts and agent files, not
    `capabilities.json`, `include/`, `Specs/`, `docs/licenses` or `THIRD-PARTY-NOTICES.md`; add them (the capability
    manifest is the consumer-interface authority) and keep `Tools/tests/Test-CodeOwners.ps1` in step.
  - Size growth is recorded only inside a receipt:
    `Measurements/WipDelivery_2026-11-02/raw/receipts/main/release-sizes.json.receipt.txt` (`DxUi.lib` 189,760,522 to
    236,095,678 bytes, +24.42%; EmbeddedControls +77.54%; EmbeddedTests +48.59%) with the decision "Retain all
    requested features; optional modular linkage is a future optimization". `AGENTS.md` requires developer advice with
    measured optimization, scope-reduction or deferral options for confirmed degradation; add a short record to
    `docs/performance.md`.
  - Refuted: the report's claim that `--text-input` is mentioned nowhere in the sample README (see Refuted); it is
    missing only from `docs/samples.md` and the root `README.md`; document it there.
- **Regression test**: `Tools/tests/Test-CodeOwners.ps1`: assert `capabilities.json`, `include/` and
  `THIRD-PARTY-NOTICES.md` are owned; fails at `6df17007`.
- **Verifier notes**: every bullet except the `--text-input` sub-claim reproduced at HEAD.

## Refuted

No whole finding was refuted. Sub-claims that were wrong:

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| DOCS-23 (`--text-input` sub-claim) | `--text-input` is never mentioned in the sample README | `Samples/EmbeddedControls/README.md:8` and `:14` both mention it; it is absent from `docs/samples.md` and `README.md:25` only |
| DOCS-11 (sub-claim) | The `.json.receipt.txt` convention "already covers 1,076 earlier files" | `git ls-tree` counts 1,076 such files in all of `Measurements/`, 912 of them in the 2 November packet; 164 are earlier |
| DOCS-15 (sub-claim) | The big section has no structure beyond milestone codes; the file has 7 headings | `Specs/UI/UI_ControlsAndLayout.md` has 14 `###` subsections between lines 1259 and 2369 inside the section (milestone codes are in their titles) |
| DOCS-17 (sub-claim) | No pending id exists for performance-policy qualification | `capabilities.json:141-153` lists `direct-source-ink-renderer-performance-qualification` and `rich-text-editor` |
| DOCS-01 (premise) | "Remote refs may be stale" | `git ls-remote origin` and local `origin/main` both give `5fc4d4c3`; the three commits are absent from the live remote and from all 159 pull-request heads |

## Area architecture and simplification proposals

1. **Evidence out of Git, manifests in Git.** Proposal: keep summary README, manifest (hash, original path, bytes) and
   the few receipts a reader must open; publish raw packets as release assets or an orphan `evidence/*` branch pinned
   by SHA-256. Rationale: 90% of tracked bytes, 330 MiB checked out by every consumer restore. Files:
   `Measurements/**`, `Test-PrePush.ps1`, `Tools/Validation.psm1`. Risk: links to raw receipts move; hash-pinned
   history stays. Effort L. Before/after production: removes about 300 MiB from consumer checkouts without losing
   auditability.
2. **One validator for packets and receipts.** Extend `Test-DxUiSpecs` and `Test-DxUiMeasurements` (Done plans without
   `- [ ]`, `Status` in the first 6 lines, labels including DONE, every `Measurements/*/` indexed,
   `*.json.receipt.txt` parsed, catalog hashes re-verified, milestone-code link targets, workflow names and runner
   labels cited in docs exist). Rationale: DOCS-02, 03, 04, 05, 06, 08, 10, 11 are the same class of drift that only
   people catch today. Files: `Tools/Validation.psm1`, `Tools/tests/Test-Docs.ps1`, `Tools/tests/Test-Validation.ps1`.
   Risk: first run fails on legacy text; land each rule with its cleanup. Effort M.
3. **Notice gate.** `Test-DxUiDependencies` compares generated-data headers, `ThirdParty/**/LICENSE*`,
   `THIRD-PARTY-NOTICES.md` and the SDK license list. Rationale: DOCS-07 was introduced by a data import that no check
   ties to notices. Effort S.
4. **Fragment policy.** One fragment per user-visible capability with a validation sentence, dated by merge day, a
   fold-size guard, and a release heading cut when the first consumer pin is announced. Rationale: DOCS-13, 16. Files:
   `Changes/README.md`, `Specs/Core/Core_Changes.md`, `Tools/Submit-ChangelogFold.ps1`. Risk: small. Effort M.
5. **PR size policy.** No 700-commit delivery PRs: split contracts and docs, source and tests, evidence. Rationale:
   DOCS-14; PR #159 was not reviewable as a unit. Files: `Specs/Testing/Testing_Validation.md`, `Test-PrePush.ps1`.
   Effort S.
6. **Receipt redaction at write time.** Machine alias, no process-list snapshots, no profile paths. Rationale:
   DOCS-12. Files: `Tools/PerformancePolicy.psm1`, `performance-paired.ps1`, `Tools/PerformanceComparison.psm1`. Risk:
   comparison key change needs a schema version. Effort M.
7. **One contract per rich-text feature** with a 5-line header and a limits table; history moves to
   `Measurements/*/README.md` or Done plans. Rationale: DOCS-15, 19. Files: `Specs/UI/**`, `docs/rich-text.md`,
   `capabilities.json`. Risk: anchor churn. Effort L.
8. **Single answer to "which commit do I take?"** Keep `docs/upgrade.md` (PR #160) as that page and add the known
   conditions (DX7, API revision 4 binary-compatibility rule, notices). Rationale: DOCS-16. Effort S.

## Test-quality observations

- `Test-DxUiMeasurements` (`Tools/Validation.psm1:162-193`) cannot fail for a packet that stores its receipts as
  `*.json.receipt.txt`: it enumerates `*.json` only, so the 912-file packet is invisible to it, and no test references
  the packet catalog or manifest.
- The link check in `Test-DxUiSpecs` (`Tools/Validation.psm1:329-373`) strips the fragment before testing existence
  (`Split('#', 2)[0]`), so a link to a README that exists but lacks the cited packet (DOCS-04) or a renamed heading
  passes by construction; the reviewer's private anchor check found 0 broken anchors today, but nothing guards it.
- `Test-DxUiRichTextDocumentation` validates that the capability table projects `capabilities.json`; it does not look
  at the prose around the table, which is where the stale statements of DOCS-02 live.
- `Test-DxUiSpecs` requires every direct WIP plan to be indexed but never inspects Done plans, so 413 unchecked boxes
  and contradictory headers pass.
- The CI and workflow names cited in `README.md`, `docs/performance.md` and `Testing_Validation.md` are not checked
  against `.github/workflows/` or `.ci/project.json` (DOCS-06).
- No tooling test pins the SDK license set (`Tools/New-SdkPackage.ps1:66-72`); the license list changed once
  (`803091f5`) and can regress silently (DOCS-07).
- Closeout wording "covered by the six-build/75-suite cohort" is unfalsifiable by the suites: a passing cohort cannot
  show that a specific repair has a directed test (DOCS-21).

## Questions for the developer

1. Will the API3/API4/SDK branches (`ej/dxui-compat-redprism-menu-20261101`, `ej/dxui-compat-redxe-menu-20261101`,
   `ej/dxui-delivery-sdk-20261102`) be pushed or tagged, or should the packet be re-bound to commits on `origin`?
   Which commit do RedPrism, RedXe, RedSalamander and RedTilde pin next? Recommended: push or tag all three now, and
   name the branch and full SHA in the packet README (DOCS-01, 16).
2. Is `apiRevision: 4` still right? The window changed documented callback semantics ("must not re-enter the view"
   became "may retire or replace retained controls", `docs/hosting.md`), `MarkdownSyntax` layout and added virtuals;
   `Specs/Build/Build_ToolchainAndConsumption.md:223-229` says a behavior that now needs a call it did not need before
   bumps the revision. Recommended: decide explicitly and record it in a fragment; not verified by the verifier.
3. Is the +24% archive and +77% sample size growth, and the semantic study's in-flight edit p95 rise (35,545 to 50,002
   us), accepted? Where should the developer-advice record required by `AGENTS.md` live? Recommended: a dated section
   in `docs/performance.md` linking the receipt (DOCS-23).
4. Should the Done plans be mechanically rewritten (checkbox to disposition), or is leaving historical bodies intact a
   deliberate rule? Recommended: rewrite mechanically and add the validator rule (DOCS-05).
5. Is it acceptable that the workstation account name, hostname and a full process list are public? Recommended: no;
   stop the growth with write-time aliasing and a validator rule, withhold the process snapshot, and do not rewrite
   hash-pinned history (DOCS-12).
6. Does counsel need to review the Unicode License V3 and WHATWG omission before the first SDK ZIP leaves the
   building? Recommended: add the notices regardless, which is two file copies and a paragraph; counsel review is
   optional (DOCS-07).
7. Is Debug and ASan native CI intentionally manual (`Build_UnifiedCI.md`) while `AGENTS.md` and `README.md` describe
   the six-profile matrix as required? Recommended: yes, and say so in each place: automatic = Release, six profiles =
   local and manual qualification (DOCS-06, 22).
8. Should R28-01 and the other 44 "PARTIAL TRACE" ids get directed tests before the "covered" wording stays?
   Recommended: R28-01 and the other ids that the packet marks source-only get tests, the rest are reworded (DOCS-21).
9. Can the 364 same-day fragments be consolidated before the next fold? Recommended: yes, one per capability, before
   running `Tools/Submit-ChangelogFold.ps1` (DOCS-13).
10. Is `docs/upgrade.md` (PR #160) intended to replace the proposed `docs/pinning.md`? Recommended: yes; extend it
    instead (DOCS-16).

## Coverage

| Area | Depth |
| --- | --- |
| `AGENTS.md`, `README.md`, `CONTRIBUTING.md`, `docs/README.md` | Read fully; claims spot-checked against code and `capabilities.json` |
| `Specs/README.md`, `Specs/Core/Core_Documentation.md`, `Core_Changes.md`, `Changes/README.md`, `.agents/skills/spec-workflow` | Read fully |
| `Specs/Testing/Testing_Validation.md` | Header, CI sections (lines 300-330, 450-460), headings; not read end to end (1,202 lines) |
| `Specs/UI/UI_ControlsAndLayout.md`, `UI_InputAndAccessibility.md` | Structure, headers, status lines, grep for stale status wording; not read end to end (2,864 / 1,585 lines) |
| `Specs/Build/Build_UnifiedCI.md`, `Build_ToolchainAndConsumption.md` (compat rule), `Core_PerformanceAndResources.md`, `Rendering_*` | Targeted reads and greps |
| `Specs/Plans/WIP/*` (3 plans and index) | Headers, status lines, checklists, accuracy against code |
| `Specs/Plans/Done/*` (26 plan files added or moved in the window and the Done index) | Index completeness, headers, unchecked-box census, 8 plans sampled in depth; bodies not read |
| `capabilities.json` | Read fully; 20+ claims and limits verified in code |
| `docs/rich-text.md` | Capability table, export, files, metadata, math (partial), examples; 1,987 lines not read end to end |
| `docs/controls.md`, `hosting.md`, `layout.md`, `getting-started.md`, `samples.md`, `performance.md` | Examples extracted and checked; targeted prose reads; performance.md CI passages verified against workflows |
| `docs/gallery/*`, `docs/images/*`, `Specs/DesignSystem/components/RichTextEditor` | `generation.json` and provenance checked; PNGs not opened |
| `Changes/` and `CHANGELOG.md` | All 413 fragments checked by script; CHANGELOG window diff and first-parent history reviewed |
| `Measurements/README.md`, `Measurements/RichText/*`, `Measurements/WipDelivery_2026-11-02/` | Index coverage, README fully, catalog hashes verified by the reviewer, privacy sweeps, structure; raw receipts sampled only |
| `THIRD-PARTY-NOTICES.md`, `ThirdParty/*/README.md`, `Tools/New-SdkPackage.ps1`, `docs/licenses` | Read; compared with generated headers; verifier re-checked headers, license texts and packager |
| `Tools/Validation.psm1` (`Test-DxUiMeasurements`, `Test-DxUiSpecs`) | Relevant functions read |
| `.github/workflows/*`, `.ci/project.json`, `CODEOWNERS` | Targeted greps |
| Git and repository metrics (dirstat, numstat, count-objects, blob sizes, reachability of cited SHAs, `ls-remote`) | Computed by reviewer; verifier re-ran reachability, `ls-remote` (heads and pull refs), `ls-tree` sizes |
| Not covered | Compiling the docs examples; visual review of PNGs; code-level correctness (other reviewers); the 600k lines of raw receipts beyond samples; external shared CI engine behavior; `origin/main` after `6df17007` except `docs/upgrade.md` |
