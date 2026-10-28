# PR #124 observer/control follow-up

Scope: read-only replay of the retained host-observations/paired.jsonl from completed run 40682230783. This is descriptive evidence only; it does not change the paired replay, qualify the base policy, or establish performance acceptance.

## Bound inputs

- Baseline source: `2cf30d7e5ad092c77945cbe6d45bfb450f0e6762` (`7E92385E380DA54ABC18878DD6BCCE99972EC0B8A9AE96772C9E33EBDEEA6E1E`).
- Candidate source: `85716c8f4b187716f4191d8b397a95dc6288db7a` (`F148833DF9B931BE1844EA7007A16D4DAA801575DAFEB08A16E8B67C44ABBDB7`); tested merge `31842b234789ce9bb581af23c7c437a7d1bbf12a` has the candidate tree.
- Paired artifact `12819704184`: local ZIP SHA-256 matches provider digest `AB5556C0EDF32220AE802A6489E64014D0CE963BCBCF4ABD99BE16A750C0A3B0` (194,033,462 bytes).
- The raw trace was streamed from the retained ZIP entry; its 106,395,730-byte SHA-256 `810895FF0F28838613432BBAF1A0490C95AA0834079FE526C372CA072861D65D` was recomputed and matches the completion receipt. The receipt SHA-256 is `795A2C55824FA6F2CF6B69782DDA9147C8C4B02E32E1D6B912D32D937716639C`.

## Coverage and logical process/report mapping

The trace contains 6354 records and 6353 counter intervals from `2026-10-28T11:49:48.9215822+00:00` through `2026-10-28T13:39:01.1307768+00:00`. Sequence numbers are contiguous. Sample spacing median is 1.004749 seconds; nearest-rank p95 is 1.017314; the maximum is 18.869506 seconds, with 17 gaps longer than five seconds.

There are 144 observed contiguous `DxUi.EmbeddedTests` process-name/PID segments and 144 raw reports. Each segment's last observation maps one-to-one to a report completion within 1.1 seconds (nearest-rank median offset 0.560889 seconds, maximum 0.998694). The trace lacks image paths/hashes and process creation times, so this is a logical time/name join, not proof of the loaded executable image.

## CPU population and quantiles

CPU is expressed as busy core-equivalents from successive `GetSystemTimes` counter deltas, not percent of an assumed VM quota. All percentiles use nearest-rank `sorted[ceil(0.95*n)-1]`; the median is the middle value or mean of two middle values.

| Population | Count | Median busy cores | Nearest-rank p95 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| All recorded counter intervals | 6353 | 1.121361 | 3.835372 | 42.755856 |
| Intervals whose ending sample observed at least one `DxUi.EmbeddedTests` process | 315 | 2.219612 | 3.316956 | 3.620731 |
| One report-end interval per logical report/process match | 144 | 2.846395 | 3.380077 | 3.620731 |

The all-observed population includes every interval endpoint with the process present; it is not the 144-row report-end subset. Both include benchmark CPU and unrelated machine activity.

## Same-side control pair CPU

The existing control replay has 70 unstable and 2 stable pairs. For each pair, CPU is the mean of its two report-end interval observations; within-pair difference compares those two intervals.

| Control status | Pairs | Median pair mean busy cores | Nearest-rank p95 | Median absolute within-pair difference | Median drifted metrics |
| --- | ---: | ---: | ---: | ---: | ---: |
| Unstable | 70 | 2.890985 | 3.331569 | 0.195711 | 3 |
| Stable | 2 | 1.803013 | 1.833386 | 0.105236 | 0 |

The two stable controls are too few to support a reliable comparison. These system counters cannot distinguish benchmark work from unrelated processes or establish that host load caused control instability.

## Disposition and limits

No quiet-host threshold is declared; the run summary marks the acceptance policy untrusted and `policy-review-required`. No threshold was applied. The observation intervals do not cover process start/end exactly, have gaps, and do not include executable identity, process creation time, scheduler/GPU/thermal data, or a processor quota. Keep the original early-return policy verdict and all 70/72 unstable-control findings unchanged; this is not performance acceptance.

Source pointer: `Tools/Watch-BenchmarkHost.ps1` captures `GetSystemTimes` and per-process snapshots (lines 54, 65). Its completion receipt disclaims dedicated-host, scheduler-delay, and GPU-duration claims. The machine-readable summary and 144 logical mappings / 72 control pairs are in `observer-control-followup.json`.