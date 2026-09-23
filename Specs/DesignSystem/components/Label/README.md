# Label

Static text in any type role, single-line or wrapped, optionally a mnemonic label for another control.

- **Consumer supplies**: `SetText`, `SetFontRole` (default `Body`), `SetAlignment`, `SetMultiline`, optional `SetTextColor` override and `SetMnemonicTarget`.
- **Colors** (`ResolveLabelVisualStyle`): `text` unless overridden. Use overrides only with palette roles (`subduedText`), never literal colors.
- **Don't** clip or ellipsize: measure the text and size the label to it.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
