- Build performance fixtures `FX6` (accessibility), `FX7` (IME) and `FX13` (whole application) on the fixture framework
  (performance and motion plan W0.4), each on the embedded host with WARP receipts under
  `Measurements/PerformanceAndMotion_2026-11-13/`. `FX6` runs the W8.1 mutations with no UI Automation client and with
  one (the client gate's test switch, so counts never depend on event arrival), 100 `Slider` steps, a 200 px `Splitter`
  drag and a 64-step word walk over a 64 Ki-unit document through the text range provider. `FX7` composes and commits
  ten syllables through the production TSF store in a 64 Ki-unit `TextField` and `RichTextEditor`, one lock
  per step. `FX13` idles a 34-control application for 60 simulated seconds with nothing armed and with a focused caret,
  replays one second at the native 10 ms timer period, then hides it; `DxUi.ControlTests.exe --fx13-cold-start` measures
  cold start to the first presented frame of fresh processes as a receipt-only real-time record outside the
  determinism gate. A new work counter, `accessibility.wordUnitsVisited`, counts the units text-range word movement steps
  over; a definition's `largeAllocationBytes` adds the per-step `cppLargeAllocations` counter. The W8.5 TSF doubles
  are available to fixtures in `Embedded.Tests.TsfDoubles.h`. Library behaviour is unchanged; hardware video memory,
  timing and the interactive screen-reader and real-IME parts of the W8 gates remain open.
