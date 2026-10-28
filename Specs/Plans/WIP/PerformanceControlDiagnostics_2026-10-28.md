# Performance control diagnostics

Status: ACTIVE. Diagnostic source extension; no established production cause or performance acceptance.
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
- [ ] Finish focused tooling/static/format validation and record exact commands/results.
- [ ] After coordinated cleanup permits native work, build the changed embedded target and execute registered native
  assertions plus each explicit v3 scenario/phase. Preserve fresh raw reports, binaries, captures and sidecars.
- [ ] Declare a finite fresh-process serial schedule for Default and distinct-grid, Clean/Dirty, timer Off/On and
  Default/CallingThread WARP. Retain every process; compare same-ranked-frame intervals, query overhead and raw CPU
  counters. Additional owned synthetic load controls require an explicit bounded design; never stop independent work.
- [ ] Run required profile/CI qualification and hosted paired gates under their unchanged contracts. Native ARM64,
  foreground, consumer R10 and measured-base policy acceptance remain independently owned gates.

The diagnostic uses 200 warm frames and repeats dirty input frames 200–239 in each command-control window; it is not
the canonical 20-frame/five-round acceptance workload. Main-thread cumulative CPU differences include query overhead;
they do not measure WARP workers, actual placement, scheduler delay or GPU duration. Timers Off retains CPU probes,
so it cannot alone isolate their effect. CPU-cycle counts remain raw counts. Process-memory peaks in the existing
canonical report are sampled peaks by contract, not exact transient high-water marks.

Docs/gallery review: performance instructions and tool inventory descriptions change. No library source, supported
API, visual control, theme or gallery content changes; gallery regeneration is unnecessary for this diagnostic.
This plan remains WIP until source qualification and diagnostic interpretation are complete. An inconclusive study
or a green native test run cannot close the policy or consumer performance holds.
