- Block-incremental semantic projection (performance and motion plan W7.1): a `SemanticBuilder` with a current
  publication builds only the top-level blocks an edit touched and copies the unchanged blocks before and after them,
  shifted, so a one-character insert in a 64 Ki-unit document creates the elements and display units of the edited
  block instead of the whole document. The publication equals a fresh build (checked by a seeded edit fuzz and the
  pixel oracle). Footnotes, edited link reference definitions, front matter or abbreviation definitions, admission
  limits a copy could change, scoped source presentation and unpaired syntax rebuild the whole document and count the
  cause. `SourceDisplayMap` no longer keeps a copy of the source or two anchors per source unit: a semantic
  publication's map borrows the syntax source and holds only its display text and spans (plus the source order of its
  primary spans when the projection reorders them); answers are unchanged. Semantic geometry pairs previous layouts
  with new paragraphs without an identity map and compares their input in place before copying it. The parser's
  identity map remains until incremental parse reuse.
