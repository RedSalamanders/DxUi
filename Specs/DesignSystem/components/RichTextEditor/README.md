# RichTextEditor

Bounded Unicode Markdown v0 preview for source and formatted editing.

- **Consumer supplies**: initial Markdown, a bounded rectangle, view/read-only policy and an optional text-change callback. Persistence belongs to the application.
- **Metrics**: 8 DIP inset on each side, 6 when compact; 4 DIP corners; 1 DIP caret; 500 ms blink phase using the host clock. Wheel scrolling moves 48 DIP per detent.
- **Typography**: host Body with Monospace family for code; v0 ATX headings use the existing rich-layout heading scale.
- **Colors**: TextField's input, border, text, selection and caret roles. Focus adds the host accent bottom stroke. Selection ink uses the selected-text role, including high contrast.
- **Ownership**: one retained control uses existing host services in either hosting mode. Source/mode/layout changes never reconstruct its identity.
- **Limits**: 64 Ki UTF-16 units, 128 undo items and whole-document layout. This preview has basic Unicode entry and selection; TSF/IME and rich UIA remain D2d. Full Markdown/math/Mermaid/footnotes/front matter are pending. Do not substitute it for a qualified accessible IME editor yet.

The preview is a static HTML rendition using `bundle.css`; the populated harness gallery is the authoritative Direct2D capture.
