- Text field paint (performance and motion plan W2.7). A multiline `TextField` lays its text out once per change of text,
  width, font role or typography and reuses that layout and its line metrics to paint (text, selection, caret), measure
  caret and range geometry, hit-test clicks and drags, scroll by the wheel and move by arrows, Home, End, PageUp and
  PageDown: on the 64 Ki-unit field of fixture FX1 a focused paint, a partial wheel notch and a PageDown create no text
  layout, where they created two, two and five. The fallback line height is measured only when no layout can be made. A
  reused single-line layout no longer clears and copies its text, a focused single-line paint allocates nothing, the
  secure wipe of a layout cache's text copy applies to masked fields only (including the first drop after unmasking), and
  a paint reads the text-input state only while a composition is active, once. Pixels are unchanged.
  Moving a multiline field without resizing it keeps its layout, and masking a field or hiding its revealed password
  releases its layouts of the plaintext at once instead of at the next paint.
