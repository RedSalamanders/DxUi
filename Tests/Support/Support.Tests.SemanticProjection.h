#pragma once
// Block-incremental semantic projection (plan item W7.1): an incremental SemanticBuilder publication must equal a fresh
// build of the same syntax after every edit. The oracle compares every published value, the map in both directions at
// every source and display offset, and the source formats. A separate reference reproduces the former dense
// SourceDisplayMap anchors to prove the sparse map answers the same.
#include "Controls/DxUi.RichSemanticValues.h"
#include "Controls/DxUi.SemanticExtensions.h"
#include "Support.Tests.RichSemanticDocument.h"
#include "Support/WorkCounters.h"
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace SemanticProjectionTests
{
using namespace DxUi::RichText;
using RichSemanticTests::Check;

[[noreturn]] inline void Fail(const std::string& message)
{
    std::cerr << "FAIL semantic projection: " << message << '\n' << std::flush;
    std::exit(1);
}
inline void Expect(bool condition, const std::string& context, std::string_view what)
{
    if (! condition)
        Fail(std::format("{}: {}", context, what));
}

// The former dense anchors (one leading and one trailing display offset per source offset), as the map computed them
// before W7.1. Independent of the sparse lookup it checks.
inline void DenseAnchors(
    std::size_t sourceSize, std::size_t displaySize, std::span<const DisplaySpan> spans, std::vector<std::size_t>& leading, std::vector<std::size_t>& trailing)
{
    constexpr auto missing = static_cast<std::size_t>(-1);
    leading.assign(sourceSize + 1u, missing);
    trailing.assign(sourceSize + 1u, missing);
    std::vector<std::size_t> primary;
    for (std::size_t i = 0u; i < spans.size(); ++i)
        if (spans[i].primary)
            primary.push_back(i);
    std::sort(primary.begin(), primary.end(), [&](auto a, auto b) { return spans[a].source.first < spans[b].source.first; });
    const auto atomic = [](DisplaySpanKind kind) { return kind == DisplaySpanKind::Transformed || kind == DisplaySpanKind::Object; };
    for (const auto index : primary)
    {
        const auto& span = spans[index];
        for (auto position = span.source.first; position <= span.source.last; ++position)
        {
            const auto lead   = atomic(span.kind) && position < span.source.last ? span.display.first : span.display.first + position - span.source.first;
            const auto trail  = atomic(span.kind) && position > span.source.first ? span.display.last : span.display.first;
            leading[position] = atomic(span.kind) && position == span.source.last ? span.display.last : lead;
            if (position != span.source.first || trailing[position] == missing)
                trailing[position] = atomic(span.kind) ? trail : span.display.first + position - span.source.first;
        }
    }
    auto next = displaySize;
    for (auto position = sourceSize + 1u; position > 0u;)
    {
        --position;
        if (leading[position] == missing)
            leading[position] = next;
        else
            next = leading[position];
    }
    std::size_t previous = 0u;
    for (auto& anchor : trailing)
    {
        if (anchor == missing)
            anchor = previous;
        else
            previous = anchor;
    }
}
inline void CheckSparseMapAgainstDense(const SourceDisplayMap& map, const std::string& context)
{
    std::vector<std::size_t> leading, trailing;
    DenseAnchors(map.Source().size(), map.Display().size(), map.Spans(), leading, trailing);
    for (std::size_t offset = 0u; offset <= map.Source().size(); ++offset)
    {
        if (! IsEditBoundary(map.Source(), offset))
            continue;
        if (map.ToDisplay(offset, MappingAffinity::Leading) != leading[offset] || map.ToDisplay(offset, MappingAffinity::Trailing) != trailing[offset])
            Fail(std::format("{}: sparse map differs from the dense anchors at source {} (leading {} vs {}, trailing {} vs {})",
                             context,
                             offset,
                             map.ToDisplay(offset, MappingAffinity::Leading),
                             leading[offset],
                             map.ToDisplay(offset, MappingAffinity::Trailing),
                             trailing[offset]));
    }
}

inline bool SameElement(const SemanticElement& a, const SemanticElement& b) noexcept
{
    return a.role == b.role && a.source == b.source && a.display == b.display && a.taskMarker == b.taskMarker && a.parent == b.parent &&
           a.firstChild == b.firstChild && a.nextSibling == b.nextSibling && a.blockIdentity == b.blockIdentity && a.syntaxNode == b.syntaxNode &&
           a.heading == b.heading && a.task == b.task && a.alignment == b.alignment && a.tableHeader == b.tableHeader && a.nativeHtml == b.nativeHtml &&
           a.wiki == b.wiki && a.disclosureExpanded == b.disclosureExpanded && a.rows == b.rows && a.columns == b.columns && a.imageWidth == b.imageWidth &&
           a.imageHeight == b.imageHeight && a.name == b.name && a.destination == b.destination && a.title == b.title;
}
inline bool SameParagraph(const SemanticParagraph& a, const SemanticParagraph& b) noexcept
{
    return a.display == b.display && a.element == b.element && a.syntaxNode == b.syntaxNode && a.table == b.table && a.row == b.row && a.column == b.column &&
           a.columns == b.columns && a.indent == b.indent && a.flags == b.flags && a.heading == b.heading && a.alignment == b.alignment &&
           a.markerEnd == b.markerEnd && a.markerOwner == b.markerOwner && a.emptyInsertion == b.emptyInsertion;
}
inline bool SameRun(const StyleRun& a, const StyleRun& b) noexcept
{
    return a.start == b.start && a.length == b.length && a.flags == b.flags && a.heading == b.heading;
}
inline bool SameSpan(const DisplaySpan& a, const DisplaySpan& b) noexcept
{
    return a.source == b.source && a.display == b.display && a.kind == b.kind && a.primary == b.primary;
}
template <typename T, typename Same> bool SameRange(std::span<const T> a, std::span<const T> b, Same same)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), same);
}

// Native object content, not just presence: a parsed formula tree and a diagram model with its syntax revision.
inline bool SameMathNode(const MathNode& a, const MathNode& b)
{
    return a.kind == b.kind && a.text == b.text && a.columns == b.columns && a.children.size() == b.children.size() &&
           std::equal(a.children.begin(), a.children.end(), b.children.begin(), [](const auto& x, const auto& y) { return SameMathNode(x, y); });
}
inline bool SameMath(const MathExpression* a, const MathExpression* b)
{
    return a == b || (a && b && a->issue == b->issue && a->diagnostic == b->diagnostic && SameMathNode(a->root, b->root));
}
inline bool SameGraph(const DxUi::RichText::Detail::FlowchartModel* a, const DxUi::RichText::Detail::FlowchartModel* b)
{
    return a == b || (a && b && a->source == b->source && a->revision == b->revision && a->family == b->family && a->direction == b->direction &&
                      a->outcome == b->outcome && a->accessibleTitle == b->accessibleTitle && a->accessibleDescription == b->accessibleDescription &&
                      a->nodes.size() == b->nodes.size() && a->edges.size() == b->edges.size() && a->groups.size() == b->groups.size());
}

// Everything a SemanticDocument publishes, and every map query at every offset.
inline void CompareDocuments(const SemanticDocument& incremental, const SemanticDocument& fresh, const std::string& context)
{
    using Access = Detail::SemanticExtensionAccess;
    Expect(incremental.Syntax() == fresh.Syntax(), context, "both publications hold the same syntax");
    const auto& a = incremental.Map();
    const auto& b = fresh.Map();
    Expect(a.Source() == b.Source() && a.Source().data() == incremental.Syntax()->Source().data(), context, "the map borrows the exact syntax source");
    if (a.Display() != b.Display())
    {
        std::size_t at = 0u;
        while (at < a.Display().size() && at < b.Display().size() && a.Display()[at] == b.Display()[at])
            ++at;
        Fail(std::format("{}: display text differs at unit {} (incremental {} units, fresh {} units)", context, at, a.Display().size(), b.Display().size()));
    }
    Expect(SameRange(a.Spans(), b.Spans(), SameSpan), context, "display spans");
    Expect(SameRange(incremental.Runs(), fresh.Runs(), SameRun), context, "display style runs");
    Expect(SameRange(incremental.SourceRuns(), fresh.SourceRuns(), SameRun), context, "source format runs");
    if (! SameRange(incremental.Elements(), fresh.Elements(), SameElement))
    {
        std::size_t at = 0u;
        while (at < incremental.Elements().size() && at < fresh.Elements().size() && SameElement(incremental.Elements()[at], fresh.Elements()[at]))
            ++at;
        Fail(std::format("{}: elements differ at {} ({} vs {})", context, at, incremental.Elements().size(), fresh.Elements().size()));
    }
    Expect(SameRange(incremental.Paragraphs(), fresh.Paragraphs(), SameParagraph), context, "geometry paragraphs");
    Expect(SameRange(incremental.Headings(),
                     fresh.Headings(),
                     [](const auto& x, const auto& y)
    { return x.element == y.element && x.level == y.level && x.source == y.source && x.target == y.target && x.name == y.name && x.slug == y.slug; }),
           context,
           "heading index and slugs");
    Expect(SameRange(incremental.Navigation(), fresh.Navigation(), [](const auto& x, const auto& y) { return x.element == y.element && x.target == y.target; }),
           context,
           "navigation targets");
    Expect(SameRange(incremental.Diagnostics(),
                     fresh.Diagnostics(),
                     [](const auto& x, const auto& y) { return x.kind == y.kind && x.source == y.source && x.element == y.element; }),
           context,
           "diagnostics");
    Expect(incremental.HasCallouts() == fresh.HasCallouts() && incremental.HasPropertiesBody() == fresh.HasPropertiesBody() &&
               incremental.SourceBlock() == fresh.SourceBlock(),
           context,
           "document flags");
    Expect(SameRange(Access::Maths(incremental),
                     Access::Maths(fresh),
                     [](const auto& x, const auto& y)
    { return x.element == y.element && x.display == y.display && SameMath(x.expression.get(), y.expression.get()); }),
           context,
           "math objects");
    Expect(SameRange(Access::Images(incremental), Access::Images(fresh), [](auto x, auto y) { return x == y; }), context, "image objects");
    Expect(SameRange(Access::AbbreviationNodes(incremental), Access::AbbreviationNodes(fresh), [](auto x, auto y) { return x == y; }),
           context,
           "abbreviation definitions");
    Expect(SameRange(Access::Diagrams(incremental),
                     Access::Diagrams(fresh),
                     [](const auto& x, const auto& y)
    {
        return x.element == y.element && x.display == y.display && x.sourceEnd == y.sourceEnd && x.bodyUnits.size() == y.bodyUnits.size() &&
               std::equal(x.bodyUnits.begin(), x.bodyUnits.end(), y.bodyUnits.begin(), [](const auto& p, const auto& q) {
            return p.source == q.source && p.generated == q.generated && p.transformed == q.transformed;
        }) && SameGraph(x.graph.get(), y.graph.get());
    }),
           context,
           "diagram objects");
    // Both directions at every offset, including hidden markup, surrogate pairs and block edges.
    for (std::size_t offset = 0u; offset <= a.Source().size(); ++offset)
    {
        std::size_t left{}, right{};
        for (const auto affinity : {MappingAffinity::Leading, MappingAffinity::Trailing})
        {
            const auto x = a.TryToDisplay(offset, affinity, left), y = b.TryToDisplay(offset, affinity, right);
            if (x != y || (x == SourceResult::Success && left != right))
                Fail(std::format("{}: ToDisplay({}) differs: {} vs {}", context, offset, left, right));
        }
    }
    for (std::size_t offset = 0u; offset <= a.Display().size(); ++offset)
    {
        std::size_t left{}, right{};
        for (const auto affinity : {MappingAffinity::Leading, MappingAffinity::Trailing})
        {
            auto x = a.TryToSource(offset, affinity, left), y = b.TryToSource(offset, affinity, right);
            if (x != y || (x == SourceResult::Success && left != right))
                Fail(std::format("{}: ToSource({}) differs: {} vs {}", context, offset, left, right));
            x = a.TrySnapDisplayBoundary(offset, affinity, left);
            y = b.TrySnapDisplayBoundary(offset, affinity, right);
            if (x != y || (x == SourceResult::Success && left != right))
                Fail(std::format("{}: SnapDisplayBoundary({}) differs", context, offset));
        }
    }
}

// splitmix64, the same generator as the pixel oracle.
struct Random
{
    std::uint64_t state{};
    std::uint64_t Next() noexcept
    {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z               = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
        z               = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31u);
    }
    std::size_t Below(std::size_t bound) noexcept
    {
        return bound ? static_cast<std::size_t>(Next() % bound) : 0u;
    }
};

// A document with every block kind the builder knows: headings, paragraphs with inline styles, links and images, lists
// (ordered, bullet, task, nested), quotes and callouts, tables, fenced and indented code, display and inline math, a
// diagram, native HTML, a thematic break, definition lists and surrogate pairs.
inline std::wstring RichBase()
{
    return L"# Title \U0001F600\n\nIntro *em* **strong** `code` [link](https://example.test) and $x^2$ text.\n\n"
           L"## Section\n\n- one\n- [x] two\n  - nested\n\n1. first\n2. second\n\n> quote line\n> more\n\n"
           L"> [!note] Callout\n> body\n\n| A | B |\n| --- | :-: |\n| 1 | \U0001F600 |\n| x | y |\n\n"
           L"```cpp\nint a = 1;\n```\n\n    indented code\n\n$$\na + b\n$$\n\n```mermaid\nflowchart TD\nA-->B\n```\n\n"
           L"<p align=\"center\">html <b>bold</b></p>\n\n---\n\nTerm\n: Definition\n\n![alt](image.png)\n\nLast \U0001F600 line\r\nwith CRLF\r\n";
}

// Edits mixed for the fuzz: inserts and deletes of every construct, joins and splits of blocks, edits at offset 0 and at
// the end, and constructs with document-wide effect (reference definitions, footnotes, front matter, abbreviations).
inline constexpr std::wstring_view kSnippets[] = {L"a",
                                                  L"Z",
                                                  L" ",
                                                  L"\n",
                                                  L"\n\n",
                                                  L"\r\n",
                                                  L"# ",
                                                  L"## H\n",
                                                  L"- ",
                                                  L"1. ",
                                                  L"> ",
                                                  L"> [!tip] T\n",
                                                  L"| c |",
                                                  L"| --- |\n",
                                                  L"```",
                                                  L"```\n",
                                                  L"$$",
                                                  L"$y$",
                                                  L"*",
                                                  L"**",
                                                  L"`",
                                                  L"[t](u)",
                                                  L"![i](p.png)",
                                                  L"<b>",
                                                  L"</b>",
                                                  L"<details><summary>S</summary>x</details>\n",
                                                  L"---\n",
                                                  L"    ",
                                                  L": def\n",
                                                  L"\U0001F600",
                                                  L"[r]: https://r.test\n",
                                                  L"[r]",
                                                  L"[^n]",
                                                  L"[^n]: note\n",
                                                  L"---\ntitle: x\n---\n",
                                                  L"*[HTML]: Hyper Text\n",
                                                  L"HTML ",
                                                  L"```mermaid\nflowchart LR\nC-->D\n```\n"};

// Without documentWide, snippets that create constructs with document-wide effect (footnotes, reference definitions,
// front matter, abbreviation definitions) are never inserted, so almost every edit can reuse blocks.
inline std::wstring_view Snippet(Random& random, bool documentWide)
{
    for (;;)
    {
        const auto snippet = kSnippets[random.Below(std::size(kSnippets))];
        if (documentWide || (snippet.find(L"[^") == snippet.npos && snippet.find(L"]: ") == snippet.npos && snippet.find(L"---\ntitle") == snippet.npos &&
                             snippet.find(L"*[") == snippet.npos))
            return snippet;
    }
}

inline std::size_t Boundary(std::wstring_view text, std::size_t offset) noexcept
{
    offset = std::min(offset, text.size());
    while (offset > 0u && ! IsEditBoundary(text, offset))
        --offset;
    return offset;
}

struct FuzzResult
{
    std::size_t edits{}, incremental{}, full{};
};

inline std::shared_ptr<const SemanticDocument> CompleteBuild(SemanticBuilder& builder, const std::shared_ptr<const MarkdownSyntax>& syntax, std::size_t budget)
{
    builder.Begin(syntax);
    while (builder.HasPendingPreparation())
    {
        const auto step = builder.Advance(budget);
        if (step.status == MarkdownPreparationStatus::Failed)
            return nullptr;
    }
    return builder.Published();
}

// One seeded run: every edit is parsed by one persistent parser (as an editor does), built incrementally by one
// persistent builder and compared with a fresh builder's publication of the same syntax.
inline FuzzResult RunEditFuzz(
    std::uint64_t seed, std::size_t edits, MarkdownProfile profile, std::optional<MarkdownExtensions> extensions, bool documentWide = true)
{
    Random random{seed};
    std::wstring text = RichBase();
    MarkdownParser parser;
    SemanticBuilder incremental;
    FuzzResult result;
    std::uint64_t revision = 1u;
    const auto counters    = [] { return ::DxUi::Detail::Work().rich; };
    // DXUI_SEMANTIC_FUZZ_TRACE prints every edit before it is parsed and built, to locate a fatal failure.
    char* traceValue = nullptr;
    size_t traceSize = 0u;
    const bool trace = _dupenv_s(&traceValue, &traceSize, "DXUI_SEMANTIC_FUZZ_TRACE") == 0 && traceValue;
    std::free(traceValue);
    for (std::size_t edit = 0u; edit <= edits; ++edit)
    {
        std::string operation = "initial";
        if (edit)
        {
            // Bias half of the edits into an existing block so every block kind is edited.
            std::size_t at = random.Below(text.size() + 1u);
            if (const auto syntax = parser.Published(); syntax && random.Below(2u))
            {
                const auto& node = syntax->Nodes()[random.Below(syntax->Nodes().size())];
                at               = node.block.source.first + random.Below(node.block.source.last - node.block.source.first + 1u);
            }
            switch (random.Below(8u))
            {
                case 0u: at = 0u; break;
                case 1u: at = text.size(); break;
                default: break;
            }
            at              = Boundary(text, at);
            const auto kind = random.Below(5u);
            if (kind <= 1u || text.size() < 8u)
            {
                const auto snippet = Snippet(random, documentWide);
                text.insert(at, snippet);
                operation = std::format("insert {} units at {}", snippet.size(), at);
            }
            else if (kind <= 3u)
            {
                auto last = Boundary(text, at + 1u + random.Below(kind == 2u ? 3u : 24u));
                if (last <= at)
                    last = std::min(text.size(), at + 1u);
                while (last < text.size() && ! IsEditBoundary(text, last))
                    ++last;
                text.erase(at, last - at);
                operation = std::format("delete [{}, {})", at, last);
            }
            else
            {
                auto last = Boundary(text, at + random.Below(12u));
                while (last < text.size() && ! IsEditBoundary(text, last))
                    ++last;
                const auto snippet = Snippet(random, documentWide);
                text.replace(at, last - at, snippet);
                operation = std::format("replace [{}, {}) with {} units", at, last, snippet.size());
            }
            if (text.size() > 12000u)
            {
                const auto first = Boundary(text, text.size() / 2u);
                text.erase(first, Boundary(text, first + text.size() / 4u) - first);
                text.insert(first, L"\n\n");
            }
        }
        if (trace)
            std::cout << std::format("    seed 0x{:X} edit {} ({}), {} units\n", seed, edit, operation, text.size()) << std::flush;
        parser.Begin(text, revision++, profile, extensions);
        while (parser.HasPendingPreparation())
            parser.Advance(4096u);
        const auto syntax = parser.Published();
        Check(syntax && syntax->Source() == text, "the fuzz parser publishes the edited source");
        if (trace)
        {
            {
                std::ofstream dump("semantic-fuzz-current.utf16", std::ios::binary);
                dump.write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.size() * sizeof(wchar_t)));
            }
            SemanticBuilder probe;
            static_cast<void>(CompleteBuild(probe, syntax, 4096u));
            std::cout << "    fresh build ok\n" << std::flush;
        }
        const auto before   = counters();
        const auto document = CompleteBuild(incremental, syntax, 1u + random.Below(64u));
        const auto after    = counters();
        SemanticBuilder fresh;
        const auto reference = CompleteBuild(fresh, syntax, 4096u);
        const auto context   = std::format("seed 0x{:016X} edit {} ({})", seed, edit, operation);
        Expect(static_cast<bool>(document) == static_cast<bool>(reference), context, "incremental and fresh builds both publish or both fail");
        if (! document)
            continue;
        CompareDocuments(*document, *reference, context);
        if (edit % 16u == 0u)
            CheckSparseMapAgainstDense(document->Map(), context);
        ++result.edits;
        if (after.semanticIncrementalBuilds != before.semanticIncrementalBuilds)
            ++result.incremental;
        else
            ++result.full;
    }
    return result;
}
} // namespace SemanticProjectionTests
