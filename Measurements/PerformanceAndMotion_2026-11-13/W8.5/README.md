# W8.5 IME copies — document-sized allocations per TSF lock

Plan item W8.5 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior
in [input and accessibility](../../../Specs/UI/UI_InputAndAccessibility.md) ("Shared input text") and the gate in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md).

- **Scenario** (`Tests/Embedded/Embedded.Tests.ImeCopies.h`, the reusable shape for the plan's FX7 and FX2): a headless
  native `ControlHost` (no window, no focus taken) with a focused control and the production TSF store
  (`CreateNativeTextInputTextStore`), driven by in-process sink, range and composition-view doubles. The document is
  65,472 UTF-16 units (64 Ki less room for the composition) with the caret in the middle. Ten Hangul syllables are
  composed: syllable 1 is one read-write lock (`GetSelection`, `GetEndACP`, `InsertTextAtSelection`,
  `OnStartComposition`, `GetText`); syllables 2-10 are one read-write lock each (`GetSelection`, `GetEndACP`, `SetText`
  of the composition, `OnUpdateComposition`, `SetSelection`, `GetText`). Each is followed by one read lock (`GetSelection`,
  `GetEndACP`, `GetTextExt`, `GetText`). The commit lock is `SetText` of the result and `OnEndComposition`. The ComboBox
  scenario composes the same syllables into an empty editable ComboBox with four items and its popup open (F4).
- **Count**: the replaced allocation functions of `Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h` count every C++
  allocation inside `RequestLock` (including the store's commit to the control and the host synchronization) and, through
  its opt-in threshold, those of at least one document's bytes (130,944): a document-sized allocation. Popup rebuilds are
  the work counter `popups.comboBoxPopupRebuilds`. No timing is recorded.
- **Command**: `DxUi.EmbeddedTests.exe --test-ime-copies <receipt>` (also run, without a receipt, by the Embedded suite).
- **Source**: "before" is base `c128bdb1` plus the measuring test and the rebuild counter only; "after" is the W8.5 change
  on `9df9b1b3` (uncommitted at capture; the commit carrying these files is the source). Each receipt records its
  `baseCommit` and the SHA-256 of its fixture inputs. **Build**: x64, MSVC
  19.52.37025. **Adapter**: none (no rendering). **OS**: Windows 11 build 26300.
- **Files**: [before, Release](ime-copies-before-release.json); after: [Release](ime-copies-after-release.json),
  [Debug](ime-copies-after-debug.json), [ASan Debug](ime-copies-after-asan-debug.json). Each lists every lock.

| Release | Before | After |
| --- | --- | --- |
| TextField, document-sized allocations in the worst lock | 33 | **1** |
| TextField, per composed syllable (edit lock + read lock) | 44.9 allocations, 5.62 MiB | **1 allocation, 128 KiB** |
| TextField, one read lock (selection, end, extent, text) | 14 | **0** |
| TextField, commit lock | 33 (4.12 MiB) | **1** |
| RichTextEditor, worst lock / per syllable | 32 / 42.8 | 9 / 6 (remaining copies belong to W7.4) |
| RichTextEditor, read lock | 12 | 0 |
| ComboBox popup rebuilds before the commit / at the commit | 10 / 2 | **0** / 1 |

Debug and ASan Debug report the same counts as Release (bytes differ by allocator rounding only). The one allocation
left in a `TextField` lock is the edit itself: the new text, built once at its final size and then shared by the store,
the control, its undo history and the host cache.

The "before" run predates two fixture additions made while implementing: an uncounted read lock before syllable 1 that
asks for the caret extent (a shown field has a layout before anyone types; without it the first counted lock also
allocated the layout cache's first text copy) and the open popup in the ComboBox scenario. Neither changes what the
base code copies per lock: it allocated an exact-size layout-cache copy on every relayout either way.

`RichTextEditor` still flattens its document in its `ExportTextInputState`/`ImportTextInputState` and editor
transactions (6 document-sized allocations per edit lock, 9 at the commit); the plan assigns that snapshot export to W7.4,
so this item does not gate it. A real IME composition remains a developer check (`test.ps1 -Interactive`).
