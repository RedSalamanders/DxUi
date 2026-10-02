# Per-entry UI Automation elements in menus without descriptions, 2026-10-02

Library scope; no consumer pin changes. Main gave UI Automation one element per row only in a menu with at least one
described entry, so a screen reader found no row in an ordinary menu: RedSalamander's destination-menu test reported it
on 27 September, and the `codex/menu-description-layout` branch fixed it (`15be545`, `d45c361`). This change ports that
fix, so every row of every menu popup is an element, and the branch's snapshot pre-sizing (`65b0257`). Popups publish
their elements whether or not a client listens (a window host registers its accessibility target when it attaches), so
the question is what the elements cost a menu that no screen reader reads, and a long one.

## Answer

The final code against main, six interleaved runs per side, on one machine where UI Automation clients listen (see
Limits):

- **Memory.** An open menu holds about 1.6 KB more live heap per command row and 1.9 to 2.0 KB per radio row, linear from
  12 to 4,096 rows: +19,967 bytes for twelve commands (+18.7%), +76,287 for 48 (+57.5%), +6.39 MB for 4,096 (+208%).
  Closing returns all of it.
- **Keyboard and scrolling.** A key that moves the selection, or a wheel notch that moves the rows, rebuilds the popup's
  snapshot and raises a focus change: about 0.3 ms plus 1.4 to 2.6 µs per row. Down takes 340 µs instead of 38 for
  twelve rows, 635 instead of 38 for 128, and 6.1 ms instead of 38 µs for 4,096. A described menu of twelve rows takes
  375 µs, as it did before.
- **Opening, closing and pointer.** Opening and closing are within noise up to 128 rows; at 4,096 rows opening takes
  77 ms instead of 60 and closing 5.9 ms instead of 3.4. A pointer move costs 4 µs more at twelve rows and 236 µs more
  at 4,096, because the rows' bounds are compared on every invalidation.
- **Paint.** No paint costs more CPU cycles. The rows are not drawn.
- **Described menus.** The described control menu is within noise everywhere but its wheel notch, 17% faster with the
  pre-sizing, and holds 748 bytes less.

These are confirmed degradations of menus without descriptions, which the
[performance contract](../../../Specs/Core/Core_PerformanceAndResources.md) leaves to the developer: see Options.

## Sources and environment

| Item | Value |
| --- | --- |
| Baseline (A) | main `265c280` |
| Port (B) | `15be545` and `d45c361` on main: `DxUi.Menu.cpp` (no description condition in `PopulateMenuAccessibility`, `SynchronizeMenuAccessibility` and the `WM_SETFOCUS` handling) and `DxUi.Accessibility.cpp` (`IsOffscreen` of a menu row its viewport scrolled away) |
| Final (C) | the port and `65b0257` rebuilt on main: `PublishWindowHostAccessibilitySnapshot` counts the records (`CountAccessibilitySnapshotNavigation`) and reserves its three vectors once, and each record is made in place |
| Machine | AMD Ryzen AI 7 PRO 350 (8C/16T), 55.6 GB, Radeon 860M driver 32.0.22024.19001, balanced power plan |
| OS | Windows 11 Enterprise 10.0.26200 |
| Toolchain | Visual Studio 18 Insiders, MSVC 14.51.36231, x64 Release |
| UI Automation | `UiaClientsAreListening()` was true at the start and the end of every run: another application on this desktop listens, so the focus changes reach real clients |

All three executables are built from the same test sources with the harness switched on (`harness/Set-Harness.ps1`),
so every comparison runs identical fixtures. `DxUi.ControlTests.exe` SHA-256: A `04923D538BF85B54...EB218D1415546E`,
B `37B9845616F6D620...8F8DD07430D3B3F5`, C `A61686AC2D8CADC2...F0E81EF0C022D05073D645F`; each `runs.tsv` has them in
full.

## The fixture

`harness/DxUiTests.MenuUiaCost.h`, suite `MenuUiaCost`, fixture `dxui-menu-uia-cost-v1`, run with `--no-activate`:
it opens menus from an owned, nonactivating window on the primary monitor (96 DPI here) and drives their popups with
sent messages, so nothing takes the foreground, the keyboard focus or the pointer. Menus are constrained to 300 by
456 DIP, as `MenuResourceScaling` constrains them. Rows read "Archives photographiques de la réunion familiale" (the
radio rows, as in `MenuResourceScaling`) or "Archives photographiques *n* de la réunion familiale" (the command rows);
the described control menu adds a path below each of twelve radio rows.

- **Memory**: 24 cycles over nine menus (radio 12, 24, 48; command 12, 48, 128, 512, 4,096; described 12), each cycle
  in another order. Before an opening, once the open menu has painted and after it closed: bytes busy in all process
  heaps, private bytes, and `DebugGetContextMenuResources()`.
- **Timing**: eight openings of four menus (command 12, 128, 4,096; described 12). Each opening times `ShowAsync`
  (which shows and paints the popup), then 32 Down keys, 16 End and Home keys in turn, 16 wheel notches down and 16 up,
  32 pointer moves between two rows, and Escape until the popup is destroyed. Each input's handler is timed alone, then
  the paint it requested (`RedrawWindow` with `RDW_UPDATENOW`): wall time and the thread's own CPU cycles
  (`QueryThreadCycleTime`), because a paint's `Present` waits for a vertical blank, which the cycles exclude.

`harness/Invoke-MenuUiaPairs.ps1` ran each comparison as three interleaved A, B, B, A passes, six runs per side, one
process each, with nothing else measured or built meanwhile. `harness/Measure-MenuUiaPairs.ps1` reduces each run to one
value per metric (the memory medians of cycles 8 to 23, the timing medians of every sample after the first opening)
and compares the sides as the paired benchmark does (`Tools/PerformanceComparison.psm1`, `Get-MetricVerdict`: an exact
Mann-Whitney test and bands of 5% for timings and 2% for memory). Six runs per side make p = 0.0022 the strongest
result possible; every flag below reaches it unless stated. Three sets ran, in this order:

| Set | Sides | Fixture | Result |
| --- | --- | --- | --- |
| `port/` | A against B | all | the port's cost |
| `presize/` | B against C | timing only (one memory cycle, so no memory metric) | what `65b0257` saves |
| `final/` | A against C | all | the final cost, reported here |

## Live heap of an open menu (final)

Rendered minus before, per cycle after the eighth. In the port set every run of a side gave one value per menu, the
same to the byte. In the final set, some cycles of nearly every run, on both sides alike, gave 7,786 bytes less (a few
other amounts): a block that is not the menu's was freed between the two samples, and closed minus before shows it
too. A run's median then depends on how many of its cycles it reached, which is why the comparison's medians differ a
little from these values. The table gives the value of the other cycles, identical in every run of a side; the lower
cycles show the same change to the byte (98,855 and 118,822 for twelve commands, for example).

| Menu | Main | Final | Change | Per row |
| --- | ---: | ---: | ---: | ---: |
| 12 radio rows | 106,641 | 130,544 | +23,903 (+22.4%) | 1,992 |
| 24 radio rows | 115,407 | 162,006 | +46,599 (+40.4%) | 1,942 |
| 48 radio rows | 132,783 | 224,814 | +92,031 (+69.3%) | 1,917 |
| 12 command rows | 106,641 | 126,608 | +19,967 (+18.7%) | 1,664 |
| 48 command rows | 132,783 | 209,070 | +76,287 (+57.5%) | 1,589 |
| 128 command rows | 190,703 | 391,237 | +200,534 (+105.2%) | 1,567 |
| 512 command rows | 468,719 | 1,268,362 | +799,643 (+170.6%) | 1,562 |
| 4,096 command rows | 3,063,574 | 9,448,721 | +6,385,147 (+208.4%) | 1,559 |
| 12 described radio rows (control) | 390,208 | 389,460 | -748 | |

A row costs its accessibility proxy (a `Label`, 440 bytes, or for a radio or toggle row a `Toggle`, 768 bytes, with
its name, automation id and invoke action) and its record in the published snapshot. `accessibilityRecords` rises by
exactly the row count while a menu is open, and closing returns everything on both sides. Private bytes move in whole
pages for small menus and are not compared there; at 4,096 rows the median rises from 3.30 MB to about 11 MB.

## Handler time per input (final)

Wall microseconds, the median of each run, then the median of the six runs; the thread's CPU cycles agree, and
`final/final.comparison.json` has both.

| Menu | Input | Main | Final | Change |
| --- | --- | ---: | ---: | ---: |
| 12 commands | Down | 37.8 | 339.7 | +301.9 |
| | End, Home | 37.7 | 394.7 | +357.0 |
| | Wheel notch | 41.9 | 48.0 | +6.1 |
| | Pointer move | 46.9 | 51.3 | +4.4 (p = 0.017) |
| 128 commands | Down | 38.1 | 635.1 | +597.0 |
| | End, Home | 39.2 | 667.0 | +627.8 |
| | Wheel notch | 42.0 | 446.3 | +404.3 |
| | Pointer move | 48.4 | 59.4 | +11.0 |
| | Open (`ShowAsync`) | 13,921.2 | 12,132.7 | -1,788.5 (p = 0.041) |
| 4,096 commands | Down | 37.8 | 6,107.9 | +6,070.1 |
| | End, Home | 39.5 | 6,337.4 | +6,297.9 |
| | Wheel notch | 43.1 | 6,105.5 | +6,062.4 |
| | Pointer move | 49.0 | 284.9 | +235.9 |
| | Open (`ShowAsync`) | 59,956.3 | 76,817.2 | +16,860.9 (p = 0.041) |
| | Close (Escape) | 3,391.4 | 5,897.4 | +2,506.0 |
| 12 described rows (control) | Down | 381.3 | 375.3 | within noise |
| | Wheel notch | 181.7 | 150.6 | -31.1 |

Opening and closing twelve rows, and closing 128, are within noise. The twelve-row wheel notch costs little because the
rows stop moving after three notches; a notch that moves them costs what a key does. The fixed part of a key's cost is
the focus change raised for listening clients and the snapshot's own fixed work; the per-row part is the snapshot's
rebuild, whose records check each control's type and copy its strings. No paint costs more CPU cycles; after a key or
notch at 4,096 rows the paint takes 12% to 13% fewer (p ≤ 0.0087), presumably because its `Present` waits less of the
frame once the slower handler has spent part of it, which is also why a paint's wall time there is shorter.

## The port and the pre-sizing

The port alone (`port/`, A against B) costs the same per row in time and slightly more in memory: 1.73 to 1.85 KB per
command row and 2.05 to 2.15 KB per radio row, with Down at 343 µs for twelve rows, 650 for 128 and 8.4 ms for 4,096.
Its described control menu is within noise everywhere, its live heap identical to the byte.

The pre-sizing (`presize/`, B against C) then saves, at 4,096 rows, 24% of a Down (7,880 to 5,954 µs), 20% of End and
Home and 28% of a wheel notch (all p = 0.0022) and 10% of the opening (86 to 77 ms, p = 0.026); at 128 rows 10% to 13% of
a key or notch (p ≤ 0.041); at twelve rows 6% of a Down (p = 0.041); and 11% of the described menu's wheel notch
(p = 0.041). Everything else is within noise. Reserving exactly also holds less: the same open menus hold 971,652 bytes
less at 4,096 rows, 148,852 at 512, 9,724 at 128 and 748 for the described menu (from the port and final sets, whose
values are exact). It is in the final code.

## Options

The cost is linear in the row count and paid by every menu without descriptions, screen reader or not:

1. **Accept it** (what this branch does). At ordinary sizes, 12 to 48 rows, an open menu holds 20 to 92 KB more and a
   key takes about 0.3 ms more, what a described menu of that size already costs. A long menu pays more: 6.1 ms a key at
   4,096 rows, about a third of a 60 Hz frame.
2. **Optimize further first** (not built). The per-row time is the snapshot builder's: about 20 type checks and three
   string copies per record. Keeping each control's unchanging semantics between publishes would cut it for every host,
   Tree and Grid included; that is a change to the accessibility snapshot, not to menus. Comparing a menu's row bounds
   only when its geometry changed would remove the pointer move's per-row cost (236 µs at 4,096 rows).
3. **Reduce the scope.** Give rows elements only up to a row count, which leaves long menus unreadable, or only when
   `UiaClientsAreListening()` at opening, which saves nothing here (a client always listened) and leaves a client that
   starts during a menu without rows.
4. **Defer.** Keep main's contract; ordinary menus stay unreadable to screen readers, RedSalamander's destination menu
   included.

## Tests

Three tests join the described-menu group (`RunMenuDescriptionTests`), so they run in the Menu lane and in the
nonactivating NewControls lane; `Specs/Testing/Testing_Validation.md` describes them.
`TestPlainMenuAccessibilityInvokesAndDisconnects` reads a plain popup's rows through its provider and invokes one,
`TestPlainMenuAccessibilityScrollsFocusedRow` checks the offscreen rows of a long plain menu, and
`TestMenuNativeFocusSelectsNoRowAndRestoresTheChosenOne` gives a plain and a described popup native focus.

With main's `DxUi.Menu.cpp` and `DxUi.Accessibility.cpp` under them (x64 Debug, NewControls lane), each fails: "every row
of the open plain menu is published", "the long menu exposes its first and last rows" and "the chosen row is UIA's focus
again". Two single-point reversions each fail their test: `IsOffscreen` without the scrolled-away rule fails "a row the
viewport cuts off is offscreen, without a rectangle", and the `WM_SETFOCUS` handling kept to described popups fails
"native focus selects no row of a plain or described menu". The pre-sizing changes no behavior, and the snapshot and
accessibility tests of every suite run on it.

## Limits

- One machine and one toolchain, at 96 DPI. A desktop with no listening UI Automation client raises no focus events
  and pays less per key; this one always had a listener.
- Live heap is what the heaps hold. Private bytes and working set include allocator and renderer reserves that move in
  whole pages and are not attributed here.
- Not measured: a real screen reader, ARM64 execution, Debug and ASan timings (the Menu suite's 4,096-row bounds,
  `TestLargeMenuPaintsOnlyVisibleRowsWithCachedOffsets`, run in all six CI profiles), and the hosted paired benchmark,
  whose scene opens no menu.

## Files

| Path | Content |
| --- | --- |
| `harness/` | The fixture header, `Set-Harness.ps1` (switches it on or off in a checkout), `Invoke-MenuUiaPairs.ps1` (interleaved runs) and `Measure-MenuUiaPairs.ps1` (the comparison) |
| `port/`, `presize/`, `final/` | Each set's twelve runs (`runs.zip`: `A1.jsonl` to `B6.jsonl`, zipped from 3.5 MB to 0.7 MB), their standard error, `runs.tsv` (order, start, duration, exit code, SHA-256 and build directory of each run; the directory replaces the executable's local path) and the comparison of every per-run value (`*.comparison.json`) |
| `SHA256SUMS` | Hashes of the files in the three set directories |

Reproduce: build x64 Release at main and at the branch with the harness on (`harness/Set-Harness.ps1 -Mode On` in each
checkout), copy each `DxUi.ControlTests.exe` aside, run `harness/Invoke-MenuUiaPairs.ps1 -Baseline <main> -Candidate
<branch> -OutputDirectory <dir>`, then `harness/Measure-MenuUiaPairs.ps1 -Directory <dir>`. The pre-sizing set ran
with `DXUI_MENU_UIA_CYCLES=1`. To recompute a retained comparison, expand that set's `runs.zip` into a directory and
measure it.
