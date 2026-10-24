# Performance and resources

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

The lossless Markdown analysis preview retains bounded source (65,536 UTF-16
units), one staged result and one published result per parser. Advance has an
operation budget; source validation/copy/comparison and allocator work are not
hard wall-clock bounds. Block transitions inspect one bounded physical line;
reference finalization, block-tree compaction, construct lookahead, emphasis resolution and sorting are
indivisible bounded work. Each revision reparses block/inline dependencies before identity reconciliation; this is not
incremental block reuse. Lexical reference recognition examines at most
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
exposes those deterministic counts; it is not a total CPU/time counter. The maximum
all-newline source exercises maximum root-trivia and lexical partition counts; owned
C++ retained storage must stay within 24 MiB and release completely. Replacing
that maximum result while retaining its previous publication is bounded to
32 MiB peak C++ storage. This is not
a process/SDK/GPU or large-document capacity claim. Same completed requests,
read-only publication access and retirement allocate no C++ storage. Parser
allocation-failure sweeps must retain the previous publication and allow explicit
retry. No parser result changes the existing frame performance HOLD.
MSVC Debug iterator proxies allocate even in default/noexcept STL container
constructors. Parser staging and node values use explicit fallible zero-size
construction and copies rather than noexcept allocator-backed container moves.
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
child/header collection construction must return E_OUTOFMEMORY safely and allow
retry. Child providers hold weak semantic graph identity; optional rich snapshot
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

## Mandatory performance policy

Paired setup MUST share the benchmark project and its effective compiler options,
including per-item flags, while retaining each side's own library project/source.
Fresh builds stage the common benchmark project's timestamp after restore and
record/restore original timestamps so MSBuild's newest-import selection is
comparable. Reused builds retain their attested context. Before any measured
process, both v2 full build states MUST match fixture, harness, compiler,
dependency and environment identities. A mismatch consumes no measurement slots;
retain the refused states. No property, flag or threshold is ignored to get a match.

Hosted paired runs retain original measured executables, libraries, symbols and
build attestations as CI artifacts alongside every report. Continuous host
observations record native cumulative CPU counters, process CPU/memory, sample
gaps and collection intervals; a failed observer cannot admit a completed study.
Inspect observer cost and missing/inaccessible processes explicitly. These
observations are neither a quiet-host guarantee nor a scheduler/GPU trace.
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

Before changing implementation, measure the existing code with the same workload that will measure the candidate.
Retain the original baseline and measure again after the change. Record exact source/content and executable/fixture
fingerprints, compiler/dependencies, architecture/configuration, hardware/OS/driver, power policy, resolution/DPI,
visible controls/data size and repetitions. Run serially on the same quiet fixture. Cross-machine, changed-workload,
Debug/Release or x64/ARM64 comparisons cannot establish non-regression. If a new benchmark is required, run the same
harness against both implementations; `performance-paired.ps1` measures a baseline (a revision in a detached
worktree, or an existing tree named as it is) and a candidate (this checkout, a revision or a named tree) with this
checkout's harness inputs and runs the interleaved A/B/B/A pass serially, repeated as a [paired set](#paired-sets).
It refuses a pair with nothing to compare: two revisions that name one commit, or, when a tree is named, two
identical library source fingerprints; each side's revision or path, commit and fingerprint are recorded. A missing
baseline is explicitly unpaired and cannot close an implementation performance gate. Documentation/tool-only changes
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
- Fresh measurements require a `performance-build-v2` attestation before and after build and execution. Each side's
  expanded projects and resolved compile/shader inventory belong to its build-input identity. The comparison profile
  separately binds actual MSBuild, compiler/backend, shader compiler, linker/librarian and SDK bytes plus effective
  options and target evaluation context for both the library and benchmark. A changed source inventory alone is not a
  toolchain mismatch. Missing metadata, mixed compiler profiles and unsupported build overrides are inconclusive;
  historical attestations cannot be relabelled as v2 or reused without a fresh build.
- Default-output incremental `Build` additionally requires a separate receipt binding every intermediate path and byte
  in the supported platform/configuration profile, including objects, tracking logs and generated shaders for all solution
  projects. Missing or changed cache forces `Rebuild`; final-only `SkipBuild` does not depend on disposable intermediates.
  Both receipts are revoked before build and on failed publication. Resolved C++/HLSL source bytes and the owned native
  include closure belong to each side's build identity, never the shared compiler comparison. Arbitrary external or
  ignored native inputs, response/path-bearing option overrides, custom outputs, precompiled headers and modules are
  unsupported. Intermediate include directories accept only the exact two derived composite-shader headers plus
  supported native object/tracking/output files, preventing extensionless or alternate header shadowing. The selected
  MSVC/ATL, SDK/UCRT/WinRT/C++/WinRT, auxiliary VS and NETFX input roots bind all file bytes and optional-root absence.
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

### Native accessibility scheduling and synchronization

Native attachments start without a full tree snapshot. Mutations mark a revision dirty and post at most one private,
attachment-cookie-checked publication message while a provider or UIA listener is interested. An owner-thread provider
query flushes that revision synchronously; worker queries read the last complete published snapshot. Actual focus changes
publish synchronously so the system's first worker focus query can report the new control. Event comparison has a separate
baseline: a fresh owner query does not consume the queued notifications. Embedded views continue to publish explicitly
after application preparation, with no snapshot work in composition.

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

The `paired-benchmark` job of [ci.yml](../../.github/workflows/ci.yml) runs the study on a hosted x64 Release runner.
During migration, `Tools/BenchmarkGate.psm1` fails closed: a measured report cannot pass unless its base commit contains an
independently qualified versioned policy, all required receipt identities are present and the policy is unchanged. The new
candidate-only policy is explicitly untrusted. `Tools/tests/Test-BenchmarkGate.ps1` covers that fail-closed path.

- **When.** For every pull request to `main` that changes something the benchmark measures: the library inputs a receipt's
  `sourceFingerprint` covers (`src`, `include`, `Build`, the build props and vcpkg manifests), the harness and its
  benchmark inputs, the sources of the benchmark executable (`Tests/Embedded`, `Tests/Support`) and the fixtures and samples
  compiled into it (`Samples`), the build and restore scripts, the paired measurement and gate tooling and the workflow.
  Markdown never counts, and neither do the control and foundation tests, the validators, the gallery and the formatter. The
  job always starts for a pull request to `main`: its first step matches the changed paths against those rules and names
  the files that decided, and the steps after it run only when one matched, so a pull request that changes none of them
  ends the job within a couple of minutes with the check passed. A step that cannot decide fails the job, so nothing passes
  unmeasured because its scope could not be read. The job is not skipped for such a pull request, because GitHub reports a
  skipped job under its unevaluated name expression and a required check of the same name would wait forever. A manual
  dispatch with `benchmark_baseline` measures the revisions it names. A newer push to a pull request cancels its older run,
  and its job is bounded at 180 minutes; manual diagnostic runs are bounded at 240 minutes. A
  pull request's check is named `paired-benchmark (pull request)`, apart from the push and manual runs of the same job, so
  that a required check names exactly one check run.
- **What is compared.** The candidate is the pull request's merge ref. The baseline is that merge commit's first parent, the
  base as the merge ref was made, so the two differ by exactly this pull request. The merge base with the branch would
  also hold whatever `main` gained since the branch was cut and would blame a regression it brought on this pull
  request; the two are one commit when the base has not moved. Both are built in Release and measured by
  `performance-paired.ps1` with the merge ref's harness in `Default`, `MultilineGrid` and `MultilineGridDistinct`, twelve
  independently drawn ABBA/BAAB blocks each: 24 runs per side per scenario. The assignment protocol and schedule seed/order are retained. The
  gate keeps no baseline receipt, so it measures the base afresh.
- **What is published.** The job summary lists each scenario's set verdict and, for every metric, its medians, change,
  p-value, band, same-binary controls and outcome, with the flagged metrics first, and the artifact
  `paired-benchmark-x64-Release` keeps every receipt, comparison, `summary.json` and the conclusion as `verdict.json`.
  A flagged metric is listed whatever the conclusion; the gate dismisses nothing.
- **Conclusion.** Under the migrated contract, a complete statistical result is still not a pass until the measured base
  contains the trusted qualified policy. Missing/mismatched toolchain, executable, dependency or harness identity and any
  unqualified/changed policy produce `inconclusive` and fail the pull-request check. Source fingerprints alone cannot
  label an observed timing shift as noise. The report keeps the old judge and new judge decisions over identical receipt
  hashes so reviewers can assess the migration; neither decision can self-approve the candidate's policy.
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
- **Identical library inputs.** Matching source or executable hashes are provenance only. Timing/memory flags remain
  unresolved unless a trusted policy and its independent block/A/A evidence support the inference. Exact deterministic
  budgets retain the run-median check and add a separately reported no-tolerance comparison of raw-round maxima.
- **No verdict.** Invalid evidence, missing provenance, an unqualified policy, a missing/unreadable summary, a summary that
  lists no scenario or uses only an older six-run judge, and a run that did not finish fail with the reason in the job
  summary. A pull request that changes the harness and library interfaces together, whose base cannot be
  built with the merge ref's harness. Its comparison is measured by hand, as before.
- **Confirmed degradation** follows the advice rule above: stop, present the deltas and suspected cause, and never relax a
  band or replace a baseline to pass. The gate has no waiver list: a
  tradeoff the developer approves is recorded in this contract, as the sections below do, and a maintainer merges over the
  failed check.
- **S0-S2 implementation integration, 11 October 2026.** The developer's
  [implementation-only decision](../Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage) establishes no accepted
  performance envelope, policy trust, non-regression or production qualification. Retained timing/memory flags remain
  unresolved in [the qualification WIP](../Plans/WIP/ConsumerUiQualification_2026-10-11.md); bands, deterministic budgets
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
equal line payloads. Sweep each observed changed-preparation allocation failure
and prove complete prior-state preservation and a successful retry. Measure
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

Steady-state hot paths use bounded reusable storage; cache derived state and batch compatible work. Never allocate,
shape text, create targets, traverse layout, do I/O, block or read back in clean composition. Coalesce dirty state and
prepare only changed visible content. Hidden, minimized, occluded, suspended, display-off and idle work is event-blocked,
with no embedded timer/worker/polling loop. Share immutable resources per device generation; explicitly bound queues,
caches, textures, replacement peaks and concurrent instances. Checked arithmetic/capacity failures must fail cleanly,
release ownership and retain coherent visual/input state. Destroy device-generation resources together on recovery.

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
Overlay rollback MUST retain and verify the original hash, refuse changed backups, attempt the remaining restorations,
and report failure when a completed study cannot restore a named tree. A failed or mismatched overlay supplies no
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
Per-host caches are bounded to 256 solid brushes and 96 configured text formats; a cache beyond its bound is cleared
at the start of the next embedded preparation or native paint, never mid-paint, so steady-state residency stays
proportional to the painted working set. Diagnostics are borrowed and optional; composition does not emit them.
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
selected row beyond the ordered ids' 8 (the model itself grows from 40 to 64 bytes), and never a heap node per row. Both
copies give their room back when a selection that needed room for more than 4,096 ids (32 KiB) is replaced by one that needs
half of it or less (`Clear`, `SetSingle`, `SetRange` and a `PreserveOrdered` that drops ids; `Toggle` leaves its room):
after Ctrl+A over 200,000 ids a model holds 0 bytes after `Clear` and 16 after a click, not 3.2 MB, for 0.1 to 0.2 ms to
free and 0.3 ms to get back, and a selection of 4,096 ids or fewer keeps its room. The mutators, which run on user gestures
and data changes, keep both copies equal at O(n log n) at most: a `SetRange` over ids that do not already ascend sorts
them (1.0 ms for 20,000 ids, 10.9 ms for 200,000; a radix sort of them takes about a seventh of that and is not done);
`PreserveOrdered` asks about every row of the model, and a table of bits rules out most rows before the search answers the
rest, so 200,000 rows with 3 to 5,000 ids selected take 0.2 to 1.1 ms (a hash set took 1.2 to 2.8) with 2 or 3 heap
calls; and `SetRange` and `PreserveOrdered`, the two that may throw, leave the selection as it was when an allocation
fails. A selection structure that costs a node per row, a membership test that scans more than 1,024 ids or that searches a
smaller selection more slowly than the scan would, a model that keeps the room of a selection it no longer holds, a
`PreserveOrdered` that costs more than reading its list, or a paint that allocates for the selection regresses this
budget. The
[paired record](../../Measurements/GridSelection/2026-10-01/README.md) holds the measurements.

Rich snapshot slicing must propagate allocation failure rather than terminate in
Debug STL proxy construction. Sweep every observed long-slice allocation and
preserve the captured source, document revision and histories. The isolated
allocation diagnostics install a terminate handler only in their explicit child;
normal suites and canonical benchmark dispatch never install it. Retain the
original failure separately from successful final sweeps and build qualification.

Guarded rich document geometry retains at most 127 chunk layouts plus one optional
whole-layout compatibility resource, with old/staged replacement bounded together.
The qualified positioned-line implementation preserves DirectWrite's global coordinate math with
private invisible origin lines. Reuse additionally requires an identical document
origin; height-changing edits may rebuild the entire suffix and must report every
prepared source unit. Inexact origins and empty displayed suffixes fall back
explicitly; count whole-layout work separately from chunk reuse. Owner preparation may construct that cache; cached
embedded composition must allocate and prepare nothing. No per-control worker or
periodic timer follows from reference DLL recalculation code.

The 1,024-line/512-edit source/history/geometry fixture is bounded by 8 MiB retained
and 16 MiB replacement-peak C++ storage. Maximum-source geometry fixtures are
bounded by 24 MiB and 128 layouts. All tracked deltas must release to zero; these
limits exclude SDK-private heaps and do not establish whole-process acceptance.
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

The [cooperative preparation record](../../Measurements/RichText/README.md) qualifies 57 ordinary x64 scopes, six builds and 324 exact WARP pairs without changing the existing ceilings or oracles. Work-unit bounds do not establish quiet-host frame acceptance, hard latency or large-document capacity.

The opt-in M4d semantic path uses the same host turns with separate bounded CPU
semantic events (default 4,096), SDK layout calls (4) and geometry metadata events
(4,096). Its graph is bounded by 4,096 native layouts (markers, bodies, cells and caret boxes) and 32,768
elements, plus the source/display/map bounds. One SDK layout, leaf tokenization
or final map construction remains indivisible. It retains at most a published
and staged native geometry graph; source edits currently rebuild semantic leaves
rather than claiming v0 chunk reuse. Immutable CPU syntax/document ownership can
outlive native geometry when clients hold it. Raster preparation may allocate;
composition must not call the builder/layout backend or allocate.

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
