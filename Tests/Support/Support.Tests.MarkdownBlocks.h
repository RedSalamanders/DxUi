#pragma once
#include "Controls/DxUi.NativeFlowchart.h"
#include "Support.Tests.NativeFlowchartCorpus.h"
#include <DxUi/MarkdownSyntax.h>
#include <iostream>
#include <tuple>
namespace MarkdownBlockTests
{
using namespace DxUi::RichText;
inline std::wstring BlockSignature(const MarkdownSyntax& syntax)
{
    std::wstring result;
    const auto& nodes = syntax.Nodes();
    struct Visit
    {
        std::size_t index;
        bool end;
    };
    std::vector<Visit> pending{{0u, false}};
    while (! pending.empty())
    {
        const auto visit = pending.back();
        pending.pop_back();
        const auto& node = nodes[visit.index];
        std::wstring tag;
        switch (node.block.kind)
        {
            case MarkdownBlockKind::Paragraph:
                if (node.parent && nodes[node.parent].block.kind == MarkdownBlockKind::ListItem && nodes[nodes[node.parent].parent].tight)
                    break;
                tag = L"p";
                break;
            case MarkdownBlockKind::AtxHeading:
            case MarkdownBlockKind::SetextHeading: tag = L"h" + std::to_wstring(node.block.heading); break;
            case MarkdownBlockKind::BlockQuote: tag = L"blockquote"; break;
            case MarkdownBlockKind::List: tag = node.listMarker == L'.' || node.listMarker == L')' ? L"ol" : L"ul"; break;
            case MarkdownBlockKind::ListItem: tag = L"li"; break;
            case MarkdownBlockKind::FootnoteDefinition: tag = L"footnote"; break;
            case MarkdownBlockKind::ThematicBreak: tag = L"hr"; break;
            case MarkdownBlockKind::FencedCode:
            case MarkdownBlockKind::IndentedCode: tag = L"pre"; break;
            case MarkdownBlockKind::HtmlBlock: tag = L"htmlblock"; break;
            case ::DxUi::RichText::MarkdownBlockKind::BlankLine:
            case ::DxUi::RichText::MarkdownBlockKind::Document:
            case ::DxUi::RichText::MarkdownBlockKind::FrontMatter:
            case ::DxUi::RichText::MarkdownBlockKind::ReferenceDefinition:
            case ::DxUi::RichText::MarkdownBlockKind::SourceEditor:
            case ::DxUi::RichText::MarkdownBlockKind::Table:
            case ::DxUi::RichText::MarkdownBlockKind::TableCell:
            case ::DxUi::RichText::MarkdownBlockKind::TableRow:
            default: break;
        }
        if (! tag.empty())
            result += (visit.end ? L"</" : L"<") + tag + L">";
        if (visit.end)
            continue;
        if (! tag.empty())
            pending.push_back({visit.index, true});
        std::vector<std::size_t> children;
        for (auto child = node.firstChild; child != MarkdownNoNode; child = nodes[child].nextSibling)
            children.push_back(child);
        for (auto child = children.rbegin(); child != children.rend(); ++child)
            pending.push_back({*child, false});
    }
    return result;
}
template <typename Check> inline void CheckDeclarationProfile(const MarkdownSyntax& syntax, bool uppercase, Check&& check)
{
    const bool html = uppercase || syntax.Profile() != MarkdownProfile::Gfm029;
    check(BlockSignature(syntax) == (html ? L"<htmlblock></htmlblock><p></p>" : L"<p></p>"),
          "declaration block start follows the exact selected CommonMark or GFM profile");
    check(syntax.Source() == (uppercase ? L"<!DOCTYPE html>\nfoo" : L"<!doctype html>\nfoo"), "profile-sensitive declaration recognition retains exact source");
}
template <typename Check> inline void CheckLazyHtmlProfile(const MarkdownSyntax& syntax, Check&& check)
{
    const auto expected =
        syntax.Profile() == MarkdownProfile::Gfm029 ? L"<blockquote><p></p></blockquote><htmlblock></htmlblock>" : L"<blockquote><p></p></blockquote>";
    check(BlockSignature(syntax) == expected && syntax.Source() == L"> a\n<custom>", "unmatched quote and type-seven HTML follow the selected pinned profile");
}
struct SplitTableScenario
{
    std::wstring_view source, paragraph, table;
};
inline constexpr SplitTableScenario splitTables[] = {
    {L"intro\n| a | b |\n| - | - |\n| 1 | 2 |\n", L"intro\n", L"| a | b |\n| - | - |\n| 1 | 2 |\n"},
    {L"> intro\r\n> | a | b |\r\n> | - | - |\r\n> | 1 | 2 |\r\n", L"> intro\r\n", L"> | a | b |\r\n> | - | - |\r\n> | 1 | 2 |\r\n"},
};
template <typename Check> inline void CheckSplitTable(const MarkdownSyntax& syntax, const SplitTableScenario& scenario, Check&& check)
{
    const auto& nodes    = syntax.Nodes();
    const auto paragraph = std::ranges::find_if(
        nodes, [&](const auto& node) { return node.block.kind == MarkdownBlockKind::Paragraph && syntax.Slice(node.block.source) == scenario.paragraph; });
    check(paragraph != nodes.end(), "earlier paragraph retains its exact source and EOL");
    if (paragraph == nodes.end())
        return;
    check(paragraph->nextSibling < nodes.size(), "table is the next sibling of the retained paragraph");
    if (paragraph->nextSibling >= nodes.size())
        return;
    const auto& table = nodes[paragraph->nextSibling];
    check(table.block.kind == MarkdownBlockKind::Table && table.parent == paragraph->parent && syntax.Slice(table.block.source) == scenario.table,
          "only the final paragraph line becomes the source-exact table header");
}
struct Scenario
{
    std::wstring_view name, source, signature;
};
struct IdentityEdge
{
    std::wstring_view source;
    Selection selection;
};
inline constexpr IdentityEdge identityEdges[] = {{L"alpha", {0u, 1u}}, {L"alpha", {4u, 5u}}, {L"\U0001f600alpha", {0u, 2u}}, {L"alpha\U0001f600", {5u, 7u}}};
inline constexpr Scenario scenarios[]         = {
    {L"nested quote and lazy continuation", L"> > alpha\ncontinued\n", L"<blockquote><blockquote><p></p></blockquote></blockquote>"},
    {L"unmatched quote before ordered start", L"> a\n2. b", L"<blockquote><p></p></blockquote><ol><li></li></ol>"},
    {L"unmatched quote before empty item", L"> a\n-", L"<blockquote><p></p></blockquote><ul><li></li></ul>"},
    {L"type-seven HTML remains lazy inside unmatched quote", L"> a\n<custom>", L"<blockquote><p></p></blockquote>"},
    {L"matched paragraph cannot be interrupted by ordered start", L"a\n2. b", L"<p></p>"},
    {L"matched paragraph interprets a single dash as setext", L"a\n-", L"<h2></h2>"},
    {L"matched paragraph cannot be interrupted by type-seven HTML", L"a\n<custom>", L"<p></p>"},
    {L"nested lists and ordered start", L"- alpha\n  1. beta\n  2. gamma\n- delta\n", L"<ul><li><ol><li></li><li></li></ol></li><li></li></ul>"},
    {L"loose list", L"- alpha\n\n- beta\n", L"<ul><li><p></p></li><li><p></p></li></ul>"},
    {L"nested list followed by a loose outer paragraph", L"* foo\n  * bar\n\n  baz\n", L"<ul><li><p></p><ul><li></li></ul><p></p></li></ul>"},
    {L"invalid heading remains lazy", L"> alpha\n#not-heading\n", L"<blockquote><p></p></blockquote>"},
    {L"invalid fence remains lazy", L"> alpha\n```invalid`\n", L"<blockquote><p></p></blockquote>"},
    {L"mixed thematic spelling is a list item", L"* _ *\n", L"<ul><li></li></ul>"},
    {L"wide empty marker padding cannot interrupt a paragraph", L"alpha\n+                         \ntext\n", L"<p></p>"},
    {L"indented code and paragraph interruption", L"alpha\n    beta\n\n    gamma\n", L"<p></p><pre></pre>"},
    {L"partial tab prefix", L">\t\tcode\n", L"<blockquote><pre></pre></blockquote>"},
    {L"container fenced code", L"> ~~~lang\r\n> # literal\r\n> ~~~\r\n", L"<blockquote><pre></pre></blockquote>"},
    {L"HTML raw-text terminator", L"<ScRiPt>\n# literal\n</style>\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"raw opening name excluded from type seven", L"<pre/>\n# heading\n", L"<p></p><h1></h1>"},
    {L"spaced raw name starts type one", L"<pre />\n# literal\n", L"<htmlblock></htmlblock>"},
    {L"raw closing name may start type seven", L"</pre>\n# literal\n\ntext\n", L"<htmlblock></htmlblock><p></p>"},
    {L"hyphenated generic tag is not a raw name", L"<pre-widget/>\n# literal\n\ntext\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML comment", L"<!--\n# literal\n-->\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML processing instruction", L"<?\n# literal\n?>\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML declaration", L"<!DOCTYPE\nhtml>\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML CDATA", L"<![CDATA[\n# literal\n]]>\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML block tag", L"<div>\n# literal\n\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"HTML custom tag", L"<custom x='a'>\n# literal\n\nafter\n", L"<htmlblock></htmlblock><p></p>"},
    {L"custom HTML does not interrupt paragraph", L"alpha\n<custom>\nbeta\n", L"<p></p>"},
    {L"block HTML interrupts paragraph", L"alpha\n<div>\nbeta\n", L"<p></p><htmlblock></htmlblock>"},
    {L"HTML spelling inside code", L"```\n<div>\n```\n", L"<pre></pre>"},
    {L"reference prefix and setext", L"[label]: /url\nTitle\n===\n", L"<h1></h1>"},
    {L"multiline reference label destination and title", L"[a\nb]:\n /url\n 'title'\n\ntext\n", L"<p></p>"},
    {L"reference does not interrupt paragraph", L"alpha\n[label]: /url\n", L"<p></p>"},
    {L"empty items", L"-\n- beta\n", L"<ul><li></li><li></li></ul>"},
    {L"ordered interruption requires one", L"alpha\n2. beta\n", L"<p></p>"},
    {L"thematic precedence", L"- alpha\n- - -\n", L"<ul><li></li></ul><hr></hr>"},
};
} // namespace MarkdownBlockTests

namespace FrontMatterTests
{
using namespace DxUi::RichText;
struct Scenario
{
    std::wstring_view name, source, content, after;
    MarkdownFrontMatterKind kind{};
    bool closed{};
};
inline constexpr Scenario scenarios[] = {
    {L"empty YAML", L"---\n---\n# after\n", L"", L"# after\n", MarkdownFrontMatterKind::Yaml, true},
    {L"YAML BOM Unicode CRLF and dot close",
     L"\ufeff---\r\n# comment\r\nname: '\U0001f600e\u0301'\r\n...\r\n# after\r\n",
     L"# comment\r\nname: '\U0001f600e\u0301'\r\n",
     L"# after\r\n",
     MarkdownFrontMatterKind::Yaml,
     true},
    {L"YAML exact close at EOF", L"---\nkey: value\n---", L"key: value\n", L"", MarkdownFrontMatterKind::Yaml, true},
    {L"YAML unclosed opener at EOF", L"---", L"", L"", MarkdownFrontMatterKind::Yaml, false},
    {L"YAML incomplete body", L"---\r\nkey: [\r\n", L"key: [\r\n", L"", MarkdownFrontMatterKind::Yaml, false},
    {L"YAML fences require exact lines", L"---\n--- \n ---\n# ---\n...\n", L"--- \n ---\n# ---\n", L"", MarkdownFrontMatterKind::Yaml, true},
    {L"YAML leaves Markdown and entities opaque",
     L"---\n# not heading\n- [x] **&amp;**\n[r]: /not-a-link\n---\n[r]\n",
     L"# not heading\n- [x] **&amp;**\n[r]: /not-a-link\n",
     L"[r]\n",
     MarkdownFrontMatterKind::Yaml,
     true},
    {L"empty TOML", L"+++\n+++\n# after\n", L"", L"# after\n", MarkdownFrontMatterKind::Toml, true},
    {L"TOML BOM and CR lines", L"\ufeff+++\r# comment\rtitle = '\u03b2'\r+++\r", L"# comment\rtitle = '\u03b2'\r", L"", MarkdownFrontMatterKind::Toml, true},
    {L"TOML does not close at YAML fences", L"+++\n---\n...\n+++", L"---\n...\n", L"", MarkdownFrontMatterKind::Toml, true},
    {L"YAML does not close at TOML fence", L"---\n+++\n...", L"+++\n", L"", MarkdownFrontMatterKind::Yaml, true},
    {L"TOML unclosed opener", L"+++\n", L"", L"", MarkdownFrontMatterKind::Toml, false},
    {L"TOML incomplete body", L"+++\nx = [1,\n", L"x = [1,\n", L"", MarkdownFrontMatterKind::Toml, false},
    {L"leading blank", L"\n---\nkey: value\n---\n", {}, {}},
    {L"later thematic break", L"text\n\n---\nkey: value\n---\n", {}, {}},
    {L"indented opener", L" ---\nkey: value\n---\n", {}, {}},
    {L"opener with trailing space", L"--- \nkey: value\n---\n", {}, {}},
    {L"quoted opener", L"> ---\n> key: value\n> ---\n", {}, {}},
    {L"code opener", L"```\n---\nkey: value\n---\n```\n", {}, {}},
    {L"JSON is not metadata", L"{\"title\": \"source\"}\n", {}, {}},
    {L"two BOMs are not an opener", L"\ufeff\ufeff---\nx: 1\n---\n", {}, {}},
};
template <typename Check> inline void CheckSyntax(const MarkdownSyntax& syntax, const Scenario& scenario, Check&& check)
{
    check(syntax.Source() == scenario.source, "metadata envelope retains exact UTF-16, comments, spelling and EOLs");
    std::size_t count = 0u;
    for (const auto& node : syntax.Nodes())
    {
        if (node.block.kind != MarkdownBlockKind::FrontMatter)
            continue;
        ++count;
        check(syntax.Profile() == MarkdownProfile::RichMarkdown && node.parent == 0u && node.block.source.first == 0u,
              "metadata is a RichMarkdown document-start leaf, never a nested or later block");
        check(node.frontMatterKind == scenario.kind && node.block.closed == scenario.closed && syntax.Slice(node.block.content) == scenario.content,
              "metadata language, incomplete-input state and fence-excluding content match independent expectations");
        check(syntax.Source().substr(node.block.source.last) == scenario.after && node.firstChild == MarkdownNoNode && node.firstInline == MarkdownNoNode,
              "metadata has no Markdown descendants and does not capture the following document");
        check(syntax.Slice(node.block.info) == (scenario.kind == MarkdownFrontMatterKind::Yaml ? L"---" : L"+++"),
              "metadata opener range excludes only the optional BOM");
    }
    const bool expected = syntax.Profile() == MarkdownProfile::RichMarkdown && scenario.kind != MarkdownFrontMatterKind::None;
    check(count == (expected ? 1u : 0u), "base profiles and non-leading or inexact fences cannot acquire metadata semantics");
    std::size_t unclosed = 0u;
    for (const auto& issue : syntax.Diagnostics())
    {
        if (issue.code != MarkdownDiagnosticCode::UnclosedFrontMatter)
            continue;
        ++unclosed;
        check(issue.node < syntax.Nodes().size() && issue.source == syntax.Nodes()[issue.node].block.info,
              "metadata missing-fence diagnostic names the exact opener and final revision-local source owner");
    }
    check(unclosed == (expected && ! scenario.closed ? 1u : 0u), "only incomplete metadata envelopes publish the missing-close diagnostic");
}
} // namespace FrontMatterTests

namespace FootnoteBlockTests
{
using namespace DxUi::RichText;
struct Scenario
{
    std::wstring_view source, signature, labels;
};
inline constexpr Scenario scenarios[] = {
    {L"[^n]: first\n", L"<footnote><p></p></footnote>", L"n"},
    {L"[^n]: first\r\n\r\n    second\r\n", L"<footnote><p></p><p></p></footnote>", L"n"},
    {L"[^n]:\nnext\n", L"<footnote></footnote><p></p>", L"n"},
    {L"[^n]: first\n[^m]: second\n", L"<footnote><p></p></footnote><footnote><p></p></footnote>", L"n\nm"},
    {L"intro\n[^n]: note\n", L"<p></p><footnote><p></p></footnote>", L"n"},
    {L"[^n]: one\nlazy\n\noutside\n", L"<footnote><p></p></footnote><p></p>", L"n"},
    {L"> [^n]: first\n>     second\n", L"<blockquote><footnote><p></p></footnote></blockquote>", L"n"},
    {L"- [^n]: first\n      second\n", L"<ul><li><footnote><p></p></footnote></li></ul>", L"n"},
    {L"[^n]:\n    [^other]: /url\n", L"<footnote><p></p></footnote>", L"n"},
    {L"```\n[^n]: body\n```\n", L"<pre></pre>", L""},
    {L"---\n[^n]: body\n---\n", L"", L""},
    {L"    [^n]: body\n", L"<pre></pre>", L""},
    {L"[^]: body\n", L"<p></p>", L""},
    {L"[^   ]: body\n", L"<p></p>", L""},
    {L"[^n: body\n", L"<p></p>", L""},
    {L"   [^a b]: body\r", L"<footnote><p></p></footnote>", L"a b"},
    {L"[^\u03b2\U0001f600]: **body**\r\n    *child*\r\n", L"<footnote><p></p></footnote>", L"\u03b2\U0001f600"},
    {L"[^a\\]b]: body\n", L"<footnote><p></p></footnote>", L"a\\]b"},
    {L"- item\n[^n]: body\n", L"<ul><li></li></ul><footnote><p></p></footnote>", L"n"},
    {L"[^n]:\n\n    - first\n    - second\n", L"<footnote><ul><li></li><li></li></ul></footnote>", L"n"},
    {L"[^n]:\n\tsecond\n", L"<footnote><p></p></footnote>", L"n"},
};
template <typename Check> inline void CheckSyntax(const MarkdownSyntax& syntax, const Scenario& scenario, Check&& check)
{
    check(syntax.Source() == scenario.source, "footnote containers retain every raw source unit and line ending");
    std::wstring labels;
    for (const auto& node : syntax.Nodes())
    {
        if (node.block.kind != MarkdownBlockKind::FootnoteDefinition)
            continue;
        check(syntax.Profile() == MarkdownProfile::RichMarkdown && node.contentIndent == 4u && node.firstInline == MarkdownNoNode,
              "footnote definitions own block children with a four-column continuation rule");
        check(node.block.source.first <= node.block.info.first && node.block.info.last <= node.block.source.last && node.label.first < node.label.last &&
                  node.block.content.last <= node.block.source.last,
              "footnote marker, raw label and body ranges belong to their source-owning container");
        if (! labels.empty())
            labels += L'\n';
        labels += syntax.Slice(node.label);
        for (auto parent = node.parent; parent != MarkdownNoNode; parent = syntax.Nodes()[parent].parent)
            check(syntax.Nodes()[parent].block.kind != MarkdownBlockKind::FootnoteDefinition, "footnote definitions do not recursively nest");
    }
    if (syntax.Profile() == MarkdownProfile::RichMarkdown)
    {
        check(labels == scenario.labels && MarkdownBlockTests::BlockSignature(syntax) == scenario.signature,
              "footnote source order, exact label spelling and block grammar match independent fixtures");
        for (const auto& node : syntax.Nodes())
            if (node.block.kind == MarkdownBlockKind::ReferenceDefinition)
                check(! syntax.Slice(node.label).starts_with(L"^"), "footnote syntax cannot leak into global ordinary link definitions");
    }
    else
        check(labels.empty(), "CommonMark and GFM retain their existing grammar without footnote containers");
}
} // namespace FootnoteBlockTests

namespace NativeFlowchartTests
{
using namespace DxUi::RichText;
using namespace DxUi::RichText::Detail;
// Independently authored DxUiNativeFlowchart1 expectations. These check the
// admitted graph, not just successful rejection of broader Mermaid syntax.
struct Scenario
{
    std::wstring_view source;
    FlowchartOutcome outcome{};
    FlowchartIssue issue{};
    std::size_t nodes{}, edges{};
    FlowchartDirection direction{};
    std::wstring_view firstLabel;
    FlowchartShape firstShape{};
};
inline constexpr Scenario scenarios[] = {
    {L"flowchart TD\nA[Start] --> B[End]\n", {}, {}, 2u, 1u, {}, L"Start"},
    {L"graph LR; A(One) --> B((Two)) --> C{Three};", {}, {}, 3u, 2u, FlowchartDirection::LeftRight, L"One", FlowchartShape::Rounded},
    {L"flowchart BT\r\nA --- B\r\n", {}, {}, 2u, 1u, FlowchartDirection::BottomTop, L"A"},
    {L"flowchart RL\nA -->|yes| B\n", {}, {}, 2u, 1u, FlowchartDirection::RightLeft, L"A"},
    {L"flowchart TB\nA[\"Unicode \u03b2 \U0001f600\"] --> B[\"second\\nline\"]\n", {}, {}, 2u, 1u, {}, L"Unicode \u03b2 \U0001f600"},
    {L"flowchart TB\n%% comment\nA[Old]; A[New]; A --> B %% trailing\n", {}, {}, 2u, 1u, {}, L"New"},
    {L"flowchart TD\nA --> B\nB --> A\nA --> A\n", {}, {}, 2u, 3u, {}, L"A"},
    {L"graph TD\nA\n", {}, {}, 1u, 0u, {}, L"A"},
    {L"flowchart TD\nA --> B --> C\n", {}, {}, 3u, 2u, {}, L"A"},
    {L"graph TB;A{Decision};A-->B;A[Updated];A;", {}, {}, 2u, 1u, {}, L"Updated"},
    {L"graph TB;A[\"a;[b] \\\"quote\\\" \\\\ end\"];A-->|\"yes; or no\"|B", {}, {}, 2u, 1u, {}, L"a;[b] \"quote\" \\ end"},
    {L"%% preface\rgraph TB\rA((circle))--- B\r", {}, {}, 2u, 1u, {}, L"circle", FlowchartShape::Circle},
    {L"graph TB; A{why};A-->xB;", {}, {}, 2u, 1u, {}, L"why", FlowchartShape::Diamond},
    {L"graph TD;A[a\\\"b];A-->a;", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD;A[\"\"];a[lower];A-->a", {}, {}, 2u, 1u, {}, L""},
    {L"graph TD;A-->|yes%%keep|B", {}, {}, 2u, 1u, {}, L"A"},
    {L"graph TD;A--- ops;A--- xB", {}, {}, 3u, 2u, {}, L"A"},
    {L"sequenceDiagram\nA->>B: hello\n", {}, {}, 2u, 1u, {}, L"A"},
    {L"flowchart TD\nsubgraph G\nA --> B\nend\n", {}, {}, 2u, 1u, {}, L"A"},
    {L"flowchart TD\nA --> B\nstyle A fill:red\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"flowchart TD\nA --> B\nclick A \"https://example.invalid\"\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"%%{init: {}}%%\nflowchart TD\nA --> B\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedConfiguration},
    {L"flowchart TD\nA[\"<b>HTML</b>\"]\n", {}, {}, 1u, 0u, {}, L"HTML"},
    {L"flowchart TD\nA@{shape: hex}\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape},
    {L"flowchart TD\nA -.-> B\n", {}, {}, 2u, 1u, {}, L"A"},
    {L"flowchart TD\nA ---oB\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge},
    {L"flowchart TD\nA ---xB\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge},
    {L"graph TD; end", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"graph TD; A[[subroutine]]", {}, {}, 1u, 0u, {}, L"subroutine", FlowchartShape::Subroutine},
    {L"graph TD; A[\"&amp;\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD; A[\"#9829;\"]", {}, {}, 1u, 0u, {}, L"\u2665"},
    {L"graph TD; A[\"fab:fa-truck\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD; A[\"$$x^2$$\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"flowchart TD\nA[unclosed\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingLabelClose},
    {L"flowchart TD\nA -->\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingTarget},
    {L"flowchart ZZ\nA --> B\n", FlowchartOutcome::Malformed, FlowchartIssue::InvalidDirection},
    {L"graph TD;A[\"bad\\q\"]", FlowchartOutcome::Malformed, FlowchartIssue::InvalidEscape},
    {L"graph TD;1A", FlowchartOutcome::Malformed, FlowchartIssue::InvalidIdentifier},
    {L"%% comment only\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingHeader},
    {L"graph TD;A((circle)", FlowchartOutcome::Malformed, FlowchartIssue::MissingLabelClose},
    {L"flowchart TD;A & --> B", FlowchartOutcome::Malformed, FlowchartIssue::UnexpectedToken},
    {L"flowchart TD;A --> & B", FlowchartOutcome::Malformed, FlowchartIssue::UnexpectedToken},
    {L"flowchart TD;A & B & --> C", FlowchartOutcome::Malformed, FlowchartIssue::UnexpectedToken},
};
inline std::shared_ptr<const FlowchartModel> Complete(NativeFlowchartParser& parser, std::size_t budget = 1u)
{
    while (parser.HasPendingPreparation())
        parser.Advance(budget);
    return parser.Published();
}
template <typename Check> inline void CheckModel(const FlowchartModel& model, const Scenario& scenario, Check&& check)
{
    if (model.source != scenario.source || model.outcome != scenario.outcome || model.diagnostic.issue != scenario.issue)
    {
        std::wcerr << L"Flowchart scenario mismatch: source=";
        std::wcerr << scenario.source;
        std::wcerr << L" expected outcome/issue=";
        std::wcerr << static_cast<unsigned>(scenario.outcome);
        std::wcerr << L"/";
        std::wcerr << static_cast<unsigned>(scenario.issue);
        std::wcerr << L" actual outcome/issue=";
        std::wcerr << static_cast<unsigned>(model.outcome);
        std::wcerr << L"/";
        std::wcerr << static_cast<unsigned>(model.diagnostic.issue);
        std::wcerr << L'\n';
    }
    check(model.source == scenario.source && model.outcome == scenario.outcome && model.diagnostic.issue == scenario.issue,
          "native diagram result preserves exact source and independently classified outcome/diagnostic");
    check(model.nodes.size() == scenario.nodes && model.edges.size() == scenario.edges, "rejected native diagrams expose no partially accepted graph");
    if (model.outcome != FlowchartOutcome::Prepared)
    {
        check(model.diagnostic.source.first <= model.diagnostic.source.last && model.diagnostic.source.last <= model.source.size(),
              "native diagram diagnostic is a counted original-source range");
        return;
    }
    check(model.direction == scenario.direction && model.nodes.front().label == scenario.firstLabel && model.nodes.front().shape == scenario.firstShape,
          "prepared native graph has expected direction, last explicit label and shape");
    if (scenario.source == L"flowchart TD\nA[\"<b>HTML</b>\"]\n")
    {
        const auto& node = model.nodes.front();
        check(node.identifier.first == 13u && node.identifier.last == 14u && node.declaration.first == 13u && node.declaration.last == 29u &&
                  node.styles.count == 1u && node.styles.spans[0u].first == 0u && node.styles.spans[0u].last == 4u && node.styles.spans[0u].flags == LabelBold,
              "bold HTML fixture preserves the exact declaration and bold text interval");
    }
    for (const auto& node : model.nodes)
        check(node.identifier.first < node.identifier.last && node.identifier.last <= model.source.size() && node.declaration.last <= model.source.size(),
              "native graph identifier and latest declaration retain raw body coordinates");
    for (const auto& edge : model.edges)
        check(edge.from < model.nodes.size() && edge.to < model.nodes.size() && edge.source.first < edge.source.last && edge.source.last <= model.source.size(),
              "native graph edges bind deterministic first-seen node identities and raw source");
}
inline std::wstring FencedSource(const Scenario& scenario)
{
    std::wstring source = L"~~~~mermaid\n";
    source += scenario.source;
    if (source.back() != L'\n' && source.back() != L'\r')
        source += L'\n';
    source += L"~~~~\n";
    return source;
}
inline std::wstring LiteralDisplay(const Scenario& scenario)
{
    std::wstring display;
    for (std::size_t i = 0u; i < scenario.source.size(); ++i)
    {
        if (scenario.source[i] == L'\r')
        {
            display += L'\n';
            if (i + 1u < scenario.source.size() && scenario.source[i + 1u] == L'\n')
                ++i;
        }
        else
            display += scenario.source[i];
    }
    if (display.empty() || display.back() != L'\n')
        display += L'\n';
    return display;
}
template <typename Check> inline void CheckFencedSource(const MarkdownSyntax& syntax, const Scenario& scenario, Check&& check)
{
    check(syntax.Source() == FencedSource(scenario), "Mermaid fence retains exact raw definition and original EOLs");
    const auto& node = syntax.Nodes()[syntax.Nodes()[0u].firstChild];
    check(node.block.kind == MarkdownBlockKind::FencedCode && node.block.closed && syntax.Slice(node.block.info) == L"mermaid",
          "native diagram definition remains an opaque source-owning code fence before renderer integration");
    const auto body = syntax.Slice(node.block.content);
    check(body.starts_with(scenario.source), "Mermaid fence body retains every counted original source unit");
    NativeFlowchartParser parser;
    parser.Begin(body.substr(0u, scenario.source.size()), syntax.Revision());
    CheckModel(*Complete(parser), scenario, check);
}
template <typename Check> inline void CheckFamilyGrammar(Check&& check);
template <typename Check> inline void CheckFanout(Check&& check);
template <typename Check> inline void CheckOriginalCorpus(Check&& check)
{
    NativeFlowchartParser parser;
    std::size_t prepared{}, unsupported{};
    for (const auto& example : NativeFlowchartCorpus::flowcharts)
    {
        parser.Begin(example.source, example.number);
        const auto result = Complete(parser, 1u);
        if (result->outcome != example.outcome || result->diagnostic.issue != example.issue || result->diagnostic.source.first != example.diagnosticFirst ||
            result->diagnostic.source.last != example.diagnosticLast || result->nodes.size() != example.nodes || result->edges.size() != example.edges ||
            (example.outcome == FlowchartOutcome::Prepared && result->direction != example.direction))
            std::wcerr << L"Mermaid original example " << example.number << L" mismatch: outcome=" << static_cast<unsigned>(result->outcome) << L" issue="
                       << static_cast<unsigned>(result->diagnostic.issue) << L" nodes=" << result->nodes.size() << L" edges=" << result->edges.size()
                       << L" direction=" << static_cast<unsigned>(result->direction) << L'\n';
        check(result->source == example.source && result->outcome == example.outcome && result->nodes.size() == example.nodes &&
                  result->edges.size() == example.edges && result->diagnostic.issue == example.issue &&
                  result->diagnostic.source.first == example.diagnosticFirst && result->diagnostic.source.last == example.diagnosticLast &&
                  ! example.reason.empty(),
              "all original Mermaid flowchart examples match independent status, graph counts, reason, issue and exact diagnostic range");
        if (example.outcome == FlowchartOutcome::Prepared)
        {
            check(result->direction == example.direction && result->diagnostic.issue == FlowchartIssue::None,
                  "admitted original Mermaid graphs preserve expected direction without a fallback diagnostic");
            ++prepared;
        }
        else
        {
            check(result->diagnostic.issue != FlowchartIssue::None && result->diagnostic.source.first <= result->diagnostic.source.last &&
                      result->diagnostic.source.last <= result->source.size(),
                  "unsupported original Mermaid syntax retains a bounded source explanation");
            ++unsupported;
        }
    }
    for (const auto& family : NativeFlowchartCorpus::familyExamples)
    {
        parser.Begin(family.source, 1u);
        const auto result = Complete(parser, 1u);
        check(result->outcome == family.outcome && result->source == family.source && result->nodes.size() == family.nodes &&
                  result->edges.size() == family.edges && family.reason.size() > 0u,
              "every original diagram family has an independently reviewed first-example outcome and reason");
        if (family.outcome == FlowchartOutcome::Prepared)
            check(result->direction == family.direction && result->diagnostic.issue == FlowchartIssue::None,
                  "prepared original family example matches its independent direction and has no diagnostic");
        else
            check(result->diagnostic.issue == family.issue && result->nodes.empty() && result->edges.empty() &&
                      result->diagnostic.source.first < result->diagnostic.source.last && result->diagnostic.source.last <= result->source.size(),
                  "unsupported original family examples retain their independently classified diagnostic and exact source fallback");
    }
    check(prepared == 41u && unsupported == 64u && std::size(NativeFlowchartCorpus::familyExamples) == 20u,
          "independent Mermaid subset expectations separate graph preparation from rejection");
    std::array<std::size_t, 4u> outcomes{};
    for (const auto& fence : NativeFlowchartCorpus::allFences)
    {
        parser.Begin(fence.source, fence.number);
        std::size_t work{};
        const auto maximumWork = 16u * (fence.source.size() + 1u);
        while (parser.HasPendingPreparation() && work < maximumWork)
        {
            const auto result = parser.Advance(1u);
            check(result.workUnits == 1u, "pinned Mermaid parser advances by the requested bounded work unit");
            ++work;
        }
        check(! parser.HasPendingPreparation(), "every pinned Mermaid fence finishes within its finite parser work bound");
        const auto result = parser.Published();
        check(result && result->source == fence.source && result->revision == fence.number && ! fence.document.empty(),
              "every pinned Mermaid fence retains its complete counted definition and inventory identity");
        const auto outcome = static_cast<std::size_t>(result->outcome);
        check(outcome < outcomes.size(), "every pinned Mermaid fence publishes a known outcome");
        ++outcomes[outcome];
        if (result->outcome == FlowchartOutcome::Prepared)
        {
            check(result->diagnostic.issue == FlowchartIssue::None && result->nodes.size() <= NativeFlowchartParser::MaximumNodes &&
                      result->edges.size() <= NativeFlowchartParser::MaximumEdges,
                  "admitted original Mermaid fences respect model capacity without a fallback diagnostic");
            for (const auto& edge : result->edges)
                check(edge.from < result->nodes.size() && edge.to < result->nodes.size(),
                      "admitted original Mermaid fences reference only published endpoints");
        }
        else
            check(result->nodes.empty() && result->edges.empty() && result->groups.empty() && result->diagnostic.issue != FlowchartIssue::None &&
                      result->diagnostic.source.first <= result->diagnostic.source.last && result->diagnostic.source.last <= result->source.size(),
                  "declined original Mermaid fences retain bounded diagnostics and expose no partial model");
    }
    check(std::size(NativeFlowchartCorpus::allFences) == 342u, "all pinned Mermaid fences run, including every family and example collection");
    std::cout << "Pinned Mermaid fences: " << std::size(NativeFlowchartCorpus::allFences) << "; prepared=" << outcomes[0u] << "; malformed=" << outcomes[1u]
              << "; unsupported=" << outcomes[2u] << "; capacity=" << outcomes[3u] << '\n';
    CheckFamilyGrammar(check);
    CheckFanout(check);
}

inline wchar_t FamilyCode(NativeDiagramFamily family) noexcept
{
    switch (family)
    {
        case NativeDiagramFamily::Flowchart: return L'F';
        case NativeDiagramFamily::State: return L'S';
        case NativeDiagramFamily::Class: return L'C';
        case NativeDiagramFamily::Sequence: return L'Q';
        case NativeDiagramFamily::Pie: return L'P';
        case NativeDiagramFamily::EntityRelationship: return L'E';
        case NativeDiagramFamily::Gantt: return L'G';
        case NativeDiagramFamily::Mindmap: return L'M';
    }
    return L'?';
}
inline wchar_t DirectionCode(FlowchartDirection direction) noexcept
{
    switch (direction)
    {
        case FlowchartDirection::TopBottom: return L'T';
        case FlowchartDirection::BottomTop: return L'B';
        case FlowchartDirection::LeftRight: return L'L';
        case FlowchartDirection::RightLeft: return L'R';
    }
    return L'?';
}
inline wchar_t ShapeCode(FlowchartShape shape) noexcept
{
    switch (shape)
    {
        case FlowchartShape::Rectangle: return L'R';
        case FlowchartShape::Rounded: return L'O';
        case FlowchartShape::Circle: return L'C';
        case FlowchartShape::Diamond: return L'D';
        case FlowchartShape::Stadium: return L'S';
        case FlowchartShape::Bar: return L'B';
        case FlowchartShape::Hexagon: return L'H';
        case FlowchartShape::Subroutine: return L'U';
        case FlowchartShape::DoubleCircle: return L'2';
        case FlowchartShape::Cylinder: return L'Y';
        case FlowchartShape::Asymmetric: return L'A';
        case FlowchartShape::Parallelogram: return L'P';
        case FlowchartShape::ReverseParallelogram: return L'Q';
        case FlowchartShape::Trapezoid: return L'T';
        case FlowchartShape::ReverseTrapezoid: return L'V';
        case FlowchartShape::Ellipse: return L'E';
        case FlowchartShape::Note: return L'N';
    }
    return L'?';
}
inline std::wstring ModelSignature(const FlowchartModel& model)
{
    std::wstring result{FamilyCode(model.family), L'|', DirectionCode(model.direction), L'|'};
    for (const auto& node : model.nodes)
    {
        result.append(node.key).append(L"=").append(node.label).push_back(L':');
        result.push_back(ShapeCode(node.shape));
        result.push_back(L';');
    }
    result.push_back(L'|');
    for (const auto& edge : model.edges)
    {
        result.append(std::to_wstring(edge.from)).push_back(L'>');
        result.append(std::to_wstring(edge.to)).push_back(L':');
        result.push_back(edge.relation == FlowchartRelation::Inheritance   ? L'I'
                         : edge.relation == FlowchartRelation::Aggregation ? L'G'
                         : edge.relation == FlowchartRelation::Composition ? L'C'
                                                                           : (edge.arrow ? L'A' : L'L'));
        result.push_back(L':');
        result.append(edge.label).push_back(L';');
    }
    return result;
}
struct FamilyGrammarExpectation
{
    std::wstring_view source;
    FlowchartOutcome outcome{};
    FlowchartIssue issue{};
    std::wstring_view signature;
};
inline constexpr FamilyGrammarExpectation familyGrammarExpectations[] = {
    {L"stateDiagram-v2\nstate \"Ready\" as S\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready:O;|"},
    {L"stateDiagram-v2\nstate\t  \"Ready\" as S\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready:O;|"},
    {L"stateDiagram-v2\nstate \"ready \\\"now\\\"\" as S\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=ready \"now\":O;|"},
    {L"stateDiagram-v2\nS : path\\next\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=path\\next:O;|"},
    {L"stateDiagram-v2\nA --> B : path\\next\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|A=A:O;B=B:O;|0>1:A:path\\next;"},
    {L"stateDiagram-v2\nS : Ready\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready:O;|"},
    {L"stateDiagram-v2\nS : Ready\nS : Running\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready\nRunning:O;|"},
    {L"stateDiagram-v2\nstate \"Ready\" as S\nS : Running\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready\nRunning:O;|"},
    {L"stateDiagram-v2\nS : Ready\nstate \"Running\" as S\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=Ready\nRunning:O;|"},
    {L"stateDiagram-v2\nS : S\nS : Running\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=S\nRunning:O;|"},
    {L"stateDiagram-v2\nstate \"\" as S\nS : \n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|S=S:O;|"},
    {L"classDiagram\nS : Ready\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|S=S\nReady:R;|"},
    {L"stateDiagram-v2\nstate F <<fork>>\nstate J <<join>>\nA --> F\nF --> J\nJ --> B\n",
     FlowchartOutcome::Prepared,
     FlowchartIssue::None,
     L"S|T|F=:B;J=:B;A=A:O;B=B:O;|2>0:A:;0>1:A:;1>3:A:;"},
    {L"stateDiagram-v2\nstate F <<fork>>\nF : label\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
    {L"stateDiagram-v2\nstate F <<fork>>\nstate F <<join>>\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
    {L"stateDiagram-v2\nstate Choice <<choice>>\nA --> Choice\nChoice --> B : yes\n",
     FlowchartOutcome::Prepared,
     FlowchartIssue::None,
     L"S|T|Choice=:D;A=A:O;B=B:O;|1>0:A:;0>2:A:yes;"},
    {L"stateDiagram-v2\nstate Choice <<choice>>\nChoice : label\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
    {L"stateDiagram-v2\nChoice <<choice>>\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge, {}},
    {L"stateDiagram-v2\nA --> B : go\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|T|A=A:O;B=B:O;|0>1:A:go;"},
    {L"stateDiagram-v2\ndirection   LR\nA --> B\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"S|L|A=A:O;B=B:O;|0>1:A:;"},
    {L"classDiagram\nclass Animal {\n+name: String\n+run()\n}\nAnimal -- Pond\n",
     FlowchartOutcome::Prepared,
     FlowchartIssue::None,
     L"C|T|Animal=Animal\n+name: String\n+run():R;Pond=Pond:R;|0>1:L:;"},
    {L"classDiagram\nclass Empty {\n}\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|Empty=Empty:R;|"},
    {L"classDiagram\nclass A {\n+field: int\n", FlowchartOutcome::Malformed, FlowchartIssue::UnexpectedToken, {}},
    {L"classDiagram\nclass A {\n+field: T~int~\n}\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|A=A\n+field: T<int>:R;|"},
    {L"classDiagram\nclass A {\nclass B {\n}\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
    {L"classDiagram\nAnimal <|-- Duck\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|Animal=Animal:R;Duck=Duck:R;|1>0:I:;"},
    {L"classDiagram\nBase --|> Derived\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|Base=Base:R;Derived=Derived:R;|0>1:I:;"},
    {L"flowchart LR\nid1([This is the text in the box])\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|L|id1=This is the text in the box:S;|"},
    {L"flowchart TD\nA[path\\next]\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|T|A=path\\next:R;|"},
    {L"flowchart TD\nA[\"path\\nnext\"]\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|T|A=path\nnext:R;|"},
    {L"flowchart TD\nA(-x-)\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|T|A=x:E;|"},
    {L"flowchart TD\nA[|field:value|text]\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape, {}},
    {L"flowchart TD\nA(\"-x-\")\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|T|A=-x-:O;|"},
    {L"flowchart TD\nA[\"|field:value|text\"]\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"F|T|A=|field:value|text:R;|"},
    {L"stateDiagram-v2\nStill{x} --> Moving\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape, {}},
    {L"stateDiagram-v2\nA -->|go| B\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge, {}},
    {L"classDiagram\nAnimal[Creature]\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape, {}},
    {L"classDiagram\nAnimal -->|uses| Pond\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge, {}},
    {L"stateDiagram-v2\nstate \"<b>Ready</b>\" as S\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup, {}},
    {L"stateDiagram-v2\nS : <b>Ready</b>\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup, {}},
    {L"stateDiagram-v2\nA --> B : go &amp; stop\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup, {}},
    {L"classDiagram\nStack : +push(T~int~ item)\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|Stack=Stack\n+push(T<int> item):R;|"},
    {L"classDiagram\nCar --* Wheel\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|Car=Car:R;Wheel=Wheel:R;|0>1:C:;"},
    {L"classDiagram\nnote \"From Duck till Zebra\"\n", FlowchartOutcome::Prepared, FlowchartIssue::None, L"C|T|@note0=From Duck till Zebra:N;|"},
    {L"classDiagram\nA:::service\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
    {L"stateDiagram-v2\nA:::active\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement, {}},
};
template <typename Check> inline void CheckFamilyGrammar(Check&& check)
{
    NativeFlowchartParser parser;
    parser.Begin(L"flowchart TD\nsubgraph O[Outer]\nsubgraph I[Inner]\nA-->B\nend\nend\n", 1u);
    const auto nestedGroups = Complete(parser);
    check(nestedGroups->outcome == FlowchartOutcome::Prepared && nestedGroups->groups.size() == 2u && nestedGroups->nodes.size() == 2u &&
              nestedGroups->groups[0].title == L"Outer" && nestedGroups->groups[1].title == L"Inner" && nestedGroups->groups[1].parentMask == 1u &&
              nestedGroups->nodes[0].groupMask == 3u && nestedGroups->nodes[1].groupMask == 3u,
          "nested group titles and source membership retain their parent relationship");
    parser.Begin(L"stateDiagram-v2\nstate First {\n[*]-->A\nA-->[*]\n}\nstate Second {\n[*]-->B\nB-->[*]\n}\n", 1u);
    const auto scopedTerminals = Complete(parser);
    check(scopedTerminals->outcome == FlowchartOutcome::Prepared && scopedTerminals->groups.size() == 2u && scopedTerminals->nodes.size() == 6u &&
              scopedTerminals->edges.size() == 4u && scopedTerminals->nodes[0].key != scopedTerminals->nodes[3].key,
          "initial and final pseudostates have distinct identities in separate composites");
    parser.Begin(L"stateDiagram-v2\nstate \"Outer title\" as O {\nstate I {\nA-->B\n}\n}\n", 1u);
    const auto nestedStates = Complete(parser);
    check(nestedStates->outcome == FlowchartOutcome::Prepared && nestedStates->groups.size() == 2u && nestedStates->groups[0].title == L"Outer title" &&
              nestedStates->groups[1].parentMask == 1u,
          "state composites retain quoted titles and nested membership");
    parser.Begin(L"stateDiagram-v2\nstate O {\nS\nnote left of S: Review this state\n}\n", 1u);
    const auto stateNote = Complete(parser);
    check(stateNote->outcome == FlowchartOutcome::Prepared && stateNote->nodes.size() == 2u && stateNote->edges.size() == 1u &&
              stateNote->nodes[1].shape == FlowchartShape::Note && stateNote->nodes[1].noteTarget == 0u && stateNote->nodes[1].noteLeft &&
              stateNote->nodes[1].groupMask == 1u && stateNote->nodes[1].label == L"Review this state" &&
              stateNote->edges[0].relation == FlowchartRelation::Note && stateNote->edges[0].style == FlowchartLineStyle::Dotted,
          "state note retains its target, requested side, text and composite membership");
    parser.Begin(L"stateDiagram-v2\nnote right of S\nfirst; 100%\n\nsecond %% literal\nend note\n", 1u);
    const auto multilineNote = Complete(parser);
    check(multilineNote->outcome == FlowchartOutcome::Prepared && multilineNote->nodes.size() == 2u &&
              multilineNote->nodes[1].label == L"first; 100%\n\nsecond %% literal" && multilineNote->nodes[1].shape == FlowchartShape::Note,
          "multiline state notes retain punctuation, empty lines and source-order text");
    parser.Begin(L"stateDiagram-v2\nnote right of S\nmissing terminator\n", 1u);
    const auto unclosedNote = Complete(parser);
    check(unclosedNote->outcome == FlowchartOutcome::Malformed && unclosedNote->diagnostic.issue == FlowchartIssue::UnexpectedToken &&
              unclosedNote->nodes.empty() && unclosedNote->edges.empty(),
          "unterminated state notes preserve source without a partial graph");
    std::wstring maximumNote = L"stateDiagram-v2\nnote right of S\n";
    maximumNote.append(NativeFlowchartParser::MaximumLabelUnits, L'x');
    maximumNote += L"\nend note\n";
    parser.Begin(maximumNote, 1u);
    const auto admittedNote = Complete(parser);
    check(admittedNote->outcome == FlowchartOutcome::Prepared && admittedNote->nodes.size() == 2u &&
              admittedNote->nodes[1].label.size() == NativeFlowchartParser::MaximumLabelUnits,
          "multiline note admits the exact label capacity without charging an unrendered trailing newline");
    maximumNote.insert(maximumNote.find(L"\nend note"), 1u, L'x');
    parser.Begin(maximumNote, 1u);
    const auto oversizedNote = Complete(parser);
    check(oversizedNote->outcome == FlowchartOutcome::Capacity && oversizedNote->diagnostic.issue == FlowchartIssue::LabelCapacity &&
              oversizedNote->nodes.empty(),
          "oversized multiline notes discard staged graph state");
    for (const auto source : {L"graph TD\nsubgraph G\nA\n", L"graph TD\nsubgraph A\nend\n", L"graph TD\nsubgraph A\nX\nend\nsubgraph B\nX\nend\n"})
    {
        parser.Begin(source, 1u);
        const auto rejected = Complete(parser);
        check(rejected->outcome != FlowchartOutcome::Prepared && rejected->source == source && rejected->nodes.empty() && rejected->groups.empty(),
              "unclosed, empty and conflicting groups preserve exact source without a partial graph");
    }
    for (const auto& [source, from, to] :
         {std::tuple{L"classDiagram\nInterface <|.. Implementation\n", 1u, 0u}, std::tuple{L"classDiagram\nImplementation ..|> Interface\n", 0u, 1u}})
    {
        parser.Begin(source, 1u);
        const auto model = Complete(parser);
        check(model->outcome == FlowchartOutcome::Prepared && model->edges.size() == 1u && model->edges[0].from == from && model->edges[0].to == to &&
                  model->edges[0].relation == FlowchartRelation::Realization && model->edges[0].style == FlowchartLineStyle::Dotted,
              "class realization uses a dotted line and hollow inheritance endpoint in either spelling");
    }
    parser.Begin(L"classDiagram\nClient .. Service\n", 1u);
    const auto association = Complete(parser);
    check(association->outcome == FlowchartOutcome::Prepared && association->edges.size() == 1u && association->edges[0].relation == FlowchartRelation::Line &&
              association->edges[0].style == FlowchartLineStyle::Dotted,
          "class dotted association retains its line type and no arrowhead");
    for (const auto& [op, reverse, relation, style] : {std::tuple{L"*--", true, FlowchartRelation::Composition, FlowchartLineStyle::Solid},
                                                       std::tuple{L"--*", false, FlowchartRelation::Composition, FlowchartLineStyle::Solid},
                                                       std::tuple{L"o--", true, FlowchartRelation::Aggregation, FlowchartLineStyle::Solid},
                                                       std::tuple{L"--o", false, FlowchartRelation::Aggregation, FlowchartLineStyle::Solid},
                                                       std::tuple{L"*..", true, FlowchartRelation::Composition, FlowchartLineStyle::Dotted},
                                                       std::tuple{L"..*", false, FlowchartRelation::Composition, FlowchartLineStyle::Dotted},
                                                       std::tuple{L"o..", true, FlowchartRelation::Aggregation, FlowchartLineStyle::Dotted},
                                                       std::tuple{L"..o", false, FlowchartRelation::Aggregation, FlowchartLineStyle::Dotted}})
    {
        const auto source = std::wstring(L"classDiagram\nA ") + op + L" B\n";
        parser.Begin(source, 1u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->edges.size() == 1u && result->edges[0].from == (reverse ? 1u : 0u) &&
                  result->edges[0].to == (reverse ? 0u : 1u) && result->edges[0].relation == relation && result->edges[0].style == style,
              "composition and aggregation preserve their diamond endpoint and line style");
    }
    parser.Begin(L"classDiagram\nA : +size: int\nA : +draw()\nA : +other: int\n<<interface>> A\n", 1u);
    const auto compartments = Complete(parser);
    check(compartments->outcome == FlowchartOutcome::Prepared && compartments->nodes.size() == 1u &&
              compartments->nodes[0].label == L"«interface»\nA\n+size: int\n+draw()\n+other: int" && compartments->nodes[0].compartmentCount == 3u &&
              compartments->nodes[0].compartmentStarts[0] == 14u && compartments->nodes[0].compartmentStarts[1] == 25u &&
              compartments->nodes[0].compartmentStarts[2] == 33u,
          "class compartments follow source group changes and survive a later annotation prefix");
    std::wstring tooMany = L"classDiagram\nclass A {\n";
    for (std::size_t index = 0u; index < 17u; ++index)
        tooMany += index % 2u ? L"f()\n" : L"v\n";
    tooMany += L"}\n";
    parser.Begin(tooMany, 1u);
    const auto limitedCompartments = Complete(parser);
    check(limitedCompartments->outcome == FlowchartOutcome::Capacity && limitedCompartments->source == tooMany && limitedCompartments->nodes.empty(),
          "excess class compartments preserve source and discard staged graph");
    parser.Begin(L"classDiagram\nInterface \"1\" <|.. \"0..*\" Implementation : uses\n", 1u);
    const auto multiplicities = Complete(parser);
    check(multiplicities->outcome == FlowchartOutcome::Prepared && multiplicities->edges.size() == 1u && multiplicities->edges[0].from == 1u &&
              multiplicities->edges[0].to == 0u && multiplicities->edges[0].fromCardinality == L"0..*" && multiplicities->edges[0].toCardinality == L"1" &&
              multiplicities->edges[0].label == L"uses",
          "class cardinalities follow their original endpoints when relations reverse");
    parser.Begin(L"classDiagram\nA \"invalid!\" --> B\n", 1u);
    const auto invalidMultiplicity = Complete(parser);
    check(invalidMultiplicity->outcome == FlowchartOutcome::Unsupported && invalidMultiplicity->nodes.empty(),
          "unsupported cardinality preserves source without a partial graph");
    for (const auto& expected : familyGrammarExpectations)
    {
        parser.Begin(expected.source, 1u);
        const auto model = Complete(parser);
        check(model->source == expected.source && model->outcome == expected.outcome && model->diagnostic.issue == expected.issue,
              "state/class grammar has independent family, outcome and diagnostic expectations");
        if (expected.outcome == FlowchartOutcome::Prepared)
        {
            check(ModelSignature(*model) == expected.signature, "state/class accepted model matches its complete normalized golden");
        }
        else
        {
            check(model->nodes.empty() && model->edges.empty() && model->diagnostic.source.first < model->diagnostic.source.last &&
                      model->diagnostic.source.last <= model->source.size(),
                  "state/class rejection retains exact source, a bounded diagnostic and no partial graph");
        }
    }
    for (const auto source : {L"stateDiagram-v2\nA & B --> C\n", L"classDiagram\nA & B --> C\n"})
    {
        parser.Begin(source, 1u);
        const auto isolated = Complete(parser);
        check(isolated->outcome == FlowchartOutcome::Unsupported && isolated->diagnostic.issue == FlowchartIssue::UnsupportedEdge && isolated->nodes.empty() &&
                  isolated->edges.empty(),
              "fan-out separators remain unsupported in state and class grammar");
    }
    std::wstring oversizedAlias = L"stateDiagram-v2\nstate \"";
    oversizedAlias.append(NativeFlowchartParser::MaximumLabelUnits + 1u, L'x');
    oversizedAlias += L"\" as TooLong\n";
    parser.Begin(oversizedAlias, 2u);
    const auto capacity = Complete(parser);
    check(capacity->outcome == FlowchartOutcome::Capacity && capacity->diagnostic.issue == FlowchartIssue::LabelCapacity && capacity->nodes.empty() &&
              capacity->edges.empty() && capacity->diagnostic.source.last <= capacity->source.size(),
          "oversized state alias is reported as label capacity and publishes no partial graph");
    struct LineCase
    {
        std::wstring_view token;
        FlowchartLineStyle style;
        bool arrow;
    };
    for (const auto& item : {LineCase{L"-.->", FlowchartLineStyle::Dotted, true},
                             LineCase{L"-.-", FlowchartLineStyle::Dotted, false},
                             LineCase{L"==>", FlowchartLineStyle::Thick, true},
                             LineCase{L"===", FlowchartLineStyle::Thick, false},
                             LineCase{L"~~~", FlowchartLineStyle::Invisible, false}})
    {
        const auto source = L"flowchart LR\nA " + std::wstring(item.token) + L" B\n";
        parser.Begin(source, 20u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->edges.size() == 1u &&
                  result->edges[0u].style == item.style && result->edges[0u].arrow == item.arrow,
              "flowchart dotted, thick and invisible operators retain their independent style and arrow semantics");
    }
    parser.Begin(L"flowchart LR\nA ~~~|hidden| B\n", 21u);
    const auto invalidInvisible = Complete(parser);
    check(invalidInvisible->outcome == FlowchartOutcome::Unsupported && invalidInvisible->nodes.empty() && invalidInvisible->edges.empty(),
          "an unsupported invisible-edge label retains source instead of silently displaying it");
    for (const auto& item : {LineCase{L"-- hello world -->", FlowchartLineStyle::Solid, true},
                             LineCase{L"-- hello world ---", FlowchartLineStyle::Solid, false},
                             LineCase{L"-. hello world .->", FlowchartLineStyle::Dotted, true},
                             LineCase{L"-. hello world .-", FlowchartLineStyle::Dotted, false},
                             LineCase{L"== hello world ==>", FlowchartLineStyle::Thick, true},
                             LineCase{L"== hello world ===", FlowchartLineStyle::Thick, false}})
    {
        const auto source = L"flowchart LR\nA " + std::wstring(item.token) + L" B\n";
        parser.Begin(source, 23u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->edges.size() == 1u &&
                  result->edges[0u].style == item.style && result->edges[0u].arrow == item.arrow && result->edges[0u].label == L"hello world",
              "inline edge labels retain text, line style and arrow semantics");
    }
    for (const auto source : {L"flowchart LR\nA -- missing B\n", L"flowchart LR\nA -- text -->|other| B\n"})
    {
        parser.Begin(source, 24u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Unsupported && result->source == source && result->nodes.empty() && result->edges.empty(),
              "incomplete or doubly labelled inline edges preserve exact source");
    }
    for (const auto source : {L"flowchart LR\nA -- \"quoted\" --> B\n", L"flowchart LR\nA -- a|b --> B\n"})
    {
        parser.Begin(source, 26u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Unsupported && result->diagnostic.issue == FlowchartIssue::UnsupportedLabelMarkup &&
                  result->source == source && result->nodes.empty() && result->edges.empty(),
              "inline edge markup is retained as source rather than displayed literally");
    }
    for (const auto& [token, style, arrow] : {std::tuple{L"---->", FlowchartLineStyle::Solid, true},
                                              std::tuple{L"-----", FlowchartLineStyle::Solid, false},
                                              std::tuple{L"-...->", FlowchartLineStyle::Dotted, true},
                                              std::tuple{L"-...-", FlowchartLineStyle::Dotted, false},
                                              std::tuple{L"====>", FlowchartLineStyle::Thick, true},
                                              std::tuple{L"=====", FlowchartLineStyle::Thick, false},
                                              std::tuple{L"-- label ---->", FlowchartLineStyle::Solid, true},
                                              std::tuple{L"-. label ...->", FlowchartLineStyle::Dotted, true},
                                              std::tuple{L"== label ====>", FlowchartLineStyle::Thick, true}})
    {
        const auto source = L"flowchart LR\nA " + std::wstring(token) + L" B\n";
        parser.Begin(source, 27u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->edges.size() == 1u && result->edges[0u].minimumRanks == 3u &&
                  result->edges[0u].style == style && result->edges[0u].arrow == arrow,
              "extended visible links retain three requested ranks with their style and arrow");
    }
    parser.Begin(L"flowchart LR\nA ------------------> B\n", 28u);
    const auto excessiveRanks = Complete(parser);
    check(excessiveRanks->outcome == FlowchartOutcome::Capacity && excessiveRanks->diagnostic.issue == FlowchartIssue::LinkRankCapacity &&
              excessiveRanks->source == L"flowchart LR\nA ------------------> B\n" && excessiveRanks->nodes.empty(),
          "link rank requests beyond the bounded profile retain exact source");
    for (const auto& [source, label] : {std::pair{L"flowchart LR\nA[\"#quot;#9829;#NotEqualTilde;\"]\n", L"\"♥≂̸"},
                                        std::pair{L"flowchart LR\nA[one<br>two<br/>three<br />four]\n", L"one\ntwo\nthree\nfour"},
                                        std::pair{L"flowchart LR\nA[#128512;]\n", L"😀"}})
    {
        parser.Begin(source, 29u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->nodes.size() == 1u && result->nodes[0u].label == label,
              "Mermaid decimal and named entities and supported line breaks decode into exact label scalars");
    }
    parser.Begin(L"flowchart LR\nA -->|#quot;<br>#9829;| B\n", 30u);
    const auto entityEdge = Complete(parser);
    check(entityEdge->outcome == FlowchartOutcome::Prepared && entityEdge->edges.size() == 1u && entityEdge->edges[0u].label == L"\"\n♥",
          "pipe edge labels decode semicolon-terminated entities without splitting the statement");
    for (const auto source :
         {L"flowchart LR\nA[#bogus;]\n", L"flowchart LR\nA[#1;]\n", L"flowchart LR\nA[#128;]\n", L"flowchart LR\nA[<script>text</script>]\n"})
    {
        parser.Begin(source, 31u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Unsupported && result->diagnostic.issue == FlowchartIssue::UnsupportedLabelMarkup &&
                  result->source == source && result->nodes.empty() && result->edges.empty(),
              "unknown entities, unsupported controls and arbitrary HTML preserve exact source");
    }
    std::wstring entityOverflow = L"flowchart LR\nA[";
    entityOverflow.append(NativeFlowchartParser::MaximumLabelUnits - 1u, L'a');
    entityOverflow += L"#128512;]\n";
    parser.Begin(entityOverflow, 32u);
    const auto entityCapacity = Complete(parser);
    check(entityCapacity->outcome == FlowchartOutcome::Capacity && entityCapacity->diagnostic.issue == FlowchartIssue::LabelCapacity &&
              entityCapacity->source == entityOverflow && entityCapacity->nodes.empty(),
          "supplementary decoded entities count both UTF-16 units before label admission");
    for (const auto& [source, label] : {std::pair{L"flowchart LR\nA[\"C# app\"]\n", L"C# app"},
                                        std::pair{L"flowchart LR\nA[\"Issue #12\"]\n", L"Issue #12"},
                                        std::pair{L"flowchart LR\nA[\"x > 5\"]\n", L"x > 5"},
                                        std::pair{L"flowchart LR\nA[\"x < 5\"]\n", L"x < 5"},
                                        std::pair{L"flowchart LR\nA[\"Progress so far: 3/5\"]\n", L"Progress so far: 3/5"},
                                        std::pair{L"flowchart LR\nA[\"Sofa: 3 seats\"]\n", L"Sofa: 3 seats"}})
    {
        parser.Begin(source, 39u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->nodes.size() == 1u && result->nodes[0u].label == label,
              "ordinary hash, comparison and icon-like substrings remain plain text");
    }
    for (const auto source : {L"flowchart LR\nA[fa:fa-home]\n", L"flowchart LR\nA[\"<script>bad</script>\"]\n", L"flowchart LR\nA[#bogus;]\n"})
    {
        parser.Begin(source, 40u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Unsupported && result->source == source && result->nodes.empty(),
              "real icon, HTML and unknown entity syntax retains exact source");
    }
    std::wstring oversizedInline = L"flowchart LR\nA -- ";
    oversizedInline.append(NativeFlowchartParser::MaximumLabelUnits + 1u, L'x');
    oversizedInline += L" --> B\n";
    parser.Begin(oversizedInline, 25u);
    const auto inlineCapacity = Complete(parser);
    check(inlineCapacity->outcome == FlowchartOutcome::Capacity && inlineCapacity->diagnostic.issue == FlowchartIssue::LabelCapacity &&
              inlineCapacity->source == oversizedInline && inlineCapacity->nodes.empty() && inlineCapacity->edges.empty(),
          "oversized inline edge text retains source and publishes no partial graph");
    // Mermaid 11.4.1 shapeUtil.ts presents the first annotation in guillemets above the class name.
    for (const auto source : {L"classDiagram\nclass Shape\n<<interface>> Shape\nShape : draw()\n", L"classDiagram\nclass Shape {\n<<interface>>\ndraw()\n}\n"})
    {
        parser.Begin(source, 33u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->nodes.size() == 1u &&
                  result->nodes[0u].label == L"«interface»\nShape\ndraw()",
              "standalone and block class annotations render the same bounded stereotype label");
    }
    for (const auto source : {L"classDiagram\n<<interface>> Missing\n",
                              L"classDiagram\nclass A\n<<interface>> A\n<<service>> A\n",
                              L"classDiagram\nclass A {\n<<bad-annotation>>\n}\n"})
    {
        parser.Begin(source, 34u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Unsupported && result->source == source && result->nodes.empty() && result->edges.empty(),
              "undefined, repeated and unsupported class annotations retain exact source");
    }
    parser.Begin(L"flowchart TD\nA{{label}}-->B\n", 35u);
    const auto hexagon = Complete(parser);
    check(hexagon->outcome == FlowchartOutcome::Prepared && hexagon->nodes.size() == 2u && hexagon->nodes[0u].shape == FlowchartShape::Hexagon &&
              hexagon->nodes[0u].label == L"label" && hexagon->edges.size() == 1u,
          "double-brace flowchart nodes retain the hexagon shape and label");
    parser.Begin(L"flowchart TD\nA{{label}\n", 36u);
    const auto unclosedHexagon = Complete(parser);
    check(unclosedHexagon->outcome == FlowchartOutcome::Malformed && unclosedHexagon->diagnostic.issue == FlowchartIssue::MissingLabelClose &&
              unclosedHexagon->nodes.empty(),
          "missing hexagon closer never publishes a partial graph");
    parser.Begin(L"flowchart TD\nA[[operation]]-->B\n", 37u);
    const auto subroutine = Complete(parser);
    check(subroutine->outcome == FlowchartOutcome::Prepared && subroutine->nodes.size() == 2u && subroutine->nodes[0u].shape == FlowchartShape::Subroutine &&
              subroutine->nodes[0u].label == L"operation",
          "double-bracket flowchart nodes retain the subroutine shape and label");
    parser.Begin(L"flowchart TD\nA[[operation]\n", 38u);
    const auto unclosedSubroutine = Complete(parser);
    check(unclosedSubroutine->outcome == FlowchartOutcome::Malformed && unclosedSubroutine->diagnostic.issue == FlowchartIssue::MissingLabelClose &&
              unclosedSubroutine->nodes.empty(),
          "missing subroutine closer never publishes a partial graph");
    for (const auto family : {L"flowchart TD", L"stateDiagram-v2", L"classDiagram"})
    {
        const auto source = std::wstring(family) + L"\naccTitle: Overview β\naccDescr {\nFirst line; 100%\n\nSecond line\n}\nA-->B\n";
        parser.Begin(source, 41u);
        const auto result = Complete(parser);
        check(result->outcome == FlowchartOutcome::Prepared && result->source == source && result->accessibleTitle == L"Overview β" &&
                  result->accessibleDescription == L"First line; 100%\n\nSecond line" && result->nodes.size() == 2u,
              "bounded accessibility title and multiline description apply to every native diagram family");
    }
    for (const auto source :
         {L"flowchart TD\naccTitle: one\naccTitle: two\nA\n", L"flowchart TD\naccDescr {\nmissing close\n", L"flowchart TD\naccDescr: one\naccDescr: two\nA\n"})
    {
        parser.Begin(source, 42u);
        const auto result = Complete(parser);
        check(result->outcome != FlowchartOutcome::Prepared && result->source == source && result->nodes.empty() && result->edges.empty(),
              "duplicate or unclosed accessibility metadata retains exact source without a partial graph");
    }
    std::wstring longDescription = L"flowchart TD\naccDescr: ";
    longDescription.append(NativeFlowchartParser::MaximumLabelUnits + 1u, L'x');
    longDescription += L"\nA\n";
    parser.Begin(longDescription, 43u);
    const auto descriptionCapacity = Complete(parser);
    check(descriptionCapacity->outcome == FlowchartOutcome::Capacity && descriptionCapacity->diagnostic.issue == FlowchartIssue::LabelCapacity &&
              descriptionCapacity->source == longDescription && descriptionCapacity->nodes.empty(),
          "accessibility text obeys the bounded UTF-16 label budget");
    std::wstring oversizedMember = L"classDiagram\nclass A {\n+";
    oversizedMember.append(NativeFlowchartParser::MaximumLabelUnits, L'x');
    oversizedMember.append(L"\n}\n");
    parser.Begin(oversizedMember, 22u);
    const auto memberCapacity = Complete(parser);
    check(memberCapacity->source == oversizedMember && memberCapacity->outcome == FlowchartOutcome::Capacity &&
              memberCapacity->diagnostic.issue == FlowchartIssue::LabelCapacity && memberCapacity->nodes.empty() && memberCapacity->edges.empty(),
          "oversized class-block members preserve exact source and discard the partial graph");
    std::wstring boundedDescription = L"stateDiagram-v2\nS : ";
    boundedDescription.append(NativeFlowchartParser::MaximumLabelUnits, L'x');
    parser.Begin(boundedDescription, 3u);
    const auto atLimit = Complete(parser);
    check(atLimit->outcome == FlowchartOutcome::Prepared && atLimit->nodes.size() == 1u &&
              atLimit->nodes.front().label == std::wstring(NativeFlowchartParser::MaximumLabelUnits, L'x'),
          "first state description uses the full label allowance without the fallback ID");
    boundedDescription.append(L"\nS : y\n");
    parser.Begin(boundedDescription, 4u);
    const auto appendCapacity = Complete(parser);
    check(appendCapacity->outcome == FlowchartOutcome::Capacity && appendCapacity->diagnostic.issue == FlowchartIssue::LabelCapacity &&
              appendCapacity->nodes.empty() && appendCapacity->edges.empty(),
          "later state descriptions check the combined label and newline before appending");
}
struct FanoutExpectation
{
    std::wstring_view source;
    std::wstring_view signature;
};
inline constexpr FanoutExpectation fanoutExpectations[] = {
    {L"flowchart LR;A[Alpha & beta] & B --> C & D --> E", L"F|L|A=Alpha & beta:R;B=B:R;C=C:R;D=D:R;E=E:R;|0>2:A:;0>3:A:;1>2:A:;1>3:A:;2>4:A:;3>4:A:;"},
    {L"graph TD;A & B-->|yes & no|C & D", L"F|T|A=A:R;B=B:R;C=C:R;D=D:R;|0>2:A:yes & no;0>3:A:yes & no;1>2:A:yes & no;1>3:A:yes & no;"},
};
template <typename Check> inline void CheckFanout(Check&& check)
{
    NativeFlowchartParser parser;
    struct OriginalFanoutModel
    {
        std::size_t number;
        std::wstring_view signature;
    };
    constexpr OriginalFanoutModel originalFanoutModels[] = {
        {80u, L"F|L|a=a:R;b=b:R;c=c:R;d=d:R;|0>1:A:;0>2:A:;1>3:A:;2>3:A:;"},
        {81u, L"F|T|A=A:R;B=B:R;C=C:R;D=D:R;|0>2:A:;0>3:A:;1>2:A:;1>3:A:;"},
    };
    for (const auto& expected : originalFanoutModels)
    {
        const auto& example = NativeFlowchartCorpus::flowcharts[expected.number - 1u];
        parser.Begin(example.source, expected.number);
        const auto model = Complete(parser);
        check(model->outcome == FlowchartOutcome::Prepared && ModelSignature(*model) == expected.signature,
              "original Mermaid fan-out examples match independent normalized endpoint order and edge-kind goldens");
    }
    for (const auto& expected : fanoutExpectations)
    {
        parser.Begin(expected.source, 1u);
        const auto model = Complete(parser);
        check(model->outcome == FlowchartOutcome::Prepared && model->diagnostic.issue == FlowchartIssue::None && ModelSignature(*model) == expected.signature,
              "grouped endpoints expand to the independent stable ID, label, direction and edge-kind golden");
        const auto firstOperator = expected.source.find(L"-->");
        const auto nextOperator  = expected.source.find(L"-->", firstOperator + 3u);
        if (nextOperator != std::wstring_view::npos)
        {
            for (std::size_t index = 0u; index < 4u; ++index)
                check(model->edges[index].source.first == firstOperator && model->edges[index].source.last == nextOperator,
                      "each expanded edge retains the exact operator-to-endpoint-group source range");
        }
        else
        {
            for (const auto& edge : model->edges)
                check(edge.source.first == firstOperator && edge.source.last == expected.source.size(),
                      "labeled fan-out edges retain the complete exact source range");
        }
    }
    for (const auto source : {L"flowchart TD;A & --> B", L"flowchart TD;A --> & B", L"flowchart TD;A & B & --> C"})
    {
        parser.Begin(source, 2u);
        const auto malformed = Complete(parser);
        check(malformed->outcome == FlowchartOutcome::Malformed && malformed->diagnostic.issue == FlowchartIssue::UnexpectedToken && malformed->nodes.empty() &&
                  malformed->edges.empty(),
              "empty endpoint groups reject atomically with a bounded syntax diagnostic");
    }
    std::wstring oversized = L"flowchart TD;";
    for (std::size_t index = 0u; index < 24u; ++index)
    {
        if (index)
            oversized += L" & ";
        oversized += L"A" + std::to_wstring(index);
    }
    oversized += L" --> ";
    for (std::size_t index = 0u; index < 23u; ++index)
    {
        if (index)
            oversized += L" & ";
        oversized += L"B" + std::to_wstring(index);
    }
    parser.Begin(oversized, 3u);
    const auto edgeCapacity = Complete(parser);
    check(edgeCapacity->outcome == FlowchartOutcome::Capacity && edgeCapacity->diagnostic.issue == FlowchartIssue::EdgeCapacity &&
              edgeCapacity->nodes.empty() && edgeCapacity->edges.empty(),
          "oversized endpoint cross-product is rejected atomically before publishing any graph");
    std::wstring maximumEdges = L"flowchart TD;";
    for (std::size_t index = 0u; index < 16u; ++index)
    {
        if (index)
            maximumEdges += L" & ";
        maximumEdges += L"L" + std::to_wstring(index);
    }
    maximumEdges += L" --> ";
    for (std::size_t index = 0u; index < 32u; ++index)
    {
        if (index)
            maximumEdges += L" & ";
        maximumEdges += L"R" + std::to_wstring(index);
    }
    parser.Begin(maximumEdges, 4u);
    const auto admittedMaximum = Complete(parser);
    check(admittedMaximum->outcome == FlowchartOutcome::Prepared && admittedMaximum->nodes.size() == 48u &&
              admittedMaximum->edges.size() == NativeFlowchartParser::MaximumEdges,
          "the exact 512-edge cross-product remains within the documented edge admission bound");
    std::wstring tooManyNodes = L"flowchart TD;";
    for (std::size_t index = 0u; index <= NativeFlowchartParser::MaximumNodes; ++index)
    {
        if (index)
            tooManyNodes += L" & ";
        tooManyNodes += L"N" + std::to_wstring(index);
    }
    parser.Begin(tooManyNodes, 5u);
    const auto nodeCapacity = Complete(parser);
    check(nodeCapacity->outcome == FlowchartOutcome::Capacity && nodeCapacity->diagnostic.issue == FlowchartIssue::NodeCapacity &&
              nodeCapacity->nodes.empty() && nodeCapacity->edges.empty(),
          "node-group admission respects the existing finite node cap atomically");
}
} // namespace NativeFlowchartTests
