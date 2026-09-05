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


The application text component is committed as 2ca8cc03aae08f943d00abdfeddfe6f87f7800cd. Local relocated Release
consumption passed three rendered examples and five invalid pins. Native x64 Debug/Release CI passed; ARM64 is
pending. The declared ABBA comparison and same-source controls remain open in Measurements/TextInput/HostServices-2026-09-05.
No failing receipt is waived. The isolated RedXe integration passed 6,401 synthetic AV checks before toggle semantics.

The next library component adapts the existing UIA provider/pattern implementation to EmbeddedHost. The application
supplies process-unique runtime identity, physical-screen placement and a module-local parent/root/focus site;
no HWND, renderer, timer or worker is created. Providers use COM STA threading and reject stale control identities.
Hide/detach/device replacement disconnects them. The consumer keeps provider code mapped while references survive.
The initial implementation and its tests are not yet validated. New library-only UIA lifecycle/thread/geometry and
pattern tests, lazy snapshot/resource measurements, full native matrix and RedXe COM adaptation remain required.
The retained 2ca8cc0 receipts establish pre-change source evidence, not a passed overall performance gate.


The shared embedded UIA implementation now passes 1,709 Embedded checks, including the actual COM-marshaled pattern
call, all three AV control patterns, negative-screen/DPI transforms, lifetime identities, stale/hidden rejection,
application completion callbacks and 1,000 allocation-free clean updates. The earlier snapshot failure was an exact
floating-point comparison; the assertion now permits 0.01 physical pixel error. The final build also fixes a missing
test completion counter. All failed logs are retained in the consumer's .build folder. Native Accessibility and
NativeTextInput passed; an invalid Controls filter was rejected, so the upcoming full run uses the default suite list.
Full matrix, event delivery in the application, independent consumption and matched performance are still open.
The adapter adds no pixels to the existing gallery; the AV minimum-size UI change lives in RedXe and has reviewed
native and browser captures there. The initial gallery assets remain applicable to this library component.

Embedded accessibility component validation: all 18 x64 Debug and all 18 x64 Release suites passed, including
1,709 Embedded checks. Both ARM64 configurations cross-built successfully. Skills, specification, dependency and
clang-format validators passed. The runs used the default suite list; logs are retained by the coordinating RedXe
task as dxui-uia-full-debug/release and dxui-uia-arm64-debug/release. Exact-commit native CI, relocated consumption,
actual UIA event delivery and matched resource acceptance still need refreshing. This component does not close the
real application IME, screen-reader or hardware gates and does not waive the earlier performance comparisons.
