# Merged-main qualification at `e0260dde` (13 October 2026)

**Disposition: partial qualification; overall acceptance remains HOLD.** This is the public summary of
the merge of [PR #76](https://github.com/RedSalamanders/DxUi/pull/76), exact source
`e0260dde04695f0794eed79708030798b7575e0c`. The original local receipts and their audit are
retained in the qualification workspace. Their archive is omitted here because it contains local
machine, workspace-path and foreground-session metadata. Its SHA-256 is
`4301D38FFD387B5B7A8562A88892D610DCC62621FD7EA7F09514BE25FAD7E85C`.
That hash identifies the local record if it is reviewed privately; the summary alone is not an
independent replay of the local diagnostic or resource suites.

## Raw evidence preservation (21 October 2026)

The later untracked raw downloads have been reconciled with this summary-only publication decision. The
[evidence inventory](evidence-inventory.csv) records each original relative path, byte length and SHA-256 for
395 raw files (22,525,454 bytes): 288 CI files, the CI status record, 24 local-success files, eight original
timeout files and 74 pilot files. All were copied to private archive `dxui-main-e0260dde-raw-20261021`, and both
source and archive hashes were checked before removing the redundant source copies and 25 empty directories.
The archive additionally retains the original README, for 396 original files and 22,529,203 bytes in total.
The CSV's SHA-256 is `F7D63433C33D5FA190557EE5232F0B199790A55C7665082C635378B50F464AB2`.

CI suite/status/child receipts and local lease records do not implement the standalone benchmark receipt schema.
No workload-owner fields, fixture hashes, outcomes or historical paths were invented or rewritten to make them
validate. Private raw reports retain their original names and bytes, including the diagnostic pilot's source and
fixture metadata, logs, captures and failed confirmation. Fifteen former repository paths exceeded the consumer
restore limit by up to 30 characters; they are no longer checkout inputs. The public inventory supports archive
integrity checking; it does not independently reproduce or qualify the results. The earlier archive hash above
identifies a separate historical record and has not been reverified by this preservation operation.

All functional, foreground-restoration, performance, platform and consumer qualification limits below remain.

## Native CI at the merged source

[Main run 38428878942](https://github.com/RedSalamanders/DxUi/actions/runs/38428878942) completed
successfully: validation, tooling, format, all six x64/ARM64 Debug, Release and ASan Debug native jobs,
and `ci-gate`. The downloaded CI artifacts were independently checked at this source: 128 native
suite receipts exited zero with no capability skips; Menu completed 108/108 and NativeTextInput
165/165 cases in every profile, including the three native Arm64 jobs. Their hosted foreground
leases completed with restoration, applicable consumer checks passed, and x64/ARM64 ASan detection
probes succeeded. Performance/build receipts reported clean source and verifiable v2 identity.
The main-push paired benchmark was **skipped** by the workflow, so this green aggregate does not
establish paired performance acceptance. Public CI artifacts are the reviewable source for this
part of the record.

## Local x64 resource and profiling work

At the same clean source, the user approved the visible 30-second automatic-Start desktop lease.
MenuResources and MenuResourceScaling passed in x64 Debug, Release and ASan Debug: six suite executions,
zero capability skips. Each lease recorded `started` confirmation, completed children and successful
foreground/focus/pointer restoration or unchanged state. An earlier ordinary Release Start
confirmation timed out before any child launched; its unchanged desktop state was retained
separately. These results close the **local x64 opt-in resource execution** slice only.

A fixed local x64 Release/WARP profiling pilot produced 12 fresh reports with exact source/build
identity and matching before/after checks. Report 6 (Default, heap on) had a 699.49 ms clean probe
(57.19 completed offscreen FPS), versus 19.66 and 18.78 ms in the other Default heap-on reports.
Its dirty probe was 380.61 ms versus 71.11 and 71.13 ms. The excursion has no established cause.
The pilot is a same-candidate diagnostic, not a paired regression test.

The previous full same-binary A/A study had 72/72 unstable controls. PR #76's paired candidate job
failed the required trusted-base policy with 69/72 unstable controls. Neither the local profiling
pilot nor the skipped main-push paired job establishes accepted non-regression or a trusted policy.

## Remaining acceptance

- Run the opt-in MenuResources and MenuResourceScaling fixtures natively on ARM64. CI Full covered
  Menu and NativeTextInput, but not these two resource fixtures.
- Attribute or bound the timing/memory excursions and qualify stable same-binary controls and a
  trusted paired performance policy.
- Complete physical ARM64/device-removal, touch, real IME, screen-reader and consumer product/pin
  qualification. No physical devices were available for this local session.

The active [qualification plan](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md)
tracks these gates. This evidence-only record changes no UI visuals, so gallery regeneration is
not required.
