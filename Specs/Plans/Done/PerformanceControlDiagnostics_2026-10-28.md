# Performance control diagnostics

Status: COMPLETE for the diagnostic implementation, qualification and bounded interpretation. Independent policy/control acceptance remains open in the [shared qualification plan](../WIP/ConsumerUiQualification_2026-10-11.md). No production cause or performance acceptance is established.
Owner: [performance and resources](../../Core/Core_PerformanceAndResources.md).

The immutable SDK086 paired study has 69/72 unstable same-side controls, with the largest distinct-grid candidate
pair changing across multiple clean and dirty rounds. Existing Default-only boundary controls cannot establish
that scene's phase costs. The early policy-review-required gate remains separate from these observations.
No band, policy, baseline or historical report is changed. Host activity can coincide with a measured process
without proving exact-frame scheduling or explaining the candidate's change.

- [x] Inspect canonical preparation/composition, 20-frame warmup, screenshot encoding, round order, sampled memory
  peaks, hidden state and caller scheduling. No deterministic production defect established from source review.
- [x] Extend the separate boundary diagnostic with explicit v3 scenario and clean/dirty phase selection, owner-thread
  CPU queries outside the timed frame, probe QPC brackets and an Update-end boundary. Preserve all canonical inputs.
- [x] Add parser/model native assertions and portable schema/runner negative controls, including availability,
  chronology, coarse zero deltas, disabled timers, budgets and exact scenario/phase transport.
- [x] Finish focused tooling/static/format validation: 168 profiling-tool assertions, full validators/tooling,
  clang-format 22.1.3 and scoped Explain pass at source `0b8e76c5`.
- [x] Build all six local profiles and run all nineteen normal native suites in each x64 configuration (57 suite
  executions, no capability skips). The actual ASan probe detects heap-use-after-free with child exit 1. Preserve
  the capacity-interrupted first Release build separately from the completed root-owned qualification.
- [x] Declare and complete a finite fresh-process serial schedule for Default and distinct-grid, Clean/Dirty, timer Off/On and
  Default/CallingThread WARP. Retain every process; compare same-ranked-frame intervals, query overhead and raw CPU
  counters. All 32 factor processes and two separate MultilineGrid smokes pass. Independent replay reconciles all
  272 windows, raw hashes and 1,195 host observations. Two processes per cell are independent replicates; repeated
  windows and frames are not. No further synthetic-load collection is declared.
- [x] Verify all six hosted native profiles at `0b8e76c5`, including actual ARM64 Debug/Release/ASan, in
  [run 40646316345](https://github.com/RedSalamanders/DxUi/actions/runs/40646316345). Validation, tooling and formatting pass.
- [x] Finish the hosted paired collection/upload and record its literal inconclusive verdict: the measured-base policy
  is not independently qualified. Bounded replay verifies 144 raw reports, 1,728 per-run medians, 78 aggregate metric
  summaries and all 72 same-side comparisons; 71 controls are unstable. P-values are not independently recomputed
  in this review. Source-input reconstruction uses the committed CRLF checkout rule and verifies six inputs at the
  base, PR head and GitHub merge tree; it is not a retained full source archive. The [CI packet](../../../Measurements/ControlDiagnostics_2026-10-28/paired-ci/README.md)
  preserves every primary row/control and its limits. Independent policy/control qualification and consumer R10 stay open.
- [x] Integrate diagnostic PR122 as `592b9fb9f31c9834bbb4b521ec2ced385366291f`. Its merge tree equals the qualified
  head `0b8e76c5`; preserve dirty documentation bytes before and after the local fast-forward. No performance cost is accepted.

The [public-safe evidence packet](../../../Measurements/ControlDiagnostics_2026-10-28/README.md) binds exact source,
executable, collector, schedule, raw artifacts and local qualification. Default-WARP clean composite tails are
dominated by blocking Map wall intervals; dirty tails share preparation and Map costs. CallingThread WARP is slower
on composite paths and is rejected as a production optimization. These observations do not establish GPU duration,
scheduler attribution or a quiet host, and do not close the historical unstable controls. Library source, supported
headers and all six canonical benchmark input files remain byte-unchanged by the diagnostic extension.

The diagnostic uses 200 warm frames and repeats dirty input frames 200–239 in each command-control window; it is not
the canonical 20-frame/five-round acceptance workload. Main-thread cumulative CPU differences include query overhead;
they do not measure WARP workers, actual placement, scheduler delay or GPU duration. Timers Off retains CPU probes,
so it cannot alone isolate their effect. CPU-cycle counts remain raw counts. Process-memory peaks in the existing
canonical report are sampled peaks by contract, not exact transient high-water marks.

Docs/gallery review: performance instructions and tool inventory descriptions change. No library source, supported
API, visual control, theme or gallery content changes; gallery regeneration is unnecessary for this diagnostic.
Source qualification and diagnostic interpretation are complete for this scope. The shared qualification plan owns
the remaining policy/control holds; the consumer owns R10, billing-blocked hosted ARM64 and physical-platform gates.
An inconclusive study or a green native test run cannot close those holds.
