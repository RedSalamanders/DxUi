#pragma once
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
