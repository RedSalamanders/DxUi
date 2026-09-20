# Control guide

All controls below are declared in [DxUi.h](../include/DxUi/DxUi.h), available through
[ControlCatalog.h](../include/DxUi/ControlCatalog.h), and shown in the [gallery](gallery/README.md).
Use `Panel::AddChild<T>(...)` for typed construction or `CreateControl` for catalog-driven construction.
Set bounds, visibility, enabled state and content before preparation. Mutate controls/models on the UI thread.

| Control | Configure and use |
| --- | --- |
| Panel | Own children with `AddChild<T>`; set explicit child bounds. Use as the root for fixed layouts. |
| PageHost | Transfer a root with `SetPage(unique_ptr<Control>, connectedAnimationKey)`; the host retains transition state. Advance animation through the containing host. |
| CardPanel | A Panel with themed card chrome; position child bounds in host DIPs, including the card's origin. |
| Label | Construct with text or call `SetText`; choose a font role for headings/body text. |
| Button | Construct with its caption; `SetOnClick` handles activation and `SetOnDropDownClick` handles split-button actions. |
| Toggle | Set initial state with `SetChecked`; handle user intent with `SetOnToggled(bool)`. |
| Checkbox | Uses toggle checked-state/event semantics with checkbox visuals. `SetMultiline(true)` wraps long captions; reserve 36 DIP horizontally for indicator/gaps and 6 DIP vertically around measured Body text. |
| RadioButton | Configure selection state and `SetOnSelected`; use RadioButtons when selection is mutually exclusive. |
| RadioButtons | Own a group of choices and handle `SetOnSelectionChanged(int)`. |
| ProgressBar | Set minimum, maximum and value; `SetIndeterminate(true)` requires host animation ticks. |
| PageIndicator | Set `SetPageCount` / `SetSelectedIndex` for a bottom strip of dots. `SetOnSelected` reports user clicks and Left/Right/Home/End. Hidden and not hittable when there are fewer than two pages. Strip height is `kStripHeightDip` (20). |
| ThroughputGraph | Feed timestamped samples through its public sample API; keep history within its configured limits. Use the rendering tests for populated series and bands. |
| Slider | Set range/value and handle `SetOnChange(SliderChange)` for preview, commit and cancel. Painted chrome is a 6 DIP track, a 20 DIP gray disc, and an accent inner thumb (6 DIP rest, 16 hover, 12 pressed). Pointer hit testing is an unpainted 48 DIP band; a press within 24 DIP of the thumb center drags, otherwise the track seeks. Hover, press and keyboard steps animate; `SetValue` snaps. |
| Toolbar | Panel for command controls; populate buttons with labels, bounds and actions. |
| MenuBar | Supply `MenuBarItem` records through `SetItems`; handle `SetOnOpenItem` and hover changes. Native menu operations require the HWND integration. |
| TabControl | Add populated tab pages with `AddTab`; handle selection, close-request, closed and reorder callbacks as needed. |
| ColorSwatch | Configure the displayed color and handle `SetOnClick` to launch your color selection flow. |
| TextField | Set text/editing options; handle `SetOnTextChanged`, `SetOnSubmitted` and `SetOnBlur`. Full IME/native text behavior needs the appropriate host bridge. |
| ComboBox | Supply `Item` records with `SetItems`; use `SetOnSelectionChanged`, text/submission callbacks and popup requests for editable selection. For touch, call `SetMinimumPopupItemHeight(48.0f)`; the gallery Modern open variant uses it. |
| TagPicker | Call `SetOptions(allLabel, values)`, `SetSelectedValues` and `SetOnSelectionChanged` for multiple named choices. |
| StatusStrip | Use `SetText` for one value or `SetSections` and `SetSectionText` for multiple aligned status values. |
| PopupLayer | A Panel for overlay content; populate real children and control its visibility/lifetime with the owning host. |
| StackPanel | Set orientation, gap and padding; call `SetChildExtent` for every child and `ApplyLayout` after content or bounds changes. |
| ScrollPanel | Own a content tree, set the content extent, and handle `SetOnScrollChanged` if needed. It clips and translates pointer coordinates into content space. |
| TooltipLayer | Usually managed through `ControlHost::SetTooltip`, `SetTooltipDelayed`, hide-delay and `ClearTooltip`; delayed behavior needs host ticks. |
| Tree | Supply a borrowed `IDxTreeModel`, optional `IDxTreeDelegate`, then `NotifyDataChanged` when data changes. Use stable IDs for selection/expansion. |
| Grid | Supply a borrowed `IDxGridModel` and optional `IDxGridDelegate`; configure columns, row height and selection. Call `NotifyDataChanged` after model changes. |

## Editable values

```cpp
auto* slider = root->AddChild<DxUi::Slider>();
slider->SetMinimum(0);
slider->SetMaximum(100);
slider->SetValue(65);
slider->SetOnChange([&model](DxUi::SliderChange change) {
    model.preview = change.value;
    if (change.phase == DxUi::SliderChangePhase::Commit) {
        model.Commit(change.value);
    }
    // Cancel carries the restored value; update the displayed draft without committing it.
});
```

Keep preview separate from durable application commands. An asynchronous backend acknowledgement is separate from
local intent. Keep model/callback captures alive, and do not reuse borrowed control pointers after root replacement.
The [embedded scene](../Samples/EmbeddedControls/EmbeddedScene.h) is a complete typed event example;
[embedded tests](../Tests/Embedded/EmbeddedTests.cpp) cover drag cancellation and root changes during callbacks.

## Layout and data-backed controls

For a localized action group, measure each label with the current host font, add the desired padding
and minimum target size, then call `ArrangeMeasuredActions` with those sizes, available DIP width,
horizontal/vertical gaps and caller-owned output rectangles. The helper allocates nothing, preserves
input order, wraps whole actions and returns total height/row count. It supports right-to-left flow.
An omitted `{0,0}` size produces an empty rectangle without changing indices. Invalid inputs,
insufficient output space and arithmetic overflow leave all outputs unchanged.

An individual size must fit the available width. Measure overlong text wrapped first; standard
`Button::SetMultiline(true)` enables matching wrapped paint with 12 DIP horizontal and 8 DIP vertical
padding on each side. Checkbox also honors this inherited option, using its own indicator/text geometry described above. The default is false, preserving existing single-line behavior. Other button
variants retain their existing text/chrome policy. The helper does not choose actions, mutate focus,
activate a host or cache application state. Cache measurements in your layout owner and recompute on
text/font/DPI/width/visibility changes; apply the returned rectangles consistently to paint/input/UIA.
At short height, reserve the action group's measured height and scroll the remaining body separately.

A Button with `SetDisclosureExpanded(bool)` exposes UIA ExpandCollapse as well as Invoke. Repeated
requests for the already acknowledged state do nothing. A changed request invokes the normal click
callback; the application updates the state, content visibility and focus. `GetDisclosureExpanded()`
returns that acknowledged optional state; `ClearDisclosureState()` removes the pattern. Disabled
buttons reject UIA state changes. Embedded hosts publish the new snapshot after preparation, using
their existing accessibility update path. Retained providers disconnect when their control is removed.

```cpp
auto* stack = root->AddChild<DxUi::StackPanel>();
stack->SetBounds(D2D1::RectF(16, 16, 320, 160));
stack->SetOrientation(DxUi::StackOrientation::Vertical);
stack->SetGap(8);
auto* caption = stack->AddChild<DxUi::Label>(L"Name");
auto* field = stack->AddChild<DxUi::TextField>();
stack->SetChildExtent(caption, 24);
stack->SetChildExtent(field, 36);
stack->ApplyLayout();
```

Grid and Tree borrow model/delegate pointers; retain these until the controls are destroyed or pointers are cleared.
Grid models provide row/column counts, column descriptors, cell data and stable-row lookup. Tree models provide
the visible item sequence with IDs, depth and expansion state. Supply actual populated data, stable identity and
notifications; a factory-created empty control does not demonstrate interaction.
See [the benchmark model](../Samples/ComplexUi/ComplexUiScene.h),
[grid tests](../Tests/Controls/DxUiTests.Grid.cpp), [tree tests](../Tests/Controls/DxUiTests.Tree.cpp), and
[gallery construction](../Tests/Controls/DxUiTests.Gallery.cpp) for concrete configurations and variants.

When overriding `OnFocusChanged` in a derived control, call the base implementation before
handling application-specific detail. It acknowledges `HasFocus`, which drives visible focus and
UIA keyboard-focus properties. Test both the provider state and the painted focus outline.

Use keyboard/focus, cancellation, disabled and hidden states alongside pointer input. Verify high contrast,
Unicode, DPI and minimum layout bounds. Consult [hosting](hosting.md) for the embedded text/UIA limitations.

`ComboBox::SetMinimumPopupItemHeight` takes a per-instance DIP minimum for popup row painting, hit testing,
scrolling and keyboard visibility. Zero restores the theme default. Finite values from 0 through 4096 are accepted;
invalid values leave the setting unchanged. `GetMinimumPopupItemHeight` returns the configured minimum, not the
effective theme height. The field bounds are independent and should also meet the consumer's touch minimum.
The popup shows fewer complete rows when space is limited and can open above the field; it does not shrink rows.
Hosts must provide space on at least one side for a full row and the popup padding, or open the selector in a larger view.

In high contrast, primary Buttons use the exact opaque selection color pair for idle, hover, pressed and focused
states. Disabled Buttons retain a border with the disabled text color; focus is visible for pointer and keyboard.
Embedded surfaces are transparent by default: consumers needing an opaque application background must paint it
from their theme in their root panel before painting the child controls.

[Primary-button contrast validation](https://github.com/RedSalamanders/DxUi/blob/1947a5b91beb029e9b99d71e0893c6075bbb29ca/docs/measurements/primary-high-contrast-2026-09-05/README.md) records the native suites, gallery and matched performance evidence.

For localized empty states, call `combo.SetNoMatchesText(L"Aucun résultat")`,
`tree.SetEmptyStateText(L"Aucune donnée")`, or `grid.SetEmptyStateText(...)` with your product's
resource strings. The controls own the supplied text. Reapply it when the language changes;
an empty string restores the English default. An open ComboBox popup updates its width.
