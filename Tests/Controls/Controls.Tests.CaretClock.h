#pragma once

// The caret clock (plan item W3.2; Specs/Rendering/Rendering_Win32Host.md, "Caret clock"). Every case drives the caret
// settings through the host's test seam, never the user's settings.
void TestCaretClockSettingsCases();
void TestCaretClockServesEveryCaretOwner();
void TestCaretClockReadOnlyCaretIsSteady();
void TestCaretClockInputRestartsPhaseAndTimeout();
void TestCaretClockSettingChangeWhileFocused();
void TestCaretClockOneOwnerPerHost();
void TestCaretClockEmbeddedHiddenPopupAndHiddenOwner();
void TestCaretClockNativeSettingChangeAndDeviceLoss();
