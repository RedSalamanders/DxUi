- Changed native presentation (performance and motion plan W3.6): a `WindowHost` now paces its frames to the display.
  Its swap chain, in both presentation modes, has a frame-latency waitable object at latency 1 (`ResizeBuffers` passes
  the same flag), and the host presents only with a credit, one unit of that object, so it never blocks in `Present` and
  never queues a frame behind another. While something animates, a thread-pool wait on the object (the process default
  pool; DxUi starts no thread) posts one paced-frame message per frame, and animation time is the compositor's predicted
  present time (`DCompositionGetStatistics`), with the existing 50 ms hitch clamp, instead of an 8 ms timer; a tick
  that presents nothing keeps its credit and ticks again one refresh period later. `WM_PAINT` without a credit is
  validated and rendered by the next paced frame, so `UpdateWindow` no longer guarantees a presented frame when it
  returns (at most one refresh interval later, in order); `RenderInitialFrameForShow` waits briefly instead, and a
  diagnostic capture no longer presents. A stalled compositor costs one wake per 250 ms while a frame is pending and
  nothing otherwise. Idle hosts arm nothing; hiding, minimizing, detaching or losing the chain cancels and drains the
  wait before the chain is released. The embedded host is unchanged. New work counters (`frame.pacedFrames`,
  `presentsWithoutCredit`, `pacer…`, `wakes.pacedFrame`, `wakes.pacerTimer`) and a `FramePaced` trace event record it.
  No path presents without a credit, so at most one frame queues behind the one shown. Fixed the test-only resource
  lifetime tracker, which held a reference to each window host's back-buffer target bitmap so that every tracked resize
  failed `ResizeBuffers` and recreated the swap chain (the window fixture runner, `FX10` included); that bitmap is now
  counted but not held. Validation: the WindowHost suite's `TestFramePacing…` tests on WARP (0 pacer wakes with nothing armed on FX1 read-only,
  ten skipped presents without deadlock at no more than one wake per refresh period, FX10 presenting only with a credit,
  stale-frame rejection, a stalled compositor, a detach fuzz under ASan). Remaining gate: FX11 at 60 and 144 Hz on
  hardware (`TestFramePacingSampleTimeIsTargetTimeOnHardware`) and the focus-taking menu owner-flood test.
