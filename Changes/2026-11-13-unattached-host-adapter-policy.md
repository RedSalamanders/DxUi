- An unattached `ControlHost` that only measures text (`GetWriteFactory`, `GetTextFormat`) now borrows the
  process-shared DirectWrite factory instead of creating the owner thread's D3D/D2D graphics pool. That pool was
  never released (only the last attachment's detach releases it), so it leaked for the thread's life and its default
  adapter policy refused every later `Attach(hwnd, {.forceWarp = true})` on the thread with `ERROR_INVALID_STATE`.
  - Validation: `TestUnattachedHostTextMeasurementLeavesAdapterPolicyFree` in the WindowHost suite (unattached
    measurement before and after forced-WARP and default attachments); full Control and Tooltip suites in suite order.
