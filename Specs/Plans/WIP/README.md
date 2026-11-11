# Active work

- **ACTIVE**: [Remaining work and qualification](RemainingWork_2026-11-04.md) — the single current plan for DxUi. Start with its verified checkpoint and ordered tasks; deferred scope and unanswered owner decisions remain explicit.

- **SPEC READY**: [Runtime efficiency and rendering quality](RuntimeEfficiency_2026-11-07.md) — the 7 November gpui-fast/rune study, red-team reviewed on 8 November 2026 and completely re-audited on 11 November 2026 across `DxUi` (`4acabe0e`), `longbridge/gpui-fast` (`af019de`), `anomalyco/rune` (`0ea75abe`, including `research/01..38` and `gpu-floor`) and clean-room `ppmpreetham/BUI` (`ae284781`): a 71-row verified evidence ledger, deterministic work-counter scenarios `S1`–`S20`, phases `P0`–`P8` (covering controls, host, scheduling, dirty regions, text/typography, layout, piece-tree/rich documents/math/diagrams, accessibility, TSF IME and struct memory), visual track `V1`–`V8` and clean-room BUI items `B1`–`B6`. D1, D2 and D4 retained; revised D3 (API revision 6) and D5–D10 await confirmation. Plan only; code implementation has not started.

- **ACTIVE**: [Explicit state machines and lifetime safety](ExplicitStateMachines_2026-11-10.md) — source-verified internal state-machine refactors and bug fixes across `EmbeddedHost`, `ControlHost`, `RichTextEditor`, `MenuController`, `ComboBox`, `TabControl`, `Tree`, `AnimationDispatcher`, `TextInputServices` and `Tools/` (`DXUI-SM-01`..`DXUI-SM-08`) with zero public ABI break. Plan only; code implementation has not started.

The [API 5 failure-policy execution record](../Done/FailurePolicy_2026-11-02.md#ordered-implementation-and-acceptance) closes library implementation and x64 qualification; the current plan retains formal performance-policy qualification, broader admission and product adoption separately. The owner's 7 November delivery scope requires x64 runtime tests and all three ARM64 compilation profiles, with ARM64 runtime explicitly set aside.

[Completed and superseded records](../Done/README.md#earlier-completed-records) retain decisions, evidence, failures and detailed appendices.
[The consolidation ledger](../Done/WipConsolidation_2026-11-04.md#pending-item-coverage) accounts for every original unchecked direct-plan item.
Add new in-scope work to the current plan instead of creating another overlapping queue.
