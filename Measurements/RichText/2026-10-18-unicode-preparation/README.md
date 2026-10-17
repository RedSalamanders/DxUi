# Unicode boundary reference preparation

Reference/prototype evidence only. **No native Unicode 17 grapheme implementation
is included in this checkpoint.** Existing controls still use the previous
approximate combining-mark/ZWJ helper. This packet prepares the next indexed-plan
step while the current implementation's build matrix was running.

References retains exact official Unicode 17.0.0 GraphemeBreakProperty,
GraphemeBreakTest, DerivedCoreProperties/InCB and emoji-data bytes, plus the Unicode
license and original URLs/SHA-256 collection records. Generate-Data verifies all
five pins before deriving 833 disjoint property intervals and all 766 original
UTF-16 conformance vectors, without exclusions. Generated headers remain
reference artifacts; nothing includes them in the library or native tests.

Test-Algorithm runs a separate PowerShell rule prototype against
every original case. It matches all 766 vectors, including GB9c, emoji ZWJ and RI
context rules. This does not qualify the C++ implementation, DirectWrite hit/caret
geometry, controls, UIA, bidi/word navigation or physical input/AT.

Replay the actual prototype with fresh generated output:

```powershell
./Test-Algorithm.ps1 -ReferenceRoot ./references -GeneratedRoot <fresh-output-directory>
```

The parser and Unicode helper policies are separate: the planned boundary walker
is owned C++, using pinned public Unicode data. Source/license/provenance and
generator failure tests, a shared pure UTF-16 boundary service, full native corpus
execution, native/embedded behavior/resource tests and configuration qualification
remain required. Header comments anticipate the future canonical data directory;
these archived headers are not a second editable implementation.

Sources: [Unicode 17 data](https://www.unicode.org/Public/17.0.0/ucd/auxiliary/),
[UAX #29 revision 47](https://www.unicode.org/reports/tr29/tr29-47.html),
[Unicode license](https://www.unicode.org/license.txt).

