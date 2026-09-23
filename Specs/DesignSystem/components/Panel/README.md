# Panel

Container that owns children with explicit bounds; paints no chrome of its own.

- **Consumer supplies**: children through `AddChild<T>` and every child's bounds in host DIPs. Use it as the root of fixed layouts.
- **Chrome**: none — it inherits the host ground (`windowBackground`). Derive a card from **CardPanel**, a toolbar from **Toolbar**, a stack from **StackPanel**.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
