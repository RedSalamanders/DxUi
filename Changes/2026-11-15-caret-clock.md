- Changed caret blinking (performance and motion plan W3.2): one caret clock per host now owns the blink of the focused
  `TextField`, editable `ComboBox` and `RichTextEditor`. It follows the user's caret blink time (`GetCaretBlinkTime`;
  `INFINITE` is steady) instead of fixed 530 ms and 500 ms periods, stops blinking with the caret shown after the caret
  timeout (`SPI_GETCARETTIMEOUT`) until input restarts it, and keeps the caret steady under the theme's reduced motion,
  which `TextField` and `ComboBox` previously ignored. A focused read-only `TextField` now shows a steady caret and
  schedules nothing (it used to blink). Input shows the caret at once and restarts the phase; showing a hidden or
  minimized host restarts a focused caret. Each flip is one deadline, so a steady or timed-out caret leaves the frame
  schedule empty. Added `ControlHost::RefreshCaretBlinkSettings()`, which a window host calls on `WM_SETTINGCHANGE`
  and an embedded application calls when its own window receives it, and the test seam
  `ControlHost::DebugSetCaretBlinkSettingsForTest`. On `FX1` the editable caret wakes 10 times in 10 s (was 18) and 0
  times after the timeout, and the read-only caret wakes nothing.
