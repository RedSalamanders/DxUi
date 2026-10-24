# Label

Static text in any type role, single-line or wrapped, optionally a mnemonic label for another control.

- **Consumer supplies**: `SetText`, `SetFontRole` (default `Body`), `SetAlignment`, `SetMultiline`, optional `SetTextColor` override and `SetMnemonicTarget`. `SetTypographyOverride` optionally overrides font size (6–96 DIP, normalized to 1/64 DIP), weight (100–900) and character spacing (−4–16 DIP); invalid values are rejected without changing the current override, and `nullopt` restores the font-role format.
- **Colors** (`ResolveLabelVisualStyle`): `text` unless overridden. Use overrides only with palette roles (`subduedText`), never literal colors.
- **Don't** clip or ellipsize: measure the text and size the label to it.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
