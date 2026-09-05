# Theme and typography

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Consumers provide colors and style tokens. Shared controls support light, dark, high contrast and reduced motion.
Retain Segoe UI typography and Fluent/MDL2/Unicode icon fallback where supported. Do not add a dependency on either
application's theme record or icon assets. Preserve the original notice for any later imported assets.

Prepare changed Unicode text/layout and glyph resources outside composition. Use real font metrics and clip to
control bounds without cutting essential ink. DPI changes rebuild appropriate resources; position-only movement
does not. Transparent-target antialiasing, alpha blending, mixed scripts and narrow bounds require visual tests.

The extracted MotionPolicy resolves immediately to the target when reduced motion is enabled and otherwise keeps
the caller's animated progress. Theme controls and text rendering remain pending the control/renderer extraction.
