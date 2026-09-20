# Qualified main revision and explicit consumer adoption

Exact main merge: `78b3de389a189c7f86f611787e0489fb6d474218`, tree-identical to reviewed PR #18 head `b9c9390732c2b586d55b6089f190fda097cec4d2`.
[Main push CI](https://github.com/RedSalamanders/DxUi/actions/runs/35531706045) passes all six native x64/ARM64 Debug, Release and ASan Debug jobs, eighteen suites per profile, external-consumer integration and gallery generation. Receipts verify native architecture, exit status and deliberate ASan detection. Capability skips remain explicit in `manifest.txt` and each raw receipt.

The initial x64 ASan run timed out during `ElementFromHandle`, before event subscription; [its failure and ten local diagnostic passes remain retained](../main-78b3-first-asan/README.md). One failed-job retry passed unchanged. This does not establish a root-cause fix for that intermittent initialization timeout. The other five profiles passed in attempt 1; their successful results were carried into attempt 2.

RedSalamander's standard `Tools/Update-DxUi.ps1` accepted this exact successful main and changed its lock from `f5c7fc7403e352425b8e82b7a4da1c042d942ede` to this revision. Its fresh Full validation is running separately; the lock change alone is not a product-validation pass. Full raw CI downloads: `C:/RedSalamander.Perf/evidence/i26-ui/main-78b3-final-artifacts`. Library receipts do not replace consumer French/mixed-DPI, keyboard/UIA, resource or real assistive-technology acceptance.

## Consumer migration notes

- Continue linking the single pinned `DxUi.lib`; `apiRevision` remains 2. No consumer source enumeration, plugin ABI or second implementation copy is introduced.
- `ArrangeMeasuredActions` arranges caller-measured action sizes into complete rows in caller-provided bounded storage. Consumers own localization, available width, padding, policy order and cache invalidation; retain its returned bounds for painting, input and UIA.
- `Button::SetMultiline(true)` opts into wrapped captions; the default remains false. Checkbox honors that inherited setting, with its indicator/padding included in consumer text measurement. `SetChecked` is acknowledged model state, not another user action.
- Disclosure acknowledgement supplies ExpandCollapse state and native notifications; consumers retain responsibility for content visibility, focus recovery and settings persistence.
- I26 still owns integrated French long-text/native mixed-DPI, keyboard/UIA and real assistive-technology acceptance, paired application performance/resource evidence, final Issues/decision presentation and its own Done transition. H4/I18/I25 and other consumers are not closed by this handoff.

The successful x64 ASan retry completed native UIA subscription in 125 ms (`Accessibility-retry.log`); the original pending-at-3000-ms failure remains linked above.
