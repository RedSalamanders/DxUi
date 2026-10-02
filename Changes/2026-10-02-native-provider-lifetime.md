- A window-host element can no longer act on a control that replaced its own, in the cases a review of the unmerged
  `codex/fileops-ui-qualified` branch found still open (its fix, rebuilt on main's element identity):
  - **The window's element.** While a single semantic control collapses into the window's element, that element is the
    control's element and is bound to it as every other element is to its control. Once the control is replaced or stops
    being the only one, a retained window element and an action already queued from it report `UIA_E_ELEMENTNOTAVAILABLE`
    instead of invoking the replacement (a queued `Invoke` from another thread ran the replacement's action), while its
    focus and point queries still answer that nothing is there (an embedded view's replaced root still reports every call
    gone); the window gives a newly acquiring client a fresh element.
    A window element acquired while no control collapsed into it is gone in the same way once one does.
  - **Focus callbacks that rebuild the controls.** An element's `SetFocus` and `Invoke` whose focus-changed callback
    rebuilds the controls report the element gone instead of success, and leave the callback's own focus choice;
    `Button::Invoke` with `focusSelf` no longer touches the button that callback destroyed.
  - **Runtime ids.** An element whose control is gone answers `GetRuntimeId` with `UIA_E_ELEMENTNOTAVAILABLE`, as every
    other call.
  - **Tests.** The branch's five native-lifetime tests, ported to the Accessibility suite, and a sixth for a window element
    acquired before its control collapsed into it. All but the queued text-range `Select`, which main already refused,
    failed before this change; each of the nine single-point reversions of the fix fails one of them.
  - Specified in `UI_InputAndAccessibility.md` and `Testing_Validation.md`; the plan is `CodexBranchReview_2026-10-02`.