# Hosted DxUi native qualification — 2fed824 PR workflow

This packet records hosted workflow run 40222621151, reviewing commit `2fed82465dcee9f213753754e0e8e54f353cf38f` at tree `2c0641bc363bf713cfc709d721c3f6549dd5aef8`. The PR run compiled synthetic merge commit `9ac8cc0bb857a0a71f0ea0e94ef3a51c69be7371`; the manual run compiled the exact reviewed commit. Both compiled trees equal reviewed tree `2c0641bc363bf713cfc709d721c3f6549dd5aef8`, which is also the tree of merged main commit `97b26bad7fcdb0aa3dbb2efd4bceac77aab5a295`.

The six native jobs passed, but the PR workflow and paired benchmark were still in progress at the audited capture. Do not read this as a completed workflow result.

## Native profiles

All six profiles passed: x64 and ARM64 Release, Debug, and ASan Debug. Each primary profile reported 21 passing suite reports, zero failed reports, and zero skips. The x64 Release profile also includes a separate two-suite resource-scaling run. Archive SHA-256 values, archive sizes, inventory counts, and per-profile result totals are in `summary.md`; the captured provider digests matched all six downloaded archives.

The ASan canary deliberately triggered and was detected in both architectures. That expected nonzero canary outcome is separate from the 21 passing ASan Debug suites. ARM64 here is hosted native execution, unlike the local matrix's ARM64 cross-builds.

All recorded external-consumer checks passed (10 negative checks and 12 native-module checks per consumer), and all six gallery runs passed. Consumer checks cover the recorded compatibility/module probes only; they do not establish every downstream product behavior.

## Limits

The hosted ARM64 Debug Menu pass does not explain the earlier 4faf16ed ARM64 Debug Menu failure; its cause remains open. This packet reports native qualification evidence only. It does not establish paired performance acceptance, a quiet host, or downstream RedTilde acceptance. The separate CD35 paired study remains inconclusive under an unqualified base policy. The current PR102 paired benchmark is pending and must not be combined with that older study.
