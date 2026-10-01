- A `Tree` row that straddles the viewport's top or bottom edge no longer paints past the tree's frame. Before, rows
  were culled only when wholly outside the viewport, so a straddling row was drawn whole. A selected row half scrolled
  away by a thumb drag painted its fill up to a row height over the frame and the controls above, and a reorder marker
  on such a row escaped the same way. Rows, including those an expansion animates, are now clipped to the viewport, and
  the marker to 1 DIP past it, so an insertion line on the edge keeps its width. Wholly visible rows are unchanged.
  The gallery's Tree / Hierarchy tile, whose viewport cuts its last row, loses one pixel row of that row's badge edge
  that used to paint into the frame's inset. `TestTreeRowsStraddlingTheViewportEdgesPaintOnlyInsideIt` fails without
  either clip.
