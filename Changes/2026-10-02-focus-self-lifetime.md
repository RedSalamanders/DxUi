- A control that focuses itself from an input handler no longer touches itself once the focus callbacks destroyed it
  (the control that lost the focus, its own focus handler and the host's focus-changed callback run inside
  `SetFocusControl`, and an application may rebuild the controls from any of them). The internal
  `FocusControlAndSurvive` gives a control the host's focus, first the window's when asked, and says whether the control
  outlived it. All 28 places where a library control focused itself now go through it, and the 25 that went on using
  the control stop when it is false: the presses, double clicks, context menus and mnemonics of Button, Toggle,
  RadioButton, PageIndicator, Slider, ColorSwatch, MenuBar, TabControl, ComboBox, Splitter, ColorPicker, TextField, Grid
  and Tree, a TabControl's tab keys, and removing the tab that held the focus. `Button::Invoke` uses it too. `TabControl::SelectTab` (private) now says whether the control
  outlived its focus and selection callbacks, so a tab press no longer captures the mouse for a destroyed control.
  UI Automation's Select and AddToSelection, and the focus of a grid row, a grid cell or a single-selection tree item,
  now check after the selection's delegate ran that their control survived, before focusing it. They report
  `UIA_E_ELEMENTNOTAVAILABLE` where they focused a destroyed control before.
  - **Tests.** `RequireFocusReplacementLeavesControlAlone` (`DxUiTestHelpers.h`) runs each of those handlers with a
    focus-changed callback that replaces every control, in the Control, EditorControls, ComboBox, TextField, Grid and
    Tree suites. `TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus` does the same with a grid's and a
    tree's selection delegate for UI Automation's three actions. AddressSanitizer catches a handler that touched its
    destroyed control.
  - Specified in `UI_InputAndAccessibility.md`; `Testing_Validation.md` and the codex review plan follow. No API
    changes, and nothing visual changed.
