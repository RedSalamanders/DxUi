- Layout now runs only where something changed (performance and motion plan item W6.3,
  [layout invalidation scopes](../Specs/UI/UI_ControlsAndLayout.md#layout-invalidation-scopes)). A host preparation pass
  enters only the branches marked as holding layout, content or visual preparation work, and tells a `PageHost` apart by
  its painted-extent kind instead of a type test per control. A layout-affecting property assignment (a `Label`'s text,
  a `FlexPanel`'s gap) re-measures its control under every constraint its measure cache recorded: when every result is
  unchanged, its ancestors keep their measurements and are not laid out again; otherwise the check moves up one parent at
  a time. A child a `FlexPanel` arranges at `FlexChildOptions::explicitSizeDip` stops propagation. The check runs before
  the next preparation and before any measurement or measure revision is read, so a measurement made right after a
  setter sees the new size; direct `InvalidateMeasure` calls, visibility, attachment and detached controls stay eager.
  `FlexPanel::ApplyLayout` checks its siblings once per pass instead of after every child, and a `Label` keeps its
  prepared layout on a position-only move. On `FX12` form-layout a same-size `Label` text change went from 1,205
  preparation visits, 3 ancestor arrangements and 107 arranged children per frame to 1 visit and none (x64 Debug, WARP,
  window host); 1,000
  position-only moves of typed `Label`s on `FX12` typography-120 went from 1,000 text layouts created to 0. New work
  counters `frame.treeVisitsPreparation`, `layout.intrinsicArrangements`, `layout.measureCutoffs` and the Debug
  verification counter `layout.scopeVerifyMismatches` (a whole layout compared after each pruned pass, enabled by
  `DXUI_LAYOUT_VERIFY=1`; `DXUI_LAYOUT_SCOPES=0` turns the scopes off in a Debug test program); `FX12` gains a last
  phase, "move". The incremental-versus-fresh oracle runs a new `layout-scopes` script on both hosts. Open: on `FX11`
  the frames of a `Tree` expand animation still create text layouts after the first (28 at 30 Hz), from the host layout
  table's box key while the rows slide.
