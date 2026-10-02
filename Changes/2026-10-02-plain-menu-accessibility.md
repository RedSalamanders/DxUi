- Every row of a menu without descriptions is now a UI Automation element, as a described menu's rows already were, so
  a screen reader can read, focus and invoke the rows of any native menu (RedSalamander's destination-menu test found
  none in an ordinary menu). Ported from `codex/menu-description-layout` (`15be545`, `d45c361`): three conditions on a
  popup's descriptions are gone from `DxUi.Menu.cpp`. Commands are MenuItem elements with Invoke, radio and toggle rows
  report their checked state, information rows are text and separators stay out of the tree. A row the viewport scrolled
  away reports `IsOffscreen` with no rectangle until UIA focus scrolls it into view (`DxUi.Accessibility.cpp`), and the
  native focus a popup receives selects no row in any menu: it restores only a row the keyboard or UI Automation chose.
  - **Snapshot sizing.** A window host's accessibility snapshot now counts its records first, reserves its vectors once
    and makes each record in place (`65b0257` of the same branch, rebuilt on main), which helps every host and a long
    menu most: a Down at 4,096 rows takes 24% less (7.9 to 6.0 ms) and holds 0.97 MB less.
  - **Cost.** Six interleaved runs per side against main (x64 Release, `Measurements/PlainMenuUia/2026-10-02`): an open
    menu holds about 1.6 KB more live heap per command row and 1.9 to 2.0 KB per radio row (+19,967 bytes for twelve
    commands, +6.39 MB for 4,096), all returned on close. A key or wheel notch that moves the rows takes about 0.3 ms
    plus 1.4 to 2.6 µs per row more: Down takes 340 µs instead of 38 for twelve rows, 635 instead of 38 for 128 and
    6.1 ms instead of 38 µs for 4,096, where opening takes 77 ms instead of 60. Described menus are unchanged.
  - **Tests.** `TestPlainMenuAccessibilityInvokesAndDisconnects`, `TestPlainMenuAccessibilityScrollsFocusedRow` and
    `TestMenuNativeFocusSelectsNoRowAndRestoresTheChosenOne` join the described-menu group, which runs in the Menu and
    the nonactivating NewControls lanes.
  - Specified in `UI_InputAndAccessibility.md`; `docs/controls.md`, `Testing_Validation.md` and the `accessibleName`
    comment in `DxUi.h` follow. No API changes, and nothing visual changed.
