# Native Mermaid diagrams: parsing, layout and rendering: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: `NativeFlowchart` (parser), `NativeFlowchartLayout`,
`PreparedFlowchart` (shaping, palette, paint), `NativeColor`, and their callers' preparation-failure and accessibility
paths for the flowchart, state, class, ER, pie, Gantt, sequence and mind map profiles.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity. Geometry and timing figures are
derived from the code (test-harness stub label sizes where stated), not measured.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| FLOW-01 | P1 | Confirmed | Trailing space inside a styled label span publishes an inadmissible model and the whole document drops to source | `src/Controls/DxUi.NativeFlowchart.cpp:555` | M |
| FLOW-02 | P1 | Confirmed | ER and class cardinality labels collide whenever a node has two relationships on the same side | `src/Controls/DxUi.NativeFlowchartLayout.cpp:1097` | M |
| FLOW-03 | P2 | Confirmed | State and class notes are placed after routing and checked only against nodes | `src/Controls/DxUi.NativeFlowchartLayout.cpp:940` | M |
| FLOW-04 | P3 | Confirmed | In Subtle presentation authored node fill and text colors are discarded unless the author sets a dark stroke | `src/Controls/DxUi.PreparedFlowchart.cpp:884` | S |
| FLOW-05 | P3 | Confirmed | Cartesian edge expansion shapes up to 512 identical edge labels before the extent cap is checked | `src/Controls/DxUi.NativeFlowchart.cpp:2050` | M |
| FLOW-06 | P3 | Confirmed | The `<letter` HTML lookahead scans to the end of the statement, so `A{"i<n?"} --> B` is rejected | `src/Controls/DxUi.NativeFlowchart.cpp:252` | S |
| FLOW-07 | P3 | Confirmed | Scanner pipe and bracket state is not family-aware: `%%` and `;` are lost after class `<|--` and ER `{` | `src/Controls/DxUi.NativeFlowchart.cpp:2183` | S |
| FLOW-08 | P3 | Confirmed | `flowchart` or `graph` without a direction is reported as malformed; unrelated limits reuse `NodeCapacity` | `src/Controls/DxUi.NativeFlowchart.cpp:940` | S |
| FLOW-09 | P3 | Confirmed | Whole-source `find` calls inside per-statement parsing | `src/Controls/DxUi.NativeFlowchart.cpp:1769` | S |
| FLOW-10 | P3 | Confirmed | Accessible alternative: state start and end are both `[*]`, choice, fork and join read as an empty label | `src/Controls/DxUi.RichSemanticDocument.cpp:1790` | S |
| FLOW-11 | P3 | Confirmed | Pie shading repeats every four slices (documented design) | `src/Controls/DxUi.PreparedFlowchart.cpp:762` | S |
| FLOW-12 | P3 | Confirmed | `ParseNativeColor` accepts unprefixed 3-digit hex; `EncodeNativeColor` copies its result | `src/Controls/DxUi.NativeColor.h:189` | S |
| FLOW-13 | P3 | Confirmed | Paint recomputes WCAG contrast for every node and edge on every frame (authored-color diagrams only) | `src/Controls/DxUi.PreparedFlowchart.cpp:884` | S |
| FLOW-14 | P3 | Confirmed | Layout allocates about 170 KB of fixed staging per `Begin` regardless of graph size | `src/Controls/DxUi.NativeFlowchartLayout.cpp:76` | M |
| FLOW-15 | P3 | Plausible | Empty pseudostate labels are measured as a full text line | `src/Controls/DxUi.NativeFlowchartLayout.cpp:283` | S |

## P1 findings

### FLOW-01 — Trailing space inside a styled label span publishes an inadmissible model and the whole document drops to source

- **Severity** P1 (reviewer P1, verifier P1) · **Category** bug, contract violation · **Verdict** Confirmed ·
  **Confidence** high · **Effort** M · **Introduced** `c8d60c45` (span recording, 2026-11-01); the trims are older
  (`c1a77e42f`, `aa2ff53f6`) · systemic class: ARCH-03 in `architecture.md` (a `logic_error` from preparation escalates to a
  whole-document fallback); MATH-02 in `math.md` is another instance
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:510-512` (unquoted close trim), `:546-557` (span recorded at the
  closing tag, `:555`), `:608-612` (the same trim on the `implicitClose` path), `:672` and `:730` (node `Label` calls),
  `:1907-1919` (inline edge label), `:2016` (pipe label), `:2092-2145` (finish block, no validation);
  rejection at `src/Controls/DxUi.PreparedFlowchart.cpp:380-381` and `:383-388`, and
  `src/Controls/DxUi.NativeFlowchartLayout.cpp:1393-1394` and `:1395-1401`, via `ValidLabelStyles`
  (`src/Controls/DxUi.NativeFlowchart.h:105-116`);
  escalation at `src/Controls/DxUi.RichSemanticGeometry.cpp:2396-2398` and `:2845-2849` (rethrow),
  `src/Controls/DxUi.RichEditorLayout.h:42-48`, and `src/Controls/DxUi.RichTextEditor.cpp:4102-4132`
  (`catch (const std::logic_error&)`).
- **What**:
  - `Label()` records a style span as `{open.first, value.size(), flags}` when it sees the closing tag. For an unquoted
    label it then removes trailing whitespace from `value` (`while (! value.empty() && Space(value.back())) value.pop_back();`
    at `:511-512`, and again at `:610-611`). The span is never clamped, so `span.last` can exceed the final label length.
    The parser still publishes `FlowchartOutcome::Prepared`.
  - `PreparedFlowchart::Begin` and `NativeFlowchartLayout::Begin` both require `span.last <= label.size()` through
    `ValidLabelStyles` and throw `std::invalid_argument` ("Invalid native node stroke width" / "Invalid native edge label
    styles").
  - `PreparedSemanticGeometry::Advance` calls `placement.paint->Begin(...)` (`:2397`) with no local catch. Its outer
    `catch (const std::exception&)` (`:2845`) resets staging and rethrows. `RichEditorLayout::AdvancePreparation` has no
    catch. There is no per-fence catch earlier in the chain; `RichSemanticDocument.cpp:1672-1736` only consumes
    `graph.outcome`.
  - `RichTextEditor::PrepareContent` (try at `:3590`) sets `richPreparationActive` at `:4000` and calls
    `AdvancePreparation()` at `:4008`. `invalid_argument` is a `logic_error`, so it reaches `:4102`, which stores a
    `preparationRejected` key (`:4107-4119`, including the source revision) and clears the semantic document. On the next
    call `RejectsPreparation` (`:462-469`) matches, `:3838` skips the semantic document and `:3908-3909` selects exact source for
    the whole view. Every edit changes the key, retries, and fails again until the label is fixed.
  - Read-only and embedded hosting use the same function: `RichEditorLayout` and `PreparedSemanticGeometry` are used only by
    `RichTextEditor`, and the embedded host reaches it through `Control::PrepareContent` (`src/Controls/DxUi.Controls.cpp:2434`).
- **Failure scenario**:
  - A document containing a Mermaid fence with this body:
    ```
    flowchart LR
    A[<b>Warning: </b>] --> B
    ```
    Trace: `<b>` opens a span at offset 0; `Warning: ` makes `value.size() == 9`; `</b>` records `{0, 9, Bold}`; `]` closes and
    the trim shortens the label to `Warning:` (8 units). The span `[0,9)` is longer than the label.
  - Other triggers: `A -- <b>yes </b> --> B` (inline edge label; `labelEnd` is trimmed before the call, the in-label space is
    not), `A -->|<i>maybe </i>| B` (pipe label), `A[<b>x<br></b>]` (the `<br>` newline is trimmed), `A[a<b> </b>]` (span that
    is entirely trimmed), and the asymmetric form `A>text <b>x </b>]`.
  - Result: the heading, lists, tables and every other fence in the document render as raw source; only
    `Debug::Warning` says why.
- **Fix**: fix the root cause in the parser, in two parts.
  1. Record spans against the final label. Add a small helper (for example `ClampStyles(FlowchartLabelStyles&, std::size_t length)`)
     that sets `last = std::min(last, length)` and removes any span with `first >= last` (stable, preserving order). Call it
     after both trim loops (`:511-512` and `:610-611`). Recording spans after trimming is not possible at the close tag because
     the final trim is not yet known; clamping at the trim sites is equivalent.
  2. Add a parser-side admission check so the parser never publishes a model that `PreparedFlowchart::Begin` or
     `NativeFlowchartLayout::Begin` would reject. Extract the model-only invariants that both services duplicate
     (`PreparedFlowchart.cpp:365-429`, `NativeFlowchartLayout.cpp:1378-1420`: label styles, stroke widths, colors, shapes, note
     targets, edge relation and cardinality rules, group masks, compartment offsets) into one function declared in
     `DxUi.NativeFlowchart.h` (for example `bool ValidFlowchartModel(const FlowchartModel&) noexcept`). Call it from the finish
     block of `NativeFlowchartParser::Preparation::Step()` before `done = true` (`:2144`). On failure publish
     `FlowchartOutcome::Unsupported` with a new `FlowchartIssue::InvalidModel` and the whole-body range, so the fence becomes a
     per-fence source fallback with a diagnostic through the existing `RejectDiagram` path
     (`RichSemanticDocument.cpp:1683-1694`). Keep the services' own checks as a backstop that calls the same function.
  - Pitfall: do not map every `PreparedFlowchart::Begin` failure to a per-object reject in `RichSemanticGeometry`. `Begin` also
    throws `invalid_argument` for option admission (`fontSizeDip`, `diagramLabelWidthDip`, font family and locale lengths, reading
    direction at `PreparedFlowchart.cpp:355-362`); those are per-view errors and must stay view-level. Share only the
    model-only checks, not the option or measurement checks.
- **Regression test**:
  - Parser (`Tests/Support/Support.Tests.MarkdownBlocks.h`): for `flowchart LR\nA[<b>x </b>]\n`, `A -- <b>yes </b> --> B`,
    `A -->|<i>maybe </i>| B`, `A[<b>x<br></b>]` and `A[a<b> </b>]`, assert every published node and edge satisfies
    `ValidLabelStyles(styles, label.size(), true)` and that no span is empty. Fails at `6df17007` (for the first case the label
    is `x`, the span `[0,2)`).
  - Services (`Tests/Support/Support.Tests.PreparedFlowchart.h`): `PreparedFlowchart::Begin` on the model of
    `A[<b>x </b>]` must not throw (it throws at `6df17007`).
  - Property test over the pinned corpus (`NativeFlowchartCorpus::flowcharts`, `familyExamples`, `allFences`, plus a small
    fuzz set of styled labels with leading, trailing and internal spaces, `<br>`, entities and CRLF): every model the parser
    publishes as `Prepared` is accepted by `NativeFlowchartLayout::Begin` and `PreparedFlowchart::Begin` (neither throws). Assert
    `plan->issue == FlowchartLayoutIssue::None` and run `FlowchartLayoutTests::Invariants` only for the families where those
    invariants apply (flowchart, state and class now; ER after FLOW-02), because `Invariants` requires node labels inside their
    bounds, which pie, Gantt and sequence layouts do not satisfy.
  - Editor level (`Tests/Support/Support.Tests.RichSemanticEditor.h` or `Controls.Tests.RichSemanticGeometry.cpp`): a document
    `# Title\n\n` + a fence containing `A[<b>x </b>]` keeps the heading rendered rich (`MarkdownRenderingStatus::Semantic`) and
    only that fence shows source with a diagnostic.
- **Verifier notes**: traced character by character; the span `[0,9)` on an 8-unit label is confirmed. Confirmed that no catch
  between `PreparedFlowchart::Begin` and `RichTextEditor.cpp:4102` intervenes, and that read-only and embedded use the same path.
  No existing test has trailing space inside a styled span (the closest, `PreparedFlowchart.h:1214`, uses `<b>go</b>`). The
  reviewer's claim that this "contradicts docs/rich-text.md" is overstated: `docs/rich-text.md:1687-1689` promises per-object
  fallback only for a persistent native geometry-capacity failure, and whole-view fallback on a deterministic `logic_error` is
  tested policy (`Tests/Support/Support.Tests.RichSemanticEditor.h:3092-3150`, stage 2).
- **Lead note**: severity stays P1. Even if whole-view fallback is tested policy for deterministic preparation failures, a parser
  defect that publishes an inadmissible model for an ordinary label turns the entire document into raw source, which is a
  user-visible defect. The fix belongs at the root (clamp plus parser-side admission), not in mapping `Begin` failures to
  per-object rejects.

### FLOW-02 — ER and class cardinality labels collide whenever a node has two relationships on the same side

- **Severity** P1 (reviewer P1, verifier P1; borderline P2) · **Category** bug, capability truthfulness · **Verdict**
  Confirmed · **Confidence** high · **Effort** M
- **Location**: `src/Controls/DxUi.NativeFlowchartLayout.cpp:1060-1066` (start side and `fromX`/`toX` are the node centre),
  `:1097-1098` (cardinality rectangles anchored at `fromX + 4`, `start + 2` and `toX + 4`, `end - h - 2`), `:1178-1180` and
  `:1232-1287` (`CardinalityCheck`), `:751-800` (`Ports` offsets only the Y of `first`/`last`),
  `src/Controls/DxUi.PreparedFlowchart.cpp:577`, `:692-695`; `src/Controls/DxUi.RichSemanticGeometry.cpp:2408-2412`.
- **What**:
  - Every edge leaving a node starts at `(fromX, start)` with `fromX` the node centre; ports (`startPort`, `endPort`) change only
    the Y of the second vertex. So all edges that leave the same node on the same side get a `fromCardinality` rectangle with
    the same top-left corner, and all edges that enter a node on the same side get an identical `toCardinality` corner. Two
    non-empty rectangles with one shared corner always overlap under the strict test at `:1247-1248`.
  - `CardinalityCheck` runs whenever any edge has a cardinality (`:1178-1180`). It compares each cardinality with every other
    edge's cardinality and calls `Fail(FlowchartLayoutIssue::DiagramExtent)` (`:1274-1285`). `PreparedFlowchart::Ready()` is false,
    the semantic geometry rejects the element, and the editor keeps that fence as exact source.
  - In ER every edge has both cardinalities (`ErEnd` fills them, `NativeFlowchart.cpp:1213-1223`), and ER is never `direct`.
    Two back edges from one node also collide (shared top). Class diagrams collide the same way when both cardinalities are
    present.
- **Failure scenario**:
  - The pinned reference example (`Tests/Support/Support.Tests.NativeFlowchartCorpus.h:124`, first example of the Mermaid ER
    document, expected `Prepared`, 4 nodes, 3 edges, top-bottom):
    ```
    erDiagram
        CUSTOMER ||--o{ ORDER : places
        ORDER ||--|{ LINE-ITEM : contains
        CUSTOMER }|..|{ DELIVERY-ADDRESS : uses
    ```
    CUSTOMER to ORDER and CUSTOMER to DELIVERY-ADDRESS are both forward edges from rank 0, so their `fromCardinality`
    rectangles ("1" and "1..*") share a corner. Layout fails with `DiagramExtent` and the fence shows exact source.
  - Fan-in: `erDiagram\nA ||--o{ C : x\nB ||--o{ C : y\n`. Class: `A "1" --> "*" B` plus `A "1" --> "*" C`.
  - Chains and trees where each entity is a "from" once and a "to" once render (all existing ER and class tests are chains:
    `Tests/Support/Support.Tests.FlowchartLayout.h:104-126`, `PreparedFlowchart.h:1631`,
    `Tests/Embedded/Embedded.Tests.RichTextResources.h:2045, 2337, 2557, 2626`).
- **Fix**: assign per-node-side attachment slots.
  - In `Ports()` (or a new stage before `Routes`) count, per node, the edges that start and end on each side (bottom for forward
    starts and back ends, top for forward ends and back starts). Give each edge a slot index per endpoint, ordered by the other
    endpoint's position in its row so slots do not create needless crossings.
  - In `Routes()` set `fromX = from.x + from.width * (slot + 1) / (count + 1)` and likewise `toX` for the end side. Use them for
    the path vertices, the arrow and diamond (`:1090-1093`), and the cardinality rectangles (`:1097-1098`).
  - Pitfalls: `direct[]` routes set `cross = toX` (`:1069`); with distinct `fromX` they gain a jog in the corridor `first`, so
    re-verify orthogonality and crossing counts. `originalPaths` (`:1075-1082`) recomputes the pre-ordering `oldFrom`/`oldTo`
    from the node centre and must use the same slot offsets. ER endpoint marks (`PreparedFlowchart.cpp:814-852`) derive from the
    path ends and follow automatically. The slanted-shape `inset` in `TransformEdges` (`:1204-1221`) tests the point against the
    node centre and must be made relative to the attachment. Raster baselines will move.
  - Keep `CardinalityCheck` as the safety net. Offsetting only the labels is a weaker alternative: edges still share a trunk and
    the labels beside it stay ambiguous.
- **Regression test**:
  - `Tests/Support/Support.Tests.FlowchartLayout.h`: add the pinned ER example above as an explicit case, plus
    `erDiagram\nA ||--o{ B : x\nA ||--o{ C : y\n`, the fan-in form, `classDiagram\nA "1" --> "*" B\nA "1" --> "*" C\n` and a
    two-back-edge form. Assert `plan->issue == FlowchartLayoutIssue::None`, `Invariants`, pairwise-disjoint cardinality
    rectangles and distinct attachment X values. Each fails at `6df17007` with `DiagramExtent`.
  - Extend `FlowchartLayoutTests::Corpus` (`:575-578`, which iterates `flowcharts` only) to lay out every `Prepared` entry of
    `NativeFlowchartCorpus::familyExamples` and assert `issue == None`. Apply `Invariants` only to state, class and ER: it requires
    node labels inside their bounds, which pie legends (`NativeFlowchartLayout.cpp:343-344`) and sequence and Gantt layouts do not
    satisfy. If another family example fails for an unrelated reason, allow-list it with a reason rather than weakening the test.
- **Verifier notes**: confirmed with the layout code and the decoded corpus entry. `docs/rich-text.md:1866-1872` documents fail-closed
  geometry for class cardinality overlap, so class behaviour is "documented fail-closed", but the cause is the shared anchor and
  not real label crowding. The ER paragraphs (`docs/rich-text.md:1810-1813`, `Specs/UI/UI_ControlsAndLayout.md:2706-2713`) say
  nothing about it, and the spec's "the ER example prepares four entities and three relationships" (`:2226`) is parser-level only.
  "Ordinary ER diagrams always fall back" holds only for diagrams with an entity on the same side of two or more relationships.
- **Lead note**: severity stays P1. The class-cardinality fail-closed note does not cover ER, and Mermaid's own reference ER
  example falls back to source, which conflicts with the repository rule to keep public capability status truthful.

## P2 findings

### FLOW-03 — State and class notes are placed after routing and checked only against nodes

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug · **Verdict** Confirmed · **Confidence** medium-high (geometry
  derived, not executed) · **Effort** M · **Introduced** `ff646cab`, `999769b5`
- **Location**: `src/Controls/DxUi.NativeFlowchartLayout.cpp:628-646` (a Note edge is ranked like a forward edge),
  `:821-828` (`Rows` widens the note's rank row), `:846-910` (`NodesPosition`), `:911-939` (`NotePositions`),
  `:940-962` (`NoteCollision`, nodes only), `:1027-1099` (`Routes`, lanes at `:1069-1072`), `:1313-1316` (stage order);
  paint order `src/Controls/DxUi.PreparedFlowchart.cpp:790-866` (edges), `:872-969` (opaque nodes and notes), `:972-996` (labels last).
- **What**:
  - A note is a node connected to its target by a `Note` edge. That edge defaults to `route == Forward` and
    `minimumRanks == 1` (`NativeFlowchart.h:200`), so `Rank()` gives the note `rank[target] + 1` and `Rows()` widens that row.
  - `NotePositions` later moves the note beside its target (`target.x - w - 12`, or `+ width + 12`), which can put it outside the
    band, over the exterior lane area. `NoteCollision` checks only note-versus-node. Routes are computed afterwards (stage
    order `NodesPosition`, `NotePositions`, `NoteCollision`, `GroupPositions`, `Routes`) and nothing compares routes or edge
    labels with note rectangles. The layout code has no route-versus-node interior check at all; only the test oracle
    `InteriorSegment` does (`Tests/Support/Support.Tests.FlowchartLayout.h:352-357, 435`).
  - A back or multi-rank edge whose lane spans the note's row then has a vertical segment inside the note. Edges are painted
    before the opaque note, so the segment disappears under it; a labelled lane paints its label over the note.
  - `hasNotes` disables `direct` routes (`:766`), so every forward edge uses the right lane, but adjacent-rank lane verticals sit
    in the inter-rank corridor and do not cross a note; only back and multi-rank edges do.
- **Failure scenario**:
  ```
  stateDiagram-v2
      A --> B
      B --> C
      C --> A
      note left of B : note
  ```
  With the harness stubs (labels 16x12, so nodes and the note are 40 wide; `Margin 8`, `PortGap 16`, `NodeGap 24`): row 2 is
  C (40) + 24 + note (40) = 104, which sets the band; the back lane for C to A sits at `x = 8 + 16 - 0 - 16 = 8`; B.x = 56 and
  the note spans x in [4, 44]. The lane at x = 8 runs from above row 2 to below row 0, through the note, so the C to A edge is
  hidden under it. With real label widths the collision holds whenever `note.width > C.width - B.width` and a lane exists on that
  side. `docs/rich-text.md:1902` promises that colliding notes fail closed to exact source.
- **Fix**:
  - Minimum fix: after `Routes` (and the transform that fixes final rectangles), fail closed on note-versus-route-segment and
    note-versus-edge-label overlap, reusing the orthogonal segment test from `InteriorSegment` (strict interior). Keep
    `NoteCollision` for node overlap. This matches the documented contract and loses capability only where the output is
    already wrong.
  - Fuller fix (optional, larger): do not rank notes. Skip Note-relation edges in `Indegree` and `RankEdges`, assign
    `rank[note] = rank[target]`, and reserve the note width beside its target when building the row (`rowWidth`), so the band
    includes it. Pitfall: an unranked note has `indegree` 0 and `ChooseRank` (`:615-624`) would pick it as a root, so special-case
    it. Note placement must then stay consistent with `NodesPosition` ordering.
- **Regression test**: `Tests/Support/Support.Tests.FlowchartLayout.h`: lay out the source above through `Place(Graph(...))` and
  run `Invariants`. The "edge routes avoid node interiors, including skipped ranks" check (`:435`) fails at `6df17007` for the C
  to A route against the note (note x range [4, 44], lane x 8). After the minimum fix, the same input must instead publish
  `issue == FlowchartLayoutIssue::DiagramExtent` (assert whichever contract is chosen, but one of the two). Add a mirrored
  `note right of` case with a forward multi-rank edge. The existing `StateNoteRaster` has only an isolated state with no edges.
- **Verifier notes**: the worked arithmetic above was re-derived by the verifier from `Measure`, `Rows`, `NodesPosition`,
  `NotePositions` and `Routes`; the reviewer's ~116 and -2 figures correspond to real font widths.

## P3 findings

### FLOW-04 — In Subtle presentation authored node fill and text colors are discarded unless the author sets a dark stroke

- **Severity** P3 (reviewer P2, verifier P3) · **Category** conformance, specification gap · **Verdict** Confirmed ·
  **Confidence** high on the logic · **Effort** S · **Introduced** `c4c1be02`
- **Location**: `src/Controls/DxUi.PreparedFlowchart.cpp:878-896`; color context at
  `src/Controls/DxUi.RichTextEditor.cpp:4743` (`DiagramColorContext{style.text, subtleMarkdown ? host.GetTheme().borderDefault :
  style.caret, style.fill}`); theme values `src/Controls/DxUi.Theme.cpp:1091, 1141, 1194`; spec text
  `Specs/UI/UI_ControlsAndLayout.md:2744-2747` and `docs/rich-text.md:1839-1840`.
- **What**: authored colors are adopted only when `Contrast(candidateFill, text) >= 4.5` and `Contrast(candidateFill,
  candidateStroke) >= 3.0` (`:884-885`). With no authored stroke, `candidateStroke` is the theme stroke. In Subtle that is
  `borderDefault`, a deliberately light outline (below 2:1 against the fill in both themes), so the test fails for every node even when
  the author only set a text color. The else-branch (`:894`) can adopt only an authored stroke. Default presentation uses
  `style.caret` and works.
- **Failure scenario**: Subtle presentation, `flowchart LR\nA[x]\nstyle A color:#000080` keeps the theme text color;
  `classDef hot fill:#ffcc00,color:#000` is ignored unless the author also adds a dark `stroke:`.
- **Fix**: evaluate the outline condition only for an outline the author influenced:
  `(fillAuthored || strokeAuthored) ? Contrast(fill, stroke) >= 3.0 : true`, and when only the fill is authored compare the
  theme stroke against the new fill (not against a fixed 3:1 the theme itself cannot meet), for example against
  `min(3.0, theme-stroke contrast on the original fill)`. Update the specification sentence at `UI_ControlsAndLayout.md:2744-2747`
  and `docs/rich-text.md:1839-1840` with the code.
- **Regression test**: `Tests/Support/Support.Tests.PreparedFlowchart.h`, `PaletteRaster` (contexts at `:868, 932`): add
  `DiagramColorContext{Black, ColorF(0.804f, 0.804f, 0.804f), White}` with `style A color:#000080` and assert navy text pixels;
  fails at `6df17007`. Existing contexts use Blue or Black strokes only.
- **Verifier notes**: the code is spec-literal ("the chosen text and outline must contrast against the chosen fill by at least
  4.5:1 and 3:1"), fails safe to theme colors, and Subtle is opt-in (`docs/rich-text.md:88`), hence P3. It becomes P2 if a
  consumer enables Subtle by default.

### FLOW-05 — Cartesian edge expansion shapes up to 512 identical edge labels before the extent cap is checked

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf, resource amplification · **Verdict** Confirmed ·
  **Confidence** medium-high · **Effort** M
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:2044-2060` (`auto expanded = edge;` copies label, style array and
  cardinalities per expanded edge), `src/Controls/DxUi.PreparedFlowchart.cpp:458-537` (one `CreateTextLayout` per edge label,
  via `State::Label` `:265-317`), `:560-571` (layout begins only after all labels exist; `labelCapacity` `:313` tests one label),
  `src/Controls/DxUi.NativeFlowchartLayout.cpp:770, 797-799` (labelled lanes are exclusive) and `:813-817` (the band check in `Rows`).
- **What**: the parser admits `|previousGroup| * |nextGroup| <= 512 - edges` (`:2045`). A statement such as
  `A1&...&A16 -- <1000-unit label> --> B1&...&B32` (about 1.2k source units, 48 nodes) expands to 512 edges that each copy a
  1000-unit label (about 1.4 MB of model per fence including the 776-byte style arrays). `PreparedFlowchart` then builds 512
  separate `IDWriteTextLayout`s for the same text. Every labelled edge needs an exclusive lane of about 183 DIP, so
  `rightWidth` is about 93.7k, which exceeds `MaximumExtent` (65,536) in `Rows`; the geometry is rejected only after all shaping.
  The per-document record cap (32,768, `RichSemanticDocument.cpp:1708`) admits about 58 such fences (560 records each).
- **Failure scenario**: a document of about 58 such fences (about 75 KB) keeps preparation busy for many owner turns (4 layouts
  per turn, `include/DxUi/RichTextLayout.h:174`) and holds several multi-megabyte transient models and layout sets before every
  fence falls back to source. Work is bounded and cooperative; I did not measure it.
- **Fix**: intern the `IDWriteTextLayout` (and measured size) per distinct `(label text, style spans)` in the `Labels` stage of
  `PreparedFlowchart::Advance`, so expanded edges share one layout (layouts are immutable after creation and `Draw` only passes an
  origin). Key by content, or by the shared `edge.source` range plus label. With one shared measurement the exclusive-lane lower
  bound (labelled-edge count times measured cross size plus `Margin`) is exact and can reject before `BeginGeometry`. Do not change
  `FlowchartEdge` ownership (it is used by `ValidLabelStyles`, the accessibility alternative and layout); interning the layout is
  much smaller. A lower bound using `PortGap` alone (512 x 16) never rejects.
- **Regression test**: `Tests/Support/Support.Tests.PreparedFlowchart.h` with `CountingWriteFactory`
  (`Tests/Support/Support.Tests.CountingWriteFactory.h`): a 16x32 fan-out with one edge label must perform exactly one
  `CreateTextLayout` for edge labels; it performs 512 at `6df17007`. Optionally assert the 1000-unit variant reaches the
  `DiagramExtent` rejection after one measurement.
- **Verifier notes**: the 512 bound, the unconditional per-edge shaping and the late extent check are all confirmed. Downgraded:
  work is spread by the per-turn layout budget, rejected fences are remembered and not retried on unrelated edits, and the trigger is
  adversarial rather than authored (realistic fan-outs produce a handful of short duplicates). Promote to P2 if untrusted Markdown is
  in the threat model.

### FLOW-06 — The `<letter` HTML lookahead scans to the end of the statement, so `A{"i<n?"} --> B` is rejected

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:244-262` (`remaining = Text().substr(at, end - at)` at `:248`, the
  `html` test at `:252-255`), `:518-532` (style-tag search `rest.find(L'>')` over the same range).
- **What**: `PlainTextUnit` treats `<` followed by a letter as HTML when any `>` appears later in the statement. `end` is the
  statement end, so the `>` of the edge operator `-->` counts. `Label()` passes the statement end for quoted labels as well.
- **Failure scenario**: `flowchart TD\nA{"i<n?"} --> B` returns `Unsupported` / `UnsupportedLabelMarkup` and the fence shows source.
  The same label alone on a line is accepted, and `A["x < 5"]` (space after `<`) is accepted, which is all the existing test
  covers (`Tests/Support/Support.Tests.MarkdownBlocks.h:957`). `docs/rich-text.md:1723` says comparison characters remain text.
- **Fix**: bound both lookaheads to the label's own extent. Compute the label end once in `Label()` (the matching unescaped quote
  for a quoted label; the close character for an unquoted one; the statement end only for `implicitClose`) and pass it to
  `PlainTextUnit` and to the `:532` search.
- **Regression test**: `MarkdownBlocks.h`: `flowchart TD\nA["i<n"] --> B\n` is `Prepared` with label `i<n`; also
  `A{"i<n?"} --> B`, `A[x<y] --> B`. Fails at `6df17007`. Keep `A["<b>HTML</b>"]` and `A[<script>text</script>]` unsupported.

### FLOW-07 — Scanner pipe and bracket state is not family-aware: `%%` and `;` are lost after class `<|--` and ER `{`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance · **Verdict** Confirmed (the finding was incomplete) ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:2149` (`%%` gate), `:2160` (`;` gate), `:2183-2184` (`pipe = ! pipe`),
  `:2187-2190` (`depth`), `:2168-2169` (reset only at a newline), ER statement `:1183-1239` (label via `PlainText` at `:1233`).
- **What**: `Step()` toggles `pipe` on every `|` and counts `[({` / `])}` as `depth` for every family, and honours `;` and `%%`
  only when `! pipe && ! depth`. Only flowchart edge labels (`-->|text|`) pair pipes; class `<|--` and `..|>` and ER `||`, `|{`,
  `o{` do not. An odd number of pipes, or an ER `{` without a matching `}`, leaves the state set until the end of the line.
- **Failure scenario**:
  - `classDiagram\nA <|-- B %% inherits`: `pipe` stays set, `%%` is not a comment, the relation parser meets `%%` after `B` and
    returns `UnsupportedEdge` (`:1998`); the fence falls back to source. `A <|-- B; C <|-- D` fails the same way.
  - ER is worse than fallback. `erDiagram\nA ||--o{ B : x %% c` leaves `depth` at 1 (`o{`), so `%%` is not a comment and
    `PlainText` accepts `x %% c` as the relationship label. `erDiagram\nA ||--|{ B : x; B ||--o{ C : y` is published as one
    edge whose label is `x; B ||--o{ C : y`: a silent mis-render, not a fallback.
- **Fix**: make the scanner family-aware. Toggle `pipe` only when `model->family == NativeDiagramFamily::Flowchart` (the family
  is set by the header statement at its newline, before any later line is scanned), and count `depth` only for the families whose
  statements have bracketed labels. Fixing `pipe` alone does not fix the ER `{` case. Pitfall: run the pinned corpus
  (`CheckOriginalCorpus`, `CheckFamilyGrammar`) because `;` splitting inside bracketed text changes statement boundaries.
- **Regression test**: `MarkdownBlocks.h`: `classDiagram\nA <|-- B %% c\n` is `Prepared` with 1 edge; `classDiagram\nA <|-- B; C <|-- D\n`
  has 2 edges; `erDiagram\nA ||--o{ B : x %% c\n` has 1 edge labelled `x`; `erDiagram\nA ||--|{ B : x; B ||--o{ C : y\n` has 2 edges
  labelled `x` and `y`. All fail at `6df17007` (the last yields 1 edge).
- **Verifier notes**: the class case is as reported; the ER case and the incompleteness of "toggle `pipe` only for flowcharts"
  were found by the verifier. The state only matters for a `%%` or `;` on the same line after such tokens.

### FLOW-08 — `flowchart` or `graph` without a direction is reported as malformed; unrelated limits reuse `NodeCapacity`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance, diagnostics · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:938-953` (header); `:451` (group count), `:983` (subgraph count and depth),
  `:1055` (mind map depth), `:1176` (Gantt 366-day span) all report `FlowchartIssue::NodeCapacity`;
  `src/Controls/DxUi.RichSemanticDocument.cpp:1686-1693` (user-visible text keys off `outcome`).
- **What**: the pinned Mermaid grammar accepts `graph` / `flowchart` followed directly by a newline and sets direction `TB`
  (`ThirdParty/Markdown/mermaid-11.4.1/flow.jison.txt:127, 325-326`, `GRAPH NODIR`). DxUi returns `InvalidDirection` (outcome
  `Malformed`). No pinned corpus fence starts with a direction-less header, so no test pins the behavior.
- **Failure scenario**: `flowchart\nA-->B` is reported as malformed and shown as source although it is valid Mermaid.
- **Fix**: when the header line ends after the keyword, default to `TopBottom` and mark the header consumed. The second half
  (separate `GroupCapacity` / `DepthCapacity` issues) is cosmetic: `diagnostic.issue` is not read in `src` (only tests and debug
  inspection), and the user-visible text already keys off `outcome`; add them only if the diagnostics are surfaced later.
- **Regression test**: `MarkdownBlocks.h`: `flowchart\nA-->B` and `graph\nA-->B` are `Prepared` with `TopBottom` and 1 edge; fails at
  `6df17007`. Keep `flowchart ZZ\nA --> B` malformed (`:381`).

### FLOW-09 — Whole-source `find` calls inside per-statement parsing

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.NativeFlowchart.cpp:693` (`find(L']', at)`), `:1137` (`find(L':', at)`, Gantt), `:1401`
  (`find(L">>", at + 2u)`, class annotation), `:1769` (`find(marker, at)`, three per `state ...` statement).
- **What**: each search runs from `at` to the end of the whole source instead of the statement `end`, and the result is discarded
  when it is at or beyond `end`. Worst case is about 1,024 statements x 3 x 16k units, a few milliseconds spread over `Step`
  calls. Bounded; the least important finding.
- **Failure scenario**: none observable; only wasted scanning.
- **Fix**: search `Text().substr(at, end - at)` (or pass `end` to a bounded helper) and drop the `>= end` checks.
- **Regression test**: none needed beyond the existing parser tests.
- **Verifier notes**: the line numbers in the reviewer's list are correct.

### FLOW-10 — Accessible alternative: state start and end are both `[*]`, choice, fork and join read as an empty label

- **Severity** P3 (reviewer P3, verifier P3) · **Category** a11y · **Verdict** Confirmed · **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.RichSemanticDocument.cpp:1790` (node entries `id = label; `), `:1885` (edge entries use the
  identifier); labels from `src/Controls/DxUi.NativeFlowchart.cpp:653, 662-663` (start `●`, end `◉`, identifier is the `[*]`
  range) and `:1756-1783` (`label.clear()` for `<<choice>>`, `<<fork>>`, `<<join>>`).
- **What**: start and end both read `[*] = ●;` / `[*] = ◉;`, so the only difference is a glyph name. Choice, fork and join
  read `C = ;` and never say what they are. Connections read `[*] — A; A — [*]`, which is ambiguous.
- **Failure scenario**: a screen reader user navigating `stateDiagram-v2\n[*] --> A\nstate C <<choice>>\nA --> C\nC --> [*]`
  cannot tell the start from the end or learn that `C` is a decision.
- **Fix**: add localized vocabulary through new `AccessibilityTextId`s (start, end, choice, fork, join) and emit them in the
  node and edge entries from the node shape and the `@start`/`@end` key, keeping the identifier so edges remain matchable.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp`: the diagram name for the source above contains
  "start", "choice" and "end" (and the fork and join words for a fork source). Fails at `6df17007`. No existing test pins the
  current pseudostate text.

### FLOW-11 — Pie shading repeats every four slices (documented design)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** a11y, visual · **Verdict** Confirmed (observation, not a defect) ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.PreparedFlowchart.cpp:762` (`.28f + .22f * (index % 4u)`); documented at
  `Specs/UI/UI_ControlsAndLayout.md:2702-2703`.
- **What**: with up to 16 slices, slices i, i+4, i+8 and i+12 share one opacity for sector and legend swatch. Adjacent sectors
  always differ and sectors follow source order clockwise with an ordered legend, and the specification states "repeated
  theme-derived shades with a matching ordered legend". The accessible alternative is correct.
- **Failure scenario**: a viewer cannot match a legend entry to a sector by shade alone when more than four slices exist.
- **Fix**: backlog enhancement. Add a second cue for indices of 4 and above (a hatch or outline pattern) and update the
  specification. Not a release blocker.
- **Regression test**: none until the enhancement is scheduled; then a raster test comparing slice 0 and slice 4 pixels.
- **Verifier notes**: reclassified from a defect to a documented limitation.

### FLOW-12 — `ParseNativeColor` accepts unprefixed 3-digit hex; `EncodeNativeColor` copies its result

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance, simplification · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.NativeColor.h:189-207` (optional `#` stripped, then 3 or 6 digits), `:218`
  (`return std::wstring(result);`). Only native math calls it (`src/Controls/DxUi.NativeMath.cpp`, `src/Controls/DxUi.PreparedMath.h`).
- **What**: after the named-color lookup misses, `bad` resolves to `#bbaadd` and `decade` to `#decade`. The pinned KaTeX document
  says "The '#' is optional before a six-digit specification" (`ThirdParty/Markdown/katex-0.16.11/supported.md.txt:592`), so
  an unprefixed 3-digit value is an unknown name there. The project specification only says "three- and six-digit hexadecimal
  values" (`Specs/UI/UI_ControlsAndLayout.md:2617`), so this is a KaTeX deviation, not a clear specification violation.
  Flowchart styles use their own `#`-required parser (`NativeFlowchart.cpp:166-185`). `std::wstring(result)` copy-constructs from an
  lvalue, which prevents the move (6 units, negligible).
- **Failure scenario**: `\color{bad}` renders `#bbaadd` instead of falling back to the default ink.
- **Fix**: require `#` for the 3-digit form (keep unprefixed 6-digit), and `return result;`.
- **Regression test**: `Tests/Support/Support.Tests.MathCorpus.h` (existing checks at `:233-234`): `ParseNativeColor(L"bad")` is empty,
  `ParseNativeColor(L"bbaadd")` is `0xbbaadd`, `ParseNativeColor(L"#bad")` is `0xbbaadd`; the new first case fails at `6df17007`.

### FLOW-13 — Paint recomputes WCAG contrast for every node and edge on every frame (authored-color diagrams only)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.PreparedFlowchart.cpp:17-33` (`Contrast`, `Composite`, 3 `std::pow` per color), `:797`,
  `:878-896`; palette gate `:138-143` (`DiagramPalette::Needed`) and `src/Controls/DxUi.RichSemanticGeometry.cpp:2419`.
- **What**: `Draw` calls `Contrast` up to three times per node (`:884, 885, 894`) and once per authored edge stroke. The result
  depends only on (model, `DiagramColorContext`). A palette exists only when some node or edge has an authored color, so
  other diagrams pay nothing. Estimated cost for 256 nodes is under 0.3 ms per paint; unmeasured.
- **Failure scenario**: none functional; avoidable arithmetic in the paint path of authored-color diagrams.
- **Fix**: resolve the per-node and per-edge brush choice once in `DiagramPalette` for a given `DiagramColorContext` (recompute
  when the context changes), and have `Draw` index the result. This is also the single place to fix FLOW-04.
- **Regression test**: covered by the FLOW-04 raster test; add a check that two draws with the same context and a changed
  context produce the same and different pixels respectively.

### FLOW-14 — Layout allocates about 170 KB of fixed staging per `Begin` regardless of graph size

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf, resources · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M
- **Location**: `src/Controls/DxUi.NativeFlowchartLayout.cpp:76-109` (fixed `std::array` members), `:1445` (`std::make_unique`);
  parser staging `src/Controls/DxUi.NativeFlowchart.cpp:40-66` (about 35 KB).
- **What**: `originalPaths` is 512 x 6 x 16 B = 48 KB, `laneRanks` 16 KB, plus about 20 further arrays of 256 or 512 elements, in one
  zero-initialised heap block per `Begin`. A two-node diagram pays the same as a 256-node one on every reflow or typography
  change. It is freed on publication (`PreparedFlowchart.cpp:655-662`) and only one is live at a time; the cost is microseconds.
  Not measured.
- **Failure scenario**: none functional; avoidable memory churn.
- **Fix**: size the vectors from `graph->nodes.size()` and `edges.size()` at `Begin`, or reuse one staging block per owner. Low
  value; schedule only with other layout changes (FLOW-02 and FLOW-03 touch the same struct).
- **Regression test**: none required; optionally assert a staging-byte counter scales with graph size.

### FLOW-15 — Empty pseudostate labels are measured as a full text line

- **Severity** P3 (reviewer P3, verifier P3) · **Category** visual · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S
- **Location**: `src/Controls/DxUi.NativeFlowchartLayout.cpp:283-324` (`Measure`), `:906-909` (label rectangle), `:1157-1159`
  (`Include(node.label)`).
- **What**: node size is `max(40, labelWidth + 24)` by `max(32, labelHeight + 16)` (`:284-285`), doubled for a diamond
  (`:295-299`). If DirectWrite reports one line of height (about 19 DIP) for an empty layout (not provable from the repository),
  an empty choice diamond is 80x70. Fork and join bars are a fixed 64x8 (`:287-291`) but `NodesPosition` gives them a label
  rectangle `{.., Cross(label), Major(label)}` with a line height, and `TransformNodes` includes it, so the rectangle can
  extend past the 8 DIP bar and widen the extent. The `[*]` circle is `hypot` of the minimum box (about 53 DIP).
- **Failure scenario**: oversized choice diamonds and slightly padded fork/join extents in state diagrams.
- **Fix**: treat an empty label as 0x0 in `Measure` and `NodesPosition`, and give pseudostates fixed sizes.
- **Regression test**: `Tests/Support/Support.Tests.FlowchartLayout.h` (choice and fork cases at `:194-202`): assert fork/join label
  rectangles have zero area and choice size equals the fixed pseudostate size. Existing tests check topology and shape only, so the
  raster baselines for choice and fork will change.
- **Verifier notes**: arithmetic confirmed; the DirectWrite empty-layout height is the one step that is not statically provable.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| none | The verifier refuted no finding; severity was lowered for FLOW-04 and FLOW-05 only | not applicable |

## Area architecture and simplification proposals

1. **One model validator, run at the parser boundary.** Rationale: `NativeFlowchartLayout::Begin` (`:1373-1444`) and
   `PreparedFlowchart::Begin` (`:355-429`) carry near-identical lists of about 40 invariants and the parser checks none, so any
   drift becomes a `std::invalid_argument` that the editor turns into a whole-document fallback (FLOW-01, ARCH-03). Proposal:
   a model-only `bool ValidFlowchartModel(const FlowchartModel&) noexcept` called by the parser before publishing (a violation
   becomes `Unsupported` with `FlowchartIssue::InvalidModel`) and by both services as a backstop. Keep option and
   measurement validation in the services; do not map all `Begin` failures to per-object rejects (per-view option errors must
   stay view-level). Files: `DxUi.NativeFlowchart.h/.cpp`, `DxUi.NativeFlowchartLayout.cpp`, `DxUi.PreparedFlowchart.cpp`. Risk low,
   effort M. Before: an inadmissible published model blanks the whole view. After: the fence falls back with a diagnostic.
2. **Port distribution along node sides** (FLOW-02; also stops coincident arrowheads and ER marks). Assign attachment slots per node
   side in `Ports()` and use them in `Routes()`. Files: `DxUi.NativeFlowchartLayout.cpp`. Risk medium (raster baselines shift,
   `direct` routes and ordering comparison need care), effort M. Before: ER and class hubs fall back to source. After: they render.
3. **Notes as row attachments, not ranked nodes** (FLOW-03 only; FLOW-15 is independent). Exclude Note edges from ranking, reserve
   the note width in the target's row, and check notes against routes and labels. Files: `DxUi.NativeFlowchartLayout.cpp`. Risk
   medium (special-case in `ChooseRank`), effort M. Before: edges can disappear under notes. After: collisions fail closed or are
   avoided by construction.
4. **Label interning** (FLOW-05). One `IDWriteTextLayout` per distinct `(text, styles)` per diagram; expanded edges reference it.
   Files: `DxUi.PreparedFlowchart.cpp`. Risk low, effort M. Before: up to 512 layouts per fence. After: one per distinct label.
5. **Pre-staged color decisions** (FLOW-13, single place for FLOW-04). `DiagramPalette` resolves per-node and per-edge brushes for a
   `DiagramColorContext`; `Draw` only indexes them. Files: `DxUi.PreparedFlowchart.cpp/.h`. Risk low, effort S. Before: contrast math
   in every paint. After: none.
6. **Scanner and grammar split.** Statement splitting (`Step`) tracks quote, bracket and pipe state for all families although only
   flowcharts have pipe labels and bracketed labels (FLOW-07). Family-specific tokenization removes a class of false rejections and
   one silent mis-parse. Files: `DxUi.NativeFlowchart.cpp`. Risk low (run the pinned corpus), effort S. Before: odd pipes or ER
   braces swallow `%%` and `;`. After: they behave as in flowcharts.

## Test-quality observations

- The all-fences corpus (`Tests/Support/Support.Tests.MarkdownBlocks.h:528-560`) is a smoke test: it checks that parsing terminates
  and is structurally sane for all 342 pinned fences, but no admitted fence is laid out or prepared, so neither FLOW-01 nor FLOW-02
  is caught. Run every `Prepared` fence through both services (acceptance) and through layout with `Invariants` where they apply.
- No parser-to-services round-trip property exists. A test that every published model is accepted by
  `PreparedFlowchart::Begin` and `NativeFlowchartLayout::Begin`, over the corpus and a small fuzz set, would have caught FLOW-01.
- `FlowchartLayoutTests::Corpus` (`Tests/Support/Support.Tests.FlowchartLayout.h:575-578`) iterates `NativeFlowchartCorpus::flowcharts`
  only, never `familyExamples`, which is why the canonical ER example is not laid out anywhere.
- The palette oracle uses maximal-contrast contexts only (Blue or Black on White, `Support.Tests.PreparedFlowchart.h:868, 932`;
  `Support.Tests.RichSemanticGeometry.h:1105`). The Subtle `borderDefault` context (FLOW-04) is untested.
- Topology coverage gaps: notes are tested only on isolated nodes (`PreparedFlowchart.h:1936-1944`); ER and class cardinality tests
  are all single chains; there is no note plus back-edge or note plus multi-rank-edge fixture (FLOW-02, FLOW-03).
- No pathological-cost test: the 512-edge fixture (`FlowchartLayout.h:561-571`) uses zero-size labels, and nothing asserts a
  `CreateTextLayout` count for a fan-out (FLOW-05).
- The crossing oracle (`FlowchartLayout.h:468-484, 548-560`) re-implements `ProperCrossings` almost verbatim, so a shared blind spot
  (collinear overlapping segments, coincident trunks) cannot be detected (reviewer observation; not independently verified).
- The existing `Invariants` (`FlowchartLayout.h:358-439`) are genuinely independent geometric oracles for graph families (containment,
  non-overlap, orthogonality, interior avoidance); they do not apply to pie, Gantt and sequence layouts.

## Questions for the developer

1. Is fail-closed geometry for any ER entity with two relationships on the same side (FLOW-02) known and accepted? Recommended
   answer: no. `docs/rich-text.md:1866-1872` documents fail-closed only for class cardinality overlap, and the ER documentation says
   nothing, so implement per-side attachment slots before treating the ER profile as shipped.
2. Should a deterministic per-diagram preparation failure (`std::invalid_argument` or `logic_error` from `PreparedFlowchart`) ever escalate
   to the editor's whole-document `preparationRejected` path? (False premise in part: `docs/rich-text.md:1687-1689` scopes the
   per-fence promise to capacity failures and whole-view fallback for deterministic failures is tested policy at
   `Support.Tests.RichSemanticEditor.h:3092-3150`; see ARCH-03 in `architecture.md`.) Recommended answer: keep the view-level policy for
   option and device errors, but make the parser incapable of publishing an inadmissible model (FLOW-01) so diagram content never
   reaches that path.
3. Are notes intentionally ranked as ordinary nodes, which reserves and widens a row they never occupy? Recommended answer: no explanation
   exists in the code; stop ranking them (FLOW-03 fuller fix) or at least fail closed on route overlap.
4. In Subtle presentation, is it intended that authored text and fill colors need an authored dark stroke to be honored? Recommended
   answer: no; the specification sentence at `UI_ControlsAndLayout.md:2744-2747` should apply the outline test only to outlines the author
   influenced (FLOW-04).
5. Should `flowchart` or `graph` without a direction be admitted? Recommended answer: yes; the pinned Mermaid grammar defaults to `TB`
   (`flow.jison.txt:325-326`) (FLOW-08).

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.NativeFlowchart.cpp` | full (2267 lines at HEAD; nearly all added in the window) |
| `src/Controls/DxUi.NativeFlowchart.h` | full |
| `src/Controls/DxUi.NativeFlowchartLayout.cpp` | full |
| `src/Controls/DxUi.NativeFlowchartLayout.h` | full |
| `src/Controls/DxUi.PreparedFlowchart.cpp` | full |
| `src/Controls/DxUi.PreparedFlowchart.h` | full |
| `src/Controls/DxUi.NativeColor.h` | full (new in the window) |
| `src/Controls/DxUi.NativeColorPalette.h` | full (new in the window) |
| `Tools/Generate-NativeFlowchartTestData.ps1` | full, plus its diff (inputs hash-pinned, file names validated, expectations from curated JSON; no findings) |
| `Tests/Support/Support.Tests.FlowchartLayout.h` | `Invariants`, `CrossingReduction`, `Corpus`, admission read; `Geometry` (53-351) skimmed |
| `Tests/Support/Support.Tests.PreparedFlowchart.h` | function inventory, `ErRaster`, `StateNoteRaster` read; palette contexts checked; other raster tests not read line by line |
| `Tests/Support/Support.Tests.NativeFlowchartCorpus.h` | structure and generator contract reviewed; ER family vector decoded; hex data rows not individually read |
| `Tests/Support/Support.Tests.SemanticDiagrams.h` | function inventory only |
| `Tests/Support/Support.Tests.MarkdownBlocks.h` (corpus and link-rank sections) | `CheckOriginalCorpus` and lines 870-960 read |
| Callers: `DxUi.RichSemanticDocument.cpp` (1606-1925), `DxUi.RichSemanticGeometry.cpp` (122-150, 780-830, 2160-2200, 2370-2450, 2845, 3262-3425), `DxUi.RichTextEditor.cpp` (3990-4140, 4405-4425, 4605-4625, 4733-4765) | diff not reviewed; touched call paths read to verify fallback, palette, device and target, and accessibility behavior. A suspected stale partial accessible name after alternative overflow was ruled out (`CloseElement` reassigns `name`, `RichSemanticDocument.cpp:880-891`). |
