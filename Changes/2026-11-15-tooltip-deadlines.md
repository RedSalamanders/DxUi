- A tooltip now wakes its host only at its show and hide deadlines (`W3.3`). Show and hide are deadlines of the host's
  frame schedule, read from the tooltip layer itself, so leaving a waiting mode (pointer leave, replacement, dismissal,
  capture change, inspection, detach) withdraws its deadline, and `TooltipLayer::Tick` returns false: the layer is no
  longer ticked on every frame while a show is pending and for the 5 s a tooltip is shown, and a native host runs no
  periodic timer for it. On `FX14` the tooltip is ticked once at its show and once at its hide deadline and never between
  them, on both hosts; on a native window in real time the tooltip costs 2 timer wakes, both deadline wakes. A host that
  is hidden drops a tooltip still waiting to show; a shown tooltip's hide is held and fires on show. The show delay (the
  user's mouse hover time, clamped to 100-2,500 ms) is read once per host and again on `RefreshTooltipHoverTime()`,
  which a window host calls on `WM_SETTINGCHANGE` and an embedded host's application calls when its window receives one.
  New work counters `tooltipDeadlineTicks` and `tooltipShowsDropped`; `FX14` gains a `hide` phase.
  While a tooltip is pending or shown, hiding, disabling, removing or retitling its target (or hiding an ancestor), and a
  `Grid` or `Tree` data change or a removed tab under the resting pointer, make one wake follow at once that resolves the
  hover again, so a tooltip no longer outlives its target or row for up to 5 s (or, for `Grid` and `Tree`, until the
  pointer moved); with no tooltip these changes cost nothing, and a wake or embedded advance resolves the target only
  when marked or at the tooltip's own deadline. New work counters `tooltipTargetMarks` and `tooltipTargetValidations`.