# Mermaid revalidation — 2026-10-24

This continuation rechecks Mermaid findings against the `rt24` candidate. The historical per-row “pending run” dispositions below are superseded by the parent diagnostics: `combined-14` passes full EditorControls and Embedded, including all pinned outcomes, normalized models, fan-out, shape rasters and expanded allocation sweeps. `combined-19` passes the governed full Accessibility suite. Final-source profile and paired-performance qualification remain separate. No parser, layout, renderer, corpus, or public-API claim here means complete Mermaid compatibility. The admitted surface remains the bounded local subset documented in `Specs/UI/UI_ControlsAndLayout.md`.

The owner selected finishing the current profiles and qualification on 24 October. Additional families and TeX font assemblies remain with the existing rich-text language roadmap. The per-family next-step table below is retained for that owner and is not a promise of new-family implementation in this completion task.

## Finding disposition

“Fixed, pending run” means current code and a named regression now address the finding; it is not a pass claim. “Remains” means the behavior/gap still exists or is an intentional exclusion. “Refuted” means the old claim is no longer supported by the current contract or evidence. “Separate owner” means another implementation area owns the remaining issue.

| Finding | Current disposition and evidence | Current regression / verification |
|---|---|---|
| mermaid-01 | **Fixed and Debug diagnostic passed.** Both state forms use common bounded plain-text/markup validation; the first nonempty description replaces the implicit ID and later descriptions append in source order, including the ID-equal first-description case. | `NativeFlowchartTests::CheckFamilyGrammar` normalized state goldens fail before the correction and pass afterward through `TestNativeFlowchartPinnedIndependentCorpus`; full EditorControls/Embedded pass in `combined-14`. |
| mermaid-02 | **Remains (separate performance follow-up).** `PreparedSemanticGeometry::Begin` recreates native placements on width-only option changes; the coalescing path remains only on an existing `PreparedFlowchart`. This is not a correctness blocker and needs an SDK-call-count/perf measurement before prioritization. | `PreparedFlowchartTests::Coalescing` only proves same-instance intrinsic-option reuse; no semantic-geometry width-reflow call-count assertion found. |
| mermaid-03 | **Refuted as a defect.** The exterior-lane routing behavior is explicitly part of the owning layout contract and its exact extent/path assertions. A different routing policy is a design change, not a correctness fix. | `FlowchartLayoutTests::Geometry`, `Invariants`, and `Corpus` pin lane/path behavior. |
| mermaid-04 | **Fixed, pending run.** The normative UI spec now describes the bounded native preview and separates remaining qualification from implementation state. | Documentation consistency is not checked by a runtime test; manual source review of `Specs/UI/UI_ControlsAndLayout.md` §Native Mermaid profile. |
| mermaid-05 | **Implementation fixed; data validation passed.** The family matrix, generator and README classify state/class as limited profiles and use per-example expectations; state is no longer a special case inside unsupported families. | `Tools/Generate-NativeFlowchartTestData.ps1 -Check` and `Tools/tests/Test-MarkdownData.ps1` passed (8/8). Native `CheckOriginalCorpus` still awaits the current joint test run. |
| mermaid-06 | **Fixed, pending run.** Independent normalized-model grammar cases now cover state/class success, failure, family boundaries and diagnostics rather than count-only smoke checks. | `NativeFlowchartTests::CheckFamilyGrammar`; `FlowchartLayoutTests::Relations`; `PreparedFlowchartTests::Relations`. |
| mermaid-07 | **Fixed, pending run.** State/class paths reject flowchart-only shapes and pipe labels with family-appropriate unsupported issues. | `CheckFamilyGrammar` rows for state diamond syntax, class shape syntax, and state/class pipe labels. |
| mermaid-08 | **Remains.** `NativeFlowchartLayout.cpp::Adjacency` still computes one maximum `laneWidth` for all exterior lanes. | No regression found for per-lane width; existing layout tests continue to pin current geometry, not this optimization. |
| mermaid-09 | **Fixed, pending run.** `FlowchartRelation::Inheritance` is a typed relation; inheritance labels stay empty unless source provides one; layout and drawing carry the kind and draw a hollow head. | `FlowchartLayoutTests::Relations`; `PreparedFlowchartTests::Relations` checks model relation and distinct raster geometry. |
| mermaid-10 | **Fixed in normative prose, pending run.** `docs/rich-text.md` and the UI spec now identify the bounded state/class profiles alongside flowchart. | Manual review only; no prose consistency test found. |
| mermaid-11 | **Partially remains.** Direction matching is whitespace-tolerant and class undirected operator parsing no longer uses the old decrement workaround. `Statement()` remains a multiplexed parser for three dialects; this is maintainability work, not a demonstrated current behavior defect. | `CheckFamilyGrammar` direction whitespace and typed relation rows; no parser-structure assertion. |
| mermaid-12 | **Remains (low priority).** Validation is repeated at parser/layout/prepared boundaries, and pending/completed coalescing order is unchanged. No correctness consequence was established. | `FlowchartLayoutTests::Admission`; `PreparedFlowchartTests::Coalescing` and invalid caller tests. |
| mermaid-13 | **Remains / contract decision needed.** Escape decoding still applies in the bounded label scanner; the spec and scenario table currently define the local behavior. Aligning it to upstream unquoted-text escapes would change the accepted language and needs a deliberate contract decision. | Existing `NativeFlowchartTests` label scenarios; no new unquoted-backslash discriminator was found in the current family/fanout rows. |
| mermaid-14 | **Remains as an explicit subset boundary.** Direction-less headers, renderer aliases and broader identifiers are outside the documented grammar; preserve them as exact source with an unsupported diagnostic if valid-but-excluded classification is desired. | Existing `NativeFlowchartTests` invalid/unsupported header and identifier scenarios; no expansion regression. |
| mermaid-15 | **Remains (refactor-only).** Layout stage handling still multiplexes rank work and inline normalization; no functional or budget defect was reproduced in this review. | `FlowchartLayoutTests::Corpus`, `Geometry`, `Invariants`, and admission/budget cases cover observable determinism, not internal method shape. |

## Completeness ledger revalidation

For IDs without an explicit “fixed” status below, the cited behavior remains out of the local grammar or is a separately owned renderer/integration improvement. Unsupported syntax must remain source-backed and receive an honest diagnostic; these entries do not imply that every upstream-invalid form has a proven diagnostic category.

| ID | Disposition | Current tests / owner |
|---|---|---|
| MM-01 | Remains: subgraphs/nesting are not implemented. | `NativeFlowchartTests::CheckOriginalCorpus`; `CheckFamilyGrammar` exclusions. |
| MM-02 | Remains: inline edge-text grammar is not admitted. | `CheckFamilyGrammar` edge cases. |
| MM-03 | Remains: dotted/thick/invisible link operators are not admitted. | `CheckFamilyGrammar` edge cases. |
| MM-04 | Remains: circle/cross/bidirectional arrowheads are not admitted. | `CheckFamilyGrammar` malformed/unsupported operators. |
| MM-05 | Remains: minimum link length is not modeled; ranking still uses unit edges. | `FlowchartLayoutTests::Geometry`; no min-length oracle. |
| MM-06 | **Fixed, pending run:** flowchart endpoint groups expand in stable Cartesian order, chains continue from the right group, `&` labels are not split, and cap failures publish no partial model. Original example rows 87 and 88 are also admitted from pinned source documentation. | `NativeFlowchartTests::CheckOriginalCorpus` plus `CheckFanout`; `FlowchartLayoutTests::Corpus` plus `Fanout`; normalized `ModelSignature` expectations. |
| MM-07 | **Partially fixed, pending run:** Stadium (`([label])`) is a real node shape; other legacy shape families remain unsupported. | `CheckFamilyGrammar` Stadium row; `FlowchartLayoutTests::Geometry`; `PreparedFlowchartTests::StadiumRaster`. |
| MM-08 | Remains: expanded `@{ shape: ... }`, icon and image shapes are not admitted. | `CheckFamilyGrammar`; exact-fence fallback. |
| MM-09 | Remains: HTML/inline markup remains rejected rather than rendered. | `CheckFamilyGrammar` label rejection rows. |
| MM-10 | **Fixed, pending run:** ordinary literal ampersands are allowed while entity/markup spellings are rejected; do not conflate all `&` with markup. | `CheckFamilyGrammar` literal `&` and entity rows; fanout test with `&` label. |
| MM-11 | Remains: entity decoding is not implemented; entity-looking text remains unsupported. | `CheckFamilyGrammar` entity diagnostic rows. |
| MM-12 | Remains: Markdown label styling and multiline quoted labels are not admitted. | `CheckFamilyGrammar`; exact-fence fallback. |
| MM-13 | Remains: icon prefixes are rejected; icon fonts/packs are not part of this renderer. | `CheckFamilyGrammar` markup/unsupported labels. |
| MM-14 | Remains: identifier grammar is intentionally narrow. | `CheckFamilyGrammar` identifier outcomes; exact source preserved. |
| MM-15 | Remains: no directionless/default or renderer-alias header expansion. | `CheckFamilyGrammar` header outcomes. |
| MM-16 | Remains: front matter and executable/init directives are outside the grammar. | `CheckFamilyGrammar` unsupported statements. |
| MM-17 | Remains: accessibility metadata statements are not implemented as metadata. | Family grammar rejection cases; no positive metadata contract. |
| MM-18 | Remains: style/classDef/class/linkStyle grammar is unsupported. | `CheckFamilyGrammar` style and class statements. |
| MM-19 | Remains: callbacks, hyperlinks and tooltip interaction are excluded. | `CheckFamilyGrammar` click/link exclusions. |
| MM-20 | **Revalidated as a required guard; current parser rejects the identified ellipse/property-vertex openers rather than treating them as ordinary labels.** | `CheckFamilyGrammar` shape-operator cases; pending joint run. |
| MM-21 | Remains by design: distinct exterior lanes and per-edge rank corridors are contract behavior. | `FlowchartLayoutTests::Geometry` and route invariants. |
| MM-22 | Remains: ordering/crossing reduction and long-edge dummy nodes are not implemented. | `FlowchartLayoutTests::Geometry`; no crossing-quality oracle. |
| MM-23 | **Separate owner / partially addressed in integration.** Native diagram rejection now has a source-backed literal-object fallback path, but broad whole-document geometry/capacity behavior must be revalidated by parent integration tests. | Parent-owned `NativeObjectFailureContainment` tests and `Controls.Tests.RichSemanticGeometry.cpp`; pending joint run. |
| MM-24 | Remains: node label wrapping/max width is not an explicit native profile feature. | `PreparedFlowchartTests` label geometry; no wrapping oracle. |
| MM-25 | Remains: semantic geometry width-only reflow rebuild behavior needs an SDK-call-count performance test. | `PreparedFlowchartTests::Coalescing` does not cover `PreparedSemanticGeometry`. |
| MM-26 | Remains: paragraph alignment/typographic fidelity differs from Mermaid’s centered/mixed-style labels. | `PreparedFlowchartTests::Labels`; separately scoped rendering fidelity. |
| MM-27 | **Partially fixed, pending run:** diagnostics distinguish parser/family/shape/markup/capacity outcomes and retain source ranges; no exhaustive upstream diagnostic parity claimed. | `NativeFlowchartTests::CheckFamilyGrammar`, `CheckOriginalCorpus`, `CheckFanout`. |
| MM-28 | Remains: state terminal and class member alternative-text wording is not specialized. | Parent-owned UIA tests do not yet characterize terminal alternative text; accessibility owner. |
| MM-29 | Remains: state composites/nesting are unsupported. | `CheckFamilyGrammar` unsupported state constructs. |
| MM-30 | Remains: choice/fork/join pseudo-states are unsupported. | `CheckFamilyGrammar` unsupported state constructs. |
| MM-31 | Remains: state notes are unsupported. | `CheckFamilyGrammar` unsupported statements. |
| MM-32 | Remains: state concurrency regions are unsupported. | `CheckFamilyGrammar` unsupported state constructs. |
| MM-33 | **Revalidated as family isolation:** state/class `:::` syntax is not consumed as a flowchart label; styling itself remains unsupported. | `CheckFamilyGrammar` family-boundary rows; pending joint run. |
| MM-34 | Remains: accTitle/accDescr are not consumed as metadata; keep unsupported until implemented. | `CheckFamilyGrammar` reserved/unsupported statements. |
| MM-35 | **Remains open.** Current parser stores colon description as `S\nReady` while quoted alias stores `Ready`; pinned Mermaid state parsing removes the fallback ID on the first description and appends later descriptions. Class member descriptions remain a separate ID-plus-description rule. | `CheckFamilyGrammar` currently captures validation but its equivalence expectation is incorrect; parent is adding a state-specific expected-model test before the bounded parser correction. |
| MM-36 | Remains: state terminal markers use the current bounded circle representation; Mermaid-specific start/end marker geometry/global uniqueness is not claimed. | `FlowchartLayoutTests::Geometry`; `PreparedFlowchartTests::Relations` does not establish Mermaid terminal fidelity. |
| MM-37 | **Fixed, pending run:** family grammar isolation rejects flowchart-only shape and pipe-label syntax in state/class. | `CheckFamilyGrammar` state/class shape and edge rows. |
| MM-38 | Remains: class member blocks/compartments are unsupported. | `CheckFamilyGrammar` class blocks and notes. |
| MM-39 | Remains: only class association, directed association and inheritance are typed; composition/aggregation/dependency/realization/lollipop and dashed relations remain unsupported. | `CheckFamilyGrammar` class operator matrix; `FlowchartLayoutTests::Relations` for admitted relation kinds. |
| MM-40 | Remains: multiplicity labels are unsupported. | `CheckFamilyGrammar` unsupported class operators/labels. |
| MM-41 | Remains: class annotations are unsupported. | `CheckFamilyGrammar` unsupported class statements. |
| MM-42 | Remains: class notes are unsupported. | `CheckFamilyGrammar` note row expects `UnsupportedStatement`. |
| MM-43 | Remains: class namespaces/clusters are unsupported. | `CheckFamilyGrammar` unsupported class statements. |
| MM-44 | Remains: class compartments, generic/member styling and return-type formatting are unsupported; current single-label subset rejects `~` generics. | `CheckFamilyGrammar` class member rows. |
| MM-45 | Remains: `classDiagram-v2` alias is not admitted. | `CheckFamilyGrammar` header rows. |
| MM-46 | Remains: class identifiers use the shared bounded identifier subset; alternate quoted/generic class names are not admitted. | `CheckFamilyGrammar` class identifier rows. |
| MM-47 | Remains: class styling/interactions and compartment-specific capacities are not implemented. | `CheckFamilyGrammar` unsupported class statements; current global label capacity test. |
| MM-48 | **Fixed, pending run:** typed inheritance has no synthetic `inherits` label and uses a hollow triangular head. | `FlowchartLayoutTests::Relations`; `PreparedFlowchartTests::Relations` raster oracle. |
| MM-49 | Remains: sequence diagrams are outside the local family set. | `NativeFlowchartTests::CheckOriginalCorpus` family matrix. |
| MM-50 | Remains: ER diagrams are outside the local family set. | Family-matrix per-example expectations; generator checks pending. |
| MM-51 | Remains: pie diagrams are outside the local family set. | Family-matrix per-example expectations; generator checks pending. |
| MM-52 | Remains: Gantt is outside the local family set. | Family-matrix per-example expectations; generator checks pending. |
| MM-53 | Remains: mindmap is outside the local family set. | Family-matrix per-example expectations; generator checks pending. |
| MM-54 | Remains: gitGraph, timeline and userJourney remain unsupported. | Family matrix per-example outcomes. |
| MM-55 | Remains: other listed families remain unsupported; Mermaid plugin/runtime families remain outside native scope. | Family matrix per-example outcomes; no “full Mermaid” claim. |
| MM-56 | **Partially fixed, pending run.** The typed local model/prepared pipeline covers the three limited profiles; family dispatch remains centralized in `Statement()`, not a family-neutral parser architecture. | `NativeFlowchartTests::CheckFamilyGrammar`; no architecture-specific test. |
| MM-57 | Remains: one parser statement function still dispatches multiple local grammars. | `CheckFamilyGrammar` gives behavioral coverage; structural refactor deferred. |
| MM-58 | **Fixed, pending run.** Matrix/generator classify limited state/class separately from unsupported families. | `NativeFlowchartTests::CheckOriginalCorpus`; generator `-Check`; `Test-MarkdownData.ps1`. |
| MM-59 | **Fixed in current prose, pending run.** README and UI spec describe the local subset and current original-corpus count. | Manual review of `ThirdParty/Markdown/README.md` and `Specs/UI/UI_ControlsAndLayout.md`; no prose validator. |
| MM-60 | **Partially fixed, pending run.** Flowchart row diagnostics are checked against issues; state/class has an independent normalized model expectation table. Upstream Jison source pins beyond current local grammars remain unnecessary for claims of full conformance and are deferred. | `CheckOriginalCorpus`, `CheckFamilyGrammar`, `CheckFanout`; generator/test-data checks pending. |
| MM-61 | **Fixed, pending run.** Directed accepted/rejected state/class scenarios now exercise normalized model signatures and issue categories. | `NativeFlowchartTests::CheckFamilyGrammar`. |
| MM-62 | **Fixed, pending run.** Direction parsing skips horizontal whitespace and checks empty/trailing input safely. | `CheckFamilyGrammar` rows for multiple spaces and tabs. |

## Remaining-family priority and bounded next steps

Effort is a relative implementation estimate, not a schedule. “Candidate” means a useful local slice may be scoped after the active qualification run; it does not imply immediate implementation.

| Priority | Family / bounded slice | Relative effort | Required independent tests before admission |
|---|---|---|---|
| P1 candidate | Pie: quoted label/value pairs, fixed slice cap, deterministic arcs and legend; reject directives and unsupported options. | Small–medium | Normalized value/percentage golden; malformed/duplicate/zero/negative values; capped slices; independent arc bounds and raster oracle; exact-source rejection. |
| P2 candidate | Mindmap: indentation-only tree subset and a small explicit set of node delimiters, no icons/classes/Markdown. | Medium | Golden parent/child tree and labels; indentation ambiguity/rejection; depth/node caps; stable layout and finite geometry; exact-source fallback. |
| P3 candidate | Gantt: fixed date format, title/section/task with explicit start or duration, no excludes or locale-dependent parsing. | Large | Independent date/order model; leap-day and invalid-date cases; duration/dependency bounds; timezone independence; axis/layout and source fallback. |
| Defer | ER diagram: entities, attributes and cardinality edges require table nodes plus more relation glyphs. | Large | Full entity/attribute/cardinality model, relationship direction, table geometry/raster, resource limits and malformed statements. |
| Defer | Sequence, class compartments, state composites/notes, subgraphs and style directives. | Large | Each requires a distinct structural model (lifelines/messages, compartments, nested scopes, or style cascade); do not emulate with flat nodes. |
| Keep unsupported | GitGraph, timeline, userJourney, quadrant/XY, sankey, packet, block, kanban, architecture, requirement, C4 and ZenUML. | Medium–large / plugin-dependent | Prefer truthful per-family diagnostics and explicit corpus outcomes; ZenUML requires an external runtime and is out of native scope. |

Before any new family admission, update the pinned reference expectations only when an independently authored model and geometry oracle exist; keep the upstream source corpus bytes and notices immutable. The current derived expectation file admits 16 of the 105 original flowchart examples and rejects 89; original examples 80 and 81 exercise endpoint groups. Their model goldens expect 4 nodes and 4 edges in source-stable order, with LeftRight and TopBottom respectively. This correction followed an initial misidentification of rows 87/88 as the fan-out examples; those rows were restored to their original independent expectations before regeneration. Treat the first attempt as a test-data authoring failure, not parser evidence.

## Files fully read for this revalidation

The following current candidate implementation and test files were read in full; line references in prior audit entries are historical and should not be reused without refresh. Four review notes per file are recorded to support the parent’s ledger update.

| File | Four review notes / test IDs |
|---|---|
| `src/Controls/DxUi.NativeFlowchart.cpp` | `CheckOriginalCorpus` parsing preserves per-example outcome; `CheckFamilyGrammar` covers aliases/descriptions and family-isolated syntax; `CheckFanout` covers group parsing, source ranges and atomic limits; parser caps remain 16,384 source units / 256 nodes / 512 edges / 1,024 statements. |
| `src/Controls/DxUi.NativeFlowchart.h` | Model carries family, direction, shapes and typed relation; diagnostic has issue/outcome/source interval; node/edge vectors publish only after parser outcome; constants bound source/labels/nodes/edges/statements. |
| `src/Controls/DxUi.NativeFlowchartLayout.cpp` | `FlowchartLayoutTests::Corpus` lays out every admitted original graph; `Geometry` checks route and extents; `Fanout` checks finite paths and ports; Stadium sizing/ports branch on true shape, while global lane width remains. |
| `src/Controls/DxUi.NativeFlowchartLayout.h` | Layout accepts only a prepared bounded graph; plan has bounded node/edge placements; relation and shape flow through placed geometry; maximum extent and work-step API are explicit. |
| `src/Controls/DxUi.PreparedFlowchart.cpp` | `PreparedFlowchartTests::Relations` exercises inheritance paint; `StadiumRaster` checks curved capsule output; `Labels` exercises text measurement; incremental preparation publishes only after layout/paths complete. |
| `src/Controls/DxUi.PreparedFlowchart.h` | Prepared renderer validates model limits and resource epoch; exposes immutable geometry/readiness; draw separates relation arrow and node shape paths; drawing remains native-only. |
| `Tests/Support/Support.Tests.MarkdownBlocks.h` | `CheckOriginalCorpus` tests original 105 outcomes and current 16/89 split, with case numbers printed on mismatches; `CheckFamilyGrammar` uses independent normalized signatures and issue expectations; `CheckFanout` tests stable Cartesian expansion/source ranges/caps and full original model signatures for cases 80/81; `CheckFencedSource` checks exact source preservation on rejection. |
| `Tests/Support/Support.Tests.FlowchartLayout.h` | `Corpus` covers admitted reference graphs; `Geometry` pins rank/routes/typed relations; `Fanout` checks finite bounded geometry and edge ports; `Invariants` checks extents, endpoint boundaries and resource bounds. |
| `Tests/Support/Support.Tests.PreparedFlowchart.h` | `Coalescing` only tests one prepared instance; `Relations` checks hollow inheritance and directed association raster difference; `StadiumRaster` checks nonrectangular capsule raster; `Labels` checks native label geometry and bidi/multiline behavior. |

The file review itself is source-only. Parent-owned combined build/runtime, generator checks and data-validation results above supply the separate diagnostic results; final-profile qualification still requires its own receipts.

