# RichText audit: defects, simplification and Markdown/math/Mermaid completion

Status: IMPLEMENTING. Current SDK qualification evidence is recorded in the linked 24 October continuation; it does not close the RichText implementation audit, D4c performance HOLD, physical IME/AT, or product acceptance. The continuation records the current 2fed local and hosted native evidence, its pending paired benchmark, and the still-unexplained earlier ARM64 Debug Menu failure. The original audit and planning baseline is retained below. The bounded math, flowchart,
state and class corrections are implemented in the [24 October continuation](RichTextAudit_2026-10-21/RedTildeCompletion_2026-10-24.md).
Their final-source qualification, performance and consumer handoff remain open; the full language roadmap is separate.
Owner request (2026-10-21): review all RichText code in DxUi for bugs and architectural simplification, use
cheaper agents where that helps to establish what a complete Markdown, Mermaid and math implementation needs,
and record a detailed specification plan of everything to fix or improve.
Starting revision: `fa5821c8`. No RichText source, test, plan or pinned data changed up to `b69f452f`, so every
location below applies to both. Main then merged the completed
[rich editor presentation plan](../Done/RichEditorPresentation_2026-10-22.md) and related work (through
`bba8da23`). That work changed files cited by 123 findings, including `DxUi.RichTextEditor.cpp`,
`DxUi.RichSemanticGeometry.cpp`, `DxUi.MarkdownBlocks.h` and `docs/rich-text.md`. Locations therefore refer to
`fa5821c8`; W0.3 revalidates them before any fix starts.

This plan owns remediation of the audit findings and the completion roadmap. It does not close or replace any gate
of [the rich-text editor plan](RichTextEditor_2026-10-17.md): its M5/M6 milestones, the D4c performance HOLD,
physical IME/AT, native ARM64 and consumer gates stay there. [File drag-and-drop](RichTextFileDrop_2026-10-20.md)
stays separate. Where an item here completes a parent milestone (for example M5c math or M5e.2b inline objects),
close it in the parent plan in the same change.

Appendices:

- [RedTilde completion continuation, 24 October](RichTextAudit_2026-10-21/RedTildeCompletion_2026-10-24.md):
  current-source corroboration, ordered library work packages and expanded compatibility/coverage gates for the
  consumer request. Existing W/T/C IDs remain the implementation owners; no product code changed in planning.
- [Findings register](RichTextAudit_2026-10-21/findings.md): every finding with location, failure scenario,
  proposed fix and verifier verdicts.
- [Completeness specification](RichTextAudit_2026-10-21/completeness.md): Markdown (MD-*), math (MA-*) and
  Mermaid (MM-*) gap inventories with priority and effort.

## How the audit was done

- **Finders.** Sixteen independent reviewers each read one partition completely: Markdown blocks, inlines,
  document core and v0 layout, RichTextEditor, structural commands, semantic builder and display map, semantic
  geometry, math parser, math layout, Mermaid, images and clipboard, accessibility, host integration, status
  truth, engine tests and host tests. They reported 273 findings with file, line, failure scenario and fix.
- **Verification.** Every bug, security and performance finding went to two independent verifiers, one trying to
  refute it and one constructing a concrete reproducer. Other findings went to a feasibility judge. A finding is
  *confirmed* only when no verifier refuted it. The lead independently re-read the most severe findings.
- **Completeness.** Three cheaper-model agents inventoried Markdown editing/rendering, math against KaTeX 0.16.11
  and Mermaid against 11.4.1 (see the completeness appendix).
- **Architecture.** Three reviewers proposed simplifications through layering, pipeline and extension-model
  lenses; a judge merged 37 proposals into a ranked, contract-checked roadmap. A completeness critic then looked
  for files and cross-cutting concerns the partitions missed.
- **Evidence.** Raw agent outputs, the workflow scripts and the merged verdicts are retained privately under
  `.build/reports/richtext-audit-20261021` (see its `RESUME.md`). They are working evidence, not committed data.

Result: 278 findings (273 from reviewers, 5 from the critic). 241 are confirmed, 16 disputed, 1 uncertain and 20
refuted. Disputed findings are scheduled only as optional cleanups or contract proposals. Refuted findings stay in
the register with the refuting evidence and are not scheduled. Every other finding is assigned to exactly one
checklist item below; the register's **plan** field names it.

## Conclusions at the 21 October audit baseline

The following findings describe the recorded audit revision, not current capability. The 24 October implementation
corrects the bounded math status/limits and state/class matrix discrepancies. Current tests retain all 105 original
flowcharts: 16 admitted, 89 explicitly unsupported; the separate 20-family matrix records two limited profiles and
18 unsupported families. The current math corpus has 30 positive, 20 negative and 27 symbol-identity fixtures.
These corrections do not close broad W/T/C rows, full TeX/Mermaid support, D4c, physical input/AT or final platform gates.

1. **Correctness.** One memory-safety defect (commands-01: `DeleteTableColumn` reads `slots[column - 1]` with
   `column == 0` for a one-cell body row) and a cluster of contract violations need prompt fixes: one formula
   whose text cannot lay out on one line drops the whole document to source; capacity admission is bypassed in two
   parser paths; a rejected formula overwrites its link's title; keyboard caret movement does not leave F2 source
   editing; embedded paste rewrites line endings; noexcept UIA range methods terminate on allocation failure;
   several structural commands and multi-file paste corrupt nested Markdown. Separately, the documented policy of
   retrying non-capacity preparation failures means a deterministic failure retries on every host update while
   painting is withheld; W1.2 proposes amending that policy.
2. **Architecture.** The semantic path is not incremental. Every keystroke restarts the parser at offset zero,
   rebuilds the semantic document and recreates every DirectWrite box, while the host withholds painting until
   all stages finish. The judge's analytic estimate is about 300 owner turns for a 1,000-paragraph document; this
   is not yet measured. The v0 renderer is therefore still the only responsive path, and nine hand-written
   resumable state machines, seven coalescing keys and three parallel native-object paths (math, diagrams,
   images) multiply the cost of every change.
3. **Truthfulness.** Status data contradicts the shipped code: `DxUiMath1-target.json` says math is not
   implemented and pins limits four times larger than the parser enforces; the Mermaid family matrix marks
   `stateDiagram` and `classDiagram` unsupported; README, docs and two normative specs describe completed work as
   pending and vice versa. AGENTS.md requires truthful capability status, so this is a defect class of its own.
4. **Completeness.** Parsing is complete for CommonMark/GFM. Math covers about 15 percent of KaTeX commands and
   lacks the TeX fundamentals that affect almost every formula (math italic, atom spacing, limits, display style).
   Mermaid admits 13 of 105 pinned flowchart examples and small state/class subsets. Editing lacks undo coalescing,
   find/replace, a decoration API, link activation and several common commands.

## Execution rules for every item

- Work on a focused branch per item or small group; record one `Changes/` fragment per change and fold before
  PrePush. Keep public declarations source-compatible: a removal or rename increments the API revision under
  [the toolchain contract](../../Build/Build_ToolchainAndConsumption.md) and is only done inside an agreed window.
- Write the failing regression test first, in both native and embedded hosts where the behaviour is host-visible,
  and keep its original failing receipt with the change.
- Use `Test-Changes.ps1 -Explain` while iterating. Closeout needs `test.ps1` in x64 Debug, Release and ASan Debug,
  the three ARM64 builds, `validate.ps1` and `format.ps1 -Check`. Renderer changes add WARP, device-loss and
  clean/dirty/hidden resource evidence.
- Items marked **paired** touch the preparation turn, paint or default renderer. Compare them against the Wave 0
  baseline on the same fixture; unpaired numbers never establish non-regression, and a confirmed degradation goes
  to the owner with measured options rather than a silent rebaseline.
- Review docs and gallery under [the documentation contract](../../Core/Core_Documentation.md) and update
  `docs/rich-text.md`, `capabilities.json` and the owning `Specs/UI` contract in the same change as the behaviour.
- Do not start focus-taking suites without the person's agreement to the time.

## Checklist and sequence

Waves 0 and 1 are prerequisites. Within a wave, items touching different files can run in parallel; the notes say
where two items share a file. Wave 3 is the measurable candidate for the parent plan's D4c HOLD.

### Wave 0: baselines and test scaffolding

- [ ] W0.1 Record retained baselines before any code change: the complex-UI FPS/memory pairing that `test.ps1`
  reports, plus a new semantic-editing fixture (RichMarkdown profile, about 1,000 paragraphs with lists, one table,
  two images, three formulas and one flowchart) measuring keystroke-to-ready owner turns through
  `DebugGetState`, the image-burst restart count and retained C++ storage.
- [ ] W0.2 Add a turn-bounded shared "prepare until settled" helper and allocation-sweep helper for the rich test
  binaries (tests-engine-11, tests-host-03, tests-host-19) so the new regression tests below do not add more
  copies.
- [ ] W0.3 Revalidate against current main before starting a fix. Re-read every finding whose file changed after
  `b69f452f` (123 findings, mostly in the editor, semantic geometry, block parser, v0 layout, accessibility,
  embedded host, image and docs files), update its location, and record any that the later work fixed or changed.
  Recheck the completeness entries that the presentation plan partly implemented: find highlights and decorations
  (MD-01, MD-02), link underlines (MD-05), the selection-changed event (MD-06), palette and spacing (MD-11, MD-31),
  Source syntax ink (MD-21) and opt-in remote image requests (MD-16). Regenerate the register and appendix with the
  archived generators.

### Wave 1: correctness defects

Each item lists the findings it closes. Severity and verdicts are in the register.

- [ ] W1.1 **Table and structural command defects** (`DxUi.RichTextCommands.cpp`): commands-01 (out-of-bounds
  read, critical), commands-02 (nested markers deleted by SetList/Task/Outdent), commands-03 (promoted header loses
  its EOL), commands-04 (column-0 insert breaks rows without a leading pipe), commands-05 (wrap across an existing
  emphasis/link), commands-06 (code conversion drops math and footnote references), commands-07 (Outdent of
  over-indented children), commands-08 (unwrap escapes all punctuation), commands-09 (Enter on quoted break
  strips `>`), commands-10 to commands-12, commands-14 and commands-15. Tests: one directed case per finding in the planner fixtures plus
  retained-editor undo/redo checks.
- [ ] W1.2 **Preparation failure containment** (`DxUi.RichTextEditor.cpp`, `DxUi.PreparedMath.h`,
  `DxUi.RichSemanticGeometry.cpp`, `DxUi.PreparedFlowchart.cpp`): the contract (`UI_ControlsAndLayout.md`, sticky
  capacity versus retryable failures) treats every non-capacity exception as retryable, so a deterministic
  failure in the builder, geometry or diagram stages rebuilds and fails on every host update and withholds
  painting. Amend the contract to memo a failed preparation by its full key and fall back to source once (judge
  rank 1; covers every throw site in geometry and flowchart preparation, not only the builder). Make one formula's
  layout failure local to that formula (math-layout-01, math-parser-01, MA-07) and drop a failing image upload to
  its alternative instead of failing the geometry. Add host-level tests that inject a persistent upload failure, a
  geometry invariant throw and a newline inside `\text{}`, and count preparation attempts across idle passes.
- [ ] W1.3 **Parser admission and conformance** (`DxUi.MarkdownBlocks.h`, `DxUi.MarkdownInlines.h`): route
  reference definitions through node admission and make every remaining-budget subtraction saturating
  (blocks-01); charge emphasis wrappers and autolink children to the staging budget (inlines-04); bound the
  extended-autolink pass per work unit and stop re-decoding the remaining node after every splice (inlines-01,
  inlines-02, inlines-09); use original delimiter-run lengths for the rule of three (inlines-03); keep caret sites
  for trailing blank lines of indented code (blocks-02); attach a source-editing region only to its matching
  container (blocks-03); fix the prefix-less remainder after partial reference extraction (blocks-13) and the
  escaped table-pipe source hole (inlines-11). Decide and test blocks-04, blocks-05 and blocks-06 against the pinned
  reference behaviour. Rerun all 1,324 pinned semantic vectors.
- [ ] W1.4 **Editor input and lifetime** (`DxUi.RichTextEditor.cpp`): keyboard caret movement leaves an F2
  envelope (editor-01); typing into empty/padded cells of over-cap tables falls back to raw insertion instead of
  dropping characters (editor-02); IME preview imports invalidate rather than clear retained geometry (editor-03,
  paired); OnMouseDown takes focus only after it knows the press is honoured (editor-18); TryPrepareClipboardPaste
  returns false instead of throwing (editor-13); consistent failure bookkeeping (editor-15). Structured paste
  respects containers: multi-file references inside a table cell, list item or quote keep the container intact
  (critic-01), and paste or TSF import at an empty or padded cell materialises the focused cell as typing does
  (critic-02). Cut of a content-only formatted selection shares deletion's empty-wrapper cleanup (critic-03).
  Add the missing
  tests for envelope exit, over-cap typing and preview chunk reuse (editor-17). Shares the file with W1.2, W2.2 and
  W2.6; land in that order or coordinate.
- [ ] W1.5 **Math element ownership and admission ledger** (`DxUi.RichSemanticDocument.cpp`,
  `DxUi.SemanticExtensions.h`, `DxUi.RichImageResources.h`): create a Math element for every equation, accepted or
  rejected, and never write into the parent's title (semantic-03, math-parser-05, judge rank 2); then one native
  object ledger with one constant and one admission function for math, diagrams and images, counting admitted
  objects and reporting capacity for every type (semantic-02, semantic-12, judge rank 3). Contract sentence in
  `Specs/UI/UI_ControlsAndLayout.md` changes if "candidates" becomes "admitted".
- [ ] W1.6 **Math parser and recognizer defects** (`DxUi.NativeMath.cpp`, `DxUi.MarkdownInlines.h`): epsilon and
  phi glyph swap, `\cdot` code point (status-truth-06, math-layout-10, MA-03); `\(...\)` must not inherit the
  dollar currency heuristics, and the docs state the final edge rules (math-parser-02, inlines-10); closer search must respect code spans and raw HTML
  (math-parser-03); trailing `\\` before `\end` (math-parser-04, MA-05); `\limits`/`\nolimits` must not become an
  empty script base (math-parser-08); `\sqrt[...]` index parsing with shared budgets (math-parser-09); caps checked
  before push (math-parser-11); reject `\\` outside rows (math-parser-12, MA-06); stop fusing adjacent text atoms
  (math-parser-13); consistent unclosed `$$` (math-parser-17, disputed: decide whether the short case gains a
  diagnostic);
  ASCII minus/asterisk/prime and spacing widths (math-layout-11, math-layout-13, MA-21).
- [ ] W1.12 **Math placement defects** (`DxUi.RichSemanticGeometry.cpp`): size formulas from the enclosing run's
  font, so math in headings matches the heading (math-layout-09); anchor the caret beside a tall formula to the
  text baseline (math-layout-22). Native and embedded geometry tests at three DPIs.
- [ ] W1.7 **Mermaid grammar defects** (`DxUi.NativeFlowchart.cpp`): state/class descriptions, transition labels
  and aliases go through label validation (mermaid-01, MM-35); flowchart shapes and `|label|` are rejected inside
  state/class bodies (mermaid-07, MM-37); valid but unsupported Mermaid is classified Unsupported, not Malformed
  (mermaid-14, MM-14); correct diagnostics for alias and relation errors (status-truth-17); `:::` shorthand and
  `accTitle`/`accDescr` no longer become labels or nodes (MM-33, MM-34); `direction` parsing is whitespace-robust
  (MM-62); replace the synthetic `inherits` label with an edge kind (mermaid-09, MM-48); apply backslash escapes
  only to quoted labels (mermaid-13).
- [ ] W1.8 **Host integration** (`TextInputServices.cpp`, `Embedded.cpp`, sample): embedded paste keeps exact
  line endings for RichTextEditor (host-integration-01); hiding, zero-sizing or replacing the device keeps logical
  focus as the native host does (host-integration-02); distinguish content-preparation failure from benign
  incomplete layout and schedule a wakeup (host-integration-04); sample fixes for per-message TSF refresh, pointer
  leave and layout notification on window move (host-integration-03, -11, -12).
- [ ] W1.9 **Images and paths** (`DxUi.RichImagePaths.h`, `DxUi.RichImage.cpp`, `TextClipboard.cpp`): a
  512-component clipboard path keeps the quoted-literal fallback (images-clipboard-05); guard the diagnostics seam
  (images-clipboard-06); statistics and EXIF strictness (images-clipboard-14, -15); add the missing JPEG EXIF
  orientation test (images-clipboard-07). Network destinations stay caller policy by contract, so a document can
  still drive UNC or WebDAV requests when the application enables network files (`//host/...`, escaped
  backslashes, `file://server/...`). State that policy once in `docs/rich-text.md`, give the sample's loader a
  tested refusal for every such spelling, and add path vectors for each.
- [ ] W1.10 **Geometry and accessibility defects**: caret stepping through reordered footnotes follows source
  order (geometry-01); culling accounts for glyph overhang (geometry-04); table cells get a minimum width
  (geometry-13); visible ranges stop at visible lines (geometry-15); noexcept UIA range methods catch allocation
  failure and return `E_OUTOFMEMORY` (accessibility-01); stale-publication nulls are not reported as out-of-memory
  (accessibility-04). Add the missing allocation sweep over TextPattern range commands (accessibility-10).
- [ ] W1.11 **Truthful status** (documentation and pinned data only): reconcile `DxUiMath1-target.json` status,
  limits, command lists and envelopes with the implementation and pin it with a corpus test (status-truth-01, -04,
  -05, math-parser-07, math-layout-18, tests-engine-09, MA-39, MA-41); mark `stateDiagram`/`classDiagram` as limited profiles in the family matrix,
  generator and test (status-truth-03, mermaid-05, tests-engine-04, MM-58); fix README.md, `docs/rich-text.md`
  introduction and test list, `ThirdParty/Markdown/README.md`, the design-system guideline, the stale normative
  sentences in `UI_ControlsAndLayout.md` and `UI_InputAndAccessibility.md`, the parent plan's M5 checkboxes, the
  missing flowchart capability entry and `RichMath.h` limit comment (status-truth-02, -07 to -15, -18, -19,
  mermaid-04, mermaid-10, images-clipboard-02, tests-host-15, MD-48 to MD-50); add `RichMath.h` to the consumer
  header compilation (tests-host-04); document the clipboard transform discovery and Prepare side effects
  (host-integration-06 to -08, -13). Regenerate the gallery provenance from a clean tree (status-truth-21).

### Wave 2: simplification and hygiene

Ordered by the judge's benefit/(risk x effort). None changes public behaviour.

- [ ] W2.1 Delete the unreachable whole-layout creation in `PreparedDocumentLayout::Draw` and skip the futile
  whole-projection join (document-01; judge rank 4; paired).
- [ ] W2.2 One `LayoutKey` value and three named guards in RichTextEditor: presentation gate, composition guard and
  owner mark (editor-07, editor-08, editor-09; judge rank 5; paired).
- [ ] W2.3 Derive the D2D factory from the raster target and remove the editor's peek into
  `SemanticExtensionAccess` (judge rank 6).
- [ ] W2.4 Charge math leaves and diagram labels as block steps, cache math text formats, bound uploads per turn by
  bytes, and make `RichTextEditorDebugState` report the active stage truthfully (judge ranks 7 and 9; paired).
- [ ] W2.5 Move `PreparedLayout`'s semantic plumbing from the public header behind one Detail access struct, then
  build semantic boxes from display text plus runs and delete `IdentityProjection` (judge ranks 8 and 10;
  geometry-05, math-layout-21; sequential; paired). Header text changes, so consumers recompile.
- [ ] W2.6 One internal RichTextEditor access seam for the 24 accessibility casts, a one-shot accessibility view
  and an image-service facade (judge rank 14; accessibility-09).
- [ ] W2.7 Paint indexes decorations and boxes by top and draws the selection once per intersecting box
  (geometry-02, editor-06; judge rank 15; paired).
- [ ] W2.8 Caret and erase fallbacks during pending layout use a bounded source window instead of whole-document
  projection (editor-05, the per-keystroke part of document-11; judge rank 16; paired).
- [ ] W2.9 Identity reconciliation uses one sorted vector and releases the previous tree early (judge rank 17;
  paired for memory).
- [ ] W2.10 Shared physical-line/EOL scanner so paste and structural commands agree on the document EOL
  (editor-12; judge rank 18), and shared UTF-16/ASCII-class predicates plus `LayoutOptions` equality
  (blocks-09, inlines-06, commands-17; judge rank 19).
- [ ] W2.11 Extension cleanup: duplicated flowchart validation, uniform exception retirement, redundant math node
  counter, memoised math measurement, font and locale from `LayoutOptions` (judge rank 20; mermaid-11, -12, -15,
  status-truth-16, math-parser-10, -15, -16, math-layout-17, -20; MA-08).
- [ ] W2.12 Split `Style`/`StyleRun` into a new `RichStyle.h` (judge rank 21) and name the MSVC Debug
  fallible-construction idiom once, with parser reserve after admission (judge rank 22; math-parser-06,
  status-truth-20).
- [ ] W2.13 Dead or redundant code and oversized functions found by the partitions: blocks-07, -08, -10, -11, -12,
  -14, -15; inlines-05, -07, -08; document-02 to -10, -12, -15 to -18; editor-10, -16; commands-16, -18, -19;
  semantic-04, -05, -07 to -09, -11, -13, -14; geometry-06 to -12; accessibility-02, -03, -05 to -07;
  images-clipboard-03, -04, -08 to -13, -16, -17; host-integration-05, -10. Each is small; group by file.
  Several are disputed because a verifier found the current behaviour contracted (geometry-09, geometry-11,
  semantic-05, semantic-11, semantic-13, host-integration-10); treat those as optional cleanups or contract
  proposals, not defects.

- [ ] W2.14 Localizable accessibility text: the about 25 English UIA names, help texts and diagnostics produced for
  footnotes, diagrams, math and F2 hints get one consumer override, following the library's existing pattern
  (`SetStepButtonNames`, `SetLabels`, `SetNoMatchesText`), with English defaults unchanged (critic-05).

### Wave 3: incremental semantic preparation (paired; D4c candidate)

- [ ] W3.1 Geometry reuse: a Reuse phase in `PreparedSemanticGeometry` keyed by block identity, display slice,
  runs, indent, gutter, native placement set and table key, so unchanged boxes keep their DirectWrite layouts
  across documents (judge rank 11). Pixel oracles must stay byte-identical.
- [ ] W3.2 Block-level incremental parse: reuse closed root blocks before the tracked change and shifted blocks
  after it when closure is proven by lookahead, with whole-parse fallback; keep the lexical partition incremental.
- [ ] W3.3 Let the builder run in the same turn as a completed parser when budget remains.
- [ ] W3.4 Image completions invalidate instead of clearing, coalesce to one snapshot per owner turn and keep
  in-flight decodes across edits (judge rank 12; editor-04, images-clipboard-10, -13, -17).
- [ ] W3.5 Diagram and math prepared objects survive width-only reflow (mermaid-02, MM-25).
- [ ] W3.6 Measure the Wave 0 fixture before and after; update the turns-to-readiness paragraph in
  `docs/rich-text.md` with measured numbers; hand the paired result to the parent plan's D4c decision.

### Wave 4: consolidation

- [ ] W4.1 One `StagedService`/`CooperativeParser` lifecycle template for Detail services with a single generation
  policy (judge rank 23; paired).
- [ ] W4.2 Drop redundant retained text copies in `SemanticElement::name` and `SourceDisplayMap` (semantic-07,
  semantic-11; judge rank 24; paired for memory).
- [ ] W4.3 Unified diagnostics shape (kind, object, family detail) with one description function and existing
  public fields kept (judge rank 25; MM-27, MA-38).
- [ ] W4.4 Real translation units for the parser units and generated tables (judge rank 26), then internal range
  unification on `MarkdownRange` (judge rank 27).

### Wave 5: native-object abstraction

- [ ] W5.1 One native-object pipeline (parse, admit, measure, place, paint, UIA alternative, source entry) for math,
  diagrams and images, introduced first as adapters with no behaviour change, then by collapsing the geometry
  phases (judge rank 28; MM-56).
- [ ] W5.2 Shared native-object contract test kit (judge rank 29).
- [ ] W5.3 Optional source renames by layer with explicit test-scope rules, only together with W4.4 (judge
  rank 30).

### Test track (parallel with Waves 1 to 3)

- [ ] T1 Math: AST serializer oracle, every `DxUiMath1-target.json` case, PreparedMath placement per node kind,
  delimiter grammar cases, record-cap and cancellation tests, UIA Math children and editor F2/clipboard/hit-test
  coverage (tests-engine-01, -02, -05, -06, -08, -10; tests-host-01; math-parser-18; math-layout-19; MA-42).
- [ ] T2 Mermaid: directed state/class scenario tables, expectation `reason` mapping, edge-endpoint attachment and
  non-rectangle raster oracles (tests-engine-03, -07, -18, -24; mermaid-06; MM-60, MM-61).
- [ ] T3 Test infrastructure: shared native/embedded scenario templates, raster fixtures, boundary helpers,
  anonymous-namespace linkage, deterministic overlap and turn constants (tests-engine-11 to -17, -21 to -23;
  tests-host-02, -05, -06, -07, -09 to -14, -16, -17, -19, -20). Add the Accessibility suite to the scoped test
  rules for `src/Controls/DxUi.Rich*` and `include/DxUi/Rich*` (critic-04).
- [ ] T4 Coverage no partition owned (critic): RTL fixtures for inline/display math and diagrams with a stated
  contract; math and diagram raster at 120, 144 and 168 DPI with rule pixel snapping; high-contrast and translucent
  palette tiles for math, diagrams and table rules; undo/redo across F2 envelopes, into atomic objects and around
  image requests; Paint allocation counts and no scroll mutation during Paint; an audit that every UIA-thread path
  reaches the editor only through owner dispatch; a check for fused multiply-add sensitivity before native ARM64
  pixel oracles; Measurements/RichText README claims against the code; and a corpus harness that runs every pinned
  Mermaid fence and KaTeX support row through the parsers to replace the hand-traced coverage numbers.

### Completion tracks (after Wave 1; owner scope acceptance per track)

Each entry names completeness IDs. Library scope only; application UI stays in consumers.

- [ ] C1 Markdown enablers: decoration API (MD-02), selection-changed event and command-state queries (MD-06),
  link activation and fragment navigation (MD-05), scroll API and visible scrollbar (MD-04).
- [ ] C2 Markdown editing fidelity: undo coalescing (MD-03), Enter/soft-break policy (MD-14), context menu
  (MD-17), shortcut coverage (MD-18), find/replace on the decoration API (MD-01), quote/code-block/insertion
  commands (MD-08, MD-09, MD-38), Tab in code and Source view (MD-19), footnote traversal and insertion (MD-37,
  parent M5e.3), inline object editing (MD-07, parent M5e.2b).
- [ ] C3 Markdown rendering fidelity: palette roles (MD-11), code-block presentation and pluggable tokenizer
  (MD-10), content-aware tables (MD-12), HTML subset growth (MD-15), list/heading spacing (MD-31), Source view
  presentation (MD-21), then medium/low items.
- [ ] C4 Math fidelity before breadth: math italic and atom classes with TeX spacing (MA-01, MA-22;
  math-layout-02, math-layout-03), display style and limits (MA-02, MD-28; math-layout-04, math-layout-08),
  OpenType MATH constants, glyph variants and script/fraction metrics with fallback (MA-37; math-layout-05 to
  math-layout-07, math-layout-12, math-layout-14 to math-layout-16), display envelopes and centering (MA-35),
  prime notation (MA-09).
- [ ] C5 Math breadth: generated class-annotated symbol table pinned to KaTeX data (MA-12, MA-23 to MA-27),
  named operators (MA-11), delimiters and sizing (MA-04, MA-13, MA-14), binomials (MA-10), over/under constructs
  (MA-17, MA-18), accents (MA-16), fonts (MA-19), environments (MA-15), annotation and color (MA-28, MA-20).
- [ ] C6 Mermaid robustness: direct routing for adjacent ranks and per-lane widths (MM-21; mermaid-03, mermaid-08),
  crossing reduction (MM-22), label wrapping (MM-24), label false positives (MM-10), per-object fit if the owner
  accepts the contract change (MM-23, geometry-03).
- [ ] C7 Mermaid flowchart parity: edge styles and inline edge text (MM-02 to MM-06), shapes (MM-07), labels with
  line breaks and entities (MM-09, MM-11), subgraphs (MM-01), styling within a theme palette (MM-18),
  accessibility statements (MM-17).
- [ ] C8 Mermaid families: state composites, pseudo-states and notes (MM-29 to MM-31), class blocks, relations,
  cardinality and compartments (MM-38 to MM-44), then `sequenceDiagram` (MM-49) after W5.1, then pie, erDiagram,
  gantt and mindmap (MM-50 to MM-53). Every other family stays exact source with a precise diagnostic.

## Decisions that belong to the owner

- **Per-object fit.** Today one over-wide diagram drops the whole document to source; images and math scale.
  Accepting a per-object fallback (MM-23, geometry-03) amends `UI_ControlsAndLayout.md`.
- **Math target.** Whether the next math profile is fidelity first (C4) and what admission limits it pins.
  The current pinned target and the implementation disagree; W1.11 aligns the data with the code, not the
  ambition.
- **Mermaid scope.** Which families beyond flowchart/state/class are wanted, and whether sequenceDiagram waits for
  the native-object abstraction (recommended).
- **Editing defaults.** Enter and soft-break policy (MD-14) and undo coalescing granularity (MD-03).
- **Selection replacement and copy.** Deleting, typing over or pasting over a visual selection that crosses a
  hidden delimiter edits the raw interval, so `**bold** tail` with `ld ta` deleted becomes `**boil`. Copy always
  transfers exact source. Decide whether rich-mode replacement excludes hidden delimiters and whether copy offers a
  rendered or plain-text alternative.
- **Failure policy.** W1.2 changes the documented retry semantics for non-capacity preparation failures.
- **Network destinations.** Whether the library itself should refuse document-driven UNC/WebDAV spellings by
  default, or keep that entirely in application loader policy (W1.9 documents and tests the current policy).
- **v0 retirement.** Deferred until W3 is measured and accepted, the M6 capacity decision lets arbitrary 64 Ki
  documents fit the semantic path, consumers are checked and an API-revision window is agreed. W2.1, W2.5 and
  W2.12 are the preparatory steps that need no public removal.
- **Public API windows.** `MarkdownRange` overloads and a Control capability virtual were rejected for now; revisit
  only inside an agreed API-revision window.

## Closeout

Close each checklist item when its findings are fixed with regression tests, its validation passes and the owning
contracts and docs are current. Completion tracks close when the owner-accepted scope ships. Move this plan to
Done only when Waves 1 to 3 and the test track are complete and every remaining finding is either fixed, explicitly
deferred with a reason, or transferred to the parent plan.
