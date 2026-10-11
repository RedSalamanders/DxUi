# PR73 run398 ARM64 Full-stage timeout investigation

This is failed-source evidence, not native qualification or a CI-gate success. All three ARM64 Full steps
were interrupted by the existing 25-minute step limit. Their overall jobs failed even though all four
ARM64 clean-pin consumer stages completed. No native timeout exception is proposed.

The read-only captures bind run `38166757540`, attempt 1, PR head
`1cdbfde4c93bafa6a0c794fd80939387467fefa4`, base
`ec680e866a218b57000a1780018c00101a437f91`, actual tested merge
`58e71d4528d8cb041ff71931fe0f6c9aaaeef426`, and tree
`be000995bef911cfb3a9bf66868d4e92e47049cf`. The completed-profile collectors also verify the
actual checkout line in each original job log. API payloads and logs retain original stdout bytes.

| ARM64 profile / job | Before compilation | Reported MSBuild | Elapsed through build success | Native Full outcome | Consumers |
| --- | --- | --- | --- | --- | --- |
| Release / 114550858172 | 7m20s | 15m31s | 23m54s | 25m limit before ordinary suites or completed performance receipt | Regular 9m48s, success |
| Debug / 114550858255 | 8m35s | 12m49s | 22m29s | 25m limit after raw benchmark samples; performance provenance and ordinary suites incomplete | Regular 9m38s, success |
| ASan Debug / 114550858318 | 7m55s | 11m44s | 20m41s | 25m limit interrupted Rendering; 14 earlier ordinary PASS markers, performance wrapper and intentional ASan probe completed | Regular 9m29s and annotations-disabled 9m09s, success |

The first phase includes startup/discovery and the required prebuild v2 attestation. Its internal evaluation,
hashing and discovery durations are not individually logged. `build.ps1` starts its Stopwatch immediately
before MSBuild and stops it before postbuild attestation; its final build-success message is printed after
that attestation. These boundaries show compilation plus cold provenance consuming most of the old Full
limit. They do not demonstrate a hung ordinary native test.

Release and Debug finish all runner/filter/watchdog/interactive-lease self-checks before interruption.
Debug's surviving native benchmark JSON has no source/profile/provenance stamp or completed comparison,
so it is partial evidence. ASan's log records a warning refusal and unchanged restoration before child
input, but the native Full timeout remains a separate blocker; the packet does not accept that logged
refusal as an exemption. ASan's Rendering log ends during
`TestGridMultilineRepaintsAtANewDpiLikeAFreshGrid`, without a completed suite receipt. Accessibility,
WindowHost, InteractiveLease and final Full accounting have not completed in that profile.

The four consumer stages have completed final PASS lines and actual exact-merge receipts with exit 0,
10 rejected mismatches, 12 native-module probes, API revision 4, MD runtime and the intended annotation
policy. These facts describe the old failed head only. They do not qualify a later budget-source candidate.

## Proposed bounded repair

The lead's proposed Full allowance is 45 minutes on ARM64 and the existing 25 on x64. Enclosing budgets
are x64 Debug/Release 45, x64 ASan Debug 60, ARM64 Debug/Release 65 and ARM64 ASan Debug 80 minutes.
Each is its selected Full allowance plus the ordinary 15-minute consumer allowance, another 15 minutes
for the ASan annotations-disabled variant, and five minutes for setup/gallery/resources/artifacts.
The 300-second per-native-test watchdog and both 15-minute consumer step watchdogs remain unchanged.

The measured ARM64 build boundary is already 20m41s–23m54s; remaining performance/provenance and
native work needs its own time. Forty-five minutes supplies finite headroom without deleting or weakening
that work. All measured consumers complete within 9m09s–9m48s; this evidence does not justify raising
their 15-minute limits. This proposal is an infrastructure allowance, not a promised completion time.
Every new exact-source native/consumer/sanitizer/provenance result and the fresh paired study must still
be completed and reviewed. Existing unqualified performance HOLD and specifically verified ARM64
before-input foreground refusals are separate; these Full timeouts are not accepted under either.

## Preserved proof

- `job-*.receipt.txt`, `annotations-*.receipt.txt`, `run.receipt.txt`, `pr.receipt.txt`, `merge.receipt.txt`
  and `tested-tree.receipt.txt` are original API stdout bytes.
- `job-*.log` are complete original job-log bytes fetched using the job API with escape sequences retained.
- `inputs/ci.yml` and `inputs/test-scopes.json.source.txt` are the exact failed-source Git blobs decoded
  from retained blob API payloads. Both are checked against the actual tested tree and `git hash-object
  --no-filters`. The preserved workflow SHA/digest is `7d4a1afd4c0a74ae8b93185f5ca4f78eda76d0eda4eff3e368bffd8c207730c8`.
- `source-bindings.receipt.txt` records those source identities. The scope manifest uses a `.source.txt`
  filename to retain its exact JSON bytes without presenting it as a benchmark receipt under Measurements.
- `milestones.receipt.txt` names exact original log line numbers; `timing-analysis.receipt.txt` derives
  the stage durations and records incomplete versus completed phases explicitly.
- `artifact-seals/` preserves the three completed-profile capture seals exactly. `artifact-bindings.receipt.txt`
  lists all 10/12/77 sealed files with lengths and SHA256 values and the external capture roots. Each original
  capture includes run/job/PR/merge/artifact API bindings, original log, capture driver and downloaded files.
  All 99 entries were reopened and hashed with zero mismatches. Artifact bytes remain at those recorded
  external roots; these seals do not assert compiler/SDK/binary bytes that the workflow did not upload.
- `timing-analysis.pending.receipt.txt` records the earlier incomplete snapshot. `source-capture-attempt1.failed.txt`
  retains a corrected external orchestration-variable error. `source-bindings.attempt2.diagnostic.receipt.txt`
  preserves the incidental conservative hash computed after the lead started budget edits; it is not the frozen
  head or native compiled closure. The final source bindings rely on verified immutable Git blobs.

The packet seal excludes only itself. No repository behavior, native runtime, remote state or acceptance
policy was changed by this investigation.
