# Tree

Hierarchical list over a borrowed model with expand/collapse, badges and optional multi-select.

- **Consumer supplies**: a borrowed `ITreeModel`, optional `ITreeDelegate`, stable IDs for selection/expansion; `NotifyDataChanged` when data changes. For several selected rows, `SetMultiSelectEnabled(true)`, then read `GetSelectedItemIds` (visible order) and handle `OnTreeSelectionSetChanged`.
- **Metrics**: row 28, indent 16; badges min 28×16, max height 18, padding 16. Expander 240 ms, expansion 320 ms.
- **Colors**: as Grid rows; selected rows `selectionFill`/`selectionText`, unfocused `selectionInactiveFill`, hover `hoverFill`; rainbow mode tints rows by a stable hash.
- **Multi-select** (off by default; a tree that never enables it is unchanged): every selected row takes the selection colors, and only the focused row draws the 1 DIP `focusStroke` ring inset 1.5 DIP, even when it is not selected. Click selects one row and sets the anchor, Ctrl+click toggles, Shift+click (or Shift with a movement key) selects the visible range from the anchor, Ctrl with a movement key moves the focus alone, Ctrl+Space toggles the focused row and Ctrl+A selects every row. A right-click on a selected row keeps the selection. A reorder drag still names one row, and a press with Ctrl or Shift never starts it.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
