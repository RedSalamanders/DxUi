- Make the `FX6` accessibility performance fixture deterministic (performance and motion plan W0.4). With a client
  listening, an embedded view queues each UI Automation property change for a thread-pool work item; whether a change
  coalesced with one still queued, and whether the queue had to grow, depended on how far that work item had got, so one
  step made 118 or 120 C++ allocations and the determinism gate failed about one run in two. A test-only switch,
  `DebugSetAccessibilityNotificationDeliveryManualForTest`, keeps the owner's queuing unchanged but submits no work item,
  and `DebugCompletePendingAccessibilityNotificationsForTest` empties the queue as one delivery would, without raising
  anything; `FX6` completes the queue before each publication, so nothing it does reaches UI Automation whoever listens
  on the desktop. Library behaviour outside tests is unchanged.
