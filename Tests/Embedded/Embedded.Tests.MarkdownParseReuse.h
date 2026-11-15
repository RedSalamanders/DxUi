#pragma once
// W7.2 gate on FX8 (plan item W7.2, incremental parse reuse): a one-character insert re-lexes no more source than the
// top-level block that encloses it plus the block after it, in each of the parser's three scans (block lines, lexemes and
// inline leaves), and validates no front matter. Two measurements:
//   - the fixture itself: FX8 default and source, every measured insert step, through the editor;
//   - the parser alone on the fixture's text at both scales (default 7,936 units, full 65,280 units, the text of the
//     source-full variant), per insert kind: prose, fenced block and next to the table as in the fixture, plus a table
//     cell, a list item and the block right after a closed front-matter fence. The last two edit the fixture's text with a
//     list and a YAML front matter added, since FX8 has neither. The per-kind lines are the W7.2 receipt.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.BenchmarkInstrument.h (its allocation counters) and the
// headers Embedded.Tests.PerformanceFixtures.h needs before it; uses its Check.
#include "Embedded.Tests.MemoryMetrics.h"
#include "Embedded.Tests.PerformanceFixtures.h"
#include "Support.Tests.MarkdownParseReuse.h"

#include <DxUi/MarkdownSyntax.h>

#include <format>
#include <iostream>
#include <string>

namespace MarkdownParseReuseFx8
{
using namespace DxUi::RichText;

// The source span from the start of the top-level block that encloses `position` to the end of the top-level block after
// it: the gate's "enclosing block plus the next". A position in trivia between blocks belongs to the block after it.
inline std::size_t EnclosingAndNext(const MarkdownSyntax& syntax, std::size_t position)
{
    const auto& nodes = syntax.Nodes();
    auto root         = nodes[0].firstChild;
    auto last         = root;
    while (root != MarkdownNoNode && nodes[root].block.source.last < position)
    {
        last = root;
        root = nodes[root].nextSibling;
    }
    if (root == MarkdownNoNode)
        root = last;
    if (root == MarkdownNoNode)
        return syntax.Source().size();
    const auto first = (std::min)(nodes[root].block.source.first, position);
    const auto next  = nodes[root].nextSibling;
    return (next == MarkdownNoNode ? syntax.Source().size() : nodes[next].block.source.last) - first;
}

struct InsertKind
{
    std::string name;
    std::size_t inserts{}, boundMinimum = static_cast<std::size_t>(-1), boundMaximum{}, overBound{};
    std::uint64_t blockMaximum{}, lexicalMaximum{}, inlineMaximum{}, blockTotal{}, lexicalTotal{}, inlineTotal{};
    std::uint64_t leavesParsed{}, leavesCopied{}, nodesCopied{}, identityMapNodes{}, frontMatterValidations{}, sourceCopied{}, sourceMaterialized{};
    std::uint64_t allocations{}, allocationBytes{}, fullInlineReparses{};
};

inline std::shared_ptr<const MarkdownSyntax> Parse(MarkdownParser& parser, const Document& document)
{
    parser.Begin(document.Snapshot(), document.Revision(), MarkdownProfile::RichMarkdown);
    return MarkdownParseReuseTests::Complete(parser, Check);
}

// `inserts` one-character inserts at `position`, each followed by a complete parse, measured one by one.
inline InsertKind MeasureInserts(std::string name, const std::wstring& text, std::size_t position, std::size_t inserts)
{
    Check(position != std::wstring::npos && text.size() + inserts <= Document::MaximumUnits, "the W7.2 insert site exists within the cap");
    Document document(text);
    MarkdownParser parser;
    static_cast<void>(Parse(parser, document));
    InsertKind kind;
    kind.name    = std::move(name);
    kind.inserts = inserts;
    for (std::size_t insert = 0u; insert < inserts; ++insert)
    {
        const auto at = position + insert;
        document.Replace(at, at, std::wstring(1u, static_cast<wchar_t>(L'a' + insert % 26u)), {at + 1u, at + 1u});
        allocations      = 0u;
        allocationBytes  = 0u;
        countAllocations = true;
        DxUi::Detail::WorkCounterScope scope;
        const auto syntax = Parse(parser, document);
        const auto work   = scope.Delta().rich;
        countAllocations  = false;
        const auto bound  = EnclosingAndNext(*syntax, at);
        kind.boundMinimum = (std::min)(kind.boundMinimum, bound);
        kind.boundMaximum = (std::max)(kind.boundMaximum, bound);
        kind.overBound += work.markdownBlockUnitsScanned > bound || work.markdownLexicalUnitsScanned > bound || work.markdownInlineUnitsScanned > bound;
        kind.blockMaximum   = (std::max)(kind.blockMaximum, work.markdownBlockUnitsScanned);
        kind.lexicalMaximum = (std::max)(kind.lexicalMaximum, work.markdownLexicalUnitsScanned);
        kind.inlineMaximum  = (std::max)(kind.inlineMaximum, work.markdownInlineUnitsScanned);
        kind.blockTotal += work.markdownBlockUnitsScanned;
        kind.lexicalTotal += work.markdownLexicalUnitsScanned;
        kind.inlineTotal += work.markdownInlineUnitsScanned;
        kind.leavesParsed += work.markdownInlineLeavesParsed;
        kind.leavesCopied += work.markdownInlineLeavesCopied;
        kind.nodesCopied += work.markdownNodesCopied;
        kind.identityMapNodes += work.identityMapNodes;
        kind.frontMatterValidations += work.markdownFrontMatterValidations;
        kind.sourceCopied += work.markdownSourceUnitsCopied;
        kind.sourceMaterialized += work.markdownSourceUnitsMaterialized;
        kind.fullInlineReparses += work.markdownInlineFullReparses;
        kind.allocations += allocations;
        kind.allocationBytes += allocationBytes;
    }
    return kind;
}

inline void Print(std::string_view shape, std::size_t units, const InsertKind& kind)
{
    const auto n = static_cast<double>(kind.inserts);
    std::cout << std::format("  W7.2 parse {} ({} units) {}: {} inserts; bound (enclosing + next block) {}..{} units; scanned per insert max/avg: "
                             "block {}/{:.0f}, lexical {}/{:.0f}, inline {}/{:.0f}; over bound {}; per insert avg: leaves parsed {:.1f}, copied {:.1f}; "
                             "nodes copied {:.0f}; identity map nodes {:.0f}; front-matter validations {:.2f}; source units copied {:.0f}, from the "
                             "piece tree {:.0f}; full inline reparses {}; allocations {:.0f} ({:.0f} bytes)\n",
                             shape,
                             units,
                             kind.name,
                             kind.inserts,
                             kind.boundMinimum,
                             kind.boundMaximum,
                             kind.blockMaximum,
                             static_cast<double>(kind.blockTotal) / n,
                             kind.lexicalMaximum,
                             static_cast<double>(kind.lexicalTotal) / n,
                             kind.inlineMaximum,
                             static_cast<double>(kind.inlineTotal) / n,
                             kind.overBound,
                             static_cast<double>(kind.leavesParsed) / n,
                             static_cast<double>(kind.leavesCopied) / n,
                             static_cast<double>(kind.nodesCopied) / n,
                             static_cast<double>(kind.identityMapNodes) / n,
                             static_cast<double>(kind.frontMatterValidations) / n,
                             static_cast<double>(kind.sourceCopied) / n,
                             static_cast<double>(kind.sourceMaterialized) / n,
                             kind.fullInlineReparses,
                             static_cast<double>(kind.allocations) / n,
                             static_cast<double>(kind.allocationBytes) / n);
}

// The fixture's text with a YAML front matter in front and a list (with a lazy continuation line) before the first math
// block: FX8 has neither construct.
inline std::wstring WithFrontMatterAndList(const std::wstring& text)
{
    std::wstring derived = L"---\ntitle: Rich typing fixture\ntags: [fx8, w7]\n---\n" + text;
    derived.insert(derived.find(L"$$\n"), L"- first item of the list\n- second item\nlazy continuation\n\n");
    return derived;
}

inline void TestFx8ParserInsertKinds()
{
    using PerformanceFixtures::Fx8::DocumentShape;
    std::vector<InsertKind> kinds;
    for (const auto* shape : {&PerformanceFixtures::Fx8::kSmallShape, &PerformanceFixtures::Fx8::kFullShape})
    {
        const auto text               = PerformanceFixtures::Fx8::BuildSource(*shape);
        const auto derived            = WithFrontMatterAndList(text);
        const auto fenceLine          = L"\nv" + std::to_wstring(shape->fenceLines / 2u) + L";";
        const auto row                = std::to_wstring(shape->tableRows / 2u);
        const auto shapeName          = shape == &PerformanceFixtures::Fx8::kFullShape ? "full" : "default";
        constexpr std::size_t inserts = 40u;
        const InsertKind measured[]   = {
            MeasureInserts("prose", text, text.find(L" extend.") + 7u, inserts),
            MeasureInserts("fenced block", text, text.find(fenceLine) + fenceLine.size() - 1u, inserts),
            MeasureInserts("next to the table", text, text.find(PerformanceFixtures::Fx8::kAfterTableMarker), inserts),
            MeasureInserts("table cell", text, text.find(L"| item" + row + L" |") + 6u + row.size(), inserts),
            MeasureInserts("list item", derived, derived.find(L"second item") + 11u, inserts),
            MeasureInserts("next to front matter", derived, derived.find(L"# Rich typing fixture") + 21u, inserts),
        };
        for (const auto& kind : measured)
        {
            Print(shapeName, text.size(), kind);
            kinds.push_back(kind);
        }
    }
    for (const auto& kind : kinds)
    {
        Check(kind.overBound == 0u, "FX8 one-character inserts re-lex no more than the enclosing block plus the next, in every scan");
        Check(kind.frontMatterValidations == 0u, "FX8 inserts past a closed front-matter fence validate no YAML or TOML");
    }
}

// The same gate through the editor on the fixture: every measured insert step of FX8 default and source.
inline void TestFx8ParseWorkPerInsert()
{
    const auto& definition = PerformanceFixtures::Fx8::kDefinition;
    for (const std::size_t variantIndex : {std::size_t{0u}, std::size_t{2u}})
    {
        const auto& variant = PerformanceFixtures::Fx8::kVariants[variantIndex];
        const auto& shape   = PerformanceFixtures::Fx8::ShapeOf(variant);
        const auto text     = PerformanceFixtures::Fx8::BuildSource(shape);
        Document document(text);
        MarkdownParser parser;
        const auto initial   = Parse(parser, document);
        const auto fenceLine = L"\nv" + std::to_wstring(shape.fenceLines / 2u) + L";";
        struct Site
        {
            std::string_view phase;
            std::size_t position, inserts;
        };
        const Site sites[] = {
            {"prose-insert", text.find(L" extend.") + 7u, PerformanceFixtures::Fx8::kProseInserts},
            {"fence-insert", text.find(fenceLine) + fenceLine.size() - 1u, PerformanceFixtures::Fx8::kFenceInserts},
            {"table-insert", text.find(PerformanceFixtures::Fx8::kAfterTableMarker), PerformanceFixtures::Fx8::kTableInserts},
        };
        const auto run        = PerformanceFixtures::RunEmbedded(definition, variant);
        std::size_t overBound = 0u, unparsed = 0u;
        std::uint64_t frontMatter = 0u;
        for (const auto& site : sites)
        {
            // The block grows by one unit per insert; the bound grants the phase's every insert from the start.
            const auto bound    = EnclosingAndNext(*initial, site.position) + site.inserts;
            const auto& steps   = run.Phase(site.phase).steps;
            std::uint64_t block = 0u, lexical = 0u, inlineUnits = 0u;
            for (const auto& step : steps)
            {
                const auto& work = step.work.rich;
                block            = (std::max)(block, work.markdownBlockUnitsScanned);
                lexical          = (std::max)(lexical, work.markdownLexicalUnitsScanned);
                inlineUnits      = (std::max)(inlineUnits, work.markdownInlineUnitsScanned);
                overBound += work.markdownBlockUnitsScanned > bound || work.markdownLexicalUnitsScanned > bound || work.markdownInlineUnitsScanned > bound;
                unparsed += work.markdownParses == 0u;
                frontMatter += work.markdownFrontMatterValidations;
            }
            std::cout << std::format("  W7.2 FX8 {} {}: {} steps; scanned per insert max: block {}, lexical {}, inline {}; bound {} units\n",
                                     variant.name,
                                     site.phase,
                                     steps.size(),
                                     block,
                                     lexical,
                                     inlineUnits,
                                     bound);
        }
        Check(unparsed == 0u, "every FX8 insert step parses the edited document");
        Check(overBound == 0u, "FX8 inserts re-lex no more than the enclosing block plus the next, in every scan");
        Check(frontMatter == 0u, "FX8 inserts validate no front matter");
    }
}
} // namespace MarkdownParseReuseFx8
