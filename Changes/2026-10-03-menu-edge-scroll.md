- Home and End in a scrolling menu now show the menu's top and bottom padding with the first and last rows, as the menu
  shows them when it opens; they used to leave 4 DIP of padding scrolled out of view. A header or separator before the
  first navigable row or after the last comes into view too, when it fits with the row (`EnsureItemVisible` in
  `DxUi.Menu.cpp`).
  - **Tests.** `TestContextMenuPopupScrollsOversizedContent` (NewControls, nonactivating) now requires End to scroll to the
    full extent and Home back to zero. Against the library before this change it fails at End.
  - Specified in `UI_InputAndAccessibility.md`. API revision stays 3. The gallery shows menus as they open, so it is
    unchanged.
