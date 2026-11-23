#pragma once

// Text-input synchronization once per input event (Specs/UI/UI_InputAndAccessibility.md, "Rich text-input snapshots"): after a
// key or character a native host skips its own synchronization only when the cache already holds what it would store, and
// still refreshes the caret geometry. Driven through WM_CHAR and WM_KEYDOWN sent to a WARP WindowHost that is shown without
// activation, off every monitor, with the control focused inside the host only: nothing takes the desktop's focus.
void TestTextInputSyncOnceRefreshesTheCaretAfterAFormatterScrolls();
void TestTextInputSyncOnceRefreshesTheCaretAfterARichCallbackScrolls();
void TestTextInputSyncOnceSynchronizesAPasswordRevealChangeMadeAfterTheControlsSync();
void TestTextInputSyncOnceSkipsWhenTheControlAlreadySynchronizedTheSameState();
