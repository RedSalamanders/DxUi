#pragma once
// Plan item W2.4, "one render per menu open" (Specs/Rendering/Rendering_Win32Host.md, "Frame pacing", popups): a native host
// that renders its first frame before it is shown (as MenuPopup does) is not rendered again by the show while that frame is
// current. WindowHost suite, WARP, hidden non-activating windows shown with SW_SHOWNOACTIVATE off screen; nothing takes focus.
void TestPopupShowRendersTheFirstFrameOnce();
void TestPopupShowRendersAgainAfterAChangeBeforeTheShow();
void TestPopupShowAgainAfterHideRendersAsBefore();
