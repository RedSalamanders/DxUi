# Public projection: DxUi hosted paired study

- Verdict: **inconclusive** (`policy-review-required`); base policy not independently qualified.
- Workload owner: DxUi.
- Fixtures: Default=`dxui-complex-ui-v2`, MultilineGrid=`dxui-complex-ui-multiline-grid-v1`, MultilineGridDistinct=`dxui-complex-ui-multiline-grid-distinct-v1`.
- Comparison: `dc67aed689f0552cdb061d9b00ba9e0e920af23c` (F423000D1AD799EDBAA3EE17B121480C5AE567CC75E2BCE2AF0D783AC7AB26F1) to merge ref `ccd4692560db3b8e8a539c26cf959b82d2b135ba` / PR head `1714f8b0c0fa628bcef56de76078f85c869fc50c` (5F2A73423D94CC1804919015AF3C0DB92E2088FA2D485D316689911E20358797).
- Replay: 72/72 unstable controls; 384/1,872 metric flags; 30/78 keys held in all 24; 0/78 drifted in all 24.
- Artifact ZIP SHA-256: `47611AA1FC33BE8320A952B3B4611125245EE507C91BECAD2706DD5C96569710`.

No regression row is reported, but this does not qualify a candidate pass. The shared-host observer does not establish a quiet or dedicated host.

