# DxUi same-side control investigation

This SDK-owned record characterizes the library's existing `sdk-594d67fb` complex-UI packet. It is separate from
RedTilde application adoption and from performance acceptance. No dependency, canonical workload input, metric,
investigation band, policy qualification or baseline was changed.

The [control replay](controls-summary.md) independently matches all 144 raw receipt hashes, 1,872 metric observations
and 72 statuses. Its [aggregate JSON](controls-summary.json) records 69 controls with at least one breach, but only
307/1,872 breached metric observations. Thirty of 78 metric keys held in every control; none breached every control.
Temporal trends differ by scenario and do not identify a production defect or a host cause.

The [separate b4 timer diagnostic](clean-prepare-timer-public-summary.json) uses SDK source `b4b28b4d`, the same native
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
pre/post executable hash receipts and unsuccessful attempts remain in private repository-local build evidence. The b4 timer
executable itself was **not archived before the later rebuild**; its recorded hash identifies the binary observed
before and after the diagnostic but does not provide a retained executable. The separately frozen 1714 executable
packet does not close that earlier custody gap.

The [1714 timer-floor aggregate](clean-prepare-timer-1714f8b0-public-summary.json) is a distinct later cohort at
source `1714f8b0`. It independently replays 512 rows and six CLI contracts and verifies the four raw report hashes
plus the frozen executable and companion artifact hashes. Its JSON records `workloadOwner`, fixture, and benchmark
input hashes. It is diagnostic only and is not pooled with b4 or treated as performance acceptance.

The [hosted 9a control replay](hosted-9a-final/README.md) is also separate. It reports 71/72 unstable controls and
248/1,872 breaches, while the earlier `594d67fb` packet above reports 69/72. These packets have different source
identities. The 9a policy verdict is inconclusive and observer status is not established; neither quiet-host nor
performance-policy acceptance is claimed. Published projections omit absolute paths, raw source content, machine
details, account data and per-process identities.

The [latest 1714 hosted replay](hosted-1714-final/README.md) is another independent cohort. All six native CI jobs
pass; the paired verdict remains inconclusive because the base policy is unqualified. Its 72/72 unstable controls
and 384/1,872 breached observations remain separate from both earlier packets. Lead replay verifies 2,448
source/executable/fixture and raw-median bindings across all 144 receipts and rehashes the provider ZIP. Shared-host
activity and observer gaps remain explicit; these findings do not identify one cause or authorize a changed band.
