# Tree

Hierarchical list over a borrowed model with expand/collapse and badges.

- **Consumer supplies**: a borrowed `ITreeModel`, optional `ITreeDelegate`, stable IDs for selection/expansion; `NotifyDataChanged` when data changes.
- **Metrics**: row 28, indent 16; badges min 28×16, max height 18, padding 16. Expander 240 ms, expansion 320 ms.
- **Colors**: as Grid rows; rainbow mode tints rows by a stable hash.

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
