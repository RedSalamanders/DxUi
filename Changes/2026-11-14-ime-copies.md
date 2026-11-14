- Stop copying the whole document on every TSF call. A `TextField` and an editable `ComboBox` now keep their text in
  one immutable shared buffer that the native host's text-input cache, the TSF store and undo history share; reads,
  selection changes, extents, synchronization and caret moves no longer copy it, and only an edit makes a new buffer.
  At about 64 Ki units a TSF lock in a `TextField` went from up to 33 document-sized allocations (about 5.6 MiB per
  composed syllable) to at most one. Commit keeps its double import and single undo step; masked fields keep TSF
  deactivated; the prefix/suffix `TS_TEXTCHANGE` notifications are unchanged; the last holder of a buffer securely wipes
  it. An editable `ComboBox` now filters its popup and matches its selection by committed text: a composition preview
  no longer rebuilds the open popup, and the commit filters by the result. `RichTextEditor` still flattens its document
  per TSF import (its snapshot export is separate work).
  - Validation: the counted IME copy gate (`DxUi.EmbeddedTests.exe --test-ime-copies`, also in the Embedded suite) in
    x64 Debug, Release and ASan Debug, with receipts under `Measurements/PerformanceAndMotion_2026-11-13/W8.5`. A real
    IME composition under `test.ps1 -Interactive` and the focus-taking NativeTextInput suite remain for the developer.
