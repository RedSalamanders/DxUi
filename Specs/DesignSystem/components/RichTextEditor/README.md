# RichTextEditor

Bounded Unicode editor with v0 formatting by default and an explicit semantic Markdown rendering preview.

- **Consumer supplies**: initial Markdown, a bounded rectangle, view/read-only policy and an optional text-change callback. Persistence belongs to the application.
- **Metrics**: 8 DIP inset on each side, 6 when compact; 4 DIP corners; 1 DIP caret; 500 ms blink phase using the host clock. Wheel scrolling moves 48 DIP per detent.
- **Document surface**: `SetAppearance` can remove the input border, blend with the window background, center a bounded text column and override body/code families, sizes, font-relative line height and heading scale or explicit per-level heading values. Defaults preserve the metrics above. Geometry, caret and accessible font attributes follow the same prepared options. Borderless keyboard focus retains a visible outline; focused high contrast retains it persistently.
- **Typography**: host Body with Monospace family for code; headings use the existing rich-layout heading scale. Semantic headings retain explicit strong weight.
- **Colors**: TextField's input, border, text, selection and caret roles. Focus adds the host accent bottom stroke. Selection ink uses the selected-text role, including high contrast.
- **Ownership**: one retained control uses existing host services in either hosting mode. Source/mode/layout changes never reconstruct its identity.
- **Composition**: shared caret ink; 1 DIP composition underline and 2 DIP conversion-target underline, clipped to the same prepared text viewport. Silent previews commit as one undo transaction.
- **Accessibility**: Document with Text/Text2/TextEdit; exact Markdown source ACPs map to projected rectangles. Hidden delimiter ranges have no pixels. Read-only permits selection; disabled actions fail.
- **Selection integration**: `SetOnSelectionChanged` covers keyboard, native text services, UIA and programmatic routes. Defer transient toolbar work while `IsSelectionGestureActive()` is true; release/capture cancellation publishes final inactive state even with unchanged source endpoints. Callbacks may replace themselves or retire the editor safely.
- **Source ink**: `colorizeMarkdownSource` uses the configured parser profile to prepare subdued delimiters, accent heading/list markers and link-colored destinations. All raw source glyphs remain present with identical geometry; selection and high-contrast text retain their standard ink.
- **Optional cues**: `HoverOrCaret` shows a complete link underline on actual pointer glyph hits or a focused source caret; explicit underline and high contrast stay persistent. Accent ink is opt-in. Find highlights paint below selection and retire on source edits; optional focus mode subdues inactive paragraphs and is disabled in high contrast. No source/history changes result from these states.
- **Semantic preview**: enable a CommonMark/GFM analysis profile and `SetMarkdownRenderingEnabled(true)`. Quote indentation, styles, rules, code/image outlines and equal-width table cells use native preparation. Tables retain physical alignment and reverse columns for RTL. No-wrap marker measurement establishes a shared list gutter for wrapped/continuation text; nested blocks inherit it. Links are underlined in inherited text ink and keep inert destinations. The documented balanced native HTML subset uses existing styles; unsupported markup retains the complete normalized leaf literally. Outlines and quote/rule strokes use the caret role. Images display caller-supplied native pixels at their measured aspect, with descriptive alternatives for unavailable assets; task checkboxes commit source state on matching release.
- **Fallback**: admission overflow displays exact source once and reports SourceFallback; it changes no source, selection or history. Local HTML policy rejection keeps a literal monospaced leaf while the backend can remain Semantic. Source view remains available.
- **Source caret sites**: empty/trailing/container-only source lines keep native caret boxes with the same typography and indentation. Empty/padded cells retain distinct visual affinity at a shared raw row-end ACP. Placement and movement add no text, undo entries or selection ink. Structural cell materialization uses the focused source cell.
- **Limits**: 64 Ki UTF-16 units and 128 undo items; 4,096 native marker/body/cell/caret layouts and 32,768 elements. One native HTML leaf permits 4,096 tags and 32 open tags. Empty balanced HTML has an interior source caret; external links do not navigate. Source TSF/IME/UIA and bounded M4 semantic UIA are implemented. Footnotes have focused native/embedded preview checks; metadata validity, full extension conformance and final M5/physical/platform/performance qualification remain open.

M4e adds `ExecuteMarkdownCommand` / `ApplyMarkdownCommand`: source-exact list,
heading, style, inert link, task and table transactions. Empty/padded cells
materialize only their focused source cell. Task checkbox actions stage on press
and commit on matching release; leaving/capture/focus/source/layout loss cancels.
Enter and Tab share list/table editing, Ctrl+B/I toggles formatting, and host
traversal remains available for incompatible Tab targets. Existing source
selection/history visuals and all palette/typography metrics remain unchanged.
The sample toolbar uses standard buttons, clear action names and a target/status
caption; it adds no editor-owned toolbar, HWND, resource fetch or scheduler.
Semantic UIA exposes heading/list/quote/link/image/task/table children and typed
source-format attributes after coherent preparation. Tasks have an undoable
Toggle action; external links remain inert. Source TextPattern/TSF offsets stay exact.
Child identity retires on source/publication or owner-binding replacement, while
geometry-only reflow updates prepared bounds. Queries take no focus. The bounded
M4f implementation is [functionally qualified](../../../../Measurements/RichText/README.md).
Physical/extension/platform gates remain in the active plan; D4c stays HOLD.
The M4f slice changed no pixels or typography tokens. Its earlier five-theme
capture receipt remains historical evidence for that source.

Task checkbox glyphs use the same monochrome `Segoe UI Symbol` face for checked
and unchecked states. Their bullet, hit-target and body columns stay fixed when
toggled, including single-task lists, custom body fonts and RTL. Other text and
ordinal typography keeps the requested font. The sample and gallery show both
task states for direct alignment review.

The RichMarkdown footnote variant displays superscript references, numbered
styled bodies and separate underlined backlinks for repeated references. It
reuses existing ink and indentation; moved definitions drop outside declaration
list/quote gutters while preserving internal body containers. Ctrl+click and
Ctrl+Enter at references, or UIA Invoke, select/reveal exact source without editing
history. Generated backlinks own no raw text or selection fill. Read-only permits
navigation; disabled/composing/stale targets refuse it. Source-contiguous
selection can paint disjoint main/footer fragments. Dedicated object editing,
generated backlink keyboard traversal and final M5 qualification remain open.

File paths paste as displayed Markdown images or full-filename text links with explicit document-relative metadata. Mixed lists preserve order and one undo boundary. File links use existing inert underlined link styling; paste adds no navigation or drag-and-drop. Local PNG/JPEG/BMP/GIF/TIFF decoding belongs to an application worker. Publication preserves source/selection/history and UIA alternative names; selection tints opaque pixels, and hide/device replacement retires/recreates native resources. The [image guide and actual harness captures](../../../../docs/rich-text.md) show the raster preview. Final platform/resource/performance gates remain in the active plan.

The [file-drop spec and native/embedded test plan](../../../Plans/WIP/RichTextFileDrop_2026-10-20.md)
defines a future drop-at-caret gesture with the same image/link presentation.
Drag-and-drop remains unimplemented. Preview must preserve selection/focus and use
the normal OLE allowed/refused cursor; no new drop-marker visual is required by
the first implementation. This spec-only change leaves gallery pixels unchanged.

The preview is a static HTML rendition using `bundle.css`; the populated harness gallery is the authoritative Direct2D capture.

The continuation adds native in-document math and state/class diagrams. Use single atomic source owners, clear literal fallback with diagnostics, inherited text/selection colors, and F2 source access. Fractions and matrices reserve full line height; inline equations share the text baseline. The gallery now includes dedicated native mathematics and diagram tiles. Final physical/platform/performance qualification remains open.
