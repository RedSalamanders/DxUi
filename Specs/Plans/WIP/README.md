# Active work

Files in this directory are non-normative execution or decision records; current requirements stay in the owning
domain spec under `Specs/<Domain>/`. This index is routing only. Every direct plan in this directory is listed
exactly once, as a bullet that opens with its bold status and a colon (`- **ACTIVE**:`), links its title to its
file, and ends with an em dash and its purpose. `ACTIVE` is executable work, `HOLD` paused work, `DECISION`
unresolved product intent, and `RETIRED` a bounded tombstone that names its retirement date, reason, current
owner, and a `git show` recovery command.

- **ACTIVE**: [Remaining work and qualification](RemainingWork_2026-11-04.md) — the single current plan for DxUi. Start with its verified checkpoint and ordered tasks; deferred scope and unanswered owner decisions remain explicit.

- **ACTIVE**: [Performance and motion: speed, frame rate, memory and animation](PerformanceAndMotion_2026-11-13.md) — the one plan for making DxUi faster, raising its frame rate, lowering its memory use and animating every interactive change, on the Windows 11 version 25H2 baseline. Budgets `G1`–`G15`, assumptions `A1`–`A16`, 45 verified facts about today's cost, fixtures `FX1`–`FX14` and 92 items in waves `W0`–`W10`: measurement, correctness fixes, quick wins, frame clock, retained scene and regions, the motion system with a new `Expander` control, text and layout, rich documents, accessibility and IME cost, memory, and scale with a conditional GPU backend. It replaces the archived runtime efficiency study, its challenge review and the frontier plan and carries their confirmed decisions; the default presentation mode (`A5`) is still the developer's to decide. Plan only; code implementation has not started.

- **ACTIVE**: [Explicit state machines and lifetime safety](ExplicitStateMachines_2026-11-10.md) — source-verified internal state-machine refactors and bug fixes across `EmbeddedHost`, `ControlHost`, `RichTextEditor`, `MenuController`, `ComboBox`, `TabControl`, `Tree`, `AnimationDispatcher`, `TextInputServices` and `Tools/` (`DXUI-SM-01`..`DXUI-SM-08`) with zero public ABI break. Plan only; code implementation has not started.

The [API 5 failure-policy execution record](../Done/FailurePolicy_2026-11-02.md#ordered-implementation-and-acceptance) closes library implementation and x64 qualification; the current plan retains formal performance-policy qualification, broader admission and product adoption separately. The owner's 7 November delivery scope requires x64 runtime tests and all three ARM64 compilation profiles, with ARM64 runtime explicitly set aside.

[Completed and superseded records](../Done/README.md#earlier-completed-records) retain decisions, evidence, failures and detailed appendices.
[The consolidation ledger](../Done/WipConsolidation_2026-11-04.md#pending-item-coverage) accounts for every original unchecked direct-plan item.
Add new in-scope work to the current plan instead of creating another overlapping queue.
