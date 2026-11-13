- Host recovery and input (performance and motion plan item `W1.1`). A native `WindowHost` whose device, swap chain,
  back buffer, target bitmap or `ResizeBuffers` fails to recreate while its window can present, or that loses the
  device, now arms one one-shot retry on its own HWND instead of staying blank or repainting in a tight loss loop: the
  first retry waits one frame (16 ms), each further failure doubles the wait up to 1 s, and a successful `Present`
  disarms it and resets the schedule (`Specs/Rendering/Rendering_Win32Host.md`). `EmbeddedHost` keeps no timer: a
  failed surface returns its HRESULT, stays dirty and requests no preparation of its own. Both hosts deliver the middle
  button's release to the new `Control::OnMouseMiddleUp` (the default declines). Embedded capture follows a button
  mask: a middle press the captured control accepts joins a primary drag, which then ends with the last button up.
  `ScrollPanel` at a scroll bound invalidates nothing and leaves the wheel unhandled, so an enclosing ScrollPanel
  scrolls. Resizing a native swap chain keeps the host's solid-brush cache (only a device change clears it), the dead
  non-diagnostic `Render` body is deleted, and the partial-present comment now names the real reason a `Present1`
  frame stays correct. New test hooks: `DebugSetRecoveryRetryClockForTest`, `DebugAdvanceRecoveryRetryClockForTest`,
  `DebugGetArmedRecoveryRetryDelayMs` and `DebugGetSolidBrushCreateCount`; the size-resource fault injection also fails
  an embedded surface's target bitmap at stage 3. A ScrollPanel forwards the middle release to its child in content
  coordinates (the child under the pointer, or the one owning its forwarded gesture) and no longer turns a middle press
  the child did not capture into a held capture. Validated with new WindowHost and Embedded suite cases on x64; ARM64 execution remains a separate gate.
