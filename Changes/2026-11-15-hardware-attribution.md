- Add a hardware attribution method that records where a frame's time goes without kernel tracing (`W0.7`).
  `performance-attribution.ps1` runs `FX5`, `FX3` and `FX12` at 1920x1080 and 3840x2160, a full frame and a typical dirty
  frame each, as three fresh processes per case: the frame timing method unchanged (UI-thread CPU and GPU time), a phases
  pass that times the calls the test-only counting wrappers forward (`BeginDraw`, `EndDraw`, `Flush`, every Direct2D draw
  call, DirectWrite layout and format creation) beside the `W0.1` counters, and a sampling pass whose test-process sampler
  attributes the UI thread's instruction pointer to modules (DxUi, `d2d1`, `dwrite`, `d3d11` and the driver, runtime,
  system). `DxUi.ControlTests.exe --present-attribution` measures `Present` on a hardware `WindowHost` of the
  non-activating, off-screen test window. The runner reads the power scheme without switching it, refuses a receipt unless
  High performance is active and the host is quiet, probes kernel sampling once (`wpr`, unelevated) and records the
  refusal, judges every pass with the frame timing method's admission rule (interference with the measuring thread on an isolated
  core, `interference-interim-v2`, by default),
  measures a pass again when it was not admitted or its frames did no work (bounded per pass and in all,
  every attempt recorded), checkpoints after every pass so a later slot resumes with only the missing passes, measures the two
  decision cases alone with `-CaseSet Decision`, and keeps per-frame data in the build tree. GPU engine running time is read in
  `Tests/Support/Support.Tests.GpuEngineTime.h`, per engine instance. Receipt-only, hardware-only variants `FX3@fhd`, `FX3@uhd-192dpi`,
  `FX5@fhd-144dpi` and `FX12@form-layout-10k-uhd` supply the two sizes. No library code changes.
  - Validation: `Tools/tests/Test-FrameAttribution.ps1` (summaries, decision-rule regions, runner admissions with shimmed
    executables, `powercfg`, `wpr` and observer) and the WARP mechanics check `--test-frame-attribution` in the ordinary
    `Embedded` suite. The receipt under `Measurements/PerformanceAndMotion_2026-11-13/W0.7/` needs a quiet hardware slot.
