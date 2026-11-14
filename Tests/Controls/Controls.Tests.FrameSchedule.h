#pragma once

// The frame schedule (plan item W3.1; Specs/Rendering/Rendering_Win32Host.md, "Frame schedule").
void TestFrameScheduleTicksOnlyDueRequesters();
void TestFrameScheduleLegacyRequestStillAnimatesDerivedControl();
void TestFrameScheduleHiddenHostHoldsItsSchedule();
void TestFrameScheduleDispatcherArmsOneTimerForTheEarliestDeadline();
void TestFrameScheduleFirstTickAfterIdleAddsNoPhantomFrame();
void TestFrameScheduleLifetimeFuzz();
void TestFrameScheduleNativeFx1CaretWakesInRealTime();
void TestFrameScheduleDispatcherThreadExitWithoutFailureHost();

// The child process of TestFrameScheduleDispatcherThreadExitWithoutFailureHost; -1 when the arguments name another mode.
int RunFrameScheduleThreadExitChild(int argc, wchar_t** argv);
