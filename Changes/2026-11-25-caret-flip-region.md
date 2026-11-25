- A caret blink flip now adds only the caret's ink to the paint region (performance and motion plan item W4.7,
  [paint region](../Specs/Core/Core_PerformanceAndResources.md#paint-region)). A focused `TextField`, editable `ComboBox` or
  `RichTextEditor` records, each time it paints, the ink rectangle of its caret in its own coordinates, worked out while the
  caret is hidden too; the blink tick adds that rectangle, mapped to the view like a painted extent (scroll offsets,
  viewports, clips), snapped outward to device pixels, in place of the whole control. On the embedded host a frame after a
  flip therefore repaints a few pixels: on `FX1` an editable field's flip wrote 12,388 pixels before (its extent) and now
  writes the caret's ink, and a flip of the 64 Ki-unit multiline field no longer repaints the whole view (480,000 pixels).
  Typing, selection, scrolling, layout and focus changes invalidate as before. A flip falls back to the control's extent
  where the ink is not known (no paint since focus, a rich editor focused on a rendered element, an open `ComboBox`
  popup) and for controls derived from the library editors. The window host still paints whole frames until `W4.9`
  presents the region there. The `FX1` receipts regenerate with fewer pixels per flip frame (`frame.pixelsPainted`) and
  fewer `Paint` executions on the multiline field.
