- Add the incremental-versus-fresh oracle (W0.5). Two identical scenes run one seeded mutation script; after every
  step the fresh scene drops every retained cache through the new diagnostic `ControlHost::DebugDropRetainedStateForTest`
  and `EmbeddedHost::DebugDropRetainedStateForTest`, every registered cache must report 0 entries, and the frames must
  agree exactly in premultiplied BGRA on WARP, in every control's bounds and state and in the UI Automation tree.
  Scripts cover text, bounds, visibility, hover, focus, press, captured drags, typing, wheel scrolling, theme, DPI
  96/144/192, right-to-left flow, density, the `ComboBox` popup, the frozen animation clock, detaching a hovered control
  and rich-document edits; a failure prints the script, seed, step, pixel and control and how to replay the seed alone.
  Retained caches are rows of the private registry `src/Controls/DxUi.RetainedCaches.cpp` (entries count and drop from
  one row, or an exemption with its reason), and `Tools/tests/Test-RetainedCacheRegistry.ps1` refuses an unlisted
  cache-named member. `EmbeddedHost::DebugReadSurfaceForTest` reads the view's surface exactly. A private test seam
  injects a stale `Label` layout as the negative control.
  - Validation: Rendering-suite tests `TestNativeRetainedCachesDropToZero`, `TestNativeIncrementalParityDetectsAStaleCache`,
    `TestNativeIncrementalParityLayout`, `TestNativeIncrementalParityInteraction`, `TestNativeIncrementalParityRich` and
    the embedded equivalents. Native partial frames remain unqualified until the presented-frame reader of W0.6 exists:
    the native oracle reads full-frame back buffers.
