# Button

Push button with eight variants: Standard, DropDown, Selector, Split, Hyperlink, IconOnly, Disclosure and Repeat.

- **Consumer supplies**: the caption (constructor), `SetOnClick`, and `SetOnDropDownClick` for Split actions. Set bounds before preparation.
- **Chrome**: 1 DIP `buttonBorder` on `buttonFill`, `radius-sm` (4); Selector uses `radius-md` (6). Hover blends to `buttonHotFill` over 140 ms; pressed uses `buttonPressedFill` and shifts the caption 1,1 DIP; disabled uses `buttonDisabledFill` and `disabledText`.
- **Primary**: fill `buttonPrimaryFill`; caption `buttonPrimaryText` — the library picks `selectionText` only when it reaches 4.5:1, else `text`. High contrast paints the exact selection pair.
- **Chevrons**: DropDown 20 DIP, Selector 24 DIP, Split segment 32 DIP, glyph U+E70D (fallback ▾ U+25BE).
- **Multiline**: 12 DIP horizontal / 8 DIP vertical text padding; never clip or ellipsize a caption — size the button to the measured text.
- **Do**: one Primary per surface. **Don't**: use Hyperlink for destructive actions.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
