# Native Mermaid syntax expansion — 3 November 2026

The owner requested defect fixes and syntax expansion. The independent fixed checkpoint is
`33ce97ce2b29aed4eadb9270bf66e7080d56b9fc`; its defect evidence remains in the adjacent
[display packet](../MermaidDisplay_2026-11-03/README.md). This change adds five bounded forms:
sequence actors, chronological plain notes, bare `autonumber`, messages without arrowheads,
and directionless flowchart headers. It does not claim complete Mermaid compatibility.

The pre-change diagnostic links the preserved immutable library with matching private headers.
All five forms are rejected and all five bodies remain exact. Source/project, output, results
and the frozen native baseline are indexed under `.build/mermaid-syntax-expansion-20261103/`.
The retained unpaired default benchmark is diagnostic only; it cannot establish performance acceptance.

Two complete x64 Release builds have zero warnings/errors. The focused parser/CPU layout regression passes,
as do the full EditorControls checks preceding the new raster case. Its first two failures were diagnosed
test-oracle errors, not discarded renderer results: an arbitrary comparison between two different words,
then an asymmetric threshold that rejected the dark actor arm's actual contrast value of 128. Independent
DirectWrite widths and WIC pixel values are retained with the raw logs; the second failing executable is
preserved by hash. The corrected complete raster/resource function passes in an isolated diagnostic using
the canonical helpers. The next canonical build/run is the qualification candidate; these diagnostics are not.

Actor figures and notes reuse the existing model/timeline and label resources. Notes consume
the shared edge budget; their rectangle uses the existing route storage. Number text is derived
only during preparation and accessibility construction, while the source body and label stay exact.
No note vector, retained worker, periodic timer, draw-time path or label allocation is introduced.

The syntax checkpoint `9ed15504` subsequently passes all 19 noninteractive library suites and RedTilde's
73 native cases, 32 standard captures and 18 custom Light/Dark/DPI captures. Review of those actual images
finds another shared defect: meaningful connections and frames use faint decorative border ink. The retained
editor now selects effective foreground ink for diagram structure, while explicit snapshot/export callers
retain their stroke option. A real offscreen editor reproduction measures central-stroke pixel distance 16
from its background before the fix; the corrected diagnostic measures 184 in Light, 157 in Dark and 170 in
high contrast. These are channel-intensity differences, not contrast ratios or general accessibility acceptance.
The production library builds with zero warnings/errors. Before/after probe sources, executables, setup
failures and pixel logs remain under `contrast-probe-before/` and `contrast-probe-after/` in the private packet.
Final canonical native and consumer evidence belongs to the subsequent exact implementation revision.

References: pinned Mermaid 11.4.1 sequence documentation and `flow.jison` default direction,
plus the official [sequence](https://mermaid.js.org/syntax/sequenceDiagram.html) and
[flowchart](https://mermaid.js.org/syntax/flowchart.html) guides. Newer custom numbering arguments
remain outside this bounded profile. Activations, control fragments and interactions remain fenced source.

Final source-bound validation and exact consumer adoption are recorded after the new build;
six-profile/native ARM64, physical/foreground, performance policy and release gates retain their owners.
