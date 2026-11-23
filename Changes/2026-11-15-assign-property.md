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
  Review fixes: a `ScrollPanel` re-clamps its scroll offset when its bounds change (and reports a moved offset), so a resize
  followed by an unchanged `SetContentHeight` no longer leaves the content scrolled past its end. `TabControl::SetTabClosable`
  republishes the tab items, whose rectangles the wider header moves, as it did before `Assign`.
  `SetAccessibleFormProperties` repaints no more than `SetAccessibleName`: it routes a semantic state change. An unchanged
  `ComboBox::SetSelectedIndex` or `ColorPicker::SetColor` deliberately leaves a scrolled popup, an editable selection and a
  part being edited as they are. The assignment gate also runs on a window host, where repainting and publishing are separate,
  with fewer composite exemptions, and the setter inventory rejects a setter that calls `Assign` but stores its property
  itself, and class-scope macros. `NumericStepper::SetLabel` and `SetUnit` arrange their parts only after `Assign`'s routing and a lifetime check again (an embedded measure callback that retired the stepper made `SetUnit` use it after it was freed), `TagPicker`, `TextField` font, read-only and password setters check the lifetime between steps that can run application code, and the setter inventory rejects an `onStored` that goes on after such a step.
