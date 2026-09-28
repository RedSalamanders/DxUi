# Gallery follow-ups: static progress and high-contrast status

Status: **ACTIVE**. Library scope; consumer adoption stays separate.
Base: `3c882b2` on `ej/elegant-einstein-lmooqh`, after main `6b34456`.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [theme and motion](../../UI/UI_ThemeAndTypography.md),
the design-system [ProgressBar](../../DesignSystem/components/ProgressBar/README.md) and
[Grid](../../DesignSystem/components/Grid/README.md) guidelines and the [theming guide](../../DesignSystem/Theming.md).

The 27 September gallery review in the [grid plan](GridTextOverflow_2026-09-21.md) left two defects that predate
that work. Both change gallery pixels, so they share one regeneration and one design-system republish.

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

Changes:

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

Changes:

- [x] `MakeThemePalette` still copies supplied alert colors as given. A zero-alpha value, including the zero
  default, counts as unsupplied: the fill becomes `windowBackground` and the text becomes `text`.
- [x] The gallery keeps supplying no alert colors, so its high-contrast sheet exercises the fallback.
- [x] `TestThemeColorsPaletteFallsBackForUnsuppliedAlertColors` uses the gallery's high-contrast input plus a
  zero-alpha supplied value. Every tone badge and toned grid row resolves to opaque colors with at least 4.5:1
  contrast, and supplied colors are still copied.

## Validation and publication

- [ ] Native CI passes in all six profiles.
- [ ] Two gallery runs at identical inputs reproduce every sheet byte for byte. Against the published sheets only
  the indeterminate tile, in all five themes, and the high-contrast status colors change. Publish those sheets.
- [ ] Republish the design system: the README, theming guide, tokens, the ProgressBar and Grid guidelines, and
  the five theme sheets.
- [ ] Close out: move this plan to Done.

## Rules

With motion enabled, progress behavior is unchanged apart from the clock fix, and determinate and segmented bars
paint exactly as before. Supplied alert colors are unchanged, and the built-in palettes never pass through
`MakeThemePalette`. The complex-UI benchmark scene uses the built-in dark palette and only determinate bars, so its
paint computes the same values.
