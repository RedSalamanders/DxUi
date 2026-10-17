- Add the bounded Unicode rich-text preparation core to DxUi.lib with immutable
  piece-table transactions, source/display Markdown projection, WIL-owned
  DirectWrite state, caller font settings, explicit retirement and behavioral
  EditorControls/WARP tests. The standalone prototype consumes one exact clean
  DxUi pin through the normal props/targets; no reusable cpp copy is maintained.
  API revision stays 4 because these additions preserve existing declarations.
  Retained RichTextEditor, full Markdown/extensions, TSF/UIA, gallery integration
  and large-document/platform/performance qualification remain pending. Retain
  the unpaired existing-library pre-change baseline without calling it candidate
  acceptance or changing consumer pins in RedXe/RedSalamander.
