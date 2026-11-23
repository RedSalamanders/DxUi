- Hover and hit index (performance and motion plan W7.9): a rich-text hover no longer scans every semantic element.
  Semantic geometry indexes its titled link, image and abbreviation spans and its hover-underlined link, footnote
  reference and backlink spans by display range when a preparation publishes, and a hover title or underline lookup is
  one binary search; nested, overlapping, adjacent, empty, wrapped and right-to-left spans resolve exactly as the full
  scan did (seeded differential test). The host remembers the last resolved title with its point, modifiers and the
  editor's hover key (geometry generation, viewport, scroll, enabled, visible, editable) and resolves again only when
  one changes, so an animation tick with an unchanged pointer resolves nothing; edits, scrolling, zoom, width, density
  and re-preparation each make it resolve again (tested on both hosts). On a 601-element document, element scans per
  pointer move over a link fall from about 1,205 to 0 and title resolutions per unchanged-pointer tick from 2 to 0;
  plain-text moves already invalidated nothing. New `rich.hoveredTitleCalls` and `rich.hoverScanElements` work counters;
  the remembered title is a registered retained cache.
