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
