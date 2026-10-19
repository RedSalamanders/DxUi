# Supplied-device playground

`EmbeddedScene.h` creates a generic effect toggle and intensity slider through public headers. Its model is local
sample state; it never changes audio, camera or application settings. `Main.cpp` owns the WARP D3D11 device, window,
swap chain, render target and event-blocked message loop. The library owns its prepared surface and shared resources.

Run the executable to interact, or pass `--output image.png` for a headless rendering. Add `--complex-ui` for the
[independent 83-control scene](../ComplexUi/README.md) used by the library benchmark. Both modes use fixed 96-DPI
canvases (480x240 or 1280x720); adaptive layout and full host bridges are outside this minimal sample.
`GraphicsFixture.h` contains fixture-only readback/PNG output. No consumer checkout or plugin is required.

Use `--rich-text` for a 980x720 editable Unicode CommonMark/GFM semantic preview
with headings, styles, tables (including empty/padded cells), quotes, tasks,
code and image alternatives. This mode enables the same text-service bridge as
`--text-input`; the switches are mutually exclusive. Empty source lines retain
their own caret geometry. HTML stays literal and destinations stay inert.
Table materialization and other structural commands remain pending; text edits
still apply to exact raw source ACPs. Add `--output image.png` for a deterministic
capture that drains cooperative preparation without taking desktop focus.

This project references one DxUi project. External release consumers use the exact-pin props/targets documented in
[the consumption contract](../../Specs/Build/Build_ToolchainAndConsumption.md). See [commands](../../docs/samples.md).
