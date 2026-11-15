- Add a hardware frame timing method that can confirm or rule out a small timing change (`W0.2`, an input to the
  D-P1 timing policy). `performance-timing.ps1` runs interleaved A/B/B/A sessions of fresh
  `DxUi.EmbeddedTests.exe --timing-method` processes, each at least 1,000 frames and 2 s. The test-only harness
  `Tests/Embedded/Embedded.Tests.FrameTiming.h` records per frame the UI thread's preparation and composition cycles
  (`QueryThreadCycleTime`) separately, the submission time, and GPU time from D3D11 timestamps inside a disjoint query,
  read one frame late. `Tools/FrameTiming.psm1` judges six metrics against declared equivalence margins (3% CPU,
  5% GPU) with two one-sided exact sign-flip tests, reports the minimum detectable effect, and classifies each metric as
  judged or advisory from its same-binary noise floor in a calibration. The runner activates the existing High
  Performance power scheme and restores the original however the study ends, never creating a scheme or elevating;
  pins only the measuring UI thread; and refuses a session whose `Tools/Watch-BenchmarkHost.ps1` idle preflight finds
  the host busier than 10%. Fixtures are pluggable (`FrameTiming::MakeFixture`); `FX5` is the ComplexUi scene. The
  canonical five-round receipt, the paired judge and the benchmark inputs copied onto baseline trees are unchanged.
  - Every timed dirty frame is a declared fixture step: a timed phase that cannot continue past its last step is
    replayed from a rebuilt scene, untimed between cycles (`Definition::timedPhaseRepeats` marks FX5's as repeating).
    A judged dirty run requires every frame to prepare work, and a metric whose clock step exceeds a tenth of its
    margin is advisory. Committed receipts are the retained form (per-run summaries, raw report SHA-256 and size,
    judged statistics); `Tools/Convert-FrameTimingReceipt.ps1` converts a full receipt and refuses any changed value.
  - Calibration, 15 November 2026, RTX 5080, source `9574105be`: FX5 dirty judges median preparation cycles (noise
    floor 0.55%), median submission time (0.55%) and median GPU frame time (1.24%); composition cycles, p95 UI-thread
    cycles and p95 GPU time are advisory. The first FX3 calibration timed about 100,000 frames past the end of the
    Grid's list per run (0.00% floors) and is not retained; FX3 is to be recalibrated.
  - Validation: `Tools/tests/Test-FrameTiming.ps1` (statistics, resolution rule, retained-form conversion; runner with
    shimmed executable, `powercfg`, build and observer, including restoration after a failed run and a refused
    preflight) and the WARP mechanics check `--test-frame-timing` in the ordinary `Embedded` suite, which now also
    checks that FX3 replays its 100 declared wheel notches with every frame preparing work.
