- Four control defects are fixed (`W1.2`). `CardPanel::SetCornerRadius` repaints once when the radius changes and
  not at all for the same value. `Grid` keeps the unrounded offset the wheel's deltas lead to and draws the row nearest
  to it, so a precision touchpad's small deltas are no longer lost downward or rounded a whole row upward: four 30-unit
  deltas now scroll the three rows one notch does and four opposite ones return to the start row (before: 0 rows down,
  4 rows up); any other scroll restarts that target. `Grid`, `Tree` and a multiline `TextField` read lines per notch
  through one shared helper (`SPI_GETWHEELSCROLLLINES`, refreshed when a window host receives `WM_SETTINGCHANGE`,
  `WHEEL_PAGESCROLL` scrolling one page, 0 scrolling nothing) instead of `Grid` and `Tree` hard-coding three; tests
  replace the setting through the new diagnostics hook `DebugSetWheelScrollLinesForTest`, never the user's setting.
  `Ctrl+Shift+click` (and Ctrl+Shift with a movement key) in an extended `Grid` now adds the visible anchor-to-row range,
  skipping collapsed groups, to the existing selection and keeps the anchor, as Windows lists do; UI Automation names
  only the rows that joined. An embedded host, which receives no `WM_SETTINGCHANGE`, reads the setting again before each
  wheel event it dispatches, so its controls always use the current value, as `TextField` did before. New tests in the
  Control, Grid, Tree, Accessibility and Embedded suites cover each case.
