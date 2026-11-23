- Pointer moves no longer repaint for a tooltip that has nothing new to show (`W1.6`). While a delayed tooltip waits
  for its show delay, `TooltipLayer::SetTooltipDelayed` stores a moved origin for the same text and reports no change,
  so the native and embedded hosts neither invalidate nor wake; the tooltip still appears at the last pointer origin.
  A `Tree` row's tooltip is now anchored where the pointer entered the row instead of following the pointer, and
  `Tree::OnMouseMove` no longer invalidates a second time after `ControlHost::SetTooltip` already did. Measured on the
  same fixtures before and after: 50 moves inside one control during the hover delay went from 50 to 0 native and from
  50 to 0 embedded invalidations; 50 moves within one `Tree` row went from 100 to 0, and crossing to the next row
  causes 1. `TestTreeTooltipTracksPointerWithinSameRow` encoded the old tracking and is now
  `TestTreeTooltipStaysAtRowEntryOriginWithinSameRow`. A shown delayed tooltip still follows the pointer, and `Grid`
  cell tooltips keep tracking the pointer; the gallery has no `Tree` tooltip tile, so no capture changed.
