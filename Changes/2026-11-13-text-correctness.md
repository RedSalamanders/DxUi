- Fix five text-correctness defects. An additive `Caret(position, CaretAffinity)` overload on `PreparedLayout` and
  `PreparedDocumentLayout` places a caret at a soft wrap at the end of the upper line (`Upstream`) or the start of the
  next (`Downstream`, the existing `Caret(position)`); the editor's ordinary layout carries that visual affinity, so
  `End` on a wrapped line shows the caret at the upper line's end, without the affinity entering source, text-service or
  UIA ACPs or undo. `TokenizeBuiltinCode` keeps the first 4,096 tokens of a longer block coloured instead of dropping
  all colouring. Double-click word selection decodes surrogate pairs before classing, so supplementary letters such as
  U+20000 U+20001 select as one word. `StatusStrip` leading elision chooses a grapheme-safe cut from one layout's
  cluster metrics instead of a binary search of layouts that could start inside a cluster. Multiline `TextField`
  paints its text and selection at the unsnapped text width its caret and hit-testing use, so they wrap identically at
  125% and 150%.
  - Validation: new cases in `Controls.Tests.RichTextLayout.cpp`, `Controls.Tests.RichTextEditor.cpp`,
    `Controls.Tests.TextElements.cpp`, `DxUi.Tests.Controls.cpp` and `DxUi.Tests.MultilineText.cpp`. Gallery captures
    of the `StatusStrip` and multiline `TextField` tiles at 125% and 150% remain open: the gallery harness renders one
    scale only.
