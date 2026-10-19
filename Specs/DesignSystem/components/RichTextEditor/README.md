# RichTextEditor

Bounded Unicode editor with v0 formatting by default and an explicit semantic Markdown rendering preview.

- **Consumer supplies**: initial Markdown, a bounded rectangle, view/read-only policy and an optional text-change callback. Persistence belongs to the application.
- **Metrics**: 8 DIP inset on each side, 6 when compact; 4 DIP corners; 1 DIP caret; 500 ms blink phase using the host clock. Wheel scrolling moves 48 DIP per detent.
- **Typography**: host Body with Monospace family for code; headings use the existing rich-layout heading scale. Semantic headings retain explicit strong weight.
- **Colors**: TextField's input, border, text, selection and caret roles. Focus adds the host accent bottom stroke. Selection ink uses the selected-text role, including high contrast.
- **Ownership**: one retained control uses existing host services in either hosting mode. Source/mode/layout changes never reconstruct its identity.
- **Composition**: shared caret ink; 1 DIP composition underline and 2 DIP conversion-target underline, clipped to the same prepared text viewport. Silent previews commit as one undo transaction.
- **Accessibility**: Document with Text/Text2/TextEdit; exact Markdown source ACPs map to projected rectangles. Hidden delimiter ranges have no pixels. Read-only permits selection; disabled actions fail.
- **Semantic preview**: enable a CommonMark/GFM analysis profile and `SetMarkdownRenderingEnabled(true)`. Quote indentation, styles, rules, code/image outlines and equal-width table cells use native preparation. Tables retain physical alignment and reverse columns for RTL. Markers use existing ink; outlines and quote/rule strokes use the caret role. Images display alternatives and inert metadata; HTML remains literal. Generated task markers have no checkbox action yet.
- **Fallback**: admission overflow displays exact source once and reports SourceFallback; it changes no source, selection or history. Source view remains available.
- **Limits**: 64 Ki UTF-16 units and 128 undo items; semantic admission adds 4,096 paragraphs/cells and 32,768 elements. D2d source TSF/IME/UIA adapters are implemented; real IME/candidate-window/Narrator/NVDA/physical qualification remains pending. Empty/padded source caret sites, structural commands, semantic style accessibility and math/Mermaid/footnotes/front matter remain pending.

The preview is a static HTML rendition using `bundle.css`; the populated harness gallery is the authoritative Direct2D capture.
