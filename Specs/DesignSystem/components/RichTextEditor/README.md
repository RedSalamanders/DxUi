# RichTextEditor

Bounded Unicode editor with v0 formatting by default and an explicit semantic Markdown rendering preview.

- **Consumer supplies**: initial Markdown, a bounded rectangle, view/read-only policy and an optional text-change callback. Persistence belongs to the application.
- **Metrics**: 8 DIP inset on each side, 6 when compact; 4 DIP corners; 1 DIP caret; 500 ms blink phase using the host clock. Wheel scrolling moves 48 DIP per detent.
- **Typography**: host Body with Monospace family for code; headings use the existing rich-layout heading scale. Semantic headings retain explicit strong weight.
- **Colors**: TextField's input, border, text, selection and caret roles. Focus adds the host accent bottom stroke. Selection ink uses the selected-text role, including high contrast.
- **Ownership**: one retained control uses existing host services in either hosting mode. Source/mode/layout changes never reconstruct its identity.
- **Composition**: shared caret ink; 1 DIP composition underline and 2 DIP conversion-target underline, clipped to the same prepared text viewport. Silent previews commit as one undo transaction.
- **Accessibility**: Document with Text/Text2/TextEdit; exact Markdown source ACPs map to projected rectangles. Hidden delimiter ranges have no pixels. Read-only permits selection; disabled actions fail.
- **Semantic preview**: enable a CommonMark/GFM analysis profile and `SetMarkdownRenderingEnabled(true)`. Quote indentation, styles, rules, code/image outlines and equal-width table cells use native preparation. Tables retain physical alignment and reverse columns for RTL. No-wrap marker measurement establishes a shared list gutter for wrapped/continuation text; nested blocks inherit it. Links are underlined in inherited text ink and keep inert destinations. The documented balanced native HTML subset uses existing styles; unsupported markup retains the complete normalized leaf literally. Outlines and quote/rule strokes use the caret role. Images display alternatives; task checkboxes commit source state on matching release.
- **Fallback**: admission overflow displays exact source once and reports SourceFallback; it changes no source, selection or history. Local HTML policy rejection keeps a literal monospaced leaf while the backend can remain Semantic. Source view remains available.
- **Source caret sites**: empty/trailing/container-only source lines keep native caret boxes with the same typography and indentation. Empty/padded cells retain distinct visual affinity at a shared raw row-end ACP. Placement and movement add no text, undo entries or selection ink. Structural cell materialization uses the focused source cell.
- **Limits**: 64 Ki UTF-16 units and 128 undo items; 4,096 native marker/body/cell/caret layouts and 32,768 elements. One native HTML leaf permits 4,096 tags and 32 open tags. Empty balanced HTML has an interior source caret; links do not navigate. D2d source TSF/IME/UIA adapters are implemented; real IME/candidate-window/Narrator/NVDA/physical qualification remains pending. Semantic style accessibility and math/Mermaid/footnotes/front matter remain pending.

M4e adds `ExecuteMarkdownCommand` / `ApplyMarkdownCommand`: source-exact list,
heading, style, inert link, task and table transactions. Empty/padded cells
materialize only their focused source cell. Task checkbox actions stage on press
and commit on matching release; leaving/capture/focus/source/layout loss cancels.
Enter and Tab share list/table editing, Ctrl+B/I toggles formatting, and host
traversal remains available for incompatible Tab targets. Existing source
selection/history visuals and all palette/typography metrics remain unchanged.
The sample toolbar uses standard buttons, clear action names and a target/status
caption; it adds no editor-owned toolbar, HWND, resource fetch or scheduler.
Structured/style UIA and physical/extension qualification remain separate.

Task checkbox glyphs use the same monochrome `Segoe UI Symbol` face for checked
and unchecked states. Their bullet, hit-target and body columns stay fixed when
toggled, including single-task lists, custom body fonts and RTL. Other text and
ordinal typography keeps the requested font. The sample and gallery show both
task states for direct alignment review.

The preview is a static HTML rendition using `bundle.css`; the populated harness gallery is the authoritative Direct2D capture.
