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
    std::size_t work            = 0u;
    const bool nativeCandidates = syntax.Profile() == MarkdownProfile::RichMarkdown && std::ranges::any_of(syntax.Nodes(), [&](const auto& node) {
        return node.block.kind == MarkdownBlockKind::FencedCode && syntax.Slice(node.block.info).starts_with(L"mermaid");
    });
    const auto bound            = 4u * (syntax.Nodes().size() + syntax.Inlines().size()) + 1u + (nativeCandidates ? 8u * syntax.Source().size() + 32u : 0u);
    while (builder.HasPendingPreparation())
    {
        const auto step = builder.Advance(7u);
        Check(step.workUnits > 0u && step.workUnits <= 7u, "semantic owner turns obey the declared event budget");
        work += step.workUnits;
        Check(work <= bound, "semantic traversal and native source scanning have a linear event bound");
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
                  ! (run.flags & ~(Bold | Italic | Code | Strike | Underline | Link)) && run.heading <= 6u,
              "semantic styles partition the complete display");
        styled += run.length;
    }
    Check(styled == map.Display().size(), "semantic style coverage is complete");
    std::size_t sourceStyled = 0u;
    for (const auto& run : document.SourceRuns())
    {
        Check(run.start == sourceStyled && run.length > 0u && run.length <= map.Source().size() - sourceStyled && IsEditBoundary(map.Source(), run.start) &&
                  IsEditBoundary(map.Source(), run.start + run.length),
              "source format units partition raw UTF-16 without splitting scalar or CRLF ownership");
        sourceStyled += run.length;
    }
    Check(sourceStyled == map.Source().size(), "hidden syntax and transformed values have complete source format coverage");
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
inline void NativeHtmlAndLinkPresentation()
{
    struct TagStyle
    {
        std::wstring_view name;
        unsigned flags;
    };
    constexpr TagStyle tagStyles[] = {
        {L"b", Bold}, {L"strong", Bold}, {L"i", Italic}, {L"em", Italic}, {L"u", Underline}, {L"s", Strike}, {L"del", Strike}, {L"code", Code}, {L"span", 0u}};
    for (const auto& tag : tagStyles)
    {
        const auto document = Build(std::wstring(L"one <") + std::wstring(tag.name) + L">x</" + std::wstring(tag.name) + L">\n");
        Check(document->Map().Display() == L"one x\n", "every accepted inline style tag has an independent display oracle");
        const auto run = std::find_if(
            document->Runs().begin(), document->Runs().end(), [](const auto& value) { return value.start <= 4u && 4u < value.start + value.length; });
        Check(run != document->Runs().end() && run->flags == tag.flags, "each native style alias maps to its declared style flags");
        CheckGraph(*document);
    }
    for (unsigned level = 1u; level <= 6u; ++level)
    {
        const auto digit    = static_cast<wchar_t>(L'0' + level);
        const auto document = Build(std::wstring(L"<h") + digit + L">heading</h" + digit + L">\n");
        Check(document->Map().Display() == L"heading\n" && document->Elements()[1u].role == SemanticRole::Heading &&
                  document->Elements()[1u].heading == level && document->Runs().front().heading == level,
              "all six native HTML heading levels retain explicit native role/style metadata");
    }
    for (const auto destination : {L"http://example.invalid", L"mailto:a@example.invalid", L"relative/path#part", L"#fragment", L""})
    {
        const auto document = Build(std::wstring(L"one <a title='hint' href='") + destination + L"'>label</a>\n");
        Check(document->Map().Display() == L"one label\n", "supported HTML URI forms and title-before-href stay inert native text");
    }
    for (const auto breakTag : {L"<br>", L"<br/>", L"<br />", L"<BR />"})
        Check(Build(std::wstring(L"one") + breakTag + L"two\n")->Map().Display() == L"one\ntwo\n", "all declared void-break spellings have one native LF");
    Check(Build(L"<div>text</div>\n")->Map().Display() == L"text\n", "a single div wrapper uses native paragraph content");
    Check(Build(L"one <A HREF='#x' TITLE='hint'>label</A>\n")->Map().Display() == L"one label\n",
          "native HTML tag/attribute names use ASCII case-insensitive matching");
    struct Example
    {
        std::wstring_view source, display;
    };
    constexpr Example examples[] = {{L"one <b>bold &amp; **nested**</b> end\n", L"one bold & nested end\n"},
                                    {L"*a <b>b</b> c*\n", L"a b c\n"},
                                    {L"<p>native <strong>bold</strong><br/>line &fjlig;</p>\r\n", L"native bold\nline fj\n"},
                                    {L"<h2>Title <i>x</i></h2>\n", L"Title x\n"},
                                    {L"<pre>a &amp;\r\n  b</pre>\n", L"a &\n  b\n"},
                                    {L"one <a href='https://example.invalid?q=1&amp;x=2' title='A &quot;B&quot;'>go **now**</a>\n", L"one go now\n"},
                                    {L"a <U>u</U><s>s</s><del>d</del><code>c</code><span>x</span><br>z\n", L"a usdcx\nz\n"},
                                    {L"<b></b>\n", L" \n"},
                                    {L"<p></p>\n", L" \n"},
                                    {L"<script>alert(1)</script>\n", L"<script>alert(1)</script>\n"},
                                    {L"one <b>**x**</i> end\n", L"one <b>**x**</i> end\n"},
                                    {L"one <b>ok</b> <span class='x'>**literal**</span>\n", L"one <b>ok</b> <span class='x'>**literal**</span>\n"},
                                    {L"<div><p>nested block</p></div>\n", L"<div><p>nested block</p></div>\n"},
                                    {L"[MD](local) and <a href='data:text/plain,hi'>bad</a>\n", L"[MD](local) and <a href='data:text/plain,hi'>bad</a>\n"},
                                    {L"<p>**HTML block text**</p>\n", L"**HTML block text**\n"},
                                    {L"<p>a  b\r\n c</p>\n", L"a  b\n c\n"},
                                    {L"<p>&lt;script&gt;x&lt;/script&gt;</p>\n", L"<script>x</script>\n"},
                                    {L"one <a href='#x'>*x</a> ![alt](local)*\n", L"one x [image: alt]\n"},
                                    {L"one <a href='#x'>*x</a> [tail](local)*\n", L"one x tail\n"}};
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
        for (std::size_t index = 0u; index < std::size(examples); ++index)
        {
            const auto document = Build(examples[index].source, profile);
            if (document->Map().Display() != examples[index].display)
                std::wcerr << L"native HTML example=" << index << L" profile=" << static_cast<unsigned>(profile) << L" expected=" << examples[index].display
                           << L" actual=" << document->Map().Display() << L'\n';
            Check(document->Map().Source() == examples[index].source && document->Map().Display() == examples[index].display,
                  "native HTML independent literal/value oracle and exact raw source");
            CheckGraph(*document);
        }
    const auto styles  = Build(L"*a <b>b</b> c* <u>d</u> [go](local)\n");
    const auto flagsAt = [&](std::size_t at)
    {
        for (const auto& run : styles->Runs())
            if (run.start <= at && at < run.start + run.length)
                return run.flags;
        return 0u;
    };
    Check(flagsAt(0u) == Italic && flagsAt(2u) == (Italic | Bold) && flagsAt(4u) == Italic && flagsAt(6u) == Underline && flagsAt(8u) == Link,
          "HTML style closure and Markdown style inheritance are independent; links are explicit underlined roles");
    const auto anchor = Build(examples[5u].source);
    const auto link =
        std::find_if(anchor->Elements().begin(), anchor->Elements().end(), [](const auto& element) { return element.role == SemanticRole::Link; });
    Check(link != anchor->Elements().end() && link->name == L"go now" && link->destination == L"https://example.invalid?q=1&x=2" && link->title == L"A \"B\"",
          "HTML anchors retain decoded inert destination/title and resolved label");
    const auto opening = examples[5u].source.find(L"<a"), label = examples[5u].source.find(L"go");
    Check(anchor->Map().DisplayRanges({opening, label}).empty(), "validated HTML tags/attributes have no source selection glyphs");
    const auto entity = Build(L"<p>&fjlig; &#x1f600; &NotEqualTilde;</p>\n");
    RichSemanticTests::CheckRanges(entity->Map().SourceRanges({0u, 2u}), {{3u, 10u}}, "HTML two-scalar entity owns its exact atomic raw token");
    for (const auto unsafe : {L"javascript&#x3a;alert(1)", L"vbscript:x", L"data:x", L"java&#10;script:x", L"file:///x", L"https://a b"})
    {
        const std::wstring source = std::wstring(L"one <b>ok</b> <a href='") + unsafe + L"'>link</a>\n";
        Check(Build(source)->Map().Display() == source, "unsupported/obfuscated HTML destinations reject the whole leaf without partial tag hiding");
    }
    for (const auto invalid : {L"one <b onclick='x'>x</b>\n",
                               L"one <b/>x\n",
                               L"one </br>x\n",
                               L"one <a href='x' href='y'>x</a>\n",
                               L"one <a href=x>x</a>\n",
                               L"one <b>x\n",
                               L"one <!-- c -->x\n",
                               L"one <a href='x'>[nested](y)</a>\n",
                               L"[<a href='x'>nested</a>](y)\n"})
        Check(Build(invalid)->Map().Display() == invalid, "malformed/active/nested-anchor HTML is literal atomically");
    std::wstring deep = L"one ";
    for (unsigned i = 0u; i < 33u; ++i)
        deep += L"<b>";
    deep += L"x";
    for (unsigned i = 0u; i < 33u; ++i)
        deep += L"</b>";
    deep += L"\n";
    Check(Build(deep)->Map().Display() == deep, "native HTML nesting admission has a literal fallback");
    deep.erase(4u, 3u);
    deep.erase(deep.size() - 5u, 4u);
    Check(Build(deep)->Map().Display() == L"one x\n", "the exact supported HTML nesting depth is admitted");
    std::wstring many = L"one ";
    for (unsigned i = 0u; i < 4096u; ++i)
        many += L"<br>";
    many += L"\n";
    Check(Build(many)->Map().Display() == std::wstring(L"one ") + std::wstring(4096u, L'\n'), "the exact native HTML tag-count bound is admitted");
    many.insert(many.size() - 1u, L"<br>");
    Check(Build(many)->Map().Display() == many, "native HTML tag overflow retains the whole leaf literally");
    const auto whitespace = Build(L"<p>a  b\r\n c</p>\n");
    RichSemanticTests::CheckRanges(whitespace->Map().SourceRanges({4u, 5u}), {{7u, 9u}}, "native HTML normalized line ending owns its exact raw CRLF");
    const auto empty = Build(L"<p><b></b></p>\n");
    Check(empty->Paragraphs()[0u].emptyInsertion == 6u && empty->Map().DisplayRanges({0u, empty->Map().Source().size()}).empty(),
          "empty balanced HTML has a real interior source insertion without generated selection glyphs");
}
inline void FootnoteHtmlActionAdmission()
{
    const auto overlapping = Build(L"one <a href='#x'>known[^n]</a>\n\n[^n]: note\n", MarkdownProfile::RichMarkdown);
    Check(overlapping->Map().Display() == L"one <a href='#x'>known[^n]</a>\n1. note\n\u21a91\n",
          "resolved footnote actions inside native HTML anchors retain the entire overlapping leaf literally");
    Check(overlapping->Navigation().size() == 1u &&
              std::ranges::none_of(overlapping->Elements(),
                                   [](const auto& item) { return item.role == SemanticRole::Link || item.role == SemanticRole::FootnoteReference; }),
          "literal overlapping anchors publish no competing forward actions while preserving a source-exact footer backlink");
    const auto styling = Build(L"one <b>known[^n]</b>\n\n[^n]: note\n", MarkdownProfile::RichMarkdown);
    Check(styling->Map().Display() == L"one known\u00b9\n1. note\n\u21a91\n" && styling->Navigation().size() == 2u,
          "noninteractive native HTML styles do not suppress supported footnote links");
    const auto missing = Build(L"one <a href='#x'>missing[^n]</a>\n", MarkdownProfile::RichMarkdown);
    Check(missing->Map().Display() == L"one missing[^n]\n" && missing->Navigation().empty() && missing->Syntax()->Diagnostics().size() == 1u,
          "an unresolved literal footnote marker does not invent a nested HTML action or hide its diagnostic");
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
    RichSemanticTests::Reject<std::length_error>([&] { Parse(table); }, "sparse table amplification is rejected before allocating an excessive parser graph");
    Check(builder.Published()->Syntax() == oldSyntax && ! builder.HasPendingPreparation(), "failed semantic admission preserves its previous request state");
}
} // namespace RichSemanticDocumentTests

namespace RichSemanticDocumentTests
{
inline void SourceFormatOwnership()
{
    const std::wstring source = L"# Head\r\n\r\n**bold &fjlig; \U0001f600** and <i>slant</i> and `code`\n";
    const auto document       = Build(source);
    CheckGraph(*document);
    const auto at = [&](std::size_t offset)
    {
        for (const auto& run : document->SourceRuns())
            if (run.start <= offset && offset < run.start + run.length)
                return run;
        throw std::logic_error("Missing source format owner");
    };
    Check(at(source.find(L"Head")).heading == 1u && at(0u).heading == 0u, "heading syntax stays raw while rendered heading text owns heading style");
    const auto entity = source.find(L"&fjlig;");
    for (auto offset = entity; offset < entity + 7u; ++offset)
        Check(at(offset).flags == Bold, "every raw unit of a multi-scalar entity has the rendered value's style");
    Check(at(source.find(L"**")).flags == Regular && at(source.find(L"slant")).flags == Italic && at(source.find(L"<i>")).flags == Regular &&
              at(source.find(L"code")).flags == Code,
          "hidden delimiters and native HTML retain regular source style; bodies retain actual native style");
    const auto repeated = Build(source);
    Check(document->Identity() != repeated->Identity(), "distinct semantic publications cannot reuse a UIA fragment epoch even for identical source");
}
inline void StructuralStyleValues()
{
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029, MarkdownProfile::RichMarkdown})
        for (const std::wstring_view spelling : {L"word",
                                                 L" a ",
                                                 L"a**b",
                                                 L"a*b",
                                                 L"a~~b",
                                                 L"**a** **b**",
                                                 L"&copy;",
                                                 L"\\*literal\\*",
                                                 L"\U0001f600\u0301",
                                                 L"**# heading**",
                                                 L"**- item**",
                                                 L"**1. item**"})
            for (const auto style : {MarkdownTextStyle::Strong, MarkdownTextStyle::Emphasis, MarkdownTextStyle::Strikethrough, MarkdownTextStyle::Code})
            {
                Document document(spelling);
                document.SetSelection({0u, spelling.size()});
                MarkdownCommandRequest request;
                request.command   = MarkdownCommand::ToggleStyle;
                request.style     = style;
                const auto before = Build(spelling, profile);
                const auto plan   = BuildMarkdownCommand(
                    document.Snapshot(), document.Revision(), RichCommandTests::Parse(document, profile), document.GetSelection(), request);
                if (profile == MarkdownProfile::CommonMark0312 && style == MarkdownTextStyle::Strikethrough)
                {
                    Check(! plan.IsApplicable(), "strike style respects dialect admission");
                    continue;
                }
                Check(plan.IsApplicable() && ApplyMarkdownCommand(document, plan), "semantic style oracle exercises a committed change");
                const auto after = Build(document.Text(), profile);
                Check(before->Map().Display() == after->Map().Display(),
                      "independent semantic lowering preserves Unicode value through literal punctuation, encoded text and block-like prefixes");
                if (spelling == L"word")
                {
                    const auto flags = style == MarkdownTextStyle::Strong          ? Bold
                                       : style == MarkdownTextStyle::Emphasis      ? Italic
                                       : style == MarkdownTextStyle::Strikethrough ? Strike
                                                                                   : Code;
                    Check(std::ranges::any_of(after->Runs(), [&](const auto& run) { return (run.flags & flags) != 0u; }),
                          "new style changes actual semantic flags, not merely source delimiters");
                }
            }
    for (const std::wstring_view spelling : {L"a]b", L"a[b", L"&copy;", L"**label**"})
    {
        Document document(spelling);
        document.SetSelection({spelling.size(), 0u});
        MarkdownCommandRequest request;
        request.command     = MarkdownCommand::SetLink;
        request.destination = L"#target";
        const auto plan = BuildMarkdownCommand(document.Snapshot(), document.Revision(), RichCommandTests::Parse(document), document.GetSelection(), request);
        Check(plan.IsApplicable() && ApplyMarkdownCommand(document, plan), "link source conversion admitted");
        const auto after = Build(document.Text());
        Check(Build(spelling)->Map().Display() == after->Map().Display() &&
                  std::ranges::any_of(after->Elements(),
                                      [](const auto& element) { return element.role == SemanticRole::Link && element.destination == L"#target"; }),
              "literal brackets and encoded labels retain visible Unicode and create an actual inert semantic link");
    }
    Document table(L"a\\|b | next\n- | -\n");
    table.SetSelection({0u, 4u});
    MarkdownCommandRequest request;
    request.command = MarkdownCommand::ToggleStyle;
    request.style   = MarkdownTextStyle::Code;
    const auto plan = BuildMarkdownCommand(table.Snapshot(), table.Revision(), RichCommandTests::Parse(table), table.GetSelection(), request);
    Check(ApplyMarkdownCommand(table, plan) && table.Text() == L"`a\\|b` | next\n- | -\n", "code conversion inside a table retains raw pipe escaping");
    Check(Build(table.Text())->Map().Display() == Build(L"a\\|b | next\n- | -\n")->Map().Display(),
          "code table conversion preserves actual visible cell values");
    table.SetSelection({0u, 6u});
    request.command     = MarkdownCommand::SetLink;
    request.destination = L"/a|b";
    const auto link     = BuildMarkdownCommand(table.Snapshot(), table.Revision(), RichCommandTests::Parse(table), table.GetSelection(), request);
    Check(ApplyMarkdownCommand(table, link) && table.Text() == L"[`a\\|b`](</a&#124;b>) | next\n- | -\n", "link metadata cannot create a table separator");
    Document html(L"<a href='#target'>*word* &copy;</a>");
    html.SetSelection({20u, 20u});
    request.command   = MarkdownCommand::RemoveLink;
    const auto unlink = BuildMarkdownCommand(html.Snapshot(), html.Revision(), RichCommandTests::Parse(html), html.GetSelection(), request);
    Check(ApplyMarkdownCommand(html, unlink) && html.Text() == L"*word* &copy;" &&
              Build(html.Text())->Map().Display() == Build(L"<a href='#target'>*word* &copy;</a>")->Map().Display(),
          "native inline unlink retains its existing Markdown semantics and raw label spelling");
    Document block(L"<a href='#target'>\n*word* &copy;\n</a>\n");
    const auto original = block.Snapshot();
    block.SetSelection({20u, 20u});
    const auto opaque = BuildMarkdownCommand(original, block.Revision(), RichCommandTests::Parse(block), block.GetSelection(), request);
    Check(! opaque.IsApplicable() && ! ApplyMarkdownCommand(block, opaque) && block.Snapshot().IsSameRevision(original),
          "unlink rejects an opening HTML-block anchor rather than reinterpreting a literal body as Markdown");
    for (const std::wstring_view source : {L"<https://example.invalid/a?x&copy;>",
                                           L"https://example.invalid/a?x&copy;",
                                           L"www.example.invalid",
                                           L"person@example.invalid",
                                           L"[https://example.invalid](target)",
                                           L"[**www.example.invalid**](target)"})
    {
        Document value(source);
        value.SetSelection({4u, 4u});
        const auto before = Build(source);
        MarkdownCommandRequest unlinkRequest;
        unlinkRequest.command = MarkdownCommand::RemoveLink;
        const auto remove     = BuildMarkdownCommand(value.Snapshot(), value.Revision(), RichCommandTests::Parse(value), value.GetSelection(), unlinkRequest);
        const auto autolinkSnapshot = value.Snapshot();
        Check(remove.IsApplicable() && ApplyMarkdownCommand(value, remove), "angle, bare, email and URL-label unlink changes actual source ownership");
        const auto after = Build(value.Text());
        Check(before->Map().Display() == after->Map().Display() &&
                  std::ranges::none_of(after->Elements(), [](const auto& node) { return node.role == SemanticRole::Link; }),
              "unlink preserves Unicode value and prevents GFM from automatically recreating the same link");
        Check(value.Undo() && value.Snapshot().IsSameRevision(autolinkSnapshot), "autolink unlink restores original URI spelling in one undo");
        unlinkRequest.command     = MarkdownCommand::SetLink;
        unlinkRequest.destination = L"#target";
        const auto update     = BuildMarkdownCommand(value.Snapshot(), value.Revision(), RichCommandTests::Parse(value), value.GetSelection(), unlinkRequest);
        const bool updated    = ApplyMarkdownCommand(value, update);
        const auto retargeted = Build(value.Text());
        if (! updated || retargeted->Map().Display() != before->Map().Display())
            std::wcerr << L"retarget source=[" << source << L"] result=[" << value.Text() << L"] before=[" << before->Map().Display() << L"] after=["
                       << retargeted->Map().Display() << L"]\n";
        Check(
            updated && retargeted->Map().Display() == before->Map().Display() &&
                std::ranges::any_of(retargeted->Elements(), [](const auto& node) { return node.role == SemanticRole::Link && node.destination == L"#target"; }),
            "autolink retargeting preserves literal entity-looking URL labels and changes actual inert metadata");
    }
}
} // namespace RichSemanticDocumentTests
