- `ComboBox` measures each item's popup text width once and retains it (performance and motion plan W2.2): again only
  after `SetItems` or when the host's typography revision moves (a device replacement). The popup width is the maximum
  over the filtered items, recomputed without measuring when the filter changes; the popup placement is computed once
  per change of its inputs, and paint and hit testing derive rows from it arithmetically. A closed popup is not placed,
  so typing into a closed field measures nothing. On `FX2` (WARP) a first open measures each item once (500, then 100)
  and every hover frame, typed character and repaint of the 100-row popup measures none, from 11,000 to 20,600 text
  layouts per frame before; popup pixels, bounds and rows are unchanged (exact comparison with the previous measurement on
  seeded item sets, the new `combo-popup` incremental-versus-fresh script on both hosts, and the `ComboBox` gallery tiles
  at 100%, 150% and 200%). New work counters `popups.comboBoxItemWidthMeasurements` and `popups.comboBoxPopupLayouts`.
