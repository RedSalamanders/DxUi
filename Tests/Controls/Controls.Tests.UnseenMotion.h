#pragma once

// Unseen motion on the window host (plan item W3.4; Specs/Rendering/Rendering_Win32Host.md, "Unseen motion"). Registered in
// the Animation suite. The embedded clauses and the FX11 gate are in Tests/Embedded/Embedded.Tests.UnseenMotion.h; the
// oracle script runs on both hosts (Tests/Support/Support.Tests.UnseenMotion.h).
void TestUnseenMotionNativeLoopHoldsAndCatchesUpBeforePaint();
void TestUnseenMotionNativeDisclosureDoesNotJumpOnAHitchWhileSeen();
void TestUnseenMotionNativeCostOfUnseenWork();
