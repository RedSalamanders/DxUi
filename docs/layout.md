# Intrinsic layout and forms

`FlexPanel`, `FormPanel` and `FormField` opt into measurement and arrangement during host preparation.
Existing `Panel`, `StackPanel`, `Toolbar` and manually positioned children retain their explicit bounds.
The embedded view still uses the application's device and scheduling. Composition shapes no text, arranges
no children and allocates no layout storage.

`Control::Measure(context, constraints, desired)` returns a numeric `DesiredSize` and optional baseline.
`LayoutConstraints` has finite, nonnegative minima and optional maximum axes; an absent maximum is unbounded.
Use `LayoutConstraints::AtMost(width)` for a bounded width and unbounded height. Invalid constraints, failed
shaping or a destructive/reentrant callback leave the caller's desired-size output unchanged. An unsupported
custom control returns `E_NOTIMPL`; override the non-pure `MeasureOverride` hook or supply a Flex explicit extent.
An old arranged rectangle is never a fallback intrinsic size.

Labels, buttons, toggles, checkboxes, text fields, combo boxes and numeric steppers use actual DirectWrite
metrics. Toggle state captions reserve their maximum extent, and single-line editors keep stable preferred
width while their values change. Containers cache numeric results by control lifetime/revision, constraints,
host and typography; color and hover changes do not invalidate shaping. Text, wrapping, font, DPI, density,
flow direction and ownership changes invalidate their dependencies.
The first measurement initializes the host's typography resources before recording its cache identity.
`DXUI_HAS_INTRINSIC_LAYOUT` identifies headers with these APIs; rebuild the library and consumer together when
adopting a new pin because adding a virtual hook changes the C++ ABI.

Flex supports row/column flow, explicit wrapping, gap, padding, grow weights and cross-axis alignment. Call
`SetChildOptions` to declare a child's minimum, maximum, explicit size, grow weight and alignment. Geometry
mirrors in RTL while ownership and tab order retain their logical sequence. Each Flex has at most 1,024 logical
slots. Library text shaping consumes at most 65,536 UTF-16 units in one container measurement pass, including
nested containers and repeated uncached labels. Each standalone measured text value has the same bound.
Cached results consume no shaping budget. Custom hooks remain responsible for their own bounded work.
Flex stages represented endpoints before moving children, preserving minima, gaps and padding at fractional
origins. Positive dimensions or insets that collapse at extreme coordinates fail before geometry changes.
End alignment anchors to the represented line endpoint. Prepared content and overflow include actual occupied
geometry with padding; pure measurement still reports the preferred extent.

```cpp
auto root = std::make_unique<DxUi::Panel>();
auto* form = root->AddChild<DxUi::FormPanel>();
form->SetBounds(D2D1::RectF(16.0f, 16.0f, 440.0f, 300.0f));
form->SetColumns(1);
form->SetLabelPlacement(DxUi::LabelPlacement::Above);
auto* width = form->AddField(L"canvas.width", L"Width in pixels");
auto* editor = width->AddEditor<DxUi::NumericStepper>();
editor->SetValue(1280.0); // application-owned draft
width->SetDescription(L"Choose a width between 1 and 4096 pixels.");
width->SetRequired(true);
// Present the application's validation result; this does not mutate the value.
width->SetValidation(DxUi::ValidationState::Error, L"Enter a width between 1 and 4096.");
host.SetRoot(std::move(root));
// host preparation arranges the opted-in form; explicit preparation is also available:
const HRESULT result = form->ApplyLayout(host);
```

Check the returned HRESULTs and nullable construction results in production code. A FormPanel accepts at most
256 fields, with nonempty unique IDs of at most 65,536 UTF-16 units. `AddEditor` accepts one editor; a failed
binding removes the inserted editor while the field remains live. A field's standalone measurement uses an
above-editor label; the form supplies the shared beside-label context without mutating measurement inputs.
Default editor preference is 180 by 32 DIP. `SetEditorPreferredSize` accepts an application-selected extent
with at least 48 DIP width and a positive height. Beside-label forms retain at least 108 DIP per column rather
than collapsing labels and editors. A narrow or short body reports `GetLayoutOverflow`; the application
provides scrolling or a wider responsive tier. `GetContentSize` supplies the prepared scroll extent.
Successful editor measurement retains its intrinsic minimum, including NumericStepper unit and button chrome.
Uniform columns expand when necessary. Unsupported custom editors use their declared preference, shrinking
only as far as the 48-DIP minimum. Forms settle column widths in at most four passes with one shared text budget;
a custom minimum that keeps growing returns `HRESULT_FROM_WIN32(ERROR_RETRY)` before geometry changes.
The settled width is cached until its control, typography, host, available width or horizontal origin changes.

Column/label-placement changes retain the same editor, draft, selection and logical tab order. Reflow that moves
a pressed control cancels its old press before coherent input geometry is published. Put fixed actions in a
separate sibling and continue using `ArrangeMeasuredActions` for action wrapping.

The semantic editor exposes its field ID, label name and `LabeledBy`, description as `HelpText`, required state,
`IsDataValidForForm` and the textual error as `ItemStatus`. For a NumericStepper those properties belong to its
TextField. Empty error explanations are rejected. Hidden, detached or destroyed labels have no effective
relationship; a retained provider cannot alias a replacement at the same path. Property events use immutable
snapshots and stop after reentrant teardown. Generic controls may use `SetAccessibleFormProperties` for the
same weak label relationship without transferring ownership.

Measurement and numeric validation fail before applying child bounds. A callback that changes layout inputs,
ownership, host, focus or another branch stops an active arrangement with `E_ABORT`; the host retries preparation
before publishing usable input geometry. Allocation failures return their HRESULT from measurement/layout and
validation APIs. Presentation setters may have already updated text before a later accessibility allocation
fails; retry the presentation update after recovery. Forms do not implement a validation engine or a save
transaction.
Each field's complete translated geometry is staged before the first bounds callback, using the exact represented
label/editor edges for measurement. Arrangement does not remeasure after geometry has begun changing. Detached
forms also verify earlier siblings and editor bounds after callbacks. `ClearChildren` retires the old semantic
bindings before detach notifications, so an editor bound by a detach callback remains the live replacement.
Flex also preserves options assigned to children added by detach callbacks. A nested pure measure retires an older
arrangement's scratch records before it can move later children. Flex stages absolute endpoints in logical order,
correcting fractional inward rounding while preserving minima, gaps and applicable maxima. Collapsed positive
extents or incompatible numeric limits fail before any bounds callback. `Measure` reports preferred size;
`GetContentSize` after a successful arrangement also includes the occupied represented extent and padding.

`performance-forms.ps1` runs the library's synthetic manual/form comparison with layout and pixel parity checked
before timing. It retains a candidate-only diagnostic report and raw native bytes; it cannot qualify the paired
regression policy or consumer performance. See the [performance guide](performance.md).

The [10 October native/form/Grid evidence](../Measurements/Layout/2026-10-10/README.md) retains all three x64
noninteractive profiles, six builds and the two eight-field form profiles. Unchanged work and composition allocate
no tracked C++ memory; Form retains 7 KiB more warm plain C++ memory than Manual, released on fixture destruction.
These diagnostic costs establish no speedup or paired regression acceptance. Focus, native ARM64 and consumer
qualification remain open as described in the packet.

Native/embedded library tests, [gallery captures](gallery/README.md) and the
[design system](../Specs/DesignSystem/README.md) describe library support. Actual consumer pin updates,
Apply/Cancel parity, physical IME/touch and native ARM64 runtime qualification remain separately tracked in the
[qualification plan](../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md). The completed S0-S2 library work is
recorded in [Done](../Specs/Plans/Done/ConsumerUiImprovements_S0_S2_2026-10-04.md); deferred S3-S7 proposals have their
own [WIP plan](../Specs/Plans/WIP/ConsumerUiImprovements_S3_S7_2026-10-11.md).
