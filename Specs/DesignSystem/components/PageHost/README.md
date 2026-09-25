# PageHost

Hosts one root page and animates replacement with a connected transition.

- **Consumer supplies**: the new root via `SetPage(unique_ptr<Control>, connectedAnimationKey)`; the containing host advances animation ticks.
- **Motion**: 250 ms (`kPageTransitionDurationMs`); incoming page from +24 DIP, outgoing to −12 DIP; FastDecelerate easing. Reduced motion swaps immediately.
- **Don't** keep borrowed pointers into a replaced page.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
