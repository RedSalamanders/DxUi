# Embedded interaction layout paired measurement

This receipt preserves the official x64 Release `Default` paired comparison for the library-owned synthetic scene `dxui-complex-ui-v2`: 83 controls, 1,000 model rows, 1280×720 at 96 DPI, 40 frames per round and five rounds per run. It rendered completed offscreen WARP frames with clear and blocking one-pixel readback; there was no swap-chain presentation or vsync. The interleaved A/B/B/A order was repeated three times (six runs per side).

## Source and artifact identity

| Side | Recorded commit | Library source fingerprint | Executable SHA-256 |
| --- | --- | --- | --- |
| Baseline | `271bd54be24eadf3f03ba5221d1be7be069860f2` | `1BCF5661CC09596549D3854806538A8768C3DFF3645AF874A6A97B1343584004` | `06D7E344C9981572844CA32D41F5C394B636B6E44137295B568DD3788BF51572` |
| Candidate | `8fbaed5fa7906ac36b0bc1f334b56d7fdc769cea` (dirty source tree) | `D7406CC62C0C08CE8E5E35CFDE4800E1E3DABF60B96D1243007ABDC638CD6E2D` | `57D658C6401A2FF8237C8CE48D4C2267D4EF0087140F00381EEF30B3361E8AF2` |

Both sides use benchmark fingerprint `F9D66318E82EB344391494C5302785443D254A8AE18AB65C5FBC9FE80E60A5AE`. The candidate was a dirty tree at the recorded commit; this receipt does not establish an exact feature-branch commit. The workload is DxUi-owned and synthetic, not a consumer measurement. Machine: SINON, AMD Ryzen 9 9950X3D, Windows 10.0.26200, WARP 10.0.26100.9278, Balanced power policy.

## Paired-set medians

Medians below are the median of six per-run five-round medians. For FPS, higher is better; all other listed metrics are lower-is-better.

| Phase | Metric | Baseline | Candidate |
| --- | --- | ---: | ---: |
| Clean | FPS | 2510.1728555 | 2619.5840985 |
| Clean | frame p95 (ms) | 0.46835 | 0.45555 |
| Clean | private bytes | 27,711,488 | 27,248,640 |
| Clean | working set bytes | 43,722,752 | 43,317,248 |
| Dirty | FPS | 641.9691263 | 640.9037916 |
| Dirty | frame p95 (ms) | 1.74265 | 1.75 |
| Dirty | private bytes | 29,626,368 | 29,317,120 |
| Dirty | working set bytes | 45,312,000 | 45,133,824 |

Exact observed resource levels are unchanged between sides: surface bytes 3,686,400 and replacement peak 3,686,400 in both phases; composition C++ allocations 0; clean C++ allocations 0 and dirty per-run median 2,160. The paired set found no exact-budget increase.

## Result and retained reports

The six-versus-six paired set verdict is `within-noise-budget` with zero regressed metrics (minimum attainable p = 0.00216). This means no change was established; it is not proof of equal performance. The raw pairwise record also contains five `advice-required` comparisons, one `within-noise-budget` comparison and all six `unstable-control` comparisons. Every round and comparison is retained, including noisy outcomes.

All 37 JSON files from the qualification's `reports` directory are copied byte-for-byte here, including the raw run receipts, per-run comparison files and pairwise/control comparisons. The official `summary.json` is named `paired-summary.comparison.json` here to distinguish the comparison aggregate from a standalone measurement receipt; its bytes are unchanged. The reports bind each run to the source fingerprint, executable SHA-256 and common benchmark fingerprint above. This bounded library-owned scene result does not qualify consumer adoption, full P1, or RedPrism.
