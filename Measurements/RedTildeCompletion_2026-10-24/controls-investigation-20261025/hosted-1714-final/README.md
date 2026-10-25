# Public projection: DxUi hosted paired study

- Verdict: **inconclusive** (`policy-review-required`); base policy not independently qualified.
- Workload owner: DxUi.
- Fixtures: Default=`dxui-complex-ui-v2`, MultilineGrid=`dxui-complex-ui-multiline-grid-v1`, MultilineGridDistinct=`dxui-complex-ui-multiline-grid-distinct-v1`.
- Comparison: `dc67aed689f0552cdb061d9b00ba9e0e920af23c` (F423000D1AD799EDBAA3EE17B121480C5AE567CC75E2BCE2AF0D783AC7AB26F1) to merge ref `ccd4692560db3b8e8a539c26cf959b82d2b135ba` / PR head `1714f8b0c0fa628bcef56de76078f85c869fc50c` (5F2A73423D94CC1804919015AF3C0DB92E2088FA2D485D316689911E20358797).
- Replay: 72/72 unstable controls; 384/1,872 metric flags; 30/78 keys held in all 24; 0/78 drifted in all 24.
- Artifact ZIP SHA-256: `47611AA1FC33BE8320A952B3B4611125245EE507C91BECAD2706DD5C96569710`.

No regression row is reported, but this does not qualify a candidate pass. The shared-host observer does not establish a quiet or dedicated host.

“72/72 unstable” means each same-binary control comparison crossed a band on at least one metric; 384/1,872 metric
observations crossed bands and 1,488 held. The legacy `composeCpuP95Ms` field is elapsed QPC time around Composite,
not thread CPU time. At 10 MHz, one QPC tick is 100 ns, so clean no-op Prepare P95 values near a few ticks are
floor-sensitive.

The observer reported 32.85% weighted whole-host busy time. This `GetSystemTimes` aggregate includes the benchmark,
the observer and other processes; it does not identify external contention, establish a quiet host, or show that
preemption caused the timing variation. Replay found no control-pair matching defect and established no cause for
the shared scenario/time movement. No thresholds, fixtures or bands changed.

