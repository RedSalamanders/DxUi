#pragma once

// Retained text layouts of host-drawn plain text (Specs/Core/Core_PerformanceAndResources.md, "Retained text layouts"; plan
// item W2.1). The equivalence oracle draws every combination the callers use both through the host's layout table and
// through DrawText (the DebugSetTextLayoutCacheEnabledForTest seam) on WARP and requires identical BGRA; the lifetime test
// covers the key, the bound and what hiding, detaching, a device replacement and a DPI change do to the table.
void TestRetainedTextLayoutsDrawWhatDrawTextDraws();
void TestRetainedTextLayoutsKeyBoundAndLifetime();
void TestRetainedTextLayoutsReleasedWhenAChildHostCannotPaint();
