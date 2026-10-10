# WindowHost first full presentation

Status: COMPLETED 2026-10-08. The implementation passed all 19 requested noninteractive suites in x64 Debug, Release
and ASan Debug, with 10 existing NewControls capability skips in each profile, and the same three native profiles on
ARM64. The identified x64 RedSalamander consumer candidate passed two selected native cases: the analysis-host
detached-provider/callback-guard case, and the lifecycle-capture case, which produced eight frames that all passed
edge probes. These results qualify
those contracts only; full application,
ARM64 consumer, assistive-technology, IME and hardware qualification remain separate gates. Retained Release paired
sets preserve the initial Retention flag (+11.85%) as unconfirmed; the unchanged-source repeat was within-noise with
unstable controls and did not establish that there is no performance cost or demonstrate an improvement. The hosted
Default flag also remains unconfirmed after a retry with no flagged metrics and unstable controls; neither run
establishes a confirmed regression or demonstrates a gain.
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
- [x] Complete ARM64 Debug, Release and ASan Debug native profiles; the consumer scope below remains separate.
- [x] Retain same-fixture Release comparisons, including all findings and the quiet repeat: [qualification evidence](../../../Measurements/ViewerSpace/2026-10-08/README.md).
- [x] Qualify the identified x64 RedSalamander consumer candidate: the analysis-host detached-provider/callback-guard
      case and lifecycle-capture case passed; the lifecycle case produced eight frames, all with passing
      physical-144-DPI edge probes. This is not full application or ARM64 consumer qualification.

The durable behavior is recorded in the [WindowHost contract](../../Rendering/Rendering_Win32Host.md). The six native
DxUi library profiles and the identified x64 consumer contracts above are qualified. This plan does not claim full
application, ARM64 consumer, assistive-technology, IME or hardware-specific presentation qualification; unstable
paired controls also leave smaller performance costs unresolved.
