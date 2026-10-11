# ARM64 ASan consumer cancellation: measured CI budget investigation

Read-only historical investigation completed on 11 October 2026. Current native source is 284a9ec60cc767cad16775e4131b597ff1101b23; its fresh local validation is being handled by the lead. No fresh PR CI has started. The results below concern older source ec680e866a218b57000a1780018c00101a437f91 and do not qualify the candidate.

The cancellation was the enclosing job's 40-minute budget, confirmed explicitly by GitHub's check annotation: `The job has exceeded the maximum execution time of 40m0s`. The annotation-disabled consumer had not reached its 15-minute step timeout. It was incomplete; its surviving log records no compiler, sanitizer or module assertion failure, but cannot establish that the remaining checks would pass.

## Exact evidence

- [Main workflow run 38055762177](https://github.com/RedSalamanders/DxUi/actions/runs/38055762177): event push, main source ec680e866a218b57000a1780018c00101a437f91, attempt 1.
- [ARM64 ASan job 114223901297](https://github.com/RedSalamanders/DxUi/actions/runs/38055762177/job/114223901297): started 13:27:24Z, completed 14:07:33Z, conclusion cancelled; the execution-time cancellation occurred at 14:07:25Z, followed by uploads/cleanup.
- API payloads are retained locally as job.json, run.json, annotations.json and all-jobs.json. The complete GitHub CLI log is job-gh.log. milestones.json preserves source line numbers and precise timestamps; derived-timing.json labels extrapolations explicitly. evidence-hashes.json hashes these saved evidence files.

| Step | UTC start/end | Observed duration | Declared limit/result |
| --- | --- | --- | --- |
| Setup + checkout + vcpkg | 13:27:25 to 13:28:50 | 1m25s | completed |
| Full ARM64 ASan native | 13:28:50 to 13:49:38 | 20m48s | 25m; failed foreground lease, known separately |
| Regular relocated consumer | 13:49:38 to 13:59:22 | 9m44s | 15m; success |
| Annotations-disabled consumer | 13:59:22 to 14:07:25 | 8m03s | 15m; cancelled by enclosing 40m job |

The 40-minute job allowed only 8m03s of the final consumer's 15-minute watchdog window. The stage limits total 25+15+15=55 minutes before setup/gallery/uploads; the enclosing limit cannot accommodate all three stage budgets.

## Where the final consumer reached

Main job-gh.log:1202 records the annotation-disabled DxUi.lib built at 14:04:10.8766193Z; :1214 records ExternalConsumer.exe built at 14:05:05.4495827Z; :1240 records NativeModuleDriver.exe at 14:06:08.7335551Z. Lines 1241-1253 record all twelve module probes and `Native module fixture: 0 failed probes across EXE + two DLLs` at 14:06:10.1445082Z. Cancellation is :1254, 14:07:25.4057371Z.

The unchanged test-consumer.ps1:110-139 then verifies output isolation, runs ten negative pin/build-identity checks, hashes outputs, writes the final ExternalConsumer receipt and prints PASS. These checks are mostly redirected to fixture logs. The final module success therefore does not close consumer validation. No final PASS or completed annotation-disabled receipt is established here. The individual negative check active at cancellation is not identified by the surviving console log.

The regular consumer's module phase ended at 13:56:38.3031552Z (:918), then PASS appeared at 13:59:22.2185991Z (:920): its post-module tail took 163.915 seconds. The cancelled variant had only 75.261 seconds after its module phase. Applying that observed tail to the cancelled variant estimates completion at 14:08:54Z, about 1m29s after cancellation, before cleanup. This is an estimate, not a result.

## Cache and dependency evidence

- Pinned vcpkg tool and manifest baseline are 9e593bb18ea69cc5095e012465dcd675a822ed0d. The initial main-job dependency install restored zero packages (:327), then built the three manifest packages in approximately 6 seconds (:381).
- Both isolated consumers already reused the runner-local default binary cache: three packages restored in 891ms (:815), then three in 725ms (:1150), from C:/Users/runneradmin/AppData/Local/vcpkg/archives. Missing package-cache hits were not the reason the second consumer exhausted the job budget.
- Each consumer intentionally creates a separate clean source clone, vcpkg-tool checkout and isolated output root (test-consumer.ps1:11-25). vcpkg-install.ps1:38-40,125-143 places managed tooling/download/install/build roots beneath that output. The regular fixture spent roughly 30 seconds cloning pinned tooling, then about 9 seconds reaching its cache restore; the second spent roughly 32 seconds plus about 8 seconds. Library compilation and the post-module validators dominate the remaining work.
- No cross-job actions/cache or explicit VCPKG_BINARY_SOURCES policy is configured by the reviewed workflow/scripts. Both observed hosted jobs initially restored zero packages. Do not assume a rerun receives a warm persistent cache.
- Annotation policy deliberately changes the output fingerprint (test-consumer.ps1:22-24). Reusing the regular consumer's compiled library for the annotations-disabled variant would invalidate this validation; preserve separate outputs and both checks.

## Can the current 40-minute workflow finish?

There is direct evidence that it can, with little margin: [prior reviewed ARM64 ASan job 114196109052](https://github.com/RedSalamanders/DxUi/actions/runs/38046179264/job/114196109052), run 38046179264, finished in 38m42s. Its native stage took 19m40s, regular consumer 8m59s, disabled consumer 8m43s; both consumers completed successfully. Its post-module disabled tail took 156.130 seconds. Applying that tail to the cancelled main-job variant estimates completion at 14:08:46Z, still after the 40-minute limit.

The earlier and cancelled jobs used runner image windows-11-vs2026-arm64 version 20261004.176.1 and MSVC 14.51.36231. Workflow, test-consumer.ps1 and vcpkg-install.ps1 Git blobs are identical at reviewed head 1432180d9b252083663481ad8838bdd16f3d3ad8, main ec680e86 and current candidate 284a9ec6:

- workflow: 02a70f8651a98a6db02c14ba9cccf91f7e6494ba
- consumer: 4eb1055fc89a3ae9627bbbf1c7377329eef5ef4d
- restore: 3f5d4bba0566758bdea68370178e66aa60b3b7b8

The previous successful consumer pair leaves only 1m18s under the job cap. The later main job added 68 seconds in native work and 45 seconds in the regular consumer alone. A fresh final-source run is reasonable, but success within 40 minutes is not assured. The new candidate needs its own complete source-bound evidence; earlier success cannot qualify it.

## Bounded resolution options if the budget failure repeats

1. The smallest workflow repair is a 60-minute enclosing budget for ARM64 ASan while retaining the exact 25-minute native and both 15-minute consumer step watchdogs. Their sum is 55 minutes, leaving roughly five for setup/gallery/uploads. All six standard consumers and both ASan annotation variants remain mandatory; no assertion, resource or sanitizer gate changes. This is a proposed infrastructure budget change, not implemented here. A workflow change requires review/update of the scope manifest digest and reassessment of evidence/provenance before delegation.
2. A more structural alternative is separate bounded consumer jobs for all six ordinary profiles plus both ASan variants, with their independent clean exact-pin fixtures and 15-minute consumer step caps. Make aggregate accounting consume every consumer outcome and preserve raw artifacts. This avoids sharing the cumulative native+two-consumer deadline but is a larger workflow/manifest/gate review; do not hide consumer failures behind the ARM foreground HOLD.
3. Tooling/cache optimization can be investigated with the recorded phase timings, while preserving exact toolchain pins, isolated dependency metadata/output roots and all ten negative/twelve module checks. Repeated pinned-tool clones/bootstraps cost roughly forty seconds per consumer. Package restores are already subsecond, so adding a package cache alone does not address the measured shortfall. Do not share annotation-dependent compiled outputs or remove negative checks. This is a larger tooling change requiring its own tests and provenance review.

A single fresh run as currently configured may fit, as the previous run did. Repeated blind reruns are not a demonstrated fix. If cancellation repeats, retain the failed run and choose an explicit bounded budget/scheduling/tooling repair; do not report seven consumers plus an incomplete eighth as the required eight.

No repository, workflow, manifest, gate, Git or remote state was changed. Only these external evidence files were written. The dated S0-S2 integration decision permits verified ARM64 foreground refusals before input, not this cancelled synthetic-consumer obligation.