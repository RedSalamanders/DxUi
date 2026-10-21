# FormPanel

An opt-in form body with one or two application-selected field columns and labels above or beside editors.

- **Consumer supplies**: unique field IDs, column count, label placement, gaps/padding and a bounded body rectangle.
- **Shared labels**: visible fields share a measured label column. Long labels, descriptions and errors wrap at the actual width using DirectWrite.
- **Responsive tiers**: the application selects one or two columns. Beside-label columns retain at least 108 DIP and expand for a larger editor minimum, including NumericStepper unit chrome; a smaller body reports overflow. Supply scrolling or a wider tier. Keep fixed actions outside the scrollable form.
- **Ownership and navigation**: changing columns or label placement never reconstructs editors or geometrically sorts tab order. RTL mirrors the columns and label/editor positions.
- **Metrics**: column and row gaps default to 12 DIP; padding defaults to zero. Supply a page/card inset explicitly.
- **Work**: at most 256 live fields (a removed field frees its place). Numeric caches include host typography, child revisions and exact represented wrap widths; unchanged forms do no new shaping, including their shared-label pass. A nonconvergent custom editor is refused before geometry changes after four width passes.
- **Chrome**: none. Use CardPanel for a themed card surface. Product draft/Apply/Cancel and persistence remain outside the form.

The preview is a static HTML rendition styled by `bundle.css`; the test-harness gallery is the authoritative Direct2D rendering.
