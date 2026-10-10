# Tree

Hierarchical list over a borrowed model with expand/collapse, badges and optional multi-select.

- **Consumer supplies**: a borrowed `ITreeModel`, optional `ITreeDelegate`, stable IDs for selection/expansion; `NotifyDataChanged` when data changes. Read selection membership from `GetSelectedItemIds` (visible order) and `OnTreeSelectionSetChanged`; `GetFocusedItemId` and `OnTreeFocusedItemChanged` report independent keyboard focus.
- **Metrics**: row 28, indent 16; badges min 28×16, max height 18, padding 16. Expander 240 ms, expansion 320 ms.
- **Colors**: selected rows `selectionFill`/`selectionText`, unfocused `selectionInactiveFill`, hover `hoverFill`; rainbow mode tints rows by a stable hash. Inactive text keeps `text` when it reaches 4.5:1 against the composited row fill, otherwise uses the more contrasting pure black or white. High contrast keeps the supplied `selectionText` exactly.
- **Selection and focus**: a tree that never enables multi-select remains single-select. Focus-only movement preserves selection in either mode; removing the selected row leaves an empty single selection while retaining focus. Adding a different item to an occupied single selection is refused, and adding the same item is idempotent. Selection callbacks report membership changes; the focused-item callback reports focus changes. In multi-select, every selected row takes the selection colors, and only the focused row draws a 1 DIP ring inset 1.5 DIP, even when it is not selected. The ring keeps `focusStroke` when it contrasts with the resolved painted fill, falling back to the more contrasting pure black or white; high contrast uses `selectionText` on selected rows. Click selects one row and sets the anchor, Ctrl+click toggles and moves the anchor, Shift+click (or Shift with a movement key) selects the visible range from the anchor, Ctrl with a movement key moves focus alone, Ctrl+Space toggles the focused row and Ctrl+A selects every row. A right-click on a selected row keeps the selection. A reorder drag still names one row, and a press with Ctrl or Shift never starts it.
- **Flow direction**: RTL mirrors expander, icon, badge and text layout; pointer expander targeting and left/right group traversal follow the mirrored direction.

## Optional prepared accessibility rows

`ITreeModel::CapturePreparedAccessibilityRows()` can return a shared immutable source for the current visible-row
semantics. The source includes every visible row, including offscreen rows. Its reads are bounded and allocation-free:

```cpp
[[nodiscard]] std::shared_ptr<const DxUi::IPreparedTreeAccessibilityRows>
    CapturePreparedAccessibilityRows() const noexcept override
{
    return preparedRows_; // Retain an already prepared source; do not build or traverse it here.
}
```

Return an empty pointer to use the existing row-by-row capture through `GetVisibleItemCount()` and
`GetVisibleItem()`. The source's `GetCount()` must match the model's visible item count; a mismatched source is
discarded and the existing capture path is used. `GetItem(visibleIndex)` and `FindItem(itemId)` return an
`std::optional<TreeAccessibilityItemView>`; missing indexes or IDs return an empty optional. Each view reports its
visible index, stable item ID, text, depth, child presence, and expanded state.

Treat shared source identity as a semantic epoch. Keep the same source while the ordered visible rows and each row's
ID, text, depth, `hasChildren`, and `expanded` values are unchanged. Publish a new source identity when any changes,
including a sequence reorder or a same-count replacement. Selection is independent and does not require a new source.
The identity lets accessibility snapshots invalidate old Tree children without traversing rows to compare them.

Text is borrowed from the source. Keep its backing storage valid for every read while any shared reference to the
source can be read; storing row strings in the immutable shared snapshot is a straightforward way to provide this
lifetime. The consumer owns bounded, allocation-free query/capture behavior, source preparation and admission, and
the lane on which final source destruction occurs. Account for retained references released by foreign readers.
DxUi does not start a row-preparation worker or choose a preparation lane. The default null source preserves the existing behavior.
The [library qualification](../../../../Measurements/PreparedTreeAccessibility/2026-10-06/README.md) covers the source API and
notification/lifetime behavior; the consumer's admission, retirement and latency qualification remains separate.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.

The preview shows the default palette's resolved contrast colors. Custom fills, alpha and rainbow tints are resolved by the native control. This is a static HTML rendition styled by `bundle.css`; the gallery captures under Assets › Gallery are the authoritative rendering.
