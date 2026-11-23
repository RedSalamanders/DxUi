- One frame schedule per host (performance and motion plan W3.1). Each `ControlHost` keeps per-control requests and
  ticks only the controls holding a due request, never the tree. Additive API: `ControlHost::RequestAnimation(control)`
  (every frame while `Tick` returns true), `RequestAnimationAt(control, dueTickMs)` (one tick at a deadline) and
  `CancelAnimation(control)`. `RequestAnimation()` without a control keeps working: it ticks the visible tree once and each
  control whose `Tick` returns true keeps a request of its own; derived controls should name themselves instead. Every
  library control now requests for itself, and the carets of `TextField`, editable `ComboBox` and `RichTextEditor` sleep
  between blinks on one deadline per phase flip at their existing periods. Native hosts share one dispatcher timer per
  thread: the existing periodic cadence only while some control needs every frame, otherwise one coalescable one-shot
  timer for the earliest deadline, and nothing when nothing waits (a focused field: about 19 wakes in 10 s instead of
  about 1,000). The first tick after idle no longer adds a phantom 8 ms frame. Hidden, minimized and
  presentation-hidden hosts hold their schedule and are not woken; a deadline that passed meanwhile fires once on show.
  `Panel::Tick` and `PageHost::Tick` called during a host's scheduled wake no longer walk their children (a direct call
  still does). Embedded: `NeedsAnimation()` stays true while a deadline is pending and `AdvanceAnimation` ticks only due
  requests; the clock rules are unchanged. A dispatcher window that survives until thread-local destruction (a host left
  attached when its thread ends) is torn down without entering DxUi work, so the thread's exit no longer fails for a
  missing failure host. New private work counters: `wakes.deadlineTimer`, `frame.tickCalls`, `frame.tickRequesters`,
  `frame.tickLegacyTreeWalks`, `frame.tickDiscoveryWalks` and `frame.tickHeldWhileHidden`.
