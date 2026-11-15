- UI Automation text ranges read the published snapshot's text through a view instead of copying the document at every
  call (`W8.4`). Each call pins the snapshot it resolved (`PinnedRangeText`), on whichever thread UI Automation calls it;
  a line request dispatched to the window's thread carries that pin in the request. Composition and conversion-target
  ranges still copy their field's text once when created and now share that copy with their clones, so a held range
  retains one field's text and never a snapshot, and reads it with no copy per call. `ExpandToEnclosingUnit(Word)` finds the
  unit around the position locally instead of scanning every word from offset 0, with the same span at every offset, and
  character expansion walks forward once from the nearest established grapheme boundary instead of reading the element
  three times. A native caret event reads the caret from the host's text-input cache instead of copying the whole
  text-input state; which events are queued and raised is unchanged. On the `FX6` word walk over 64 Ki units, the units
  visited went from 2,098,168 to 1,592 (at most 25 per step, from 64,536) and no provider call copies the document; the
  fixture now also counts document-sized allocations. New tests in `Embedded.Tests.UiaTextProvider.h` compare
  word and character expansion with the replaced scans at every offset of seeded Unicode texts, count copies on the FX6
  walk and on native caret moves with no client, and read through a range after the owner thread published two newer
  snapshots (run under ASan); held composition ranges are shown to release every older snapshot, and rich-editor caret
  moves to copy nothing. A provider path that met a detached window could end in `std::terminate` on allocation failure;
  it now takes the fatal allocation path with its context. Window-thread line requests still copy the live control text (`W8.3`); a real screen
  reader was not run.
