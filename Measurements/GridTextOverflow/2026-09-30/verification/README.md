# Grid bounded multiline cells: verification suites in x64 Debug, Release and ASan Debug

This packet archives the logs and receipts of the Grid, Rendering, Accessibility, Embedded and MultilineText suites in x64
Debug, Release and ASan Debug, run after 16 new tests of the Grid's bounded multiline cells were added, and the table of
temporary mutations each new test failed under. It closes the verification item of
[the grid overflow plan](../../../../Specs/Plans/WIP/GridTextOverflow_2026-09-21.md) for x64; it does not claim native ARM64
execution, resource acceptance, assistive-technology acceptance or consumer adoption (see [Not claimed](#not-claimed)).

## Source and machine

- Tested source: `7fe47c0f16287f073a3fb9b3185df6608001c658` on branch `improve/grid-multiline-verification`; no source path differed from that commit while
  the suites ran (`raw/head.txt`), and the commit after it changes documentation and this packet only.
- Machine: Lenovo 21QMS0QJ0J, AMD Ryzen AI 7 PRO 350 (8 cores, 16 threads), 55.6 GB, AMD Radeon 860M, Windows 11 Enterprise
  10.0.26200 (x64), Visual Studio 2026 Insiders with MSVC 14.51.36231. Other agents built and ran tests in other worktrees
  of the same machine throughout, so the timing and memory lines of the benchmark below are noisy.
- Commands, from the repository root, in this order for each configuration: `build.ps1 -Configuration <configuration>
  -Platform x64`, then `test.ps1 -Configuration <configuration> -Platform x64 -SkipBuild -Suites
  Grid,Rendering,Accessibility,Embedded,MultilineText`. `test.ps1` also runs the tooling tests, the runner's filter and
  watchdog checks, the AddressSanitizer detection probe (ASan Debug) and the complex-UI benchmark; its benchmark comparison is
  `unpaired` in every run, so it establishes no performance result and is not an acceptance of any resource figure. The Menu
  and NativeTextInput suites were not run: they need the foreground, which another agent was using.

## Results

| Configuration | Build | `test.ps1` | Grid | Rendering | Accessibility | Embedded | MultilineText | Skips | Benchmark |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| x64 Debug | exit 0 | exit 0 | exit 0, 62 tests, 1.3 s | exit 0, 51 tests, 35.6 s | exit 0, 66 tests, 8.4 s | exit 0, 2838 checks | exit 0, 106 tests, 4.1 s | 0 | unpaired |
| x64 Release | exit 0 | exit 0 | exit 0, 62 tests, 0.7 s | exit 0, 51 tests, 32.5 s | exit 0, 66 tests, 8.9 s | exit 0, 2838 checks | exit 0, 106 tests, 4 s | 0 | unpaired |
| x64 ASan Debug | exit 0 | exit 0 | exit 0, 62 tests, 4.5 s | exit 0, 51 tests, 51.2 s | exit 0, 66 tests, 12.1 s | exit 0, 2838 checks | exit 0, 106 tests, 6.2 s | 0 | unpaired |

"exit 0" is a process exit code. A suite cell gives the suite's exit code, then the number of tests its log records and its
own duration (Embedded: the checks it passed). No suite recorded a capability skip. The raw log of each suite, its receipt
(`<Suite>.receipt.txt`: exit code, skips, executable SHA-256), the `test.ps1` transcript, the build log and the benchmark
receipt are under `raw/<configuration>/`; `receipt-filenames.md` maps the renamed receipts to their original names.

The same source builds for ARM64 (cross-builds on this x64 host: nothing ran). The package restore used a scratch vcpkg overlay triplet that pins toolset 14.51 (`VCPKG_OVERLAY_TRIPLETS`), because the newest MSVC installed here (14.52) has no x64-hosted ARM64 compiler; the triplet is not part of the repository:

- ARM64 Debug: `build.ps1` exit 0 (`raw/ARM64-Debug/build.log`)
- ARM64 Release: `build.ps1` exit 0 (`raw/ARM64-Release/build.log`)
- ARM64 ASan Debug: `build.ps1` exit 0 (`raw/ARM64-ASan-Debug/build.log`)


## What covers each aspect

Every comparison is between a scenario and a fresh twin painted in the same window or view, so no test depends on stored
pixels. The last column lists the temporary mutations that fail the test (table below).

| # | Aspect | Test | Fails under |
| --- | --- | --- | --- |
| 1 | Ctrl+C and `OnCopy` of trimmed multiline cells, exact across rows and columns | Grid `TestGridCopyOfTrimmedMultilineCellsIsExact` | `copy4096`, `copyNfc`, `noTooltip` |
| 2 | Decomposed accents match precomposed ones in trimming and pixels; UI Automation and copy exact | Rendering `TestGridMultilineDecomposedAccentsPaintLikePrecomposed`; the same decomposed values in the copy and UI Automation tests | `dropLastUnit`; `copyNfc`; `uiaNfc` |
| 3 | A 100,000-unit value whose shaped prefix ends inside a surrogate pair, a zero-width-joiner sequence or a letter and its marks paints like a short twin; the surrogate guard steps back | Rendering `TestGridMultilineShapedPrefixCutInsideAClusterPaintsLikeItsShortTwin` | `shortPrefix` (pixels), `noSurrogateGuard` (shaped units) |
| 4 | A grid whose flow is right to left, Arabic and Latin cells: marker side, clipping, UI Automation values | Rendering `TestGridMultilineRightToLeftFlowKeepsMarkerSideAndClipping`; Accessibility `TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues` (left-to-right and right-to-left grids) | `noMarkDirection`; `uiaTruncate` |
| 5 | A cell narrower than one word at clamps 1, 2 and 3: ink inside the cell, the ellipsis, the tooltip | Rendering `TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue` | `noTrimSign`, `noTooltip` |
| 6 | Fractional vertical and horizontal scroll: a clipped cell's surviving pixels are a shifted crop of the unscrolled capture | Rendering `TestGridMultilineCellCutByTheViewportPaintsAShiftedCropOfItsWholeSelf` | `reflowClipped` |
| 7 | A dpi change (144, 192) in a window and in an embedded host matches a fresh grid | Rendering `TestGridMultilineRepaintsAtANewDpiLikeAFreshGrid`; Embedded `TestEmbeddedMultilineGridFrenchCells` | `dpiStale` (both) |
| 8 | Theme (light, dark, high contrast), font family and scale, and density changes with retained layouts match a fresh attach | Rendering `TestGridMultilineRepaintsAfterThemeFontAndDensityChangesLikeAFreshAttach` | `themeColorCache`, `noFormatKey` |
| 9 | Prefix sharing never crosses values that lay out differently | Rendering `TestGridMultilinePrefixSharingNeverCrossesTextThatLaysOutDifferently` | `noBreakShape`, `hashOnlyPrefix` |
| 10 | The 16,384-entry ceiling: capping, correct pixels after eviction, halving when use drops | Rendering `TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction` (through the diagnostics hook `Grid::DebugSetTextLayoutEntryLimit`); Grid `TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` (the real tables at 16,384) | `noCeiling`, `noHalving`; `evictNewest` (Grid test only) |
| 11 | Embedded WARP view, long French cells at clamp 2 in 64-DIP rows: ellipsis read back, hide and show, device replacement | Embedded `TestEmbeddedMultilineGridFrenchCells` | `noMarker`, `hideCorrupts`, `replaceAliased`, `replaceHiddenAliased`, `dpiStale` |
| 12 | Device loss with trimmed cells: the capture after equals the capture before | Rendering `TestGridMultilineTrimmedCellsPaintTheSameAfterDeviceLoss` | `lossAliased`, `lossClearsFormats` |
| 13 | A multiline grid moved to another host (device, dpi, theme, density) matches a fresh grid in pixels and cell rectangles | Rendering `TestGridMultilineMovedBetweenHostsMatchesAFreshOne` | `noMoveAnnouncement` |
| 14 | UI Automation of a clipped cell and of a 100,000-unit cell: complete Name and Value, bounding rectangle clipped to the viewport | Accessibility `TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues`, `TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport` | `uiaTruncate`, `uiaUnclipped` |

Also checked and recorded in the logs (not asserted): at 144 and 192 dpi a repaint creates no layout (`Grid at <dpi> dpi:
layouts created by the repaint 0`; the same for the embedded view), because the layouts are in device-independent units.

## Falsification

A test that cannot fail proves nothing, so each new test was run against a library built with temporary mutations. The
mutations are switches (`Mutant("<name>")`, selected by the environment variable `DXUI_MUTANT`) compiled into one x64 Debug
scratch build, applied by `raw/falsification/apply-mutants.ps1.txt` and recorded as `raw/falsification/mutations.patch`;
none is committed. `run-mutants.ps1.txt` ran every pair (`raw/falsification/results.txt`). The Embedded rows ran only
`TestEmbeddedMultilineGridFrenchCells`, through a switch the scratch build adds to the embedded test's entry point
(`DXUI_ONLY_FRENCH`), so a failure there is that test's and no earlier embedded test's. 27 mutations make 33
pairs with a test, and 32 of the pairs fail at the assertion shown.

| Mutation | What it changes | Test | Result |
| --- | --- | --- | --- |
| `copy4096` | the text Ctrl+C copies from a cell is cut at 4,096 units | Grid `TestGridCopyOfTrimmedMultilineCellsIsExact` | `Ctrl+C copies every trimmed cell exactly: every unit of every value, across rows and columns` |
| `copyNfc` | the text Ctrl+C copies loses U+0301, as if normalized | Grid `TestGridCopyOfTrimmedMultilineCellsIsExact` | `Ctrl+C copies every trimmed cell exactly: every unit of every value, across rows and columns` |
| `uiaNfc` | the text UI Automation exposes for a cell loses U+0301 | Accessibility `TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues` | `left-to-right grid, cell 1,1: the Name is the complete value` |
| `uiaTruncate` | the text UI Automation exposes for a cell is cut at 4,096 units | Accessibility `TestAccessibilityMultilineGridCellsExposeTheirExactUnicodeValues` | `left-to-right grid, cell 1,0: the Name is the complete value` |
| `uiaTruncate` | the text UI Automation exposes for a cell is cut at 4,096 units | Accessibility `TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport` | `scrolled (64, 40), cell 2,0: the Name, Value and ValuePattern are the complete value` |
| `uiaUnclipped` | a cell's UI Automation rectangle is inflated by 2 DIP, not the viewport-clipped cell | Accessibility `TestAccessibilityClippedMultilineGridCellBoundsFollowTheViewport` | `scrolled (0, 0), cell 0,0: the bounding rectangle is the viewport-clipped cell` |
| `dropLastUnit` | the visible text in front of the omission marker loses its last code unit | Rendering `TestGridMultilineDecomposedAccentsPaintLikePrecomposed` | `a 420-unit value at clamp 3: decomposed accents paint like precomposed ones` |
| `noSurrogateGuard` | shaping no longer steps back from half a surrogate pair | Rendering `TestGridMultilineShapedPrefixCutInsideAClusterPaintsLikeItsShortTwin` | `clamp 1, surrogate pairs: in some alignment the cut falls inside a surrogate pair, and shaping steps back from it` |
| `shortPrefix` | a wrapped value over 4,096 units shapes only its first 100 units | Rendering `TestGridMultilineShapedPrefixCutInsideAClusterPaintsLikeItsShortTwin` | `clamp 3, surrogate pairs, phase 0: a prefix cut in the tail paints like a short twin` |
| `noMarkDirection` | the marker ignores the direction of the last visible text | Rendering `TestGridMultilineRightToLeftFlowKeepsMarkerSideAndClipping` | `an Arabic omission marker sits at the left end, where that text reads to` |
| `noTrimSign` | the layout gets no DirectWrite trimming sign (nothing is trimmed) | Rendering `TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue` | `a 40-DIP cell at clamp 1: the ink stays inside the text rectangle` |
| `reflowClipped` | a cell the viewport cuts is laid out against its clipped rectangle | Rendering `TestGridMultilineCellCutByTheViewportPaintsAShiftedCropOfItsWholeSelf` | `a cut cell paints the shifted pixels of the unscrolled one: clipping does not reflow it` |
| `dpiStale` | a layout is scaled to the dpi it was made at and kept across a dpi change | Rendering `TestGridMultilineRepaintsAtANewDpiLikeAFreshGrid` | `at 144 dpi the grid that kept its layouts paints like a fresh one` |
| `dpiStale` | a layout is scaled to the dpi it was made at and kept across a dpi change | Embedded `TestEmbeddedMultilineGridFrenchCells` | `a view whose dpi changed paints what a fresh view paints at that dpi` |
| `themeColorCache` | a retained layout entry keeps the text color it first painted with | Rendering `TestGridMultilineRepaintsAfterThemeFontAndDensityChangesLikeAFreshAttach` | `dark: the grid that kept its layouts paints like a fresh attach` |
| `noFormatKey` | the layout cache key ignores the text format (font) | Rendering `TestGridMultilineRepaintsAfterThemeFontAndDensityChangesLikeAFreshAttach` | `a font or density change lays the cells out again` |
| `noBreakShape` | prefix sharing ignores how the value continues after the shaped prefix | Rendering `TestGridMultilinePrefixSharingNeverCrossesTextThatLaysOutDifferently` | `a paragraph that continues where the retained one ended lays out for itself` |
| `hashOnlyPrefix` | prefix sharing compares only the first 4,096 units | Rendering `TestGridMultilinePrefixSharingNeverCrossesTextThatLaysOutDifferently` | `a value that differs inside the shaped prefix, past the hashed units, lays out for itself` |
| `noCeiling` | the layout tables grow without a ceiling | Rendering `TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction` | `the tables stop growing at the ceiling` |
| `noCeiling` | the layout tables grow without a ceiling | Grid `TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` | `a table never grows past its ceiling` |
| `noHalving` | the layout tables never halve | Rendering `TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction` | `each paint of a few cells halves the tables, down to 32 entries` |
| `noHalving` | the layout tables never halve | Grid `TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` | `a paint that uses under an eighth of a table halves it, once` |
| `evictNewest` | a full set at the ceiling evicts its most recently used way | Rendering `TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction` | passes: the pixels are the same whichever way is evicted; the Grid test fails it |
| `evictNewest` | a full set at the ceiling evicts its most recently used way | Grid `TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` | `a full set at the ceiling evicts one least recently used way, one the previous paint used, for a new value` |
| `noTooltip` | a trimmed multiline cell offers no tooltip | Rendering `TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue` | `a 40-DIP cell at clamp 1: hovering offers the complete value` |
| `noTooltip` | a trimmed multiline cell offers no tooltip | Grid `TestGridCopyOfTrimmedMultilineCellsIsExact` | `the cell is trimmed and its tooltip is the complete value` |
| `noMarker` | no ellipsis is appended to the visible text | Embedded `TestEmbeddedMultilineGridFrenchCells` | `the cell differs from its twin by an omission marker after the second line, with no third line` |
| `hideCorrupts` | hiding a grid changes its line clamp to 1 | Embedded `TestEmbeddedMultilineGridFrenchCells` | `showing the view again paints the pixels it painted before` |
| `replaceAliased` | a true device replacement leaves the Direct2D context with aliased text | Embedded `TestEmbeddedMultilineGridFrenchCells` | `a replacement device paints the pixels of the first one` |
| `replaceHiddenAliased` | the same, only when the view is hidden | Embedded `TestEmbeddedMultilineGridFrenchCells` | `a device replaced while hidden paints the same pixels when shown` |
| `lossAliased` | after a simulated device loss the recreated context paints aliased text | Rendering `TestGridMultilineTrimmedCellsPaintTheSameAfterDeviceLoss` | `after device loss 1 the trimmed cells paint what they painted before` |
| `lossClearsFormats` | a simulated device loss discards the configured text formats | Rendering `TestGridMultilineTrimmedCellsPaintTheSameAfterDeviceLoss` | `the layouts survive the loss: nothing is laid out again` |
| `noMoveAnnouncement` | a control moved between hosts does not announce its density change | Rendering `TestGridMultilineMovedBetweenHostsMatchesAFreshOne` | `a moved control arranges itself like a fresh one` |

Two limits, both by design. A layout table is a cache and so is pixel-transparent: the Rendering test of the lowered ceiling
paints the right pixels under any eviction order, so `evictNewest` passes it, and the Grid test of the real tables (which
asserts which way a full set evicts) fails it. The surrogate guard changes no pixel, since the cut it guards is never
visible: its test asserts the shaped units, so `noSurrogateGuard` fails that assertion and not a pixel comparison. Three
earlier candidates changed nothing the tests can observe and were replaced by the mutations above: a brush cache kept across a
device loss and a Direct2D unit mode left unrestored on device replacement (the library rebuilds both at the next paint), and
a Grid that never recomputes its density metrics (it breaks a moved grid and its fresh twin alike, so the two still agree;
`noMoveAnnouncement` is the mutation that separates them).

## Findings

- The library needed no fix. The only library change is the diagnostics hook above, which lowers the ceiling a test wants to
  reach; the production ceiling stays 16,384. The API revision stays 2.
- The Grid does not read its `FlowDirection`: `DxUi.Grid.cpp` has no reference to it, so the columns and cells of a
  right-to-left grid are not mirrored and the omission marker follows the text's own direction. The right-to-left test logs
  `Grid in a right-to-left flow against the same grid in a left-to-right flow: identical` (an observation, not an assertion:
  a grid that mirrored would still pass) and asserts the marker side, clipping and UI Automation values in such a grid.
  Mirroring a Grid would be a new feature.
- The performance contract said an entry the current or previous paint used is never evicted. The Grid test shows that holds
  only while a table can grow: at the ceiling a full set gives up its least recently used way, one of the previous paint's
  (the test runs the production code on a 64-entry table; a grid's ceiling is 16,384). The sentence in
  `Specs/Core/Core_PerformanceAndResources.md` now says so; no code changed.
- Layouts are in device-independent units. A dpi change keeps them (no layout is created) and the repaint equals a fresh
  grid's; a theme change also creates none; a font or density change creates them again.
- The vertical scroll offset rests on whole rows except while the scrollbar thumb is dragged, so the fractional-offset
  checks drag the thumb; horizontal offsets are continuous.
- At a clamp of one the omitted line of an unwrapped value is shaped a second time, so the surrogate guard's step back
  counts twice there; the tests allow that (a difference of one unit at clamps 2 and 3, two at clamp 1).

## Not claimed

- Native ARM64 execution. The three ARM64 configurations build, as listed above, but this host is x64, so the new tests
  have not run on ARM64; the next native CI run executes them.
- The Menu and NativeTextInput suites, the six-profile CI matrix, consumer adoption (the plan's last two items) and any
  resource acceptance: the benchmark lines are unpaired and were measured while the machine was busy.
- Screen-reader, IME and touch acceptance: the UI Automation tests read the provider in process.

## Reproduce

From the tested commit: build and run as above, or run one test, for example
`.build\x64\Debug\DxUi.ControlTests.exe --suite=Rendering --test=TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue --no-activate`
(`DxUi.EmbeddedTests.exe` runs the Embedded suite whole). To repeat a mutation: apply `raw/falsification/mutations.patch`
to the tested commit, build x64 Debug, set `DXUI_MUTANT=<name>` (and `DXUI_ONLY_FRENCH=1` for `DxUi.EmbeddedTests.exe`)
and run the test in the table.

## Files

- `raw/head.txt`: the tested commit.
- `raw/<configuration>/`: `build.log`, `build-exit.txt`, `test.log` (the `test.ps1` transcript), `test-exit.txt`,
  `timing.txt`, `test-<Suite>.log`, `<Suite>.receipt.txt`, `performance.receipt.txt` and `performance.comparison.txt`, and for
  ASan Debug `asan-probe.log` and `asan-probe.receipt.txt`.
- `raw/ARM64-<configuration>/`: `build.log` and `build-exit.txt` of the ARM64 cross-builds.
- `raw/falsification/`: `apply-mutants.ps1.txt`, `run-mutants.ps1.txt`, `mutations.patch`, `results.txt`.
- `receipt-filenames.md`: original names of the renamed receipts. `SHA256SUMS.txt`: every archived byte.
