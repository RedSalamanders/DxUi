- Fixed the intermittent window-host failures of the performance-fixture determinism gate (plan W0.4 test
  infrastructure): a UI Automation client elsewhere on the desktop sent `WM_GETOBJECT` to the fixture window in the
  middle of a step, which published the host's accessibility snapshot early and split the step's one coalesced
  publication in two (`FX10` copied its rich document twice, `FX12` built two snapshots). The window runner now answers
  every message another thread sends to the fixture window with `DefWindowProc`, so the host never sees it, and pins the
  accessibility build gate to a listening client for the run instead of asking `UiaClientsAreListening`. When the gate
  still fails it prints, for the differing step and the one before it, every counter that differs and the window
  messages, fixture ticks, snapshot builds and frame-schedule state that reached the host in one run and not in the
  other. The gallery's 100% tile test now names the tiles and the rectangle in which two captures of one sheet differ,
  writes both crops under `.build/test-artifacts/gallery-determinism`, and reports a hovered menu row or a frame schedule
  still waiting when the sheet is captured.
