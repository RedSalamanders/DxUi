# Tree

Hierarchical list over a borrowed model with expand/collapse and badges.

- **Consumer supplies**: a borrowed `IDxTreeModel`, optional `IDxTreeDelegate`, stable IDs for selection/expansion; `NotifyDataChanged` when data changes.
- **Metrics**: row 28, indent 16; badges min 28×16, max height 18, padding 16. Expander 240 ms, expansion 320 ms.
- **Colors**: as Grid rows; rainbow mode tints rows by a stable hash.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
