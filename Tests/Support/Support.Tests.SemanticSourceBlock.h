#pragma once
#include "Support.Tests.SemanticDiagrams.h"

namespace SemanticSourceBlockTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using namespace DxUi::RichText::Detail;
inline constexpr std::wstring_view cases[] = {
    L"```mermaid\ngraph TD;A-->B\n```\n",
    L"~~~mermaid\ngraph LR;A[\"Unicode \u03b2 \U0001f600\"]-->B\n~~~",
    L"> ```mermaid\r\n> graph TD;A-->B\r\n> ```\r\n",
    L"- ```mermaid\n  graph TD;A-->B\n  ```\n",
    L"main[^n]\n\n[^n]: note\n    ```mermaid\n    graph TD;A-->B\n    ```\n",
    L"before **bold**\n\n```mermaid\ngraph TD;A-->B\n```\n\nafter *italic*\n",
    L"---\r\nname: '\u03b2'\r\n---\r\n# Body\r\n",
    L"\ufeff+++\nname = '\u03b2'\n+++\n# Body\n",
    L"before **bold**\n\n<div>\n  <b>HTML &amp;</b>\n</div>\n\nafter *italic*\n",
    L"    \u03b2 &amp; **literal**\r\n    next\r\n",
    L"```mermaid\nsequenceDiagram\nA->>B: hello\n```\n",
    L"```mermaid\ngraph TD;\U0001f600-->A\n```\n",
    L"```mermaid\ngraph TD;A-->B\n",
    L"```mermaid\ngraph TD;A-->B\n```\n\n```mermaid\ngraph LR;C-->D\n```\n",
};
inline std::size_t SourceBlock(const MarkdownSyntax& syntax)
{
    for (std::size_t index = 0u; index < syntax.Nodes().size(); ++index)
    {
        const auto kind = syntax.Nodes()[index].block.kind;
        if (kind == MarkdownBlockKind::FencedCode || kind == MarkdownBlockKind::IndentedCode || kind == MarkdownBlockKind::HtmlBlock ||
            kind == MarkdownBlockKind::FrontMatter)
            return index;
    }
    throw std::runtime_error("source-block test fixture has no target");
}
inline const SemanticElement& Element(const SemanticDocument& doc, std::size_t node)
{
    const auto found = std::ranges::find(doc.Elements(), node, &SemanticElement::syntaxNode);
    if (found == doc.Elements().end())
        throw std::runtime_error("source-block semantic element missing");
    return *found;
}
template <typename Check> inline void Admission(Check&& check)
{
    const auto oldSyntax     = RichSemanticDocumentTests::Parse(L"previous", MarkdownProfile::RichMarkdown);
    const auto pendingSyntax = RichSemanticDocumentTests::Parse(cases[0u], MarkdownProfile::RichMarkdown);
    const auto pendingNode   = SourceBlock(*pendingSyntax);
    for (const auto source : {L"[^n]: note\n    ```mermaid\n    graph TD;A-->B\n    ```\n",
                              L"> [^n]: note\n>     ```mermaid\n>     graph TD;A-->B\n>     ```\n",
                              L"main[^n]\n\n[^n]: primary\n\n[^n]: note\n    ```mermaid\n    graph TD;A-->B\n    ```\n"})
    {
        const auto syntax = RichSemanticDocumentTests::Parse(source, MarkdownProfile::RichMarkdown);
        SemanticBuilder builder;
        builder.Begin(oldSyntax);
        const auto old = SemanticDiagramTests::Complete(builder, check);
        builder.Begin(pendingSyntax, pendingNode);
        for (const auto target : {SourceBlock(*syntax), syntax->Nodes().size(), syntax->Nodes().size() + 7u})
        {
            bool rejected{};
            try
            {
                builder.Begin(syntax, target);
            }
            catch (const std::invalid_argument&)
            {
                rejected = true;
            }
            check(rejected && builder.HasPendingPreparation() && builder.Published() == old,
                  "hidden/duplicate-declaration and out-of-range source targets preserve publication and the original pending request");
        }
        const auto finished = SemanticDiagramTests::Complete(builder, check);
        check(finished->Syntax() == pendingSyntax && finished->SourceBlock() == pendingNode,
              "valid pending source presentation still completes after invalid replacement admission");
    }
}
template <typename Check> inline void Core(Check&& check)
{
    Admission(check);
    for (const auto source : cases)
    {
        const auto syntax = RichSemanticDocumentTests::Parse(source, MarkdownProfile::RichMarkdown);
        const auto node   = SourceBlock(*syntax);
        const auto range  = syntax->Nodes()[node].block.source;
        const auto raw    = syntax->Slice(range);
        SemanticBuilder builder;
        builder.Begin(syntax);
        const auto normal = SemanticDiagramTests::Complete(builder, check);
        builder.Begin(syntax, node);
        builder.Advance(1u);
        check(builder.HasPendingPreparation() && builder.Published() == normal,
              "a scoped source request cannot publish partially or reuse the default presentation key");
        builder.Begin(syntax);
        check(! builder.HasPendingPreparation() && builder.Published() == normal, "restoring the current presentation discards stale source-mode preparation");
        builder.Begin(syntax, node);
        const auto doc      = SemanticDiagramTests::Complete(builder, check);
        const auto& element = Element(*doc, node);
        check(doc->Syntax() == syntax && doc->Map().Source() == source && doc->SourceBlock() == node && doc->Identity() != normal->Identity(),
              "scoped presentation changes neither immutable syntax nor exact source");
        check(element.role == SemanticRole::Code && element.name == raw &&
                  doc->Map().Display().substr(element.display.first, element.display.last - element.display.first) == raw,
              "source presentation includes every raw fence/container prefix/Unicode unit/EOL without generated marker duplication");
        for (auto at = range.first; at < range.last; ++at)
            if (IsEditBoundary(source, at))
                check(doc->Map().ToSource(doc->Map().ToDisplay(at), MappingAffinity::Leading) == at,
                      "each editable source position round-trips linearly inside the scoped block");
        check(doc->Map().ToSource(element.display.last, MappingAffinity::Trailing) == range.last,
              "the scoped raw envelope retains its exact trailing source affinity, including a final fence without EOL");
        const auto nativeBefore = SemanticExtensionAccess::Diagrams(*normal).size();
        const auto nativeAfter  = SemanticExtensionAccess::Diagrams(*doc).size();
        check(nativeAfter == nativeBefore - (Element(*normal, node).role == SemanticRole::Diagram ? 1u : 0u),
              "only the selected native graph becomes source; other rich objects remain prepared");
        check(std::ranges::none_of(doc->Diagnostics(), [&](const auto& diagnostic) { return doc->Elements()[diagnostic.element].syntaxNode == node; }),
              "literal source mode bypasses the selected graph parser and its preview diagnostics");
        if (source.find(L"before **bold**") != std::wstring_view::npos)
            check(doc->Map().Display().starts_with(L"before bold\n") && doc->Map().Display().find(L"after italic\n") != std::wstring_view::npos,
                  "unrelated paragraphs retain their semantic rendering during local source presentation");
        builder.Begin(syntax, node);
        check(! builder.HasPendingPreparation() && builder.Published() == doc, "identical scoped requests coalesce without a new publication");
        builder.Begin(syntax);
        bool rejected{};
        try
        {
            builder.Begin(syntax, 0u);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        check(rejected && builder.HasPendingPreparation() && builder.Published() == doc,
              "invalid container targets preserve both pending restoration and current source presentation");
        const auto restored = SemanticDiagramTests::Complete(builder, check);
        check(restored->SourceBlock() == MarkdownNoNode && restored->Map().Display() == normal->Map().Display(),
              "leaving source presentation restores ordinary semantics without source rewriting");
    }
}
template <typename Check> inline void Geometry(Check&& check)
{
    const auto write   = PreparedFlowchartTests::WriteFactory();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    const auto calls   = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(write.get());
    if (! calls)
        throw std::bad_alloc();
    for (const auto source : cases)
    {
        const auto syntax = RichSemanticDocumentTests::Parse(source, MarkdownProfile::RichMarkdown);
        const auto node   = SourceBlock(*syntax);
        SemanticBuilder builder;
        builder.Begin(syntax, node);
        const auto doc = SemanticDiagramTests::Complete(builder, check);
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            LayoutOptions options;
            options.widthDip         = 640.f;
            options.readingDirection = direction;
            PreparedSemanticGeometry geometry;
            geometry.Begin(calls.Get(), doc, options, drawing.get(), {1u, 1u});
            std::size_t turns{};
            while (geometry.Pending())
            {
                const auto before = calls->layouts;
                const auto step   = geometry.Advance({1u, 3u});
                check(step.layoutsPrepared <= 1u && step.blockSteps <= 3u && calls->layouts - before == step.layoutsPrepared && ++turns < 20000u,
                      "scoped raw text and remaining rich objects share bounded actual SDK preparation");
            }
            const auto& element = Element(*doc, node);
            const auto index    = static_cast<std::size_t>(&element - doc->Elements().data());
            check(geometry.Ready() && geometry.ElementBounds(index) && ! geometry.SourceRange(element.source).empty(),
                  "scoped raw source exposes real native geometry and selectable fences");
            for (auto at = element.source.first; at < element.source.last; ++at)
                if (IsEditBoundary(source, at))
                    check(geometry.SourceCaret(at).height <= options.fontSizeDip * 2.f, "raw source caret sites follow text line height");
        }
    }
}

template <typename Check> inline std::shared_ptr<const MarkdownSyntax> CompleteRegion(MarkdownParser& parser, Check&& check)
{
    std::size_t turns{};
    while (parser.HasPendingPreparation())
    {
        const auto step = parser.Advance(1u);
        check(step.workUnits <= 1u && ++turns < 200000u, "opaque source regions share the ordinary one-event parser budget");
    }
    return parser.Published();
}

template <typename Check> inline void SourceRegions(Check&& check)
{
    struct Scenario
    {
        std::wstring_view source;
        MarkdownRange request, effective;
    };
    constexpr Scenario scenarios[] = {
        {L"", {0u, 0u}, {0u, 0u}},
        {L"abc", {3u, 3u}, {0u, 3u}},
        {L"abc\r\n", {5u, 5u}, {5u, 5u}},
        {L"abc\nxyz\nend", {5u, 6u}, {4u, 8u}},
        {L"```mermaid\ngraph TD;A-->B\n```\n", {0u, 30u}, {0u, 30u}},
        {L"~~broken\r\n[^n]: **&amp;**\r\n---\r\n", {0u, 32u}, {0u, 32u}},
        {L"---\nx: [\n---\n# Body\n", {0u, 13u}, {0u, 13u}},
        {L"> previous\n> ```\nmissing prefix\n> ```\n\nafter **bold**\n", {11u, 37u}, {11u, 38u}},
        {L"- previous\n  ```\n- typed prefix\n  ```\n\nafter **bold**\n", {11u, 37u}, {11u, 38u}},
        {L"[x]: url\n\n[x]\n", {0u, 9u}, {0u, 9u}},
        {L"[^n]: literal\n\nmain[^n]\n", {0u, 14u}, {0u, 14u}},
    };
    const auto write = PreparedFlowchartTests::WriteFactory();
    for (const auto& scenario : scenarios)
    {
        Document model{std::wstring(scenario.source)};
        MarkdownParser parser;
        parser.BeginSourceRegion(model.Snapshot(), model.Revision(), {scenario.request, 71u});
        const auto syntax = CompleteRegion(parser, check);
        check(syntax && syntax->SourceRegion() == MarkdownSourceRegion{scenario.effective, 71u},
              "source editing expands only to the explicitly expected complete physical lines, including empty EOF");
        const auto node = syntax->SourceRegionNode();
        for (const Selection selection : {Selection{scenario.effective.first, scenario.effective.first}, Selection{0u, 0u}})
        {
            MarkdownCommandRequest request;
            request.command = MarkdownCommand::ToggleStyle;
            bool commandRejected{}, deletionRejected{};
            try
            {
                static_cast<void>(BuildMarkdownCommand(model.Snapshot(), model.Revision(), syntax, selection, request));
            }
            catch (const std::invalid_argument&)
            {
                commandRejected = true;
            }
            const std::array ranges{scenario.effective};
            try
            {
                static_cast<void>(BuildMarkdownDeletion(model.Snapshot(), model.Revision(), syntax, selection, ranges));
            }
            catch (const std::invalid_argument&)
            {
                deletionRejected = true;
            }
            check(commandRejected && deletionRejected && model.Text() == scenario.source && ! model.CanUndo() && parser.Published() == syntax,
                  "pure structural and deletion planning reject presentation-only syntax inside or outside its active raw region before staging");
        }
        check(node < syntax->Nodes().size() && syntax->Nodes()[node].block.kind == MarkdownBlockKind::SourceEditor &&
                  syntax->Nodes()[node].firstChild == MarkdownNoNode && syntax->Nodes()[node].firstInline == MarkdownNoNode,
              "the complete editing envelope is one opaque leaf, including first-line grammar and malformed prefixes");
        check(std::ranges::none_of(syntax->Footnotes(),
                                   [&](const auto& note)
        {
            return note.definition != MarkdownNoNode && syntax->Nodes()[note.definition].block.source.first >= scenario.effective.first &&
                   syntax->Nodes()[note.definition].block.source.first < scenario.effective.last;
        }) && std::ranges::none_of(syntax->FootnoteReferences(),
                                   [&](const auto& ref)
        {
            const auto at = syntax->Inlines()[ref.inlineNode].source.first;
            return at >= scenario.effective.first && at < scenario.effective.last;
        }),
              "typed definitions and references inside raw source cannot register live footnote actions");
        SemanticBuilder builder;
        builder.Begin(syntax);
        const auto semantic  = SemanticDiagramTests::Complete(builder, check);
        const auto& element  = Element(*semantic, node);
        const auto raw       = syntax->Slice(scenario.effective);
        const auto displayed = raw.empty() ? std::wstring_view(L" ") : raw;
        check(semantic->SourceBlock() == node && element.role == SemanticRole::Code && element.name == displayed &&
                  semantic->Map().Display().substr(element.display.first, element.display.last - element.display.first) == displayed,
              "hinted syntax automatically selects exact raw semantic presentation without a second source document");
        PreparedSemanticGeometry geometry;
        LayoutOptions options;
        options.widthDip = 640.f;
        geometry.Begin(write.get(), semantic, options);
        while (geometry.Pending())
            geometry.Advance({1u, 3u});
        check(geometry.Ready() && geometry.SourceCaret(scenario.effective.first).height <= options.fontSizeDip * 2.f,
              "even an empty raw source envelope retains a real native text insertion caret");
        parser.BeginSourceRegion(model.Snapshot(), model.Revision(), {scenario.request, 71u});
        check(! parser.HasPendingPreparation() && parser.Published() == syntax, "identical source editing requests coalesce");
        parser.BeginSourceRegion(model.Snapshot(), model.Revision(), {scenario.request, 72u});
        const auto other = CompleteRegion(parser, check);
        check(other != syntax && other->Nodes()[other->SourceRegionNode()].identity != syntax->Nodes()[node].identity,
              "a different editing session cannot inherit the old synthetic object identity");
        parser.Begin(model.Snapshot(), model.Revision(), MarkdownProfile::RichMarkdown);
        const auto restored = CompleteRegion(parser, check);
        check(! restored->SourceRegion() && restored->SourceRegionNode() == MarkdownNoNode &&
                  std::ranges::none_of(restored->Nodes(), [](const auto& item) { return item.block.kind == MarkdownBlockKind::SourceEditor; }),
              "ordinary parsing at the same source/revision removes the noncanonical editing hint");
    }
    Document unicode{L"\U0001f600\r\nbody\n"};
    MarkdownParser parser;
    parser.BeginSourceRegion(unicode.Snapshot(), unicode.Revision(), {{0u, 4u}, 1u});
    const auto published = CompleteRegion(parser, check);
    parser.BeginSourceRegion(unicode.Snapshot(), unicode.Revision(), {{4u, 9u}, 2u});
    for (const auto invalid : {MarkdownSourceRegion{{1u, 4u}, 3u},
                               MarkdownSourceRegion{{0u, 3u}, 3u},
                               MarkdownSourceRegion{{8u, 4u}, 3u},
                               MarkdownSourceRegion{{0u, 10u}, 3u},
                               MarkdownSourceRegion{{0u, 4u}, 0u}})
    {
        bool rejected{};
        try
        {
            parser.BeginSourceRegion(unicode.Snapshot(), unicode.Revision(), invalid);
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        catch (const std::out_of_range&)
        {
            rejected = true;
        }
        check(rejected && parser.Published() == published && parser.HasPendingPreparation(),
              "invalid session/scalar/CRLF/order/capacity admission preserves publication and the pending request");
    }
    const auto next = CompleteRegion(parser, check);
    check(next->SourceRegion() == MarkdownSourceRegion{{4u, 9u}, 2u}, "valid pending source region survives every invalid replacement");
    const auto heldIdentity = next->Nodes()[next->SourceRegionNode()].identity;
    unicode.SetSelection({5u, 5u});
    check(unicode.ReplaceSelection(L"x"), "source identity fixture records an interior edit");
    parser.BeginSourceRegion(unicode.Snapshot(), unicode.Revision(), {{4u, 10u}, 2u});
    const auto edited = CompleteRegion(parser, check);
    check(edited->Nodes()[edited->SourceRegionNode()].identity == heldIdentity,
          "an interior tracked edit preserves the same session-local source node identity");
    parser.BeginSourceRegion(unicode.Snapshot(), unicode.Revision(), {{0u, 4u}, 3u});
    parser.Advance(1u);
    parser.Cancel();
    check(parser.Published() == edited && ! parser.HasPendingPreparation(), "cancellation cannot publish part of a replacement source envelope");
}

class SourceEditingEditor : public RichTextEditor
{
public:
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::ImportTextInputState;
};
template <typename Prepare, typename Check> inline void Retained(SourceEditingEditor& editor, ControlHost& host, Prepare&& prepare, Check&& check)
{
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    std::size_t notifications{};
    editor.SetOnTextChanged([&] { ++notifications; });
    const auto disconnect = wil::scope_exit([&] { editor.SetOnTextChanged({}); });
    const auto enter      = [&](std::wstring_view source)
    {
        editor.SetText(std::wstring(source));
        prepare();
        const auto normal = editor.GetAcknowledgedSemanticDocument(host);
        check(normal != nullptr, "source entry starts only from acknowledged native geometry");
        const auto node     = SourceBlock(*normal->Syntax());
        const auto& element = Element(*normal, node);
        const auto index    = static_cast<std::size_t>(&element - normal->Elements().data());
        const auto block    = normal->Syntax()->Nodes()[node].block;
        editor.SetSelection({block.content.first, block.content.first});
        const auto revision = editor.Revision();
        const auto before   = notifications;
        auto* focus         = host.GetFocusControl();
        check(! editor.BeginSemanticSourceEdit(host, normal, 0u) && ! editor.BeginSemanticSourceEdit(host, normal, normal->Elements().size()),
              "containers and outside indexes cannot enter source editing");
        check(editor.OnKeyDown(host, VK_F2, 0u) && editor.GetSemanticSourceRegion() && ! editor.GetMarkdownSyntax() &&
                  ! editor.GetAcknowledgedSemanticDocument(host) && editor.Revision() == revision && notifications == before && ! editor.CanUndo() &&
                  host.GetFocusControl() == focus && editor.GetViewMode() == ViewMode::Rich,
              "F2 enters only local presentation without editing history, source, requested view or focus");
        check(! editor.BeginSemanticSourceEdit(host, normal, index), "the previous semantic object cannot act while replacement geometry is pending");
        prepare();
        const auto raw = editor.GetAcknowledgedSemanticDocument(host);
        check(raw && raw->Syntax()->SourceRegion() && raw->SourceBlock() == raw->Syntax()->SourceRegionNode() &&
                  Element(*raw, raw->SourceBlock()).name == source.substr(block.source.first, block.source.last - block.source.first),
              "retained owner publishes one exact raw editing envelope including Unicode, prefixes and original EOLs");
        return block.source;
    };
    for (const auto source : cases)
    {
        const auto range   = enter(source);
        const auto session = editor.GetSemanticSourceRegion()->session;
        editor.SetSelection({range.first, range.first + 1u});
        check(editor.ReplaceSelection(L""), "the opening fence/prefix can be temporarily removed with a normal recorded edit");
        prepare();
        check(editor.GetSemanticSourceRegion() && editor.GetSemanticSourceRegion()->session == session &&
                  editor.GetAcknowledgedSemanticDocument(host)->Syntax()->Nodes()[editor.GetSemanticDocument()->SourceBlock()].block.kind ==
                      MarkdownBlockKind::SourceEditor,
              "malformed opening syntax remains in the same editable region after owner preparation");
        check(editor.Undo() && editor.GetText() == source && editor.GetSemanticSourceRegion() && editor.GetSemanticSourceRegion()->session == session,
              "one exact undo restores the raw source while preserving the current editing session");
        prepare();
        const auto before = notifications;
        check(editor.OnKeyDown(host, VK_ESCAPE, 0u) && ! editor.GetSemanticSourceRegion() && editor.CanRedo() && notifications == before,
              "Esc leaves presentation and preserves committed history and redo without a text-change notification");
        prepare();
        check(editor.GetAcknowledgedSemanticDocument(host) && ! editor.GetMarkdownSyntax()->SourceRegion(),
              "leaving a source scope reparses the exact document through the ordinary grammar");
        editor.SetSelection({range.last, range.last});
        check(editor.OnKeyDown(host, VK_F2, 0u) && editor.GetSemanticSourceRegion()->source == range,
              "F2 at the trailing boundary enters the same object before following paragraphs or raw EOF");
        check(editor.OnKeyDown(host, VK_F2, 0u) && ! editor.GetSemanticSourceRegion(), "F2 can leave an entry request before its new presentation is prepared");
        prepare();
    }
    editor.SetText(L"```mermaid\ngraph TD;A-->B\n```\n~~~mermaid\ngraph LR;C-->D\n~~~\n");
    prepare();
    const auto second = editor.GetText().find(L"~~~");
    editor.SetSelection({second, second});
    check(editor.OnKeyDown(host, VK_F2, 0u) && editor.GetSemanticSourceRegion()->source.first == second,
          "two touching object envelopes prefer the new object's leading entry rather than the preceding trailing fallback");
    check(editor.EndSemanticSourceEdit(host), "adjacent object source entry can finish while preparation is pending");
    prepare();
    const auto range = enter(cases[0u]);
    editor.SetSelection({range.first, range.first + 1u});
    check(editor.OnKeyDown(host, VK_DELETE, 0u) && editor.GetText().size() == cases[0u].size() - 1u,
          "Delete in a raw object removes only the selected source unit rather than the whole rendered object");
    prepare();
    check(editor.OnChar(host, L'\r', 0u) && editor.GetSemanticSourceRegion(), "Enter inserts literal CRLF inside an invalid fence without list/table commands");
    check(editor.OnChar(host, static_cast<wchar_t>(0xd83du), 0u) && editor.OnChar(host, static_cast<wchar_t>(0xde00u), 0u) &&
              editor.GetText().find(L"\U0001f600") != std::wstring::npos,
          "raw object editing retains the ordinary supplementary Unicode character transaction");
    prepare();
    TextInputState entry;
    check(editor.ExportTextInputState(entry), "source editor exports the authoritative whole document text store");
    auto preview = entry;
    preview.text.insert(preview.caretIndex, L"\u4eee");
    ++preview.caretIndex;
    preview.selectionAnchorIndex.reset();
    const auto session = editor.GetSemanticSourceRegion()->session;
    const auto before  = notifications;
    const auto undo    = editor.CanUndo();
    check(editor.ImportTextInputState(host, preview, false) && editor.GetSemanticSourceRegion()->session == session && notifications == before,
          "non-notifying whole-state service previews rebase the same raw source scope");
    prepare();
    check(editor.ImportTextInputState(host, entry, false) && editor.GetText() == entry.text && editor.CanUndo() == undo &&
              editor.GetSemanticSourceRegion()->session == session && notifications == before,
          "preview cancellation restores exact source and session without recording history");
    prepare();
    check(editor.OnKeyDown(host, VK_RETURN, MK_CONTROL) && ! editor.GetSemanticSourceRegion() && notifications == before,
          "Ctrl+Enter finishes raw presentation without inserting another line or another edit");
    enter(cases[0u]);
    editor.SetReadOnly(true);
    check(! editor.ReplaceSelection(L"x") && editor.EndSemanticSourceEdit(host), "read-only source inspection can exit but cannot change text");
    prepare();
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    const auto node     = SourceBlock(*document->Syntax());
    const auto index    = static_cast<std::size_t>(&Element(*document, node) - document->Elements().data());
    check(editor.BeginSemanticSourceEdit(host, document, index), "enabled read-only objects permit exact source inspection");
    prepare();
    check(! editor.OnChar(host, L'x', 0u) && editor.EndSemanticSourceEdit(host), "read-only raw text rejects character edits");
    editor.SetReadOnly(false);
    const auto joined = enter(L"```mermaid\ngraph TD;A-->B\n```\nafter **bold**\n");
    editor.SetSelection({joined.last - 1u, joined.last});
    check(editor.ReplaceSelection(L""), "raw editor can remove the boundary EOL");
    prepare();
    check(editor.GetSemanticSourceRegion() && editor.GetSemanticSourceRegion()->source.last == editor.GetText().size(),
          "joining the next physical line expands the opaque source envelope instead of parsing half a line");
    check(editor.Undo(), "physical-line join undo restores exact source");
    prepare();
    check(editor.EndSemanticSourceEdit(host), "joined source envelope can leave editing");
    enter(cases[5u]);
    editor.SetSelection({0u, 0u});
    check(! editor.GetSemanticSourceRegion(), "moving the caret outside a local source scope retires raw presentation");
    prepare();
    enter(cases[0u]);
    editor.SetViewMode(ViewMode::Source);
    check(! editor.GetSemanticSourceRegion() && editor.GetText() == cases[0u],
          "explicit whole source view closes the local editing presentation without rewriting source");
    editor.SetViewMode(ViewMode::Rich);
    enter(cases[0u]);
    editor.SetEnabled(false);
    check(! editor.EndSemanticSourceEdit(host), "disabled owners reject source presentation actions");
    editor.SetEnabled(true);
    check(editor.EndSemanticSourceEdit(host), "the active source presentation can finish after re-enabling its owner");
    prepare();
    enter(cases[0u]);
    const auto raw      = editor.GetAcknowledgedSemanticDocument(host);
    const auto& element = Element(*raw, raw->SourceBlock());
    const auto rawIndex = static_cast<std::size_t>(&element - raw->Elements().data());
    const auto bounds   = editor.GetSemanticElementBounds(host, rawIndex);
    check(bounds.has_value(), "raw source gesture has acknowledged native hit geometry");
    check(editor.OnMouseDown(host, {bounds->left + 2.f, bounds->top + 2.f}, false, 0u) && host.GetCapturedControl() == &editor,
          "a raw source drag captures its retained owner in either host");
    check(editor.OnKeyDown(host, VK_ESCAPE, 0u) && ! editor.GetSemanticSourceRegion() && host.GetCapturedControl() == nullptr &&
              ! editor.OnMouseMove(host, {bounds->left + 30.f, bounds->top + 2.f}, 0u),
          "Esc source exit releases capture and cancels the drag before publishing another presentation");
    prepare();
}
} // namespace SemanticSourceBlockTests
