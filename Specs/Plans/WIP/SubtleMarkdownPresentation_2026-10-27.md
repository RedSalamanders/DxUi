# Subtle Markdown presentation and caption input

- [x] Confirm the first supplied image as the requested Markdown style.
- [x] Keep shared rendering in DxUi and preserve the default consumer appearance.
- [x] Add an explicit presentation setting, cache identity and validation.
- [x] Implement heading separators, intrinsic tables, centered display objects and quote fill.
- [x] Independently review geometry, source affinity and bounded preparation/resource work.
- [x] Implement shared nonclient hover/Snap passthrough and caption styles; retain consumer activation/cancellation qualification below.
- [ ] Run focused geometry/raster and native editor regression tests.
- [ ] Validate retained clean/dirty/hidden rendering and device replacement.
- [ ] Run applicable Debug, Release, ASan and ARM64 build/runtime gates.
- [ ] Adopt an exact reviewed SDK commit in RedTilde; capture the example through its hidden harness.
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
Full tooling, final committed-source suites, consumer adoption and actual app
captures remain pending. Existing default gallery pixels are unchanged by opt-in
styles. Source-cache compression preserved bytes, lengths and write times; its
private manifests are under `.build`. The paused RedTilde `fab6084` and SDK `33c`
checkouts remain clean at their original heads.
