# ARM64 ASan CI job budget investigation, 2026-10-11

The 40-minute enclosing limit cancelled the final annotations-disabled consumer in
[main run 38055762177](https://github.com/RedSalamanders/DxUi/actions/runs/38055762177),
[job 114223901297](https://github.com/RedSalamanders/DxUi/actions/runs/38055762177/job/114223901297).
This packet preserves that historical failure at source `ec680e866a218b57000a1780018c00101a437f91`, attempt 1.
It does not qualify the implementation candidate, waive an incomplete consumer, or establish a performance verdict.

GitHub's retained check annotation in `annotations.receipt.txt` states: `The job has exceeded the maximum execution time of 40m0s`.
The final consumer was cancelled after 8m03s, before its own 15-minute watchdog. Its last visible completed phase was
all twelve native-module probes; output isolation, ten negative checks and the final receipt/PASS were not established.
No compiler, sanitizer or module assertion failure appears in the surviving log. The precise negative check active at
cancellation is unknown, so this result remains incomplete.

| Stage | UTC start/end on 2026-10-10 | API duration | Observed outcome |
| --- | --- | --- | --- |
| Setup, checkout and dependencies | 13:27:25 to 13:28:50 | 1m25s | Completed |
| ARM64 ASan native suites | 13:28:50 to 13:49:38 | 20m48s | Failed foreground lease, accounted separately |
| Regular clean-pin consumer | 13:49:38 to 13:59:22 | 9m44s | Passed |
| Annotations-disabled clean-pin consumer | 13:59:22 to 14:07:25 | 8m03s | Cancelled by enclosing job |

The native job started at 13:27:24Z and completed cleanup at 14:07:33Z. Execution was cancelled at
14:07:25.4057371Z. The final consumer's module fixture completed at 14:06:10.1445082Z, leaving only 75.261 seconds
for its remaining checks. The regular consumer needed 163.915 seconds after the same phase. The prior reviewed
[ARM64 ASan job 114196109052](https://github.com/RedSalamanders/DxUi/actions/runs/38046179264/job/114196109052)
completed both consumers within 38m42s; its disabled-consumer tail was 156.130 seconds. The extrapolated finish times
in `derived-timing.receipt.txt` are estimates from those observed tails, not completed results.

Both jobs used `windows-11-vs2026-arm64` image `20261004.176.1`, MSVC `14.51.36231`. The two main-run consumers
already restored all three dependency packages from the runner-local cache, in 891ms and 725ms. Missing dependency
cache hits do not explain this enclosing cancellation. Each fixture intentionally retains its own clean pin, tooling
checkout and annotation-dependent output identity; their compiled libraries are not interchangeable.

## Bounded repair and continuing obligations

The workflow now gives only ARM64 ASan Debug a 60-minute enclosing job budget. Its unchanged native 25-minute,
regular-consumer 15-minute and disabled-consumer 15-minute watchdogs total 55 minutes, with five remaining minutes
for setup and artifact collection. The other five profiles retain 40-minute job budgets. This provides a bounded
allowance; it does not predict that every future run will finish or guarantee recovery from arbitrary hangs.

The six standard consumer profiles and two ASan annotations-disabled variants must still complete on the actual
tested PR merge tree. Correctness, deterministic resources, sanitizer detection, provenance and aggregate failure
accounting are unchanged. The dated implementation-only integration scope does not excuse this cancelled consumer;
only its explicitly recorded qualification HOLD and verified ARM64 foreground refusals before input remain separate.
`Test-NativeScope.ps1` checks the six profile budgets, retained stage limits and cumulative ARM64 ASan allowance,
including a falsification that restores the historical 40-minute cap.

## Preserved inputs and evidence

`source-bindings.receipt.txt` records the historical source commits, run/job identities and exact Git blob/SHA256/byte
identities for archived `inputs/ci.yml`, `inputs/test-consumer.ps1` and `inputs/vcpkg-install.ps1`. Those inputs were
downloaded by Git blob ID and independently checked against Git's blob hash. They are historical source evidence,
not supported or editable build inputs. The narrow `.gitattributes` rule preserves this packet's exact archived bytes in every checkout.

- `run.receipt.txt`, `job.receipt.txt`, `all-jobs.receipt.txt`, `annotations.receipt.txt`: complete read-only GitHub API payloads.
- `job-gh.log`: complete retained failing-job log; `milestones.receipt.txt` preserves precise source lines and timestamps.
- `prior-reviewed-all-jobs.receipt.txt`, `prior-reviewed-job-gh.log`: previous completed run's API job list and complete log.
- `derived-timing.receipt.txt`: explicitly labelled extrapolations, retaining the original investigation snapshot's
  `currentSource` (`284a9ec6`); that field is not the final candidate or a runtime qualification claim.
- `investigation.snapshot.md`: the original external read-only analysis, including then-proposed alternatives and
  its original candidate snapshot. Its historical proposals do not supersede the bounded repair described above.
- `original-evidence-hashes.receipt.txt`: original nine-file evidence inventory; `SHA256SUMS` binds this full archived packet.

New final-source local/PR results must be reviewed independently. Historical successful stages and inferred completion
times cannot replace the eight complete current-source consumer receipts.

The API and investigation metadata retain their original JSON payload bytes under `*.receipt.txt` names.
`receipt-name-map.receipt.txt` records the original names, archived names, lengths and SHA-256 values. The
original evidence inventory and historical analysis retain their original filename references. This naming keeps
metadata distinct from the canonical benchmark JSON contract; it changes no validator or measurement requirement.
