# NumericStepper

Numeric input with increment and decrement buttons, labels and units.

- **Consumer supplies**: range, `SetStep` / `SetLargeStep`, `SetDecimals`, optional `SetLabel` / `SetUnit` with widths, and `SetOnChange(NumericStepperChange)`. Typing previews; Enter, focus loss, the buttons and Up/Down (Shift: large step) commit; Escape cancels. `SetValue` is silent.
- **Accessibility**: the step buttons paint glyphs, so they carry names ("Increase" / "Decrease"); supply localized, complete names with `SetStepButtonNames`.
- **Right to left**: label, field, unit and buttons mirror; label and unit read right to left from their start (leading alignment).
- **Metrics**: height 32, buttons 22, gap 4.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
