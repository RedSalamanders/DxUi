# Stationary submenu hover

## Checklist

- [x] Preserve the local ASan Debug failure from producer source `6e90427bce52`, SDK run `20261028T164311449Z-47076-22b9b213c68e49dba392c374e2b5b54f`: `TestMenuHoveringSiblingWithChildrenReplacesOpenSubmenuAfterDelay` observed B1 with hover timer active/pending-open `0/0`. The separate latest-main run uses source `2cf` and is not the source identity for this log.
- [x] Trace the failure to cursor-seeded `hoveredIndex` in `CreatePopup` and the transition-only timer scheduling in `HandleMenuMouseMoveAtPointDip`.
- [x] Add the deterministic parked-pointer regression and disabled/leaf negative checks using the owned pointer fixture and existing foreground attempt helper. Canonical x64 Debug build passes at test-only commit `594667be7d07`; the single controlled run `20261028T173245143Z-56040-6b085d8d2d2c466f9a297fee7a0abb62` fails on the unchanged production path because the enabled row has neither an open child nor a pending-open timer. It has zero skips and no timeout/interruption; the lease restores foreground, focus and cursor. Preserve the child exit 1 independently of the wrapper's thrown failure and inherited native exit variable.
- [x] Independently review and apply the production patch only after the controlled red. Reacquire the live popup after `ShowWindow`, prove controller ownership, and reuse the existing timer only for an eligible seeded pointer row without keyboard selection, an existing timer or its own open child.
- [ ] Qualify the reviewed candidate in all required native profiles and preserve the original failure and all new attempts.
- [x] Reconcile the stationary-pointer contract into `Specs/UI/UI_InputAndAccessibility.md` after source review. Review docs/gallery: no control, token, geometry or rendered appearance changes; existing Menu gallery coverage remains applicable. This change corrects delayed pointer behavior.

## Failure evidence

The retained ASan Debug log is `.build/evidence/test-runs/20261028T164311449Z-47076-22b9b213c68e49dba392c374e2b5b54f/payload/logs/test-Menu-x64-ASan Debug.interactive.log`. It records physical B1 alignment and a delivered pointer event, then `b1-hover-observed 0 0`. The routed event hit and kept hover index 0, but `HandleMenuMouseMoveAtPointDip` saw no row transition and did not schedule the pending-open timer. The assertion then failed waiting for B11. This is source-backed evidence of the failed state transition; it does not establish external interference.

In `src/Controls/DxUi.Menu.cpp`, `CreatePopup` samples the stationary cursor when no keyboard row is selected and seeds `hoveredIndex`; after `ShowWindow`, it does not schedule a submenu timer for that seeded row. Mouse-move handling schedules only when `hit != popup.hoveredIndex`. A stationary cursor already over an enabled submenu parent can therefore remain hovered indefinitely without opening its child.

## Normative contract

The owning rule is recorded in `Specs/UI/UI_InputAndAccessibility.md` near stationary pointer history:

> When popup creation initializes hover from a stationary pointer over an enabled submenu row, the popup starts the ordinary 400 ms cascade delay after it is shown. It does not require another pointer move. Keyboard-initialized selection does not start this pointer hover timer. Disabled rows and rows without children do not start it. Existing open-child ownership and same-row movement keep their current timer behavior.

## Test scope and limits

The new test calibrates row centers from an actual popup, parks the owned physical pointer before each `Show`, drains pending mouse messages, reacquires the owner, and does not send later pointer movement. It accepts B11 already open or requires the exact pending-open timer and then uses the canonical submenu-open waiter. The disabled submenu and leaf rows must not arm a timer. Existing keyboard stationary-move tests remain the keyboard-selection negative coverage. `HandleMenuMouseMoveAtPointDip` remains transition-gated; the test-only change does not alter that path or claim new same-row no-restart runtime coverage.

The original ASan producer and controlled Debug red artifacts are retained with 36 hash-bound files under `.build/stationary-submenu-hover-6e-red-preservation`. The red evidence manifest is `92EBA043023A4867FE35443ABF21716C70ACC81C7D23972471140A69F1EFE047`. The production correction still needs its own green qualification; earlier source results do not qualify it. Consumer adoption, upstream performance policy and native/physical-platform acceptance remain separate gates.
