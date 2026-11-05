# Prepared diagram label backgrounds

Implementation: `2a7e7c28f95a70193e4a2f68a89001d54b4d2131`, API 4 unchanged.

Native arrow labels and class/ER cardinalities use the actual diagram canvas color when their retained ink
reaches the 4.5 contrast criterion. Preparation composites translucent ink and searches both lighter and darker
surfaces, choosing the smallest sufficient RGB change. If ink opacity makes that criterion impossible, it
chooses the strongest available endpoint without replacing the ink. Paint reuses a retained brush and performs
no contrast search or label containment scan. Group titles, notes and explicit selection/export fills preserve
their own surface contracts.

The ordinary brush follows canonical layout retirement and render-target binding. Its pure color remains
cached for the same ink/canvas pair; a retired brush is reacquired from the current host without repeating
the contrast calculation. This is resource ownership repair, not a claim of a measured speed improvement.

## Independent native regressions

`TestNativeDiagramLabelBackdrops` covers nine arrangements: state, flowchart, sequence, class, ER, horizontal
and vertical groups whose labels fall outside, a wide group containing its label, and a sequence note. Four
readable/low-contrast themes, 96/192 DPI and explicit/default label fills produce 144 native raster renders.
The tests independently check contrast and minimal fallback, near-black ink, exact background pixels,
cardinalities, retained group/note fills, unchanged input brushes and stable layout/preparation counters.

`TestNativeDiagramLabelBrushRetirement` uses an actual hidden RichTextEditor. It retains the old label brush,
evicts the host cache with more than 256 distinct brushes, changes source, poisons the retired brush magenta
and checks the next native capture. Against unchanged `344199d56806d22ff1e46d8afafc903875573e01`, the new
regression fails because the later revision can still paint with that old brush. The corrected library passes:
the host generation is unchanged, its brush is fresh, source/focus are preserved and no poisoned pixel appears.
The direct fixed diagnostic records 0.247 seconds for retirement and 0.107 seconds for the raster matrix;
these unpaired fixture durations are not a performance comparison.

All six documentation gallery sheets are regenerated from the clean implementation commit and reviewed:
embedded sample, Light, Dark, High Contrast, Rainbow Light and Rainbow Dark. Their original source and image
hashes remain in [gallery provenance](../../docs/gallery/generation.json).

## Exact-pin consumer verification

The [qualification projection](qualification.json) records six zero-warning/zero-error builds and the eight
affected native suites in x64 Release, Debug and ASan Debug. The isolated real ASan detection probe passes.
The remaining 11 Release suites pass against the same compiled profile, completing the 19-suite
noninteractive union. All 35 suite executions record zero capability skips. ARM64 is cross-build coverage only.

RedTilde explicitly adopts this API-4 commit in application implementation
`2879d4dc3786866227149d0267d06bedcf6a6cfd`. Its
[original hidden captures and pixel observations](https://github.com/RedSalamanders/RedTilde/tree/504bef2b887577346f5b2e4ce7f16de009371090/Measurements/DiagramLabelBackgrounds_2026-11-04)
compare the same synthetic state diagram at Light/Dark and 96/192 DPI. All eight corrected Review/Save
background samples exactly equal their unchanged neighboring canvas. The old API-4 `8c6c2818` executable
differs in all eight samples. Consumer acceptance does not advance any other application's pin.

## Hosted Release follow-up

[Main qualification 41849905967](https://github.com/RedSalamanders/DxUi/actions/runs/41849905967) passes at
`ab850a40e1026cdbbf62153db9a0a55156d2775c`, retaining the API-4 implementation above. Native x64 executes
all 23 selected suites: the 21-suite ordinary union plus `MenuResourceScaling` and `MenuTextLayoutResources`.
Native ARM64 executes all 21 ordinary suites. Both roles report zero failures/skips and successful Release
assertion probes. Downloaded original canonical receipts match every hash in the hosted regression metadata.

The combined test gate, both production SDK roles and workflow completion pass. Each SDK archive reports
test mode disabled and a successful extracted-consumer smoke. The downloaded ZIPs match their receipt sizes
and SHA-256 values:

| Archive | SHA-256 |
| --- | --- |
| `DxUi-SDK-0.1.1546-x64.zip` | `0C573CA05BCA86D2A300DE002F9F1FDB44D56D74AF38F1FFFE7234E47CB226FE` |
| `DxUi-SDK-0.1.1546-ARM64.zip` | `E990D47185D55126C33B4872A4FD3C7A6C90F52B502804E8B71EA815B9681AE2` |

Original regression metadata, canonical receipts and SDK ZIPs remain in the approved private evidence workspace.
This later hosted record supplies native ARM64 Release execution; it does not retag the earlier local cross-build
matrix. Optional ARM64 menu-resource fixtures, other runtime profiles, physical input/device behavior, paired
performance, notice reconciliation and consumer migrations retain their existing owners. The SDKs are internal
qualification artifacts; this record does not publish a release or adopt API 5.

## Retained failures and limits

The first two raster attempts had incorrect group-containment expectations. Independent native geometry
observations proved both labels outside their groups; a separate wide-title group supplies the inside case.
The original failed runs and executable/PDB pairs remain retained, alongside the brush-retirement counterfactual.

The superseded `344199d5` ASan WindowHost run
`20261104T122655805Z-30504-5369f183627843f697764e45293344ec` failed its secondary-button ownership assertion.
An unchanged-binary quiet replay passed. Its cause is not established; neither that replay nor later qualification
erases the original observation.

Full tooling in the long isolated NTFS checkout encountered hosted-MSVC fixture launches at 272-character
paths and one hardlink fixture at 261 characters. Canonical short-path validation passes. The unchanged NTFS
run-evidence fixture also passes all 41 checks with its supported short scratch root. These are distinct
path-dependent observations; the failed long-root aggregate is not reported as a pass.

Raw runs, logs, binaries/symbols and verified preservation manifests remain private. Functional raster/resource
checks and ordinary unpaired FPS/memory observations do not establish paired performance, physical input,
additional native runtime profiles or product-wide release acceptance. These gates stay with the
[single active plan](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).
