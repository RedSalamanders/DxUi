- Reset the native IME undo fixture's text and undo history before each admitted foreground attempt, preserving
  the same editor and its original composition notifications, two-undo sequence and late-result checks.
  A forced first-loss regression also verifies that a third undo cannot expose a discarded setup edit.
  Bounded per-attempt diagnostics retain the setup observations. The activation helper services sent messages
  with the existing 800 ms polling deadline; sent callbacks remain subject to the test watchdog. Foreground
  authorization and retry limits stay unchanged. Test guidance and the validation contract describe the setup.
  This fixture/helper repair changes no control rendering, gallery tiles or design-system previews.
