# Supplied-device playground

`EmbeddedScene.h` creates a generic effect toggle and intensity slider through public headers. Its model is local
sample state; it never changes audio, camera or application settings. `Main.cpp` owns the WARP D3D11 device, window,
swap chain, render target and event-blocked message loop. The library owns its prepared surface and shared resources.

Run the executable to interact, or pass `--output image.png` for a headless rendering. Add `--complex-ui` for the
[independent 83-control scene](../ComplexUi/README.md) used by the library benchmark. Both modes use fixed 96-DPI
canvases (480x240 or 1280x720); adaptive layout and full host bridges are outside this minimal sample.
`GraphicsFixture.h` contains fixture-only readback/PNG output. No consumer checkout or plugin is required.

Use `--rich-text` for a 980x720 editable Unicode RichMarkdown semantic preview
with headings, styles, tables (including empty/padded cells), quotes, tasks,
code and image alternatives. This mode enables the same text-service bridge as
`--text-input`; the switches are mutually exclusive. Empty source lines retain
their own caret geometry. Measured hanging markers align wrapped/continuation
text; links are underlined and keep inert destinations. The bounded native HTML
subset preserves original source; unsupported CSS/markup retains a literal leaf.
The two toolbar rows exercise lists/indentation, heading conversion, Markdown
styles, inert link/unlink, tasks, table creation and row/column edits through the
retained source-exact API. Numbered footnotes support repeated references,
reordered styled footer bodies and generated backlinks. Ctrl+click an internal
reference/backlink, or Ctrl+Enter at a reference, to select and reveal its exact
source target. External destinations and the sample Link action's `#sample`
target remain inert. Generated backlink keyboard traversal remains in development.
Select a compatible paragraph/item/cell first; unavailable actions explain that
target requirement. Enter continues a list or inserts a table row; Tab/Shift+Tab
navigates cells or indents/outdents lists, otherwise it traverses focus normally.
Ctrl+B/I toggles styles, Ctrl+Enter toggles tasks and Ctrl+Z/Y restores exact
source history. Task press/release toggles the source checkbox; dragging away
cancels it. Typing in padded cells materializes the focused cell only. Both task
states appear near the top of the document and use matching monochrome checkbox
metrics, keeping bullets and text aligned during toggling. All raw text services
retain exact source ACPs; structured/style accessibility remains
pending. Add `--output image.png` for a deterministic
capture that drains cooperative preparation without taking desktop focus.

This project references one DxUi project. External release consumers use the exact-pin props/targets documented in
[the consumption contract](../../Specs/Build/Build_ToolchainAndConsumption.md). See [commands](../../docs/samples.md).
