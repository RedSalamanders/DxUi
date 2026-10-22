# Rich editor presentation

Status: ACTIVE

The shared editor owns its frame, text viewport, typography, source highlights,
focus dimming and link decoration. Consumers provide policy through public
options and retain document, command and window chrome ownership.

- [x] Retain a baseline from main `b69f452f` before implementation.
- [x] Add validated appearance options, source highlights and focus dimming.
- [x] Add opt-in hover/caret link underline policy with default compatibility,
  high-contrast override and source-current UI Automation attributes.
- [x] Cover source/history invariance, native and embedded geometry, interaction,
  layout invalidation, high contrast and detached/hidden state.
- [ ] Update domain contracts, usage/design-system documentation and gallery.
- [ ] Run scoped validation and profile builds; record remaining qualification
  gates without treating focused checks as full repository acceptance.

The existing RedTilde pin `dd619ef0c5855307eb57758072658f2e3b777fd8` and current
main diverge from `f65e4a4b`; public headers are identical at both tips. The
candidate starts from main to retain newer correctness and tooling changes.
The consumer must explicitly adopt the tested candidate commit.

Source syntax ink now prepares parser-owned marker/delimiter/URL ranges with a
shared 256-step cooperative budget, incremental ordering and a 65,536-event
ceiling. Overflow publishes the complete ordinary Source view. List-marker
metadata covers nested, ordered, quoted, tab-indented and task list items.
True selection excludes syntax effects from its base glyph pass, preserving
selected pixels even with translucent fills and antialiasing.

Selection notifications cover programmatic, keyboard, text-service and UIA
changes plus pointer gesture completion. A sibling editor's pending preview
does not revoke this editor's current geometry: readiness is checked against
the exact owner/source/width/typography/resource key. Native and embedded
two-editor tests retain exact math ranges, bounds and navigation while rejecting
the editor's own stale source or reflow.

The clean baseline's x64 Release EditorControls, Accessibility and Embedded run
passed without skips before source changes. Rebuilt candidate diagnostics pass
the complete EditorControls suite, the focused native/embedded accessibility
cases, prepared Source pixel equality, dense native scheduling and source ink
capacity/work-budget tests. These diagnostics do not substitute for the final
canonical profile matrix, gallery review or separate performance/platform gates.
