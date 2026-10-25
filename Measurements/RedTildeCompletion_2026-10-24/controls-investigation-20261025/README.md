# DxUi same-side control investigation

This SDK-owned record characterizes the library's existing `sdk-594d67fb` complex-UI packet. It is separate from
RedTilde application adoption and from performance acceptance. No dependency, canonical workload input, metric,
investigation band, policy qualification or baseline was changed.

The [control replay](controls-summary.md) independently matches all 144 raw receipt hashes, 1,872 metric observations
and 72 statuses. Its [aggregate JSON](controls-summary.json) records 69 controls with at least one breach, but only
307/1,872 breached metric observations. Thirty of 78 metric keys held in every control; none breached every control.
Temporal trends differ by scenario and do not identify a production defect or a host cause.

The [separate timer diagnostic](clean-prepare-timer-public-summary.json) uses SDK source `b4b28b4d`, the same native
library implementation, four independent default-scene WARP processes, seeds 20261025–20261028, and 16 repetitions
of each 1/8/64/512-call batch. All 512 rows, schedule positions, counters, reported fixture-state fields and four raw
hashes were independently checked. Six CLI checks pass, including invalid/overflow seed rejection and preservation
of an existing report. The ordinary Embedded suite also exercises its arithmetic and real prepared-state oracle.

At one call, the median Prepare bracket rounds to zero QPC ticks and its p95 is one tick. At 512 calls, the pooled
median bracket is about 0.176 ticks per call. These are averages of an entire bracket, including loop and HRESULT
bookkeeping; the empty-loop reference is separate and never subtracted. They expose timer/method sensitivity, not
isolated method latency or an explanation of every canonical p95 flag. SDK tooling validation was still running;
there was no concurrent build. No quiet-host, observer-overhead or performance-policy acceptance is claimed.

The native assertions additionally compare replacement-peak bytes and animation state; the current report omits
those two fields, so external replay is limited to the serialized subset. Raw reports, owned-child records,
executables, compiler/build attestations, complete control replay and unsuccessful attempts remain in private
repository-local build evidence. Published projections omit local paths, host details and per-process identities.
