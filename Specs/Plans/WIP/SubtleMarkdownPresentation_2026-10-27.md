# Subtle Markdown presentation and caption input

- [x] Confirm the first supplied image as the requested Markdown style.
- [x] Keep shared rendering in DxUi and preserve the default consumer appearance.
- [x] Add an explicit presentation setting, cache identity and validation.
- [x] Implement heading separators, intrinsic tables, centered display objects and quote fill.
- [x] Independently review geometry, source affinity and bounded preparation/resource work.
- [x] Implement shared nonclient hover/Snap passthrough and caption styles; retain consumer activation/cancellation qualification below.
- [x] Run focused geometry/raster and native editor regression tests; retain the later hidden-capture regression below.
- [x] Validate retained clean/dirty/hidden rendering and simulated device replacement in the final ASan run and consumer Debug/Release.
- [x] Complete all six standalone build profiles and all nineteen x64 suites in Debug, Release and ASan, with zero capability skips and real sanitizer detection.
- [ ] Complete native ARM64, physical input and paired performance gates before claiming full qualification.
- [x] Adopt exact SDK eb0f10b9 in RedTilde; review its three-DPI hidden Markdown and caption captures.
- [x] Synchronize normative documentation, actual consumer capture gallery, qualification archive and changelog.
- [ ] Move this plan to Done only after the remaining gates pass; the original performance/CI queue stays paused.

## Scope and preservation

The user selected the first screenshot's Markdown styling: subtle H1/H2 rules,
content-sized tables with horizontal rules, centered standalone diagrams and
display equations, and a shaded quote surface. Source Markdown, native editing,
selection, history and accessibility remain authoritative. No new dependency is
introduced. Font size and DPI are not inferred from unmatched screenshots.

Implementation starts from SDK `33c3086cfbf7d02118775c2af2eb6f9eb05d0dbf` in the
isolated `ej/markdown-presentation-20261027` worktree. The frozen focus-lifecycle
checkout and its private evidence are unchanged. RedTilde's earlier performance,
CI, platform and merge queue stays paused; this plan does not close those gates.

## Implementation and review

`MarkdownPresentationStyle::Subtle` is explicit on `LayoutOptions` and
`RichTextEditorAppearance`; `Default` retains the existing presentation. Style
changes retire geometry and participate in the cache identity. Subtle separators
own real bottom spacing and culling bounds. Tables measure preferred widths during
bounded preparation, shrink to the available width and preserve physical RTL and
cell alignment. Prepared DWrite alignment owns centering, including caret sites.
Quote and rule colors use semantic theme roles, including high contrast.

Review proportional shrinking at narrow widths, empty/ragged rows, native objects
inside cells, RTL, invalid enum values, cancellation and retained readers. Each
preparation event must preserve the SDK-layout budget. Composition must not measure
or allocate resources. Compare default output and source/history before and after
style switches, and verify actual pixels as well as numeric geometry.

## Validation and evidence

Use canonical `vcpkg-install.ps1`, `build.ps1` and `test.ps1` with checkout-local
`.build` output; retain the first exploratory `D:\DxUi\.build\markdown-style-20261027`
attempt separately. Existing outputs and source-bound proof
packets must not be overwritten. Keep the two-GiB free-space hard floor, monitor
owned work and stop before exhausting the volume. Store this task's private logs
under the new worktree's `.build` and the isolated output root.

Start with `Test-Changes.ps1 -Explain`, focused RichSemanticGeometry and
RichTextEditor suites, then embedded WARP/resource/device-loss obligations. Run
`validate.ps1` and `format.ps1 -Check`. Qualify all required configurations before
reporting a repository pass. Cross-compilation does not establish native ARM64.

The public headers change: the earlier private PB-only V3 ABI profile is inapplicable
to this candidate. Eventual consumer adoption needs an exact source/header/library
identity and full applicable compatibility validation; do not extend that profile's
allowlist or relabel prior executions. Gallery and consumer captures must carry
actual scenario, source, build and DPI provenance. No manual desktop screenshots.

## Current evidence

The first exploratory isolated-output Debug build ended with MSBuild child-node
failures (`MSB4166`), before publishing a complete test executable. It is retained
as a failed attempt; no C++ compiler diagnostic preceded that failure. Tooling
validation did not publish a completion result across the session/tool restart.
Neither attempt is a pass. Capacity subsequently recovered; the final candidate
will use checkout-local outputs so `test.ps1` validates its build attestation.

Two checkout-local x64 Debug builds pass. The first canonical filtered run passes
the geometry/raster case but fails the editor capture fixture before its first
bitmap publication. The fixture now uses the existing non-activating window
pattern. After that repair and the empty-hover invalidation guard, all four direct
Debug checks pass: Markdown geometry/raster, native appearance/source preservation,
NC hover/default passthrough at three DPIs, and caption theme/high-contrast colors.
Raw logs/results are `.build/markdown-style-direct-*-02.log` and
`markdown-style-direct-02.result.json`. These directed checks do not replace full
suite, platform or paired-performance qualification. The first canonical run's
performance report is explicitly unpaired.

Formatting passes in both source checkouts; the SDK's five static validators pass.
Full tooling, final-source suites and consumer adoption remain pending. Existing
gallery configurations retain Default; opted-in consumer captures are reviewed separately.
Source-cache compression preserved bytes, lengths and write times; its
private manifests are under `.build`. The paused RedTilde `fab6084` and SDK `33c`
checkouts remain clean at their original heads.

SDK `83d6a160` builds x64 Debug/ASan Debug and ARM64 Debug. Its full x64 Debug
run passes eighteen normal suites but Embedded aborts during injected allocation
failure. The isolated image-resource diagnostic exits 197 with a symbolized stack
through `PreparedSemanticGeometry::State` construction. MSVC Debug's default and
move vector constructors allocate iterator proxies inside `noexcept`; new decoration
and table-staging containers exposed that fault path. The repair makes decoration
and staging ownership opt-in, uses fallible size constructors, and keeps per-table
owners stable across collection growth. A two-table Subtle allocation sweep is
added. Final-source rebuilding and rerunning remain required.

The repair's x64 Debug build passes. The isolated image-resource failure sweep
now passes all 218 loaded-geometry allocation points and its raster-driver cases.
The complete Embedded executable passes **460,383 checks**, including the new
two-table Subtle allocation sweep; 1,000 warm composites make zero C++ allocations.
The native appearance/geometry checks also pass, including the targeted quote-fill
pixel. These direct diagnostic executions precede final canonical suite/matrix
closeout and do not retag `83d6a160` or establish paired performance acceptance.

RedTilde's isolated consumer of `83d6a160` passes its x64 Debug Full run, including
the caption lifecycle at all three DPIs and 32 actual hidden captures with provenance.
The first catalog refusal and subsequent stale-receipt refusal are retained; its
reviewed case catalog now includes the new Editor case. Those successful app runs
do not qualify the library fault path or the upcoming repaired SDK pin. Visual
review also found input/card theme roles could coincide; quote fill now derives
from an eight-percent ink blend, with a targeted empty-background pixel assertion.

SDK 2bb1aaaf passes all nineteen normal x64 Debug and ASan Debug suites, including the real sanitizer detector. Its x64 Release and ARM64 Debug/Release builds pass; ARM64 ASan and native ARM64 remain pending. These are source-bound results, not evidence for the subsequent capture overload.

A stronger consumer capture check detects identical resting/close/maximize images despite live hover state. The app read its D2D target after swap-chain Present, whereas the SDK stages capture before Present. The follow-up exposes explicit hidden capture through the canonical staging path, removes the consumer readback copy and tests first-frame and repeated hover pixels at three DPIs. Earlier standalone hover captures are rejected as visual-state evidence; lifecycle/cancel and source-preservation results retain their narrower scope. Final-source capture/matrix qualification is required.

## Final focused follow-up status

Runtime/library producer: `eb0f10b91bb8de19b0d7671cc7078fd0c1f5e767`.
All six standalone build profiles pass: x64 and ARM64 Debug, Release and ASan
Debug. Each x64 configuration passes all nineteen noninteractive suites with
zero capability skips; the ASan run verifies real heap-use-after-free detection.
All five validators and the complete tooling suite pass at eb0. Subsequent
build/runtime producer a0f67fd3 differs from eb0 only in WIP/changelog metadata;
archived receipts retain their actual producer, artifact and toolchain identities.

Retain the initial full ASan run: eighteen suites pass, but EditorControls misses
its unchanged twelve-second dense-source deadline while compilation is active
(6,000 blocks, 12,012 ms; maximum Source advance 2,994,878 us). The unchanged
complete EditorControls retry and final full run pass after compiler/linker work
ends. This does not attribute the original delay or establish quiet-host
performance acceptance. No deadline, assertion or comparison policy was relaxed.

RedTilde adopts exactly eb0. Its six consumer builds pass. Each x64 configuration
passes all 67 native cases, all 32 hidden captures and crash/recovery probes;
its ASan run verifies actual sanitizer detection. Capture/resource/device-loss
coverage includes the corrected pre-Present staging and synthetic pointer
lifecycle at 96/144/192 DPI. Actual capture pixels and provenance, including the
ASan gallery, remain byte-bound in the consumer qualification archive. Native
source identity is 2166a79bc250d6042ad5698927eb0d3ac3e95a22e1525ff4d7934808590e0e87;
the three runtime executions retain distinct full source identities.

Independent read-only audits replay the first SDK Debug packet and the four new
consumer build profiles, verify their hashes and exact pin/compiler bindings,
and confirm the SDK metadata successor has no native changes. MSVC 14.52.37025
compiles, links and archives native ARM64 targets. The restore log's separate
14.53 x64 detection concerns only the vcpkg CMake helper triplet.

Native ARM64 execution, physical input and paired performance remain open.
Cross-builds and unpaired smoke measurements cannot close them. Earlier 2bb/83
executions remain historical. The original performance/CI/keyboard/Menu/merge
queue stays paused, including hosted RedTilde ARM64; no new CI, push, merge or
real-focus suite was performed. Keep this plan in WIP.

Preserve capacity-stopped build/tooling attempts and refused source-drift runs;
none becomes a pass. D: ReFS compression changed no files. Automatic approval
review rejected compiler-cache cleanup and subsequently the user-authorized
.Perf cache/fixture and single-fixture removals with "blocked by policy" before
launch. Zero files were removed by those commands. Capacity changed through
unattributed external activity; remaining qualification used the existing C:
workspace. The original D: cleanup record later became unavailable; its observed
denial summary is explicitly reconstructed in the C: continuation notes rather
than presented as recovered raw evidence. Never bypass the declined deletions.

Original queue files remain byte-preserved, with additional verified C: copies.
Continuation notes and exact remaining gates are saved under the consumer's
`.build/markdown-caption-continuation/`. Final metadata updates do not relabel any
original producer or claim repository-wide/performance acceptance.
