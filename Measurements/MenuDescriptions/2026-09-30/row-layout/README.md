# Described-menu row layout, 2026-09-30

Plan item: "evaluate one formatted layout per described row"
([memory plan](../../../../Specs/Plans/WIP/MenuDescriptionMemory_2026-09-27.md)). Library scope; no consumer pin changes.

## Question and decision

The plan's isolated twelve-row probe put separate primary and secondary layouts at 453,754 live bytes and one
formatted layout per row at 250,346. Main already shared identical text within a popup, though, and that sharing had
taken the repeated-label probe to 246,338 bytes, so the real question was whether one layout per row beats main.

**Decision: implemented.** A described row holds one DirectWrite layout (the label, an empty spacer paragraph and the
description) and draws it with one call. Against main's two shared layouts:

- **Contract and pixels are kept.** All 420 captures of a five-theme, eight-fixture, four-DPI matrix and all six
  gallery sheets are byte-identical to main's, with identical geometry and debug state.
- **Live heap falls about a third wherever labels differ** (34% to 38% for twelve to 48 rows) **and rises a little where
  every row repeats one label** (+1.6% to +6.9%), the shape main's sharing was built for. A layout costs about 17 KB
  plus about 30 bytes a character, so sharing beats a layout per row only where one label repeats in about seven rows of one popup.
- The sharing is gone, because one layout holds both fields and labels can no longer be shared alone.

The rise where every row repeats one label is a measured degradation for that shape, which the
[performance contract](../../../../Specs/Core/Core_PerformanceAndResources.md) leaves to the developer. The options,
with their measured cost: **accept it** (what this branch does: about 200 KB to 800 KB saved for twelve to 48 distinct
rows against 6 KB to 84 KB spent for the same counts of one repeated label); **keep main's sharing for rows whose label
repeats in about seven or more rows** (a hybrid with a second drawing and measuring path, for a case only the synthetic
fixtures exercise; not built); or **keep main's two layouts** (reject this branch: no saving for distinct labels).

This is open-menu live heap, not the waived common-scene flag, and it is not a paired benchmark.

## Sources and environment

| Item | Value |
| --- | --- |
| Baseline | main `a0b4934` (described menus with popup-local sharing) |
| Candidate | this branch; `git diff a0b4934 -- src include` is the change (`DxUi.Menu.cpp`, two diagnostics in `DxUi.h`) |
| Machine | AMD Ryzen AI 7 PRO 350 (8C/16T), 55.6 GB, Radeon 860M driver 32.0.22024.19001, balanced power plan |
| OS | Windows 11 Enterprise 10.0.26200; `DWrite.dll` 10.0.26100.1, `d3d10warp.dll` 10.0.26100.9278 |
| Toolchain | Visual Studio 18 Insiders, MSVC 14.51 (`cl` 19.51.36257), Windows SDK 10.0.26100.0, x64 Release |
| Fonts | `Segoe UI Variable Text` and `Small` are not families of the system collection here, so DxUi resolves both formats to `Segoe UI` (13 and 11 DIP), as it does wherever the collection lacks them |

The baseline and the candidate executables are built from the same test sources (the v5 fixture, below), so the
matched comparison runs identical fixtures: `DxUi.ControlTests.exe` SHA-256 `1AD3DD00...36A47` (main's library) and
`864E3013...A03301` (the branch). `MenuResourceScaling` v4 on the retained main build gave the same values as v5 for
every case v4 has.

## Live heap of an open menu

`MenuResourceScaling` v5 (`./test.ps1 -Suites MenuResourceScaling`, foreground): open minus before, bytes busy in all
process heaps (`Tests/Support/HeapDiagnostic.h`), median of the last 16 of 32 cycles, 320 openings at one window
extent. Both builds' medians were identical in three runs (and main's v4 run gave the same values as its v5 runs). `Get-MenuLiveBytes.ps1`
reproduces the table from a suite log.

| Entries / described / captions | Main | Branch | Change |
| --- | ---: | ---: | ---: |
| 12 / 12 / distinct | 596,882 | 395,910 | -200,972 (-33.7%) |
| 24 / 24 / distinct | 1,096,836 | 695,752 | -401,084 (-36.6%) |
| 48 / 48 / distinct | 2,100,287 | 1,298,940 | -801,347 (-38.2%) |
| 12 / 12 / repeated | 388,728 | 394,952 | +6,224 (+1.6%) |
| 24 / 24 / repeated | 660,862 | 692,886 | +32,024 (+4.8%) |
| 48 / 48 / repeated | 1,208,673 | 1,292,258 | +83,585 (+6.9%) |
| 12 / 1 / repeated | 171,830 | 153,804 | -18,026 (-10.5%) |
| 12 / 2 / repeated | 191,616 | 175,780 | -15,836 (-8.3%) |
| 12 / 4 / repeated | 230,740 | 219,348 | -11,392 (-4.9%) |
| 12 / 8 / repeated | 309,550 | 307,014 | -2,536 (-0.8%) |
| 12, 24, 48 / 0 (plain) | 106,689 / 115,455 / 132,831 | 106,705 / 115,471 / 132,847 | +16 each (two pointers in `MenuPopup`) |

Closing returns what the menu held in both builds (the median closed-minus-before is 0). `DebugGetContextMenuResources().rowLayouts`
reads 12 for twelve described rows on the branch and 24 on main. Per row, main's marginal cost is about 41.8 KB with
distinct labels and 22.8 KB with one repeated label, the branch's about 25.1 KB either way; the two meet where a label
repeats in about seven rows.

The scratch probe `MenuRowLiveBytes` (`harness/`) repeated the comparison with its own rotation and gave the same
deltas to the byte in three runs per build (for example 591,762 to 390,790 for twelve distinct rows and 383,672 to
389,896 for twelve repeated ones; its absolute values differ a little from the suite's, which rotates other cases in its cycles).

### Isolated native layouts

`MenuTextLayoutResources` v2 (twelve rows at 396 DIP, no popup): live heap after metrics, last 16 of 32 cycles.

| Mode | Created | Measured | Released |
| --- | ---: | ---: | ---: |
| Primary only (12) | 6,624 | 226,272 | 0 |
| Secondary only (12) | 6,648 | 227,482 | 0 |
| Pairs: what a row held (24 layouts) | 13,272 | 453,754 | 0 |
| One formatted layout per row (12 layouts) | 13,728 | 253,762 | 0 |

The standalone probe (`probe/`) adds why: a layout with one 50-character paragraph holds 18.9 KB live, with both
paragraphs 20.7 KB, with the description's size 21.1 KB and with the spacer's size 21.2 KB; the drawing effect adds
nothing it could measure. The cost is about 17 KB (a layout of one character holds 12.1 KB, of ten 17.9 KB) plus about 30 bytes a character (100
characters 20.5 KB, 800 characters 41.8 KB), so a second layout is what costs.

## Pixels and geometry

- **Gallery.** `gallery.ps1 -Configuration Release -Platform x64 -SkipBuild` on both builds: all six sheets are
  byte-identical (and identical between two runs of main, so the comparison is exact):

  | Sheet | SHA-256 |
  | --- | --- |
  | `embedded-controls.png` | `566E96606C40388F2B6795DEEFD0C590515CB6F65A0ED3C723D12DF063D3E4EC` |
  | `theme-controls-dark.png` | `B78EEFB2CDDEB73D373330C1A395D3A5F0E77B4FA278F3F162C05DA471861366` |
  | `theme-controls-high-contrast.png` | `7CA835CBA6F078389B9B5AC811CF8EEE39506EE09C7E251C0B7514C00A280FBF` |
  | `theme-controls-light.png` | `CB02C752A6022F3F6A1F4D768CB5C2FD453144AE9798C9C660357C68DCE1511C` |
  | `theme-controls-rainbow-dark.png` | `5DC11AD1E92CD3E49198BE45CD678CF8411F1EB57126CC7EE0014CB439065B88` |
  | `theme-controls-rainbow-light.png` | `F86C529EE09862F745C61C34C502DA7F6DC790DB926E12B725C0614C93EBB526` |

  `docs/gallery` is unchanged for that reason.
- **Capture matrix** (`harness/DxUiTests.RowProbe.h`, suite `MenuRowProbe`, nonactivating): five themes (light, dark,
  rainbow light, rainbow dark, high contrast) x eight fixtures x DPIs 96, 120, 144 and 192 (by `WM_DPICHANGED`), and at
  96 and 144 also keyboard selection, hover, End and wheel states: 420 popup captures. The fixtures are the gallery's
  two-row menu, twelve distinct and twelve repeated labels in a scrolled viewport, a mixed menu (icons, accelerators,
  Toggle, Radio, Info, a disabled row, headers, separators, a submenu row, mnemonics, plain rows), short labels,
  scripts (emoji and a ZWJ sequence, Hebrew, Arabic, bidirectional text, CJK, Thai, stacked combining marks), and edge
  cases (line breaks and tabs in descriptions, a trailing or leading newline, an empty label, a 90-character token, a
  label with a line break, `&&`). Each state is pinned by delivered messages with nothing pumped before the capture, so
  the physical cursor cannot change it (an earlier version of the harness was not deterministic for that reason). Main
  was captured three times: the same manifest each time. **All 420 PNG files of main and of the branch are
  byte-identical, and so are the complete manifests** (hashes of the pixels, hover, keyboard, scroll, content height and
  every row's rectangles, line counts and layout widths), SHA-256
  `2E42EE7CDBC1FF1C0427883BC10268BA080EE6079AB0666F80BA96404E698841`. `captures-manifest.tsv` keeps the pixel hashes
  and state.

## What the standalone probe showed

`probe/probe.cpp` (scratch C++ against DirectWrite and a Direct2D device context; results in `probe/results.txt`) drew
two separate layouts the way main did and one formatted layout, and compared:

- 240 pixel comparisons (15 texts, four DPIs, fractional origins) and 140 more across seven font pairs (other families,
  italic, semibold, a larger description than label, a bold label) and two widths: no differing pixel.
- Height and line counts: 588 cases; the float sum of the layout's line heights equals the height a separate layout
  reports bit for bit, so `ceil()` agrees exactly.
- The spacer: its line height is linear in its font size, so one measurement at any size gives the size that needs
  `ceil(label) - label + 3`; the first correction lands within a few millionths of a DIP in every case.
- Tab stops are a property of the whole layout and differ by font size (52 DIP for 13, 44 for 11): a description with
  tabs differs by 196 pixels until the layout takes the small format's tab stop, and then by none.
- A shared brush recolored between rows draws as dedicated brushes do (Direct2D resolves the color at the draw call).
- Edge cases match (line and paragraph separators, CRLF, empty and newline-only descriptions, zero-width and no-break
  characters, mixed bidirectional text, and an unterminated bidirectional override, embedding or isolate at the end of
  the label, which does not reach the description), except a label that ends in a carriage return, which the first
  newline joins into a line break (478 pixels). `DecodeMenuItemText` trims a label's trailing white space, so none
  reaches the layout.
- Preparing 48 rows takes 0.97 ms as separate layouts, 2.2 ms as one layout per row, and 1.1 to 1.3 ms when each row
  starts from the spacer size the last row settled on (what the code does). These are isolated layout times, not a
  paired benchmark of the library.

## Tests and mutants

Six tests in `Tests/Controls/DxUiTests.Menu.cpp` (both the Menu and NewControls lanes): the layout count (one per
described row, none for others, all returned on close), the gap and sizes against separate layouts measured in the test
(heights, line counts, widths, the 3 DIP gap, the row's padding, through DPI changes, for eight label and description
shapes), colors (enabled, disabled at 40%, hovered and neighboring rows; color glyphs), tab stops, the host's body and
small formats agreeing on every property one layout holds for all its text, and the description surviving a lost device
(the capture is unchanged to within 4/255 per channel). `mutants/Apply-Sabotage.ps1` adds ten runtime mutants (`DXUI_ROWLAYOUT_SABOTAGE=n`) to
an uncommitted copy; `mutants/results.tsv` records each test under each (mutant 0 is the control run):

| Mutant | Fails |
| --- | --- |
| 1 no spacer calibration | gap |
| 2 brush kept across a device change | device loss |
| 3 brush color never set per row | colors |
| 4 no tab stop override | tab stops |
| 5 no color-font option | colors |
| 6 description drawn in the label's color | colors, device loss |
| 7 host's small format spaces lines differently | gap, format policy |
| 9 drawing effect never bound | colors, device loss |
| 11 layout counter counts two per row | layout count |
| 12 row height from the unrounded label height | gap |

The control run passes all six. Against main's implementation (its library with these tests, less the two that use the
new diagnostics) the layout-count test fails (ten layouts for five described rows) and the colors, tab stops and
format policy tests pass, as contract tests should.

## Validation

On the branch, x64 Debug, Release and ASan Debug each ran `./test.ps1 -SkipBuild -Suites Menu,MenuExitLifetime,
NewControls,NativeTextInput,Accessibility,MenuResourceScaling,MenuTextLayoutResources` directly (no wrapper) on the
developer's desktop: all seven suites pass in all three. Menu records one capability skip (another window covers the
menu-closing window) and NewControls nine (the nonactivating lane's interactive-desktop tests), as on main. The
six tests also ran in both lanes of each. Debug, Release and ASan Debug for ARM64 build (nothing ran). `format.ps1 -Check`
and `validate.ps1` pass. The six native CI profiles, the activating Menu lane on ARM64 and the consumer handoff are not
part of this evidence.

## Limits

- One machine and one toolchain. Live heap is what the heaps hold, not private memory, and allocator capacity is not
  reported here; the plan's waived flag is about the latter and is untouched.
- The family, weight, style, stretch and locale overrides are applied only where the small format differs from the
  body format, and on this machine only the size (and the tab stop) differs, so the test suite does not execute the
  others. The probe's seven font pairs show the technique is exact for them.
- Not measured: a paired benchmark of the library, a hardware-GPU or mixed-DPI run, ARM64 execution, and real
  assistive technology. The UI Automation, focus and scroll behaviors are the existing tests' (unchanged and passing).

## Files

| Path | Content |
| --- | --- |
| `Get-MenuLiveBytes.ps1` | Summarizes `MenuResourceScaling` (v4, v5) and `MenuRowLiveBytes` logs as above |
| `captures-manifest.tsv` | Pixel hash, size and state of each of the 420 captures (identical for both builds) |
| `probe/` | `probe.cpp` and its results: pixels, font pairs, heights, tabs, brush, edges, reflow, memory, steps, characters, timing |
| `harness/` | `DxUiTests.RowProbe.h` (suites `MenuRowProbe` and `MenuRowLiveBytes`) and `Set-Harness.ps1`, which switches it on in a checkout (`-Mode On`; it edits `DxUiTests.cpp` and `DxUiTests.Menu.cpp`) or off (it restores `DxUiTests.cpp`) |
| `mutants/` | `Apply-Sabotage.ps1` and the test-by-mutant results (`results.tsv`) |

Reproduce: build Release x64 at main and at the branch with the same test sources; run
`DxUi.ControlTests.exe --suite=MenuResourceScaling` on each (foreground) and
`./gallery.ps1 -Configuration Release -Platform x64 -SkipBuild -OutputDirectory <dir>`; with the harness on, run
`--suite=MenuRowProbe --no-activate` with `DXUI_ROW_PROBE_DIR=<dir>` and compare the two directories. Raw suite logs
are not retained (2.6 MB each): the suites are deterministic here and the tables reproduce them.
