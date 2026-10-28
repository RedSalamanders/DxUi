# Qualification runner reliability — 2026-10-28

**Current producer: `0a2279c7ea658e4bb0cf185cb7a04a2da08299d3` (PR #128). Status: PARTIAL; full qualification remains open.** This packet binds current claims to the 0a source and keeps older source attempts distinct.

## Current producer: 0a2279c7

All six local build profiles pass and are bound to 0a. ARM64 results are cross-builds only. Six focused x64 Debug Menu tests also pass with zero skips, using the x64 Debug build identity recorded in the machine-readable receipt. These focused cases do not replace the full runtime matrix.

The ordinary x64 suite is partial at 19/57 executions: all 19 completed x64 Debug executions passed with zero skips; Release and ASan remain pending. The twelve-suite cross-profile foreground obligation is pending. Windows/Linux tooling and formatting checks pass in the latest hosted snapshot, while all six native profiles remain in progress. Final static/tooling rerun and the final hosted gate remain pending. The earlier full static attempt on 0a failed when the timeout fixture tried to read a missing progress path after a separate path-existence check. `TestSupport` removed the fixture while unwinding, so the initiating cause is unassigned. A revised isolated tooling fixture now stages its body-readiness marker and passed its focused 8/8 suite, but that result is not a full static/CI result for 0a. The source-bound statuses and receipt hashes are in [`qualification.json.template`](qualification.json.template).

PR [#128](https://github.com/RedSalamanders/DxUi/pull/128) is the current producer candidate. Its captured formatting run passed; the final validation/native gate is not recorded as complete. No performance acceptance is claimed.

## Historical source attempts

### Production source 6e90427b

The 6e checkpoint passed six local builds and 57 ordinary x64 suite runs. Its authorized foreground matrix passed 11 of 12 runs; ASan Debug Menu failed `TestMenuHoveringSiblingWithChildrenReplacesOpenSubmenuAfterDelay`. Completed leases verified owner, cursor and focus restoration. Test-only source 594 reproduced the pending-submenu timer failure against unchanged 6e production code; it is a controlled red reproduction, not a production qualification failure. Hosted run [40717449947](https://github.com/RedSalamanders/DxUi/actions/runs/40717449947) passed all six native profiles for 6e only.

### Candidate c155de84 / PR #126

The c155 candidate passed six local builds, 57 ordinary x64 suite runs and all twelve authorized foreground runs, including actual ASan execution and verified restoration. These results remain source-bound to c155.

Hosted run [40757949246](https://github.com/RedSalamanders/DxUi/actions/runs/40757949246) built all six profiles and passed all six public-consumer checks. Full native qualification had four of six profiles pass; ARM64 Release failed `TestMenuKeyboardRightArrowMatchesWindowsMenuLoop`, and ARM64 ASan Debug failed `TestStationaryMouseDoesNotOverrideKeyboardRootSwitch`. Across the retained hosted report set, 126 report rows comprise 124 pass, two fail and zero skips. The native failures remain failures; consumer/build passes do not turn the overall attempt green.

The paired artifact contains 144 raw receipts. Independent replay matches all 78 metric rows, with zero resolved primary regressions. Same-side controls are unstable in 71 of 72 comparisons, so the result remains inconclusive and requires policy review. A source-backed timing audit found that all 57,600 preparation samples fall on the 100 ns QPC grid; this limits interpretation of the tiny clean-Prepare values only. It does not explain the other unstable controls or establish host contention. Do not loosen bands, rebaseline, pool cohorts or claim performance acceptance.

### Candidate d4a36c09 / PR #126 history

The d4 attempt passed six local builds, 57 ordinary x64 suites and six focused Menu cases. A separate x64 Debug foreground Menu run failed `TestMenuKeyboardLeftArrowMatchesWindowsMenuLoop`; its cause remains unassigned. Hosted run [40737741687](https://github.com/RedSalamanders/DxUi/actions/runs/40737741687) passed five native profiles and failed ARM64 Debug on an Info-row result. Its paired job was cancelled by a newer push; artifact 12846207518 was not downloaded or replayed. Its CI gate failed. These d4 outcomes are historical and are not carried forward to 0a.

### Test-only and compile attempts

- Test-only source 594 reproduced the pending submenu timer failure while production remained at 6e.
- Test-only source 921, based on d4, failed the ordered Home-cancellation assertion because the pending timer remained active; this does not establish a production failure on 0a.
- Source 762 retained a compile-only C2039 failure because test code referenced nonexistent `ContextMenuPopupDebugState::itemCount`; it has no runtime result.

## Open gates

Complete 0a's 57 ordinary x64 runs and twelve-suite foreground matrix, rerun final tooling against the integrated timeout-fixture correction, and record the final PR/hosted result. Keep the existing WIP plans open until their own source-bound gates pass. Shared performance policy/control review and consumer/platform gates remain separate.

The machine-readable packet contains sanitized source identities, statuses and hashes. Raw logs, binaries, process identifiers, desktop identity and private output locations remain outside this packet.
