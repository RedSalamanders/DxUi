# Pinned Unicode data

The shared UTF-16 text-element service implements the default extended grapheme
rules of [Unicode 17.0.0, UAX #29 revision 47](https://www.unicode.org/reports/tr29/tr29-47.html).
It is our C++ implementation, using the data below under the included Unicode
License V3. There is no Unicode runtime dependency or ANSI build.

`17.0.0/collection.receipt.txt` records each official download URL, byte count,
SHA-256 and collection time. The original bytes are retained without line-ending
conversion. They comprise GraphemeBreakProperty, DerivedCoreProperties (for
Indic_Conjunct_Break), emoji-data (for Extended_Pictographic), all original
GraphemeBreakTest vectors and the license. Changing a version requires an explicit
data/rule/test review; regeneration never downloads floating inputs.

From the repository root:

```powershell
./Tools/Generate-UnicodeGraphemeData.ps1
./Tools/Generate-UnicodeGraphemeData.ps1 -Check
./Tools/tests/Test-UnicodeGraphemeData.ps1
```

The generator verifies all five input hashes before generating 833 disjoint
property intervals and all 766 test vectors. Hangul LV/LVT is classified
arithmetically. Outputs are `src/Controls/DxUi.TextElementsData.h` and
`Tests/Controls/Scope.Tests.UnicodeGraphemeCorpus.h`. Check mode verifies them
without writing. The tooling tests cover deterministic bytes, read-only checking,
missing/stale output and every changed/missing input or license.

`TestUnicode17DefaultGraphemeCorpus` checks next/previous movement and nearest
caret snapping at every UTF-16 offset, and element counts, without corpus
exclusions. Separate malformed-input, long-context, retained-editor and UIA cases
exercise the integration. This pin concerns logical grapheme segmentation. Word
and sentence boundaries, normalization, visual bidi navigation, font shaping and
physical IME/assistive-technology qualification have their own contracts.
