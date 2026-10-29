# DxUi qualification reliability — 2026-10-29

## Current functional qualification

The current producer is F, commit [`dbb6f1bea84db775c95cc392b4d7214253ecfb30`](https://github.com/RedSalamanders/DxUi/commit/dbb6f1bea84db775c95cc392b4d7214253ecfb30), branch `ej/consumer-progress-recovery-20261029`, PR [#131](https://github.com/RedSalamanders/DxUi/pull/131). The tested CI merge is `f69240dd77c5c9f84d6256c783893443979a46b1`, tree `6f599ac333a305413520b4560a8bd7a4b6da405f`.

- All six x64/ARM64 Debug, Release and ASan Debug build profiles passed. Local ARM64 build profiles are cross-build evidence; hosted native receipts separately passed all six runtime profiles.
- Final static qualification passed `format.ps1 -Check`, `validate.ps1`, `Test-Changes.ps1 -Explain`, `Test-PrePush.ps1 -Explain` and the changelog check.
- Local x64 Debug, Release and ASan Debug completed 57/57 ordinary suite runs with zero skips. The actual ASan detector probe detected the expected use-after-free. Twelve foreground/resource suite outcomes passed across three leases; local foreground, focus and cursor states were unchanged.
- Hosted PR #131 run [40798600608](https://github.com/RedSalamanders/DxUi/actions/runs/40798600608) passed all six native profiles and format, native-scope, Windows-tooling and validation jobs. Hosted lease receipts report foreground and cursor restored, focus `none`, and `runMovedSomething=1`; they do not establish exact focus restoration. This differs from the exact unchanged-state local restoration receipts.
- The overall hosted run failed at `paired-benchmark (pull request)`, step `Publish-BenchmarkVerdict.ps1`; the CI gate also failed. Hosted native success does not make the overall run green.

## Publisher behavior qualified on F

The consumer progress journal keeps one owner. Initial publication does not overwrite an existing target. Updates use `File.Replace` with the explicit CLR-null backup argument. Retry is limited to Win32 sharing violation 32 or ReplaceFile error 1175, at most three replace calls, two 25 ms waits and a 250 ms retry-admission window. The deadline is checked again after file identity, length and SHA-256 verification immediately before a retry. A retry requires the existing destination and pending file to retain their verified identity and content. Unknown errors, changed or unreadable state, and persistent locks fail without rerunning the phase body. The external-timeout child publishes a closed readiness marker before parent reads; a still-live child keeps its fixture for diagnosis. These rules are covered by the owner’s source, focused tooling tests and final static validation.

## Paired study and acceptance boundary

Artifact `12870366521` is 194,072,684 bytes, SHA-256 `4b0314392b19a8c061b7ecff04c11f99f81ebf1ee683b2ff24ed54ccee058321`. Its tested base is `2cf30d7e5ad092c77945cbe6d45bfb450f0e6762`, with F head `dbb6f1bea84db775c95cc392b4d7214253ecfb30`; tested merge and tree are recorded above. Independent replay matched 144/144 receipt files, 78/78 metric rows and 72/72 same-side control classifications. All 78 primary rows were within noise; all 72 same-side controls were unstable. The base measurement policy is unqualified, so the verdict is **inconclusive; policy review required**. Performance acceptance is not claimed. Do not rebaseline, pool cohorts or loosen the bands. The independent replay report SHA-256 is `cccdee93a5ea1f60188b52ce58282cb9253075e99f9da5a252aeefd55243323c`; the latest independent review manifest SHA-256 is `9A072EB08E8106B2F2D1B689B1775FD2B5CC12B10D8B59822DFAF2A2F372F610`.

## Preserved earlier outcomes

- Source 6e had an 11/12 foreground result; ASan Debug failed `TestMenuHoveringSiblingWithChildrenReplacesOpenSubmenuAfterDelay`. Test-only 594 reproduced the pending-timer assertion against unchanged 6e production source.
- Source d4 had a separate Debug Menu failure in `TestMenuKeyboardLeftArrowMatchesWindowsMenuLoop`. Hosted run 40737741687 passed five native profiles and failed ARM64 Debug on the Info-row result; its cause remains unassigned, paired job was cancelled, and the CI gate failed. Test-only 921 reproduced the Home-cancellation failure on d4. Source 762 is a compile-only C2039 reference to missing `ContextMenuPopupDebugState::itemCount`.
- Source c155 passed the local six-build, 57-suite and 12-foreground matrices. Hosted run 40757949246 failed ARM64 Release RightArrow and ARM64 ASan stationary-mouse tests. Its paired result matched 78 metric rows but had 71/72 unstable controls and remains inconclusive.
- Source 0a was a historical producer that passed all six local builds, 57/57 ordinary x64 suite executions with zero skips, and the actual ASan detector probe. Its 12-suite foreground matrix was not run. Its original static timeout fixture observed a missing progress path after a separate existence check; `TestSupport` removed the fixture while unwinding, and the initiating cause was unassigned. The later documentation checkout’s validation failure on an auxiliary observer JSON and stale digest is also retained as its own history.
- Predecessor 9d974c0 had a full-static failure in the external-timeout child at `File.Replace`; the caller did not preserve the numeric OS error, so the cause remains unassigned. Its eight focused Menu smoke cases passed. Hosted run 40791275031 passed six native profiles but failed at the paired benchmark and CI gate. These are not F outcomes.

The machine-readable packet retains the source-attempt rows and hashes. The immutable pre-closeout WIP wording is available at F commit `dbb6f1bea84db775c95cc392b4d7214253ecfb30`; the qualification plan closeouts below report only F evidence.

## Owner gates that remain open

Shared paired-performance policy/control qualification remains open. Consumer adoption and any product-specific physical, IME or assistive-technology acceptance remain separately owned. The four functional plans have completed their own F-source criteria; their Done records do not close these separate gates.
