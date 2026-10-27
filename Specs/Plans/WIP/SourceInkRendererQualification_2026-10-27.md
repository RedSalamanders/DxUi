# Source ink renderer qualification

## Checklist

- [x] Review the source/test diff and preserve exact source identity; the private feature branch is `codex/source-ink-renderer-qualification-20261027` at `a5b0067efe9c03a4a5aecd87bc83f95a25b1b826`. `src` and `include` are byte-identical to pixel-tested candidate `bc2ac1d517d10e2a6713195790c0a4052e7b028b`; the renderer header SHA-256 is `1F0877AA4A255CA35409E6B09B4BA9B80A4F4D242316D68890B03D61680F3ECD`. This source identity review does not close the current-source matrix, adoption or merge gates.
- [x] Extend the independent pixel oracle to transformed translucent gradient brushes and a non-axis-aligned context transform for segmented monochrome, proportional, actual CJK fallback, and color-font runs. The filtered Release test passes in 0.468 seconds with test source SHA-256 `87CADA7200D01FBAFB6D72F61BDF87A15F280CCA048D310C447B67ABEBDC91FF` and executed PE `3488F9C3A51F74427B56F4C2C2C79643096B54F77DA9F4C2A58366CFF942DB05`; this project-only diagnostic is not a full current-source qualification.
- [ ] Run all six SDK CI/source-exact profiles against the final reviewed source. Preserve exact head, tree, artifact identities, skips and failures.
- [ ] Run the three x64 SDK runtime profiles (Debug, Release and ASan Debug), including the real ASan detector, with zero unexplained skips. Keep prior private focused raster evidence distinct from this qualification.
- [ ] Qualify each intended consumer at its final exact DxUi pin and supported matrix. For performance-sensitive adoption, run fresh paired comparisons against each consumer's actual PR base with the accepted profile and policy; do not reuse the older study below as acceptance evidence.
- [ ] Finish the required documentation/evidence review, fold any genuinely new changelog fragment if one is introduced, and merge only after source, test, CI and adoption gates are reviewed.
- [ ] Keep parent-owned policy, physical input, hosted RedTilde and ARM64 holds separate. These are not closed by SDK renderer qualification.

## Scope and current status

This WIP owns only the bounded DirectWrite source-ink draw route in DxUi and its SDK qualification. The shared feature is source ink for `RichTextEditor` Source view. The renderer must preserve the shaped glyph geometry, syntax colors, selection precedence, fallback-font behavior and canonical rendering output. A private candidate exists, but all-source functional qualification, performance acceptance and consumer adoption remain pending. The original source snapshot plan remains the owner of source sharing and parsed-data reuse; this plan takes only the renderer experiment out of its appendix.

The intended renderer contract is in [Controls and layout](../../UI/UI_ControlsAndLayout.md). Current controls: at most 32 clipped ink spans per direct-draw/prepared-layout call, 256 glyphs and 512 UTF-16 units per run. Monospaced monochrome runs may retain the existing brush segmentation. A proportional monochrome or color-font run may use the intact original shaped run only when every covered glyph resolves to one identical borrowed `ID2D1Brush` pointer. Color fonts additionally require a successful runtime `ID2D1DeviceContext7` query and use `ID2D1DeviceContext7::DrawGlyphRunWithColorSupport`. The entire run list and all source/brush/description constraints are preflighted before paint; a preflight refusal may use the canonical layout path with all colors. No target, brush, or device-context capability is retained by this route. Device replacement and a copied prepared layout must preserve parity. Errors after painting begins follow the host/device error path and must never trigger a second paint.

The 65,536 staging-event presentation limit is a separate source preparation bound and is not changed by the renderer's 32-span paint bound.

## Evidence ledger

### Precursor and exact S5 observation

- Canonical Release checkpoint `1e28fdff0d1d60fc991d8e96c8f4355e98180b28` preceded a preserved fixture-DPI-state failure; keep that failure and its original artifacts. It is not a passing raster receipt.
- Candidate `bc2ac1d517d10e2a6713195790c0a4052e7b028b` is a test-only incremental raster pass (`0.460`); it is not full qualification. The production/include bytes later present at `a5b0067efe9c03a4a5aecd87bc83f95a25b1b826` match this pixel-tested candidate, while the final test changes still require execution on exact current source.
- Actual Source S5 trace `8afd668071043462c90db0a327fc7a51c9c9aa39`, pinned by RedTilde app revision `d5212e4f23568a1348dbee39989c8f5e010826d3`, emitted `CALL_LINKS` for 653 display units and 26 clipped source-ink ranges, then `DRAWN`; it did not fall back. This is the app-level observed result for that trace, not evidence of a performance win.
- The earlier private32 trace is different evidence: its actual S5 call also had 653 display units/26 ranges; exact LF no-paint was accepted, then a two-glyph proportional run was rejected as `REJECT_FONT_NOT_MONOSPACED`, followed by canonical fallback. There was no source-ink BeginPaint or Drawn event in that earlier trace. Do not conflate it with `8afd`.

### Paired study (descriptive only)

The 16-process, two-block, 64-cycle study seeded `20261027` compared app `1fb2fbbc32d08baee36a2cdfdc9ceaeaa20d3b26` / SDK `bc2ac1d517d10e2a6713195790c0a4052e7b028b` with qualified app `a29c9f8447fe06b33b3a9c611dda5d6e6d29205e` / SDK pin `4077f57df8207eac6c39c43437feb45b19d23cf8`. The Split owner CPU delta was `-7.2736445309%`; command was `+1.458596%` and private was `+1.45197%`. All 12 metrics had no resolved change, and CPU and private controls were unstable. The independent 4,263,527 checks passed. The observer captured 91 samples with maximum gap 1.1007 seconds and completed. No quiet-host claim was established. These are descriptive observations only: no causality or band change is established, and there is no acceptance result.

### Earlier private32 experiment and preserved limits

The old private candidate had a focused Release raster pass for expanded typography/DPI/transform, target replacement and 26/32/33-span boundary cases. It accepted exact source CR/LF no-glyph callbacks only after text matching, and LF/CRLF matched the canonical oracle; mismatched text, invalid/out-of-bounds intervals, effects, sideways or odd-bidi runs, and other empty callbacks rejected. The old implementation's proportional Segoe UI counterfactual failed at `mono=false` (25 glyphs). The earlier candidate's filtered raster test passed exact canonical comparisons for proportional Segoe UI, CJK fallback, emoji color-font rendering across 96/120/144/192 DPI and fractional origins/transforms, mixed Consolas/CJK/emoji CRLF source, forced prepaint Context7 refusal, heterogeneous-brush rejection, and a retained mixed-font layout copy on a second target. It was one focused no-activation test after a diagnostic incremental build, not full qualification. Later review added a Context7-missing-host-safe oracle, a color run at text offset 2, and fail-closed fallback-family diagnostics; those newer test bytes had no execution at the time of that record. Preserve all original logs and receipts and let exact-source qualification determine their status.

The experiment does not establish a speedup or broad rendering equivalence beyond its tested fixtures. It adds no dependency or public API. It uses existing DirectWrite/Direct2D interfaces and a borrowed, target-local Context7 capability. Older systems or unsupported targets may reject during preflight and draw through the canonical color-preserving path; errors after painting begins follow ordinary host/device error handling and must never trigger a second paint.

## Test and review plan

- Counterfactual: demonstrate a real uniform-brush proportional run rejected by the old candidate before enabling the route.
- Canonical pixel oracle: compare Source ink output byte-for-byte with independent full-layout drawing over actual proportional, CJK fallback, color emoji and mixed-font CRLF fixtures. Include 96/120/144/192 DPI, fractional origins, identity and transformed targets, and a color-font run with the two-unit positioned prefix.
- Whole-run geometry: confirm the original `DWRITE_GLYPH_RUN` and description are forwarded unchanged; test advances/offsets, cluster-to-display/source mapping, bidi/sideways and malformed input guards. Preserve existing monospaced segmentation coverage.
- Preflight/fallback: reject heterogeneous brushes and unavailable Context7 before any paint. On real QI-miss hosts, exercise `PreparedLayout::Draw` canonical fallback and compare to an independent full-layout oracle. Forced-unavailable test-seam results must be labelled as prepaint refusal plus manual canonical oracle, not as an actual missing-runtime-interface test.
- Lifetime/device: render a retained prepared-layout copy on a second target and compare both the candidate and an independent full-layout oracle; verify target replacement does not retain a device-bound capability or brush.
- Gallery/design review: no visible UI change is intended. The exact native pixel oracle is the review evidence; do not make manual desktop screenshots for this renderer-only change.
- Stop if pixel parity differs, if any unsupported run reaches paint, if fallback cannot preserve all colors, if a Context7/device object escapes its target lifetime, if qualification artifacts bind to different source bytes, or if performance policy remains unresolved. A focused pass cannot waive these stops.

## Ownership and closeout

This plan is the active SDK renderer owner. The UI contract is normative; `docs/rich-text.md` documents usage only. Detailed adoption measurements and current consumer decisions belong to the RedTilde parent plan. Parent physical-input, hosted RedTilde and ARM64 holds remain separate. Move this plan to Done only after all required SDK gates pass, the reviewed consumer pins/matrices are recorded, authoritative docs are current and exact evidence is retained. Do not claim acceptance from private diagnostics or from a descriptive paired study.
