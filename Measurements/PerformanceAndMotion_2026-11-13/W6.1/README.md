# W6.1 width-change reuse: closed unbuilt

Plan item `W6.1` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md) assumed
that keeping an `IDWriteTextLayout` and calling `SetMaxWidth` on a width change is cheaper than creating the layout again,
and said the item closes with its receipt if it is not. **It is not:** on this Windows build DirectWrite redoes a layout's
whole analysis after any change to its box, so the in-place path costs 0.98 to 1.03 of the re-create path. Separately,
the two large families the item names cannot change a layout in place without corrupting a graph that is still
observable. Decided by the conductor on 25 November 2026: close unbuilt; no code change.

- Source commit: `68a66f3bb` (integration tip, with `W7.3`). The program does not use DxUi code.
- Machine: AMD Ryzen 9 9950X3D (16 cores, 32 logical processors); Windows 11 Pro 10.0.26300; `DWrite.dll`
  10.0.26100.8875; MSVC 19.52.37032, `/O2`.
- Load during the runs (25 November 2026): other workers were building; whole-machine load sampled at 21% before and
  57% after run 1, 44% after run 2. The ratios do not depend on this: both paths run alternately on the same thread at
  every step and their CPU is read per thread. A quiet-machine run is still to be added (below).
- Files: `SetMaxWidthBench.cpp` (the program), `run1.csv` (run 1, copied from its console output), `run2.txt` (run 2:
  the same table with a breakdown line before each row).

## Method

A shared DirectWrite factory, a Segoe UI 14 DIP format (`en-us`), maximum height 100,000 DIP. Texts of 200, 4,096 and
65,536 UTF-16 units made by repeating a Latin sentence, or a mixed-script one (Latin, Arabic, Japanese, Devanagari,
Korean, Hebrew, Thai and an emoji, so font fallback runs). Per repetition, the in-place layout is created at 300 DIP and
measured once, as a retained layout would be; then 30 steps grow the width by 11 DIP each (wrapping changes on most
steps). At each step the program times, on the same thread with `QueryThreadCycleTime`:

- re-create: `CreateTextLayout` at the new width, then `GetMetrics`;
- in place: `SetMaxWidth` on the kept layout, then `GetMetrics`.

The value is the median per step over all steps of all repetitions (400 repetitions at 200 units, 60 at 4,096, 9 at
65,536; the argument multiplies them). Line count, width and height of the two paths were compared at every step: equal
everywhere. A warm-up layout is created once per text, so font and fallback caches are warm for both paths.

## Results (median million cycles per step)

| Text | Units | Run 1 create / in place (ratio) | Run 2 create / in place (ratio) |
| --- | ---: | --- | --- |
| Latin | 200 | 0.0858 / 0.0858 (1.00) | 0.0867 / 0.0867 (1.00) |
| Latin | 4,096 | 1.453 / 1.456 (1.00) | 1.413 / 1.433 (0.99) |
| Latin | 65,536 | 29.03 / 29.48 (0.98) | 30.18 / 29.28 (1.03) |
| Mixed | 200 | 1.546 / 1.544 (1.00) | 1.013 / 1.012 (1.00) |
| Mixed | 4,096 | 31.86 / 32.00 (1.00) | 22.41 / 22.54 (0.99) |
| Mixed | 65,536 | 667.6 / 674.5 (0.99) | 695.2 / 699.6 (0.99) |

No size or script reaches the 1.5 that would count as clearly faster; the difference is within noise everywhere.

## Where the analysis runs, and what discards it (run 2, million cycles, medians)

| Step | Latin 65,536 | Mixed 65,536 | Latin 200 |
| --- | ---: | ---: | ---: |
| `CreateTextLayout` alone | 0.198 | 0.085 | 0.002 |
| first `GetMetrics` | 25.37 | 705.4 | 0.086 |
| `GetMetrics` again, nothing changed | 0.003 | 0.004 | 0.001 |
| `SetMaxWidth` to the same width, then `GetMetrics` | 0.002 | 0.002 | 0.001 |
| `SetMaxHeight` only, then `GetMetrics` | 25.38 | 703.9 | 0.087 |
| `SetMaxWidth` +11 DIP, then `GetMetrics` | 24.60 | 732.6 | 0.087 |

Creating a layout is nearly free: itemization, bidi, shaping and line breaking all run in the first metrics query (or
the first draw). After any change to the layout's box, even its height alone, the next query runs all of it again. So a
kept layout saves only the creation call, which is under 1% of the work.

## What the program does not measure

DxUi's own re-create path does more than a bare `CreateTextLayout`: the rich-text paths apply style runs and build
their metrics tables, and `Label` applies character spacing and the mnemonic underline. An in-place path would skip
those calls, but not the analysis. For `Label` this does not change the conclusion: `Label::PrepareVisualContent` only
creates the layout and sets two range properties, which are lazy setters like the creation itself, and the analysis
they precede runs at the first draw whichever path made the layout.

## The three families (from the code at `68a66f3bb`)

- **`Label`'s retained layout** (`_typographyLayout`, only labels with a typography override or a mnemonic;
  `Label::PrepareVisualContent` and `Label::Paint` in `src/Controls/DxUi.Controls.cpp`): eligible as is. Only the label
  holds it, and `Paint` uses it only when its key matches the current size. It gains nothing (above). Plain labels paint
  through `W2.1`'s host table (`TextLayoutCacheAccess::Draw`, `src/Controls/DxUi.WindowHost.cpp`), whose entries are
  shared by text and keyed by exact width; an entry the current paint has not drawn could be taken, with a lookup by text
  without width, and would gain nothing either.
- **Semantic paragraph boxes:** not eligible. A preparation's `previousState` is the published `state_`
  (`src/Controls/DxUi.RichSemanticGeometry.cpp`, where the `Preparation` is created), and `FindReusableBox` takes
  `old.layout` from `previousState->boxes`, so a reused layout is shared with the publication that keeps answering hit
  tests, caret and accessibility queries while the next one is prepared over several turns. At a width change every
  layout is held by that publication; a copy would be a re-create, since a DirectWrite layout cannot be cloned.
- **v0 chunks:** not eligible. Each chunk is a `std::shared_ptr<const GeometryChunk>`
  (`src/Controls/DxUi.RichDocumentLayout.cpp`), held by up to four graphs: the publication on screen, the last complete
  publication a partial one keeps (restored when a live resize steps back), the publication a preparation began from,
  and the staged graph. At a width step the shown publication holds every chunk.

## Repeat the run

From a Developer PowerShell (or after `vcvars64.bat`), in a scratch directory outside the repository:

```powershell
cl /nologo /O2 /EHsc /std:c++20 /W4 <repository>\Measurements\PerformanceAndMotion_2026-11-13\W6.1\SetMaxWidthBench.cpp /Fe:SetMaxWidthBench.exe
.\SetMaxWidthBench.exe 1   # the runs above
.\SetMaxWidthBench.exe 3   # the quiet-machine run: three times the repetitions
```

It writes the table to standard output (CSV rows, each preceded by its breakdown line), creates no window and takes no
focus. The quiet-machine run is to be added here as `run-quiet.txt` with the machine state.
