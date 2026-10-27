# Subtle Markdown presentation and caption input

- [x] Confirm the first supplied image as the requested Markdown style.
- [x] Keep shared rendering in DxUi and preserve the default consumer appearance.
- [x] Add an explicit presentation setting, cache identity and validation.
- [x] Implement heading separators, intrinsic tables, centered display objects and quote fill.
- [x] Independently review geometry, source affinity and bounded preparation/resource work.
- [x] Implement shared nonclient hover/Snap passthrough and caption styles; retain consumer activation/cancellation qualification below.
- [x] Run focused geometry/raster and native editor regression tests; retain the later hidden-capture regression below.
- [x] Validate retained clean/dirty/hidden rendering and simulated device replacement in the final ASan run and consumer Debug/Release.
- [ ] Run applicable Debug, Release, ASan and ARM64 build/runtime gates.
- [x] Adopt exact SDK eb0f10b9 in RedTilde; review its three-DPI hidden Markdown and caption captures.
- [ ] Synchronize normative docs, gallery and changelog; move this plan to Done after its gates pass.

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
The canonical x64 Debug and ASan Debug builds and ARM64 ASan cross-build pass.
All five validators and the complete tooling suite pass from the D: qualification
checkout. The final full x64 ASan execution passes all nineteen noninteractive
suites with zero capability skips and a real heap-use-after-free detector.

Retain the initial full ASan run: eighteen suites pass, but EditorControls misses
its unchanged twelve-second dense-source deadline while compilation is active
(6,000 blocks, 12,012 ms; maximum measured Source advance 2,994,878 us). The later
unchanged complete EditorControls retry passes after compiler/linker processes
exit, followed by the successful full run without compilation. This does not
attribute the original delay or establish quiet-host performance acceptance.
No source deadline, suite assertion or comparison policy was relaxed.

RedTilde adopts this exact source/header/library pin. Its x64 Debug and Release
runs each pass 67 native cases, 32 hidden captures and resource/device-replacement
probes. Its manifest preserves the nine reviewed Markdown/hover captures at three
DPIs; byte-bound JSON and PNG files have explicit Git attributes. A final read-only
review finds no actionable issue in capture/lifetime or synthetic-pointer staging.

Final standalone x64 Release and ARM64 Debug/Release builds, x64 Debug/Release
runtime suites, remaining consumer ASan/ARM64 configurations, native ARM64,
physical input and paired performance stay open. Earlier 2bb/83 profile results
are historical and cannot be relabeled. The consumer Release archive builds and
runs successfully, but that is not a standalone all-target Release pass.

Both volumes are near their reserved capacity margins. A C: build was stopped
during attestation and an earlier tooling attempt was interrupted; neither is a
pass. The later canonical Debug build does pass. D: is ReFS: attempted compression
reported unsupported and changed no files. Later C: compression commands selected
closed objects but their tool summaries report zero newly compressed files; the
observed free-space recovery is unattributed. Automatic approval review rejected
both bulk and twelve-file compiler-cache cleanup commands with "blocked by policy";
those commands deleted nothing and were not retried through another mechanism.
The user was asked to free capacity or keep the remaining qualification gates open.

The original performance/CI/keyboard/Menu/merge queue remains paused. Frozen
RedTilde fab6084 and SDK v29 33c remain clean at their exact heads, and the two copied
RedTilde queue files match their original SHA-256 values. No push, merge, new CI run
or real-focus suite was performed. Keep this plan in WIP. Exact continuation
commands are saved privately in the active consumer's
`.build/MARKDOWN-CAPTION-CONTINUE.md`.
