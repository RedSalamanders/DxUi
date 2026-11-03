# Mermaid and Markdown display corrections — 3 November 2026

The canonical library corrects two Mermaid fallbacks and two Markdown presentation defects: trimmed styled
label spans, shared ER/class relationship attachments, duplicate unordered-task bullets, and faint code
language/line-number ink. Source and per-view error ownership stay intact; no public API or syntax family is added.

At clean baseline `f2e328a0`, an isolated CPU diagnostic reports all three observed flags: an invalid label
span, a rejected ordinary ER fan, and a duplicate unordered-task marker. The same diagnostic reports zero
flags after the fixes. These are reproductions, not qualification. Both executables, source/project and JSON
results remain under `.build/mermaid-display-20261103/`.

The clean baseline and the final x64 Release candidate build have zero warnings/errors. The first candidate
library build retained C4625/C4626 diagnostics from the new optional staging owner; explicit deleted copy/move
operations corrected them without suppression. Its raw rejected log remains alongside the subsequent successful
library and complete native builds. Final native build log:
`.build/logs/build-x64-Release-b81120cba1c64a67ab8c9b0e015701c9.log`.

The candidate passes **EditorControls, Embedded, Accessibility and Rendering**, with zero capability skips.
All four new regressions pass, including independent cardinality/attachment checks and light/dark code-chrome
pixel assertions. Source-bound run:
`.build/evidence/test-runs/20261103T115558395Z-53040-5976af9da9594a079a53bf08b152ab38/`.
The five themed gallery sheets and embedded example were regenerated with `gallery.ps1 -PublishDocs` and
visually reviewed. They are actual hidden-harness captures of invented content; their generation receipt
records the exact build/profile and image hashes.

Initial repository tooling validation found the missing ignored `.ci-engine` checkout. It was restored
independently at the exact `.ci/project.json` pin `e32cf9f9b29f4e817decb09e1be44be7fc1a94de`; the original
failure remains separate from checks after that environment repair. Final validation and source/binary/report
identities are recorded in [the non-benchmark validation summary](evidence.json.template), using the existing
qualification-metadata format rather than presenting native test outcomes as benchmark measurement receipts.

The baseline and candidate complex-UI measurements are **unpaired diagnostics**, collected during ordinary
workspace activity. They do not establish a performance effect, stable controls or acceptance under the
still-unqualified base policy. Warm composition/hidden-idle resource assertions stay enabled in the native suites.

RedTilde's previous exact pin already supplied eight bounded native Mermaid families. Its stale support/fallback
claims are corrected in the consumer, which owns exact-pin adoption and actual before/after app captures.
This packet does not close the other production-review findings, six-profile/native ARM64 qualification,
physical input/AT, foreground suites, performance policy, hosted CI or release gates.
