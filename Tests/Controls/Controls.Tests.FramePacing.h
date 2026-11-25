#pragma once

// Native frame pacing (plan item W3.6; Specs/Rendering/Rendering_Win32Host.md, "Frame pacing").
void TestFramePacingWaitableSemanticsOnWarp();
void TestFramePacingIdleArmsNothing();
void TestFramePacingHideDuringArmedWaitWakesNothing();
void TestFramePacingTenSkippedPresentsNeverDeadlock();
void TestFramePacingFx10SweepPresentsOnlyWithCredit();
void TestFramePacingResizePresentsOnlyAtTheNewSize();
void TestFramePacingRejectsStaleFrames();
void TestFramePacingSynchronousRendersBuildNoLatency();
void TestFramePacingCaptureNeitherTakesNorNeedsCredit();
void TestFramePacingStalledCompositorDefersWithoutPresenting();
void TestFramePacingSampleTimeIsTheTargetTime();
void TestFramePacingEmbeddedHostOwnsNoPacer();
void TestFramePacingDetachFuzz();
// Hardware only (a capability skip on WARP or without a hardware adapter): FX11 transitions driven by the display, the
// animation sample against the compositor target per paced frame, written to .build/w36/fx11-<refresh>hz.json.
void TestFramePacingSampleTimeIsTargetTimeOnHardware();
// Review round 2: a failed present keeps the credit (A1), an out-of-range compositor target (E1), the credit seam keeps a
// deferred paint's wake (G1), the cancel race (K3), a foreign message pump, and publication after presentation (H2).
void TestFramePacingFailedPresentKeepsTheCredit();
void TestFramePacingSuccessStatusKeepsTheCredit();
void TestFramePacingOutOfRangeTargetDoesNotFreezeTime();
void TestFramePacingCreditSeamKeepsDeferredPaint();
void TestFramePacingCancelRaceKeepsTheUnit();
void TestFramePacingForeignMessagePumpKeepsPainting();
void TestFramePacingUnpresentedRenderPublishesNothing();
