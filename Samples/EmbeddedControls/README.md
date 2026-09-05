# Supplied-device consumer

`EmbeddedScene.h` creates Toggle and Slider through public headers. `Main.cpp` owns the WARP D3D11 device, window,
swap chain, render target and event-blocked message loop. The library owns only its prepared surface and shared
graphics resources. The example uses reduced motion and a fixed 480x240 client canvas for clarity; responsive AV
layout and host DPI policy belong to the consumer integration. Run the executable to interact, or pass
`--output image.png` for a headless rendering proof. `GraphicsFixture.h` contains fixture-only readback/PNG output.

This project references one DxUi project. External release consumers use the exact-pin props/targets documented in
[the consumption contract](../../Specs/Build/Build_ToolchainAndConsumption.md).
