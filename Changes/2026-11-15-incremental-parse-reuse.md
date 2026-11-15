- Incremental Markdown parse reuse (performance and motion plan W7.2): a `MarkdownParser` with a compatible previous
  publication scans block lines, lexemes and inline leaves only from the top-level block enclosing an edit to the next
  stable cut. Imported top-level blocks now bring their inline forests (copied and shifted instead of parsed; every leaf
  is parsed again, counted, when the effective link reference definitions change), the document-level blank lines
  between them, and their identities without an identity map; an unchanged closed front-matter envelope keeps its
  validation result and is itself a reuse cut. The edit window is trimmed of unchanged units a conservative tracked
  range includes, the lexical margins stop at the edit's line, the owned source copy is composed from the previous
  publication plus the edited window instead of flattening the snapshot, and only the window's line breaks are counted.
  String callers get the same reuse from their prefix/suffix correlation. Two defects of the existing closed-root reuse
  are fixed: a block before the edit was reused although the blank line after it ended the document without a line
  break (typed text continued it), and a code or HTML block open at the end of the document lost the full parse's final
  insertion-line decision. Publications are unchanged: a seeded oracle of 22,452 edits in Release compares every
  incremental parse with a fresh parse, field by field. On `FX8` each scan of a one-character insert stays within the
  enclosing block plus the next (inline units per prose insert at 65,280 units: 48,420 before, 134 after) and no insert
  past a front-matter fence validates YAML or TOML. Imported records are still copied with shifted absolute ranges
  (`Nodes()`, `Inlines()`, `Lexemes()` and a contiguous `Source()` stay dense), so the per-edit copy remains
  O(document); removing it needs the public-type change deferred to API revision 6. New work counters:
  `rich.markdownBlockUnitsScanned`, `rich.markdownFrontMatterValidations`, `rich.markdownFrontMatterValidationsReused`,
  `rich.markdownInlineFullReparses`, `rich.markdownInlineLeavesCopied`, `rich.markdownInlineLeavesParsed`,
  `rich.markdownInlineUnitsScanned`, `rich.markdownLexicalUnitsScanned`, `rich.markdownNodesCopied`,
  `rich.markdownParses`, `rich.markdownSourceUnitsCopied`, `rich.markdownSourceUnitsMaterialized`.
