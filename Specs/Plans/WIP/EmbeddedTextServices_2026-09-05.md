# Embedded text-service adoption

Status: ACTIVE
Owners: `Specs/UI/UI_InputAndAccessibility.md`, `Specs/Rendering/Rendering_EmbeddedD3D11.md`

Continue on `codex/av-text-bridge` in the isolated AV worktree; PR #3 owns the preceding `codex/av-input-services` component. Preserve the concurrent independent-sample
checkout. The retained original baseline used complex-ui-v1; the integrated component now has matched complex-ui-v2 evidence.
Retained pre-change compiled-source receipts are the contrast candidate Debug and affinity-controlled Release
reports retained at commit 1947a5b and in RedXe docs/measurements/primary-high-contrast-2026-09-05; do not replace those originals.

- [x] Public revision-checked embedded text snapshot, composition preview/commit/cancel and safe lifetime tests.
- [ ] Shared application-side OS services with no additional renderer; host-owned focus/TSF/IME/clipboard.
- [ ] Generic bounded RedXe transport and accessibility attachment; no DxUi objects or HWND across its plugin ABI.
- [ ] Debug/Release suites, ARM64 builds/native CI, matched performance comparisons and documentation review.
- [ ] Real IME and assistive-technology consumer acceptance. Synthetic text edits do not establish this gate.

Integration now includes the other task's exact independent-samples patch (SHA-256
6099a66e236e59f876b9e3107390034313ea8ba666ea7da222aaefba70f6b957), preserving new sample models/tests
and canonical DxUi spelling. A separate unchanged-library worktree applies this same v2 harness to commit 1947a5b.
The initial v1 Debug comparison exceeded noise bands; its raw receipt is retained and performance acceptance
remains open. All inherited Debug suites passed; the new embedded suite passed after correcting a test fixture
that dereferenced a field after Detach destroyed its tree. Further undo/history tests are added before acceptance.

An embedded text snapshot is only the retained-tree half of the bridge. Keep capabilities truthful until the
application-side service and consumer transport are implemented and validated.

Current component validation: all 18 suites passed in x64 Debug and Release, with six interactive Menu capability
skips per configuration and 1,569 embedded checks each. Both ARM64 configurations cross-built without warnings.
Both full-suite performance comparisons passed against retained matching final-driver baselines; all earlier
failed/mixed measurements remain in [the evidence archive](../../../Measurements/TextInput/2026-09-05/README.md).
Undo/redo, stale revisions, wrong-thread calls, callback-driven root destruction, Escape, external edits and
attachment/device/focus cancellation are covered. All six regenerated gallery sheets were inspected for affected
text/primary-control/sample behavior. Native ARM64 CI and exact-pin external consumer checks follow the component
commit; Windows text/accessibility transport and real consumer acceptance remain open.

The input component's relocated Release consumer passed at ad88833, including both rendered examples and five
invalid-pin rejections. Local validators/format and 34 tooling tests passed. Native CI passed x64 Debug and both
ARM64 configurations, while x64 Release failed the existing menu owner-message-flood hover assertion.

A subsequent native text-store fix protects callback-driven destruction, changed focus/text, thrown exceptions and
ComboBox callback argument lifetime. All 18 local x64 suites passed in each configuration (nine Menu capability
skips each); ARM64 cross-builds passed. Its performance gate is still open; all raw comparisons, including failures,
are retained in [the native-store archive](../../../Measurements/TextInput/NativeStore-2026-09-05/README.md).
The menu loop now has a candidate fix prioritizing its own pending paints after input, with unchanged flood-test
deadlines and bounded failure routing logs. Validate this separately before adoption. Full application-side OS
services, clipboard, UIA, generic consumer integration and real acceptance remain unchecked.
