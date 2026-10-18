# Markdown research and standard data

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
under the included CC-BY-SA notices; the generated test source is a mechanical
UTF-16 transcription of the original example inputs. GFM visible-tab arrows are decoded to tabs according to the upstream spec example convention; the original spec bytes are unchanged. HTML expectations remain
in the originals for the future independent renderer/conformance gate.

Run these from the repository root; generation and checking need no network:

```powershell
./Tools/Generate-MarkdownEntityData.ps1 -Check
./Tools/Generate-MarkdownTestData.ps1 -Check
./Tools/tests/Test-MarkdownData.ps1
```

Both generators require exact data and license hashes before writing anything.
Tests cover deterministic regeneration, read-only verification, stale/missing
output and every modified/missing input or notice. Native tests currently check
lossless partitions of every original corpus input and all entity values. This
does not compare HTML or establish complete CommonMark/GFM grammar conformance.
