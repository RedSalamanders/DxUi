# DxUi paired CI 0b independent projection

Workload owner: **DxUi**. Fixture metadata: **dxui-complex-ui-v2**; the three scenario-specific fixture strings are in `public-projection.json`. This packet contains a compact independent view of hosted run `40646316345`. The ZIP was preserved unchanged and hash-verified (`1029CB8B0E8BC6E34FD0B173AAF2CDB171A9A51EC300E4438F2B62EAC9B09EFB`).

## Source and run identity

The CI merge commit `246a64ff3dff36c1ce2c95af60c128f5dd2a1bf9` was verified through the GitHub commit API: tree `50df00889de214a681d3c366c934dd0feb4a1ada`, parents `881e5d5ff252d5988c1357a5c64d14f480b46793` and `0b8e76c58a11698744e6a4ce5f803f6170b1f2b6`. Baseline and PR-head `.gitattributes` blobs both specify `*.h text eol=crlf`, and no path-specific nested attributes override that rule for the six inputs. Each expected CI input SHA-256 matches the GitHub merge-tree blob after reconstructing its Windows checkout bytes using that committed rule (6/6). This is Git-content-derived checkout evidence, not a retained full source archive.

## Results

`primary-metrics.csv` has all 78 reported primary rows, including their median/max/change fields, 12-element block-effect arrays, and reported regression/budget/verdict flags. `same-side-controls.csv` has all 72 controls with summary stability status, underlying comparison status, and the complete reported drift/regression arrays. `same-side-control-metrics.csv` expands all 1,872 reported phase/metric details with before/after values, changes, noise bands, and raw flags. P-values and adjusted p-values are omitted and were not recomputed.

The 72 summary control statuses are 71 `unstable-control` and 1 `stable-control`; the underlying same-side comparison files are 61 `advice-required` and 11 `within-noise-budget`. The literal CI verdict is **inconclusive**, `strictControls=false`, `policy-review-required`, because “Policy review required: The base policy is not independently qualified; its verdict cannot establish a candidate pass.” The report does not claim a pass.

The host completion metadata declares 8075 samples at a requested 1000 ms interval, stream SHA-256 `691956131947476CC1D8110338FC468817C866B01657DC106322EFCB3F621A8F`, and 133384126 uncompressed bytes. The stream was not replayed or rehashed; the metadata itself cautions that no dedicated-host, scheduler-delay, or GPU-duration claim follows.
