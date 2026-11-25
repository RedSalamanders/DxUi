#pragma once

// Native pointer-move pacing (performance and motion plan item W3.7; Specs/Rendering/Rendering_Win32Host.md, "Pointer-move
// pacing"). Deterministic clauses on WARP with the pacer's test seams, the work counters and the trace provider's test sink.
void TestPointerPacingIdleMoveDispatchesAtOnce();
void TestPointerPacingAtMostOneDispatchPerPresentedFrame();
void TestPointerPacingSecondMoveRendersTheOwedFrameFirst();
void TestPointerPacingPendingMoveIsDispatchedBeforeTick();
void TestPointerPacingEveryFlushingMessageSeesTheLatestPosition();
void TestPointerPacingButtonDownSeesTheLatestPosition();
void TestPointerPacingCursorAnswersTheLatestPoint();
void TestPointerPacingUnpacedHiddenAndStalledHostsBoundTheWait();
void TestPointerPacingNestedDeliveryNeverRendersReentrantly();
void TestPointerPacingReleasedChainFlushesByIdentity();
void TestPointerPacingKeyboardIsNotCoalesced();
// Hardware only (a capability skip on WARP or without a hardware adapter): the FX3 grid in a WindowHost under a stream of
// pointer moves, input to present per frame-owing dispatch from the first move it absorbed, written to
// .build/w37/input-to-present-<refresh>hz.json; judged only with DXUI_W37_HARDWARE_GATE=1.
void TestPointerPacingInputToPresentOnHardware();
