# Cold ARM64 build and provenance windows, 2026-10-11

[PR #73 run 38166757540](https://github.com/RedSalamanders/DxUi/actions/runs/38166757540), attempt 1, tested
merge `58e71d4528d8cb041ff71931fe0f6c9aaaeef426`: ordered parents
`ec680e866a218b57000a1780018c00101a437f91` and `1cdbfde4c93bafa6a0c794fd80939387467fefa4`,
tree `be000995bef911cfb3a9bf66868d4e92e47049cf`. All three ARM64 Full steps hit the exact
25-minute limit. These are blocking incomplete runs, not native qualification or permitted foreground refusals.

| Profile | Time before compilation | Reported compilation | Build/post-build attestation finished | Observed Full result |
| --- | --- | --- | --- | --- |
| ARM64 Release | 7m20s | 15m31s | 23m54s after Full started | Interrupted during runner checks before ordinary suites or a stamped benchmark receipt |
| ARM64 Debug | 8m35s | 12m49s | 22m29s after Full started | Interrupted before ordinary suites or a completed provenance-stamped benchmark receipt |
| ARM64 ASan Debug | 7m55s | 11m44s | 20m41s after Full started | Stamped unpaired benchmark, deliberate sanitizer probe and first 14 suites complete; interrupted during Rendering |

The native per-test watchdog did not cause these failures. Foundation through Tooltip completed in ASan; Rendering
started at 19:35:06.083Z and the enclosing step interrupted it at 19:35:27.893Z. Accessibility, WindowHost,
InteractiveLease and the final Full outcome were not established. ASan also recorded a warning refusal before input;
that separate refusal does not excuse the Full timeout. A native-written raw benchmark without the runner's completed
source/build/environment stamping is not a canonical performance receipt.

Each ordinary ARM64 clean-pin consumer completed independently: Release 9m48s, Debug 9m38s and ASan 9m29s.
ASan's annotations-disabled consumer completed in 9m09s. Both final PASS markers and receipts are required; a last
module probe is not completion. The x64 jobs completed all Full suites, four consumers, the intentional ASan probe
and gallery work, at this old source identity. None of these receipts may be relabeled as the repaired candidate.

## Finite repair and required rerun

Full retains 25 minutes on x64 and receives 45 minutes on ARM64. The observed cold-build/provenance work plus the
remaining native work motivates this allowance; it does not prove future completion. Native 300-second test
watchdogs, the two separate 15-minute consumer limits, all suites and their failure rules are unchanged.

| Profile | Full limit | Consumer limits | Enclosing limit |
| --- | --- | --- | --- |
| x64 Debug / Release | 25m | 15m | 45m |
| x64 ASan Debug | 25m | 15m + 15m | 60m |
| ARM64 Debug / Release | 45m | 15m | 65m |
| ARM64 ASan Debug | 45m | 15m + 15m | 80m |

Each enclosing row adds five minutes for setup, resource/gallery work and artifact collection. The job remains a
finite outer watchdog; this margin cannot guarantee arbitrary slow or hung work. NativeScope regression checks
reject each of six truncated rows, the historical shared 25-minute Full cap and absent, zero or widened consumer
limits. The normalized workflow digest and scoped manifest are updated together.

The incomplete paired job was cancelled after the non-exempt native timeouts and all eight consumers completed,
so a corrected candidate could replace the blocked run. It establishes no paired verdict, controls or policy
acceptance. Fresh complete exact-head/base/merge-tree CI is required. Correctness, sanitizer, deterministic resource,
provenance and performance acceptance remain unchanged; qualification and actual consumer adoption remain in WIP.

## Preserved evidence

The source-bound timing packet retains original API payload bytes as `.receipt.txt`, raw job logs, actual tested
workflow/scoped-manifest Git blobs, annotations, timing derivations and source bindings. API and source identity
must be read from those original records. Native sample-only JSON is retained with a source-name mapping under
`.receipt.txt` so it cannot masquerade as a complete canonical benchmark. `SHA256SUMS` binds the retained packet;
copy receipts bind original paths, bytes and hashes. Historical failures and incomplete performance remain separate
from validation of the finite repair and any later native/paired CI outcome.

This workflow/test repair changes no control rendering or gallery content. The source-bound x64 harness captures
were reviewed in all five themes and embedded mode; native rendering changes, consumer screenshots and design-system
work receive their own validation when changed.

Artifact leaf names are indexed to keep every committed path within the 150-character consumer checkout limit.
The copy receipt maps each retained name to its original path and first archive name; every original byte, length
and hash is preserved. The separate archive driver record accounts for its exact copy outside the original 129
source/artifact-copy entries. The initial dependency-validation failure is retained with repair validation below.

Repair validation: all five validators and complete Windows tooling pass, including 14/14 NativeScope cases.
The validation receipt binds unchanged versioned/nonignored input bytes before and after that run. Native Ubuntu
PowerShell7.6.6 independently passes the final 14-case focused overlay. Formatting passes. The initial 14 path
findings, pre-test launcher failure and earlier focused overlay retain their original diagnostics/source bindings.
These tooling results do not substitute for fresh exact-head native, consumer, sanitizer or paired CI.
