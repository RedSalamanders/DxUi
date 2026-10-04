# Interactive Menu validation

The cursor regression passed in a filtered interactive x64 ASan run, but the full Menu suite failed subscribing to UIA focus changes. The PowerShell lease wrapper also mixed executable output into its result object.

- [x] Reproduce both failures and retain real-desktop logs.
- [x] Preserve lease diagnostics without contaminating its returned result; test passing and failing child results.
- [ ] Diagnose and repair UIA client setup without skipping accessibility assertions or silently extending deadlines.
- [ ] Pass the full interactive Menu suite and the required x64 tests and ARM64 builds.
- [ ] Update the validation contract, changelog and PR, then archive this plan.

This changes test infrastructure only. Public controls, gallery pixels and the design system require no regeneration. No consumer pin changes. ARM64 cross-builds establish compilation only.

Local evidence: the cursor-only interactive ASan run passed with no skips. The full Menu run timed out in `AddFocusChangedEventHandler` (`ready=false`, `E_PENDING`) after 20 seconds. Afterward independent runs failed to create windows, initialize COM, start PowerShell threads and discover Visual Studio (`0x80070583`, `0x80070008`, `0x8007000e`). Releasing this session's idle MSBuild workers did not recover it. Do not classify these infrastructure failures as passing validation or as proof that bounded UIA requests repair the observed stall. The complete candidate needs fresh local interactive validation and the hosted matrix.
