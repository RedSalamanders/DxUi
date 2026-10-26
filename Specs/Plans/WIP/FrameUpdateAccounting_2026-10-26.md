# Complete Update-stage metric accounting

## Progress checklist

- [x] Verify both WindowHost render paths return from pending preparation before assigning `updateUs`; retained SDK407 RedTilde reports have positive total duration with zero Update duration on attempts 0–15.
- [x] Record elapsed Update duration through scope exit on success, resource failure, deferred preparation and exception unwind. Preserve preparation, scheduling and rendering behavior.
- [x] Add a pending-then-completed native regression through ordinary dirty rendering and bitmap capture. Check incomplete Update is measured and Render/Present remain zero. Update the authoritative performance contract.
- [ ] Run the native regression, Rendering/control/embedded coverage, all three x64 profiles with real ASan detection, and all three ARM64 builds. Native ARM64 requires hosted execution.
- [ ] Inspect source-bound CI and paired evidence; retain the existing unqualified base-policy gate. This metric correction is not a product CPU improvement or a policy waiver.
- [ ] Fold changelog, publish source/test evidence, verify the remote merge and move this plan to Done when its required gates close.

## Scope and evidence

Base is SDK `3bcf7ed9d0aaa6afb14495f0e0a03232b6d3bfea`, tree `18be66885916f67cafd8aeb761207bf15fbd9120`, identical to reviewed source4077 and the earlier six-profile CI tree. Only native WindowHost timing changes. Public headers, API revision, layouts, workload, thresholds and acceptance policy stay unchanged.

The outer frame timer already records pending work. Its nested Update timer was assigned only after successful preparation, so a deferred return reported a misleading zero stage duration. A scope guard must finish before the outer metric set is emitted. Zero remains valid for true clock quantization; it must not be substituted for omitted work. Resource failure has no reliable deterministic test injector in the current harness, so that exit is covered by the shared scope guard and source review rather than a claimed injected execution.

Use canonical `build.ps1`, `test.ps1`, `Test-Changes.ps1 -Explain` and PrePush accounting. The focused case is `TestAttachedHostUpdateMetricsIncludePendingPreparation` in the Rendering suite. Its one-millisecond fixture delay establishes only a nonzero counter interval; it is not a performance threshold or product timing assertion. Keep source/binary/compiler bindings and all failed runs. No consumer pin moves implicitly; RedTilde's earlier diagnostics remain bound to SDK4077 and retain the incomplete stage-field boundary.

Durable semantics belong in [Core_PerformanceAndResources](../../Core/Core_PerformanceAndResources.md). The aggregate redesign and original-UI performance follow-up remains owned by RedTilde's `RedesignCompletionAndRendering_2026-10-24` plan.
