- Presented-frame truth test (performance and motion plan item `W0.6`). New WindowHost suite cases
  `TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnWarp` and `...OnHardware` present a full frame, a partial
  frame over rectangle A and one over an overlapping rectangle B, and read the image the compositor shows through a new,
  reusable, non-activating test-harness capture (`Tests/Support/Support.Tests.PresentedWindowCapture.h`: `DwmFlush`,
  then `PrintWindow` with `PW_RENDERFULLCONTENT` of a per-monitor-aware window placed off-screen at the bottom of the
  z-order, answering PrintWindow's own `WM_PAINT` so the host neither renders nor presents during the read, and waiting
  for a frame-identifying pixel one compositor frame at a time). On WARP and on an NVIDIA GeForce RTX 5080, in both
  `HwndSwapChain` and `CompositionSwapChain`, the presented image after B has 0 BGRA differences from a full render of
  the same state, outside B included; the back buffer read before B's `Present` holds frame 0's A-only pixels; a
  negative control whose dirty rectangle misses a change shows exactly the missed pixels. A machine without a hardware
  adapter reports the hardware case as a capability skip (allowed on GitHub-hosted Windows runners). New diagnostic
  overload `ControlHost::DebugRenderDirtyRectForTest(rect, prePresentBackBuffer)` reads the back buffer before
  `Present`. The observation is recorded in `Specs/Rendering/Rendering_Win32Host.md` with captures under
  `Measurements/PerformanceAndMotion_2026-11-13/W0.6/`. ARM64 execution remains a separate gate.
