- A multiline `TextField` draws its caret only inside its text viewport: a caret on a partly visible line was drawn past the
  viewport's edge, over whatever lies below the field. The caret's line is clipped to the viewport (`19ca44d` of the
  `codex/multiline-caret-viewport` branch). The Embedded suite's `TestMultilineCaretViewport`, from that branch, renders the
  caret on a WARP surface at 96, 144 and 192 DPI, editable and read-only, and requires no caret pixel outside the viewport;
  it failed before this change (20 caret pixels below a partly visible bottom line at 96 DPI). Specified in
  `UI_ControlsAndLayout.md`; `docs/controls.md` follows.