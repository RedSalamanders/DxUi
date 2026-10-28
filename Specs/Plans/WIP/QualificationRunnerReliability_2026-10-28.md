# Qualification runner reliability

## Checklist

- [x] Preserve source `85716c8f` qualification: six local builds, 57 normal x64 suite executions, twelve full foreground/resource suite executions and two focused Menu cases pass; ASan detection and restoration are verified. Full tooling validation separately fails one synthetic helper-readiness assertion.
- [x] Independently reproduce `File.Move(overwrite)` failure with an open delete-sharing reader, and `File.Replace` success with a coherent old handle/new path. Preserve original failed controls and the later controlled checks. The generic original validator failure has no captured child cause and remains unattributed.
- [x] Prepare atomic existing-journal replacement and an explicit successful exit-state reset after evidence finalization, with held-reader/failure controls and bounded child diagnostics.
- [x] Complete root review of initial-publication collision safety, mutable callback state and child early-exit/deadline distinctions; all seven focused tooling cases pass. Preserve the earlier failing controls and first six-case fix run separately.
- [x] Inspect latest main `2cf30d7e` run `40681696889`: ARM64 Release fails an acrylic-metric geometry readiness wait before blur assertions. Reuse one five-second discovery/dispatch handshake, retain the 800 ms geometry budget and all visual thresholds, and extend last-successful-probe diagnostics. No original cause is assigned from the generic failure alone.
- [x] Refresh the reviewed CI-contract digest for changed runner files without changing coverage, workflow or benchmark policy. All 37 scoped-tooling cases pass; the initial stale-digest CI/local failures remain retained.
- [x] Finish all six canonical builds at `103dff20`, preserving 74 artifact hashes and receipts. Hosted Linux/Windows validators and format pass. Its local validator separately fails the profiling-tool group; a captured focused run passes 167/168 and fails its three-second end-to-end inherited-pipe assertion. Original child diagnostics are incomplete, so its exact failure remains unassigned.
- [x] Reproduce the fixture boundary with unchanged helper budgets: controlled parent setup of 3.2 seconds makes total time 4.85 seconds while the exact holder remains alive at return and the 0.2-second drain error/output prefixes are retained. An isolated EOF-wait substitute with plausible timing metadata is rejected by holder liveness. These synthetic checks do not attribute the original environment failure or qualify runtime performance.
- [x] Complete independent review and all 168 profiling-tool fixtures after separating startup from observed drain time; retain actual live-holder and acknowledged-release checks, unchanged product deadlines and exact-identity cleanup. Keep the failed original/focused runs and both draft revisions separately; qualify the final source below.
- [ ] Run the focused acrylic case, delayed modal-dispatch regression and diagnostic-retention case under the final-source authorized lease, then include them in the full profiles below.
- [ ] Freeze one clean source revision, fold the changelog and run scoped Explain/PrePush accounting.
- [ ] Revalidate all six build profiles and the three full x64 runtime/foreground profiles as required by the changed tracked-script identity. Earlier exact native receipts remain historical support, not a bypass of current admission.
- [ ] Pass full validators/format, publish and inspect current CI/main, then move this completed scope to Done. Shared performance/policy, consumer and physical-platform gates remain separately owned.

The publisher's initial operation must not overwrite a competing target; updates require the existing owned
journal. An uncooperative reader still fails publication visibly. The test reader explicitly allows replacement
and retains coherent snapshot semantics. No deadline, skip rule, benchmark input, performance band, library API,
native renderer or consumer pin changes.

The old ASan foreground run had successful sealed outcomes and an ambient native child exit of one. The owning
contract already requires actual process/invocation completion. The explicit reset is additional hardening;
historical caller failures and child receipts are not rewritten. Failures and evidence-publication errors
continue to propagate.

The durable rules are in [validation](../../Testing/Testing_Validation.md) and the [tool inventory](../../../Tools/README.md).
Default gallery behavior is unchanged; the native test readiness correction and PowerShell changes do not alter product visuals.
