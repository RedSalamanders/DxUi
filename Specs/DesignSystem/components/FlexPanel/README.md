# FlexPanel

Opt-in row/column composition using measured child sizes. Existing manual panels retain their bounds.

- **Consumer supplies**: orientation, gap, padding, explicit wrapping, and child constraints/grow/alignment through `SetChildOptions`.
- **Custom controls**: provide an explicit extent or implement the optional measurement hook. A previous rectangle is not a preferred size.
- **Targets**: declare product minimums. Overflow remains observable rather than shrinking a critical action; use a scrolling body or change the responsive tier.
- **Flow**: RTL mirrors geometry while retaining logical tab order. Fixed footer actions continue to use `ArrangeMeasuredActions`.
- **Chrome**: none. Dashed guides in the preview are explanatory.
- **Preparation**: at most 1,024 live children (a removed child frees its place) and 65,536 uncached UTF-16 shaping units per container pass. Repeated unchanged preparation reuses numeric metrics.

The preview is a static HTML rendition styled by `bundle.css`; the test-harness gallery is the authoritative Direct2D rendering.
