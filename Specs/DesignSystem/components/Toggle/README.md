# Toggle

On/off switch with a caption; use for settings that apply immediately.

- **Consumer supplies**: caption, `SetChecked` for initial state, `SetOnToggled(bool)` for user intent.
- **Metrics**: track 34×20 (`radius-pill`), knob 12 at rest / 14 hover / 10 pressed, inset 2; row padding 6, text gap 8; row background radius 8 (7 compact).
- **Colors**: off track outlined in `borderStrong`; on track `selectionFill` with `toggleKnobCheckedFill`. Row hover tint 5% light / 9% dark.
- **Don't** use a Toggle for choices that need a Save — use Checkbox.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
