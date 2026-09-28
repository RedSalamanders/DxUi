# Gallery follow-ups: static progress and high-contrast status

Status: COMPLETE (2026-09-28). Library scope; consumer adoption stays separate.
Base: `3c882b2` on `ej/elegant-einstein-lmooqh`, after main `6b34456`.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [theme and motion](../../UI/UI_ThemeAndTypography.md),
the design-system [ProgressBar](../../DesignSystem/components/ProgressBar/README.md) and
[Grid](../../DesignSystem/components/Grid/README.md) guidelines and the [theming guide](../../DesignSystem/Theming.md).

The 27 September gallery review in the [grid plan](../WIP/GridTextOverflow_2026-09-21.md) left two defects that
predate that work. Both change gallery pixels, so they shared one regeneration and one design-system republish.

## Indeterminate progress under reduced motion

Two CI gallery runs at identical inputs matched byte for byte except for the ProgressBar / Indeterminate tile in
rainbow light. Nine of those ten captures showed an empty track, so the published sheets showed no indeterminate
segment at all.

Cause:

- Every gallery theme enables reduced motion, but `ProgressBar` ignored it. `Paint` requested host animation
  whenever the bar was indeterminate, so the window host's 8 ms dispatcher timer ran during capture.
- The gallery's pre-capture `PumpMessages` drains the queue, so it handles that timer only when it is due. The
  number of ticks that reached the bar before `DebugCaptureBitmap` therefore depended on rendering time.
- The gallery also called `Tick(host, 300)` with a fake time. After a dispatcher tick had seeded the bar with the
  real clock, that call moved time backwards. The unsigned elapsed time wrapped, and converting the resulting
  phase to `int` was undefined. The phase stayed out of range, so the segment never drew again: the usual empty
  track. When no dispatcher tick came first, a later real tick placed the segment by the machine's uptime, as in
  rainbow light's frame at about 21% of the sweep.

Changes (`0dc1dcd`):

- [x] Under reduced motion `ProgressBar` requests no ticks from `SetIndeterminate` or `Paint`, and `Tick` reports
  false. The segment rests centered, from 30% to 70% of the track, matching the design-system preview. Restored
  motion resumes the sweep at the next paint.
- [x] `Tick` re-seeds when the clock moves backwards and drops whole 2,000 ms loops before converting, so no
  elapsed time can leave the phase range.
- [x] The gallery no longer ticks the bar with a fake time.
- [x] Tests cover the reduced-motion state and geometry (`TestProgressBarReducedMotionRestsIndeterminateSegment`)
  and backwards and very long clocks (`TestProgressBarIndeterminateTickSurvivesClockReset`). A pixel test
  (`TestProgressBarReducedMotionIndeterminateCaptureIsStatic`) ticks the bar and pumps across several timer
  intervals between two captures. The default palette follows the desktop animation setting, so the tests that
  assert ticking, including the embedded tick contract, now choose motion explicitly.

## High-contrast status tones and badges

In the high-contrast sheet the Grid tile's Info- and Warning-toned rows showed no text, and the Info/Warn badges
on unselected rows were missing.

Cause:

- The gallery's high-contrast theme comes from `MakeThemePalette` with background, text, selection and accent but
  no alert colors, so the six `ThemeColors` alert fields stay zero.
- `MakeThemePalette` copied them as given, so `infoFill`/`infoText`, `warningFill`/`warningText` and
  `errorFill`/`errorText` were fully transparent. Toned grid rows and tone badges painted invisible text, and the
  throughput graph's `warningText` series and limit line were exposed the same way.
- The design-system tokens already give high contrast `text` on `windowBackground` for all three severities, so
  the library disagreed with its own design system.

Changes (`ed1dea9`):

- [x] `MakeThemePalette` still copies supplied alert colors as given. A zero-alpha value, including the zero
  default, counts as unsupplied: the fill becomes `windowBackground` and the text becomes `text`.
- [x] The gallery keeps supplying no alert colors, so its high-contrast sheet exercises the fallback.
- [x] `TestThemeColorsPaletteFallsBackForUnsuppliedAlertColors` uses the gallery's high-contrast input plus a
  zero-alpha supplied value. Every tone badge and toned grid row resolves to opaque colors with at least 4.5:1
  contrast, and supplied colors are still copied.

## Validation and publication

- [x] x64 CI on `ed1dea9`: pull-request run 36381939973 and dispatch run 36382031032 pass x64 Debug, Release and
  ASan Debug, including the control, embedded and theme tests above.
- [x] All six native profiles on the publishing commit `2bef8f1`, the same library code: pull-request run
  36383419233 passes every job, and push run 36383416285 passes every job after one re-run. That run's first
  x64 Release attempt failed only the Tooltip suite's real-time hide-delay check, which is untouched here and
  passed in every other run. The pull request records its stall sensitivity and a proposed patch.
- [x] Local checks: the five Python validators, the 44 tool tests, the pinned clang-format 22.1.3 on every changed
  C++ file and `git diff --check`.
- [x] Gallery determinism. `0dc1dcd`'s push and pull-request runs (36380945948, 36380948594) produced
  byte-identical images, as did `ed1dea9`'s pull-request and dispatch runs. The light, dark and both rainbow sheets
  are byte-identical across all four runs. Against the published sheets, every theme changes only inside the
  indeterminate tile. The high-contrast sheet also shows the grid's toned-row text and Info/Warn badges, two Tree
  badges and the throughput graph's limit line. `docs/gallery` is the dispatch run's output, whose receipt names
  `ed1dea9`.
- [x] Design system republished as artifact version 9: the five theme sheets as new asset uploads, then the
  README, theming guide, tokens and the ProgressBar and Grid guidelines, then the index. The published files
  match the repository byte for byte.
- [x] Performance. A [hosted paired set](../../../Measurements/GalleryFollowUps/2026-09-28/paired-hosted/README.md)
  compares `3c882b2` with `ed1dea9` on the default complex-UI fixture. No memory, allocation or surface metric
  flags. One crossing flags clean frame p95 and dirty composition CPU p95, the other crossing moves both the
  other way, and the same-binary control moves them further, so they are runner timing noise. Nothing is
  rebaselined.
- [x] Closed out: contracts, design system, docs and measurements are current, and this plan moved to Done.

## Rules

With motion enabled, progress behavior is unchanged apart from the clock fix, and determinate and segmented bars
paint exactly as before. Supplied alert colors are unchanged, and the built-in palettes never pass through
`MakeThemePalette`. The complex-UI benchmark scene uses the built-in dark palette and only determinate bars, so its
paint computes the same values.

Observed but out of scope: in the high-contrast sheet the Grid's IconText icon stays dark blue on black, because
list icons blend toward `selectionFill`; that is not an alert token.
