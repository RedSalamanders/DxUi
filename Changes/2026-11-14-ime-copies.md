- Stop copying the whole document on every TSF call. A `TextField` and an editable `ComboBox` now keep their text in
  one immutable shared buffer that the native host's text-input cache, the TSF store and undo history share; reads,
  selection changes, extents, synchronization and caret moves no longer copy it, and only an edit makes a new buffer.
  At about 64 Ki units a TSF lock in a `TextField` went from up to 33 document-sized allocations (about 5.6 MiB per
  composed syllable) to at most one. Commit keeps its double import and single undo step; masked fields keep TSF
  deactivated; the prefix/suffix `TS_TEXTCHANGE` notifications are unchanged. The last holder of a buffer zeroes its
  whole allocation before freeing it, and a growing edit copies first, so no block of shared text leaves unwiped. A
  derived `TextField` or `ComboBox` that overrides `ExportTextInputState` or `ImportTextInputState` is always called
  through them, under TSF as under IMM; one that keeps both can opt in to sharing through the new protected virtual
  `KeepsDefaultTextInputState`. A text control reads empty text, never a missing buffer, while it is destroyed. An
  editable `ComboBox` now filters its popup and matches its selection by committed text: a composition preview no
  longer rebuilds the open popup, and the commit filters by the result; a preview left by focus loss or a store
  teardown becomes the committed text, and the popup and selection follow it. `RichTextEditor` still flattens its
  document per TSF import (its snapshot export is separate work).
  - Validation: the counted IME copy gate with the override, opt-in and preview-settling cases
    (`DxUi.EmbeddedTests.exe --test-ime-copies`, also in the Embedded suite) and the TextField-suite release-wipe,
    shared-holder and teardown-read tests in x64 Debug, Release and ASan Debug, with receipts under
    `Measurements/PerformanceAndMotion_2026-11-13/W8.5`. A real IME composition under `test.ps1 -Interactive` and the
    focus-taking NativeTextInput suite remain for the developer.
