- Fixed a fatal failure when a RichMarkdown callout's first paragraph contains inline HTML that the native subset does
  not accept, such as `>[!tip]` followed by a line `<s>`: the literal fallback of that paragraph repeated the header line
  the callout had already shown as its title, giving the header's source two display owners, and the semantic map
  refused it. The literal now starts after the header line, where ordinary text in the body would. Found by the W7.1
  edit fuzz; the defect predates it.
