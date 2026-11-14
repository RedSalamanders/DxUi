# W7.4 Text-input sync and flatten sites — rich document copies per input event

Plan item W7.4 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior
in [input and accessibility](../../../Specs/UI/UI_InputAndAccessibility.md) ("Rich text-input snapshots") and the counters
in [performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md) ("Rich documents").

- **Scenario** (`Tests/Embedded/Embedded.Tests.RichTextInputCopies.h`, a reusable `Fixture` for the plan's FX8 shape): a
  `RichTextEditor` of 64,512 UTF-16 units (CRLF lines of words, punctuation and a `C:\src\path`) in an `EmbeddedHost` on
  WARP at 640 x 480, focused, its layout prepared. Three presentations: `rich` (the default, Rich view without Markdown
  rendering: the v0 projection), `source` (Source view) and `semantic` (Rich view with Markdown rendering). Each operation
  starts with the caret at line 400, column 8, and the layout current. One input event is the dispatch
  (`DispatchCharacter`, `DispatchKey`, or `OnMouseDoubleClick` at the caret) plus `CommitFocusedTextInput`, the host's
  synchronization after dispatch, so both synchronizations a native host runs per event are inside the count. The
  preparation that follows, up to a current layout, is counted separately.
- **Count**: the work counters `rich.documentBytesCopied` (bytes of the document's source or display text copied into a
  string) and `rich.documentFlattens` (copies of the whole non-empty source or display). `SourceSnapshot::Slice` copies its
  result twice, and both copies are counted. The IME part is the W8.5 scenario (`Embedded.Tests.ImeCopies.h`,
  `RunRichTextEditor`: ten Hangul syllables and a commit through the production TSF store at 65,472 units), counting
  document-sized C++ allocations per lock.
- **Commands**: `DxUi.EmbeddedTests.exe --test-rich-text-input-copies <receipt>` (also runs the behaviour tests; the
  Embedded suite runs both without a receipt) and `--test-ime-copies <receipt>`.
- **Source**: "before" is base `3c02ba4f` plus the counters and the measuring test only (the counters then did not yet
  count the host's public text-input copies, which the overlay rebuild made per keystroke, so the "before" bytes are a
  lower bound); "after" is the W7.4 change (uncommitted at capture; the commit carrying these files is the source),
  on `3c02ba4f` for Debug and rebased on `8a25e391` for Release and ASan Debug. Each receipt records its `baseCommit` and the SHA-256 of the fixture inputs as committed; the "before"
  capture ran an earlier revision of the measuring file with the same scenario and without the gate checks and
  behaviour tests.
  **Build**: x64, MSVC 19.52.37025. **Adapter**: WARP. **OS**: Windows 11 build 26300. No timing is recorded.
- **Files**: [before, Debug](before-x64-debug.json); after: [Debug](after-x64-debug.json),
  [Release](after-x64-release.json), [ASan Debug](after-x64-asan-debug.json); IME copies:
  [before, Debug](ime-copies-before-x64-debug.json), after [Debug](ime-copies-after-x64-debug.json),
  [Release](ime-copies-after-x64-release.json), [ASan Debug](ime-copies-after-x64-asan-debug.json). The counts are the
  same in every configuration.

Bytes copied / whole-document flattens per input event (gate: insert and arrow at most 1,024 bytes; 0 flattens for every
row), and in the preparation that follows (not gated):

| Presentation | Operation | Event before | Event after | Preparation before | Preparation after |
| --- | --- | --- | --- | --- | --- |
| rich | insert | 516,104 / 2 | **0 / 0** | 259,008 / 1 | 956 / 0 |
| rich | arrow right, left | 516,104 / 2 | **0 / 0** | 258,052 / 1 | 0 / 0 |
| rich | Backspace | 516,096 / 2 | **0 / 0** | 259,000 / 1 | 952 / 0 |
| rich | Ctrl+right, Ctrl+left | 643,464 / 3 | **258–260 / 0** | 258,048 / 1 | 0 / 0 |
| rich | Ctrl+Backspace | 643,448 / 3 | **260 / 0** | 258,984 / 1 | 944 / 0 |
| rich | Enter | 774,136 / 3 | **0 / 0** | 259,000 / 1 | 952 / 0 |
| rich | double-click | 643,462 / 3 | **512 / 0** | 258,048 / 1 | 0 / 0 |
| source | insert, arrows, Enter | as rich | **0 / 0** | as rich | as rich |
| source | Backspace | 516,100 / 2 | **4 / 0** | 259,000 / 1 | 952 / 0 |
| source | Ctrl+arrows, Ctrl+Backspace, double-click | as rich | **as rich** | as rich | as rich |
| semantic | insert | 516,104 / 2 | **0 / 0** | 516,104 / 2 | 258,052 / 1 |
| semantic | arrow right, left | 516,104 / 2 | **0 / 0** | 258,052 / 1 | 0 / 0 |
| semantic | Backspace | 516,100 / 2 | **4 / 0** | 516,096 / 2 | 258,048 / 1 |
| semantic | Ctrl+right, Ctrl+left | 641,808 / 3 | **0 / 0** | 258,048 / 1 | 0 / 0 |
| semantic | Ctrl+Backspace | 516,080 / 2 | **0 / 0** | 516,080 / 2 | 258,040 / 1 |
| semantic | Enter | 516,112 / 2 | **0 / 0** | 516,112 / 2 | 258,056 / 1 |
| semantic | double-click | 641,826 / 3 | **0 / 0** | 258,056 / 1 | 0 / 0 |

The flatten left in a semantic preparation after an edit is the Markdown parse's own source copy
(`MarkdownParser::TryBeginSource`, `DxUi.MarkdownSyntax.cpp`), which plan item W7.2 owns; it is not text input.

| IME copies, RichTextEditor | Before | After |
| --- | --- | --- |
| Document-sized allocations in the worst lock | 9 | **1** |
| Per composed syllable (edit and read lock) | 6 | **1** |
| Commit lock | 9 | **1** |
