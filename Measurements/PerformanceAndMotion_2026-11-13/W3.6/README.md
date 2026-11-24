# W3.6 native pacing: hardware evidence

Plan item `W3.6` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behaviour
in the [window host contract](../../../Specs/Rendering/Rendering_Win32Host.md#frame-pacing).

## `fx11-120hz-hardware-x64-debug.json`

- **Scenario**: `TestFramePacingSampleTimeIsTargetTimeOnHardware` (WindowHost suite). `FX11`'s 60 Hz rate block of state
  changes (page slide, tree expand and collapse, toggle, checkbox, slider, combo popup) applied four times, each left to
  animate for 400 ms on the host's own clock, paced by its swap chain, on a shown, non-activating, off-screen window.
  Every paced frame's `FramePaced` trace event is compared with its compositor target; DXGI's frame statistics give the
  vertical blank each present was displayed at.
- **Source**: the W3.6 commit before its last rebase (`8dfd168a3`), x64 Debug. The receipt's `benchmarkInputs` are the
  SHA-256 of its inputs as retained in the commit ending `(W3.6)`, rebased since onto the integration tip; the library and
  test sources of the measurement are those of `8dfd168a3`. The identity fields (`workloadOwner`, `fixture`,
  `benchmarkInputs`) were added to the test's output when it was retained here.
- **Machine**: the development workstation, Windows 10.0.26300, default adapter NVIDIA GeForce RTX 5080 (driver
  617.14), its display at 120 Hz (the second display is an AMD Radeon at 60 Hz).
- **Conditions**: taken under load. Other workers were building and running suites on the same machine; this is evidence,
  not the timing verdict, which belongs to the hardware slot (assumption `A6`).

| Field | Value |
| --- | --- |
| Paced frames | 607 |
| Sample equal to its target within 1 ms | 607 (100%; 0 clamped, 0 without statistics) |
| Presents DXGI reported displayed and matched to a frame | 595 |
| Displayed at the predicted vertical blank | 0 |
| Displayed 1 / 2 / 4 refresh periods after the predicted target | 77 / 483 / 35 |

Reading: the gate as agreed (the animation sample within the clock's 1 ms of the frame's `targetTime` in at least 99% of
paced frames) is met at 120 Hz on this machine. The frame is displayed a near-constant two refresh periods after the target
it was sampled for (an earlier run under the same conditions: 537 of 599). The offset is constant, and motion needs
uniformly spaced samples, not samples at the displayed time, so the phase stays at the first target after now and is not
tuned here.

## The FX10 window receipts before W3.6

The window receipts of `FX10` taken before W3.6 (`../FX10/fx10-default-window-warp-x64-release.json` and
`../FX10/fx10-uhd-window-warp-x64-release.json` as first recorded) count one swap chain created per resize step (16 per
run) and a second render on those steps. That came from the test harness, not from the library: the fixture runner's
`ResourceLifetimeTracker` held an extra reference to the window host's back-buffer target bitmap, and `ResizeBuffers`
refuses while any buffer reference remains, so each resize failed and the chain was recreated. W3.6 counts that bitmap
without holding it (`CountedCreateSwapChainTargetBitmap`), and the regenerated receipts show one chain, resized at every
step. The other retained window receipts create one chain and never resize it, so the tracker did not change their counts;
`FX4`'s interactive window receipt counts 13 swap chains for its menu popups: one per popup is expected, but a popup that
resized under the tracker would also have recreated its chain, so its `frame.swapChains` (the `G10` device line is not
affected) is unverified until that receipt is retaken in the focus-taking run.

Open, for the hardware slot: the same test at 60 and 144 Hz on a machine not loaded by other work, with the gate judged:
`$env:DXUI_W36_HARDWARE_GATE='1'; ./test.ps1 -Configuration Release -Platform x64 -Suites WindowHost -Tests TestFramePacingSampleTimeIsTargetTimeOnHardware`
(the receipt is written to `.build/w36/fx11-<refresh>hz.json`).
