# FormField

One stable editor with a label, optional description and a textual validation explanation.

- **Consumer supplies**: stable field ID, localized strings, one `AddEditor<T>`, required state and the application's validation result.
- **Ownership**: the field owns its label/editor/help/error controls. Reflow keeps the editor, draft, selection and composition; the application owns value validation and saving.
- **Metrics**: above-label and help/error gaps are 6 DIP; the beside-label gap is 12 DIP. Default editor preference is 180 by 32 DIP, with a minimum preferred width of 48 DIP.
- **Accessibility**: label name and `LabeledBy`, description as `HelpText`, required/data-valid properties and the error as `ItemStatus` belong to the semantic editor. NumericStepper uses its TextField. A hidden or detached label is not a live relationship.
- **Validation**: include a complete localized explanation. Empty errors are rejected. Color is optional reinforcement; it never replaces text. The default help/error labels use Small typography and normal text ink.
- **Theme**: existing editor palette roles and focus ring apply. No additional palette colors or density policy are introduced.

The preview is a static HTML rendition styled by `bundle.css`; the test-harness gallery is the authoritative Direct2D rendering.
