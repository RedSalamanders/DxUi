# Markdown research and standard data

`mermaid-11.4.1` pins the original Mermaid syntax documentation, flow grammar and
MIT notice from tag `mermaid@11.4.1`, peeled commit
`9868f3a4c3784e214d8b6c74277ca542774e7de9`. `collection.json` records original
URLs, repository paths, Git blob IDs and SHA-256 hashes for all 24 files;
`syntax-inventory.json` retains the complete declared syntax-document inventory.
These are reference/corpus inputs, never a compiled or deployed Mermaid runtime.
Original Markdown and grammar files use `.txt` suffixes so their upstream-relative
links remain unmodified data rather than becoming repository documentation links.
The owned C++ limited flowchart profile remains M5 work; reference availability
does not establish grammar or rendering compatibility. Unsupported families and
constructs must remain exact editable source with explicit diagnostics.

`dxui-flowchart-expectations.json` independently classifies all 105 original
flowchart fences: 13 admitted graphs and 92 unsupported definitions.
`dxui-family-matrix.json` accounts for the 22 syntax documents, including the
flowchart profile, example collection and 20 unsupported diagram families.
`Tools/Generate-NativeFlowchartTestData.ps1` checks every original file hash,
the MIT notice, inventory and expectation pins before generating native vectors.
Original snippets use LF in test vectors; original file bytes remain unchanged.
Neither accepted CPU graphs nor unsupported results establish native rendering.

`katex-0.16.11` retains only original command documentation and the MIT notice
from commit `fbbea3573439d51e6568356f5886053b111b1f88`; `collection.json` records
URLs, Git blobs and exact SHA-256 values. These pin the math command reference.
No KaTeX implementation, JavaScript runtime, browser or external helper is built
or deployed. The owned native math parser/layout remains M5 work.

The parser implementation is owned Unicode C++ in `src/Controls/DxUi.MarkdownSyntax.cpp`.
MD4C, Discount and cmark-gfm inform API boundaries; none is linked or used as the
implementation. Exact earlier research snapshots remain in the cooperative
[reference packet](../../Measurements/RichText/2026-10-18-cooperative/README.md).

`whatwg-2026-10-18/entities.json` is the original authoritative WHATWG character
reference data, downloaded from [the HTML standard](https://html.spec.whatwg.org/entities.json).
Its receipt records exact bytes, SHA-256, URL and collection time. The included
WHATWG license has CC-BY-4.0 document and BSD-3-Clause source-code terms. The
generated source table is sorted ordinally and contains all 2,125 names ending
in a semicolon. Legacy names without a semicolon are deliberately excluded by
the CommonMark lexical rule. Source spelling and multi-scalar output stay distinct.

`corpora` retains all original 652 CommonMark 0.31.2 and 672 strict GFM 0.29-gfm
examples, their original notices and the earlier collection receipt. The JSON
contains both CommonMark Markdown and expected HTML. GFM is the complete spec
input from cmark-gfm 0.29.0.gfm.13, not the implementation. CommonMark/GFM corpus
material is attributed to John MacFarlane and the CommonMark/GFM contributors
under the included CC-BY-SA notices; generated tests mechanically transcribe the
original inputs, HTML expectations and extension flags into UTF-16. GFM visible-tab
arrows are decoded to tabs according to the upstream convention; original bytes
remain unchanged. These data are independent of the owned parser/serializer.

Run these from the repository root; generation and checking need no network:

```powershell
./Tools/Generate-MarkdownEntityData.ps1 -Check
./Tools/Generate-MarkdownTestData.ps1 -Check
./Tools/tests/Test-MarkdownData.ps1
```

Both generators require exact data and license hashes before writing anything.
The generated corpus header also carries each original example's section and
expected HTML for an independent block-structure/code-content/list-start oracle.
Those expectations come directly from the pinned standard inputs, not from the
owned parser. M3 uses every original expected HTML value for an independent
semantic oracle. The generated header preserves per-example extension names;
both task examples marked `disabled` by the original harness are retained and
run with task parsing. The serializer preserves the printed GFM convention of
flattening directly nested redundant strong HTML tags, without deleting syntax
wrappers or changing original expectations.

Tests cover deterministic regeneration, read-only verification, stale/missing
output and every modified/missing input or notice. Native tests check lossless
partitions of every original corpus input, all entity values and the selected
block-section structural expectations. Full original semantic expectations,
directed Unicode/version boundaries and native/embedded inline fixtures are now
implemented; the active plan records their matrix qualification. These checks
do not qualify the formatted renderer or semantic editing.

M3 also uses original Unicode 17 `UnicodeData.txt` and `CaseFolding.txt` from the
shared [Unicode data](../Unicode/README.md). The offline
`Tools/Generate-MarkdownUnicodeData.ps1` verifies both data hashes and the Unicode
license before emitting character classes and full default case-fold mappings.
No foreign parser implementation is compiled into DxUi. A private, reference-only
cmark-gfm 0.29.0.gfm.13 CLI was built at commit
`587a12bb54d95ac37241377e6ddc93ea0e45439b` for supplemental observations.
Its raw outputs and build failures remain evidence, not replacement golden data.
The runtime accepts dotless/empty-segment domains and broader Unicode host
characters, restricts task whitespace and does not fold VT/FF labels or filter VT
tag boundaries as the adjacent specification prose describes. The strict owned grammar keeps the
pinned specification's explicit domain/whitespace rules; these differences must
remain recorded instead of silently broadening or excluding conformance cases.
