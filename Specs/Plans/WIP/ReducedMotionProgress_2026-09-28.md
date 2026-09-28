# Reduced-motion indeterminate progress

Status: **ACTIVE**. Library scope; consumer adoption stays separate.
Base: `3c882b2` on `ej/elegant-einstein-lmooqh`, after main `6b34456`.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [theme and motion](../../UI/UI_ThemeAndTypography.md)
and the design-system [ProgressBar guideline](../../DesignSystem/components/ProgressBar/README.md).

## Goal

Make the gallery's ProgressBar / Indeterminate tile deterministic and meaningful. On 27 September two CI gallery
runs at identical inputs matched byte for byte except for that tile in rainbow light. Nine of those ten captures
showed an empty track, so the published sheets showed no indeterminate segment at all.

## Cause

- Every gallery theme enables reduced motion, but `ProgressBar` ignored it. `Paint` requested host animation
  whenever the bar was indeterminate, so the window host's 8 ms dispatcher timer ran during capture.
- The gallery's pre-capture `PumpMessages` drains the queue, so it handles that timer only when it is due. The
  number of ticks that reached the bar before `DebugCaptureBitmap` therefore depended on rendering time.
- The gallery also called `Tick(host, 300)` with a fake time. After a dispatcher tick had seeded the bar with the
  real clock, that call moved time backwards. The unsigned elapsed time wrapped, and converting the resulting
  phase to `int` was undefined. The phase stayed out of range, so the segment never drew again: the usual empty
  track. When no dispatcher tick came first, a later real tick placed the segment by the machine's uptime, as in
  rainbow light's frame at about 21% of the sweep.

## Changes

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
- [ ] Native CI passes in all six profiles.
- [ ] Two gallery runs at identical inputs reproduce every sheet byte for byte, and only the indeterminate tile
  changes. Publish those sheets.
- [ ] Republish the design-system guidelines and the five theme sheets.
- [ ] Close out: move this plan to Done.

## Rules

With motion enabled, behavior is unchanged apart from the clock fix. Determinate and segmented bars paint exactly
as before. The complex-UI benchmark scene uses only determinate bars, whose paint computes the same values.
