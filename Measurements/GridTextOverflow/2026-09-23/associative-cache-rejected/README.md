# Rejected associative grid cache experiment

The candidate reduces repeated text-layout work but increases process memory. It is
**rejected for adoption**, and the five experimental code/test files plus their provisional
contract paragraph are restored to base `0ae8362`. The consumer pin is unchanged. This
does not resolve the earlier V11 memory increase or grant a resource exception.

The hypothesis was to keep the 32-entry/4,096-UTF-16-unit bounds while replacing direct
mapping with full-key associative lookup and bounded recency. More useful occupied slots
avoid collision churn, but also retain more live layouts. There is no allocation-stack
attribution and no claim that the whole process delta belongs to one cache structure.

## Rebuilt, identical fixture comparison

Both policies were rebuilt with `build.ps1 -Rebuild`, x64 Release, zero warnings/errors.
The fixture/helper/benchmark hashes are identical; only Grid.cpp and its public header
differ. Sixteen rows have two distinct French multiline cells each. Repaint and bitmap
capture produce 64 diagnostic accesses; this is a 32-cell working set, not 64 cells.
The borrowed model outlives its host. Candidate repeated pixels match and teardown passes.

| Result | Original direct mapping | Associative candidate |
|---|---:|---:|
| Warm accesses: hits / misses | 28 / 36 | 64 / 0 |
| Warm native layout creation attempts | 72 | 0 |
| Grid / Embedded | Pass / pass | Supplementary pass / pass |
| Rendering | Expected warm-reuse assertion failure, exit 1 | Supplementary pass, exit 0 |
| Common dirty private bytes | 28,643,328 | 29,659,136 |
| Common dirty peak private bytes | 28,717,056 | 29,683,712 |
| Common clean preparation p95 | 0.0003 ms | 0.0004 ms |

The candidate adds **1,015,808 bytes (0.969 MiB, 3.55%)** to common dirty private memory.
The comparison also flags the tiny clean preparation delta; its absolute size does not
authorize discarding it. Surface payload, replacement payload and composition allocation
counts do not grow. All rounds and flags remain in the raw reports.

The mandatory candidate `test.ps1 -PerformanceBaseline ...` **fails before functional
suites execute**. Its exit 1 remains an overall failed gate. Subsequent direct native
Grid/Embedded/Rendering passes, with zero skips and recorded executable hashes, establish
functionality only. They cannot establish full configuration, resource or consumer
qualification. The prior V11 six-profile results do not apply to this prototype.

## Initial heap observation and provenance corrections

The initial 14:05 local direct-map build explicitly compiles Grid.cpp; its 14:06 heap run
precedes the 14:10 candidate build. On that matched heap fixture, hits rise 116,716 ->
118,180, misses fall 7,684 -> 6,220, replacements fall 2,008 -> 0 and layout creation
attempts fall 15,368 -> 12,440. Median dirty FPS rises 84.176 -> 106.326. Clean private
bytes rise 32,174,080 -> 33,255,424; retention median rises 33,660,928 -> 34,650,112.
Free heap falls 14,262,560 -> 12,429,720. Heap free space and process private memory
are different accounting views and must not be added. This observation is not acceptance.

Restoring original source with older timestamps later left the candidate Grid object
and library linked into original-header tests. The 14:20 `direct-map-regression` build,
later `direct-map-common` reports and first `warm32-direct-map` attempt are **invalid as
original-policy evidence**. The last exited 0xC0000005 during teardown; no stack trace
attributes the cause. A separate borrowed-model lifetime hazard was corrected before
the rebuilt pair. The earlier 16-cell observations cannot establish a valid old/new
regression comparison. The accidental unchanged-candidate preparation retry is retained
and supplies no baseline.

The candidate driver also copied old original-policy suite receipts after the performance
gate stopped execution. These are quarantined under `stale-copied-receipts`, **not candidate
passes**. The actual candidate performance report and later supplementary functional logs
are separate. The driver is retained as executed, including this flaw; do not reuse its
blind receipt-copy tail. A future runner must verify current-run provenance before copying.

## Reproduction and retained scope

[SHA256.txt](SHA256.txt) binds all 99 raw/patch files. Raw reports/logs are byte-preserved;
`.gitattributes` prevents checkout newline conversion. Non-performance JSON uses the existing
`.receipt.txt` convention; the hash manifest maps original names without changing their bytes. Each rebuilt policy has a patch
against base `0ae8362`; patches normalize source text to LF, while `inputs.json` retains
the exact CRLF source hashes. Exact source snapshots remain in the recorded external root.
The packet retains successful, failed, invalid and redundant attempts with their scope.

Current source is restored V11. **Existing Release binaries are the rejected candidate**;
rebuild before any later execution. No restored-source test pass is fabricated. No public
API or visual change remains, so the already-reviewed V11 docs/gallery stay applicable;
this evidence and the performance documentation explain the rejected alternative.

Next: resolve the original grid/menu resource decisions and remaining integrated acceptance
without repeating rejected variants or substituting functional passes for resource approval.

Skills, specifications, dependencies and format checks pass after restoration. Initial archive
validation rejected generic metadata named .json and missing raw-folder explanations; the archive
now follows the existing .receipt.txt convention and supplies those explanations. Validator rules
and evidence bytes remain unchanged.
