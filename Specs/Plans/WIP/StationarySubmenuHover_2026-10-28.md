# Stationary submenu hover

## Checklist

- [x] Preserve the local ASan Debug failure from producer source `6e90427bce52`, SDK run `20261028T164311449Z-47076-22b9b213c68e49dba392c374e2b5b54f`: `TestMenuHoveringSiblingWithChildrenReplacesOpenSubmenuAfterDelay` observed B1 with hover timer active/pending-open `0/0`. The separate latest-main run uses source `2cf` and is not the source identity for this log.
- [x] Trace the failure to cursor-seeded `hoveredIndex` in `CreatePopup` and the transition-only timer scheduling in `HandleMenuMouseMoveAtPointDip`.
- [x] Draft the deterministic parked-pointer regression and disabled/leaf negative checks using the owned pointer fixture and existing foreground attempt helper; focused execution on unchanged source remains pending.
- [ ] Review the separate production patch under `.build`; do not apply it until the old-source regression is reviewed and run.
- [ ] Qualify the reviewed candidate in all required native profiles and preserve the original failure and all new attempts.
- [ ] Reconcile the proposed contract below into `Specs/UI/UI_InputAndAccessibility.md` after source review.

## Failure evidence

The retained ASan Debug log is `.build/evidence/test-runs/20261028T164311449Z-47076-22b9b213c68e49dba392c374e2b5b54f/payload/logs/test-Menu-x64-ASan Debug.interactive.log`. It records physical B1 alignment and a delivered pointer event, then `b1-hover-observed 0 0`. The routed event hit and kept hover index 0, but `HandleMenuMouseMoveAtPointDip` saw no row transition and did not schedule the pending-open timer. The assertion then failed waiting for B11. This is source-backed evidence of the failed state transition; it does not establish external interference.

In `src/Controls/DxUi.Menu.cpp`, `CreatePopup` samples the stationary cursor when no keyboard row is selected and seeds `hoveredIndex`; after `ShowWindow`, it does not schedule a submenu timer for that seeded row. Mouse-move handling schedules only when `hit != popup.hoveredIndex`. A stationary cursor already over an enabled submenu parent can therefore remain hovered indefinitely without opening its child.

## Proposed normative contract

Add to `Specs/UI/UI_InputAndAccessibility.md` near the stationary pointer-history rule:

> When popup creation initializes hover from a stationary pointer over an enabled submenu row, the popup starts the ordinary 400 ms cascade delay after it is shown. It does not require another pointer move. Keyboard-initialized selection does not start this pointer hover timer. Disabled rows and rows without children do not start it. Existing open-child ownership and same-row movement keep their current timer behavior.

## Test scope and limits

The new test calibrates row centers from an actual popup, parks the owned physical pointer before each `Show`, drains pending mouse messages, reacquires the owner, and does not send later pointer movement. It accepts B11 already open or requires the exact pending-open timer and then uses the canonical submenu-open waiter. The disabled submenu and leaf rows must not arm a timer. Existing keyboard stationary-move tests remain the keyboard-selection negative coverage. `HandleMenuMouseMoveAtPointDip` remains transition-gated; the test-only change does not alter that path or claim new same-row no-restart runtime coverage.

The separate production patch is prepared for review only. No build or runtime has been run for this test edit; all app desktop/native work remains separately gated.
