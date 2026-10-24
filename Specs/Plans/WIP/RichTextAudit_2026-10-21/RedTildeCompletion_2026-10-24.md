# RedTilde completion: current-source audit continuation — 2026-10-24

## Progress checklist

- [x] Inspect current main, consumer pin, existing audit ownership and primary reference projects.
- [x] Independently re-read the source locations admitted below; distinguish current defects from old findings.
- [x] Route library work to existing W/T/C rows and consumer execution to one coordinator.
- [ ] W0.3: revalidate every remaining historical finding and completeness row against the implementation revision.
- [ ] W1.1: reproduce and fix the current table-column memory-safety defect before larger changes.
- [ ] W1.2/W1.5: implement local object fallback, failure-key memoization and correct semantic/UIA ownership.
- [ ] W1.6/W1.12/C4/C5/T1: correct accepted math behavior, implement TeX layout fundamentals and broaden conformance tests.
- [ ] W1.7/C6/C7/C8/T2: correct Mermaid semantics, expand native profiles and qualify model/layout/render/source behavior.
- [ ] W1.11/T3/T4: reconcile status data, tests/scopes and cross-host/fault/resource coverage.
- [ ] W3: measure and qualify semantic reuse; keep W4/W5 consolidation conditional on evidence.
- [ ] Close library gates and exact consumer handoff with current-source evidence; do not close parent HOLDs by inference.

Status: **IMPLEMENTING; final profile and performance qualification remain open**. This appendix updates evidence
and sequencing for the [existing audit](../RichTextAudit_2026-10-21.md); its implementation work-item IDs remain
authoritative. The [RedTilde execution coordinator](../../../../../RedTilde/Specs/Plans/WIP/RedesignCompletionAndRendering_2026-10-24.md)
contains the complete cross-repository sequence, compatibility test matrices, reviewed commands and closeout
conditions. This appendix adds no second defect backlog or independent acceptance policy.

### Execution evidence — 24 October

The combined Debug diagnostics reproduce and repair table/source-history corruption, local semantic/geometry fallback,
math ownership/limits/recognition and prepared placement, Mermaid fan-out/state-label semantics, hollow inheritance and
capsule rendering. All 105 pinned flowcharts and the 20-family outcome matrix remain represented. The bounded math
profile has 30 positive formulas, 20 negative forms and 27 symbol goldens; full KaTeX or Mermaid parity is not claimed.

`combined-14` passes full EditorControls (69.9 s) and Embedded (55.2 s; 455,446 checks), plus the expanded parser and
layout/paint allocation sweeps. The initial Debug failure-injection termination and its exact later reproduction are
preserved, including the unavailable original binary boundary. `combined-16` characterizes interested focus lookup:
66,048 visits before, 1,792 after for 128 mutations among 512 unrelated siblings, with stale/hidden focus assertions.
The real ProgressBar range-client regression fails before and passes after publication changes. These diagnostics
do not replace final-source Debug/Release/ASan profiles, ARM64 builds/native execution, or paired performance acceptance.

The interactive lease setup denial regression fails before admission is made mandatory. Its descendant regression
exposes job accounting reaching zero before a process handle signals; retained verified member handles correct this
observed completion race. `combined-19` passes all 98 lease self-checks. A first direct Accessibility invocation omitted
`--no-activate` and failed its native-focus invariant; that invocation and exact executable are retained separately.
The governed rerun and subsequent final profiles own their own results. Seven historical whole-feature performance
regressions, including approximately 11% higher private memory, remain open pending a fresh actual-base comparison.

The governed full Accessibility rerun passes in 123.6 s. Both repositories' static/tooling validators pass; the separately
selected installed CMake also passes all 12 triplet checks. The owner selected finishing current profiles and qualification,
leaving full TeX font assemblies and additional Mermaid families in the existing language roadmap. That scope decision
does not close their W/T/C items or turn the bounded profiles into full KaTeX/Mermaid compatibility.

## Revision and drift

Planned at DxUi `ceffe76a429d15039ceff1f7dfae7e1d7126323e`, clean main. RedTilde main is
`c7456854c8f964bb9ea067295e6435f99d426515` and pins `c600993861a337012e2633356c3d35c5e129bbf3`, API 4.
At inspection there is no diff under `src`, `include` or `Tests` between that pin and the current DxUi HEAD.
Do not confuse source equality with exact binary/environment/runtime-receipt equality.

```powershell
git diff --stat ceffe76a429d15039ceff1f7dfae7e1d7126323e..HEAD -- src include Tests ThirdParty Tools
git status --short
./Test-Changes.ps1 -Explain
```

Compare every changed excerpt and uncommitted path before implementing. Existing finding locations refer to
the old audit revision unless explicitly revalidated here. The old 278-entry classification is historical; it
does not prove every finding survives current main. Keep the 20 refuted and all disputed entries visible.

## Current-source findings rechecked by the lead

| Existing owner | Current source | Verified behavior and required regression | Effort / fix risk / confidence |
| --- | --- | --- | --- |
| W1.1 / commands-01 | `src/Controls/DxUi.RichTextCommands.cpp:1904–1936` | Semantic header has multiple columns, body can have one raw cell; column zero deletion reaches `slots[column - 1u]`. Test short/padded/pipe-less rows, containers, exact result/caret and one undo/redo transaction. | S / medium / high static |
| W1.2 / math-parser-01, math-layout-01 | `DxUi.NativeMath.cpp:69–100`; `DxUi.PreparedMath.h:332–340`; `DxUi.RichTextEditor.cpp:1337–1347` | A text leaf can contain a newline; preparation demands one line and can reject the full semantic document. Reject/normalize under the specified grammar and contain any residual failure per formula. Test other rich content remains rich and idle does not retry indefinitely. | M / medium / high static path |
| W1.5 / semantic-03 | `DxUi.RichSemanticDocument.cpp:341–345` | Rejected math stores its diagnostic in `visit.parent.title`. Test a rejected formula in a titled link and assert link title/destination, math alternative and client-observed UIA HelpText separately. | S / low / high |
| W1.6 / math-parser-08; C4 | `DxUi.NativeMath.cpp:240–241,389–412` | Limits/nolimits are Space atoms; scripts attach to that atom. Use an operator modifier or explicit Unsupported until placement is correct; assert AST binding and actual glyph origins. | M / medium / high |
| W1.6 / math-parser-02/03 | `DxUi.MarkdownInlines.h:1215,1218–1231` | Dollar whitespace/digit rules also apply to explicit slash-parenthesis delimiters. Add spaced slash notation, a following digit and candidate closers inside code/HTML; characterize precedence against the pinned contract before fixing it. | M / medium / high for shared heuristic; precedence needs tests |
| C4 / MA-01/02/22/37 | `DxUi.PreparedMath.h:41–52,62–84,303–310`; `DxUi.NativeMath.cpp:365–368` | Generic text/upright defaults and fixed layout ratios do not establish TeX atom spacing, italic correction, display/text/script styles or operator limits. Add these fundamentals before broader accepted commands. | L / high / high compatibility gap |
| W1.7 / mermaid-01 | `DxUi.NativeFlowchart.cpp:69–119,253–277,290–298` | State alias and description paths bypass common label validation. Alias assigns description; colon appends description to implicit ID. Assert equivalent normalized labels and common markup/control/capacity rules. | M / medium / high |
| W1.7 / family grammar | `DxUi.NativeFlowchart.cpp:147–165,238–250,329–334` | Generic node shapes and pipe labels are shared across families; direction uses a fixed whitespace spelling. Add independent family grammar cases and precise Unsupported/Malformed results before broadening admission. | M / medium / high for code sharing; grammar verdicts need pinned oracles |
| W1.11 / status-truth-01/03 | `ThirdParty/Markdown/profiles/DxUiMath1-target.json`; `mermaid-11.4.1/dxui-family-matrix.json` | Math status says not implemented; state/class families are labeled unsupported despite native subsets. Effective math matrix cap is 128 cells, although public comment says 16x16. Reconcile data, public header, docs and assertions together. | S / low / high |
| T1/T2/T4 | `Tests/Controls/Scope.Tests.RichSemanticDocument.cpp:14–99`; `Tests/Support/Scope.Tests.MarkdownBlocks.h:365–400` | Math tests cover broad parsing and alignment but not TeX fundamentals; flowchart corpus mostly outcomes/counts and special-cases state inside unsupported families. Add semantic/geometry/editor/resource oracles, not more count-only success cases. | M–L / low / high test gap |

All paths in the table without a directory prefix are under `src/Controls`, except named ThirdParty/Tests paths.
No new native reproducer ran during planning; the table deliberately distinguishes static evidence from runtime
qualification. Record fail-before/pass-after evidence as implementation starts.

## Exact work packages and dependency order

1. **W0.1–W0.3:** retain baseline and revalidate critical source/finding drift before fixes. The full file/finding
   sweep may continue alongside focused correctness work; finish it before broad refactoring or claiming a complete
   review. The coordinator's inventory covers tracked first-party code/build/test inputs; initial state is pending.
2. **W1.1/W1.3:** memory safety, source corruption and admission accounting. Use existing command/parser fixtures
   and native/embedded retained editor undo tests; preserve the original 1,324 CommonMark/GFM semantic vectors.
3. **W1.2/W1.5:** local failure containment and one truthful object admission ledger. Amend the owning failure/
   fallback contract; preserve previous complete publication, explicit transient-device retries and allocation policy.
4. **W1.6/W1.12 + T1 + C4:** math parser/recognizer corrections, then atom/style/MATH layout correctness. C5 breadth
   follows the fixed fundamentals. Add AST and glyph/line oracles before increasing supported rows.
5. **W1.7 + T2 + C6/C7:** correct current Mermaid grammar, then graph/routing/shape/subgraph parity. C8 expands
   state/class, sequence, ER/pie/gantt/mindmap and the remaining pinned families through explicit matrix rows.
6. **W1.4/W1.8/W1.10 + T3/T4:** input, embedded host, UIA and full source/fallback/lifetime integration. Add missing
   Accessibility scope mapping for rich/public-header changes; native and embedded must run the same semantic cases.
7. **W3.1–W3.6:** reuse geometry/blocks/objects, then measure on unchanged fixtures. W4/W5 are conditional
   consolidation; introduce adapters before collapsing pipelines. Do not require a universal object abstraction
   to deliver a small correctness fix or a bounded new family.
8. **Library/consumer handoff:** all relevant x64 configurations, six builds and separate native/input/paired gates.
   Update exact-pin consumer evidence in RedTilde; leave other consumer migrations with their current owners.

Waves sharing `DxUi.RichTextEditor.cpp`, `DxUi.RichSemanticGeometry.cpp` or the parser units must serialize their
edits. Independent engine fixtures can be researched in parallel; each cheaper executor gets exact scope,
test commands and stop conditions. The lead verifies the final diff and independently inspects each new oracle.

## Compatibility and test contract to implement

Keep old reference bytes stable: Mermaid 11.4.1 commit `9868f3a4c3784e214d8b6c74277ca542774e7de9` and KaTeX
0.16.11 commit `fbbea3573439d51e6568356f5886053b111b1f88`. Live docs inspected on 24 October show newer
versions; any refresh is a versioned corpus migration. Every row needs a stable ID, provenance, source example,
expected status/reason/range, normalized semantic model, applicable geometry/render oracle and native/embedded/
consumer test IDs. Separate policy exclusions, malformed syntax, valid unsupported syntax and admitted behavior.

- Math: atom classes/spacing, variable/operator fonts, style propagation, scripts/limits, fractions/roots/accents/
  delimiters, matrix/cases/aligned/gathered layout, explicit display envelopes, Unicode and source spans.
- Mermaid: every original flowchart/family fence, then accepted shapes/edges/labels/subgraphs, state/class
  equivalences, sequence semantics and each expanded family's typed model. Include multiple directions, cycles,
  disconnected/parallel/back/self edges, long labels and shape-aware endpoint/routing invariants.
- Both: exact-source local fallback, parent semantic ownership, hit/caret/selection, F2 apply/cancel/undo/redo,
  UIA client traversal and event/name/help assertions, high contrast, RTL, fractional DPI and clipped ink.
- Limits: old and proposed boundary cases, invalid UTF-16, control characters, source/node/record/depth/geometry
  bounds, allocation failure at every observed point, cancellation/replacement during each stage, same numeric
  revision with changed immutable source, device/font/width changes and clean/hidden idle work counts.
- Security/policy: no script/directive/action execution, file/process/network/package access or user macro
  expansion. An external reference tool is an offline development oracle, never a shipped helper invoked by a document.
- Expected output comes from independent specification/reference/hand-checked data. A candidate parser must not
  generate its own expected AST/geometry. Unsupported status for all rows is not evidence of supported compatibility.

Existing test anchors are `Scope.Tests.RichMarkdown.cpp`, `Scope.Tests.RichSemanticDocument.cpp`,
`Scope.Tests.RichSemanticGeometry.cpp`, `Scope.Tests.RichTextEditor.cpp`, `Scope.Tests.RichTextAccessibility.cpp`,
and Support/Embedded semantic/resource fixtures. Reuse shared preparation/heap/fault helpers before adding one.
Register new `Scope.Tests.*` files in `Tests/native-test-files.json` and update test scopes/callers/project inputs.
Changes to `Tools/Generate-NativeFlowchartTestData.ps1` or new tooling also require the Tools policy/inventory/help
and offline negative/generation tests; keep this planning pass free of tool changes.

## Reference projects and adoption decision

The owner explicitly excludes new dependencies. Extend owned C++ mechanisms, studying concrete algorithms and
independent test ideas. MicroTeX, Mermaid renderers and KaTeX are reference material; replacement engines,
Rust/FFI backends and imported rendering or font packages are outside this execution.

- [mermaid-rs-renderer at 3726ccb](https://github.com/1jehuang/mermaid-rs-renderer/tree/3726ccbffe0e8032361eb9668694b24f77858060):
  study stage invariants and route-label obstacles. Its [manifest](https://github.com/1jehuang/mermaid-rs-renderer/blob/3726ccbffe0e8032361eb9668694b24f77858060/Cargo.toml)
  identifies Rust 2024/version 0.3.1/MIT; embedding it would add build/ABI/source/UIA obligations.
- [MicroTeX at 0e3707f](https://github.com/NanoMichael/MicroTeX/tree/0e3707f6dafebb121d98b53c64364d16fefe481d):
  study row atom classes/kerning and delimiter glyph assemblies. Its [README](https://github.com/NanoMichael/MicroTeX/blob/0e3707f6dafebb121d98b53c64364d16fefe481d/README.md)
  documents C++17/CMake/GDI+ Windows integration. Main MIT licensing does not settle all font/data redistribution.
- [KaTeX supported functions](https://katex.org/docs/supported.html) and [options](https://katex.org/docs/options.html):
  use as semantic references; pin corresponding source and configuration for a comparison. Macro/trust defaults
  from a web renderer do not override the library's finite non-executing notation policy.

## Reviewed commands and exit criteria

From `D:\DxUi`:

```powershell
./Test-Changes.ps1 -Explain
./test.ps1 -Platform x64 -Configuration Debug -Suites EditorControls,Embedded,Accessibility
./test.ps1 -Platform x64 -Configuration Release -Suites EditorControls,Embedded,Accessibility
./test.ps1 -Platform x64 -Configuration 'ASan Debug' -Suites EditorControls,Embedded,Accessibility
./validate.ps1
./format.ps1 -Check
./Test-PrePush.ps1 -Explain
```

Use the broader affected Control/Tree/Theme/Rendering/WindowHost suites for shared presentation changes.
Final qualification requires full applicable x64 tests, six builds, real sanitizer detection, recorded WARP/device/
clean/dirty/hidden resource evidence, accepted paired comparison, and native ARM64/input obligations under the
existing contracts. The coordinator gives the current RedTilde commands and final pin tests.

No interactive suite runs without the applicable agreement to time and the governed restoring lease. Preserve
still-applicable human authorization; a historical successful run alone is not present scheduling agreement.
No runtime, gallery, platform or performance claim is established by this plan's static validation.

Stop a slice on mismatched source, an unproved bug, an unplanned public ABI change, unsupported font/license/
resource assumptions, incompatible baselines or repeat verification failure. Do not weaken tests/rebaseline to
finish. Complete work-item code, regression evidence, normative docs, capability status and the required folded
changelog together. The parent audit, rich-editor M5/M6/D4c and shared qualification close only under their own
criteria; app publication is not library acceptance.
