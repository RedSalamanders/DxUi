- Keep `TextField` and editable `ComboBox` undo and redo by bytes (performance and motion plan W9.3). Each step now
  keeps only the units its edit replaced (the span between the old and new texts' common prefix and suffix) with the
  caret, selection and scroll state to restore, instead of a whole copy of the text; the newest step shares the text it
  started from until the next history operation seals it. History is bounded at 64 steps each way, as before, and at
  4 MiB per editor across undo and redo, the newest step's earlier text included, dropping the oldest steps first (undo before redo) and never the step just
  added, so an edit larger than the budget stays undoable while it is the newest. What one undo step is, and the text,
  caret, selection and notifications undo and redo restore, are unchanged: a 700-operation script on each control
  (typing, Backspace and Delete runs, word deletions, cut, paste, replacement, IME commits, previews left in place,
  ComboBox item commits, undo and redo) records the same state after every step as the full-text history did. The FX7
  and FX2 fixtures gain a receipt-only "history" variant of 128 edits on 64 Ki units: 7,234,416 and 7,143,776 bytes of
  history before, 132,080 and 130,336 bytes now (x64 Debug), within G13's 4 MiB text-history line after every edit, undo
  and redo.
