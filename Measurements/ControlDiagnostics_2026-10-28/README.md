# DxUi SDK control diagnostics and native qualification evidence

Workload owner: **DxUi**. Fixture: **dxui-complex-ui-boundaries-v3**. This SDK-only packet contains the public-safe diagnostic projections for source `0b8e76c58a11698744e6a4ce5f803f6170b1f2b6` plus a hash-only projection of local native qualification. It excludes the RedTilde census and all raw archives/log copies.

## Diagnostic scope

The SDK data covers 34 slots: 32 factor processes and 2 separate smoke processes; 272 windows; 64 within-cell two-process comparisons; and 8 timer observer comparisons. Each cell has two fresh process replicates. The windows and frames are repeated observations, not independent replicates.

For Default WARP composition-enabled paths, clean tail wall time is concentrated in blocking Map intervals (roughly 0.8–1.0 ms in two-process per-window summaries). Dirty tails include Prepare (about 1.2–2.2 ms) and Map (about 0.8–1.6 ms where composition is enabled). CallingThread Map intervals are longer (roughly 5.7–6.4 ms clean and 6.3–7.5 ms dirty). These are observed diagnostic wall intervals; Map is not GPU duration and the measurements do not establish causality. Timer Off has no inner Prepare/Map boundaries. CPU query brackets include query wall time, preemption, and endpoint overhead; coarse zero CPU time is retained as valid and cycles are not converted to time. The host observer sampled 1,195 times over 34 slots with approximately 1.026 s median sample gap. This does not establish a quiet host. Controls and performance-policy review remain open.

## Local native qualification

The qualification projection is source-wide supporting evidence, not a claim that every validation/build/full-suite result ran this exact performance fixture. It is bound to source `0b8e76c58a11698744e6a4ce5f803f6170b1f2b6` and retains hashes for all 12 root result/log pairs: six build profiles, three full x64 test configurations, validation, formatting check, and `Test-Changes -Explain`. The three full test runs each report 19 passing suites (x64 Release, Debug, ASan Debug). The actual ASan detection probe exited 1 and reported detection true for heap-use-after-free; its result and log are hash-bound in `native-qualification.json`.

All six hosted native profiles in [run 40646316345](https://github.com/RedSalamanders/DxUi/actions/runs/40646316345) pass. The paired collection and artifact upload complete, but verdict publication fails with an explicit inconclusive result because the measured-base policy is not independently qualified. [Bounded independent replay](paired-ci/README.md) verifies all 144 raw reports, 1,728 per-run medians, 78 aggregate metric summaries and 72 controls; 71 controls are unstable. It omits p-value recomputation and full host-stream replay. Overall CI fails at the separate policy gate; no performance acceptance is claimed.

PR122 is merged as `592b9fb9f31c9834bbb4b521ec2ced385366291f`; its tree equals qualified head `0b8e76c5` and tested CI merge `246a64ff`. The completed diagnostic plan is [archived](../../Specs/Plans/Done/PerformanceControlDiagnostics_2026-10-28.md). Shared policy/control and consumer performance gates remain open.

## Files and identities

- `sdk-identity.json` binds source, executable, collector, schedule, host stream/state and artifact manifest hashes.
- `sdk-slots.csv`, `sdk-windows.csv`, `sdk-replicate-comparisons.csv`, `sdk-largest-replicate-spreads.csv`, `sdk-timer-observer-comparisons.csv`, `sdk-smokes.csv`, `sdk-artifact-hashes.csv` contain the SDK projections only.
- `native-qualification.json` contains source-wide supporting result/log hashes, native run IDs, and scoped hosted-CI status. `projection-manifest.json` has role `packet-integrity-manifest`. All three JSON metadata records carry workload owner, fixture, the exact 12-entry InitialAttestation.profilingInputs map, and the distinct 14-entry InputClosure map from the preserved native state.
- `projection-manifest.json` hashes all outer payload files and the `paired-ci/integrity-manifest.json`; that child manifest closes the five nested paired-CI files. Its direct entry count/bytes and transitive closure totals are recorded explicitly.
- `paired-ci/integrity-manifest.json` seals the independently reviewed hosted comparison projection. Packet-local Git attributes preserve declared bytes across checkouts.

This is diagnostic and scoped qualification evidence, with the performance-policy refusal preserved.
