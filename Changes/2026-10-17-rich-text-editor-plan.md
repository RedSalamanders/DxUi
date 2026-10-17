- Add the bounded Unicode rich-text preparation core to DxUi.lib with immutable
  piece-table transactions, source/display Markdown projection, WIL-owned
  DirectWrite state, caller font settings, explicit retirement and behavioral
  EditorControls/WARP tests. The standalone prototype consumes one exact clean
  DxUi pin through the normal props/targets; no reusable cpp copy is maintained.
  API revision stays 4 because these additions preserve existing declarations.
  Add the retained RichTextEditor v0 preview using shared ControlHost services in
  native/embedded hosting, with source selection/history/formatting, Unicode
  clipboard/input, bounded scrolling, lifetime tests and catalog/gallery/design
  guidance. Add D2d native/embedded source TSF/IME and Document/Text/Text2/TextEdit
  UIA adapters with history-preserving preview/rollback, mapped visible/range/
  caret geometry, stale-service and callback-retirement guards, protocol and real
  in-process client tests. Full Markdown/extensions and large-document/platform/
  performance qualification remain pending. Retain
  the unpaired existing-library pre-change baseline without calling it candidate
  acceptance or changing consumer pins in RedXe/RedSalamander.
