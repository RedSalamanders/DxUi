- A pointer move walks the control tree once (`W2.9`). A window host keeps the hit result of `WM_SETCURSOR` for the
  mouse message Windows delivers with it, and an `EmbeddedHost::DispatchPointer` shares one result between its hover
  update and its dispatch; no result outlives its pointer event. The result is keyed by the exact point, the root, the
  interaction revision and a new hit revision, which moves on popup open, close and refilter, scroll offsets, page
  transitions, `Splitter`, `Slider` and `PageIndicator` hit geometry, overlay dismissal, size, DPI, device loss, detach and
  hiding. New public `Control::InvalidateHitTest()` moves it; a control class outside the library (a subclass of a
  library control included) turns reuse off while attached unless its constructor calls the new protected
  `Control::DeclareHitTestReported()`. Measured on the same fixtures before and after: `FX14` went from 2 to 1 tree hit
  tests per move on the embedded host and from 2 to 1 per announced move on the window host (`FX14` window steps now make
  the cursor query `WM_SETCURSOR` makes), and `FX2` popup hover from 2 to 1. `Label` and `Button` visual preparation no
  longer run an `IsControlInTree` search, and plain labels (no mnemonic, no typography override) are no longer prepared:
  a live-resize step of `FX10` went from 11 membership searches and 10 plain-label preparations to 0. A seeded
  differential against the uncached hit test on both hosts, lifetime tests (removal between the cursor query and its
  move and from inside the control's own callbacks) and the `Label` gallery tiles at 100%, 150% and 200% are unchanged.
  New work counters: `frame.hitTests`, `frame.hitTestReuses`, `frame.preparationMembershipSearches` and
  `frame.plainLabelPreparations`. Native menu popups are covered only by the focus-taking suites, which were not run.
