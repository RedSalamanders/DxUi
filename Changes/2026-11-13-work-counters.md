- Added private work counters (performance and motion plan W0.1): a thread-local record of plain increments, present in
  every configuration, counting `Paint` executions, paint/tick/hit-test walk visits, preparations, compositions, native
  renders and presents, repainted pixels, wakes by source, UI Automation snapshot builds with records and bytes, GDI
  backdrop captures and every counted native resource the library creates. Library code now creates text layouts and
  formats, geometries, stroke styles, brushes, gradient stops, bitmaps, effects, command lists, layers, device contexts and
  D3D11 devices, and draws `DrawText`, only through counted helpers; `Tools/tests/Test-CountedCreation.ps1` fails on a raw
  call whatever its receiver. Tests measure deltas with `WorkCounterScope` and count destructions and churn with a
  `ResourceLifetimeTracker`. The ComplexUi receipt records a dirty frame painting all 83 controls with 54 C++ allocations
  (Release) and a clean frame with none, no creation and no churn. Counting COM wrappers in test executables and a
  Direct2D draw-call count remain open.
