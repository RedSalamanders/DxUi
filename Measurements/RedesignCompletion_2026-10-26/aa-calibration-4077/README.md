# DxUi current-source A/A calibration — 26 October 2026

This archive records the completed same-binary calibration on DxUi commit `4077f57df8207eac6c39c43437feb45b19d23cf8`, tree `18be66885916f67cafd8aeb761207bf15fbd9120`, x64 Release, seed `20261027`, 12 randomized blocks and three scenarios. Baseline and candidate used the same physical source tree and executable identity.

## Results

Both the provenance audit and independent numerical replay completed successfully. Both full judges matched the consumed set and agreed with one another in all three scenarios. The numerical replay covers 144 receipts and 78 rows. The complete set has no reported regressions: none of the 54 statistical rows regressed. Separately, all 24 deterministic exact-budget rows met both median and peak limits.

The study recorded 72/72 unstable controls, 51 of 78 scenario/phase/metric slots with at least one drift, and 631 of 1,872 control observations beyond their bands. These controls do not establish a usable noise floor; they limit interpretation of the no-regression result. The overall policy remains `policy-review-required`, `qualified=false`. This same-source calibration is not a pull request, product, or CI performance acceptance result.

The host observer is contextual only. It does not establish a quiet host or scheduler-delay qualification. No dedicated-host, hardware, consumer, ARM64, or new performance-measurement qualification is claimed.

## Files and digests

- `calibration-summary.json` records source identity, judge agreement, policy state, control counts, scenario outcomes, and auditor/output digests.
- `metric-rows.json` contains all 78 independently reconstructed numerical rows.
- `.gitattributes` disables text conversion for packet files. `manifest.json` binds the README, summary, rows and attributes file by SHA-256.

The summary digest is `FA4AD8238DA1C806C5785B6285B1153922E6AAEF76B8DEAA258D3DED7DAE3C63`. The executing provenance auditor source digest is `0AB6AB3A1C3CE19A39637D093A80DC8888690CBE2C33D1688F485D9517CAB170`; the numerical auditor source digest is `738BF63555821A1C18A995BC7A024DD168180FCF7AAADCF37D16D92BD8E39725`. The exact provenance and numerical result-file digests are in `calibration-summary.json`.

## Audit history and limits

The provenance auditor had earlier tooling failures, which were preserved. The final run follows a source-diff review of the dictionary-fix change and a self-test addition; the reviewed delta changes JSON value projection and adds one sum fixture, without changing data, judge, policy, or qualification rules. The numerical auditor independently replays the same retained receipts. These calculations do not create independent measurements. The archived audit outputs do not promote policy or close unstable controls.