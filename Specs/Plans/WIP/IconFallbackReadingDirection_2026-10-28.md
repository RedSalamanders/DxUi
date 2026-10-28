# Configured icon fallback reading direction

## Checklist

- [x] Forward configured reading direction through the missing-Fluent Small fallback.
- [x] Add deterministic coverage for Icon, IconLarge and HeroIcon with a seeded unavailable-family cache entry; verify Small RTL/LTR separation and warm reuse.
- [x] Record the fallback behavior in the typography contract and hosting documentation.
- [x] Reproduce the old-call failure in Debug run `20261028T115724860Z-51792-90cdfcba4c9a4eddb11ac22695630847`: exit one at the direction-matched Small assertion, zero skips. Root independently verifies all fifteen preserved binary/library/receipt entries; restore the fixed source exactly afterward.
- [x] Qualify fixed producer `85716c8f`: all six local builds, 57 normal x64 suite executions, twelve foreground/resource suites and two focused Menu cases pass without skips, with ASan detection and restoration verified. Hosted PR124 run `40682230783` passes all six native profiles, including actual ARM64 execution.
- [ ] Finish the separately tracked qualification-runner follow-up and full validator, then close this scope. Source85716's full validator failed one synthetic helper-readiness case; later source103's local profiling-fixture failure is separately investigated. Shared performance/policy and consumer adoption remain open.

The test seed uses the existing `Typography` font-family cache under its mutex, targets only the retained owner's exact factory/family key, and restores that entry's prior answer. No public API or font installation is required. An earlier D: build was interrupted for capacity before the test ran and was restored exactly; it is not a test result. The root's primary progress snapshot is preserved separately. Current shared performance evidence has zero resolved primary regressions across 78 metrics and 70/72 unstable controls; the base policy remains unqualified, so this is not acceptance or implicit consumer adoption.
