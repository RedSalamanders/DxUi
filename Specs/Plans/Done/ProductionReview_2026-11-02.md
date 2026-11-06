# Production review of the five-day delivery, 2 November 2026

Status: SUPERSEDED — sequencing consolidated; unfinished product work remains open under the new owner.
Archived: 4 November 2026
Current work: [the single remaining-work plan](../WIP/RemainingWork_2026-11-04.md).
Original: `Specs/Plans/WIP/ProductionReview_2026-11-02.md` at `7257f3e31e575c633e8cbbbf3a74f3115f2eeb73`; SHA-256 `F02671B387F40820FB9BE52914D60498C002D985487B54203482E3BF5F1B1253`.
[Original committed record](https://github.com/RedSalamanders/DxUi/blob/7257f3e31e575c633e8cbbbf3a74f3115f2eeb73/Specs/Plans/WIP/ProductionReview_2026-11-02.md).

The dated record below preserves decisions, source-specific evidence, failures and rejected hypotheses.
Transferred items are not completed features. Old pins, billing blocks, draft/merge claims and historical
queues describe their recorded checkpoints; the new plan owns current status. This archive does not
waive performance, consumer, native-platform, physical-input or release gates.

## Historical record

Historical status: SPEC READY — review recorded; the developer took Wave 0 decisions D0, D1, D2, D9 and D10 on 2 November and the
D10 publication is done. Code implementation has not started and no library, test or tooling behavior has changed for it.

- **Date**: 2026-11-02.
- **Window**: 28 October to 2 November 2026, from `a0f67fd3` (last commit before 28 October) to `6df17007` (merge of
  PR #159, "complete DxUi WIP delivery"): 779 commits and 27 first-parent merges. `src/` and `include/` changed by
  +30,508/−3,150 lines over 99 files, 27 of them new; `Tests/` by +26,969/−1,430 over 115 files. PR #159 alone carries
  almost all of the source change and merged without an independent post-merge review.
- **Current `main`**: `origin/main` is `5fc4d4c3` (PR #160). PR #160 changes documentation only (`docs/upgrade.md` and
  eight other text files); `src/`, `include/`, `Tests/`, `Tools/` and `.github/` are byte-identical to `6df17007`, so
  every source finding below holds on current `main`.
- **Owning contracts**: [controls](../../UI/UI_ControlsAndLayout.md),
  [input and accessibility](../../UI/UI_InputAndAccessibility.md), [theme](../../UI/UI_ThemeAndTypography.md),
  [Win32 host](../../Rendering/Rendering_Win32Host.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
  [performance](../../Core/Core_PerformanceAndResources.md), [architecture](../../Core/Core_Architecture.md),
  [validation](../../Testing/Testing_Validation.md), [toolchain](../../Build/Build_ToolchainAndConsumption.md),
  [unified CI](../../Build/Build_UnifiedCI.md) and [documentation](../../Core/Core_Documentation.md).
- **Related reviews** (historical, re-verified here rather than trusted):
  [10 October](../Done/CodeReview_2026-10-10.md), [28 October](../Done/CodeReview_2026-10-28.md),
  [31 October](../Done/CodeReview_2026-10-31.md), [delivery review](../Done/DeliveryReview_2026-10-31.md),
  [RichText audit](../Done/RichTextAudit_2026-10-21.md) and [5 October](../Done/ProductionReview_2026-10-05.md).
- **Shared qualification** (performance policy, native ARM64 execution, physical IME/AT/touch, consumer adoption)
  stays owned by [Library qualification and consumer adoption](ConsumerUiQualification_2026-10-11.md). This plan owns
  the code, test, documentation and process corrections below and does not become a second qualification ledger.

## Findings appendix

Each file is self-contained: a summary table, then every surviving finding with location at `6df17007`, failure
scenario, corrected fix, regression test and the red-team verifier's notes, followed by refuted claims, area proposals,
test-quality observations, developer questions and a coverage table.

| Area | File | P0 | P1 | P2 | P3 |
| --- | --- | --- | --- | --- | --- |
| Rich text editor core | [editor.md](ProductionReview_2026-11-02/editor.md) | 0 | 2 | 7 | 10 |
| Commands, clipboard, export, file drop | [commands-clipboard.md](ProductionReview_2026-11-02/commands-clipboard.md) | 0 | 0 | 2 | 7 |
| Markdown, front matter, projection | [markdown.md](ProductionReview_2026-11-02/markdown.md) | 0 | 3 | 4 | 10 |
| Native math | [math.md](ProductionReview_2026-11-02/math.md) | 0 | 0 | 1 | 15 |
| Native Mermaid diagrams | [diagrams.md](ProductionReview_2026-11-02/diagrams.md) | 0 | 2 | 1 | 12 |
| Semantic document and geometry | [semantic-geometry.md](ProductionReview_2026-11-02/semantic-geometry.md) | 0 | 1 | 4 | 5 |
| Layout, rendering and images | [rendering-images.md](ProductionReview_2026-11-02/rendering-images.md) | 0 | 1¹ | 1 | 11 |
| UI Automation | [accessibility.md](ProductionReview_2026-11-02/accessibility.md) | 0 | 0 | 2 | 14 |
| Window host, embedded host, public API | [hosts-api.md](ProductionReview_2026-11-02/hosts-api.md) | 0 | 0 | 0 | 16 |
| Controls and native text input | [controls-input.md](ProductionReview_2026-11-02/controls-input.md) | 0 | 1 | 0 | 13 |
| Tooling and CI | [tooling-ci.md](ProductionReview_2026-11-02/tooling-ci.md) | 0 | 0 | 0 | 17 |
| Documentation, specifications, process | [docs-process.md](ProductionReview_2026-11-02/docs-process.md) | 0 | 1 | 8 | 14 |
| Architecture and error policy | [architecture.md](ProductionReview_2026-11-02/architecture.md) | 0 | 0 | 0 | 3² |
| Closure of earlier review findings | [prior-closure.md](ProductionReview_2026-11-02/prior-closure.md) | 1 | 1 | 7 | 11 |

¹ RENDER-01 is the same defect as EDIT-01 and is counted once in the verdict. ² Plus 17 unscored structural proposals.

## Verdict

**Not ready for a production pin of `5fc4d4c3` yet; the remaining work is bounded.** One P0 survived verification:
PRIOR-03, a use-after-free in `TextField` that commit `d04ab124` (1 November) made reachable when it widened the embedded
`requestPreparation` contract to allow synchronous control retirement without guarding every caller. Apart from it, no
crash, use-after-free, out-of-bounds read or hang was proven in the window's code under hostile input. The untrusted-data
boundaries held up well: clipboard and OLE drop ownership, CF_HTML offsets, export escaping and the link-scheme
allow-list, DROPFILES/DIB validation, WIC dimension caps before allocation, EXIF orientation, OpenType MATH table
bounds, TeX nesting/node/dimension caps, NaN/Inf geometry guards, CI workflow security (no `pull_request_target`, no
event interpolation into `run:` blocks, SHA-pinned actions, narrow permissions) and the fail-closed zero-warning gate.

What blocks a production pin is a set of user-visible P1 defects, most of them small, concentrated in the rich editor
and in rich presentation: frames withheld while purely cosmetic work restarts on every keystroke, one keystroke editing
outside a source region, supplementary characters dropped from injected input, script paragraphs that never render a
colour glyph, Markdown text that silently disappears, an incremental parse that disagrees with a fresh parse, a Debug
abort on CR-only YAML, two diagram families that fall back to source for ordinary input, and a third-party licence
notice gap in the SDK. The claimed-fix audit of the four earlier reviews found 35 of 60 traced items fixed, 9 partial
(one hiding the P0), 14 not fixed, and the RedPrism/RedXe menu stop-ship port only on unpushed local branches
(PRIOR-02).

The process findings matter as much as the code: earlier review plans were closed as DONE by narrative while most of
their boxes stayed unchecked, several P1 fixes landed without a regression test that would fail if the fix were
reverted, qualification evidence points at commits that exist on no remote, and the repository now carries about
330 MiB of evidence including workstation identity. Treat "DONE" in those plans as "implemented, not proven".

## How the review was made

1. **Scope and partition.** Every one of the 99 changed `src/`/`include/` files was assigned to exactly one of ten
   code areas; tooling/CI, documentation/process, architecture and a "claimed-fix" audit of the four earlier reviews
   ran as four further areas. Twelve reviewers ran on the strongest available model (all code areas, the claimed-fix
   audit and architecture); tooling/CI and documentation ran on a cheaper model, which suits their more mechanical scope.
2. **Method.** Read-only static review at `6df17007`: every changed hunk, then the full functions it touches and their
   callers. Reviewers took a red-team posture (hostile documents, clipboard, drops, images, fonts, UIA clients and IME;
   re-entrancy; `bad_alloc` everywhere; device loss; DPI/RTL; surrogate pairs; quadratic inputs). Nothing was built or
   executed; performance figures are estimates from code.
3. **Red team.** Every finding of every severity was then challenged by an independent verifier instructed to refute
   it: trace the real input path, look for guards elsewhere, check the proposed fix and whether the proposed test would
   fail today. The verifier's verdict and severity are the ones recorded; proposed fixes and tests in the appendix are
   the corrected versions. Verifiers refuted or narrowed many claims (for example TOOL-01, A11Y-04, HOST-17, EDIT-18,
   CMD-04's fix) and raised others (MD-04, DOCS-07).
4. **Lead checks and overrides.** The lead independently traced SEM-01, CTRL-01, MD-04, PRIOR-03 and the FLOW-01
   catch site in the code, and overrode verifier severities in four places, each with a written reason in the appendix:
   PRIOR-03 to P0 (a use-after-free the public contract permits), SEM-01 back to P1 (the documented fallback fires on
   every device, not only on unsupported ones), EDIT-11 to P1 (one keystroke silently corrupts structure outside the
   edited region) and FLOW-01/FLOW-02 kept at P1.
5. **Limits.** No execution, so timing claims are estimates and Release-only behaviour (MD-04) is partly unproven.
   Consumer lock files live outside this repository and were not re-read. Large generated tables were reviewed through
   their generators. Each appendix ends with a per-file coverage table naming anything read only partially.

## Stop-ship (P0)

| ID | Defect | Primary location | Effort |
| --- | --- | --- | --- |
| PRIOR-03 | `EmbeddedCallbacks::requestPreparation` is "invoked synchronously ... It may retire or replace retained controls" (`include/DxUi/Embedded.h:29`, widened by `d04ab124`). `TextField::ImportTextInputState`, `RestoreEditHistoryState` (Ctrl+Z/Ctrl+Y), `NotifyChanged` and the clear button call `InvalidateMeasure()`, which reaches `EmbeddedHost::RequestPreparation` synchronously (`src/Rendering/Embedded.cpp:263-281`), and then keep writing members of a control the callback may have freed. Reachable in embedded hosts through `ApplyTextInput` Commit and `DispatchKey`. | `src/Controls/DxUi.TextInput.cpp:3105`, `:3162`, `:3179`, `:3662` | S (guards) to M (deferral) |

## Release blockers (P1)

Fix these before the next consumer pin. Each ID's full entry is in the appendix file for its area; effort is S (hours),
M (a day or two), L (more).

| ID | Defect | Primary location | Effort |
| --- | --- | --- | --- |
| EDIT-01 (= RENDER-01, RENDER-14) | Opt-in source line numbers, whitespace marks and code ink are restaged from zero on every edit and reported as pending content, so both hosts withhold the whole editor frame; a 64 Ki-unit note needs ~400 turns per keystroke and can hold up to 65,537 layouts. EDIT-02 (drag cancelled) and EDIT-04 (tokenizer `bad_alloc` retried forever) share the root. | `src/Controls/DxUi.RichTextEditor.cpp:3672`, `src/Controls/DxUi.RichSourceGuides.h:58` | M |
| EDIT-11 | Backspace/Delete at a source-edit region edge edits outside the region and can break a closing code fence so the block swallows the rest of the document. | `src/Controls/DxUi.RichTextEditor.cpp:5113` | S |
| CTRL-01 | `TextField` and `RichTextEditor` clear the pending lead surrogate on every key-down, so supplementary characters from `VK_PACKET` input (RDP, on-screen keyboards, auto-type, emoji fallback) are dropped; both window and embedded hosts. | `src/Controls/DxUi.TextInput.cpp:2222`, `src/Controls/DxUi.RichTextEditor.cpp:5659` | M |
| SEM-01 (+ RENDER-05) | The raster target is released before `LayoutOne` checks colour-glyph support, so every superscript/subscript paragraph that needs a colour glyph (`x^2^` next to an emoji) shows raw source on every device. | `src/Controls/DxUi.RichSemanticGeometry.cpp:2456`, `:1994` | S |
| MD-01 | GFM autolink deferred suffix is never decoded when the next link follows a delimiter run: `Visit www.a.com today, **not** www.b.com.` renders without " today, ". Source is intact; view, UIA and export lose the text. | `src/Controls/DxUi.MarkdownInlines.h:1326` | S |
| MD-02 | Incremental closed-root reuse ignores the document-start rule of front matter: pressing Enter at offset 0 above a `---` block produces a different tree from a fresh parse, and the wrong tree persists. | `src/Controls/DxUi.MarkdownSyntax.cpp:682` | S |
| MD-04 | CR-only line endings in YAML front matter trip a rapidyaml assertion that reports through the global callbacks DxUi never installs: Debug and ASan Debug abort; Release outcome unproven. | `src/Controls/DxUi.YamlMetadata.cpp:79` | S-M |
| FLOW-01 | A trailing space inside a styled diagram label publishes a span past the trimmed label; preparation throws `invalid_argument` and the editor shows the whole document as source. Same channel as MATH-02 and ARCH-03. | `src/Controls/DxUi.NativeFlowchart.cpp:555` | M |
| FLOW-02 | Every ER (and class) diagram where an entity has two relationships on the same side falls back to source, including Mermaid's own reference ER example. | `src/Controls/DxUi.NativeFlowchartLayout.cpp:1097` | M |
| DOCS-07 | `DxUi.lib` embeds Unicode 17 data and the WHATWG entity table, but `THIRD-PARTY-NOTICES.md` and the SDK ZIP carry neither notice. | `Tools/New-SdkPackage.ps1:66` | S |
| PRIOR-02 | The DX-2/R28-45 menu stop-ship port for RedPrism and RedXe exists only on unpushed local branches (`a5b3ac81`, `a8cb8e1d`); `origin/ej/wall-compat-*` still point at the old locks, so those consumers cannot take the fix. Evidence facet: DOCS-01. | `Measurements/WipDelivery_2026-11-02/README.md:9` | S |

## Before-production P2 set

Fix in the same delivery unless the developer defers an item explicitly in Wave 0.

| Area | IDs |
| --- | --- |
| Editor | EDIT-02, EDIT-03 (IME underline staleness), EDIT-04, EDIT-06 (disclosure/zoom discards geometry and scrolls), EDIT-07 (Tab in nested code blocks edits container prefix), EDIT-09 (auto-pair inside words), EDIT-10 (read-only reports editing chords handled) |
| Commands | CMD-01 (task commands edit wrong bytes), CMD-02 (MoveLine moves parent marker) |
| Markdown | MD-03 (`$$`/`\[` do not interrupt lazy/table), MD-05, MD-06, MD-07 (quadratic scans reachable within the 64 Ki cap) |
| Math | MATH-01 (aligned/gathered/equation cells lose display style) |
| Diagrams | FLOW-03 (notes placed after routing; edges run under opaque notes) |
| Semantic geometry | SEM-02 (each image completion/animation frame re-prepares all math and re-uploads all bitmaps), SEM-03 (image placeholder decides paragraph direction), SEM-04 (definition lists/callouts lose list markers), SEM-05 (supplementary-plane word starts) |
| Rendering | RENDER-02 (`lineHeightMultiplier > 0` disables chunk reuse; same as the R28-03 "fix wrong" item) |
| Accessibility | A11Y-01 (new RadioButtons group has no Name — ships with the new control), A11Y-02 (semantic Expand/Collapse/Toggle raise no state event) |
| Documentation and process | DOCS-01 (evidence commits on no remote), DOCS-02, DOCS-04, DOCS-05, DOCS-06, DOCS-11, DOCS-12 (workstation identity in public receipts), DOCS-14 (evidence is 90% of the tree) |
| Earlier-review closure | PRIOR-07 (one failing `PrepareContent` aborts every native frame of its window and the failure is never read), PRIOR-08 (seven earlier P1 fixes without a test that fails on revert), PRIOR-21 (masked TextField keeps the IME active), PRIOR-25 (all-collapsed grouped Grid ignores every key), PRIOR-26 (control disabled mid-drag in a ScrollPanel keeps dragging), PRIOR-27 (`NumericStepper::SetValue` loses the typed edit's terminal event), PRIOR-28 (about 20 untracked earlier items still open in code) |

P3 items that are cheap and remove a real risk also belong before production: ARCH-18 (unsynchronised static WIC
factory raced by hosts on different UI threads), ARCH-02 (SVG maps device-loss HRESULTs to "unsupported image"),
ARCH-03 logging part (log `what()` at Error level), ARCH-04/ARCH-05 (never-compiled `#else` arms, dead code), HOST-02
(guard `ApplyClipboardCut` against empty selections), CMD-07 (bidi controls from file names spoof link labels),
RENDER-03/RENDER-04 (UNC classification and `SECURITY_SQOS_PRESENT`), TOOL-21 (`Test-AsanRuntime.ps1` runs in no CI
path) and HOST-04 (complete the consumer addendum that PR #160's `docs/upgrade.md` started).

## Cross-cutting themes

1. **Cosmetic preparation is treated as content.** Code ink, line numbers and whitespace marks are staged as pending
   content work, so the hosts withhold the frame (`WindowHost.cpp:4642`, `Embedded.cpp:552-563`), cancel the editor's
   own drags and refuse input until they finish, and every edit restarts them. One rule fixes EDIT-01, RENDER-01/14,
   EDIT-02 and most of EDIT-04: cosmetic stages belong to visual preparation, never gate a frame, and do work
   proportional to the viewport or the change. The editor contract currently says the opposite and must change with
   the fix (`editor.md`, EDIT-01).
2. **One element can take down the whole view.** Any `std::logic_error` escaping rich preparation is caught once in
   `RichTextEditor::PrepareContent` (`DxUi.RichTextEditor.cpp:4102`) and turns the entire document into exact source
   with only a Warning. FLOW-01 (parser publishes an inadmissible model) and MATH-02 (font size above 4096 DIP) are
   concrete instances. Fix the root causes, convert per-element `invalid_argument` into per-element rejects at the three
   preparation sites in `RichSemanticGeometry.cpp`, keep view-level fallback for per-view option errors, and log the
   cause at Error level (ARCH-03).
3. **Supplementary-plane text is still second-class.** CTRL-01 (surrogate pairs from `VK_PACKET`), SEM-05 (word
   starts outside the BMP), MD-16 (leading BOM) and 17 hand-written surrogate sites, 4 UTF-8 converters and 5 word
   definitions (ARCH-08). A shared Unicode service is an after-production refactor; the P1/P2 fixes are not.
4. **Correctness bought by switching off reuse.** RENDER-02 (whole-document layout per edit with a line-height
   multiplier), SEM-02 (image snapshot identity defeats math/bitmap reuse) and EDIT-06 (disclosure toggles discard
   geometry). Each needs a measured baseline before and after, per `AGENTS.md`.
5. **Tests that cannot fail.** Recurrent shapes: fixtures that synthesise the very timer or recovery frame they claim
   the library schedules (menu ownership, R28-41), oracles that re-implement the code (flowchart crossing count,
   CF_HTML round-trip through the library's own decoder), loops cut short before reaching the target stage (`64ff0e7d`),
   lock-step "concurrent" tests (`87280611`), finite-only output checks (1,135 KaTeX rows) and silent skips. Every P1/P2
   fix in this plan must land with a test that fails when the fix is reverted.
6. **Closure by narrative.** Earlier plans moved to Done with hundreds of unchecked boxes and "PARTIAL TRACE"
   receipts; qualification receipts bind to commits on no remote. This plan closes only with ticked boxes, each naming
   the commit and the test that proves it.
7. **Repository hygiene, privacy and licensing.** Workstation account and host names and a whole-machine process list
   in public receipts (DOCS-12), 330 MiB of evidence in the tracked tree (DOCS-14), 413 unfolded change fragments
   (DOCS-13) and the missing Unicode/WHATWG notices (DOCS-07).

## Closure of earlier reviews

The claimed-fix audit re-traced 60 stop-ship, P1, DX and critical/major RichText items from the 31 October, delivery,
28 October, 10 October and RichText reviews against `6df17007`, mentally reverting each fix to judge its test.

| Verdict | Items |
| --- | --- |
| Fixed (some with caveats such as "fixed for the read, new defect") | 35 |
| Partial (one hides PRIOR-03) | 9 |
| Not fixed (several deferred or awaiting owner decisions) | 14 |
| Contained at a boundary / cannot verify | 1 / 1 |

All earlier P0s are fixed. The partial and open items, the weak tests and the per-item evidence are in
[prior-closure.md](ProductionReview_2026-11-02/prior-closure.md); seven of its findings duplicate items carried by other
appendix files and are cross-referenced rather than repeated. Systemic causes: closure by narrative rather than ticked
boxes, a single large fix commit (`b5869cbc`) with few directed tests, tests that trigger their own recovery,
correctness bought by disabling an optimisation, contracts rewritten to match the code, and menu regressions that run
only in the interactive suite.

## Implementation sequence

Work on a focused feature branch from current `main`. Each item keeps its appendix ID in the commit message and its
change fragment. Tick a box only with the fixing commit and the regression test named next to it.

### Wave 0 — decisions that unblock implementation

Recommended answers are the lead's; the appendix files carry the area-level questions behind them. Decided items record
the developer's answer of 2 November 2026; open items keep the lead's recommendation.

- [x] **D0. Embedded `requestPreparation` re-entry contract** (PRIOR-03; the 31 October review's Q1 recommended the
  opposite of what `d04ab124` shipped). **Decided 2 November: the recommendation.** Keep the widened contract
  (`include/DxUi/Embedded.h:29`: the callback may retire or replace retained controls), and make it safe: the embedded
  host defers the wake while any input entry point (`ApplyTextInput`, `DispatchKey`, `DispatchCharacter`,
  `DispatchPointer`, clipboard and drop entry points) is on the stack, delivering it once the outermost entry point has
  revalidated its target and before it returns; add lifetime guards at every listed `TextField` site for setters the
  application calls outside a dispatch. The retiring fixtures stay; the header and `docs/hosting.md` state the deferral.
- [x] **D1. Cosmetic stages never gate a frame** (EDIT Q1). **Decided 2 November: the recommendation.** Code ink,
  source line numbers and whitespace marks become visual preparation, never count as pending content, never cancel a
  gesture or refuse input, and do work proportional to the viewport or the change. The frame shows the previous cosmetic
  state (lines whose position is unknown are omitted) until the new one is ready. The editor contract
  (`Specs/UI/UI_ControlsAndLayout.md`, the paragraphs named under EDIT-01) changes with the fix.
- [x] **D2. Per-element failure policy** (FLOW Q2, ARCH Q4, MATH Q3). **Decided 2 November: the recommendation.** A
  deterministic failure inside one element (diagram, formula, image geometry) is a per-element source fallback with a
  diagnostic; only per-view option errors fall back for the whole view; the cause is logged with `what()` at Error level;
  no Debug fail-fast while tests inject plain `logic_error` on purpose. Parsers must never publish a model their preparer
  rejects (FLOW-01).
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D3. Source-region containment applies to every edit, not only Tab** (EDIT Q3). Recommended: yes.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D4. Clipboard bitmap versus text priority** (CMD Q1, CTRL Q1). Recommended: prefer text when the clipboard
  also offers Unicode text unless the application opts into image-first, and declare a decline path for bitmap paste.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D5. Scheme-relative `//host/...` and `file://host/...` image destinations** (CMD Q4, RENDER Q5). Recommended:
  classify as network, gated by the remote-image policy, never as `LocalFile`.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D6. Windows 10 colour glyphs beside scripts** (RENDER Q3). Recommended: keep the documented paragraph fallback
  where `ID2D1DeviceContext7` is absent, but compute support once per preparation (SEM-01 fix) and document it.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D7. KaTeX display-style parity for aligned/gathered/equation cells** (MATH Q1). Recommended: yes.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D8. Public `Debug*` declarations** (ARCH Q3). Recommended: document them as unsupported and outside the
  revision-4 contract now; consolidate after production.
- [x] **D9. Evidence storage and workstation identity** (DOCS Q5, DOCS-12, DOCS-14). **Decided 2 November: redact and
  rewrite history.** The lead had recommended redacting forward only; the developer chose to also remove the existing
  data from Git history. Forward redaction (host/account aliases with a receipt schema bump, no raw process snapshots,
  raw receipts as release artifacts with reviewed summaries in Git) still applies. The rewrite is executed as its own
  coordinated operation; see [History rewrite](#history-rewrite-d9).
- [x] **D10. Unpublished qualification commits** (DOCS Q1, DOCS-01, PRIOR-02). **Decided and done 2 November: pushed.**
  The three local-only branches are now on `origin` at exactly the qualification commits:
  `ej/dxui-compat-redprism-menu-20261101` at `a5b3ac81` (API 3, 12 commits ahead of
  `origin/ej/wall-compat-redprism-20261028`), `ej/dxui-compat-redxe-menu-20261101` at `a8cb8e1d` (API 4, 7 commits
  ahead of `origin/ej/wall-compat-redxe-20261028`) and `ej/dxui-delivery-sdk-20261102` at `68f6b15e` (production SDK).
  The pushed commits add no workstation identity; no workflow runs on pushes to these branches. The `wall-compat`
  branches were left unchanged: whether RedPrism and RedXe pin the new branches or those branches are fast-forwarded is
  the consumers' change, tracked by PRIOR-02. If D9's rewrite runs later, these SHAs change too.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D11. Done plans with unchecked boxes** (DOCS Q4). Recommended: mechanically convert each unchecked box in Done
  plans to an explicit disposition (done with commit, moved to a named owner, or dropped) rather than leaving them.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D12. Change-fragment consolidation** (DOCS Q9). Recommended: fold the 413 fragments now, consolidating the 364
  same-day fragments into one per capability in a fold pull request of their own.
- **Transferred to [D-DECISIONS](../WIP/RemainingWork_2026-11-04.md#d-decisions).** **D13. Consumer-callback `bad_alloc` policy** (EDIT Q5, ARCH Q1). Recommended: keep the accepted fatal-observer
  policy, but publish one per-control table and make `TryControlCallback` the single implementation.

### Wave 1 — stop-ship and release blockers

- **Transferred to [D-L1](../WIP/RemainingWork_2026-11-04.md#d-l1).** PRIOR-03 first: implement the D0 decision (defer the embedded wake while an input entry point is on the stack),
  add lifetime guards at every listed `TextField` site, and add the ASan regression tests from the appendix (Commit,
  Ctrl+Z and Ctrl+Y with a retiring callback, plus a counter that no callback is delivered while dispatch depth is
  non-zero). Audit every other control whose setters call `InvalidateMeasure()` or `Invalidate()` before touching
  members, since `d04ab124` guarded only some.
- **Transferred to [D-E1](../WIP/RemainingWork_2026-11-04.md#d-e1).** EDIT-01 with RENDER-01/14, EDIT-02 and EDIT-04: move cosmetic stages to visual preparation, scope guides to the
  viewport or the changed lines, keep the previous gutter until the new one is ready, skip a tokenizer block on
  `bad_alloc`; update the editor contract. Measure a retained Source-mode baseline before the change.
- **Transferred to [D-E1](../WIP/RemainingWork_2026-11-04.md#d-e1).** EDIT-11: clamp Backspace/Delete to the active source-region interval for every edit kind; tests for both fence
  traces in the appendix.
- **Transferred to [D-I1](../WIP/RemainingWork_2026-11-04.md#d-i1).** CTRL-01: keep the pending lead across `VK_PACKET` key-down/up and bare modifiers in both editors and both
  hosts; replay the real message sequence in tests; correct `TestTextFieldBuffersSurrogatesBeforePublishing`.
- **Transferred to [D-R1](../WIP/RemainingWork_2026-11-04.md#d-r1).** SEM-01: compute the colour-glyph capability once in the `Preparation` constructor; stop relying on the retained
  target; test `x^2^` beside an emoji with a real target.
- **Transferred to [D-M1](../WIP/RemainingWork_2026-11-04.md#d-m1).** MD-01, MD-02, MD-04: decode the deferred autolink suffix in every sibling path; forbid closed-root reuse across
  offset 0 when front matter is possible and extend the incremental-versus-fresh differential to front matter; normalise
  CR/CRLF before YAML staging and install private global rapidyaml callbacks; run the YAML corpus in LF, CR and CRLF in
  Debug and ASan Debug.
- **Transferred to [D-A1](../WIP/RemainingWork_2026-11-04.md#d-a1).** FLOW-01 and the shared channel: trim before recording style spans; add a parser-side admission check so the
  parser never publishes a model preparation rejects; per-element reject helper at the three geometry sites (also
  covers MATH-02); Error-level logging (ARCH-03 part 1); property test "every published model is accepted".
- [x] FLOW-02: distinct cardinality anchors per edge end; corpus test with entities that have two relationships. _Closed at consolidation: Published FLOW-02 fix: implementation 703a96c7, PR163._
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** DOCS-07: add the Unicode License V3 and WHATWG notices to `THIRD-PARTY-NOTICES.md`, fix the dead
  `docs/licenses/` links in the shipped copy, and add a packaging test that pins the SDK licence set.
- [x] PRIOR-02 publication: done under D10 (branches and SHAs recorded there).
- **Transferred to [D-C1](../WIP/RemainingWork_2026-11-04.md#d-c1).** PRIOR-02 adoption: RedPrism and RedXe pin the published compatibility branches (or the owner fast-forwards the
  `wall-compat` branches to them); record the remote commit each consumer pins.

### Wave 2 — before-production P2 and cheap risk removal

- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** Every row of [the P2 table](#before-production-p2-set), each with its appendix fix and regression test.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** The cheap P3 items listed under that table (ARCH-18, ARCH-02, ARCH-04/05, HOST-02, CMD-07, RENDER-03/04,
  TOOL-21, HOST-04).
- **Transferred to [D-P1](../WIP/RemainingWork_2026-11-04.md#d-p1).** Performance-affecting fixes (EDIT-01, RENDER-02, SEM-02, EDIT-06, MD-05/06/07) each carry a retained baseline
  and a same-fixture candidate measurement; a confirmed degradation goes back to the developer with options, never a
  silent rebaseline.

### Wave 3 — test honesty

- **Transferred to [D-T1](../WIP/RemainingWork_2026-11-04.md#d-t1).** Add a failing-then-passing regression test for each P1 fix of the earlier reviews that has none (listed in
  [prior-closure.md](ProductionReview_2026-11-02/prior-closure.md)), and rewrite the tests that synthesise their own
  trigger or recovery.
- **Transferred to [D-T1](../WIP/RemainingWork_2026-11-04.md#d-t1).** Add pathological-input tests with explicit step budgets for MD-05/06/07, FLOW-05 and the math parser; add a
  fuzz harness (corpus-driven, deterministic seeds) for the Markdown parser, the YAML/TOML stage, the TeX parser, the
  MATH table reader and the Mermaid parser, run in ASan Debug.
- **Transferred to [D-T1](../WIP/RemainingWork_2026-11-04.md#d-t1).** Replace self-referential oracles named in the appendices (CF_HTML real-world samples, independent crossing
  count, real font-table truncation instead of sub-spans).

### Wave 4 — documentation, process and hygiene

- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** The remaining DOCS, TOOL and HOST-04 items, the Wave 0 decisions D8-D12 and the stale CI and capability text
  (DOCS-02, DOCS-06, TOOL-12).
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** Restructure `Specs/UI/UI_ControlsAndLayout.md` and `docs/rich-text.md` so normative requirements are separate
  from history (DOCS-15, DOCS-19).

### History rewrite (D9)

The developer decided on 2 November to remove the workstation identity from Git history, not only from new receipts.
A history rewrite changes every commit SHA from the first affected commit onward, so it is a coordinated operation with
its own freeze, executed once and not mixed with code changes. Never write the account name, host name or any other
personal value into this repository, its issues or its commit messages while doing it; keep the replacement list
outside the repository.

- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Forward redaction first.** Change the receipt writers so new evidence carries stable aliases instead of the
  account and host names (receipt schema bump, because the host field is a comparison key), stops writing whole-machine
  process snapshots, and keeps user-profile paths relative. Add a validator that fails on the replacement list's
  patterns in any tracked file. Land this before the rewrite so the rewrite does not need repeating.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Inventory.** List every tracked path and every commit message that contains the account name, the host name,
  user-profile paths or the process snapshot (DOCS-12 counts about 1,150 files for the host and about 960 for the
  account across `Measurements/`, mostly older than this window, plus one process snapshot). Include all branches and
  tags that will be kept, including the three branches pushed under D10.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Freeze and timing.** Merge or close open pull requests and park active worktrees (about 120 locally) first.
  Prefer running the rewrite immediately before the Wave 1 consumer pin, so the four consumers re-pin once, not twice.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Rewrite.** Use `git filter-repo` on a fresh mirror clone with a `--replace-text` expressions file kept outside
  the repository (and `--replace-message` for commit messages), plus path removal for raw process snapshots. Keep the
  generated `commit-map` as the old-to-new SHA record.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Verify before publishing.** On the rewritten mirror: zero matches for the replacement list across all refs;
  `validate.ps1` passes; tree contents other than the replaced strings are byte-identical (compare `git diff` per
  rewritten commit pair from the map); receipt hash manifests are regenerated or explicitly re-attested, because
  replacing text inside receipts changes their hashes.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Publish.** Temporarily allow a force push on the protected branches, force-push all kept branches and tags,
  restore protection immediately, and record the new `main` SHA in this plan.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Purge cached copies.** GitHub keeps `refs/pull/*` and cached commit views reachable by old SHA, and forks or
  clones keep their own copies; ask GitHub Support to remove cached views and pull-request references for the affected
  commits. Treat anything already cloned as published; no secrets were found, so no credential rotation is needed.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Repair SHA references.** Every SHA cited in `Specs/`, `Measurements/`, `docs/`, `Changes/` and this review
  (including `a0f67fd3`, `6df17007` and the D10 commits) becomes dangling. Rewrite them from the commit map in one
  mechanical commit, and keep the map in `Measurements/` so historical receipts stay attributable.
- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** **Consumers.** RedXe, RedSalamander, RedTilde and RedPrism pin DxUi by SHA in `Dependencies/DxUi.lock.json`; each
  must re-pin to the rewritten commit (old SHAs will no longer be on any branch). Notify the consumer owners before the
  force push and verify each re-pin after it.

### Wave 5 — after production

- **Transferred to [D-G1](../WIP/RemainingWork_2026-11-04.md#d-g1).** Architecture proposals ARCH-06 to ARCH-20 in the order recommended by
  [architecture.md](ProductionReview_2026-11-02/architecture.md): monolith splits behind existing seams, one Unicode
  service, header-only implementation by evidence, build throughput measured with vcperf first.

## Validation when implementation starts

- Use `Test-Changes.ps1 -Explain` while editing and `-Mode PrePush` before pushing. Code changes run `test.ps1` in x64
  Debug, Release and ASan Debug and build all three ARM64 configurations. Rich-presentation changes run the control
  and embedded suites, not Foundation alone.
- Focus-taking suites (`Menu`, `NativeTextInput` and the menu resource fixtures) run only through
  `test.ps1 -Interactive` with the developer's agreement to the time.
- Renderer-affecting changes (EDIT-01, SEM-01, SEM-02, RENDER-02) also need WARP, device-loss and clean/dirty/hidden
  resource evidence.
- Every change records one dated fragment under `Changes/`; durable behaviour moves into the owning contract when the
  plan closes.
- This plan moves to Done only when every box above is ticked with its commit and test, and no box is closed by
  narrative.

## Refuted and downgraded

Each appendix keeps a "Refuted" table with the line that refutes the claim. The most consequential:

- TOOL-01 "PR CI no longer runs the tooling tests" is the stated, tested contract (`Build_UnifiedCI.md:12`,
  `Test-Validation.ps1:324`); only `Test-AsanRuntime.ps1` running nowhere survives (TOOL-21).
- A11Y-04 "geometry exceptions terminate UIA calls" is unreachable: `LayoutIsCurrent` and the `Ready()`/`Current()`
  check precede the throwing helper.
- HOST-02 "embedded Cut with an empty selection deletes a character" is unreachable on the documented path: the only
  caller returns `S_FALSE` first. It survives as a missing precondition.
- HOST-04 "`apiRevision` must bump" contradicts the owner decision recorded in the 31 October review; it survives as
  the consumer addendum.
- CMD-04's proposed fix would reject valid display-math input; only the dead check is removed.
- ARCH-02's "device loss goes unnoticed" is wrong: loss is detected at the next draw. The misclassification survives.
