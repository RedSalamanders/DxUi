#pragma once
#include "../../src/Controls/DxUi.NativeFlowchart.h"
#include "Scope.Tests.NativeFlowchartCorpus.h"
#include <DxUi/MarkdownSyntax.h>
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
    const auto diagnostics = syntax.Diagnostics();
    check(diagnostics.size() == (expected && ! scenario.closed ? 1u : 0u), "only incomplete metadata envelopes publish the missing-close diagnostic");
    if (! diagnostics.empty())
    {
        const auto& issue = diagnostics[0u];
        check(issue.code == MarkdownDiagnosticCode::UnclosedFrontMatter && issue.node < syntax.Nodes().size() &&
                  issue.source == syntax.Nodes()[issue.node].block.info,
              "metadata diagnostic names the exact opener and final revision-local source owner");
    }
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
    {L"graph TD;A[a\\\"b];A-->a;", {}, {}, 2u, 1u, {}, L"a\"b"},
    {L"graph TD;A[\"\"];a[lower];A-->a", {}, {}, 2u, 1u, {}, L""},
    {L"graph TD;A-->|yes%%keep|B", {}, {}, 2u, 1u, {}, L"A"},
    {L"graph TD;A--- ops;A--- xB", {}, {}, 3u, 2u, {}, L"A"},
    {L"sequenceDiagram\nA->>B: hello\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedFamily},
    {L"flowchart TD\nsubgraph G\nA --> B\nend\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"flowchart TD\nA --> B\nstyle A fill:red\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"flowchart TD\nA --> B\nclick A \"https://example.invalid\"\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"%%{init: {}}%%\nflowchart TD\nA --> B\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedConfiguration},
    {L"flowchart TD\nA[\"<b>HTML</b>\"]\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"flowchart TD\nA@{shape: hex}\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape},
    {L"flowchart TD\nA -.-> B\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge},
    {L"flowchart TD\nA ---oB\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge},
    {L"flowchart TD\nA ---xB\n", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedEdge},
    {L"graph TD; end", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedStatement},
    {L"graph TD; A[[subroutine]]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedShape},
    {L"graph TD; A[\"&amp;\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD; A[\"#9829;\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD; A[\"fab:fa-truck\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"graph TD; A[\"$$x^2$$\"]", FlowchartOutcome::Unsupported, FlowchartIssue::UnsupportedLabelMarkup},
    {L"flowchart TD\nA[unclosed\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingLabelClose},
    {L"flowchart TD\nA -->\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingTarget},
    {L"flowchart ZZ\nA --> B\n", FlowchartOutcome::Malformed, FlowchartIssue::InvalidDirection},
    {L"graph TD;A[\"bad\\q\"]", FlowchartOutcome::Malformed, FlowchartIssue::InvalidEscape},
    {L"graph TD;1A", FlowchartOutcome::Malformed, FlowchartIssue::InvalidIdentifier},
    {L"%% comment only\n", FlowchartOutcome::Malformed, FlowchartIssue::MissingHeader},
    {L"graph TD;A((circle)", FlowchartOutcome::Malformed, FlowchartIssue::MissingLabelClose},
};
inline std::shared_ptr<const FlowchartModel> Complete(NativeFlowchartParser& parser, std::size_t budget = 1u)
{
    while (parser.HasPendingPreparation())
        parser.Advance(budget);
    return parser.Published();
}
template <typename Check> inline void CheckModel(const FlowchartModel& model, const Scenario& scenario, Check&& check)
{
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
template <typename Check> inline void CheckOriginalCorpus(Check&& check)
{
    NativeFlowchartParser parser;
    std::size_t prepared{}, unsupported{};
    for (const auto& example : NativeFlowchartCorpus::flowcharts)
    {
        parser.Begin(example.source, example.number);
        const auto result = Complete(parser, 1u);
        check(result->source == example.source && result->outcome == example.outcome && result->nodes.size() == example.nodes &&
                  result->edges.size() == example.edges,
              "all original Mermaid flowchart examples match independent subset status and graph counts");
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
    for (const auto& family : NativeFlowchartCorpus::unsupportedFamilies)
    {
        parser.Begin(family.source, 1u);
        const auto result = Complete(parser, 1u);
        check(result->outcome == FlowchartOutcome::Unsupported && result->source == family.source && result->nodes.empty() && result->edges.empty(),
              "every other declared Mermaid diagram family has explicit unsupported source fallback");
    }
    check(prepared == 13u && unsupported == 92u && std::size(NativeFlowchartCorpus::unsupportedFamilies) == 20u,
          "Mermaid corpus has no omitted examples/families and separates graph preparation from rejection");
}
} // namespace NativeFlowchartTests
