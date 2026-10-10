# Review and integration of the remaining branches

The user requested review and merge of useful, correct branch work on 10 October 2026.
The base is `bb1abcf84a170a1330a47b681a124d8d38d6fa8d`, after production-review PR #70.

- [x] Inventory the two remote feature branches, draft PR #69 and registered worktrees. Preserve the dirty detached baseline checkout.
- [x] Retain `codex/p1-prepared-tree-accessibility`: it predates the current model guards and publication design; its plan still requires current-source paired and consumer qualification. Do not merge its older transport or resurrect renamed tests.
- [x] Integrate #69 with the current native lazy-publication and snapshot-build guards, canonical provider identities and existing model binding revision.
- [x] Verify all eight focused native GridPattern cases in x64 Debug, including a same-model notification during cell capture. The initial integrated Debug run passed Embedded, Grid, Rendering and WindowHost; the historical nested-publication test failed because the current contract retries to the latest state. Preserve that failure and use current-source CI for full qualification.
- [ ] Run the affected local checks, six-profile native CI and the current paired comparison. Preserve any failure or inconclusive performance result; the separate Q22 policy qualification remains open.
- [x] Reproduce the cached foreign-query/model-assignment race with a deterministic peer-creation gate: the pre-fix query aliases a new assignment at the same model address. Bind the cell to the assignment that supplied its row lookup. All nine focused cases and the complete nonactivating x64 Debug Accessibility suite pass after the correction; renewed exact-source CI qualifies it separately from head `910fd82`. Local validators and formatting pass again.
- [x] Regenerate the gallery through the nonactivating harness. All five theme sheets and the embedded sample are byte-identical to current main; only generation provenance changes. Update the owning UIA contract for current revision guards and lazy publication. `validate.ps1` and `format.ps1 -Check` pass locally.
- [ ] Merge the reviewed implementation after inspecting the exact-source results and preserving remaining qualification limits.

Owning contracts: [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[window hosting](../../Rendering/Rendering_Win32Host.md), [performance](../../Core/Core_PerformanceAndResources.md)
and [validation](../../Testing/Testing_Validation.md).
