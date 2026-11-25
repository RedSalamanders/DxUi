#pragma once
// Plan item W2.4, G10 (Specs/Rendering/Rendering_Win32Host.md, "Shared device stack"): a thread's shared native device stack
// is kept past its last native host or popup while an EmbeddedHost is attached on the thread. WindowHost suite, WARP; every
// popup session is a hidden, never shown, non-activating window on a fresh thread, attached as MenuPopup attaches.
void TestSharedDeviceStackKeptAcrossEmbeddedOnlyMenuSessions();
void TestSharedDeviceStackReleasedAtLastDetachWithoutEmbeddedHost();
void TestSharedDeviceStackReleasedWithTheLastEmbeddedHost();
void TestSharedDeviceStackDeviceLossWhileIdle();
void TestSharedDeviceStackRegistryRowCountsAndDrops();
void TestSharedDeviceStackNativeHostUnaffected();
