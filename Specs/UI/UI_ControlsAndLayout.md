# Controls and layout

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Controls retain state independently of GPU resources. Buttons, toggles, sliders, selectors and text fields are shared
implementations, not application-local clones. An active gesture has a defined preview, commit and cancel lifecycle.
External state acknowledgement is separate from local intent. Hidden controls leave focus and accessibility trees.

Layout accepts explicit bounds/DPI, reports usable minimums and prepares coherent visuals/hit rectangles together.
Do not reduce a touch target below the consumer's minimum to hide an overflow. Labels and state values may not clip
or overlap. Optional content is removed before required controls. Geometry is recomputed on relevant changes only.
Consumers own density tiers and responsive policy; DxUi contains no AV profile or XENEON dimension rules.

The bootstrap only supports frame timing/stage and motion policy. The complete original control inventory is
preserved in `upstream/RedSalamander/Common/DxUI`; control extraction and behavior tests remain D1/D2 work.
