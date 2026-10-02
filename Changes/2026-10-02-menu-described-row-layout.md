- A described native menu row holds one text layout instead of two (plan `MenuDescriptionMemory_2026-09-27`, the
  row-layout item). Its label, an empty spacer paragraph and its description are paragraphs of one DirectWrite layout
  built from the body format.
  - **Layout.** The description's range takes the small format's font (family, size, weight, style, stretch and locale,
    read back from the formats and applied where they differ, and the small format's tab stop for the layout, since only
    the description can hold a tab). The spacer's font size is set per row and width until its line is exactly
    `ceil(label height) - label height + 3` DIP tall, so the description starts 3 DIP below the label's height rounded up
    to whole DIPs, where a layout of its own was drawn. The heights and line counts are sums of the layout's line
    heights, which are bit-identical to what two layouts reported, and a row seeds the next row's spacer with the size it
    settled on, so rows whose labels are as tall are laid out once.
  - **Drawing.** One call draws the row: the label in its color and the description through a drawing effect on its
    range, which is one brush per popup and Direct2D device, recolored for each row just before its draw and recreated
    with the device.
  - **Memory.** A layout holds about 17 KB of shaping storage plus about 30 bytes a character, so the saving is per
    layout, and the popup-local sharing this replaces saved only where labels repeat. Whole-menu live heap, Release x64,
    `MenuResourceScaling` v5, open minus before, median of the last 16 of 32 cycles (the same in three runs of each
    build): rows with distinct labels 596,882 to 395,910 bytes for twelve rows (-33.7%), 1,096,836 to 695,752 for 24
    (-36.6%) and 2,100,287 to 1,298,940 for 48 (-38.2%); rows that all repeat one label, which main shared one layout
    for, 388,728 to 394,952 (+1.6%), 660,862 to 692,886 (+4.8%) and 1,208,673 to 1,292,258 (+6.9%). The developer
    accepted that cost for repeated labels on 2 October 2026 (the row-layout packet's option 1: real menus have distinct
    labels). A label has to repeat in about seven rows of one popup before sharing beats a layout per row, and a plain
    menu holds 16 bytes more (two pointers). In isolation (`MenuTextLayoutResources` v2, twelve rows) the native layouts
    hold 453,754 live bytes as pairs and 253,762 as one formatted layout per row.
  - **Pixels.** The 420 captures of a five-theme, eight-fixture, four-DPI matrix with keyboard, hover, wheel and scrolled
    states (emoji, Hebrew, Arabic, CJK, Thai, stacked marks, tabs, line breaks, an empty label, a long token) are
    byte-identical to main's in pixels and geometry, and so are all six gallery sheets, so `docs/gallery` needs no update.
  - **Tests.** `MenuResourceScaling` v5 adds the distinct-label menus (the earlier cases repeat one label in every row)
    and `MenuTextLayoutResources` v2 the one-layout mode. Six Menu-suite tests hold the row to the two-field contract
    against a separate layout measured in the test (heights, line counts, widths and the gap, through DPI changes), to
    its colors (enabled, disabled and hovered rows, and color glyphs), its tab stops, the layout count, the host's two
    formats agreeing on every property a layout holds for all its text, and the description surviving a lost device; ten
    mutants each fail a test, and the count test fails on main's two layouts.
  - **API.** `DebugSimulateContextMenuPopupDeviceLoss` and `ContextMenuPopupItemLayoutDebugState::descriptionOffsetDip`
    are additive diagnostics, so the API revision does not change; `rowLayouts` now counts one per described row.
