# Acrylic menu visual-capture pointer fixture

## Checklist

- [x] Preserve the failed ARM64 Release provider archive and distinguish observation from cause.
- [x] Review the existing pointer/DPI helper and separate capture, geometry, hover and clear predicates.
- [x] Pass focused Release light/dark checks with automatic Start and verified desktop restoration.
- [x] Pass the complete Menu suite in local x64 Release and hosted x64 Release, Debug and ASan Debug.
- [x] Pass the scoped validation obligations and all six hosted build/test profiles.
- [x] Verify the repaired fixture on hosted ARM64 and retain the original failed evidence.
- [x] Publish evidence, synchronize the normative test contract and changelog, and merge PR #104.
- [x] Move this completed fixture plan to Done; parent qualification holds remain open.

## Status

COMPLETE for this fixture on 2026-10-26. This plan addresses one test-fixture failure in the Acrylic light visual baseline and adds dark-theme
coverage for the same physical pointer transition. It does not modify production menu behavior, the baseline
image, interaction policy, timeouts, or qualification thresholds.

## Evidence and uncertainty

The 25 October SDK CI run passed five native profiles and failed the ARM64 Release Menu suite at
`TestMenuPopupAcrylicLightVisualBaseline` with `menu popup exposes unhovered acrylic geometry for visual-baseline
capture`. The old predicate combined positive visible dimensions and absent hover, so the provider report does not
show which part failed. Its pointer fields record only a later exit position. This supports investigating fixture
pointer state; it does not prove the cause or establish a production defect. Preserve that failed provider archive.

`ContextMenu::Show` intentionally samples the physical cursor when a popup opens. The test already uses a driver
thread and debug probe, but previously neither aligned the physical cursor nor separated modal capture, geometry,
and hover readiness before bitmap capture.

## Fixture change

- Wait independently for popup capture and positive visible dimensions.
- Under the popup's DPI-awareness context, align the physical cursor to the first row and deliver the matching
  move; require that row's hover state.
- Move the physical cursor to the popup's shadow margin, deliver the matching move, and require hover to clear
  before installing the deterministic backdrop or capturing pixels.
- Restore the cursor only if it remains at the fixture's last aligned point. Failure output records the readiness
  predicates, popup DPI, hover index, physical cursor position, and bounded delivered-move count/last target.
- Add the same row-hover then margin-clear assertion for dark Acrylic. The light visual baseline remains the only
  image comparison.

## Validation and stop conditions

Root reviews the source and docs before native execution. Then run the existing interactive Menu validation in
the authorized profiles, keeping the original failed archive and all new receipts. Do not lengthen the timeout,
remove the unhovered assertion, or infer cause from a passing rerun. If geometry, capture, row hover, or margin
clear fails, retain the separate predicate diagnostics and stop for review. The broader six-profile gate and
current qualification status remain owned by the root task.

## Completion evidence

PR [#104](https://github.com/RedSalamanders/DxUi/pull/104) merged as `619dc88b776f6e545542e0242e5943bbdddc5b3c`.
Its tree equals tested head `30fccffe13b2b07a5369f384943a35c23d836761`. The
[publication packet](../../../Measurements/MenuPointerFixture_2026-10-26/README.md) binds all six provider archive
digests. Independent lead replay checks 126 zero-skip scope reports and 738 individual Menu case completions
(123 per profile; the suite banner is not a case), including Light and Dark in every profile. Both ASan probes
detect the intentional fault. Local Release Full and Menu evidence remains separately bound to 95 and 8 payload files.

The original combined-predicate failure and first local build-admission failure remain unexplained. Hosted
ARM64 Menu leases report focus `none` with `foreground-minimized`; no initial keyboard focus was available to
verify restoration, so that parent gate stays open. Paired benchmark collection/verdict steps were skipped.
This completion supplies no shared performance acceptance or RedTilde ARM64 runtime qualification. The durable
pointer/readiness/restoration contract is in [Testing Validation](../../Testing/Testing_Validation.md).
