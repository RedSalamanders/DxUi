# Multiline caret viewport clipping

Status: **DONE**, October 1, 2026. Source baseline `c5dccff08bb00c2668a72f337c0a7eaf3446f53f`.

Clip painted multiline carets to the same text viewport as glyphs and selection.
Partial lines and wheel-scrolled offscreen carets must not draw into padding or
sibling controls. Preserve native text geometry, selection, read-only behavior
and accessibility. No consumer pin or application code changes are implicit.

Authority: [controls](../../UI/UI_ControlsAndLayout.md),
[input](../../UI/UI_InputAndAccessibility.md),
[performance](../../Core/Core_PerformanceAndResources.md),
[validation](../../Testing/Testing_Validation.md), and
[documentation](../../Core/Core_Documentation.md).

- [x] Inspect reported harness capture and retain unchanged-source Debug,
  Release and ASan Debug common performance baselines before implementation.
  External library evidence: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-caret-clip-20261001`.
- [x] Demonstrate failed WARP pixel regression before implementation, including
  partially visible and fully offscreen carets; retain exact fixture identity.
- [x] Apply bounded paint clipping; preserve editing and text-service geometry.
- [x] Run all x64 Debug/Release/ASan functional suites, all three ARM64 builds,
  WARP/device-loss and clean/dirty/hidden resource checks. Final Embedded repeats
  cover the strengthened on-surface wheel witness. Full suites and final relinked
  binaries have separate identities; see the evidence packet.
- [x] Resolve paired performance acceptance. Original comparator flags,
  ABBAABBA processes and same-binary controls remain retained. On October 1 the
  developer explicitly accepted the reported Debug timing tradeoff and directed
  adoption. Its cause remains unproven; no thresholds or baselines changed.
- [x] Update usage/domain docs, publish and review five-theme gallery, validate
  skills/specs/dependencies/format, and retain reviewed library receipts.
- [x] Complete root production/test/evidence review and move this plan to Done.
  A cheaper-model packet audit was independently checked and its one wording
  correction applied. Native ARM64 execution and consumer adoption remain separate.

Reviewable [qualification packet](../../../Measurements/MultilineCaretViewport/2026-10-01/README.md)
binds source/fixture/binary identities, before/after pixels, all suites/builds,
gallery and the bounded performance investigation. Root's production review
approved the balanced multiline-only clip. Performance disposition is recorded
in the packet. This closes the library slice only; it does not close consumer I26
or other owners' deferred platform, presentation or assistive-technology work.
