- Hosts now read the user's font smoothing (`SPI_GETFONTSMOOTHING`, `SPI_GETFONTSMOOTHINGTYPE`) explicitly and refresh it
  on `WM_SETTINGCHANGE`: the opaque `HwndSwapChain` keeps Direct2D's ClearType default and draws grayscale text when
  ClearType is off; the composition chain and embedded views stay grayscale. A window host binds its monitor's DirectWrite
  rendering parameters and reads them again on DPI, monitor and font-smoothing changes. The new
  `EmbeddedHost::SetTextRenderingMonitor` lets an application supply the monitor; without it an embedded view binds no
  parameters, as before. Parameters an antialias mode rejects are replaced with a compatible copy. New work counters
  `text.setTextRenderingParams` and `resources.renderingParams`; tests use `DebugSetFontSmoothingForTest` and never
  change the system setting. With an unchanged ClearType setting, FX5 captures in both presentation modes and embedded
  and all gallery sheets are byte-identical to before, and four text tiles in the light and dark themes at 100, 150 and
  200% on WARP are identical to the G1 gallery receipt.
