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


Continuation validation at bcc65a10b2abd7f117ecc544788f904ab54087f2: native CI 33985347314 passed all
four runtime configurations and format CI 33985347310 passed. x64 Debug Menu recorded zero capability skips.
The prior fad2522 run retained its failed hover evidence; do not erase that intermittent failure history.

Subsequent uncommitted implementation separates native control adaptation from the shared COM text store,
adds application-side TSF/clipboard services, opaque focus identity, coalesced deferred locks and focus-guarded
cancellation. Targeted NativeTextInput tests pass; embedded input currently passes 1,574 checks with zero C++
allocations in 1,000 warm composites. The full matrix, exact-commit CI, matched performance and consumer connection
must be refreshed for this implementation before adoption. The native-store measurement gate remains open.
These changes alter input/services, not initial control pixels, layouts or themes; existing reviewed gallery assets
remain applicable. Review a live composition in the consumer in addition to the unchanged initial gallery.

The application-side component now includes shared TSF/clipboard services, focus identity, coalesced deferred
locks, bounded clipboard access, prepared/revisioned geometry and a public-only standalone client. The optional
profile-name sample rendered successfully with a private-clipboard Unicode paste and hidden TSF attachment;
the generated initial image was visually reviewed. Targeted Embedded passes 1,588 checks with zero C++ allocations
in 1,000 warm composites. Full x64 Debug and Release suites have passed after the final notification-lifetime fix,
with nine Menu capability skips each; ARM64 builds, exact-commit CI, external consumer and matched timing are being
refreshed. Keep the native-store performance gate open until the new declared comparison finishes. No real IME,
screen-reader, camera or audio-policy acceptance has been established.
