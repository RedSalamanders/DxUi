# Build a Markdown editing experience

Use this guide for common editor tasks. The [rich-text reference](rich-text.md)
lists the supported syntax, limits and individual APIs. Pair headers and
`DxUi.lib` from one exact pin. Install the application's API-5 failure host before
constructing controls, following the [failure contract](../Specs/Core/Core_FailureHandling.md)
and the compiled [standalone host example](../Samples/EmbeddedControls/Main.cpp).

## Create and attach an editor

```cpp
#include <DxUi/DxUi.h>
#include <memory>

std::unique_ptr<DxUi::RichTextEditor> MakeNotebookEditor()
{
    auto editor = std::make_unique<DxUi::RichTextEditor>(L"# Notebook\n\nStart writing here.\n");
    editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    return editor;
}
```

Attach the result to your existing `ControlHost` with `SetRoot`. Keep the control
tree and its callbacks on the owner thread. Native and supplied-device hosts use
the same editor; follow [hosting and lifetime](hosting.md) for device, input,
preparation and redraw integration. Without explicit Markdown analysis/rendering,
the control retains its default v0 rich-text behavior.

## Turn selected text into a quote or code block

An Insert command should preserve the editor's source selection while its toolbar
takes focus, then use the selection-wrapping API:

```cpp
const bool quoted = editor.WrapSelectionInMarkdownBlock(
    DxUi::RichText::MarkdownBlockKind::BlockQuote);

// A separate Code Block action:
const bool fenced = editor.WrapSelectionInMarkdownBlock(
    DxUi::RichText::MarkdownBlockKind::FencedCode, L"cpp");
```

These are separate actions. Each successful action wraps the selected source,
keeps its original line endings and selection direction, and creates one Undo/Redo
transaction. Embedded backticks cause a longer safe fence. The operation works in
Rich and Source views without waiting for geometry.

An empty selection returns false so your application can choose its ordinary
insertion template. A refusal on a nonempty selection must keep that selection;
do not replace it with a template. Read-only, disabled, composing, invalid-language
and capacity cases decline without changing source or history. See the
[exact insertion contract](rich-text.md#selection-to-block-insertion).

For structural operations such as changing a heading, use
`ExecuteMarkdownCommand` and handle `Changed`, `Unchanged` and `Unavailable`.
Those commands require current Rich-view analysis and must not force synchronous
preparation while it is pending. Selection wrapping is a separate API with the
Rich/Source behavior above.

## Switch views and documents without losing edits

Use `SetViewMode` to switch between Rich and Source. Keep the authoritative source
and history in the editor; never recreate source from rendered glyphs or HTML.
Use `GetSelection`/`SetSelection` for oriented UTF-16 source positions.

For multiple open files, keep an application-owned `RichText::Document` per file
and use `ExchangeDocumentState` to exchange the active document without clearing
its history. The application owns paths, saved baselines, dirty state and crash
recovery. A composition or transient text-service preview can refuse a switch;
preserve both documents and retry only after the transient state ends. See
[document workspaces](rich-text.md#application-document-workspaces).

## Paste files and load images

Set the document's location before converting pasted file paths so generated
references can be relative to that document. Unicode text is preferred when text
and a bitmap coexist; an application with an image-paste handler may explicitly
choose image-first. Preserve source and selection when a handler declines.

Image transport and persistence belong to the application. Consume bounded image
requests, perform I/O/decoding away from the UI owner, then publish through the
documented owner-thread completion path. Apply permission checks before remote or
network file access. `//host/...` and `file://host/...` are network destinations.
Cancellation or a stale completion must not modify a replacement document. See
the [image resource reference](rich-text.md) and [input policy](../Specs/UI/UI_InputAndAccessibility.md).

## Handle unsupported or oversized content

Read source-owned semantic diagnostics rather than treating every fallback as
invalid syntax. `MathCapacity`/`DiagramCapacity` identify capacity; unsupported
syntax or native operations keep their distinct cause. A refused object retains
exact editable source and leaves healthy neighboring rich content available.
Its accessible explanation follows the same cause. A view or object change may
retry a native refusal; idle preparation does not repeatedly retry an unchanged
key.

Checked refusals are ordinary results. Unexpected C++ failures follow the API-5
host fatal/dump protocol. Physical IME, assistive technology, consumer integration
and formal paired performance retain their own qualification; implemented syntax
support does not establish those gates.
