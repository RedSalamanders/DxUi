- Record the runtime efficiency and rendering quality plan in `Specs/Plans/WIP/RuntimeEfficiency_2026-11-07.md`:
  the 7 November `longbridge/gpui-fast`/`anomalyco/rune` study, red-team reviewed on 8 November 2026 against
  `a7f276c` and `fb8cf94`. A 45-row evidence ledger removes draft items that were already closed (`EDIT-01`,
  `RENDER-01`/`02`/`05`/`14`, `SEM-02`, `MD-05`..`07`) or refuted (linear brush scans, per-icon WIC factory,
  `TagPicker` concatenation, the unsourced 6.10 ms menu step), corrects misread reference mechanisms, and flags the
  unproven `FLIP_SEQUENTIAL` partial-present assumption for a truth test before any reliance. Every item is gated on
  deterministic work counters and an incremental-versus-fresh WARP oracle while paired timing stays advisory. The
  plan orders phases P0-P8 and a measured visual track V1-V8 (springs, OKLab transitions, device-pixel hairlines,
  analytic shadows, DWM/self-blur popup materials, smooth scrolling, real connected animation, text glide
  stability), plus a clean-room `ppmpreetham/BUI` (`e8126ca`) review section (`B1`-`B4`, GPL-3.0 boundary,
  visibility-latched media clocks, per-corner radius masks and numeric scrubbing). D1, D2 and D4 are retained;
  revised D3 (one API revision 6 bundle, slab lifetime handle withdrawn) and new D5-D10 await developer
  confirmation. Plan only; no library, test or tooling behavior changes yet.
