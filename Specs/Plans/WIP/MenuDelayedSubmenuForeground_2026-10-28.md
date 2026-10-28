# Delayed submenu-close foreground qualification

The ARM64 Release job in main run 40668541474 failed `TestMenuHoveringSiblingClosesOpenSubmenuAfterDelay` on source `592b9fb9f31c9834bbb4b521ec2ced385366291f`. The retained hover trace shows the second root popup created, then activation cancellation before its modal loop began. It does not identify the deactivation message, external application or cause. The test never reached its delayed submenu-close assertions. The other five native profiles passed; those results do not close this failed profile.

The existing fixture acquires foreground only before its calibration Show. Its second Show follows calibration, driver cleanup and physical pointer alignment without reacquisition or a foreground-loss guard. The production menu's deactivation cancellation remains unchanged.

- [x] Preserve the failure log and inspect the exact test/driver/menu paths against the failed source.
- [x] Prepare a test-only repair using the canonical fresh-attempt helper, owner reacquisition before the tested Show and deferred behavior predicates.
- [x] Preserve per-attempt failures and owner/cancellation traces; fail exhaustion and never retry a held behavioral failure.
- [x] Add a scenario-specific controlled-loss regression using synthetic `WM_ACTIVATEAPP` loss and `WM_CANCELMODE`. This proves wrapper recovery, not a real external thief caused the CI failure. Reuse the existing `TestForegroundAttemptDoesNotRetryAReadySequenceWithAFailedPredicate` and five-attempt refusal/loss tests for the helper's negative semantics.
- [ ] Root patch review, focused formatting/source checks, canonical build and authorized interactive Menu regression execution.
- [ ] Complete required native/profile qualification and evidence accounting before closeout.

The enduring contract is in [validation](../../Testing/Testing_Validation.md). No timeout, skip rule, product menu behavior, pixel expectation or performance threshold changes. Gallery/docs behavior is unchanged because this repair changes only native test orchestration. No native execution has yet validated this patch; do not infer a passing failed CI profile from source review.
