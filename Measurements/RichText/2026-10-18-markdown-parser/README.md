# Lossless Unicode Markdown root and lexical preparation

This qualifies the first owned parser stage and its opt-in RichTextEditor host
integration. It does **not** qualify complete CommonMark/GFM grammar, semantic
rendering/editing, the accepted extensions or frame performance. The active
[implementation plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md) remains open.

## Source and scope

Base commit: `ba6c999976ea632e9c8dcd8b783cdfbb2d53ff71`.
Qualified compiled-source fingerprint:
`EFBADBD8239B1918D1C31517B6EF2BD3B6B73D51F53AA1B39518F2C1372A5743`.
The frozen manifest records all 38 changed code/test/project/data/spec/doc inputs
before qualification and verifies their exact bytes at the end. Current plan and
guide closeout are prose-only changes, separately validated after qualification.

- [MarkdownSyntax.h](../../../include/DxUi/MarkdownSyntax.h) and the owned
  [C++ parser](../../../src/Controls/DxUi.MarkdownSyntax.cpp) retain exact UTF-16,
  root leaf-block ranges and context-free lexical candidates. Original delimiters,
  trivia, unknown constructs, line endings and NUL stay in source. Completed
  syntax is immutable; callers cannot construct an unparsed public shell.
- Current root recognition covers paragraphs, blanks, ATX/setext headings,
  thematic breaks and fenced code including unfinished input. Container/reference
  dependencies, HTML/indented blocks and complete inline precedence remain open.
- Source-backed and transformed lexical affinity handles named references that
  expand to several UTF-16 units. Lexical values are candidates: references and
  escapes inside code/HTML require context resolution before rendering.
- `MarkdownParser` owns one pending and one published request, resumes scanning
  cursors under an operation budget, coalesces exact keys and rejects equal-revision
  different-source/profile reuse. Failed Begin retains an existing pending request;
  failed Advance retires staging and retains the previous publication.
- `SetMarkdownAnalysisProfile` opts into native/embedded ControlHost turns.
  Current-only getters never drain preparation. Source transactions/history and
  source TSF/UIA ACPs remain independent of analysis; stale geometry is unavailable.
  Hiding, zero extent and resource/owner retirement cancel pending work. Pure
  completed analysis survives width/DPI changes and held readers can outlive a view.
- No parser runtime dependency, worker, periodic timer, HWND, device or swap chain
  is introduced. The default and completed formatted pixels remain the v0 preview.
  Existing gallery/catalog/styles are unchanged; enabled/disabled analysis pixels
  are compared exactly in the embedded fixture.

## Validation

All 19 ordinary suites pass in each x64 Debug, Release and ASan Debug profile:
57 complete unfiltered native scope receipts, zero capability skips. All six root
profile builds pass, including ARM64 Debug/Release/ASan cross-builds. ARM64 native
execution is not claimed. Eight isolated Debug/ASan allocation probes pass before
the ordinary suites, including complete ordinal parser failure traces. The fixed
matrix declares and completes 16 owned operations without timeout.

All 652 original CommonMark 0.31.2 and 672 strict GFM 0.29-gfm example **inputs**
pass exact-source/block/lexical partition checks without exclusions. These 1,324
checks are preservation tests, not expected-HTML conformance. All 2,125 authoritative
semicolon-terminated entity names match independently transcribed Unicode values.
Data, notices, pins and offline generators are under
[ThirdParty/Markdown](../../../ThirdParty/Markdown/README.md). Generation tooling
checks deterministic bytes, read-only checking, stale/missing outputs and all
modified/missing data/license inputs.

Existing geometry/rendering qualification also passes: 108 exact actual/reference
WARP PNG pairs per x64 profile, 324 pairs total across six fonts, two directions,
three transforms and three viewports. Geometry tolerance remains 0.125 DIP; pixel
comparison remains byte-exact. An independent replay verifies all 57 native
receipt/artifact bindings and all 324 pairs. Native/embedded analysis-enabled
source/UIA, pending-range geometry and foreign-thread dispatch cases complete.

Parser C++ allocator measurements (not process/SDK/GPU memory):

| Profile | Begin/Advance failure points | Pending Begin failure points | Maximum retained bytes | Replacement peak bytes | Released delta | Unchanged/retire allocations |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Debug | 41 | 7 | 11929181 | 26316417 | 0 | 0 |
| Release | 38 | 4 | 11929085 | 26316217 | 0 | 0 |
| ASan Debug | 41 | 7 | 11929181 | 26316417 | 0 | 0 |

The maximum fixture has 65,536 physical newline/source units, with a block and
lexeme for every unit. Retained storage is bounded to 24 MiB and repeated old/staged
replacement to 32 MiB; all tracked storage releases. Same completed preparation,
read-only publication access and retirement allocate no C++ storage. Scanner work
is bounded by twice the source-unit count plus three; source admission/copy,
comparisons and allocator work remain indivisible and have no hard latency claim.

The five validators, complete repository tooling tests and pinned clang-format
22.1.3 checks pass. Post-closeout validators/format/scope accounting are retained
separately. These checks do not substitute for the remaining interactive/manual
or native-platform gates.

## Original attempts and performance limits

First: Release built; Embedded, Accessibility and WindowHost passed. The new native
fixture failed because it requested an active native OS text-store session without
establishing OS focus. The correction tests the protected source adapter without
taking the desktop. The original failed predicate, source, receipts and exact
artifacts remain retained. Second: the corrected fixture and immutable-construction
boundary passed all four affected Release suites. Third: additional pending-Begin
failure/replacement-peak guards passed the focused Release resource suite. The final
matrix reruns complete ordinary coverage on the final frozen source.

Canonical five benchmark inputs, methods, baselines, judges and thresholds remain
unchanged (input digest
`E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E`).
All new raw canonical reports are unpaired diagnostics; ambient compiler activity
precludes a quiet-host claim. No passing repeat establishes a causal performance
fix, and the prior clear/readback/scheduler/frame-performance HOLD stays open.
Prior Ghidra/parser research is copied as previously observed reference evidence,
not a new Ghidra session or new installed-Office version claim.

Full grammar/HTML conformance, stable dependency-aware semantic nodes, generated/
reordered projection anchors, semantic editing, GFM/math/Mermaid/footnotes/front
matter, end-to-end NUL editing/codecs/clipboard/text services, hard latency/capacity,
physical IME/AT and native ARM64 execution remain open. The raw parser accepts NUL;
the existing Document/editor/codec admission still rejects it. Capacity remains
65,536 UTF-16 units. No standalone consumer pin or application was updated.

## Replay and preservation

Run `./Verify.ps1` from this directory with PowerShell 7. It checks sealed bytes,
source/data hashes, receipt/build/canonical bindings, suite/test completion,
all exact raster pairs, allocation traces/storage predicates and the original
negative attempt. It replays recorded evidence without rebuilding or rerunning
the original Windows executables. Original binaries/PDBs/deployed runtimes remain
in recorded private `.build` artifact directories; no ordinary build product is
published here. `inventory.receipt.txt` maps source paths, hashes and bounded
restore-path aliases. `SHA256SUMS` seals every packet file except itself.

One-byte changes to a disposable native-receipt copy and a disposable source copy
are rejected at the expected hash predicate, with originals unchanged. A Git-only
staged archive replay checks the bytes that will be committed. The final seal and
local commit identity are retained privately after this packet is sealed.

The first post-closeout documentation check found the missing explanation in the
Performance directory. That explanation was added without altering any report,
compiled input or native result. Its original failed validator output and receipt
remain under `Review/FailedDocumentChecks`; successful repeated document checks
are separate records.

The first staged-tree check also caught saved `.gitattributes` files acting as
active Git policy inside archived source snapshots. Its original staged tree,
whitespace output and specific hash-replay rejection are retained privately and
under `Review/StagedFirst`. Those four policy files now use inert receipt names;
their bytes and original work paths are unchanged and mapped in the inventory.
Raw CRLF metadata is preserved; the final staged whitespace check recognizes CR
as a line terminator while retaining the ordinary whitespace checks. No canonical
code, test, data input or Git configuration was changed for this repair.
The index also required explicit raw renormalization after removing those active
policies; its second specific staged hash rejection is preserved separately under
`Review/StagedSecond` before the final passing Git-only replay.
