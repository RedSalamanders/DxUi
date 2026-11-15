- One way to assign a property (performance and motion plan W2.10). `Control::Assign(field, value, affects[, onStored])`
  stores a property only when it changes (`AssignEquality`: floats exactly with NaN equal to NaN and -0 equal to +0, strings
  by content, pointers by identity, colours and rectangles member by member, optionals, tuples and ranges element by
  element) and then routes the declared `Control::Affects` once: `Hit` moves the pointer hit revision, `Layout` calls
  `InvalidateMeasure`, `Paint` invalidates and the semantic aspects raise one accessibility mutation through the host's
  client gate. 153 public setters of the library's controls now assign through it, and setting any of them again to the
  same value invalidates, measures, moves the hit revision and publishes nothing. Before, 48 of them repainted, relaid
  out, moved the hit revision, republished or re-synchronized their parts on an unchanged value, among them
  `ProgressBar::SetValue`, `Slider::SetMinimum`, the `ThroughputGraph` and `StatusStrip` setters, `StackPanel::SetGap`
  and `ComboBox::SetItems`, which also rebuilt its popup and reset its scroll. Setters whose readers saw stale state now
  route what reads them: `TextField::SetPlaceholder`, `SetCaretColor`, `SetMultiline` and `SetClearButtonEnabled`
  repaint; `TextField::SetReadOnly` and `SetMultiline` and `ComboBox::SetVariant` republish UI Automation;
  `Toggle::SetStateLabels` republishes the name and the `Splitter` limits republish its range; `StackPanel`'s
  orientation, gap, padding and child extents invalidate the measure; `Button::SetPressedVisual` repaints. Accessible
  names, help text, roles and connected-animation keys no longer repaint, and `ComboBox::SetChromeVisible` no longer
  invalidates the measure, since nothing paints or measures them. A control outside the library assigns its own
  properties the same way. `Tools/tests/Test-AssignSetters.ps1` requires every public setter to use `Assign`, register a
  callback or sit on a reviewed allow list of 66 entries with reasons, and the FX5 gate sets every assigning setter
  twice. Work counters add `invalidations`, `measureInvalidations`, `hitRevisionMoves` and accessibility `mutations`.
