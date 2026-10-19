#pragma once
#include "Scope.Tests.RichSemantic.h"
#include <DxUi/RichSemanticDocument.h>

namespace RichSemanticDocumentTests
{
using namespace DxUi::RichText;
using RichSemanticTests::Check;
inline std::shared_ptr<const MarkdownSyntax> Parse(std::wstring_view source, MarkdownProfile profile = MarkdownProfile::Gfm029)
{
    MarkdownParser parser;
    parser.Begin(source, 1u, profile);
    while (parser.HasPendingPreparation())
        parser.Advance(4096u);
    return parser.Published();
}
inline std::shared_ptr<const SemanticDocument> Complete(SemanticBuilder& builder, const MarkdownSyntax& syntax)
{
    std::size_t work = 0u;
    while (builder.HasPendingPreparation())
    {
        const auto step = builder.Advance(7u);
        Check(step.workUnits > 0u && step.workUnits <= 7u, "semantic owner turns obey the declared event budget");
        work += step.workUnits;
        Check(work <= 4u * (syntax.Nodes().size() + syntax.Inlines().size()) + 1u, "semantic traversal has a linear event bound");
    }
    const auto result = builder.Published();
    Check(result && result->Syntax().get() == &syntax, "semantic publication retains the exact requested immutable syntax");
    return result;
}
inline std::shared_ptr<const SemanticDocument> Build(std::wstring_view source, MarkdownProfile profile = MarkdownProfile::Gfm029)
{
    const auto syntax = Parse(source, profile);
    SemanticBuilder builder;
    builder.Begin(syntax);
    return Complete(builder, *syntax);
}
inline void CheckGraph(const SemanticDocument& document)
{
    const auto& map     = document.Map();
    const auto elements = document.Elements();
    Check(! elements.empty() && elements[0u].role == SemanticRole::Document && elements[0u].parent == MarkdownNoNode,
          "semantic document has one parentless document root");
    std::vector<bool> visited(elements.size(), false);
    std::vector<std::size_t> pending(1u, 0u);
    while (! pending.empty())
    {
        const auto index = pending.back();
        pending.pop_back();
        Check(index < elements.size() && ! visited[index], "semantic ownership graph is dense, acyclic and singly owned");
        visited[index]      = true;
        const auto& element = elements[index];
        Check(element.source.first <= element.source.last && element.source.last <= map.Source().size() && IsEditBoundary(map.Source(), element.source.first) &&
                  IsEditBoundary(map.Source(), element.source.last) && element.display.first <= element.display.last &&
                  element.display.last <= map.Display().size(),
              "semantic element ranges use valid bounded source/display coordinates");
        if (element.parent != MarkdownNoNode)
        {
            Check(element.parent < elements.size() && elements[element.parent].display.first <= element.display.first &&
                      element.display.last <= elements[element.parent].display.last,
                  "semantic children fit their enclosing display range");
        }
        std::size_t siblings = 0u;
        for (auto child = element.firstChild; child != MarkdownNoNode; child = elements[child].nextSibling)
        {
            Check(++siblings <= elements.size(), "semantic sibling chains terminate");
            Check(child < elements.size() && elements[child].parent == index, "semantic child edges retain their actual parent");
            pending.push_back(child);
        }
    }
    Check(std::all_of(visited.begin(), visited.end(), [](bool value) { return value; }), "semantic forest has no unreachable element");
    std::size_t styled = 0u;
    for (const auto& run : document.Runs())
    {
        Check(styled <= map.Display().size() && run.start == styled && run.length > 0u && run.length <= map.Display().size() - styled &&
                  ! (run.flags & ~(Bold | Italic | Code | Strike)) && run.heading <= 6u,
              "semantic styles partition the complete display");
        styled += run.length;
    }
    Check(styled == map.Display().size(), "semantic style coverage is complete");
    std::size_t previous = 0u;
    for (const auto& paragraph : document.Paragraphs())
    {
        Check(paragraph.element < elements.size() && paragraph.syntaxNode < document.Syntax()->Nodes().size() && paragraph.display.first >= previous &&
                  paragraph.display.first < paragraph.display.last && paragraph.display.last <= map.Display().size(),
              "semantic geometry paragraphs are bounded ordered leaf ranges");
        previous = paragraph.display.last;
    }
    for (std::size_t offset = 0u; offset <= map.Display().size(); ++offset)
        if (IsEditBoundary(map.Display(), offset))
        {
            Check(IsEditBoundary(map.Source(), map.ToSource(offset)) && IsEditBoundary(map.Source(), map.ToSource(offset, MappingAffinity::Trailing)),
                  "every display hit returns a legal source ACP");
        }
}
inline void DirectedValues()
{
    struct Example
    {
        std::wstring_view source, display;
    };
    constexpr Example examples[] = {{L"# A &amp; **B**\r\n\r\nText \\*literal\\* and \x0060\x0060 code \x0060\x0060.\n", L"A & B\nText *literal* and code.\n"},
                                    {L"> one\r\n> \x0060two\r\n> three\x0060\n", L"one two three\n"},
                                    {L"\x0060 a\r\nb \x0060\n", L"a b\n"},
                                    {L"one  \r\ntwo\\\nthree\n", L"one\ntwo\nthree\n"},
                                    {L"- [x] done\n- [ ] todo\n", L"\u2022 \u2611 done\n\u2022 \u2610 todo\n"},
                                    {L"3) one\n4) two\n   - child\n", L"3) one\n4) two\n\u2022 child\n"},
                                    {L"| h | j |\n| :--- | ---: |\n| x |\n", L"h\tj\nx\t \n"},
                                    {L"[go](https://example.invalid \"hint\") ![a *b*](local \"pic\")\n", L"go [image: a b]\n"},
                                    {L"<script>alert(1)</script>\n", L"<script>alert(1)</script>\n"},
                                    {L"    a &amp; \\*\r\n", L"a &amp; \\*\n"},
                                    {L"[only]: /definition\n", L""},
                                    {L"&fjlig; &#x1f600; &NotEqualTilde;\n", L"fj \U0001f600 \u2242\u0338\n"}};
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
    {
        for (std::size_t index = 0u; index < std::size(examples); ++index)
        {
            // Tasks and tables are explicitly GFM extensions, not CommonMark.
            if (profile == MarkdownProfile::CommonMark0312 && (index == 4u || index == 6u))
                continue;
            const auto document = Build(examples[index].source, profile);
            if (document->Map().Display() != examples[index].display)
                std::wcerr << L"semantic directed index=" << index << L" profile=" << static_cast<int>(profile) << L"\nexpected:\n"
                           << examples[index].display << L"actual:\n"
                           << document->Map().Display() << L'\n';
            Check(document->Map().Source() == examples[index].source && document->Map().Display() == examples[index].display,
                  "independent directed semantic output retains exact source");
            CheckGraph(*document);
        }
    }
    const auto code = Build(L"\x0060 a\r\nb \x0060\n");
    RichSemanticTests::CheckRanges(code->Map().SourceRanges({1u, 2u}), {{3u, 5u}}, "normalized code space owns the complete raw CRLF");
    RichSemanticTests::CheckRanges(code->Map().DisplayRanges({0u, 2u}), {}, "code delimiter and trimmed leading space have no glyph selection");
    const auto entities = Build(L"&fjlig; &#x1f600;\n");
    RichSemanticTests::CheckRanges(entities->Map().SourceRanges({1u, 2u}), {{0u, 7u}}, "named two-scalar entity is one source edit target");
    const auto table = Build(examples[6u].source);
    const auto tableRole =
        std::find_if(table->Elements().begin(), table->Elements().end(), [](const auto& element) { return element.role == SemanticRole::Table; });
    Check(tableRole != table->Elements().end() && tableRole->rows == 2u && tableRole->columns == 2u && table->Paragraphs().size() == 4u,
          "table metadata retains exact row/column geometry including padding");
    Check(table->Paragraphs()[0u].alignment == MarkdownAlignment::Left && table->Paragraphs()[1u].alignment == MarkdownAlignment::Right,
          "semantic table cells retain declared independent alignments");
    const auto links = Build(examples[7u].source);
    const auto image =
        std::find_if(links->Elements().begin(), links->Elements().end(), [](const auto& element) { return element.role == SemanticRole::Image; });
    Check(image != links->Elements().end() && image->name == L"a b" && image->destination == L"local" && image->title == L"pic",
          "atomic image alternative retains editable source, accessible name and inert destination/title");
}
inline void SchedulingAndAdmission()
{
    const auto oldSyntax = Parse(L"old\n");
    const auto newSyntax = Parse(L"new **value** &amp;\r\n");
    SemanticBuilder builder;
    builder.Begin(oldSyntax);
    const auto old = Complete(builder, *oldSyntax);
    builder.Begin(newSyntax);
    const auto first = builder.Advance(1u);
    Check(first.status == MarkdownPreparationStatus::Pending && first.workUnits == 1u && builder.Published() == old,
          "semantic owner yields without publishing a partial document");
    builder.Begin(newSyntax);
    RichSemanticTests::Reject<std::invalid_argument>([&] { builder.Advance(0u); }, "zero semantic event budget rejected");
    RichSemanticTests::Reject<std::invalid_argument>([&] { builder.Begin(nullptr); }, "null semantic request rejected");
    Check(builder.HasPendingPreparation() && builder.Published() == old, "rejected arguments preserve pending and published ownership");
    builder.Begin(oldSyntax);
    Check(! builder.HasPendingPreparation() && builder.Published() == old, "returning to a published request cancels superseded preparation");
    builder.Begin(newSyntax);
    builder.Advance(1u);
    builder.Cancel();
    Check(! builder.HasPendingPreparation() && builder.Published() == old, "semantic cancellation retains the previous immutable publication");
    builder.Begin(newSyntax);
    const auto current = Complete(builder, *newSyntax);
    Check(current != old && current->Map().Display() == L"new value &\n" && old->Map().Display() == L"old\n",
          "equal numeric syntax revisions with different source never coalesce");
    builder.Clear();
    Check(! builder.Published() && ! builder.HasPendingPreparation() && current->Map().Source() == newSyntax->Source(),
          "semantic retirement releases ownership without invalidating held documents");
    const auto nul = Parse(std::wstring_view(L"a\0b", 3u));
    RichSemanticTests::Reject<std::invalid_argument>([&] { builder.Begin(nul); }, "semantic builder preserves Document NUL admission");
    std::wstring many;
    for (std::size_t i = 0u; i <= SemanticDocument::MaximumParagraphs; ++i)
        many.append(L"a\n\n");
    const auto excessive = Parse(many);
    builder.Begin(oldSyntax);
    Complete(builder, *oldSyntax);
    builder.Begin(excessive);
    RichSemanticTests::Reject<std::length_error>([&] { Complete(builder, *excessive); }, "semantic geometry leaf admission fails before unbounded SDK work");
    Check(builder.Published()->Syntax() == oldSyntax && ! builder.HasPendingPreparation(), "failed semantic advance preserves publication and retires staging");
    std::wstring table(L"|");
    for (unsigned cell = 0u; cell < 128u; ++cell)
        table.append(L"a|");
    table.append(L"\n|");
    for (unsigned cell = 0u; cell < 128u; ++cell)
        table.append(L"---|");
    table.push_back(L'\n');
    for (unsigned row = 0u; row < 300u; ++row)
        table.append(L"| |\n");
    const auto amplified = Parse(table);
    Check(amplified->Nodes().size() > SemanticDocument::MaximumElements, "sparse table independently exercises source-to-node amplification");
    RichSemanticTests::Reject<std::length_error>([&] { builder.Begin(amplified); }, "amplified semantic node graph rejected before staging copies");
    Check(builder.Published()->Syntax() == oldSyntax && ! builder.HasPendingPreparation(), "failed semantic admission preserves its previous request state");
}
} // namespace RichSemanticDocumentTests
