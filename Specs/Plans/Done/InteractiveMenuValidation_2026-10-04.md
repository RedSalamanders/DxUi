# Interactive Menu validation

The cursor regression passed in a filtered interactive x64 ASan run, but the full Menu suite failed subscribing to UIA focus changes. The PowerShell lease wrapper also mixed executable output into its result object.

- [x] Reproduce both failures and retain real-desktop logs.
- [x] Preserve lease diagnostics without contaminating its returned result; test passing and failing child results.
- [x] Diagnose and repair UIA client setup without skipping accessibility assertions or silently extending deadlines.
- [x] Pass the full interactive Menu suite and the required x64 tests and ARM64 builds.
- [x] Update the validation contract, changelog and PR, then archive this plan.

This changes test infrastructure only. Public controls, gallery pixels and the design system require no regeneration. No consumer pin changes. ARM64 cross-builds establish compilation only.

## Evidence and closeout

The initial full Menu run timed out in `AddFocusChangedEventHandler` (`ready=false`, `E_PENDING`) after 20 seconds.
Independent runs subsequently failed to create windows, initialize COM, start PowerShell threads and discover Visual
Studio (`0x80070583`, `0x80070008`, `0x8007000e`). The untouched Debug executable reproduced the window-creation
failure. Those failures are retained as failures; the Windows allocation issue is not claimed to be fixed by DxUi.

Once the baseline window test recovered, the final candidate passed `test.ps1 -Configuration 'ASan Debug' -Platform
x64 -SkipBuild -Suites Menu -Interactive`: the full Menu suite ran for 100.2 seconds with exit 0 and no capability
skips. The lease verified foreground, focus and pointer restoration, and its structured result reached the
PowerShell reporter correctly. The lease reported that it restored a pointer the suite had left moved; that is
lease restoration evidence, not a claim that every fixture restored its own pointer. The cursor regression itself
reports exact pointer restoration.

[Hosted validation](https://github.com/RedSalamanders/DxUi/actions/runs/37218201708) passed on `9ebc5b1` in all six native
configurations, including consumer checks. The hosted x64 ASan Menu receipt has exit 0 and no skips; its described-row
focus-event test completes in 0.619 seconds, with successful UIA setup. ARM64 runs execute natively and record their
capability skips, including covered cursor windows; they are not full interactive desktop qualification. Formatting,
the five validators and tooling tests pass on the hosted run, including passing/failing lease output regression cases.

The contract and changelog are updated, and [PR #63](https://github.com/RedSalamanders/DxUi/pull/63) carries the fix.
The final plan archival changes documentation only; it does not change the native inputs validated above.
