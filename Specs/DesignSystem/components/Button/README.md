# Button

Push button with eight variants: Standard, DropDown, Selector, Split, Hyperlink, IconOnly, Disclosure and Repeat.

`SetAppearance` independently selects Filled (default), Subtle, Outline or Tab. Use Subtle for quiet toolbars,
Outline for secondary inspector actions, and checked Tab for an underlined view selector. Subtle checked commands
still use selection fill/text; primary actions remain filled. Keyboard focus and system high contrast stay visible.
`SetLeadingIcon` and `SetShortcutText` decorate text buttons with a Fluent glyph and a keyboard hint. Measurement
reserves both lanes and RTL mirrors them; the consumer owns shortcut dispatch and the accessible command name.

- **Consumer supplies**: the caption (constructor), `SetOnClick`, and `SetOnDropDownClick` for Split actions. Set bounds before preparation.
- **Chrome**: 1 DIP `buttonBorder` on `buttonFill`, `radius-sm` (4); Selector uses `radius-md` (6). Hover blends to `buttonHotFill` over 140 ms; pressed uses `buttonPressedFill` and shifts the caption 1,1 DIP; disabled uses `buttonDisabledFill` and `disabledText`.
- **Primary**: by default the fill and caption derive from the current theme selection colors. `ThemePalette::primaryButtonFill` and `primaryButtonText` optionally override those colors for normal themes; hover and pressed fills derive from the supplied fill. High contrast keeps the exact system selection pair.
- **Optional toggle semantics**: `SetToggleState(std::optional<bool>)` opts a Button into UI Automation TogglePattern; `GetToggleState()` returns the consumer-owned state. `SetToggleState(std::nullopt)` removes the pattern. A checked non-primary button persistently uses the selection fill/text pair, including IconOnly buttons. The setter publishes a state change to UI Automation but does not replace `SetOnClick` or toggle state during invocation: the consumer handles the command and publishes its resulting state. Existing buttons are unchanged until opted in. Use `SetPrimary` separately for a primary action.
- **Theme preview**: `SetThemePreview` optionally shows three color swatches within a Standard button; custom swatches are suppressed in high contrast and system window/selection colors are shown. The control remains an ordinary accessible Button and may independently opt into TogglePattern.
- **Chevrons**: DropDown 20 DIP, Selector 24 DIP, Split segment 32 DIP, glyph U+E70D (fallback ▾ U+25BE).
- **Disclosure**: the chevron rotates right (or left) to down over 240 ms from its first animation tick, so a state changed while hidden rotates once shown, and a pause in the ticks pauses it; reduced motion snaps it.
- **Multiline**: 12 DIP horizontal / 8 DIP vertical text padding; never clip or ellipsize a caption — size the button to the measured text.
- **Content size**: `SetContentFontSizeDip` is optional and leaves the Body 13/Icon 12 DIP defaults unchanged when null. Finite 6..96 DIP values normalize to 1/64 DIP, with idempotent HRESULT validation. Use 14 DIP for larger chrome labels and 18 DIP for line icons; the host's UI/icon family and weight, intrinsic wrapping and cached painting stay consistent. Internal dropdown/disclosure chevrons keep their own metrics.
- **Do**: one Primary per surface. **Don't**: use Hyperlink for destructive actions.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
