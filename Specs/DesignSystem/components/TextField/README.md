# TextField

Single- or multi-line editable text with IME, selection and clipboard support.

- **Consumer supplies**: text and editing options; `SetOnTextChanged`, `SetOnSubmitted`, `SetOnBlur`. Full IME/native text needs the host bridge.
- **Metrics**: padding 8 left/right, 4 top/bottom; `radius-sm`; clear (U+E711) and reveal (U+E7B3) buttons 30 DIP wide; caret blink 530 ms.
- **Colors**: `inputFill` with `inputBorder`; hover blends the border 24% (light) / 32% (dark) toward `focusStroke`; focus paints `focusStroke`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
