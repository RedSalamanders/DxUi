- Control tests no longer depend on how fast they run (W1.1's recovery-retry test). `DxUi.ControlTests` disables
  window ghosting for its process: its tests paint through `UpdateWindow` without a message loop, and once Windows
  declared a test window hung (five seconds without a message check) and ghosted it, invalidations left no update
  region, so `TestWindowHostRecoveryRetryBacksOffOnTheDeclaredSchedule` failed in x64 ASan Debug at whichever step
  crossed that line (10 of 24 runs before the frame scheduler, 22 of 36 after it). With ghosting disabled it passed 20
  of 20 under ASan; its assertions are unchanged.
