# Qualification runner reliability — 2026-10-28

**Current qualification remains pending.** The active recovery candidate is on `ej/consumer-progress-recovery-20261029`, based on `9d974c0efbc24ea3168afaf7d0953f2000b797e2`; its two executable-source changes still require final qualification. The previous 9d local static attempt failed validation, and an 8-case x64 Debug Menu smoke passed with focus/restoration. Hosted run [40791275031](https://github.com/RedSalamanders/DxUi/actions/runs/40791275031) is bound to the unchanged 9d source: validation, Windows/Linux tooling, formatting, native-scope and x64 Debug/Release native jobs passed; x64 ASan, all ARM64 native profiles and the paired benchmark remain in progress. The recovery candidate inherits none of those results. The reviewed CI contract already includes the unchanged `Tools/TestRunEvidence.psm1` identity helper; its digest was recomputed canonically for the changed journal module and test.

The exact candidate module/test copy passed its focused 12/12 publisher suite, including a red/green deadline control against retained pre-guard code. This is development evidence for those copied files, not a full static result or repository qualification. Current changed source hashes are recorded in `qualification.json.template`. The current candidate still requires its own local build/runtime/foreground/static and hosted gates. Shared performance-policy/control, consumer-adoption and ARM64 runtime gates remain separate.

## Preserved producer: 0a2279c7 (historical)

All six local build profiles pass and are bound to 0a. ARM64 results are cross-builds only. Six focused x64 Debug Menu tests also pass with zero skips, using the x64 Debug build identity recorded in the machine-readable receipt. These focused cases do not replace the full runtime matrix.

The ordinary x64 suite completed 57/57 executions with zero skips across x64 Debug, Release, and ASan Debug; the ASan result is actual runtime evidence, not a build-only claim. Six local build profiles pass and six focused x64 Debug Menu cases pass. The twelve-suite cross-profile foreground obligation is NOT RUN; do not infer foreground coverage from ordinary suites. Hosted run [40777143980](https://github.com/RedSalamanders/DxUi/actions/runs/40777143980) has six failed Full native qualifications; all six builds and relocated public consumers passed. Verified x64 artifacts and the ARM64 ASan log identify the stationary-pointer initial-A setup assertion. The other two ARM64 failure causes have not been mapped. Windows/Linux tooling and formatting checks pass. The final qualification gate remains open. The earlier full static attempt on 0a failed when the timeout fixture tried to read a missing progress path after a separate path-existence check. `TestSupport` removed the fixture while unwinding, so the initiating cause is unassigned. A revised isolated tooling fixture now stages its body-readiness marker and passed its focused 8/8 suite, but that result is not a full static/CI result for 0a. The merged e343 documentation checkout received one full static attempt: formatting passed; validation failed on the public auxiliary observer JSON and a stale CI contract digest. `Test-Docs.ps1` passed 25/25. The digest was refreshed after confirming only the reviewed `Tools/tests/Test-ConsumerQualification.ps1` input changed since 0a; focused `Test-ScopedTesting.ps1` then passed 37/37. The focused spec validator still reports the observer JSON because the 144 source reports cover three fixture identities and the aggregate has no truthful singular fixture; `Test-Changelog.ps1` passed. The full static sequence has not been rerun. The source-bound statuses and receipt hashes are in [`qualification.json.template`](qualification.json.template).

PR [#128](https://github.com/RedSalamanders/DxUi/pull/128) retains the failed 0a producer. The active recovery branch carries the A-before-B setup and reviewed journal fixture/retry correction. Its focused copied publisher suite passed, but the final source has not completed validation or native qualification. The latest hosted run above only covers the unchanged 9d base. No performance acceptance is claimed.

## Historical source attempts

### Production source 6e90427b

The 6e checkpoint passed six local builds and 57 ordinary x64 suite runs. Its authorized foreground matrix passed 11 of 12 runs; ASan Debug Menu failed `TestMenuHoveringSiblingWithChildrenReplacesOpenSubmenuAfterDelay`. Completed leases verified owner, cursor and focus restoration. Test-only source 594 reproduced the pending-submenu timer failure against unchanged 6e production code; it is a controlled red reproduction, not a production qualification failure. Hosted run [40717449947](https://github.com/RedSalamanders/DxUi/actions/runs/40717449947) passed all six native profiles for 6e only.

### Candidate c155de84 / PR #126

The c155 candidate passed six local builds, 57 ordinary x64 suite runs and all twelve authorized foreground runs, including actual ASan execution and verified restoration. These results remain source-bound to c155.

Hosted run [40757949246](https://github.com/RedSalamanders/DxUi/actions/runs/40757949246) built all six profiles and passed all six public-consumer checks. Full native qualification had four of six profiles pass; ARM64 Release failed `TestMenuKeyboardRightArrowMatchesWindowsMenuLoop`, and ARM64 ASan Debug failed `TestStationaryMouseDoesNotOverrideKeyboardRootSwitch`. Across the retained hosted report set, 126 report rows comprise 124 pass, two fail and zero skips. The native failures remain failures; consumer/build passes do not turn the overall attempt green.

The paired artifact contains 144 raw receipts. Independent replay matches all 78 metric rows, with zero resolved primary regressions. Same-side controls are unstable in 71 of 72 comparisons, so the result remains inconclusive and requires policy review. A source-backed timing audit found that all 57,600 preparation samples fall on the 100 ns QPC grid; this limits interpretation of the tiny clean-Prepare values only. It does not explain the other unstable controls or establish host contention. Do not loosen bands, rebaseline, pool cohorts or claim performance acceptance.

### Historical PR #124 observer follow-up

The [compact observer report](historical-124-observer.md) summarizes a read-only host-observation replay. The full auxiliary JSON is preserved in the superseded PR #129 recovery record and in private archival copies; SHA-256 `45BD0BB55B5914D3FE8278E9BA75DF8E3FB8D1F4DB8023161807FF48EF84A1D2` (126,811 bytes). This selective handoff does not include that auxiliary JSON. The raw report did not define a quiet-host threshold; this follow-up does not attribute control instability to broad host load or establish performance acceptance.

### Candidate d4a36c09 / PR #126 history

The d4 attempt passed six local builds, 57 ordinary x64 suites and six focused Menu cases. A separate x64 Debug foreground Menu run failed `TestMenuKeyboardLeftArrowMatchesWindowsMenuLoop`; its cause remains unassigned. Hosted run [40737741687](https://github.com/RedSalamanders/DxUi/actions/runs/40737741687) passed five native profiles and failed ARM64 Debug on an Info-row result. Its paired job was cancelled by a newer push; artifact 12846207518 was not downloaded or replayed. Its CI gate failed. These d4 outcomes are historical and are not carried forward to 0a.

### Test-only and compile attempts

- Test-only source 594 reproduced the pending submenu timer failure while production remained at 6e.
- Test-only source 921, based on d4, failed the ordered Home-cancellation assertion because the pending timer remained active; this does not establish a production failure on 0a.
- Source 762 retained a compile-only C2039 failure because test code referenced nonexistent `ContextMenuPopupDebugState::itemCount`; it has no runtime result.

## Open gates

The 57 ordinary x64 executions are complete. Run the twelve-suite foreground matrix when its initial-A setup prerequisite is resolved; do not mark it passed from ordinary suite evidence. Rerun final tooling against the integrated timeout-fixture correction and record the final PR/hosted result. Keep the existing WIP plans open until their own source-bound gates pass. Shared performance policy/control review and consumer/platform gates remain separate.

The machine-readable packet contains sanitized source identities, statuses and hashes. Raw logs, binaries, process identifiers, desktop identity and private output locations remain outside this packet.
