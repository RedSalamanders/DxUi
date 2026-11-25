- Unseen motion (performance and motion plan W3.4): a library control's visual motion that nobody can see costs no frames.
  The frame schedule tags the pure visual motion of the library's own classes (indeterminate `ProgressBar`, `Slider`,
  `ThroughputGraph`, the hover, focus and disclosure transitions of `Button`, `Toggle`, `Checkbox` and `RadioButton`, `Tree`
  chevrons and expansion slides, `Grid` spinners and sort glyph, scrollbar transitions) with the time it settles. While its
  control is hidden or its painted extent misses every `ScrollPanel` viewport above it and the view (the paint region's own
  mapping, independent of the paint-region switch; unknown means seen), a transition is ticked once, at its settle time,
  and a loop not at all; a host whose only motion is unseen leaves its dispatcher and pacer, and an embedded view reports
  `None`. The frame that shows held motion again (a scroll, a reselected tab, the host shown) ticks it first, after layout
  and before paint, so it is never one frame late, and arms the next frame: a native host joins its dispatcher or pacer, an
  embedded preparation calls `requestPreparation`. Motion is computed from its start on the host's clock, so a loop resumes
  in phase and a transition is finished or part way, never restarted: the `ProgressBar` loop's phase, the `ThroughputGraph`
  start and the scrollbar transitions' anchor now come from the clock. The visibility test runs only when a revision that
  can show or hide a control moved (with 1,000 held requests a frame that changes nothing tests none). Inside a `Tree`, a
  turning chevron whose row is collapsed away or scrolled out keeps no frames. Behavior change: the `Button` disclosure
  chevron runs on the host's clock from the state change (its 100 ms per-tick clamp is gone): after a hidden period or a
  pause in ticks it shows where the clock puts it instead of resuming where it stopped; a native stall moves it by at most
  the dispatcher's 50 ms hitch clamp, and an embedded application's clock is the motion clock, so a long delta moves it by
  the whole delta (clamp the deltas you pass for hitch protection). Untagged and unchanged: application controls,
  `PageHost` slides, carets, the rich editor's middle-button scrolling, the menu popup slider, the tooltip layer. New
  counters `tickHeldUnseen`, `tickCatchUps`, `motionSeenRefreshes`, `catchUpLayoutRepeats`; `FX11` gains an `unseen-60hz`
  phase after its existing ones. Validation: `Controls.Tests.UnseenMotion.cpp`, `Embedded.Tests.UnseenMotion.h`, the
  `unseen-motion` incremental-versus-fresh script on both hosts with a negative control. Open: application controls cannot
  opt in (no public tag), a window covered by another window counts as seen, carets keep W3.2's rules.
