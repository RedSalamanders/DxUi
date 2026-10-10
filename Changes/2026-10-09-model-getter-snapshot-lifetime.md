- Abort unpublished accessibility snapshots when reentrant Tree or Grid model getters retire their control or change
  its model, including navigation, hit geometry and focused-item lookup. Revalidate action-side model reads and
  resolve disabled Tree/Grid child actions through their actual owner. Retired Grid records report unavailable.
  Share a private borrowed-model guard across Tree, Grid and accessibility; a mutation revision rejects pointer
  restoration and same-model reentrant notifications as well as physical retirement and model replacement.
  Reacquire parent child storage across paint, tick and hit-test callbacks; restore page/scroll paint state and reject
  retired host hits or partial root-replacement frames. Keep failed collections out of selection commits, preserve
  both selection buffers on failed growth, and contain standard exceptions at native dispatch and geometry boundaries.
  Distinguish completed notified checkbox/expansion edits from replaced model bindings so lifetime checks preserve
  successful UI Automation actions while rejecting retired requests. Empty Grid painting emits metrics without a
  borrowed-model callback during cleanup.
  Propagate model changes into host geometry invalidation so a partially painted embedded frame cannot publish
  new row hit geometry against incomplete pixels. Re-resolve stable row/column identities after a selection delegate
  notifies a reorder in the same binding.
  Defer page bounds installed during paint until the next preparation, including outgoing transition pages, and
  reject partial page-replacement frames. Check the accessibility rebuild budget at its lazy first-query boundary.
  Reuse the guarded parent traversal for StackPanel layout without allocating a snapshot, and preserve a nested
  completed layout after child bounds callbacks retire the tree or change its layout inputs.
  Reject partial native frames after supported child clearing, extraction, reparenting, visibility or enabled-state
  changes under an unchanged root, and schedule corrective painting for attachment changes. Include delegate
  replacement in host geometry invalidation for Tree and Grid. Preserve the single-select UIA Add rejection while
  testing child selection replacement through Select.
  Invalidate flow/density and Tree/Grid metric changes, and capture host geometry in borrowed queries. Recheck input
  after hover and hit callbacks so the current click cannot reach a newly mirrored cell before repaint. Dispatch each
  captured embedded event once, preserving draft continuation after sibling layout. Keep color-only dirty input
  available outside paint, and reject an in-paint palette replacement.
  Bound shared UI Automation test-client provider requests while retaining the existing startup deadline and event assertions.
