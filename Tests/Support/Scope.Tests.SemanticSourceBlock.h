#pragma once
#include "Scope.Tests.SemanticDiagrams.h"

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
} // namespace SemanticSourceBlockTests
