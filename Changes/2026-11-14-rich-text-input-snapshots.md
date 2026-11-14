- Stop flattening a `RichTextEditor`'s document for text input and editing keys. Text input now carries the editor's
  immutable source revision instead of a copied string: the native host's cache keeps it as the previous state,
  synchronization and the UI Automation text-changed check compare revisions in place, and the TSF store computes its
  prefix/suffix `TS_TEXTCHANGE` from the tracked changed range with the same result as on the whole texts. Contiguous text
  is made only for a reader that needs it (a TSF lock, IMM, a public text-input state), once per revision, and a TSF import
  reads the store's buffer in place and keeps it. In a 64 Ki-unit editor a one-character insert went from about 504 KiB
  copied and 2 whole-document flattens to none, an arrow key likewise, and Backspace, Ctrl+arrow, Ctrl+Backspace, Enter
  and a double-click flatten nothing (word boundaries read a window around the caret; the first line ending comes from
  the line summaries). A TSF lock in a `RichTextEditor` went from up to 9 document-sized allocations to at most one, and a
  composition commit stays one undo step. A native host no longer synchronizes text input a second time after a key or
  character when the control already synchronized the state it shows and the cache already holds every field it would
  store (password reveal state included); it still refreshes the caret, so a field that scrolls after synchronizing
  keeps its system caret in place. An unfocused editor keeps no text-input copy. The private copy counters attribute
  copies to the editor's own document (`rich.documentFlattens`) apart from other documents such as code-fence line
  numbers (`rich.otherDocumentFlattens`). New additive APIs: `SourceSnapshot::UnitAt` and
  `SourceSnapshot::LineStart`, and the protected virtual `RichTextEditor::KeepsDefaultTextInputState`: a derived editor
  that overrides `ExportTextInputState` or `ImportTextInputState` is always called through them; one that keeps both can
  opt in to the snapshot path.
  - Validation: the counted gate and its behaviour tests (`DxUi.EmbeddedTests.exe --test-rich-text-input-copies`, also in
    the Embedded suite: a seeded differential of every windowed read against the flattening code over CJK, emoji and ZWJ
    sequences, combining marks, CRLF, tabs, right-to-left text and long runs; the keystroke notification; filtering and
    opting-in subclasses on the TSF and embedded paths) and the IME copy gate, now gating `RichTextEditor`, in x64 Debug,
    Release and ASan Debug, with receipts under `Measurements/PerformanceAndMotion_2026-11-13/W7.4`. A real IME
    composition and the focus-taking NativeTextInput suite remain for the developer.
