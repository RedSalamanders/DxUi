# RadioButton

Single choice within a mutually exclusive set; place related choices in **RadioButtons**.

- **Consumer supplies**: selection state and `SetOnSelected`.
- **Metrics**: 20 DIP circle; inner dot 8 at rest, 10 hover, 6 pressed.
- **Colors**: selected fill `selectionFill`, dot `selectionText`; group caption in `BodyStrong`.
- **Accessibility**: a RadioButton element with SelectionItem, selected while checked. UI Automation's Select chooses
  it as a click does; a change of choice raises the selection events whoever made it. In a group it reports its place
  among the group's choices ("2 of 3").

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
