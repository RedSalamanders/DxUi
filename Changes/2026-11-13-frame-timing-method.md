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
  - Validation: `Tools/tests/Test-FrameTiming.ps1` (statistics; runner with shimmed executable, `powercfg`, build and
    observer, including restoration after a failed run and a refused preflight) and the WARP mechanics check
    `--test-frame-timing` in the ordinary `Embedded` suite. One smoke session ran on the hardware adapter under load;
    the five-session calibration receipt under `Measurements/PerformanceAndMotion_2026-11-13/W0.2/` waits for a quiet
    machine.
