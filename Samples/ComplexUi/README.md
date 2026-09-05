# Independent complex-UI sample

`ComplexUiScene.h` is a library-owned synthetic workload shared by the runnable sample and timed benchmark.
It has 83 controls: one root, 16 cards with four children each, a Grid and a Tree backed by 1,000 generated records.
Its models outlive the views that borrow them. It imports only DxUi/public platform dependencies.

Run `DxUi.EmbeddedControls.exe --complex-ui` after building, or add `--output image.png` for headless inspection.
The host uses a fixed 1280x720 canvas. `Update(frame)` applies deterministic slider/progress values and scrolling;
the benchmark invokes it only for warm-up/dirty frames. The interactive example remains event-driven while idle.

See [sample commands](../../docs/samples.md) and [measurement protocol](../../docs/performance.md).
