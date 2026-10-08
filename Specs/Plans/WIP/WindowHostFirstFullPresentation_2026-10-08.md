# WindowHost first full presentation

Status: ACTIVE: source implemented. On x64, each of Debug, Release and ASan Debug passed all 19 requested
noninteractive suites; NewControls recorded 10 existing capability skips in each profile. ARM64 Debug, Release and
ASan Debug cross-builds also passed. Retained paired evidence did not confirm the first retention-scene timing finding. Native ARM64 runtime and consumer capture
qualification remain pending.
Date: 2026-10-08
Owner: [Window hosting contract](../../Rendering/Rendering_Win32Host.md)

## Problem

A native flip-model swap chain can receive a partial WM_PAINT before any complete frame has populated its buffers.
Likewise, ResizeBuffers invalidates retained contents. Treating either event as ordinary dirty rendering can issue
Present1 with a dirty rectangle when a complete frame is required.

## Implementation

- [x] Mark new and resized native swap-chain buffers as requiring full-frame rendering and presentation.
- [x] Clear the pending state only after EndDraw and Present both return S_OK; retain it through occlusion and failure.
- [x] Add a deterministic native rendering regression for first partial invalidation after creation and resize.
- [x] Run the x64 Debug, Release and ASan Debug suites: all 19 requested noninteractive suites passed in each profile,
      including the Rendering regression; NewControls recorded 10 existing capability skips per profile.
- [x] Complete ARM64 Debug, Release and ASan Debug cross-builds; these are not native runtime passes.
- [x] Retain same-fixture Release comparisons, including all findings and the quiet repeat: [qualification evidence](../../../Measurements/ViewerSpace/2026-10-08/README.md).
- [ ] Complete native ARM64 runtime qualification and consumer
      capture qualification; retain exact receipts and report presentation failures separately from screenshot-capture
      success.

The durable behavior is recorded in the [WindowHost contract](../../Rendering/Rendering_Win32Host.md). This plan does
not claim ARM64, paired performance, consumer capture or hardware-specific presentation qualification.
