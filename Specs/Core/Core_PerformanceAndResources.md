# Performance and resources

Allocation failure and retry below mean explicit checked nonthrowing refusal under
[Failure handling and allocation](Core_FailureHandling.md), not an escaped C++ exception. Unexpected bad_alloc
and callback exceptions require host fatal diagnostics, implemented by API 5. Its final qualification remains
tracked in the failure migration plan; historical throwing-fault recovery sweeps and resource measurements
alone do not qualify it.

Semantic script/color admission performs at most one native capability query per new script-bearing geometry
preparation with a supplied target. A preparation-local scalar survives staging-raster retirement. Compatible
reuse and documents without scripts make no such query. Diagnostic counts cover these admission calls, not
interface acquisition by the native glyph renderer. The temporary query interface is released immediately;
published target binding keeps its existing ownership. Clean/hidden frames and composition add no capability
work, allocation, layout or periodic wake. Native resource and pixel oracles remain separate from elapsed-time
or paired-performance acceptance.

Each linked native module retains at most one animation dispatcher HWND per UI thread. Attached hosts share
that window, including idle intervals between gestures; unsubscribing a single host must not churn a sibling's
cache. The last attachment releases an idle window synchronously, or at the end of its active timer callback.
This uses the existing attachment count and adds no allocation, worker, polling or timer to the idle/embedded
path. The public consumer fixture observes creation, sibling preservation, warm idle reuse and final HWND
retirement independently; resource counts do not establish paired elapsed-time acceptance.

Editor cosmetics use the existing owner-thread optional visual traversal, with
no worker or periodic timer. Content readiness excludes code/source ink, source
guides and Source-mode analysis used for colouring. Guide retention and copying
are bounded by the visible source window plus overscan; each turn spends at most
256 source/number steps and four number layouts. Source coordinate lookup uses
immutable piece-tree summaries and per-buffer newline indexes; ordinary typed
fragments without line breaks allocate no newline vector. Snapshots share these
indexes and edits update only replacement buffers and tree paths. Code ink queries
the semantic paint index and calls the consumer only for visible/overscan blocks,
at most once per turn. Publications swap atomically; completed clean windows do
not schedule another visual request. Host cancellation releases publications.
Composition borrows prepared spans, labels and coordinates without heap work,
parsing or measurement. Whole-document source-analysis admission and bounded
cooperative syntax work remain separately accounted; logical work bounds do not
establish elapsed-time or paired-performance acceptance.

Embedded input wake deferral uses an intrusive UI-thread stack with borrowed hosts and one pending bit per
attachment. Entry, nesting and completion add no heap allocation, worker, timer or retained queue. Scope exit
visits current synchronous nesting only; detached requests disappear with their attachment. Direct mutations
on another host remain independent. Composition and clean/hidden preparation retain their existing contracts.

Selection-to-block insertion scans bounded source/selection text and stages one replacement. It preserves
existing counted UTF-16 and EOL spellings, and refuses a result beyond document capacity before publication.
It performs no layout, device work or clipboard access. Its staged data is retired when the one source
transaction completes or declines; this functional bound is not paired-performance acceptance.

Arrow-label background contrast is resolved once for each changed effective foreground/surface or resource
binding, outside composition. A retained target-bound brush is reused on unchanged frames. Two bounded
16-step neutral searches handle insufficient contrast; group/note surface membership is cached with
prepared labels. Composition selects borrowed brushes without contrast scans, layout, allocation or new
device resources. This functional/resource contract does not establish paired performance acceptance.
Layout retirement releases the target-bound label brush alongside the other semantic brushes. Its pure
color result remains reusable while foreground and surface stay unchanged; resource or host replacement
reacquires the brush without retaining the old target resource.

Mermaid label-style clipping and shared model validation run during bounded preparation and allocate no extra
label storage. Model validation is an indivisible bounded check, not a wall-clock guarantee. Relationship layouts
and compact fan-in/fan-out routing may stage one bounded endpoint-slot state (256 nodes, two sides and 512
edges). Each endpoint may scan up to 512 edges; this is bounded preparation, not a linear-work or wall-clock
guarantee. The temporary state retires with preparation and is never retained by the published plan. Diagrams
that need neither relationship slots nor compact fan routing avoid that allocation. Slot admission and row
sizing remain bounded by existing node/edge/extent limits. Clean/hidden
frames, warm drawing and code-chrome foreground selection add no workers, timers, allocation or measurement.

Bulk Grid and Tree selection builds a staged visible-order list with membership in an ascending copy of the requested
IDs behind a table of bits (`SelectedIdFilter`, one load rules out most visible IDs) and a fixed-width radix-sorted
membership copy. Its work is linear in visible IDs plus a sort of the requested IDs, including reverse-ordered IDs;
allocation failure preserves the previously published selection. Repeated IDs occur only once in first-visible order;
the final requested visible ID remains focus and anchor. No heap node is allocated per ID.
Tree pointer hit testing computes the expander lane without shaping badge text or changing the badge measurement cache;
paint and input share the same lane geometry, including reserved-space and right-to-left layouts.
Repeated hover on the same unchanged Tree row reuses its tooltip geometry; explicit tooltip text bypasses badge and
overflow measurement. Model presentation, host, typography, resource or interaction changes invalidate that reuse.
Icon-only button preparation is reached only from the host's preparation pass and checks at entry that it is attached
to that host (no tree-membership search). Later reentrant boundaries check the owner and root
lifetimes, host, root identity and interaction revision before accessing or publishing control state.
Button shortcut width is prepared and cached by host, typography, resources and reading direction; painting uses the
prepared scalar and performs no shortcut measurement. A text change invalidates it, and detachment retires its host key.

Semantic spacing overrides are prepared geometry policy. Both absent retain
legacy placement and allocate no new spacing metadata. An opted-in list minimum
may stage one nearest-ListItem index per bounded syntax node in the existing
parent-first metadata pass; it is retired with staging, not queried by walking
ancestors during input or composition. A paragraph minimum in native body lines
uses at most one additional native empty-body layout, counted against the SDK
layout budget; an explicit body line-height multiplier needs no such layout.
Only scalar margin/blank-credit state changes placement. Cancellation and every
allocation/SDK failure retain the previous complete source/geometry publication.
Warm queries, clean/hidden frames and composition add no spacing allocations,
layout calls, timers or workers. Tests cover both override combinations, exact
empty-site navigation, native-height allocation failure and unchanged defaults.
Only an opted-in non-page Up/Down hit which stays on the same visual line uses
the adjacent-line fallback. It reads bounded prepared box/SDK line spans and
the existing source hit/caret mapping, preserving preferred x and site affinity
across large gaps without allocation or layout. Ordinary wrapped movement and
the default fixed-spacing fallback remain unchanged.

TOML front matter validation stages bounded UTF-8 converted from the admitted UTF-16 source and parses it without reserialization. Its 128-level value and dotted-key nesting ceilings and 1,024 total key-segment ceiling are reported as capacity, not invalid TOML. Parsing one bounded content envelope is indivisible; the prior syntax remains published while replacement stages. Unexpected C++ allocation failure is fatal through the host. Input queries, composition painting and idle frames do not parse. The native resource and timing gates still require measured qualification.

YAML front matter event parsing and key/alias validation stage owned temporary
buffers only. Admission limits are 128 tag directives, 4,096 graph nodes, 128
graph levels, 256 digits per numeric key, 100,000 canonical graph visits,
8 MiB cumulative canonical bytes, a 512 KiB event arena and 4 MiB parser callback
allocations. Exhaustion is a capacity diagnostic, not an invalid syntax report.
The event buffer itself is capped at 262,144 words. Validation is an indivisible
bounded preparation operation; native timing and resource qualification remains open.

The lossless Markdown analysis preview retains bounded source (65,536 UTF-16
units), one staged result and one published result per parser. Advance has an
operation budget; source validation/copy/comparison and allocator work are not
hard wall-clock bounds. Block transitions inspect one bounded physical line;
reference finalization, block-tree compaction, construct lookahead, emphasis resolution and sorting are
indivisible bounded work. An incremental revision (plan item W7.2, rules in [controls and
layout](../UI/UI_ControlsAndLayout.md)) scans block lines, lexemes and inline leaves only from the enclosing top-level
block to the next stable cut, imports unchanged top-level subtrees and their inline forests, and validates front matter
only when its envelope may have changed. Imports are copies with shifted absolute ranges, so node, inline, lexeme,
root and identity passes still visit every published record once per revision, and the owned source is one complete
copy (prefix and suffix from the previous publication, the edit window from the piece tree). Work counters
`rich.markdownBlockUnitsScanned`, `rich.markdownLexicalUnitsScanned`, `rich.markdownInlineUnitsScanned`,
`rich.markdownFrontMatterValidations`, `rich.markdownRootTriviaUnitsScanned` (trivia lines of the root partition;
unchanged ones are copied), `rich.markdownReferenceUnitsRead` and their copy counterparts record it; on `FX8` each of
these source reads of a one-character insert stays within the enclosing block plus the next. Link reference
definitions are the remaining whole-document source read: a top-level block holding one is scanned, and every
definition is collected, on every edit. Lexical reference recognition examines at most
33 source units per candidate and uses a fixed sorted standard-data table.
Native tests bound scanner/root/index/reconciliation steps by
16 * (current source units + previous source units) + 32. M3 adds global-reference,
inline/autolink and dense-forest marking/index/copy passes to the earlier M2
eight-step fixture bound. This counts steps,
not all character visits inside block transitions. Prefix inspection fixtures
independently check 128/512/4096 nested list levels
and a 2048-level quote with 512 lazy lines. Required indentation lookahead is
capped, thematic suffix rejection uses one line metadata pass, and containers
finalize once rather than extending every ancestor per lazy line. BlockStatistics
exposes those deterministic counts; it is not a total CPU/time counter.
Autolink preparation separately indexes host/email suffix facts and resumes one
region edge or source candidate per parser operation. Failed overlapping host
and email candidates do not rescan their complete suffix. Successful splices
decode only final text prefixes and the final remaining suffix, once each.
`InlineStatistics()` exposes indexing/candidate visits and decoded suffix units;
these counters do not include allocator work or claim a wall-clock deadline.
The index is bounded by the admitted source length and is retired with staging.
Inline raw-HTML terminator lookup MUST reject a missing or already-passed final
terminator without scanning the remaining leaf again. Four leaf-local final
offsets suffice for `-->`, `?>`, `]]>` and `>`; collect them in the existing
backtick indexing pass without a per-unit HTML index in ordinary profiles.
Successful HTML searches consume their complete emitted node. Non-angle inline
link destinations admit at most 32 balanced parenthesis levels; exceeding that
limit retains literal source. Table recognition MUST check pipe presence, cell
counts and delimiter alignment before flattening a reference-like paragraph.
Reference extraction runs only for a viable table or normal paragraph closure.
Private opt-in owner-thread work probes count searched source spans, destination
loop iterations and raw reference-flatten spans, rather than CPU loads or time.
Required hostile doubling fixtures stay within 2.5 times their prior logical work
and retain independently specified literal/block results. These counters add no
public ABI fields and do not qualify timing, allocator work or paired performance.
The maximum
all-newline source exercises maximum root-trivia and lexical partition counts; owned
C++ retained storage must stay within 24 MiB and release completely. Replacing
that maximum result while retaining its previous publication is bounded to
32 MiB peak C++ storage. This is not
a process/SDK/GPU or large-document capacity claim. Same completed requests,
read-only publication access and retirement allocate no C++ storage. Parser
checked-allocation-failure sweeps must retain the previous publication and allow explicit
retry under the owning status contract. No parser result changes the existing frame performance HOLD.
MSVC Debug iterator proxies allocate even in default/noexcept STL container constructors. The checked/fatal
allocation closure must cover staging, node values, appearance font overrides, default arguments and nested
proxy/control-block construction. The published explicit zero-size construction/copy pattern was designed to
permit throwing failure instead of accidental noexcept termination; it is legacy behavior, not the new checked
failure contract. A public aggregate's exception specification alone cannot satisfy the allocation/diagnostic
policy. Fault coverage must reach each owned override, not only the first one.
Mermaid sequence accessibility-label staging follows the same checked/fatal allocation rule for empty derived labels. A genuine checked refusal preserves the published/pending semantic generation and permits its documented retry; unexpected C++ allocation failures are fatal. Numbered messages, notes and compact endpoint-routing staging require their corresponding isolated fatal and checked native-result coverage.
The allocation sweep must cover admission, growth, reference extraction, nested
inline values/links, tables/tasks/tagfilter and compaction. Literal text coalesces
into runs rather than retaining one string per scalar. An 8,193-unit emphasis
fixture publishes 2,049 nodes iteratively; a maximum plain paragraph has one Text
value. The expanded mixed-feature fault fixture has an independently counted
schedule below 2,048 requests (the M2 fixture's 512 ceiling covered fewer stages
and constructs). Every observed request is still injected individually.
Diagnostic children may emit bounded raw stack addresses on unexpected
termination; ordinary suites and canonical benchmarks keep their normal path.

Semantic source-format indexing is prepared once with the immutable semantic
document, sharing its 65,536-unit admission. UIA collection queries do not parse,
create layouts or mutate the document. Every observed C++ allocation in semantic
child/header collection construction must either use a checked nonthrowing refusal returning E_OUTOFMEMORY
with its documented retry/state guarantee, or enter the host fatal path. A caught bad_alloc is not a checked
refusal. Child providers hold weak semantic graph identity; optional rich snapshot
payloads keep bounds/font strings out of unrelated control records. Preparing
snapshot rectangles is separate from allocation-free composition and from the
existing warm caret/visible-range query budget.

Semantic coordinate preparation admits at most 65,536 source units, 131,072
display units and 65,536 display spans. Primary source intervals are disjoint;
building their caret indexes visits each owned source unit once plus shared
edges, rather than once per repeated display occurrence. Sorting primary indices
and resolving repeat ownership are bounded preparation work, not composition.
Coordinate/atomic-boundary queries allocate nothing. Fragment queries return
owned vectors and may allocate; they belong to preparation/input/accessibility
work, never Composite. Range returns use explicit fallible copies/prvalue elision
so MSVC Debug iterator-proxy allocation cannot terminate in a noexcept move.
The simultaneous maximum source/display/span fixture must retain at most eight
MiB of C++ storage and release it completely. This is a map-only bound, excluding
parser, geometry, histories, driver and GPU storage. Observe and inject every
allocation in map creation, both fragment directions and atomic source edit
staging; failures must leave held maps and complete Document state unchanged.
The source transaction materializes its enclosing changed interval once and
preserves all untouched gaps. It introduces no worker, timer or idle polling.

SemanticBuilder additionally bounds the combined syntax graph to 32,768 nodes and
geometry paragraphs to 4,096 before native SDK work. It traverses block/inline
events cooperatively. Normalization, one value's tokenization, one image alternative
and final map/index construction remain indivisible bounded preparation work.
Ordered markers use one sibling pass, without repeated prefix walks. Every
resolved value must match its normalized source tokens before publication; a
mismatch rejects staged work. Failure sweeps cover complete preparation and
replacing a pending request. Coalesced requests, publication reads and retirement
allocate nothing. The maximum plain-source fixture must retain at most eight MiB
of additional tracked C++ storage and release it completely, with its syntax held
outside the accounting window. This is a named fixture bound, not an all-input,
parser, process, SDK or GPU bound. General pathological capacity remains M6.

Status: normative current contract
Last reviewed: 2026-10-21

The migrated paired acceptance policy is under review and is not yet qualified. Until a qualified versioned policy exists in
the measured base, paired reports are `policy-review-required`; neither the candidate's policy nor the candidate's judge
can authorize its own pass. Historical receipts remain readable under their original judge and are not migrated or rewritten.

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

## Work counters

Status: implemented (performance and motion plan item W0.1: counters 13 November 2026, counting wrappers and draw calls
14 November 2026). Private; nothing here is public API.

Deterministic work is counted by `Detail::WorkCounters` (`src/Support/WorkCounters.h`): one thread-local record of
plain `uint64_t` increments, present in every configuration, Release included. Counting is dormant until a test reads
the record; its cost is an increment, with no allocation, lock, I/O, worker or timer, so composition and clean or hidden
frames keep their contracts. A reading is per thread: work another thread does is in that thread's record. Counters only
grow; a test measures an action as the difference of two readings (`WorkCounterScope::Delta`). Counters are grouped by
subsystem, each group a list kept in alphabetical order: frame, wakes, resources, text, popups, lists, rich,
accessibility, layout and memory. A new counter is one line in its group's list and is then present in every
snapshot, delta and receipt.

- **Frame and walks.** `paintExecutions` counts each control whose `Paint` a host, `Panel` or `PageHost` dispatched;
  `treeVisitsPaint`, `treeVisitsTick`, `treeVisitsHitTest` and `treeVisitsStatistics` (the retained-resource walk of
  `EmbeddedHost::GetStatistics`) count each control such a walk examined, visible or not. `hitTests` counts host hit
  tests that walked the tree and `hitTestReuses` those answered from the pointer event's hit result
  ([pointer hit result](../UI/UI_InputAndAccessibility.md#pointer-hit-result)). `preparationMembershipSearches` counts
  `IsControlInTree` searches started during a host preparation pass and `plainLabelPreparations` the visual preparations
  of a `Label` with neither a mnemonic nor a typography override; both are 0 since W2.9.
  `preparations` and `composites` count embedded preparations that painted and compositions; `renders` and `presents`
  count native render passes and swap-chain presents; `pixelsPainted` adds the device pixels a preparation or render
  cleared and repainted (the dirty rectangle of a partial native frame, the area of an embedded preparation's paint
  region). `paintRegionRects` counts the region rectangles embedded preparations repainted alone and `paintRegionFull`
  the embedded preparations that repainted the whole view; `paintCulled` counts visible children a panel did not paint
  because their painted extent missed the paint clip, `clipPushesSkipped` the viewport clips a `ScrollPanel` did not push
  because the paint clip already lay inside its viewport, and `inkEscapes` the controls whose paint first drew text
  beyond their bounds plus reach ([paint region](#paint-region)). `drawCalls` counts Direct2D draw calls (every
  `Draw…`, `Fill…`, `Clear` and `BlendImage` call DxUi makes on a device context). Only the counting wrappers below
  increment it, so it is populated only where a test executable installs them and is 0 everywhere else, shipping builds
  included. `shadowPasses` counts shadow effect passes drawn for elevated surfaces (popups, cards, dialogs).
  `fullVisualPreparations` counts preparation passes that prepared every visible control's visual content rather than
  the pending ones (a resource or typography generation change); `brushEvictions` counts keyed solid brushes a host
  released above its bound and `brushColorReuses` each transition brush served again by `SetColor` instead of created
  ([format and brush caches](#format-and-brush-caches)). A thread's
  shared native device stack counts `d2dDevices` and `compositionDevices` it creates, and native hosts count the
  `swapChains` they create; with `resources.d3d11Device` and `resources.deviceContext` these give the device creations of
  one menu session (`FX4`, `G10`).
- **Frame schedule** ([window hosting](../Rendering/Rendering_Win32Host.md#frame-schedule)). `tickRequesters` counts the
  due requests a host found at its wakes and `tickCalls` the `Tick` calls it made for them (they differ only by requests
  dropped for a destroyed, departed or hidden control); `tickLegacyTreeWalks` counts whole-tree ticks an anonymous
  `RequestAnimation()` asked for, which the library's own controls never make, and `tickDiscoveryWalks` those an embedded
  root installation asked for; `tickHeldWhileHidden` counts requests that came due while their native host was hidden
  and fired once on its show; `tooltipTargetValidations` counts the wakes that resolved a tooltip's target again (a
  hit-test walk and a title lookup, not a `Tick`).
- **Frame pacing** ([window hosting](../Rendering/Rendering_Win32Host.md#frame-pacing)). `pacedFrames` counts native paced
  frames that sampled the compositor clock and ran the schedule; `presentsWithoutCredit` counts presents made without a
  frame-latency credit (0 on every path); `pacerDeferredPaints` counts `WM_PAINT` deferred to the next credit;
  `pacerExcessUnits` units absorbed because a credit was kept for a present that returned a status other than `S_OK` whose
  frame had queued after all; `presentStatuses` presents that returned a success status other than `S_OK` (the credit is
  kept);
  `pacerStaleFrames` paced-frame messages ignored because their request was cancelled or belongs to an earlier chain or
  attachment; `pacerWaitTimeouts` waits that saw no retired frame within their timeout (a stalled compositor);
  `pacerSynchronousTimeouts` synchronous renders that found no credit within their bound and did not present;
  `pacerStatsFallbacks` frames sampled at the current time for want of compositor statistics; `pacerTargetsOutOfRange`
  frames sampled at the current time because the compositor's target lay more than three periods ahead (or its period was
  implausible); `pacerInvariantViolations` (Debug) pacer transitions that left a deferred paint with no wake (0 on every
  path); `pacerUnavailable` chains
  without a waitable object; `pacerTeardownOrderViolations` chains released while their pacer still held its wait or
  handle (0 on every path).
- **Wakes by source.** `animationTimer` (the animation dispatcher's periodic `WM_TIMER`), `deadlineTimer` (its one-shot
  wake for the earliest deadline), `menuHoverTimer`, `paintMessage`
  (`WM_PAINT`), `pacedFrame` (a paced-frame message from the pacer's thread-pool wait, stale ones included), `pacerTimer`
  (the paced skip timer) and `embeddedAdvance` (each `EmbeddedHost::AdvanceAnimation` the application made while animation
  was requested).
- **Text.** `setTextRenderingParams` counts each `SetTextRenderingParams` call a host makes to bind or clear rendering
  parameters ([theme and typography](../UI/UI_ThemeAndTypography.md#text-antialiasing-and-rendering-parameters)).
- **Resources.** Library code creates text layouts and formats, DirectWrite rendering parameters, geometries, stroke styles, brushes, gradient stop
  collections, bitmaps, effects, command lists, layers, Direct2D device contexts and D3D11 devices only through the
  counted helpers of `src/Support/CountedCreation.h` (available through `DxUi.Internal.h`), and draws `DrawText` only
  through `CountedDrawText`. Each helper forwards to the method unchanged and counts the object it returns; a refused
  creation is not counted. D3D11 devices are counted where `Detail::CreateD3D11DeviceWithWarpFallback` receives one from
  its creator. `DrawText` lays out its whole string on every call, so each call counts one implicit text layout created
  and destroyed. GDI backdrop captures count `popups.backdropCaptures` and their pixels, and each blur drawn over such a
  capture counts `popups.backdropBlurPasses`; each filter of an editable `ComboBox`'s popup items counts
  `popups.comboBoxPopupRebuilds`, each popup text width a `ComboBox` measures counts `popups.comboBoxItemWidthMeasurements`
  (one per item when its retained widths are stale, the no-matches text once) and each popup placement it computes because
  an input changed counts `popups.comboBoxPopupLayouts`. Each `MenuPopup` content paint counts `popups.menuPopupPaints` and adds the rows it drew
  to `popups.menuRowsPainted`.
- **Text.** `text.formatEvictions` counts the configured text formats `ControlHost::TrimCaches` evicted above the key
  bound, `text.formatCacheTrims` each trim that evicted one, and `text.formatCacheOverBound` each trim that left the
  map above its bound because every remaining entry was in use. No eviction prepares any control again
  ([format and brush caches](#format-and-brush-caches)). The host's [retained text layouts](#retained-text-layouts) count each string drawn from a
  retained layout (`text.textLayoutCacheHits`), each layout the table created and kept (`text.textLayoutCacheMisses`; also
  a `resources.textLayout` creation), each string drawn through `DrawText` because the table could not serve it
  (`text.textLayoutCacheBypasses`) and each one drawn through `DrawText` at the table's bound
  (`text.textLayoutCacheOverflows`). `text.lineMetricsHeapFallbacks` counts text measurements with more lines than the
  16-line stack buffer of line metrics.
- **Lists.** A `Tree` counts each model row it reads outside `Paint` (`treeItemReads`; `ITreeModel`'s default
  `FindVisibleItemById` scan counts its reads too), each `FindVisibleItemById` it asks of its model (`treeModelLookups`),
  each build of its [id index](../UI/UI_ControlsAndLayout.md#tree-item-lookup-and-model-notifications)
  (`treeIdIndexBuilds`), and per expand or collapse animation the whole row records it keeps (`treeExpansionRecords`) and
  those it copied (`treeExpansionRecordCopies`). On `FX9` each of the 24 navigation keys of its `tree-keys` phase reads one
  row, the one that takes the focus, and builds no index (its model answers lookups itself); on `FX11` every expand keeps
  no record and every collapse the six rows it removes, and none copies a record (`Embedded.Tests.TreeDataPaths.h`;
  receipt `Measurements/PerformanceAndMotion_2026-11-13/W2.6`).
- **Rich documents.** `hoveredTitleCalls` counts each `RichTextEditor::HoveredTitle` resolution (the hover title under a
  point, asked for by the host's supplemental tooltip), and `hoverScanElements` each semantic element a pointer-hover
  lookup tests one by one. Hover lookups go through the prepared hover index, so the latter stays 0.
  `documentBytesCopied` counts the bytes of a `RichTextEditor`'s own document copied into a string (source slices and
  flattens, a flattened prepared display, a word window, a public text-input copy), and `documentFlattens` each copy of
  its whole non-empty source or display. The editor marks its `Document` (and every lineage that document later takes,
  through a reset, an adopted snapshot or an exchanged document), so copies out of any other `RichText::Document` count
  apart, as `otherDocumentBytesCopied` and `otherDocumentFlattens`: the small documents preparation builds for labels,
  such as a line number or language badge of a numbered code fence in the Subtle presentation (one whole-document copy of a few
  units per layout built, which is only a number or a language not built before; plan item `W7.6`, below). With no UI Automation client listening (with one, the
  accessibility event and snapshot still copy the document; see "Rich text-input snapshots" in the input contract), per
  input event in a 64 Ki-unit `RichTextEditor` (the event itself and both text-input synchronizations, not the
  preparation that follows), a one-character insert and an arrow key copy at most 1 KiB and no gated event (insert,
  arrow, Backspace, Ctrl+arrow, Ctrl+Backspace, Enter, double-click) flattens the document. The preparation after an edit
  in Markdown rendering still copies the source once for the Markdown parse (plan item `W7.2`)
  (`Embedded.Tests.RichTextInputCopies.h`; receipt `Measurements/PerformanceAndMotion_2026-11-13/W7.4`).
  A semantic build counts each element it creates (`elementsBuilt`) and each display unit it emits from syntax
  (`displayUnitsWritten`); units copied from reused blocks are not written. Semantic geometry counts each native diagram paint
  object it begins rather than carries (`diagramPreparations`), each prepared formula (`mathPreparations`, beside the
  `testRichMathPreparations` seam), each rasterized SVG strip (`svgRasterizations`, beside `testRichSvgRasterizations`) and
  the pixel bytes it uploads into image bitmaps, at creation with pixels or by `CopyFromMemory` (`imageUploadBytes`).
  Math and diagram painting counts each relative-luminance evaluation its contrast decisions run (`luminanceEvaluations`), each
  flowchart edge whose connector a paint drew (`edgesDrawn`) and each draw call that stroked one (`edgeConnectorDraws`), and
  each math font read from DirectWrite (`mathFontReads`; plan item `W7.7`).
  It counts each line number layout (`codeNumberLayouts`) and language badge layout (`codeBadgeLayouts`) of a numbered code
  fence that it builds rather than takes from the layout cache; a prepared text layout counts each DirectWrite
  `HitTestTextPosition` call that reads a caret box (`caretHitTests`).
  Document layout counts each text chunk it lays out rather than reuses (`chunksLaid`) and each whole-document layout it builds (`wholeLayoutsLaid`). Each owner turn of a
  `RichTextEditor`'s cooperative content preparation counts `preparationTurns`.
  A semantic build also counts the top-level blocks it reuses (`semanticBlocksReused`) and what it copies from the
  previous publication for them: elements (`semanticElementsCopied`), display units (`semanticDisplayUnitsCopied`) and
  all bytes copied, strings, spans, runs, records and source formats included (`semanticBytesCopied`). A build that reused blocks counts
  `semanticIncrementalBuilds`; every other build counts exactly one cause: `semanticFullBuildNoPrevious`, `Presentation`,
  `Footnotes`, `References`, `FrontMatter`, `Abbreviations`, `Capacity` or `Structure`. `identityMapNodes` counts nodes or
  boxes inserted into an identity map to pair one revision with the next: the parser's old-node map (until incremental
  parse reuse replaces it); semantic projection and semantic geometry pair by block range and order and add none.
- **UI Automation.** Each whole-window snapshot build counts `snapshotBuilds`, its control records (`snapshotNodes`) and
  the bytes its per-control record arrays reserve (`snapshotBytes`, owned strings excluded). It counts the same builds as
  the diagnostic `DebugGetAccessibilitySnapshotBuildCountForTest`, which remains. Text-range movement by word counts the
  UTF-16 units it steps over (`wordUnitsVisited`), the enclosing-word search's walk from offset 0 included.
- **Layout.** Each `Control::Measure` with valid input counts a `measureCacheHits` (answered from the control's
  [measure cache](../UI/UI_ControlsAndLayout.md#measure-cache)) or a `measureCacheMisses`; each miss counts one
  `measureOverrideExecutions`. `measureCacheEvictions` counts misses that displaced a slot still valid for the control's
  current revision and context: while it stays 0, every miss is a key the control had not been asked before in that
  revision, so no `MeasureOverride` ran for an unchanged key. `measureCacheVerifyMismatches` counts Debug verification
  disagreements (a hit whose re-run measurement differed), which reveal a control that skipped `InvalidateMeasure`.
  `arrangedChildren` counts each child `SetBounds` an intrinsic owner's arrangement (`FlexPanel`, `FormPanel`,
  `FormField`) makes.
- **Memory.** `controlHeapSizeReads` counts the `HeapSize` calls control object allocation makes, one for each block
  allocated while an allocation sink is set and none otherwise
  ([control object allocation](#control-object-allocation)).

Destruction is observed only by a test-owned `ResourceLifetimeTracker`, in storage the test supplies: while installed on
a thread it holds one extra reference to each resource the helpers create there, and `Sweep` counts and releases those
whose owners released them. Destruction therefore happens at the sweep instead of at the owner's release; nothing else
changes. One creation is counted but never held: a native host's target bitmap over its swap chain's back buffer
(`CountedCreateSwapChainTargetBitmap`), because `ResizeBuffers` refuses (`DXGI_ERROR_INVALID_CALL`) while any reference
to a buffer remains. Before W3.6 the held reference made every tracked window-host resize fail and recreate the chain
(the retained `FX10` window receipt shows one chain created per resize step); its destruction is not observed. Objects
beyond the storage are counted as overflow. Per-interval churn is `min(created, destroyed)` per resource
kind, summed over kinds, and is meaningful only under a tracker.

Enforcement: `Tools/tests/Test-CountedCreation.ps1` (run by the tooling tests) fails on any occurrence under `src` of a
counted method name, whatever the receiver (`->`, `.`, `::`, a call split across lines, a member-function pointer or a
function pointer such as `&::D3D11CreateDevice`), outside the helper header and the creator type in
`src/Support/NativeRendering.h`. Comments and literals are ignored.

The matcher sees only DxUi's source. Counting COM wrappers in test executables observe what actually reaches Direct2D and
DirectWrite (`Tests/Support/Support.Tests.CountingDirect2D.h`, with `Support.Tests.CountingWriteFactory.h` for
DirectWrite). The hosts create their Direct2D and DirectWrite factories through the private seam
`Detail::CreateHostD2DFactory` and `Detail::CreateHostWriteFactory` (`src/Support/NativeRendering.h`); a test installs a
`Detail::FactoryInterposer` on its thread to receive each new factory and return a wrapper. Without one, which is always
the case outside test executables, the seam returns the system factory unchanged; nothing is added to `include/DxUi`.

- **What is wrapped.** The factory as `ID2D1Factory1`, the device as `ID2D1Device`, each device context up to
  `ID2D1DeviceContext7` and the DirectWrite factory as `IDWriteFactory`: the highest version DxUi requests of each.
  Everything they create is the real object, so no wrapper is ever passed to Direct2D, DirectWrite, DirectComposition, an
  effect, a command list, `IDWriteTextLayout::Draw` or a bitmap made from a DXGI surface. A context reports the wrapped
  device and factory from `GetDevice` and `GetFactory`, and a device the wrapped factory, so resources DxUi creates
  through them are seen too. DirectComposition receives the DXGI device, never the Direct2D device.
- **Identity and lifetime.** `QueryInterface` answers a wrapped version with the wrapper itself, one COM identity, and
  only when the real object implements that version. Any other interface is answered by the real object and counted as
  escaped, since calls through it are no longer seen. Each wrapper holds one reference to its real object and releases
  it when it is destroyed, so real lifetimes are unchanged; a test requires every wrapper released after its host is.
- **What is counted.** Each successful creation by resource kind (the helpers' kinds; D3D11 devices are created outside
  Direct2D and are not seen), each object of another kind (a mesh, an ink, a render target, which is returned unwrapped),
  and each draw call in `frame.drawCalls`. A `DrawText` call is also one implicit text layout, as `CountedDrawText`
  counts it.
- **The cross-check.** On the ComplexUi fixture the wrappers see, frame by frame and kind by kind, exactly the creations
  the helpers count on every dirty frame, and both see none on every clean frame; no frame creates another kind and no
  interface escapes. A test-only control that creates a brush directly on the host's device context is reported as the
  one mismatch (helpers 0, wrappers 1).
  The comparison also holds over setup (attachment, first layout and the warm-up frames).
- **Where they run.** Only in dedicated tests: the cross-check, the pixel-identity comparison and the seeded mismatch in
  the Embedded suite, and the `--test-counting-wrappers` receipt. Ordinary suites test the shipping path with real
  objects. Every composited pixel of all 80 measured ComplexUi frames is identical with and without the wrappers. The
  native host creates its shared factories through the same seam, but no test installs wrappers there yet: its shared
  resources outlive a single test, and the cross-check is established on the embedded fixture.

Baseline on the ComplexUi fixture (FX5 of the [performance fixtures](../Testing/Testing_Validation.md#performance-fixtures):
83 controls, 1280×720, 96 DPI, WARP, reduced motion; `DxUi.EmbeddedTests.exe --test-work-counters <receipt>`, also run by
the Embedded suite; the fixture framework's FX5 run must report the same counts): a dirty frame prepares and composites once, paints
every visible control once (83 `Paint` executions and 83 paint visits), repaints 921,600 pixels and, in a Release build,
makes 2 C++ allocations (54 at the W0.1 baseline; the Grid column-description cache removed 52); a clean frame composites
once and reports 0 allocations, 0 created resources, 0 churn and no paint. The baseline receipt is retained under
[Measurements/PerformanceAndMotion_2026-11-13/W0.1](../../Measurements/PerformanceAndMotion_2026-11-13/W0.1/README.md), the
current one under [W2.5/WorkCounters](../../Measurements/PerformanceAndMotion_2026-11-13/W2.5/WorkCounters/README.md).
Under the counting wrappers (`--test-counting-wrappers <receipt> <comparison>`) a dirty frame makes 399 Direct2D draw
calls and a clean frame 0, identically in consecutive runs; the wrappers' creations equal the helpers' (49 implicit
`DrawText` layouts and 7 text layouts per dirty frame, nothing per clean frame) and no pixel changes
([receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W0.1/CountingWrappers/README.md)). These are counts, not timing; a change to them is a measured change with its own receipt, never a silent rebaseline.

IME copy gate (`DxUi.EmbeddedTests.exe --test-ime-copies <receipt>`, also run by the Embedded suite): a composition of
ten syllables and its commit, driven through the production TSF store on a headless native host at about 64 Ki units,
makes at most one document-sized C++ allocation (at least one document's bytes, counted by the test allocator) in any lock
of a `TextField`, and an editable `ComboBox` counts no popup rebuild before the commit. The receipts are retained under
[Measurements/PerformanceAndMotion_2026-11-13/W8.5](../../Measurements/PerformanceAndMotion_2026-11-13/W8.5/README.md).

ComboBox popup gate (`FX2`, `DxUi.EmbeddedTests.exe --test-combobox-popup-layout`, also run by the Embedded suite): a
first open measures each item once (one single-line measuring layout per item), and every open hover frame, every
character typed with the popup open or closed, every repaint of the 100-row popup and a second open of unchanged items
create no measuring layout ([controls and layout](../UI/UI_ControlsAndLayout.md#combobox-popup-layout)). The receipt is
retained under
[Measurements/PerformanceAndMotion_2026-11-13/W2.2](../../Measurements/PerformanceAndMotion_2026-11-13/W2.2/README.md).

Retained text layout gate (`FX5`, `DxUi.EmbeddedTests.exe --test-retained-text-layouts`, also run by the Embedded suite;
[retained text layouts](#retained-text-layouts)): every frame of the fixture's hover phase creates 0 text layouts,
explicit or inside `DrawText` (`G11`); after every step the host's table holds no more entries than the strings its last
paint drew; the first frame creates its layouts in preparation while its composition creates nothing and makes no C++
allocation; and each of FX5's labels measures with 0 C++ allocations. The receipt is retained under
[Measurements/PerformanceAndMotion_2026-11-13/W2.1](../../Measurements/PerformanceAndMotion_2026-11-13/W2.1/README.md).

## Trace provider

Status: implemented (performance and motion plan item W0.8, 14 November 2026). The provider name, GUID, keywords, events
and fields are a consumer-facing diagnostic interface, documented in [performance](../../docs/performance.md#trace-provider);
a change to them is a documented change. The C++ code behind it (`src/Support/Trace.h`, `src/Foundation/Trace.cpp`) is
private.

- **One provider.** DxUi registers one TraceLogging provider, `DxUi` `{89b12dac-c17b-5934-baa0-35b9f94990e6}` (the
  EventSource name hash), once for the life of the process. Registration starts no thread, timer or window; ETW's enable
  callback only updates the listener flag.
- **Dormant.** While no session has the provider enabled and no test sink is installed, every event site costs one
  relaxed load and one predictable branch, with no allocation, formatting, lock or I/O; per-host correlation numbers do not
  advance. With a listener, an event is a fixed record written on the thread doing the work, still without heap work, so
  composition keeps its contract and clean or hidden frames add nothing but the events of work they already do. All event
  writes go through one internal function, which writes the TraceLogging event while a session listens and hands the
  same record to the test sink while one is installed.
- **Events.** Input received (a `WindowHost` mouse, wheel, key or character message; an `EmbeddedHost` pointer, key or
  character dispatch), frame requested with its reason (the first request after each preparation begins), prepare begin
  and end (a native render pass; an embedded preparation that paints), present (a native swap-chain present) or composite
  (an embedded composition), wake by source, hit reuse off (a host stops reusing pointer hit results because a control
  whose hit testing it cannot see attached), and frame paced (a native paced frame's animation sample against the
  compositor's target time, its refresh period and whether the hitch clamp applied;
  [frame pacing](../Rendering/Rendering_Win32Host.md#frame-pacing)). Each event site sits beside the work counter of the
  same moment where one exists: renders and preparations, presents, composites, paced frames and every wake source.
- **Correlation and clock.** Every event carries a process-unique host number and a `QueryPerformanceCounter` timestamp.
  Each host numbers its inputs and its preparations; a preparation records the newest input received before it began, and
  its end, present or composite carry that number, so input-to-present for input *N* is the first present (composite) on
  the same host whose input number is at least *N*.
- **Content.** Events carry identifiers, counts, sizes, reasons, results and times only, never text, control names, paths
  or document content.

Gate (WARP): with nothing listening, a dirty ComplexUi frame makes exactly the allocations of the work-counter baseline
(2 in Release), and with a test sink listening it makes the same number while writing one prepare begin, prepare end and
composite per frame (`TraceProviderComplexUi`, Embedded suite). On a 2,048-row `Grid` scrolled in a `WindowHost` by 100
wheel notches with pointer moves and keys between them, every input yields exactly one input-received event, every
present exactly one present event (equal to the present counter), and each notch's input-to-present is computed from its
frame request and present (`TestTraceProviderPairsEveryGridScrollInputWithItsPresent`, Rendering suite). Where the desktop
session may start a real-time ETW session, the same scenario's events read back through ETW equal the sink's records
field for field (`TestTraceProviderEventsReadBackThroughEtw`); where it may not, that test records a capability skip.
Input-to-present timing on hardware against `G8` is a separate, hardware-gated measurement.

## Mandatory performance policy

Paired setup MUST share the benchmark project and its effective compiler options,
including per-item flags, while retaining each side's own library project/source.
Fresh builds stage every project in the canonical performance attestation catalog
(`Get-PerformanceProjectCatalog`: library and benchmark) after restore and
record/restore every original timestamp so MSBuild's newest-import selection is
comparable. Reused builds retain their attested context. Before any measured
process, both v2 full build states MUST match fixture, harness, compiler,
dependency and environment identities. A mismatch consumes no measurement slots;
retain the refused states. No property, flag or threshold is ignored to get a match.
`MSBuildAllProjects` remains part of the complete profile: staging only the benchmark
can leave the library's newest-project/import selection different for identical bytes.
The staging manifest records the role, physical path, original UTC stamp and staged UTC
stamp for every selected root/project. Partial staging failure restores all acquired stamps.

The failure-policy study also shares the reviewed native build-policy include closure. Its historical baseline
lacks the `Samples/Shared` include directory needed by the common failure-host harness. Strict preflight must
reject that difference before measurement. An explicit study-only overlay of `Build/NativeBuildPolicy.targets`
may align that one include path after retaining the original policy and checking header-resolution collisions;
library sources and project files remain each side's own. Retain both overlays and fresh build attestations,
and restage the complete project catalog after the overlay. This does not qualify or change the acceptance
policy/judge and cannot convert a refused study into a pass.

The current build driver's transitive console module is part of the shared overlay and
full harness identity. Older baseline trees must import that staged module before building;
staging preserves their library sources and compiler/linker project rather than replacing
the measured implementation or its policy. Missing-module and warning-bearing controls
must fail, and restoring a named tree removes only the dependency introduced by the overlay.

Compiled shared-fixture includes must also resolve under the historical baseline's include profile. The embedded
benchmark names its scene relative to the staged header, preserving the baseline library/project configuration.
A real compiler preprocessing control must reproduce the old missing include, accept the staged real headers and
restore the old harness bytes. This control checks include resolution; it is not native benchmark execution.

Hosted paired runs retain original measured executables, libraries, symbols and
build attestations as CI artifacts alongside every report. Continuous host
observations record native cumulative CPU counters, process CPU/memory, sample
gaps and collection intervals; a failed observer cannot admit a completed study.
Inspect observer cost and missing/inaccessible processes explicitly. These
observations are neither a quiet-host guarantee nor a scheduler/GPU trace.
When an observer reports weighted whole-host busy time from `GetSystemTimes`,
that aggregate includes the benchmark, the observer, and other processes on the covered processors.
The API covers all processors on systems with at most 64 processors; above that it covers the calling thread's
primary processor group, so the field name alone does not prove whole-host coverage. Retain that
[documented coverage limit](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getsystemtimes).
It does not isolate external contention or establish that preemption caused a
timing change; report it as shared-host context only.
Analysis MUST retain the original ISO-8601 UTC timestamp precision and parse it
independently of the machine's locale. Implicit JSON date conversion followed by
culture-sensitive string conversion cannot establish process-lifetime bounds.
Artifact retention does not independently retain proprietary compiler/SDK bytes;
record that remaining evidence-closure limit during policy review. Download and
seal expiring artifacts before their retention period ends.

Calling a local study build-free requires checking machine-wide observations,
including other chats' work. Stopping only the invoking chat's builds is
insufficient. Retain compiler/linker activity and distinguish MSBuild property
evaluation from compilation. CPU advances in coarse observation intervals that
contain timed windows do not identify exact-frame scheduling, but must not be
hidden by calling the host quiet. Keep that collection as under-load diagnostics
and coordinate a fresh complete study when build-free admission was required;
do not reuse selected slots. Never stop independently launched processes to
manufacture a quiet host. Scheduler/GPU attribution still requires its own evidence.

Boundary diagnostics MUST report subintervals from the same ranked frame as the
reported frame tail and distinguish window summaries from independent process
replicates. A fixed-order timers-off/on pilot cannot by itself establish isolated
instrumentation overhead or candidate speed; retain ordering reversals and all
processes rather than selecting a favorable mode.

Speed and low memory consumption are primary library requirements for every design, implementation, review and test.
Among correct designs, minimize steady-state CPU/GPU time, private/resident memory, allocations and copies,
synchronization, thread/handle count, wake-ups and graphics submissions. Preserve correctness, accessibility,
security and visual quality; do not trade them away for an unmeasured optimization. All consumers share this policy.

At the native host message-entry pruning seam, a completed stale-interaction pass may be reused only while the
root identity and lifetime, all retained observer identities, and each observer lifetime
remain unchanged. For each nonnull observer, the full resolver captures a bounded reverse path of logical-child
indices during its existing traversal; a cache hit checks the observer's weak token before walking that path from
the currently owned root, requires every path node to remain visible and enabled, and verifies the final identity.
This membership proof detects a live control moved out through mutable `Panel::GetChildren()` even when its address,
lifetime and host interaction revision are unchanged. An over-depth or over-index path is not cached and uses the
full pruning pass on each message until it fits. Cache-path validation uses the host's effective-interactivity rule
for every retained observer; it is distinct from UIA's visible-only control-path semantics.
An ordinary cache hit compares observer identities and checks weak-token expiry without dereferencing a stale control.
Invalidate the published snapshot before invoking cancellation or pruning callbacks; reentrant message handling
must validate independently, and an outer pass may publish only if it was not superseded and its entry state still
matches. Acquire observer lifetime tokens only after the resolver proves effective interactivity. A missing root,
observer identity change, expired token or invalid live path requires the full cleanup pass, including native
text-input deactivation. An unrelated completed presentation mutation may advance the interaction revision without
invalidating these live paths: the full prune classifies membership and effective visibility/enabled state, not
bounds or text. After validating the paths, recheck the entry root/lifetime, revision, pass serial and all observer
keys; a change during validation requires the full pass. In-progress full-pass publication still requires the
unchanged entry revision and latest pass serial. Logical-child queries obey the stable read-only traversal contract
in `UI_ControlsAndLayout.md`; a final state check is not support for destructive getter reentry.
Do not extend this optimization to other prune call sites. Deterministic host tests must show zero additional full
resolver visits for stable messages and unrelated presentation mutations, plus fresh cleanup for invalid paths,
state/lifetime changes, same-message reentry and injected observer changes during path validation.
The diagnostic pass count also distinguishes an uncached cleanup of an empty observer set from a cache hit;
an empty set can require a new pass while visiting no resolver nodes. Cached path walks are separate from full
recursive resolver visits and must not be reported as zero total tree-node work.

Prepared text rendering may retain at most one device-independent DirectWrite layout per eligible IconOnly base
Button. Create or replace it only in the separate owner visual-preparation pass, never during composition. A warm
paint draws it with the current origin and brush and performs no layout creation. Text/format/extent/flow changes,
host retirement, hide and resource cancellation release it; optional preparation failure preserves the existing
DrawTextW rendering path. Toggle and RadioButton descendants retain their distinct paint paths and do not prepare a
base-Button layout. This visual-only work MUST NOT set content-geometry-changed state, publish UIA/text-input
geometry, or make input wait for layout readiness. Include the bounded retained COM resource and its preparation
CPU in measurements; do not infer a process-memory benefit from avoiding per-paint shaping.

On a warm owner pass, resolve the current effective text-format pointer and check it against the retained key before
returning. This preserves exact-format identity through bounded host-cache eviction; a changed pointer prepares one
replacement. A matching key skips the DirectWrite factory lookup and text-layout creation. This is a narrow owner-pass
work reduction, not evidence of lower process memory or a measured performance gain.

Evicting configured text formats prepares nothing: a format a frame draws with is never evicted, and a retained key
holds its format by `com_ptr`, so an evicted format keeps its identity for that key until the key changes
([format and brush caches](#format-and-brush-caches)). Embedded hosts trim before an update's visual-preparation
traversal. Cache trimming is not performed on clean composition frames.

Before changing implementation, measure the existing code with the same workload that will measure the candidate.
Retain the original baseline and measure again after the change. Record exact source/content and executable/fixture
fingerprints, compiler/dependencies, architecture/configuration, hardware/OS/driver, power policy, resolution/DPI,
visible controls/data size and repetitions. Run serially on the same quiet fixture. Cross-machine, changed-workload,
Debug/Release or x64/ARM64 comparisons cannot establish non-regression. If a new benchmark is required, run the same
harness against both implementations; `performance-paired.ps1` measures a baseline (a revision in a detached
worktree, or an existing tree named as it is) and a candidate (this checkout, a revision or a named tree) with this
checkout's harness inputs and runs the interleaved A/B/B/A pass serially, repeated as a [paired set](#paired-sets).
It refuses a pair with nothing to compare: two revisions that name one commit, or, when a tree is named, two
identical library source fingerprints; each side's revision or path, commit and fingerprint are recorded. The library
fingerprint includes the vendored libfyaml 0.9.4 tree; full native build attestation independently covers its compiled
inputs. A missing baseline is explicitly unpaired and cannot close an implementation performance gate. Documentation/tool-only changes
that leave compiled library inputs unchanged record that fact.

DxUi MUST NOT accept a confirmed performance or memory regression silently. Compare FPS, frame/input percentiles,
preparation and composition costs, allocations/bytes, surface/cache residency and peaks, private bytes/working set,
resource counts, idle activity and long-run retention. Keep all samples and investigate repeatable degradation,
including trends smaller than automated noise bands. `performance.ps1` compares five-round medians, with 5% timing/FPS
and 2% process-memory investigation bands; deterministic surface/allocation budgets allow no growth. A
[paired set](#paired-sets) applies the same bands to repeated runs. Those bands address measurement noise, not a
permitted regression budget. Repeat suspicious results against the retained baseline. Never relax a threshold, omit a
workload or replace the baseline merely to make a test pass.

On confirmed degradation, stop accepting/merging the affected development, present the measured deltas and suspected
cause, ask the developer for advice, and propose options with quantified costs: optimize the affected path/caches,
reduce optional scope, or defer/revert the change. Any explicitly approved tradeoff needs durable rationale and the
chosen resource budget in this contract or its owning domain; a WIP note alone cannot waive a requirement.

### Paired sets

The migrated study uses `performance-paired.ps1 -Blocks 12` by default. Every independent block contains two baseline and
two candidate runs, scheduled as either ABBA or BAAB. Each block independently draws either order with probability 1/2,
using the retained seed. Neither the counts of the two orders nor later draws constrain previous draws. Both orders
balance linear within-block drift. The `independent-ABBA-BAAB-v1` allocation protocol, seed and literal schedule are retained;
qualification replays them exactly. Repetitions inside a block are not treated as independent blocks.

- The judge computes a direction-normalized paired log-ratio for each block from the two run medians per side, then uses a
  two-sided exact sign-flip randomization test across independent blocks. Holm step-down correction controls familywise
  alpha at 0.05 across the predeclared 26 scenario-phase-metric slots. Twelve blocks are the minimum: complete separation
  has minimum two-sided p = 2/4096 = 0.000488, below the first Holm boundary 0.05/26. Fewer observations cannot be
  promoted to qualification by treating correlated runs as independent.
- Study schema 3 and judge `dxui-paired-block-sign-flip-holm-v2` require that assignment protocol. Earlier schema 2 studies
  forced six blocks of each order, permitting only C(12,6) = 924 assignments, while their judge enumerated 4096 independent
  sign choices. Their claimed exact randomization inference is invalid for that design. The correct restricted-design
  minimum two-sided p is 2/924 = 0.002165, above the first 26-outcome Holm boundary. Preserve those records for diagnosis;
  fresh independent-order A/A calibration and paired studies are required before approving the migrated policy. A valid
  independent draw may itself produce six of each order, so the protocol identity and replay, rather than observed counts,
  determine eligibility. This follows the requirement that a randomization test reflect the actual assignment scheme
  ([Hemerik and Goeman](https://arxiv.org/html/1912.02633v2)). Investigation bands, family size and exact budgets are unchanged.
- Timing/FPS investigation bands remain 5%, process-memory bands remain 2%, and deterministic surface/allocation budgets
  remain exact. Exact budgets do not use statistical averaging: any candidate run median above the baseline run-median
  median is a regression, and any candidate raw-round maximum above the retained baseline raw-round maximum is a separate
  no-tolerance peak regression. The report shows both maxima and both findings. No timing or memory flag becomes “chance”
  merely because source fingerprints or executable hashes match.
- Each receipt must identify source, executable, benchmark inputs, harness, toolchain, dependencies, machine, CPU, OS,
  GPU/driver, WARP binary hash and version, system DPI and power policy. Toolchain identity uses MSBuild-evaluated target properties,
  compiler/linker/MSBuild/default-toolset byte hashes, imported project files, resolved compiler item metadata, and the
  selected Windows SDK header/library closure. Dependency identity uses the platform install's vcpkg status, package
  inventories/ABI metadata, and installed/output runtime DLL closure. Missing or mismatched required provenance makes the
  study inconclusive; a source fingerprint by itself
  never establishes runner noise. A/A calibration is a separate study and cannot qualify a pull request by itself.
  It builds one explicit revision once and measures that same executable under both randomized role labels, with
  distinct raw receipts for every invocation. Separate builds can encode different PDB paths or link timestamps;
  matching source revisions therefore do not satisfy A/A executable identity.
  The [26 October current-source A/A archive](../../Measurements/RedesignCompletion_2026-10-26/aa-calibration-4077/README.md)
  illustrates this boundary: no regression rows coexisted with 72/72 unstable controls, so the result does not
  establish runner noise or approve the policy.
- Fresh measurements require a `performance-build-v2` attestation before and after build and execution. Each side's
  expanded projects and resolved compile/shader inventory belong to its build-input identity. The comparison profile
  separately binds actual MSBuild, compiler/backend, shader compiler, linker/librarian and SDK bytes plus effective
  options and target evaluation context for both the library and benchmark. A changed source inventory alone is not a
  toolchain mismatch. A library `.c` item may explicitly select `CompileAsC` alongside C++ items only when every
  other effective compiler option matches the common C++ profile. Each source path retains its exact language and
  compiler metadata in the build-input identities; opaque `/TC` or `/TP` option overrides, a C++ item compiled as C,
  C-only option drift, a mixed benchmark
  profile, missing metadata, other mixed compiler profiles and unsupported build overrides are inconclusive;
  historical attestations cannot be relabelled as v2 or reused without a fresh build.
- Default-output incremental `Build` additionally requires a separate receipt binding every intermediate path and byte
  in the supported platform/configuration profile, including objects, tracking logs and generated shaders for all solution
  projects. Missing or changed cache forces `Rebuild`; final-only `SkipBuild` does not depend on disposable intermediates.
  An empty `unsuccessfulbuild` marker is recognized only in the evaluated
  `TLogLocation`, a non-reparse direct `.tlog` child of that project's profile
  intermediate directory. This includes MSBuild's shortened directory names.
  Populated, misplaced or nested lookalikes remain unsupported ignored inputs.
  A failed attempt revokes reuse; recognizing its status marker permits a fresh
  `Rebuild` and supplies no passing build or runtime evidence.

RichTextEditor prepares selection ranges and an optional union mask before
composition. Multiline selection redraws selected text once through that mask,
rather than repainting a document for every line. One retained layer is bounded
by the control viewport, reused within its resource generation, and released on
layout retirement/hide/detachment. Mask creation and layer allocation occur only
in preparation; single-rectangle selections retain the direct clip route.
Actual draw-call counts, exact raster parity and paired/resource evidence are
separate required checks for this rendering change.
  Both receipts are revoked before build and on failed publication. Resolved C++/HLSL source bytes and the owned native
  include closure belong to each side's build identity, never the shared compiler comparison. Arbitrary external or
  ignored native inputs, response/path-bearing option overrides, custom outputs, precompiled headers and modules are
  unsupported. Intermediate include directories accept only the exact two derived composite-shader headers plus
  supported native object/tracking/output files, preventing extensionless or alternate header shadowing. The selected
  MSVC/ATL, SDK/UCRT/WinRT/C++/WinRT, auxiliary VS and NETFX input roots bind all file bytes and optional-root absence.
  Native Git input discovery uses the exact scoped-testing module imported by that performance-policy
  instance. Paired baseline/candidate/harness checkouts may load identically named modules together;
  process-wide name lookup must neither select another checkout nor turn multiple instances into a command string.
- A policy binds the approved judge version and normalized source SHA-256. The candidate judge must match both values from
  the measured-base policy to be eligible for qualification; agreement of verdict labels alone does not approve changed
  judge code. It also binds the assignment protocol and normalized `Tools/PairedRun.psm1` source SHA-256; both the immutable
  measured-base source and candidate generator/replay source must match that approved hash. A candidate cannot change
  assignment semantics while keeping the protocol string. Proposed hashes in the unqualified candidate policy are not trusted. Before seeding the first qualified
  policy, reviewers inspect a retained, same-source A/A calibration for the same runner, toolchain, harness and scenarios,
  including full provenance, per-metric controls and exact-budget findings. A/A review is manual, has no invented numeric
  pass threshold, and cannot qualify a pull request.
- The migration summary loads the legacy judge from `git show <measured-base>:Tools/PerformanceComparison.psm1` in an
  isolated module scope, then runs it and the candidate judge over identical parsed reports. It records both source
  hashes, the base commit, every receipt SHA-256, and both decisions; the report files are rehashed around judgment. An
  unavailable base judge, changed reports, or a changed verdict is inconclusive and requires explicit review; a
  candidate cannot bootstrap the trusted policy from its own tree. The first policy is marked
  `policy-review-required`; the migrated gate remains inconclusive until an independently reviewed, versioned policy is
  present and qualified in the measured base.
- Qualification independently replays both frozen judges over the same hash-verified, once-parsed raw receipt bytes.
  All three declared scenarios, every scheduled run, the complete 26-slot decisions and both same-side controls per
  block are mandatory. The consumed set must equal the full replayed decision, including numeric and exact-budget
  evidence. Control comparisons must reproduce their scheduled raw pairs; drift is then consumed from that canonical
  replay, with absolute 5% timing and 2% memory investigation bands and unchanged exact budgets. Summary labels and
  later comparison-file reads cannot substitute for this evidence. Each scheduled role's summary commit and library
  fingerprint, each run's source state and executable/harness hashes, and the declared build profile must match its
  hashed raw receipt before the summary can classify an unchanged library. Qualification also checks every raw round
  against the absolute whole-frame C++ allocation budget: zero in clean rounds, at most 64 per frame in Release dirty
  rounds or 320 in Debug/ASan Debug dirty rounds. The receipt's declared ceiling must match that fixed profile; equal
  over-budget baseline and candidate measurements do not qualify a pass.
- `advice-required` remains a measured finding, not permission to merge or rebaseline. The retained summary records the
  block schedule, seed, receipt identities, the legacy and migrated decisions, the correction family and every metric.
  Historic Mann-Whitney summaries stay readable, but the migrated gate treats them as read-only and never as qualified passes.

### Frame timing method

Timing budgets and architecture decisions that need a measured speed-up use the frame timing method
(`performance-timing.ps1`, `Tools/FrameTiming.psm1`, native `DxUi.EmbeddedTests.exe --timing-method`). It is an input
to the timing policy owned by D-P1 in the remaining-work plan. It changes neither the canonical five-round receipt, the
paired sets above, their judges, bands and exact budgets, nor the ComplexUi benchmark inputs copied onto baseline trees.

- **Fixture.** A run measures one registered fixture (`FrameTiming::MakeFixture`) in one phase: clean (no
  application change) or dirty (the fixture's per-frame update). The method is fixture-agnostic; `FX5` is the ComplexUi
  scene. Each run is a fresh process with 60 warm-up frames. Every timed dirty frame is a step the fixture declares:
  unless the fixture declares that its timed phase repeats (`Definition::timedPhaseRepeats`, FX5), the phase's declared
  steps are played in a cycle, and before each new cycle the scene is rebuilt and the phases before it replayed outside
  the timed frames. A judged dirty run requires every timed frame to prepare work (`Prepare` returns `S_OK`).
- **Run length.** A judged run holds at least 1,000 measured frames and lasts at least 2 s, ending when both hold.
- **CPU.** `QueryThreadCycleTime` of the measuring UI thread is taken around `Prepare` and around target binding plus
  `Composite`, separately, every frame. Submission wall time (update, preparation and composition) is recorded too.
- **GPU.** In test fixtures only, each frame is bracketed by a D3D11 disjoint query holding three timestamps (begin,
  after preparation, after composition). A frame's results are read one frame late, after the next frame has been
  submitted; the blocking wait maps that frame's own one-pixel staging copy, never the frame just submitted. A disjoint
  frame is excluded and counted; a judged run needs at least 95% valid frames. The library's shipping paths issue no
  timestamp query, staging copy or pinning.
- **Judged metrics and margins.** Six: median preparation cycles, median composition cycles, p95 UI-thread cycles
  (preparation plus composition), median submission time, all with a 3% margin; median and p95 GPU frame time
  (preparation plus composition timestamps), with a 5% margin. Percentiles are nearest-rank.
- **Schedule.** A session runs A, B, B, A as four processes; a session's effect per metric is the mean log of its B runs
  minus the mean log of its A runs, which cancels drift linear in log time.
- **Equivalence.** Two one-sided exact sign-flip tests over the session effects against the declared margin, at
  alpha 0.05; five sessions are the minimum that can establish equivalence (2^-5 < 0.05). A difference is the paired
  judge's two-sided exact sign-flip test, which needs at least six sessions. Every result reports the minimum
  detectable effect, (t(0.975, n-1) + t(0.80, n-1)) x sd / sqrt(n) of the session effects, as a percentage.
- **Noise floor.** A calibration measures one attested executable in all four slots of every session. A metric's noise
  floor is its largest same-binary pair deviation |B/A - 1| over the sessions. Its resolution is the largest clock step
  of any run relative to that run's statistic: the UI-thread cycle clock advances in steps (about 427 cycles on the
  reference machine), submission time in 0.1 us, GPU time in one timestamp tick. A metric is judged when its noise floor
  stays below its margin and its resolution is at most a tenth of the margin; otherwise it is advisory, recorded with
  its noise floor and the reason, and a comparison reports it without a verdict. A 0.00% floor is judged only under that
  resolution rule: a constant statistic has no observable step and is advisory. With a calibration, a faster or slower
  verdict must also exceed the metric's noise floor.
- **Admission.** Calibration and comparison require a hardware adapter, the Release configuration, judged run length,
  five sessions (six for a comparison), a required idle preflight and the High Performance power scheme; WARP stays for
  counters and pixels. Anything weaker is a smoke run, labelled as such, with no calibration, noise floor or verdict.
- **Machine state.** The runner activates the existing High Performance scheme with `powercfg` and restores the
  original scheme however the study ends, verifying the restoration. It never creates a scheme and never elevates; an
  absent or refused scheme stops the study. `auto` samples every logical processor's busy share for 500 ms and pins the
  measuring UI thread to the lowest-numbered logical processor of the quietest physical core of the highest efficiency
  class, never processor 0's core when another exists; the run records the core, its siblings, the last-level cache and
  the sample. Each session starts `Tools/Watch-BenchmarkHost.ps1` for the idle preflight and keeps observing until the
  session ends. Other people's processes are never stopped.
- **Core isolation.** With `-CoreIsolation On` (default), the measured process keeps its other threads (Direct2D,
  DirectWrite, WARP and driver workers) off the measuring core, without elevation: its default CPU set
  (`SetProcessDefaultCpuSets`) is every logical processor of group 0 except the measuring core's, and the measuring thread
  selects its own processor's set. A thread on the measuring core's sibling shares its execution resources: one busy
  sibling thread made FX5 frames about 50% slower. A process that is multi-threaded by design is measured this way: its
  other threads run on the other cores, where their processor time is the process's own use (recorded, never judged as
  interference); the rule judges only what reaches the measuring core, whoever's work it is. The run records whether
  isolation was applied; `-CoreIsolation Off` reproduces earlier runs.
- **Interference records.** Every run records, for its timed loop, without elevation: each frame's running share (the
  measuring thread's cycles over time-stamp-counter ticks during preparation and composition, x64 only); every logical
  processor's busy share (`NtQuerySystemInformation`); this process's own processor use; hence the busiest sibling of
  the measuring processor, other work on the measuring processor itself, and other processes' load in logical
  processors. Per-processor busy shares are not attributed to processes: without isolation the sibling's figure also
  holds this process's own helper threads. Around each session of every purpose, load sensitivity included, the runner reads the `GPU Engine`
  running-time counters and records the busiest engine of the measured adapter (by LUID) occupied by other processes,
  from per-instance deltas; the study's own children (timing runs, synthetic load) start and end inside the window, so
  an instance present in only one snapshot is left out. A load-sensitivity session's quiet runs are judged on that figure.
- **Admission rule.** Receipts name the rule they were admitted under. `interference-interim-v2` (default, provisional):
  the preflight refuses a session when other processes keep more than a quarter of the logical processors busy (at least
  one; 8 on the 32-thread reference machine), leaving out the study's own processes (observer, runner, load children);
  a run is rejected when the measuring thread's running share is below 0.99 at the median or 0.97 at the 5th
  percentile, its sibling is more than 10% busy, other work keeps its own processor more than 10% busy, other processes
  keep more logical processors busy than the cap, or other processes keep the measured GPU's busiest engine more than 15%
  busy; a session is rejected when its four runs' other-process load differs by more than 2 logical processors. A
  calibration or comparison repeats a rejected session up to three times and records every rejection; a load-sensitivity
  study judges only its quiet runs. A refused preflight waits (30 s by default) and retries (10 times); every admitted
  session is checkpointed, and the same command resumes an interrupted study (`-Restart` discards the checkpoint). A run
  without these records (an older executable, or ARM64, which has no time-stamp counter read) is not admitted.
  `interference-interim-v1` (15 November: own processor at most 5%, no steadiness clause) and `host-busy-10pct-v1`
  (the original whole-host 10% preflight; receipts without an admission field were taken under it) stay selectable
  with `-Admission InterimV1` and `-Admission Legacy10`; receipts taken under them stay valid.
- **Evidence for the rule.** A partial load-sensitivity study on the reference machine (23 November, FX5 dirty, 26 of
  30 sessions; the study stopped on a desktop burst of 9.4 busy logical processors in the next preflight) gives, for
  the loaded runs against quiet runs of the same sessions (mean change of the three judged metrics): 0 extra busy
  logical processors, +0.04 to +0.11% (equivalent); 2, +1.6% (equivalent within the 3% and 5% margins); 4, +3.2 to
  +3.4%; 8, +4.2 to +4.5%; 12, +5.7% (GPU +7.4%); one busy thread on the sibling, +46 to +52%. Load elsewhere slows the
  measuring thread without preempting it (its running share stayed at 0.99 at every level), so the total-load cap and
  the session steadiness clause, not the running share, guard against it: two extra busy logical processors are within
  margin, four are not. The busy sibling justifies the strict sibling limit and core isolation. Whole-host load alone
  did not predict differences in the earlier data (attribution passes at 9% or more of the host differed by at most
  0.74%; calibration deviations correlate with host load by at most 0.34), while the running share did: the largest
  attribution difference (7.3%) came from a pass at 0.976 cycles per tick, and the three FX5 calibration runs whose
  5th-percentile cycle rate fell about 6% carried the three largest GPU p95 deviations (+13 to +18%). Quiet runs read
  0.00 to 0.06 of other work on the measuring processor; FX5 comparisons were rejected at 0.050 and 0.058 under the 5%
  limit, so v2 allows 10%. The 15% GPU limit has no evidence yet.
- **Load sensitivity.** `-Purpose LoadSensitivity` measures one executable on FX5 dirty frames with the measuring thread
  pinned to the quietest core, in arms of five A/B/B/A sessions run round-robin: A runs quiet, B runs under a synthetic
  load of busy logical processors placed away from the measuring core (one per other core, farthest first, then second
  hardware threads), and optionally one arm with one busy thread on the measuring processor's sibling. The load is
  `DxUi.EmbeddedTests.exe --busy-load`, the runner's own child, started before and ended (and waited for) after each B
  run. Per arm, every judged metric of the given calibration is tested for equivalence within its margin; the admissible
  level is the largest level at which it and every lower level are equivalent. That level sets the total-load cap and
  the session steadiness limit.
- **Receipt.** The committed receipt is the retained form: per-run summaries (statistics, clock steps, resolution,
  frame counts), each raw run report's path, SHA-256 and size, the judged statistics, the host preflight records, the
  power scheme record, the pinning, adapter identity and driver version, the executables' hashes and this tree's v2
  build identity (checked before and after the study under the measured power scheme). The raw reports, the full
  receipt and the host logs stay in the build tree. `Tools/Convert-FrameTimingReceipt.ps1` derives the retained form
  from a full receipt and refuses if a summary, floor, minimum detectable effect, equivalence test or status changes.

Calibration on the reference machine (15 November 2026, source `9574105be`, x64 Release, NVIDIA GeForce RTX 5080 driver
32.0.16.1714, five sessions, idle preflight 5.7 to 8.6% busy; receipt in
`Measurements/PerformanceAndMotion_2026-11-13/W0.2/`). Comparisons on this machine judge only the judged metrics.

FX5 dirty (ComplexUi, 1280x720, 96 DPI):

| Metric | Margin | Noise floor | Minimum detectable effect | Status |
| --- | --- | --- | --- | --- |
| Median preparation cycles | 3% | 0.55% | 0.35% | judged |
| Median composition cycles | 3% | 7.69% | 3.42% | advisory (one clock step, 7.65% of the statistic) |
| p95 UI-thread cycles | 3% | 4.81% | 2.09% | advisory |
| Median submission time | 3% | 0.55% | 0.39% | judged |
| Median GPU frame time | 5% | 1.24% | 0.63% | judged |
| p95 GPU frame time | 5% | 15.14% | 4.95% | advisory |

FX3 dirty (2,048-row Grid, one wheel notch per frame, 800x600, 96 DPI; source `19e970047`, idle preflight 7.7 to 9.9%
busy). Each run replays the 100 declared notches about 57 times from a rebuilt scene. A first calibration that timed
frames past the end of the list is not retained.

| Metric | Margin | Noise floor | Minimum detectable effect | Status |
| --- | --- | --- | --- | --- |
| Median preparation cycles | 3% | 1.85% | 0.87% | judged |
| Median composition cycles | 3% | 0.00% | 0.00% | advisory (one clock step is 9.95% of the statistic) |
| p95 UI-thread cycles | 3% | 15.04% | 11.89% | advisory |
| Median submission time | 3% | 1.69% | 0.83% | judged |
| Median GPU frame time | 5% | 5.64% | 2.66% | advisory |
| p95 GPU frame time | 5% | 9.05% | 6.90% | advisory |

### Hardware attribution method

Where a frame's time goes on a hardware adapter is recorded by the hardware attribution method
(`performance-attribution.ps1`, `Tools/FrameAttribution.psm1`, native `DxUi.EmbeddedTests.exe --frame-attribution` and
`DxUi.ControlTests.exe --present-attribution`). It is the `W0.7` receipt of the performance and motion plan and the input to
the choice between retained painting (`W4`) and a DxUi-owned GPU backend (`W10.4`). It needs no kernel-trace rights.

- **Matrix.** `FX5`, `FX3` and `FX12` at 1920x1080 and 3840x2160 (`FX5@fhd-144dpi` and `@uhd-192dpi`; `FX3@fhd` and
  `@uhd-192dpi`; `FX12`'s dense screen, default and `@uhd-192dpi`, and its library-laid-out `@form-layout-10k` and
  `@form-layout-10k-uhd`), each with a full frame (`ControlHost::Invalidate` with nothing changed) and a typical dirty
  frame (the timed phase of `FX5` and `FX3`, one hover change on `FX12`), on the embedded host; and `FX12`'s dense screen
  at both sizes on the window host.
- **Passes.** Each case runs three fresh processes on the frame timing method's loop, each at least 10 s for a receipt.
  *Timing* is that method unchanged: the frame's UI-thread CPU (preparation plus composition thread cycles, converted at the
  time-stamp counter rate, which `QueryThreadCycleTime` counts in on x64; the report checks thread cycles per tick), the GPU
  timeline from timestamps around preparation and composition, and the GPU's busy time: the per-process "GPU Engine"
  performance counters' running time over the measured frames, readable without elevation. The timeline of a CPU-bound
  frame includes the GPU waiting for the CPU's next submission, so GPU time is compared with CPU time through the busy time
  wherever the counters exist. *Phases* installs the counting COM wrappers with a test-only
  call-time record: the time inside `BeginDraw`, `EndDraw`, `Flush`, every Direct2D draw call and DirectWrite text layout
  and format creation, per frame, with the `W0.1` counters (Direct2D draw calls, `Paint` executions, pixels painted,
  preparations, layouts created). Preparation minus every wrapped call is DxUi's own code, with the C++ runtime and system
  calls it makes. The pass is instrumented; its overhead against the timing pass is recorded, and its shares, not its
  absolute times, are its result. *Sampling* runs a sampler thread of the test process that suspends the measuring thread
  about every millisecond while it is inside preparation or composition, reads its instruction pointer and attributes the
  sample to the module holding it: DxUi (the executable), `d2d1`, `dwrite` (with `textshaping`), `d3d11` with `dxgi` and the
  user-mode driver (every module under the driver store), the C++ runtime, system (`ntdll`, `kernelbase`, `kernel32`,
  `win32u`) and other. It is leaf-module, user-mode sampling: a sample in a system call lands in its user-mode stub, callee
  time is never credited to a caller, and a blocked thread's samples are counted apart as waiting.
- **Present.** On the window host the pass draws full frames of the non-activating test window, which sits off every
  monitor, through the host's own `WM_PAINT`, and reads its render and `Present` wall times from the public frame metrics
  (`SetFrameMetricSink`). `DwmFlush` after each frame lets the compositor take the frame first, so `Present` measures its
  own cost, not a wait for a free buffer. The embedded host has no `Present`: its composition is the equivalent.
- **Kernel sampling.** Each run probes the documented unelevated route once (`wpr.exe -start CPU`, cancelled at once if
  accepted) and records the result with the command an administrator would run. Without kernel-trace rights the module
  attribution is the in-process sampling above; no number is invented for what it cannot see.
- **Machine state.** The power scheme is read, never switched: a receipt refuses unless High performance is already
  active. A receipt also requires a hardware adapter, Release, x64, 10 s passes and a `Tools/Watch-BenchmarkHost.ps1`
  preflight that the frame timing method's admission rule admits; the observer keeps running and each pass records the
  host's load beside its own. Nothing takes focus. A smoke run relaxes these and says so.
- **Clean passes, retries and resumption.** A pass is clean when at least 95% of its frames did work and the frame timing
  method's admission rule (interference with the measuring thread, `interference-interim-v2` by default; earlier rules
  selectable) admits it. Every embedded pass pins its measuring thread with the method's isolating form, which keeps the
  measured process's other threads off the measuring core; the pass records whether isolation took effect. Admission is
  judged on the interference the timing child recorded during the pass and on other processes' busy time on the measured
  adapter. The sampling pass is judged on every indicator but the measuring thread's running share,
  which its own sampler lowers by design; the window-host present pass, whose thread is not pinned, on other processes' load
  from the observer against the rule's cap. Each pass records its indicators and admission, and the receipt names the rule.
  In a receipt run an unclean pass is measured again once the host is quiet,
  within a bound per pass and in all; every attempt and its load is recorded, and only clean attempts enter the run. After
  every pass the run writes a checkpoint beside its raw reports; a later run may resume from it, reusing clean passes of the
  same commit, build attestation, executables, adapter and driver, power scheme and run length, within a maximum age, and
  records which run measured each pass. The receipt is published only when every case of the matrix has every pass clean; a
  run of part of the matrix (the two decision cases alone, for example) writes a partial record, never the receipt.
- **Decision reading.** For `FX12@uhd-192dpi` (the 2,000-control screen at 4K of budget `G5`) and
  `FX12@form-layout-10k-uhd`, the receipt reads `W0.7`'s rule on the full frame: at most 4 ms of UI-thread CPU with GPU time
  below it, more than 8.33 ms with `d2d1`, `d3d11` and driver submission dominating, or between. Dominance is read
  inclusively, as the phases pass's share of the frame spent inside Direct2D calls (which contains DirectWrite rendering,
  the runtime and the driver work those calls do) reaching half; the sampled leaf shares are recorded beside it. The two
  regions the rule leaves open are reported as outside the rule. The decision is the developer's.
- **Receipt.** `attribution.json` is a library measurement (`workloadOwner` `DxUi`, fixture `dxui-frame-attribution-v1`) with the
  SHA-256 of every input source in `benchmarkInputs`, checked unchanged at the end of the run. It holds summaries only
  (medians, means, shares, counters, adapter and driver, source commit, build profile and executables' hashes, power scheme
  as read, idle preflight and host load); every raw per-frame
  report stays in the build tree, named in the receipt with its SHA-256. Fixture code, the wrappers, the sampler thread and
  the timers are test-only: no library path gains a timer, thread or query.

### Native accessibility scheduling and synchronization

Native attachments start without a full tree snapshot. Mutations mark a revision dirty and post at most one private,
attachment-cookie-checked publication message while a provider or UIA listener is interested. An owner-thread provider
query flushes that revision synchronously; worker queries read the last complete published snapshot. Actual focus changes
publish synchronously so the system's first worker focus query can report the new control. Event comparison has a separate
baseline: a fresh owner query does not consume the queued notifications. Embedded views continue to publish explicitly
after application preparation, with no snapshot work in composition.

Interested native mutations resolve unchanged focus through the published address/path index first. Validate
that path against the live tree, visibility and semantic control type on every use; a stale or absent path
falls back to the canonical live-tree search. Do not retain a second focus cache or skip focus-transition publication.
Label text, button text and button toggle changes may reuse an already-posted publication without repeating
interest/focus checks. They still mark the exact attachment revision dirty. This content-only path is forbidden
for focus, focusability, enabled state, visibility or logical-structure changes. General mutations must reconcile
focus even while publication is pending; provider queries must still return all latest text and toggle state.

When neither a provider has been requested nor a UIA listener is present, a native mutation still marks the
revision dirty but does not load the published snapshot, search the focused-control path or post a new flush.
The next owner-thread provider query resolves the latest dirty revision. A flush already posted before interest
ceased remains scheduled and includes subsequent mutations; interested focus-transition publication is unchanged.
Diagnostics may override only this mutation-interest decision for an exact owner-thread HWND attachment cookie;
the override must not alter other attachments, provider queries or global UIA listener state.

The developer approved retaining this coalescing tradeoff after reviewing the 9 October 2026
[paired scheduling study](../../Measurements/Review-2026-10-09/AccessibilitySchedulingPairedBaseline/Runs/20261009T201426Z-seed-20261009/README.md).
For its 128-operation synthetic batches, keystroke and selection total cost fell 95.92% and 98.43%, while representative
first-query cost rose from 11.90/8.10 to 431.83/321.93 microseconds. The queue budget remains at most one pending
publication message. These controlled batches, with unchanged focus and no intervening query or publication delivery,
must build no full snapshots during mutation and one coherent fresh snapshot at the first owner query.
Fresh-query correctness remains required. Larger-tree and query-heavy consumer workloads need their own retained
baseline and latency qualification; this Debug study and approval establish no universal query-latency allowance or
Release benchmark-policy pass.

The study's globally balanced schedule is historical: its original independent-sign-flip inference is superseded by
the [restricted-allocation correction](../../Measurements/Review-2026-10-09/RandomizationDesignCorrection50/README.md).
All six corrected Holm-adjusted p-values are 0.0129870129870; the observations and approved tradeoff are unchanged.
It cannot seed the independent-order qualification policy.

The HWND registry and canonical-provider cache use separate nonrecursive mutexes. Immutable provider queries do not take
the mutable range-state lock, and owner-thread actions resolve, unlock, invoke and revalidate across callbacks. Text ranges
retain a recursive mutex per target while their helpers can reenter; demoting that mutex requires bounded reentrancy
evidence, not a mechanical substitution. No range lock is held while waiting for an owner-thread visual-line operation.
If another caller changes its endpoints or the document text during that dispatch, the delayed operation returns `UIA_E_INVALIDOPERATION`,
leaves the newer range unchanged and reports no movement. A caller may retry using the current range. Replacement controls
remain unavailable to the old range. Snapshot-derived peers bind the source control identity, so navigation and range
creation cannot silently attach an old snapshot's answer to a new control at the same path.

Native default text ranges read current coherent text and clamp their endpoints when queried. Explicit composition text
overrides retain their declared text. Native peers retain control lifetime identities, not a whole predecessor tree
snapshot per peer; embedded peers retain their creation snapshot for the embedded lifetime contract.

The callback-retirement review retains immediate destruction with ownership-safe extraction and checks at every external
callback/capture-cleanup boundary. A deferred physical-retirement queue is not introduced: it would require immediate
logical invalidation, outermost nested-dispatch accounting and a measured retained-byte budget, and would not protect
borrowed models or destruction of the host itself. Revisit it on a reproduced residual lifetime failure with ASan and
matched latency/retention evidence. This decision does not close whole-host callback destruction or hardware/client gates.

### Hosted paired gate

The paired-benchmark job of [benchmark.yml](../../.github/workflows/benchmark.yml) is an explicit manual hosted x64
Release action. [The unified CI contract](../Build/Build_UnifiedCI.md) owns event routing: PRs do not compile,
and automatic main batches do not repeat this long study. Removing the PR/timer route does not waive required
before/after performance acceptance.

- **When.** An operator names the baseline and optionally the candidate (empty selects the dispatched source). The
  harness resolves immutable commits before measurement. Each manual run has its own non-cancelling concurrency lane,
  a 240-minute enclosing bound, 15 minutes for restore and 210 minutes for measurement, leaving time for evidence.
- **What is compared.** Both frozen commits build in Release and run Default, MultilineGrid and MultilineGridDistinct
  with twelve independently drawn ABBA/BAAB blocks each: 24 observations per side per scenario. The assignment protocol,
  schedule, seeds, original outputs and receipts are retained. The baseline is measured afresh. The allocation counters
  and the shared fixture are part of the benchmark identity.
- **Conclusion.** require_passing_verdict defaults to true and reaches the canonical Publish-BenchmarkVerdict gate.
  Diagnostic A/A requires identical explicit revisions and explicitly disabling that gate; a green diagnostic workflow
  never establishes performance qualification. Inconclusive, incomplete or mismatched evidence cannot pass acceptance.
- **Evidence.** A continuous host observer must report readiness before measurement, stop cooperatively in finally and
  retain continuity/cost information. Failed measurement still produces a verdict, and every outcome uploads original
  EXE/LIB/PDB/DLL outputs, receipts, comparisons, reports and host observations. A repeat cannot replace failed evidence.
- **Policy trust.** The independently qualified baseline policy and approved judge hash remain mandatory. A candidate's
  policy cannot approve itself. Historical six-run reports remain readable but cannot pass a twelve-block policy merely
  through reclassification. Old/new judge differences remain visible over identical receipt hashes; unexpected
  disagreement is inconclusive. The scope and verdict helpers retain optional ExpectedBaseSha/ExpectedCandidateSha
  equality checks for explicit callers.
- **Open trust boundary.** The study runs from the selected dispatch source; this routing change does not establish a
  separate trusted-base execution boundary or independently attest an event pair. CR-29/Q12 remains with its existing
  owner. CODEOWNERS still names workflow/tooling/performance owners; requiring code-owner review is a maintainer
  ruleset decision. The legacy PR scope/aggregate commands are compatibility entry points and are not current CI gates.

- **Controls of the metric in question.** A shared hosted runner drifts beyond some band in nearly every control, in a few
  metrics (mostly the p95 of frame, preparation and composition times, and the working set), so the check judges the
  controls of the metric a set flagged, not of all twenty-six together. The
  [retained hosted run](../../Measurements/HostedPairedGate/2026-10-01/README.md) records the drift: all 18 controls
  drifted in some metric, and the eight exact budgets in none. A
  [second hosted A/A run](../../Measurements/HostedPairedGate/2026-10-01/aa-2/README.md) flagged five
  clean-phase timings (p 0.004 to 0.026) of the same library code with their controls drifted: the candidate was slow in
  five of its six runs and the baseline in none. Per metric that is inconclusive, not a degradation, so a hosted run can
  need a re-run. The strict reading,
  any unstable control making its scenario inconclusive, is the `-StrictControls` switch of
  `Tools/Publish-BenchmarkVerdict.ps1`; it is off because it would have made every hosted run inconclusive.
  An unstable control means at least one metric in that control crossed its band. Report this control count
  separately from the number of metric observations that crossed bands, with both denominators and per-key counts;
  an unstable-control total does not mean every metric drifted.
- **Identical library inputs.** Matching source or executable hashes are provenance only. Timing/memory flags remain
  unresolved unless a trusted policy and its independent block/A/A evidence support the inference. Exact deterministic
  budgets retain the run-median check and add a separately reported no-tolerance comparison of raw-round maxima. The
  job summary gives an exact budget's change as that of its medians (from zero when the baseline is zero), since the
  paired-block judge gives it no block effect.
- **No verdict.** Invalid evidence, missing provenance, an unqualified policy, a missing/unreadable summary, a summary that
  lists no scenario or uses only an older six-run judge, and a run that did not finish fail with the reason in the job
  summary. When admission withholds every metric verdict, the flagged-metrics section MUST say that no metrics
  were assessed. It MUST NOT infer no regressions or improvements from the empty verdict set. Admitted metric
  rows and all original policy/identity refusal reasons remain visible. A pull request that changes the harness and library interfaces together, whose base cannot be
  built with the merge ref's harness. Its comparison is measured by hand, as before.
- **Confirmed degradation** follows the advice rule above: stop, present the deltas and suspected cause, and never relax a
  band or replace a baseline to pass. The gate has no waiver list: a
  tradeoff the developer approves is recorded in this contract, as the sections below do, and a maintainer merges over the
  failed check.
- **S0-S2 implementation integration, 11 October 2026.** The developer's
  [implementation-only decision](../Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage) establishes no accepted
  performance envelope, policy trust, non-regression or production qualification. Retained timing/memory flags remain
  unresolved in [the qualification WIP](../Plans/Done/ConsumerUiQualification_2026-10-11.md); bands, deterministic budgets
  and the measured-advice rule are unchanged.
- **A manual run** reports the same verdict in its job summary and stays green for a finding, as it always did: only a pull
  request is gated. Hosted runs are serial on one machine but not on a controlled quiet desktop, and their evidence says so.

### Described-menu clean private memory

On 2026-09-27 the user directed a waiver for described native menu entries (#24, merged as `6f769ab`). In three
matched September 21 Release pairs against `78b3de3`
([common-scene investigation](../../Measurements/MenuDescriptions/2026-09-21/README.md)), clean-round median
private bytes had risen by 823,296, 1,175,552 and 1,175,552 bytes (+3.23%, +4.56%, +4.51%). The waiver accepted up to
1,175,552 bytes (+4.6%) on the default x64 Release WARP fixture while an
[optimization plan](../Plans/Done/MenuDescriptionMemory_2026-09-27.md) investigated. The scene opens no menu, and
outside menu popups the change adds no allocation.

On 2026-09-30 the waiver is removed, because the plan's repeated paired runs of `e47c836` against `6f769ab` are within
the bands. A local set of six runs per side
([receipts](../../Measurements/MenuDescriptions/2026-09-30/paired-local/README.md)) measures clean private bytes
+0.36% (p = 0.70) and dirty private bytes -0.89% (p = 0.56). The earlier hosted set measured clean private bytes
-0.50% and -0.70%, and pair three's unaccepted dirty-round +8.67% did not repeat. Described menus now carry no memory
envelope, and the regular investigation bands and exact budgets apply to them.

### Accepted UI Automation elements of menus without descriptions

On 2026-10-02 the developer accepted the measured cost of a UI Automation element for every row of a menu without
descriptions (#56), which lets a screen reader read, focus and invoke an ordinary menu. A popup publishes its rows'
elements whether or not a client listens. Three local sets of six interleaved runs per side, x64 Release, in a
nonactivating harness ([packet](../../Measurements/PlainMenuUia/2026-10-02/README.md)), measured the final code against
`265c280`, with UI Automation clients listening throughout:

- **Memory.** An open menu holds about 1.6 KB more live heap per command row and 1.9 to 2.0 KB per radio row, linear from
  12 to 4,096 rows: +19,967 bytes for twelve commands, +76,287 for 48 and +6.39 MB for 4,096. Closing returns all of it.
- **Keyboard and scrolling.** A key or wheel notch that moves the rows rebuilds the popup's snapshot and raises a focus
  change. Down takes about 0.3 ms more at twelve rows (38 to 340 µs), 0.6 ms more at 128 and 6.1 ms more at 4,096.
- **Opening and pointer.** Opening 4,096 rows takes 77 ms instead of 60. A pointer move costs 4 µs more at twelve rows
  and 236 µs more at 4,096.

The accepted envelope is those recorded costs on that fixture. Described menus already paid them and are unchanged.
The snapshot pre-sizing taken with the change makes every host's snapshot cheaper to build. A larger cost per row, any
cost for a menu that opens no popup, and any change to the complex-UI benchmark remain regressions that need developer
advice. The regular investigation bands and exact budgets still apply to everything else.

### I26 accepted multiline Grid memory tradeoff

On 2026-09-23 the user explicitly accepted the measured V11 multiline Grid process-memory
cost to retain complete-line clipping, omission markers and readable Issues summaries.
The observed fixed-fixture Release median rises from 33,214,464 to 40,226,816 private bytes
(+7,012,352 bytes); paced retention rises from 29,743,104 to 36,831,232 bytes. The accepted
envelope is those recorded approximately 6–7 MiB costs, alongside the measured multiline
offscreen throughput improvement from 61.974 to 122.243 FPS. Preserve the
[original qualification](../../Measurements/GridTextOverflow/2026-09-21/qualification/Release/README.md)
and [heap attribution](../../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md).
Process private bytes are not equivalent to live cache storage; attribution includes native
heap capacity and does not establish a settled long-run bound.

This decision does not accept the rejected associative-cache experiment, separate common-path
timing/memory flags, menu benchmark differences, or additional growth. Existing investigation
bands and deterministic allocation/surface/hidden-work budgets remain unchanged. It neither
updates a consumer pin nor replaces the remaining functional and integrated qualification.

On 2026-09-29 the [review fixes](../Plans/Done/ReviewFixes_2026-09-29.md) stopped drawing multiline cells with
`D2D1_DRAW_TEXT_OPTIONS_CLIP` on their fractional layout box. A local paired set against `40c6c21`
([receipts](../../Measurements/ReviewFixes/2026-09-29/paired-local/README.md)) measured MultilineGrid dirty private
bytes 4.0–5.9 MB lower, dirty peaks about 9.7 MB lower and clean private bytes 5.0–6.9 MB lower, with unchanged
allocations. That removes most of the cost accepted above; the envelope stays until a developer tightens it after a
quiet-fixture repeat.

### Accepted Grid layout retention

On 2026-09-29 the developer set the priority for Grid text layouts: the best frame rate first, then the least memory
for it. That replaces the memory-first rejection of the associative-cache experiment for the Grid. The
[review follow-ups](../Plans/Done/ReviewFollowUps_2026-09-29.md) keep cell layouts in 32-way set-associative tables
(at most 16,384 entries; a table grows instead of evicting an entry the current or previous paint used, and at the ceiling a
full set gives up its least recently used way) and keep single-line captions' layouts as well. Two local paired sets against
the review fixes
([receipts](../../Measurements/ReviewFollowUps/2026-09-29/paired-local/README.md)) record equal or fewer dirty-round
allocations and no clean-round allocation in all three scenes, and a higher `Default` dirty rate in all four crossings
(+5.1% to +14.5%). B's median private bytes averaged -0.08 to +0.52 MB from A's per scene and phase. Every
same-binary control in both sets drifted beyond its band, so neither set alone establishes the gain or the cost.
Pooled (four runs per side) and judged by the paired-set rule, the `Default` dirty rate is 11.3% higher (p = 0.029),
and no memory change is established in any scene (the largest, `Default` dirty private bytes +2.4%, has p = 0.11).

What a painting grid keeps is one layout per cell its last paint drew, about 20 KB each, and the
[reliability follow-ups](../Plans/Done/ReliabilityAndFollowUps_2026-09-30.md) return all of it when the grid stops
painting: hidden (itself, under a hidden ancestor or in a hidden embedded view), detached or given another model. Their
local paired set against `main`, six runs per side in each scene
([receipts](../../Measurements/ReliabilityAndFollowUps/2026-09-30/paired-local/README.md)), keeps the painting path's
allocations identical and establishes no timing or memory change while painting. Where every cell holds its own
layout, hidden private bytes are 0.52 MB lower (p = 0.026); elsewhere the heap keeps the freed blocks committed, and
the Grid suite's layout counters establish the release.

On 2026-09-30 the developer accepted this retention for its frame rate. The accepted envelope is what the Grid
suite's counters and the Release heap walk of the reliability follow-ups record: while it paints, a Grid keeps at most
one text layout per cell its last paint drew, about 20 KB each (about 0.57 MB for a 6x4 multiline grid, 0.93 MB for
ten single-line rows, 1.3 MB for 10x6), with their key strings (6 to 12 KB) and tables (5 to 14 KB), which on the
complex-UI fixtures is the about 0.5 MB of private bytes the 2026-09-29 sets recorded; hidden, detached or given
another model, it keeps none. Layouts beyond the cells of the last paint, any layout kept by a grid that is not
painting, and any rise in the painting path's allocations remain regressions that need developer advice. The
accepted retention is bounded by the cells on screen, not by the model's size.

### Retained text layouts

Plain text that a control draws through its host (`DrawCenteredText` and the host-aware replacements of `DrawText`:
`Button`'s selector chrome, `ThroughputGraph`'s labels, a `Tree` row with a typography override and a single-line field
without its own layout) is drawn with a layout the host retains, not through `DrawText`, which lays its string out on
every call (plan item `W2.1`). The public `DrawButtonChrome`, which has no host, still draws through `DrawText`. The host
keeps one table (`ControlHost::_textLayoutCache`, a registered retained cache) in the set-associative form of the Grid's
cell tables above (32 ways a set; 32 entries growing to 2,048, `kTextLayoutCacheMaxEntries`). An entry is keyed by the whole string (hashed to choose
its set, compared in full on a hit, which allocates nothing), its text format (held by the entry, so a released format's
address can never match) and the exact width and height of the box. Its layout is created with that format at that size
and drawn at the box's origin with the caller's draw options; that draws exactly what `DrawText` draws. A host text
format is immutable once created: the key holds a format by identity and hashes only its size, weight, style, stretch,
alignments, wrapping and reading direction, so a format changed afterwards would be drawn with layouts made before the
change. Debug builds compare, on every hit, the format's trimming, line spacing, flow direction, tab stop, locale and
family with those its layout was made with and report a difference.

Each host paint (a native render, an embedded preparation) is one generation. When it ends, every entry it did not draw
releases its layout and format and keeps only its string storage for later keys, so the table holds the strings of the
last paint and never more. A paint that meets a full set at the bound draws the string through `DrawText` rather than
evict a layout the same paint drew (`text.textLayoutCacheOverflows`), so no paint creates a layout twice. Not retained,
and drawn through `DrawText` as before (`text.textLayoutCacheBypasses`): a draw outside a host paint, a box whose width or
height is not positive and finite, and a string over 4,096 UTF-16 units; an empty string draws nothing. A single-line
field that could not create its own layout draws its text through `DrawText` and never through the table, because a
released entry keeps its string for reuse without wiping it and that text may be a revealed password. After a change of
draw order, the first frame at the bound can lay out up to a set's worth of strings again before it settles, and a table
that halves can drop entries when two merged sets hold more than 32 current entries; either costs only a miss.

The whole table is released:

- by an embedded view when it releases its surface: hidden, prepared at zero extent, device replaced or detached;
- by a native host when `SetNativePresentationVisible(false)`, `WM_SHOWWINDOW` false, `WM_SIZE` minimized or a client
  area of zero width or height reaches it, on device loss or replacement and on detach;
- by a native host whose window or an ancestor stopped showing it without a message (a minimized top-level, a hidden
  parent, `SWP_HIDEWINDOW`) at its next schedule wake, which finds the window not visible. A host with nothing scheduled
  has no such wake and keeps its table until a message or a paint reaches it.

A parent's `WM_SETREDRAW` false sends `WM_SHOWWINDOW` false, so a redraw block releases the table too and the next
paint lays every string out again. A new typography revision (a new DirectWrite factory) releases it at the next paint;
a DPI change keeps it, since layouts are in DIPs. On an embedded view a string new to the table is laid out during
preparation; composition only composites the surface.

The equivalence oracle (`TestRetainedTextLayoutsDrawWhatDrawTextDraws`) draws 1,782 cases through `DrawText` (the
`DebugSetTextLayoutCacheEnabledForTest` seam) and through the table, on the frame that creates the layouts and on the
next, and requires identical BGRA on WARP at 96, 120, 144, 168 and 192 DPI on an embedded view and at the system DPI on a
native window: 12 strings (among them an empty one, tabs, ampersands, colour emoji, CJK, combining marks, Arabic and hard
line breaks), 8 boxes (whole and fractional origins and sizes, boxes narrower and shorter than their text, zero and
negative sizes, and boxes sharing only their width or only their height with another box drawn in the same frame with
the same string and format), every alignment and paragraph alignment, wrapping, both reading directions, `DrawCenteredText`,
clip-only options and a rotated transform. The FX5 frames of the HWND, composition and embedded hosts are byte-identical
with and without the table in both themes.

A retained layout is resident memory that `DrawText` freed after each call: one DirectWrite layout per distinct string
of the last paint: about 18 KB of live heap for a short string (17.8 KB to 18.0 KB per layout returned when FX12's table is
released), about 27 KB of private bytes with the heap's own retention. The registry row reports only the table's entries
and its own bytes (the entries and their key strings, about 147 KB for 1,014 entries); the layouts' storage is
DirectWrite's, about 18 MB of heap for those entries, which the
[W2.1 receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W2.1/README.md) measures.

That cost sets the bound, chosen from the receipt's balance of memory against layout work within the developer's
envelope (2,048 entries at about 50 MB accepted). It is the smallest table that keeps every string of the densest
fixtures that fit a real screen, so an unchanged frame lays nothing out: FX12's 1080p and 4K dense screens draw 1,014
distinct strings, which a 1,024-entry table (32 sets of 32 ways) holds only about 950 of, laying 67 out again every frame;
2,048 entries hold all of them. A host therefore keeps at most about 37 MB of heap (about 55 MB private) for its retained
layouts; the Grid's ceiling of 16,384 would allow about 290 MB. A screen beyond the bound draws its overflow through
`DrawText` each frame (the 10,000-row form, `form-layout-10k`, lays out 374 of its 9,000 strings per frame at 2,048
entries, against 9,000 without the table). The table holds only what the last paint drew, so a sparse screen holds little:
FX13 21 layouts (about 0.4 MB), FX5 34 (about 0.65 MB), FX1 none. Releasing it when a visible host goes idle is
planned with `W9.2`. Text measurement reads the first line's metrics into a 16-line stack buffer, so a `Label`
measure makes no C++ allocation; text with more lines reads them into a heap buffer (`text.lineMetricsHeapFallbacks`).

### Retained drawing

Hosts do not retain a control's drawing as an `ID2D1CommandList`: every visible control a frame paints runs its `Paint`.
The developer decided this on 23 November 2026 from the `W4.0` spike
([receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W4.0/README.md)), which met three of its four go criteria
and failed the one that matters. Replaying a recorded list with `DrawImage` cost 0.85 to 1.07 of painting the control on
hardware (`Label`, `Button`, `CardPanel`, 800 controls at 4K), because Direct2D keeps nothing rasterized for a command
list and submits every recorded primitive again; FX5 frames were not faster with the item's kinds retained and 7 to 8%
slower with every leaf kind retained. On the opaque window-host target `DrawImage` of a list also draws ClearType text
grayscale; streaming the list's commands back into the context (`ID2D1CommandList::Stream`) keeps ClearType exactly but
costs 1.07 to 1.19 of painting. What does remove the Direct2D cost of an unchanged control is not drawing it (0.04 to 0.09
of painting) or drawing a bitmap tile of it (0.24 to 0.53 of painting, about 36 KB per 90×24 DIP control at 192 DPI). The
replacement, plan item `W4.10`, is the [paint region](#paint-region) below: invalidation attributed to the changed control
by its painted extent, and culling of children outside the paint clip. No raster tile is built: every shadow and blur
effect in the library is drawn by popups (`ComboBox` and `Menu`, plan item `W2.4`), and no control in a tree draws one.

Facts the spike established for any later design: a command list keeps brushes by value (a brush changed after
recording does not change the list), and it holds text as glyph runs, not as the `IDWriteTextLayout` that drew them, so a
recorded list replays identically after the layout is released; a list translated by whole device pixels draws text at
exactly the positions `DrawTextLayout` draws at the translated origin.

### Paint region

Plan item `W4.10` ([receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W4.10/README.md)). A host keeps a paint
region: at most eight rectangles in device pixels of its view, each snapped outward to the whole pixels its extent touches
(antialiased coverage never leaves the pixels a shape's geometry touches) at the scale the view is painted at (an embedded
view's own DPI, fractional ones included), in fixed storage, so adding, merging and clearing do no heap work. A rectangle that
touches or overlaps another merges with it; a ninth merges with the stored one whose union grows least; above half the
view, or on any invalidation it cannot attribute, the region is the whole view. What enters it:

- an attributed invalidation (`Assign` with `Affects::Paint`, `Control::Invalidate`, `RequestInvalidate`,
  `InvalidateMeasure`) adds the control's [painted extent](../UI/UI_ControlsAndLayout.md#painted-extent), a panel's with its
  descendants, mapped through its ancestors to the view: translated by each `ScrollPanel`'s offset and clipped to its
  viewport (not for overlay content, a `PopupLayer`'s children or a panel that holds some, which no viewport clips), then
  clipped to the view, so an invalidation of a control scrolled out of view adds nothing;
- `SetBounds` adds the control's extent at its old bounds and at its new ones; a hover change adds the old and the new
  hovered control (the host's, and a `ScrollPanel`'s inner hover), which no control invalidates for, and every ancestor of
  them whose `Paint` draws a hover below it (`TagPicker`, from its inner `ComboBox`); a `ScrollPanel`'s offset change adds
  its overlay content at the old and the new offset;
- the whole view: `ControlHost::Invalidate` (an application's, and every host-level change: theme, DPI, typography, text
  rendering, focus visibility, tooltip, default button), a tree change (a control joins, leaves or moves), an undeclared or
  unbounded extent anywhere on the control's ancestor chain, a `ComboBox` popup closing and a page transition ending.

An embedded preparation repaints only the region when its surface still holds the previous coherent frame at the same
size, DPI, text rendering and device: for each rectangle, an aliased axis-aligned clip, a clear to premultiplied
`(0, 0, 0, 0)`, and the tree, its overlays and the tooltip painted under that clip (the root and every panel run their
`Paint`; panels skip the children that miss the clip). Anything else, and a preparation in which text newly escaped its
control, repaints the whole surface in that same preparation. `Composite` is unchanged: it draws the whole surface and
does no heap work. The `frame.pixelsPainted` counter and the trace provider's prepare-end pixel count report the region's
area, so they no longer equal the surface for a partial preparation. A preparation that does not complete leaves the next
one the whole view.

The window host keeps painting whole frames (or the system's damage alone) until it presents the region (plan item `W4.9`):
its children are culled against the window or the damage, and the region its invalidations build is discarded with each
render.

Caches that keep what the last paint drew keep, after a region frame, what the last whole paint drew: a frame that paints
a region (embedded) or the system's damage alone (native) starts no new generation of the host's text layout table and no
new cache frame of its brush and format caches, and releases or trims nothing, so the entries of controls outside it stay;
a whole frame releases what it did not draw, a control culled by a viewport included. The cached painted extent of a panel
(`Panel._paintExtent`) is a registered retained cache; a tree change invalidates every one of a host's at once, and a change
of a control's bounds, visibility or reach those of the panels above it. A `Paint` that asks for its next animation frame
or resumes a transition (`Button`'s disclosure, `ProgressBar`'s indeterminate sweep, `Grid`'s animated cells, `PageHost`)
does so only as a resume after a hidden period: a request held by a visible control keeps ticking whether or not the
control is painted, its ticks invalidate it, and those invalidations bring it into the next region. No library `Paint`
stores geometry that hit testing or accessibility reads.

The definition of correct is a frame painted whole with regions and culling off: a frame painted with them equals it, pixel
for pixel on WARP. The Debug test programs check every embedded region or culled frame against that reference as it is
made (the frame verifier, [Testing_Validation.md](../Testing/Testing_Validation.md#frame-verifier)), and the
incremental-versus-fresh oracle paints its fresh scene with regions and culling off on both hosts.

The switch `DxUi::Detail::SetPaintRegionsEnabledForTest` (private, present in every configuration; `DxUi.EmbeddedTests.exe`
reads `DXUI_PAINT_REGIONS=0`, which `performance-timing.ps1 -CandidateEnvironment` sets for one role) turns regions and culling off, which repaints whole frames as before, for same-binary
comparisons.

### I19 accepted resource trade-off

On 2026-09-13 the user accepted the measured static-library adoption trade-off and
requested closeout. The [matched library record](../../Measurements/SharedLibrary/2026-09-13-local/README.md)
retains all original comparisons, including investigation-band failures and baseline variation.
The accepted envelope is the observed I19 result on that fixed fixture: Release clean peak
private memory increases by a median 731,136 bytes, while surface storage, composition
allocations and hidden work remain unchanged. Release dirty completed throughput changes
by +0.02%; Debug dirty throughput changes by -1.65%. This is an explicit acceptance of
the recorded costs for canonical source ownership and the clipboard, module-lifetime and
font-refresh corrections, not a claim that every metric is unchanged.

The existing deterministic budgets and investigation bands remain in force. This decision
does not admit additional growth in later revisions or replace the retained baseline.
It covers the named offscreen fixture only; hardware presentation and long-run retention
still require their own evidence. Consumer measurements and their accepted costs remain
in the corresponding product repositories.

### Ongoing validation and resource budgets

Every retained cache (state derived from model, theme, DPI, typography or device state and rebuilt on demand) MUST be a row
of the retained-cache registry (`src/Controls/DxUi.RetainedCaches.cpp`) with an entries count and a drop, or an exemption
with its reason, and a change that adds retention, partial frames or skipped paint MUST pass the incremental-versus-fresh
oracle of the [validation contract](../Testing/Testing_Validation.md#incremental-versus-fresh-oracle). Counters cannot see
a stale pixel; the oracle is the evidence that a cache is correct, not only that it is cheap.

Canonical benchmark output keeps its fully qualified logical path for evidence ownership,
receipt reads, comparisons and diagnostics. Only the Windows native benchmark argument uses
an extended-length spelling: preserve an existing `\\?\` prefix, map ordinary UNC to
`\\?\UNC\`, and prefix absolute drive paths with `\\?\`. Do not shorten or relocate
run evidence to fit MAX_PATH. Before/after v2 attestation and native failure propagation remain
required; path transport does not qualify an unpaired measurement.

Every `test.ps1` invocation MUST report complex-UI FPS and memory, including filtered suites, and each suite receipt
must include the measurement or its linked receipt. A failing/missing benchmark fails the test entrypoint. Unit-test
execution rate and CPU command-submission rate must never be labeled rendered FPS. Hidden/static idle views have no
requested frames (report zero work rather than an artificial FPS loop). The default executable fixture measures
1280x720/96-DPI completed offscreen WARP frames with 83 controls, 1,000-row Grid/Tree models, 20 warm-up frames and
five 40-frame rounds each for clean and changing content. Dirty work updates sliders/progress and scrolls the grid.
One reusable staging pixel synchronizes GPU completion outside production code; report its cost in total FPS.
Record p50/p95 frame time, p95 preparation/CPU composition, C++ allocations, exact surface bytes/replacement peak,
process private bytes/working set and sampled peaks/growth. Composition allocation and extra surface creation are
hard failures, and hidden preparation/composition must remain zero. Whole-frame C++ allocations are gated as well:
clean rounds must record zero, and dirty rounds may not exceed 64 allocations per frame in Release (2,560 per
40-frame round; 54 per frame measured on 2026-09-07) or 320 per frame in Debug (12,800 per round; 257 measured),
where the Debug STL allocates one container proxy per std::vector/std::wstring. The receipt records
`dirtyAllocationCeilingPerFrame`. A ceiling is never raised to pass; a failing gate reports the measured count for
advice. The benchmark does not establish displayed FPS.

New complex-UI receipts retain all forty frame, preparation and CPU composition times per round in frame order,
plus the `std::chrono::steady_clock` implementation, QPC frequency and counter-tick duration. Report the clock's
nominal C++ period separately; it is not the hardware counter's resolution. Bounded stack copies and JSON output
occur outside measured/allocation-counted frames. These diagnostic fields do not add judged metrics or change the
existing percentiles, thirteen metrics, 26-slot family, investigation bands or exact budgets. Receipt validation
checks complete finite samples and reproduces the reported FPS/percentiles with tolerance only for JSON rounding.
Paired receipts must use the same clock metadata. Historical receipts without either diagnostic group remain
readable under their original harness; partial new groups are invalid. A no-op preparation taking a few counter
ticks warrants quantization analysis, not an automatic noise waiver or a silently batched replacement metric.
WindowHost's `dxui.frame.update_us` records the complete Update-stage duration on every exit, including resource
failure and deferred preparation; incomplete frames may report zero Render/Present duration, but must not leave
the elapsed Update duration at its default zero.

In historical complex-UI receipts, `composeCpuP95Ms` is the elapsed steady-clock/QPC duration around Composite,
not thread CPU time; preserve the field name for compatibility and interpret the value as wall duration. The
whole-frame/FPS bracket also includes dirty Update and the blocking one-pixel WARP readback. Report QPC frequency
and counter-tick duration with few-tick no-op preparation tails; those observations are floor-sensitive and do
not justify changing the metric, band, or sampling protocol.

Benchmark receipts also record process-memory phases at entry, device creation, scene
creation, warm-up, screenshot encoding and the hidden state. These untimed samples help
locate changes; they do not replace matched frame/retention comparisons. The opt-in
`dxui-complex-ui-profiling-v1` fixture adds fixed warm-up checkpoints and bounded phase-boundary CPU/thread/cache
observations on the same synthetic scene. Its heap-off/on profiles record sampling duration and query errors;
heap inventory bounds do not bound traversal time. Worker identities include thread creation time and verified
opened-handle process ownership, not only reusable thread IDs. Cumulative CPU and cycle counters describe consumption,
while priority/affinity describe constraints;
they MUST NOT be reported as actual placement or scheduler-delay attribution. Diagnostic warm-up and probe intervals
are distinct from the canonical workload. Retain every scheduled process, outlier, unavailable field and partial
failure; enforce an owned-child timeout and a declared overall pilot budget. Exact before/after provenance and native
byte retention apply to these reports too. This diagnostic supplies no policy or regression acceptance verdict.
Sampling outside timed frames can still change later backend/cache readiness. The diagnostic MUST report total
sampling duration and the optional heap sampling block separately; the heap block includes enumeration, traversal
and JSON construction/parsing. A subtraction is a derived non-heap residual, not an independently timed causal
effect. Differing profiles MUST NOT imply convergence or isolate a heap observer effect. Coarse cumulative CPU times may yield zero deltas during real work; available
cycle deltas MUST remain raw consumption counters rather than fabricated time or scheduler-delay estimates.

The [source-command Default investigation](../../Measurements/RichText/README.md)
retains 48 matching-profile interleaved reports and eight diagnostic profiles.
Its paired clean-p95 effect is -1.31%, with no judged regression, but all 24
controls flag drift somewhere and 13 cross the clean-p95 band. A within-noise
judgment under the unqualified policy does not establish non-regression or close
acceptance. The measured tail's combined bind/clear/copy/readback/other-overhead
remainder is an attribution boundary, not a separately measured scheduler or
GPU interval. Original warning/refusal receipts and the full-scene gates remain
unchanged; this focused record changes no metric, threshold, judge or policy.

`dxui-complex-ui-boundaries-v1` is a separate Default-only diagnostic, selected
by `performance-profile.ps1 -BoundaryTimers Off|On`. It uses 200 dirty warm frames,
a populated capture, and eight 40-frame clean windows: full, clear-only,
composite-only, readback-only, then reverse order. All windows prepare a clean
view, bind the target and complete a one-pixel copy/Map/Unmap. Interior timers
record absolute QPC edges after preparation, binding, clear, composition, copy,
blocking Map and Unmap; Off retains only frame start/end. Fixed frame storage,
zero C++ allocations, surface reuse and hidden zero-work are checked natively.
The guarded runner validates complete ordered windows/frames/edges, CPU query
availability, clock units, PID/thread creation identity and the diagnostic source
closure, retaining the original bytes and owned-child bounds. Compare fresh
timers-off/on processes to expose observer effects. Removing commands changes
backend scheduling and dependencies: neither subtraction across controls nor a
blocking Map interval is a GPU-duration or scheduler-delay measurement. Scheduler
attribution requires an actual aligned trace and its loss/completeness checks;
a permission refusal leaves that gate open. This diagnostic changes no acceptance
workload, judge, policy, threshold or historical receipt.

The explicitly selected `dxui-complex-ui-boundaries-v3` diagnostic adds
`performance-profile.ps1 -BoundaryTimers Off|On -BoundaryScenario Default|MultilineGrid|MultilineGridDistinct
-BoundaryPhase Clean|Dirty`. It retains the same 200 dirty warm frames, populated capture and eight forward/reverse
command-control windows. `BoundaryScenario` is separate from the existing startup-profile `Scenario` argument;
`BoundaryPhase` defaults to Clean and Dirty requires an explicit boundary scenario. Each dirty window updates the
same model frames 200 through 239, allowing order reversals without silently advancing to different input ranges.
The distinct model retains short Tree names and four different multiline values per row, matching the canonical
scene's model setup. These diagnostic warm-up/phase sequences remain distinct from canonical acceptance.

V3 preserves absolute frame edges and, with timers On, adds the end-of-Update edge before Prepare. Attribute the
reported tail using every subinterval from that same ranked frame, never independently ranked segment tails.
Each frame additionally retains the owned main thread's cumulative kernel/user time in 100ns units and raw cycle
counts before/after it, with separate QPC brackets around both query probes. The CPU-counter interval surrounds
the timed frame and includes query overhead; neither the CPU-counter difference nor the probe cost establishes
isolated frame CPU time, worker consumption, scheduler delay or GPU duration. Coarse time deltas of zero remain
valid, and cycles are not converted into invented elapsed time. Queries that fail remain explicit and make the
report partial. Timers Off leaves all interior/update edges zero but retains CPU probes; it does not isolate
their observer effect. V1/V2 remain separate historical schemas with no per-frame owner CPU samples.
All windows require zero composition allocations, unchanged surfaces and matching preparation counts; clean
whole-frame allocations remain zero and dirty allocations retain the existing profile-specific ceiling.
No canonical benchmark input, investigation band, judged metric, baseline or policy changes with this diagnostic.

Use `updateEndQpc` to start the pure Prepare interval in both phases. In Clean, the preceding start-to-Update-end
interval contains marker/no-op work and is not an Update call. Disabled Clear/Compose intervals likewise retain
branch/marker wall costs rather than zero-cost operation measurements. A window's p95 frame and all its component
intervals must come from that same chronological rank. The [34-process diagnostic packet](../../Measurements/ControlDiagnostics_2026-10-28/README.md)
retains two fresh processes per factor cell and separate smokes; repeated windows are not additional independent
replicates. Its observed blocking-readback costs and slower CallingThread mode do not qualify the canonical
controls, explain scheduling, approve policy or replace any historical acceptance outcome.

The [external thread-state controls](../../Measurements/RichText/README.md)
verify owned-child main-thread identity and a common native/managed QPC clock.
WaitReason is read only for Wait. Require the entire acquisition bracket inside
a frame/segment before assigning a sampled state; retain mixed/outside samples,
errors and truncation separately. These samples are unweighted observations,
not occupancy, ready latency, scheduler events or GPU time. In the retained study,
all 35 brackets exceed/cross native frame boundaries: 17–25 ms query p95 cannot
resolve 0.45–0.57 ms frame p95. Empty Map histograms mean unavailable evidence.
Same-binary observer controls do not establish stable overhead or close HOLD.
Explicit `-WarpRasterMode Default|CallingThread` with `-BoundaryTimers` selects
`dxui-complex-ui-boundaries-v2`. Its requested and actual device flags must match
the selected policy; an absent/contradictory policy cannot be accepted as v1.
CallingThread uses the documented WARP profiling flag, with no canonical/library
device default change. Omitting the option preserves the v1 command/report. This
intervention changes work placement/runtime behavior and cannot by itself measure
  scheduler latency. Device-reported flags prove the accepted request, not actual
  application-main-thread execution. Keep native qualification and raw results in
  the rich-text plan; the observed CallingThread runs remain Map dominated.

The shared Unicode 17 grapheme walker retains constant numeric state and requests
no C++ heap storage. Prepared rich navigation and password mask indexing walk
established boundaries forward once; arbitrary-caret queries may recover context
from the beginning. The 49,152-unit mixed-script resource fixture requires exactly
16,384 elements and zero C++ allocation calls/bytes. This is a deterministic
resource bound, not a latency or visual bidi qualification.

Bulk UIA Character movement is linear in source length, including extreme signed
counts: one nearest-boundary recovery, forward traversal or a prefix count plus
target traversal, with at most three forward passes and constant numeric state.
Collapsed deletion uses a single forward pass without a prepared index, or
O(log G) lookup in the prepared layout's G grapheme stops. Neither operation
requests C++ heap storage. The 49,152-unit mixed-script fixture requires actual
movement of +/-16,384 elements and zero allocation calls/bytes, including both
indexed and streaming deletion queries. Input/index construction is outside the
measured query budget. Fixed Release before/after traversal trials are descriptive
observations; they do not resolve the independent Default readback HOLD.

DecodeUtf8 rejects byte lengths above three times the 65,536 UTF-16-unit capacity
before conversion, and checks the decoded length before allocating its result.
Every admitted Unicode scalar needs at most three UTF-8 bytes per UTF-16 unit.
The 65,537-byte ASCII and 196,609-byte impossible-input rejection fixtures permit
at most 4 KiB of C++ allocation requests, excluding caller-owned input. Exact
three-byte BMP and supplementary-scalar limits must still decode. These bounds
do not raise capacity or claim to measure Windows/CRT-internal allocations.

RichDocument's bounded immutable snapshots use a persistent balanced piece tree
for O(log P) offset lookup and O(log P + K) traversal of K intersecting pieces.
Immutable nodes use cumulative piece/child ends, fanout at most 32 and at least
16 entries outside the root. Edits copy boundary paths and retain untouched
subtrees/buffers; fixed staging arrays handle split/merge without temporary heap
containers. Reads retain no mutable lookup cache. Text/projection/layout still
materialize the complete bounded source.
A fixed 8,192-unit seed plus 4,096 interleaved single-unit edits, including the
capped 128-entry history, must retain at most 4 MiB of tracked C++ storage and
release that tracked delta on destruction. A caller-prepared same-content
replacement must request zero C++ allocations/bytes and preserve both histories.
These measure the C++ override, not Windows/CRT-internal storage. Sweep every
observed allocation point of changed source/history operations to verify failure
atomicity. Retain before failures and fixed edit/read/single-step timings.
Source-change metadata must retain numeric edit pages without a source-root or
page-parent chain. At most three sealed 32-edit pages accompany the active page;
bounded queries use stack storage and request zero C++ bytes. The 512-query /
64-retired-preview fixture must remain tracked and allocation-free. A separate
8,192-unit / 4,096-preview fixture must retain at most 256 KiB of tracked C++
storage, with no undo/redo and zero tracked delta after destruction. The existing
4 MiB fragmented-source/history limit remains fixed. Sweep page-rollover failures
and retain metadata overhead/timing observations without changing acceptance policy.
PreparedProjection must reuse immutable unchanged outer line blocks and avoid
whole-source flattening during tracked edits. Its source/display prefix rebuild
is linear in bounded physical-line count. Four complete mapping sweeps over the
512-line fixture, including unchanged preparation, must request zero C++ bytes.
The 1,024-line / 512-edit projection+source/history fixture retains at most 4 MiB;
the 65,536-newline and 32,768-unique-line maximum-source fixtures each retain at
most 16 MiB. Every fixture releases its tracked delta to zero. Share adjacent
equal line payloads. Exercise the changed preparation's genuine checked refusal
seams and prove their documented prior-state preservation and permitted retry; exercise unexpected C++
allocation failures in isolated fatal children. Measure
initial and edit preparation separately, retain full-projection comparisons and
original failures; these supporting-core checks do not qualify retained geometry,
shared scheduling, composition or completed-frame performance acceptance.
The fixed fragmented-lookup trials are descriptive test diagnostics, separate from
the canonical completed-frame fixture and its unresolved acceptance policy.

The opt-in
`MenuResources` control suite records 96 open/render/capture/close cycles for twelve
plain or described entries at matched 456-by-300-DIP viewport constraints, including
private/working-set bytes and process/GDI/USER handles. Report screenshot-buffer cost
separately from the open product menu; a short cycling run alone cannot establish a
long-run retention bound. The identical probe can characterize plain menus on the old
implementation and reports described mode as unsupported there.
The benchmark executable's opt-in `--benchmark-retention <output-prefix>` repeats
sixty complete create/render/hide/destroy cycles in one process, retaining each inner
report. Bind that diagnostic's executable/source/fixture hashes and keep its raw rounds;
it does not replace the default acceptance comparison or a controlled long-duration soak.
The executable's opt-in `--benchmark-grid-selection <report.json> [parts]` (fixture `dxui-grid-selection-v2`; `parts` names
some of paint, selectionCost, membership, retention, mutators, preserve and complexUiScene, comma separated, and defaults to
all) measures Grid selection with synthetic data: Grids of 1,000 to 1,000,000 rows, selected as Ctrl+A selects them and painted
offscreen on WARP, what `IsSelected` costs inside such a paint (one Grid painted alternately with a small and a full selection
that draw alike), `IsSelected` per call over selections of 0 to 1,000,000 ids with questions in an order a processor learns
and in a random order it cannot (a binary search is only as cheap as the first), the C++ heap bytes a selection model holds
after Ctrl+A and after each way back from it (Clear, a click, a Shift+click, a data change), counted exactly by the executable's
allocation hook, the time and C++ heap bytes of the selection model's mutators, and `PreserveOrdered` over 200,000 rows with a
few to 5,000 clicked. It also reports how many rows the default complex-UI scene's Grid holds selected. Its entry is dispatched
outside `Embedded.Tests.BenchmarkMain.h`, so the complex-UI fixture's hashed inputs are unchanged. Compare it only between builds of one
harness, as an interleaved paired set (a record of three builds runs them as A, B, C, C, B, A); it supplements the default
benchmark for a change to selection and does not replace it.

Shipping/consumer acceptance additionally requires a named hardware fixture and actual presented complex-UI FPS,
frame pacing and p50/p95/p99 latency at the target refresh rate (at least 60 FPS / 16.67 ms per frame for a 60 Hz
consumer while actively updating). Include 96/144/192 DPI, large data sets, typing/dragging, animation, scrolling,
multiple simultaneous views, resize/DPI transitions, allocation failure and device recovery. Record workload/driver
and CPU/GPU/total frame costs separately. WARP coverage and a clean composite alone cannot satisfy these gates.
Run retention/soak checks long enough to distinguish bounded warm caches from sustained growth; report samples,
duration, maximum views/data and start/steady/peak/end resources. Do not claim a hardware or long-run pass from
the short default benchmark. Match new development to a targeted measured workload as well as the common fixture.

Optional heap-attribution fixtures may enumerate and walk process heaps outside timed rounds,
with bounded storage and individual heap locks. Report per-heap failures and distinguish busy/free
heap blocks from process-private memory and working set; their totals are not interchangeable.
Do not purge heaps or trim working sets to hide retention. A separately identified fixture may
pace retention frames to compare wall-clock allocation rates, but must leave measured FPS rounds
unpaced and must not replace ordinary unpaced performance/resource acceptance. Diagnostic timing
includes sampling overhead. [The grid investigation](../../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md)
records the implemented opt-in diagnostics and their unresolved conclusions.

### Format and brush caches

Each host keeps a map of configured text formats (role, size, weight, alignment, wrapping and reading direction) and a
map of solid brushes keyed by exact color. Every entry carries the host frame that last asked for it. A frame is one
`TrimCaches` call: one native render pass (`Render`, which also serves `DebugCaptureBitmap`) or one embedded preparation
that paints; a host that paints twice in one message has two frames, and a hidden host, which neither renders nor
prepares, has none, so its stamps stand still and nothing is trimmed until it paints again. A request between frames
(measurement, hit testing) counts as a use by the latest frame. At the start of a frame a map above its bound (256
formats, 256 brushes) releases only the entries neither of the last two frames used, as `FindTextLayoutEntry` treats
text layouts. The bound is soft for entries in use: they stay, so a working set above the bound creates nothing after
its first frame and never thrashes; such a trim counts `text.formatCacheOverBound`. Evictions count
`text.formatEvictions` (with `text.formatCacheTrims` per trim that evicted) and `frame.brushEvictions`.

Eviction never forces visual re-preparation. Every key a control retains beyond the call that obtained a format holds
the format by `com_ptr` (single-line text layouts of `TextField` and `ComboBox`, the `MenuBar` layout, `Tree` badge
widths and tooltip overflow, `Button` icon layouts, `TabControl` title widths, `Grid` cell layouts), so an evicted
format stays alive for its holder and its address cannot be reused by another format; a holder whose key no longer
matches the map's current format prepares once again. A retained layout keyed without a format (a `Label` typography
layout) stays valid because a configured format is a pure function of its key and the host's typography revision.
Raw format and brush pointers are borrowed only within the call that obtained them.

A color transition in progress (a `Tree` row's fade, a `Button`, `Toggle`, `Checkbox` or `RadioButton` hover or focus
blend) paints under a `TransitionBrushScope`: a color the keyed map does not hold is then drawn through the host's
transition brushes instead of becoming a keyed entry. Each such request in a frame takes a brush no earlier request of
that frame took and sets its color with `SetColor`; the next frame reuses the same brushes (`frame.brushColorReuses`),
so a running transition creates no brush after its first frame and the keyed map stops growing by one brush per
animated color. Brushes are keyed by color quantized to 8 bits per channel, and a key draws the exact color it was
first asked for with, whoever asks for it later; a key only transitions drew keeps that color without a brush (evicted
by last use like any entry), so a transition and a steady paint of one key draw identical pixels. Because no transition brush is set twice within a frame, a brush that a deferred Direct2D path records
(the shadow mask command list, a layer, a text layout drawing effect) keeps its color until the frame ends. A keyed
brush is never given another color; code that sets its own colors before each draw (`DrawButtonChrome`) takes a
transition brush. Nothing may retain a transition brush beyond the frame; drawing retained across frames (a command
list kept between frames) uses keyed brushes, which a control that keeps one holds by `com_ptr` (as the rich editor's
callout, properties and diagram label brushes are). At most 64 transition brushes exist per host; beyond them requests
use the keyed map.

Both maps are dropped exactly where they were before: a detached host keeps neither; a device loss, a shared device
generation change, a theme change and an embedded device replacement release every brush (keyed and transition); DPI
and text rendering parameter changes keep both, because formats are device-independent and brushes do not depend on
DPI. The incremental-versus-fresh oracle drops both maps.

### Text field layouts

A multiline `TextField` lays out its display text once per change of that text, its text rectangle's width (and height,
above the 32,768-DIP layout height), its font role or its configured format (a typography revision), and keeps that one
layout with its line metrics. Paint (text, selection and caret), caret and range geometry for text input and UI
Automation, hit-testing (clicks, drags, double-clicks, text-input points), wheel scrolling, the arrow, Home, End, PageUp
and PageDown keys, bounds changes, read-only imports and keeping the caret in view all read that layout. The key is the
identity of the display buffer (observed, never owned, so an edit in place is not forced to copy), its length, the exact
width and height, the role and the held format. The key cannot see an edit in place that keeps the buffer and its length:
undo and redo apply their stored span to the buffer in place (`W9.3`), so undoing or redoing a replacement of equal length
keeps both, as does any edit by the buffer's sole owner. Every edit therefore also marks the layout dirty
(`NotifyChanged`, imports, `RestoreEditHistoryState`, `SetText`), and freshness rests on that flag together with the key,
not on the key alone; font role, padding, density, DPI, multiline, masking and reveal
changes mark it dirty too. A move that keeps the text rectangle's width and layout height keeps the layout. A composition preview is imported into the field's text, so the layout holds the composed
text and the composition underlines are drawn over it. A masked field's mask is one retained buffer per dot count, so its layout is reused too; the
navigation keys of a masked multiline field lay out the field's own text without retaining it, as before. The fallback
line height (the height of "Ag") is measured only when no layout can be made, once per held format. With unchanged text and
width a focused paint, a wheel step that does not complete a notch and a PageDown create no text layout (`FX1`, `W2.7`).

A single-line `TextField` (and `ComboBox`) layout that is reused neither clears nor copies its key text. Its copy of the
display text is wiped (zeroed, then cleared) when a masked field drops or replaces it, including the first drop after the
field is unmasked or its password hidden, and on destruction; an unmasked field clears it without the wipe. The multiline
layout keeps no copy of the text: the single-line key is the only copy of a field's text a layout cache keeps, and a
concealed masked field's key holds its dots, never its plaintext. DirectWrite keeps its own copy inside a layout object,
which DxUi cannot wipe; a field releases its layouts (single-line and multiline) the moment it is masked or its revealed
password is hidden again, so a layout of the plaintext does not outlive the reveal. A paint reads the native text-input
state only while a composition is active, once for both underlines.

Outside this rule, and not wiped: the text-input state copies of a whole document into a plain string, made by
`ControlHost::TryReadNativeTextInputState` (a paint while composing, UI Automation, `DebugGetSingleLinePaintState`),
`TextField::ExportTextInputState` and the IMM composition path; and the embedded `ApplyTextInput` preview, which accepts a
masked field, so a composing masked field's paint copies its plaintext that way.

### Memory metrics

Every [performance fixture](../Testing/Testing_Validation.md#performance-fixtures) receipt (`fixture`
`dxui-performance-fixture-v1`) carries these memory fields under `memory`; the FX5 receipt is what later memory budgets
are judged with. It is written by the fixture framework, not by the timed benchmark, whose inputs a paired run compiles
against older libraries: every Embedded run writes the WARP receipt of each built fixture to
`.build/test-artifacts/performance-fixtures/`, `performance-fixtures.ps1` writes retained receipts, and
`DxUi.EmbeddedTests.exe --memory-receipt <report.json>` or `--memory-receipt-hardware <report.json>` writes the FX5 receipt
on demand, the latter on the default hardware adapter. Samples are untimed, each after an immediate-context flush: device,
built, after each phase (for FX5: 20 warm, 40 clean, 40 dirty and 20 hover frames) and hidden; the last phase sample is the shown
state. (The first FX5 receipt, `dxui-complex-ui-memory-v1` under W0.3, sampled 200 scrolling frames instead.)
Counted fields are exact and deterministic on one build; private bytes and working set remain advisory bands and never
become pass/fail gates.

- `trackedLiveBytes`: the bytes the counting `operator new` and the checked control heap hold, from the built model
  onwards, at each sample. Aligned `operator new` forms are not tracked.
- `retained` per phase: the view's `EmbeddedStatistics` surface and cache fields, including the retained text layouts,
  the UTF-16 units they shape, and retained bitmaps with their pixel bytes. DirectWrite and Direct2D keep object
  storage privately, so per-layout, per-brush and per-format native bytes are not observable; the shaped units are
  their size driver, and the hidden residency delta attributes what hiding returns.
- Video memory: on a hardware adapter, each phase's `IDXGIAdapter3::QueryVideoMemoryInfo` local and non-local usage
  and budget for the process. On a software adapter (WARP) `videoMemory.applicable` is false: rendering uses system
  memory, which private bytes already count.
- `hiddenResidencyDelta`: hidden minus shown surface bytes, retained layouts and bitmap bytes, private bytes, working set
  and, on hardware, local and non-local video memory. An embedded view is minimized by hiding it, so
  `minimizedResidency` is the hidden residency; native-window minimized residency belongs to the native fixtures.
- `controlBytes`: `sizeof` of `Control`, `Label`, `Button`, `Toggle`, `Checkbox`, `RadioButton` and
  `ThroughputGraphSample`. Compile-time probes hold each at or below its measured size, separately for Debug STL and
  Release layouts (x64 and ARM64 measure the same): the public types in `Tests/Embedded/Embedded.Tests.MemoryMetrics.h`,
  the UI Automation record `AccessibilityControlNavigationSnapshot` beside its private definition in
  `src/Controls/DxUi.Accessibility.cpp`. A probe is a ratchet against growth, not a budget: a growth fails the build
  until memory is recovered or the probe is re-measured with its reason; each names the `G13` line it serves and the
  item that lowers it.
- `retainedCaches`: every row of the retained-cache registry (`src/Controls/DxUi.RetainedCaches.cpp`) after the last
  phase, with its live entries and resident bytes; `EmbeddedStatistics::retainedCacheEntries` and `retainedCacheBytes`
  are their totals, the surface excluded. Resident bytes are the heap storage DxUi holds for a cache: container element
  storage by capacity (an unordered map by the MSVC node layout), heap strings and bitmap pixels. A row without a byte
  count (a `RichTextEditor`'s prepared layout) reports `null`; DirectWrite and Direct2D object storage is never included.
  `largestTextHistoryBytes` is the undo and redo storage of the editor that holds the most, with the text buffers its
  entries keep beyond the current text.

#### G13 budget lines

Every fixture receipt reports each line of the memory budget `G13` under `budgetLines.G13`: the counter, the measured
bytes, the limit and a status of `met`, `recorded exception`, `advisory` or `not measured` with its reason. The lines
and their counters:

| Line | Budget | Counter |
| --- | --- | --- |
| `idle-private` | idle native window 8 MB private above process entry at 1080p | private bytes after the last phase minus at process entry; advisory |
| `minimized-video` | 8 MiB video memory when minimized | the process's local video usage, hidden (hardware only) |
| `per-control` | 912 B per control all-in on the `FX12` 10,000-control form (restated on 24 November 2026 from 256 B per simple control) | tracked live bytes after the build per control of the built tree |
| `per-grid-cell` | 1 KB per visible single-line `Grid` cell | live heap bytes that rebuilding the cell layouts adds after hiding every `Grid` released them, per layout |
| `text-history` | 4 MiB text history per editor | `largestTextHistoryBytes` after the last phase (TextField and ComboBox; a fixture whose only editors are RichTextEditors reports "not measured", their piece-shared history having no byte count yet) |

The `per-control` line has one limit, 912 B, and every fixture is judged against it, not only `FX12`. A fixture over it
holds few controls, so the host attachment and data that is no control's own state (documents, item lists, models)
divide over few; each such fixture records an exception whose finding gives its measured bytes per control and what is
shared. A fixture that meets the line carries no exception.

The cell line is measured by the receipt's Grid cell probe: the busy blocks of every process heap before and after
hiding each visible `Grid`, and again after showing and rendering it, which rebuilds the cell layouts. The rebuild is
the counter, the one line that includes DirectWrite's own layout storage; hiding returns far less (both are recorded
under `gridCellRelease`), because the released storage stays busy in the process heaps. The line is judged on hardware
receipts only: on a software adapter (WARP) the rasterizer's threads allocate in the same heaps while the cells are
rebuilt, so the line reports "not measured" there, as it does when the process heaps do not grow (AddressSanitizer). Private bytes and working set stay advisory, compared as paired bands (same-binary drift is about 0.3 MB),
never judged. A counted line over its budget MUST be a recorded exception that names the item closing it
(`kG13Exceptions` in `Tests/Embedded/Embedded.Tests.PerformanceFixture.h`); an exception on a line within budget fails as
well, so the item that closes a line deletes its exception and the gate then enforces the budget. The FX5 receipt and the
Embedded suite apply this gate; the current exceptions and measurements are in
[the W9.1 receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W9.1/README.md).

#### Control object allocation

`Control::operator new` and `operator delete` (`src/Foundation/ControlAllocation.cpp`) take every control object's storage
from the process heap. A block belongs to the overload pair that allocated it, so `delete` needs no tag:

- Default alignment (`new` and `new (std::nothrow)`): one `HeapAlloc` of the object size plus a 16-byte prefix
  `{requested, accounted}` immediately below the returned pointer. `HeapAlloc` returns blocks aligned to
  `MEMORY_ALLOCATION_ALIGNMENT` (16 bytes on x64 and ARM64), so the object keeps `__STDCPP_DEFAULT_NEW_ALIGNMENT__` with no
  back-pointer and no alignment slack; both are checked at compile time, and a Debug build stops on a misaligned block. The
  per-allocation overhead is 16 bytes. Sized, array and other `operator delete` forms are not declared: a control is
  never deleted through them.
- Over-aligned (`new (std::align_val_t)` and its `nothrow` form, used for any control type with a larger alignment): the
  24-byte header `{storage, requested, accounted}` below an address rounded up inside a block with the alignment's slack.
  Its `delete (void*, std::align_val_t)` forms read that header.
- `HeapSize` is read only when `Diagnostics::nativeAllocationSink` is set on the allocating thread: `accounted` is then the
  heap's block size, which includes the prefix, and the sink receives `{allocating, requested, accounted}`. With no sink the
  prefix records `accounted` as 0: the block was not reported. `delete` reports a block (the same `requested` and
  `accounted`, `allocating` false) only when `accounted` is not 0 and a sink is set on the freeing thread, so a block
  allocated before the sink was installed and freed after is never reported as a free without its allocation, and the totals
  a sink keeps (live bytes and live count) stay exact whenever the sink is installed at both ends of a block's life. The
  counter `memory.controlHeapSizeReads` counts the `HeapSize` calls.

#### Single-line text history

A `TextField` and an editable `ComboBox` keep undo and redo by bytes, like the rich document's history above
(`src/Controls/DxUi.TextEditHistory.h`). What one undo step is does not change: a typing run, a Backspace run and a
Delete run of a `TextField` are each one step until the caret moves or another kind of edit starts; every other edit (a
paste, a cut, a word deletion, a replacement, an IME commit) is one step, and an editable `ComboBox` makes each typed
character one step; a selection change, a composition preview or its cancellation records none, and `SetText` clears
both histories. Undo restores the text, caret, selection and scroll position the step started from, and redo the state
its undo started from, with the same text-change, selection and accessibility notifications as before.

- Entry: the caret, selection and scroll position to restore and the text in one of two forms. The newest undo step,
  while its edit is in progress (a typing run), is open: it shares the whole text the step started from, which costs no
  copy when it is recorded. The next history operation seals it against the text then current: it keeps only the units between the two
  texts' common prefix and suffix (position, units replaced, units put back), so the restored text is identical unit for
  unit wherever a span edge falls (inside a surrogate pair or a grapheme cluster included). A sealed typed character or
  paste keeps no text (an insertion puts nothing back); a deletion, a cut or a replacement keeps the units it removed; a
  replace-all keeps the old text once. Undo and redo apply a step in place and push its inverse, which keeps the text the
  step replaced (undoing a 64 Ki-unit paste keeps the pasted 64 Ki units for redo). A text change that records no history
  (a composition preview left in place, an item an editable `ComboBox` commits) first reopens the newest sealed step of
  each history against the text before it, so undo still restores exactly the text it restored before the change.
- Budget: at most 64 steps each way (the count cap is unchanged) and at most 4 MiB per editor across undo and redo
  together: the stacks' element storage, every sealed entry's span and every open step's whole earlier text. When an
  edit, undo, redo or reopening exceeds it, the oldest sealed steps are dropped first, the undo side before the redo side,
  until the sealed spans and storage fit 4 MiB less the open texts. The step the operation just added and an open step
  are never dropped, so an edit larger than the budget is still applied and stays undoable while it is the newest step;
  the next step drops it with everything older. When a single open step's text alone exceeds 4 MiB (a text of more than
  2 Mi UTF-16 units), that step stays, as the newest, and every older step is dropped: the history then holds that one
  text and exceeds the budget by it until the next history operation seals it.
- Counter: `EmbeddedStatistics::largestTextHistoryBytes` (G13's `text-history` line) counts the same bytes (an open
  step's earlier text only once the current text no longer shares it), read without allocating, so it never exceeds
  4 MiB except in the single-oversized-step cases above. The gate: the FX7 `TextField` and
  FX2 editable `ComboBox` "history" variants (64 Ki units, 128 edits each, every edit its own step) MUST keep at most
  4 MiB after the edits and after every undo and redo, and every undo and redo MUST restore the expected text, caret and
  selection; a scripted edit sequence MUST record the same text, caret and selection after every operation, undo and redo
  as the full-text snapshot history it replaced (`Controls.Tests.TextHistory.cpp`). Measurements:
  [the W9.3 receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W9.3/README.md).

Teardown accounting: after one FX5 lifetime (scene build, 20 warm and 20 changing frames, hide, destruction) has
created the process's first-use state, a second lifetime MUST return every tracked C++ byte (0 live tracked bytes) in
every configuration, and in Debug on a D3D11 debug-layer WARP device MUST leave the debug layer's live-object report
equal to the fixture's own objects (0 DxUi-created live device objects). Without the SDK layers the device report is
skipped and says so; the tracked-byte check still runs.

The opt-in `--benchmark-clean-prepare-floor <fresh-report.json> <seed>` diagnostic is dispatched outside the
canonical six-input complex-UI benchmark. It measures the already prepared default scene on WARP with one QPC
bracket per batch of 1, 8, 64 or 512 no-op Prepare calls. Retain the seeded size ordering, balanced group positions,
alternating preparation/reference order, all raw ticks, frequency, call counts and normalization. An empty-loop
reference is a separate observation; never subtract it from canonical metrics or change their investigation bands.
Verify unchanged preparation/composition counters, resource statistics and the sampled model/slider/progress/Grid
selection state. These consistency fingerprints do not establish every control's state or visual equivalence.
Every actual clean Prepare call in each batch and warmup must return exactly `S_FALSE`. A sticky all-results
check rejects an earlier `S_OK`, other success code or failure even if the final result is `S_FALSE`; the native
`--test-clean-prepare-timer-oracle` covers that rejection as well as call/tick normalization and real prepared-scene invariants;
unknown argument counts fail before functional suites, and reports refuse to overwrite existing evidence. A
batched diagnostic does not replace the canonical clean preparation p95, same-side controls or policy review.
The bracket includes HRESULT checks, `S_FALSE` classification and loop bookkeeping; normalized values are bracket averages, not isolated method
latency. The [control investigation](../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/README.md)
retains aggregate replay and diagnostic limits. Internal assertions cover more state than the serialized subset.

Steady-state hot paths use bounded reusable storage; cache derived state and batch compatible work. Never allocate,
shape text, create targets, traverse layout, do I/O, block or read back in clean composition. Coalesce dirty state and
prepare only changed visible content. Hidden, minimized, occluded, suspended, display-off and idle work is event-blocked,
with no embedded timer/worker/polling loop. Share immutable resources per device generation; explicitly bound queues,
caches, textures, replacement peaks and concurrent instances. Checked arithmetic/capacity failures must fail cleanly,
release ownership and retain coherent visual/input state. Destroy device-generation resources together on recovery.

Native occlusion is an application visibility signal through
`SetNativePresentationVisible`: the application supplies known occlusion,
suspension or display availability changes. Native flip-model hosts do not infer
arbitrary coverage by other windows and add no polling or global tracking hook.
Embedded visibility remains application-owned through `EmbeddedHost::SetVisible`.

## Independent measurement ownership

DxUi owns its samples, synthetic data, benchmark workloads and baseline receipts. They MUST build and run using
this repository, the Windows SDK and pinned library dependencies alone. No consumer checkout, plugin, settings,
AV endpoint or application service is required. Inspiration from application layouts is allowed; executable fixtures
and their acceptance criteria remain library-owned. The runnable complex sample and timed benchmark use the same
`Samples/ComplexUi/ComplexUiScene.h` scene. Every fixture change requires a new identity and matched fixture hashes.
A harness-only change (assertions or receipt fields) keeps the workload identity but changes the fixture hash, so the
matched baseline is measured with the final harness on the previous implementation before the candidate is compared.
Renamed benchmark headers MUST also overlay any existing legacy include paths in the older measured tree with the
current payload. Those aliases exist only in the temporary overlay and are restored with it; recorded current inputs
and the older entrypoint must reach the same fixture bytes. Every overlaid path MUST match its retained SHA-256
after copying, before build/reuse, and before and after every measurement.
The paired overlay builds its benchmark-only translation unit with `DxUiPairedBenchmark=true`, using the same
scenario and instrumentation on both sides. Each side builds and links its own library project and runtime;
the translation unit and sample failure-host source and header are part of the hashed, restored overlay inputs.
Overlay rollback MUST retain and verify the original hash, refuse changed backups, preserve and report later edits
to either replaced or overlay-created paths, and attempt the remaining restorations. It records the actual installed
bytes immediately after copying so an owned partial copy can still be rolled back without treating later edits as its own,
and MUST report failure when a completed study cannot restore a named tree. A failed or mismatched overlay supplies no
performance qualification; preserve its original records for diagnosis before starting a fresh study.

Application-specific adoption reports, configurations, endpoint workloads and budgets belong in that application's
repository. Do not store them in DxUi docs or use them as a substitute for independent library evidence. Conversely,
DxUi offscreen throughput does not establish the complete application's presentation, input or service latency.
Reviewed independent raw receipts may be retained under `Measurements/`, with a README explaining the scenario,
source identity, all noisy runs and limits. Intermediate runs remain under `.build`; docs link to the retained evidence.

Measure the full sum of simultaneous views at 96/144/192 DPI, including shared-pool and replacement costs. One
1280x720 BGRA surface is 3,686,400 bytes (3.52 MiB), excluding driver overhead; physical extents determine residency.
A hidden or zero-extent view holds no surface and reports `surfaceBytes` 0: a consumer reclaims a collapsed view's
surface by hiding it, and the next visible sized preparation allocates exactly one replacement.
Applications must admit their aggregate view cost using their own instance bounds. Allocation counters distinguish
library-controlled work from OS/driver internals; both remain measured. A single composite draw does not make dirty
preparation free. Never impose one application's module topology, two-view layout or endpoint latency on all consumers.

### Single-library implementation budgets

EmbeddedHost has at most one cached surface (64 MiB maximum) while visible with a nonzero extent, and none while
hidden or zero-sized. A resize allocates the replacement before releasing the previous surface, so the single-surface
cap bounds the transactional replacement peak to 128 MiB per view by construction; `replacementPeakBytes` reports it.
Immutable composition state, the D2D device and the DWrite factory are shared through GraphicsDevice. A consumer
using tile/raised views admits their summed surface cost. Composite is a single triangle with shader constants
derived from SV_VertexID and needs no per-view vertex/index/dynamic constant buffer. Hidden state performs no timer
subscription; native WindowHost alone uses the event-driven animation dispatcher. Animation ticks add no raster work
by themselves: `AdvanceAnimation` never marks the view dirty, and every `Tick` that changes visual state invalidates.
Per-host caches are bounded to 256 solid brushes and 256 configured text formats by last use (see
[format and brush caches](#format-and-brush-caches)); nothing is released mid-paint, so steady-state residency stays
proportional to the painted working set of the last two frames. Diagnostics are borrowed and optional; composition does not emit them.
The private window-message payload registry is bounded to 128 windows and 128 queued payloads; saturation fails
immediately and releases transferred ownership. Teardown invalidates queued tokens and drains outside its lock.

A Grid's selection is a `GridSelectionModel`, which keeps each selected stable id twice: in selection order, and
ascending beside it. `IsSelected`, which a Grid asks once per visible row on every paint, allocates nothing and does not
grow with the selection. Up to 1,024 selected ids it scans the ids in selection order, as it always did (about 0.1 ns an
id); above that it binary searches the ascending copy (9 to 16 ns in an order a processor can predict and 48 to 149 ns in
one it cannot, from 1,500 to 1,000,000 ids, where the scan cost up to 94 us). The limit is 1,024 because a search whose
branches cannot be predicted, as the ids of the rows on screen cannot be against a selection of hashed ids, costs what a
scan costs at about 500 to 650 ids: below the limit the search can lose to the scan (4.5 times at 32 ids), above it never
does, so the model is never slower than that scan at any size in either order. The ascending copy is the budget: 8 bytes per
selected row beyond the ordered ids' 8 (the model itself grows from 40 to 64 bytes, and to 72 with the epoch that moves
whenever its ordered ids change, which a Grid compares instead of copying the selection), and never a heap node per row. Both
copies give their room back when a selection that needed room for more than 4,096 ids (32 KiB) is replaced by one that needs
half of it or less (`Clear`, `SetSingle`, `SetRange` and a `PreserveOrdered` that drops ids; `Toggle` leaves its room):
after Ctrl+A over 200,000 ids a model holds 0 bytes after `Clear` and 16 after a click, not 3.2 MB, for 0.1 to 0.2 ms to
free and 0.3 ms to get back, and a selection of 4,096 ids or fewer keeps its room. The mutators, which run on user gestures
and data changes, keep both copies equal at O(n log n) at most: a `SetRange` over ids that do not already ascend sorts
them (1.0 ms for 20,000 ids, 10.9 ms for 200,000; a radix sort of them takes about a seventh of that and is not done);
`PreserveOrdered` asks about every row of the model, and a table of bits rules out most rows before the search answers the
rest, so 200,000 rows with 3 to 5,000 ids selected take 0.2 to 1.1 ms (a hash set took 1.2 to 2.8) with 2 or 3 heap
calls in the recorded implementation. Under the current failure policy, checked allocation refusal in `SetRange`
and `PreserveOrdered` must preserve the previous selection; an unexpected throw requires host fatal diagnostics.
Historical timings do not qualify that failure-policy migration. A selection structure that costs a node per row, a membership test that scans more than 1,024 ids or that searches a
smaller selection more slowly than the scan would, a model that keeps the room of a selection it no longer holds, a
`PreserveOrdered` that costs more than reading its list, or a paint that allocates for the selection regresses this
budget. The
[paired record](../../Measurements/GridSelection/2026-10-01/README.md) holds the measurements.

Rich snapshot slicing must return a checked nonthrowing allocation refusal or enter allocation-site fatal
diagnostics, including Debug STL proxy construction. Checked-refusal sweeps preserve the captured source,
document revision and histories. Unexpected throws run in isolated fatal probes with original-site context and
dump verification; a returning terminate-handler stack is not that oracle. Test-only handler installation stays
inside the owned child, not ordinary suite or canonical benchmark dispatch. Retain historical throwing-recovery
receipts separately; they are not acceptance of this failure policy.

Guarded rich document geometry retains at most 127 chunk layouts plus one optional
whole-layout compatibility resource, with old/staged replacement bounded together.
Position-independent chunks (W7.3): every v0 chunk is laid out at its own origin 0 with no
private text, and the document places it at the double-precision sum of the line heights
of the chunks before it (a nonfinal chunk contributes its lines without the phantom line
after its final line feed; the final chunk all its lines), rounded once to single precision.
Caret, hit-test, range, visible-range and drawing coordinates add that placement; a
placement is recomputed for every chunk of a publication, at most 127 additions. Reuse requires only
the chunk's unchanged blocks and compatible options, never its old position: an edit
re-lays the chunk that contains it, which grows up to 2,048 source units to the next unchanged
chunk instead of splitting when other chunks are reused (one chunk per insert and per Enter on
`FX8`, at most two by contract); when nothing is reusable chunks keep the 1,024-unit target. It moves every
later chunk, whatever the height change, and reports the prepared source units. An
explicit line-height multiplier or any script run spaces every chunk with the whole
document's largest run size and script flag, as one whole layout would; a change of those
inputs re-lays every chunk. An empty displayed suffix after other chunks falls back
explicitly to the whole layout. A chunk's placement is the exact sum of the line heights
before it, rounded once (at most 1/256 DIP below 2^17 DIP, one single-precision unit
above). Within a chunk DirectWrite places lines by its own single-precision sums from the
chunk's top. Guarantee: below 2^17 DIP every chunk's first line, and every line of a chunk of
ordinary text lines, lies within 1/64 DIP of the exact line sum (one single-precision unit more
above 2^17 DIP). The one exception is a chunk of about a thousand short lines (runs of empty
lines fill a 1,024-unit chunk with up to 1,024 lines): there DirectWrite's in-chunk
accumulation adds at most half a unit of the chunk-local coordinate per line before it in the
chunk, measured up to 0.39 DIP (a 1.6 line-height Source document with 1,000 leading empty
lines, 40,313 DIP tall) against at most 0.0013 DIP at its chunks' first lines. One whole
DirectWrite layout accumulates its line tops in single precision from the document start
and drifts from the exact sum as the document grows (measured: 0.02 DIP at 7,000 DIP for
1,000 lines, more than 1,000 DIP at 1.57 million DIP for 65,536 lines); position-independent
chunks cannot reproduce that drift, so a chunked coordinate differs from one whole layout by
at most 1/64 DIP plus the whole layout's own drift at that line, and the bound is stated
against the exact sum. The `TestRichDocumentLayout*` geometry oracles check every line top,
caret, selection rectangle, visible range and hit test against the exact sums and record the
whole layout's largest drift. Pixels therefore may differ from one whole layout where its drift
moved a glyph baseline across a device-pixel rounding boundary (measured: 6 of 36 viewport
pairs of the raster profiles, 116 to 56,096 differing bytes, at drifts up to 0.046 DIP); line
pitch and heights are unchanged, and documents short enough for that drift to stay far below
a pixel draw exactly the whole layout's pixels (`TestRichDocumentLayoutChunkPixelsMatchShortWholeLayout`).
Each chunk's ink is its own layout's ink at its exact line top
(`TestRichDocumentLayoutChunkInkAtPlacement`). An
incremental preparation equals a fresh preparation with the same chunking exactly: carets
bit-identical and WARP BGRA pixels identical for fractional fonts, RTL, scaled and skewed
transforms and large origins, with a wrong-offset negative control. Direct2D snaps glyph
baselines in device space from the final drawing origin, so a chunk draws the same pixels
wherever it is placed for a given document origin. Font/color
capabilities and coordinate maps are prepared once; drawing borrows them without
layout, font queries or allocation. Changed-source units, reused chunks
and any whole-layout work remain separately reported.

Viewport-first preparation (W7.3): an owner gives its current viewport to every advance
(a library-internal argument; the public `AdvancePreparation` takes none and publishes only
complete results). On each advance the chunks whose estimated placement meets the viewport
window (the viewport with one viewport height of overscan above and below, centred on the
caret the owner will reveal after an edit) are laid before any other chunk; once none is left
unlaid, the document publishes a partial publication, lays the rest and publishes again when
complete. A partial publication is made only when a chunk meeting the window was laid since
the last one: an edit outside the window whose window chunks are all reused publishes nothing
partial. The advance that publishes goes on laying within its budget, so a document whose
rest fits that advance completes in it; an advance publishes at most one partial
publication. Every chunk not yet laid has a stand-in: in order, the previous chunk of the
same unchanged blocks (laid with other options or before an edit), a stale chunk of an
earlier partial publication (it stands in again but is never reused), or an estimated
placeholder with no geometry. Estimate rule: a placeholder is as tall as its source units
times the height per unit of the plan's chunks whose heights are known (laid, reused or
stand-in); before any is known, the previous publication's height per unit; without one (a
first load, or after `Clear`) and with the viewport away from the document's top, one sample
layout of the document's first blocks (at least one, about 512 display units, never above
4,096) measures the average line height and the units a wrapped line holds, and each chunk is
its physical lines, each wrapped at that capacity, times that line height (counted as
`rich.estimateSampleLayouts`; a first block above 4,096 units uses the uniform line height,
or 1.25 em, and half an em per unit instead). At the top no sample is needed: the first chunk
meets the window. `rich.offscreenChunksLaidFirst` counts, against the window as estimated when
each chunk is laid and independently of how the chunk was chosen, every chunk laid outside
the window while a chunk meeting it is still not laid; viewport-first preparation keeps it 0.
A partial publication is not `Ready()`: every query refuses it as a pending preparation
would, and the editor treats its layout as not current, so input that needs geometry, caret
and selection rectangles, scrolling and reveal, find, UI Automation and text-service
rectangles wait for completion or answer pending; only painting draws the laid chunks, never
a stale chunk or a placeholder. Both hosts still withhold every frame while any control's
content preparation is pending (no partial surface is composed), so no host frame shows a
partial publication yet: the gain is the order of the work, and the drawing path waits for a
host policy that presents one. The owner anchors a reflow on its first visible source line so
that the partial and the complete publication show it at the same place. Work units are the
existing layout and block-step budgets; no time budget exists; the viewport is read on every
advance, so a scroll during preparation moves the window. A partial publication keeps the
last complete publication alive until a newer complete one replaces it: a preparation whose
inputs (factory, mode, options, source) equal that publication's restores it at once and
lays nothing, as a width step and back during a live resize does. Retention bound: each graph
holds at most 127 chunks plus one whole layout; during a preparation at most four graphs are
alive (the publication, the last complete publication a partial one keeps, the partial
publication the preparation began from, and the staged graph, whose laid chunks an interim
publication shares), and they hold at most 3 x 127 distinct chunk layouts and 2 whole
layouts. Old and staged graphs together can therefore exceed 127 chunks: a width step of a
90-chunk document keeps the 90 old chunks while it lays up to 90 new ones.
Code blocks (W7.6). `PreparedLayout::TryVisibleRange` answers a window without reading a caret box per line where it can
show that the caret boxes are stacked: a box's top and its bottom never decrease from one line to the next. Preparation
decides that once per layout (`PreparedLayoutAccess::CaretBoxesStacked`): the text has one style (every style run alike and
covering it), no inline object and no script run, and, in uniform spacing (a line-height multiplier or script spacing), is
printable ASCII. Then every line's caret box comes from one face at one size: in natural spacing it is the line's own box
(its height equals the line metrics' on every line of every fixture), and in uniform spacing every line has the same height
and the box has the same offset and height inside it, so tops and bottoms are the line tops plus a constant. A layout of
mixed run sizes, of inline objects or scripts, or of uniform spacing with a font fallback is not decided stacked and is
stepped from the first line, as the query always was: there a caret box can leave its line (a glyph box taller than a line
of a multiplier below the font's height, a large run starting a line after a small one), and a search over boxes that go
back would be wrong. A stacked layout of more than 16 lines keeps the sums of its line heights as a hint of the line to look
at first; the first visible line and the first line starting at or below the window are found by doubling steps from the hint
and a halving over the real caret boxes (`HitTestTextPosition` at each line's first offset). The answer is the stepping
scan's: the tests keep that scan as the reference and compare it at every line boundary and one single-precision unit either
side, with windows of 0, one unit, one line and two and a half lines, for wrapped Latin at multipliers 0, 1.0, 1.15 and 1.65 (preparation refuses one below 1),
right-to-left, mixed bidirectional, empty-line and trailing-newline text, layouts of mixed run sizes (a heading line after a
body line and the reverse) and layouts of 65,536 and 131,072 units (the source and semantic display maxima), and find no
difference (`Controls.Tests.RichCodeBlocks.cpp`). The gate rests on the argument above, not on a failing fixture: with the
search forced by a test switch on the mixed-size and fallback-font layouts the sweep is run too and its result is in the
receipt. The sums are a hint and not the answer because DirectWrite's caret tops are not
those sums: with a line-height multiplier the caret box is the glyph box inside the line (on every line its height differs
from the line's and its top is offset, by 0.63 DIP on the first line at 13.7 DIP), and without one DirectWrite's own
single-precision accumulation differs from a float sum of the line metrics on 231 of 1,201 lines. A query therefore reads a
bounded number of caret boxes instead of one per line before the window; the plan item's clause of 0 `HitTestTextPosition`
calls per visible-range query is not met, and cannot be with this data. The document-level
`PreparedDocumentLayout::TryVisibleRange` does the same per chunk, with the placed top of a line (the chunk's top added in
double precision and rounded once) in the same two predicates: a chunk of stacked boxes is searched, any other stepped, and
`PreparedDocumentLayoutAccess::VisibleRange` with `stepping` is its reference. The visible-range queries of the semantic
geometry (`TryVisibleRange` and `TryVisibleSourceRange`) visit the boxes the paint index returns for the window, in box
order, instead of every box; the index's leaves are each box's frame and ink, a superset of the boxes the same frame test
selects, so the answers are the full scan's (`TryVisibleRangeByFullScan` and `TryVisibleSourceRangeByFullScan` keep it as
the reference: 10,824 windows over a document of paragraphs, a list, a table, a quote, an image and three fences, in both
presentations and both reading directions, no difference).

The line numbers and language badges of numbered code fences are layouts shared through one cache per semantic geometry
(`CodeLayoutCache`, registered as `RichTextEditor.codeLayoutCache`). A number layout is keyed by its digits, the gutter format
(width, code font family and size, line height, tab stop, locale, alignment, wrap), the reading direction and the DPI;
a badge layout by its language and the same fields, and its width follows the document's, so a width step builds one badge
per language. The cache holds the one factory its layouts were made by and drops every entry when a preparation begins with
another, so an address a released factory had can never name one of its layouts (`TestRichCodeBlockCacheFollowsItsFactory`).
A cache hit spends no layout of a turn's budget; a turn is still bounded by its block steps, one for each number placed, hit
or built, so on a 2,000-line fence a turn of 64 steps places at most 64 numbers
(`TestRichCodeBlockTurnsAreBoundedWhenEveryNumberHits`). An insert that cannot allocate is not caught: the owner turn's
`catch (const std::bad_alloc&)` enters the fatal allocation path. The published geometry and the one in preparation share the cache; a cached layout is immutable
(`shared_ptr<const PreparedLayout>`) and is never prepared again. A publication drops the entries its generation did not use,
so after it the cache holds what that geometry draws, and during a preparation that plus what the preparation adds. Bound: 4,096
number entries (the most numbers one geometry draws) and 256 badge entries; when a table is full, the entries the preparation
in progress has not used are dropped first, and a layout that still does not fit is used and not kept. Resident bytes are the
tables, the keys' strings and options, about 560 bytes an entry (2,501 entries measured 1.4 MB; at most about 2.5 MB at the
bound); a layout's DirectWrite storage is not counted. A theme changes only colors, which no layout holds. A change of font,
size, DPI, gutter width, reading direction, locale or line height is another key and builds new layouts, each tested against a
geometry with no cache (`TestRichCodeBlockLayoutsAreNeverStale`). On `FX8`, number and badge layouts built per one-character
prose insert, Enter, arrow key and insert next to the table are 0, and 0 per character typed inside the numbered fence (it adds
no new distinct number; a line added builds exactly the numbers no fence has yet,
`TestRichCodeBlockLayoutsAreSharedAcrossEdits`); a width step builds no number layout and at most one badge layout per
language (`Embedded.Tests.CodeBlocksFx8.h`).
Count whole-layout work separately from chunk reuse. Owner preparation may construct that cache; cached
embedded composition must allocate and prepare nothing. `PreparedDocumentLayout::Draw`
uses only published reference/chunk layouts and never creates a compatibility layout,
joins a projection or changes preparation statistics. Reference-cache comparison walks
prepared blocks and coalesced style runs without materializing a whole projection; only
a required replacement layout joins it. Semantic geometry obtains a missing diagram
factory from its supplied raster target, keeping extension inspection inside that service.
No per-control worker or
periodic timer follows from reference DLL recalculation code.

The 1,024-line/512-edit source/history/geometry fixture is bounded by 8 MiB retained
and 16 MiB replacement-peak C++ storage. Maximum-source geometry fixtures are
bounded by 24 MiB and 128 layouts. All tracked deltas must release to zero; these
limits exclude SDK-private heaps and do not establish whole-process acceptance.
One raster image decode holds no duplicate encoded buffer and at most the encoded
plus decoded size of tracked C++ storage, plus one decoded-size buffer only for a
transposing orientation of a non-square image and at most 2 KiB of bookkeeping
(the [rich image contract](../UI/UI_ControlsAndLayout.md); Embedded
`TestRichImageDecodeMemory`, also `--test-rich-image-decode-memory`). WIC's own
heaps are outside this bound.
The [guarded geometry record](../../Measurements/RichText/README.md)
retains all failure schedules, geometry/pixel oracles and original negative runs.
The [positioned-line record](../../Measurements/RichText/README.md)
adds 324 exact WARP pairs, default-font native/embedded reuse, complete observed
copy/chunk/whole/flat failure schedules and all six build profiles. Clear,
unchanged queries and cached composition allocate no tracked C++ storage. Its
unchanged five-trial preparation observations do not establish quiet-host,
same-binary drift, causal speedup, large-document latency or frame acceptance.
Clear uses fallibly preconstructed retirement storage; staged projection building
must not depend on optional NRVO to avoid noexcept Debug proxy termination.

Cooperative rich preparation retains at
most one published and one staged bounded graph per owner; immutable geometry
can be shared between them. Each graph keeps the existing 127 chunk plus one
whole-layout bound. Advancing limits SDK layouts and block/reused-placement work
separately (default 4 layouts/4,096 steps per visible owner per host turn). Aggregate
host work also depends on visible owner count. Projection construction, joining
and one physical paragraph/whole SDK call are indivisible v0 work, so these are
work-unit ceilings, not hard millisecond guarantees. Cancellation/retirement adds
no heap work. No per-editor worker, timer, device or HWND is introduced.

One outstanding application wakeup is separate from dirty surface state. A
pending turn may request one continuation; a failed turn must not self-reschedule.
Clean/hidden work does not poll; new source after failure can request explicit
retry. Existing retained/peak/source-limit bounds and zero-delta oracles remain.
Repeated source-range scroll queries against a pending layout return unavailable
without advancing preparation or changing source, history, selection or scroll.
The prepared-layout path retains its ordinary caret and range reveal behavior.

The [cooperative preparation record](../../Measurements/RichText/README.md) qualifies 57 ordinary x64 scopes, six builds and 324 exact WARP pairs without changing the existing ceilings or oracles. Work-unit bounds do not establish quiet-host frame acceptance, hard latency or large-document capacity.

The opt-in M4d semantic path uses the same host turns with separate bounded CPU
semantic events (default 4,096), SDK layout calls (4) and geometry metadata events
(4,096). Its graph is bounded by 4,096 native layouts (markers, bodies, cells and caret boxes) and 32,768
elements, plus the source/display/map bounds. One SDK layout, leaf tokenization
or final map construction remains indivisible. It retains at most a published
and staged native geometry graph. A source edit builds only the top-level semantic
blocks between the unchanged leading and trailing blocks, which it copies from the
previous publication in one indivisible event each (block-incremental projection in
[controls and layout](../UI/UI_ControlsAndLayout.md#block-incremental-semantic-projection));
this is not v0 chunk reuse. The copy itself remains proportional to the document. Immutable CPU syntax/document ownership can
outlive native geometry when clients hold it. Raster preparation may allocate;
composition must not call the builder/layout backend or allocate.
Prepared formulas, diagrams, SVG rasters and image bitmaps carry from the published generation into the next by exact
content keys ([prepared objects across edits](../UI/UI_ControlsAndLayout.md#prepared-objects-across-edits)): on `FX8`
an edit, arrow key or width step outside every object prepares no formula, SVG raster or diagram and uploads no image
byte, and a device replacement prepares them all again. The carry adds no cache: an object lives only while a
publication or the one staged preparation holds it, and an unclaimed one is released when the next generation is
published.

Geometry preparation indexes source line units and bounds cooperatively before
publication; cancelling or clearing owns no new heap work. Hidden/retired controls
release SDK graphs. A semantic admission rejection prepares source geometry once,
remembering exact syntax identity so idle preparation cannot retry in a loop.
OOM/SDK/internal failures remain failed owner turns with no self-rescheduling.
Parser admission reserves a physical-line upper bound for empty-line source
metadata during its bounded source scan/copy. Replacement avoids geometric
vector-growth peaks; the existing 24 MiB retained/32 MiB parser replacement-peak
ceilings remain unchanged. Native caret-box counts are preflighted cooperatively
before SDK calls, including row-wide ordering for padded cells.
Marker measurement is a separate cooperative owner phase after admission and
before body placement. Every real no-wrap marker layout and body layout consumes
one SDK call; a source paragraph may therefore require two native layouts. Cached
native advances establish the maximum gutter per list, with a bounded 64-level
ancestor walk. Publication, scalar navigation and composition perform no marker
measurement. Cancelling a partial marker pass retains the previous publication.
Native HTML validation permits at most 4,096 tags and 32 simultaneous opening
tags in one leaf. Validation/attribute decoding and block text projection remain
bounded indivisible leaf CPU work. Policy rejection chooses literal leaf contents;
allocation/SDK errors retain the existing failure contract. These bounds do not
establish hard latency, browser layout equivalence or large-document acceptance.
Resource/failure sweeps and all-current renderer qualification remain M4g work;
earlier sealed parser/v0 packets do not establish semantic rendering performance.

#### Transient raster decoration

Transient rich-text raster ink borrows DirectWrite drawing effects only for the
synchronous draw. Plain or fully excluded decoration must not clear an untouched
effect range. Arm restoration before each attempted effect setter, including a
failing setter, and restore canonical state before returning or propagating the
failure. Preserve ordered underline application and restoration for overlapping
admitted style runs. This optimization retains no additional layout or brush;
pixel/geometry/copy regressions and measured CPU/memory evidence remain separate.

A paused ProgressBar contributes no animation continuation and keeps its current indeterminate phase. Pausing and resuming use one retained boolean and reset the tick seed; elapsed paused time is not replayed as catch-up work. A host may continue animation for other controls. Regression coverage must observe idle pause, active-to-paused phase preservation and resume without a phase jump, with enabled/indeterminate state unchanged.

Native swap-chain creation, resizing, back-buffer retrieval, and target-bitmap creation route device-loss HRESULTs through the same shared-generation retirement and full-repaint recovery as presentation failures. The native WARP fixture injects each failure stage, including ResizeBuffers on an existing swap chain, and checks a recovered resized frame. Unrelated preparation failures preserve their existing retry policy.

Single-line reverse grapheme movement and unindexed deletion search backwards to a boundary proved by the shared Unicode 17 properties, then use the canonical forward walker from that boundary. Adjacent ordinary non-Indic characters, including ordinary CJK, require constant local property work per movement. Regional-indicator runs, combining sequences, Indic linkers and pictographic ZWJ context may require scanning an entire contextual run; no arbitrary cutoff changes their boundaries. CRLF remains one element. Native text-input synchronization transfers the exported text buffer into its snapshot instead of copying it again; this does not change ownership, notifications, or secure cache retirement.

Prepared math shares an immutable OpenType MATH snapshot, capped at 4 MiB, and bounded glyph-indexed correction and variant-offset arrays across formula preparations, geometry generations and width reflows (plan item `W7.7`). The snapshot is shared per font face through the owner thread's `MathFontDataCache`: fixed storage for eight entries (the oldest displaced first), one per DirectWrite factory and math family, registered in the retained-cache registry as `RichText.mathFontData`. The cache owns nothing: an entry is the factory's identity, the family and a weak reference to the snapshot that the prepared formulas hold (the published geometry generation and the one being prepared), so no factory, font collection or font face is kept alive by the thread; when the last formula that uses a snapshot is released, the snapshot and the references it holds are released with it, and a thread that has prepared no formula holds nothing. A factory's address cannot be reused while an entry still finds data, because that data is held by a formula that holds the factory. The snapshot keeps the system font collection it was read from; a formula that finds another collection (the system fonts changed, so `Typography::IsFontFamilyAvailable` asked DirectWrite for an updated one) erases the entry and reads the font again, and so does a drop of the registry. Reading, counting and dropping the entries allocate nothing, whichever thread asks. A formula served from an entry makes one query of the factory's current collection and reads no font, so on `FX8` an edit that prepares a formula again, in the first formula or anywhere, counts no font read (`rich.mathFontReads` counts the reads: family, face and MATH table copied and parsed). A family without a usable table is not remembered. Preparation may query glyph metrics; painting performs no table parsing, allocations or font discovery. Malformed or unavailable data uses deterministic fallback and does not suppress the rest of the document.

A colored formula and a diagram with authored colors decide which authored color reads on the page colors it is painted on once per pair of page colors (text and fill; a diagram's stroke too), not at every paint: the decisions (each a contrast of relative luminances, up to three `std::pow` per color) are written into storage made when the object is prepared (a diagram without authored colors has none), at the first paint under each pair, and two pairs are kept (a light and a dark page, the older displaced). A paint allocates nothing, on a first, a second or a third pair of page colors; an object without the storage decides nothing and draws with the caller's brushes. A steady paint evaluates no luminance (`rich.luminanceEvaluations` stays 0 on `FX8` outside the steps that prepare the colored object again), and a theme change still carries every prepared object, since no page color is bound at preparation. The decisions belong to the prepared object and go with it (they are covered by the `RichTextEditor.layout` row). A flowchart edge's connector is one prepared `ID2D1PathGeometry` through every point of its route, a hollow marker's end already shortened in it, drawn with one `DrawGeometry` (`rich.edgeConnectorDraws` equals `rich.edgesDrawn`), so a dash pattern runs on across the bends and a thick edge has a join at each. Solid and dashed connectors use flat caps and a bevel join (`CreateEdgeConnectorStrokeStyle`), a dotted one the round caps and join of its dots: a bevel join fills only the wedge between the two segments' own outer corners, so a connector's ink lies inside the convex hull of the ink its segments draw apart at any angle of a bend, where Direct2D's default miter join would reach `1 / sin(angle / 2)` line widths beyond an acute corner. An SVG raster is drawn in strips of a bounded cost each, a few per owner turn; the strip's size is planned at every turn from the picture's pixel count, an estimate of its elements and that turn's whole upload budget (`PlanSvgStrips`: 254 rows of 4,096 pixels for a 16-megapixel picture of 100 elements at the default 4 MiB budget), so a strip never exceeds the budget of the turn that draws it, whatever the turns before it had; a strip that fits the turn's budget but not what the turn has left waits for the next turn, and one private device context serves every SVG picture of a preparation (`resources.deviceContext.created` at most 1 per preparation on `FX8`, whose document holds two pictures).
