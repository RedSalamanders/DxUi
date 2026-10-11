# Release Default comparison and fixed variability checks

These are library-owned DxUi synthetic Default/WARP observations on the same x64 host. The frozen clean
pre-change baseline is `869d74e35a7a85ba5af76f460630d313aa98648a`; the clean automatic-start candidate is
`02961a88321c75ccd39090873f48977206d6dfdb`. Full v2 actual tool/SDK/dependency/artifact checks guard every report.
The original candidate comparison returns **advice-required**, retained without replacement.

| Report | Median clean offscreen FPS | Median dirty offscreen FPS |
| --- | ---: | ---: |
| Frozen before | 2748.971 | 666.189 |
| Candidate after | 2358.171 | 569.974 |
| Fixed candidate control 1 | 2251.023 | 569.128 |
| Fixed candidate control 2 | 2003.005 | 605.434 |

The initial comparison flags ten timing/FPS metrics, including about 14% lower clean/dirty FPS. The fixed
two-control inventory is declared before either new report; all ten raw rounds per report remain retained.
Candidate clean rounds range from 865.685 to 2482.468 FPS across these controls. This variability does not
dismiss the original flag or identify a cause. Candidate-only controls cannot replace matched baseline-side
controls or qualify the seed policy. Performance acceptance stays open; no threshold or baseline is changed.

`initial-copy.receipt.txt` and `control-copy.receipt.txt` bind the twelve report/comparison/log copies.
`root-review.receipt.txt` records the independent raw medians/ranges and fixed inventory. The additional
candidate-state review matches all 26 candidate report identity fields to the actual Release post-focus state,
retained in the adjacent `focus-Release` leaf. That check establishes identity, not non-regression.

Canonical report JSON declares `workloadOwner: DxUi` and its benchmark/source/profile hashes. Comparison and
review JSON is retained as `.receipt.txt`; it is not a workload measurement. The archiver receipt binds every
leaf file to its immutable external snapshot. Earlier reports remain preserved elsewhere in the parent packet.
