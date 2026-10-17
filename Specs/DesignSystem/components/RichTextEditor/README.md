# RichTextEditor

Bounded Unicode Markdown v0 preview for source and formatted editing.

- **Consumer supplies**: initial Markdown, a bounded rectangle, view/read-only policy and an optional text-change callback. Persistence belongs to the application.
- **Metrics**: 8 DIP inset on each side, 6 when compact; 4 DIP corners; 1 DIP caret; 500 ms blink phase using the host clock. Wheel scrolling moves 48 DIP per detent.
- **Typography**: host Body with Monospace family for code; v0 ATX headings use the existing rich-layout heading scale.
- **Colors**: TextField's input, border, text, selection and caret roles. Focus adds the host accent bottom stroke. Selection ink uses the selected-text role, including high contrast.
- **Ownership**: one retained control uses existing host services in either hosting mode. Source/mode/layout changes never reconstruct its identity.
- **Composition**: shared caret ink; 1 DIP composition underline and 2 DIP conversion-target underline, clipped to the same prepared text viewport. Silent previews commit as one undo transaction.
- **Accessibility**: Document with Text/Text2/TextEdit; exact Markdown source ACPs map to projected rectangles. Hidden delimiter ranges have no pixels. Read-only permits selection; disabled actions fail.
- **Limits**: 64 Ki UTF-16 units, 128 undo items and whole-document layout. D2d source TSF/IME/UIA adapters are implemented; real IME/candidate-window/Narrator/NVDA/physical qualification remains pending. Full Markdown/math/Mermaid/footnotes/front matter, structural and style accessibility are pending.

The preview is a static HTML rendition using `bundle.css`; the populated harness gallery is the authoritative Direct2D capture.
