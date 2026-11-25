#pragma once
// W7.6 gate on FX8 (plan item W7.6, code blocks), through the editor on the embedded host, on the fixture's rendered
// variants (Markdown rendering, Subtle presentation, a numbered code fence):
//   - line number and language badge layouts built per one-character prose insert: 0;
//   - per insert inside the fence: at most the new distinct numbers (0 for a typed character, which adds no line);
//   - Enter presses, arrow keys, inserts next to the table: 0 as well, and each width step builds no number layout and at
//     most one badge per language (the badge's layout width follows the document's);
//   - the numbers of the first preparation are built once each (distinct numbers, not one per numbered line);
//   - the document-level visible-range query on FX8's text, Source and multiplier, reads fewer caret boxes than stepping.
// The 2,000-line full variant (receipt only, too heavy for the ordinary suites) runs through the same function when a
// run asks for it: set DXUI_FX8_CODE_BLOCKS_FULL=1.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h; uses its Check.
#include "Controls/DxUi.RichLayoutOptions.h"
#include "Embedded.Tests.PerformanceFixtures.h"
#include "Support/WorkCounters.h"
#include <DxUi/RichTextLayout.h>

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace CodeBlocksFx8
{
inline void RunVariant(const PerformanceFixtures::Variant& variant, size_t fenceLines)
{
    const auto& definition     = PerformanceFixtures::Fx8::kDefinition;
    const auto run             = PerformanceFixtures::RunEmbedded(definition, variant);
    const auto warm            = [&](std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase("warm"), counter).total; };
    const uint64_t warmNumbers = warm("rich.codeNumberLayouts"), warmBadges = warm("rich.codeBadgeLayouts");
    std::cout << "  FX8 " << variant.name << " warm: number layouts " << warmNumbers << ", badge layouts " << warmBadges << '\n';
    // The fence has `fenceLines` numbered lines, plus the fixture's own; its distinct numbers are the layouts built.
    Check(warmNumbers >= fenceLines && warmNumbers <= fenceLines + 2u, "the first FX8 preparation builds one layout per distinct line number of its fence");
    Check(warmBadges == 1u, "the first FX8 preparation builds one badge layout, for the fence's language");
    for (const std::string_view phase : {"prose-insert", "enter", "arrow", "fence-insert", "table-insert", "formula-insert", "resize"})
    {
        const auto summarize = [&](std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase(phase), counter); };
        const auto numbers = summarize("rich.codeNumberLayouts"), badges = summarize("rich.codeBadgeLayouts");
        std::cout << "  FX8 " << variant.name << ' ' << phase << " per step (max): number layouts " << numbers.maximum << ", badge layouts " << badges.maximum
                  << " (total " << numbers.total << " and " << badges.total << ")\n";
        Check(numbers.maximum == 0u, "an FX8 step builds no line number layout (a typed character adds no new distinct number)");
        if (phase == "resize")
            Check(badges.maximum <= 1u, "an FX8 width step builds at most one badge layout per language");
        else
            Check(badges.maximum == 0u, "an FX8 edit builds no badge layout");
    }
}
// The document-level visible-range query on the text of FX8's full document (65,280 units), laid out in v0 chunks, in
// Source mode and in Rich mode with a line-height multiplier of 1.5: caret boxes read per query by the search and by
// stepping through every chunk's lines from its first, over 24 scroll positions of a 720 DIP window, with equal answers.
inline void TestFx8DocumentVisibleRangeCaretReads()
{
    using namespace DxUi::RichText;
    wil::com_ptr<IDWriteFactory> factory;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())), "a DirectWrite factory");
    const std::wstring source = PerformanceFixtures::Fx8::BuildSource(PerformanceFixtures::Fx8::kFullShape);
    struct Case
    {
        const char* name;
        ViewMode mode;
        float multiplier;
    };
    for (const Case item : {Case{"source", ViewMode::Source, 0.0f}, Case{"multiplier 1.5", ViewMode::Rich, 1.5f}})
    {
        Document document(source);
        PreparedDocumentLayout prepared;
        LayoutOptions options;
        options.widthDip             = 1240.0f;
        options.fontSizeDip          = 15.0f;
        options.lineHeightMultiplier = item.multiplier;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), item.mode, options);
        Check(prepared.Ready() && prepared.ChunkCount() > 30u, "FX8's text lays out in many chunks");
        uint64_t searchMax = 0u, searchTotal = 0u, stepMax = 0u, stepTotal = 0u;
        size_t queries = 0u;
        for (size_t position = 0u; position < 24u; ++position)
        {
            const float top = prepared.Height() * float(position) / 24.0f;
            std::pair<size_t, size_t> found{}, expected{};
            const DxUi::Detail::WorkCounterScope searchScope;
            Hr(prepared.TryVisibleRange(top, top + 720.0f, found), "the search query");
            const auto reads = searchScope.Delta().rich.caretHitTests;
            const DxUi::Detail::WorkCounterScope stepScope;
            Hr(Detail::PreparedDocumentLayoutAccess::VisibleRange(prepared, top, top + 720.0f, expected, true), "the stepping query");
            const auto steps = stepScope.Delta().rich.caretHitTests;
            Check(found == expected, "the searched visible range equals the stepped one");
            searchMax = std::max(searchMax, reads);
            searchTotal += reads;
            stepMax = std::max(stepMax, steps);
            stepTotal += steps;
            ++queries;
        }
        std::cout << "  FX8 full text, " << item.name << ": " << prepared.ChunkCount() << " chunks, caret reads per visible-range query: search max "
                  << searchMax << " mean " << searchTotal / queries << ", stepping max " << stepMax << " mean " << stepTotal / queries << '\n';
        Check(searchMax < stepMax, "the search reads fewer caret boxes than stepping");
    }
}
inline void TestFx8CodeBlockLayoutsPerEdit()
{
    for (const auto& variant : PerformanceFixtures::Fx8::kVariants)
    {
        if (PerformanceFixtures::Fx8::PresentationOf(variant) != PerformanceFixtures::Fx8::Presentation::Semantic)
            continue;
        if (variant.receiptOnly)
        {
            char* value      = nullptr;
            size_t size      = 0u;
            const bool asked = _dupenv_s(&value, &size, "DXUI_FX8_CODE_BLOCKS_FULL") == 0 && value && *value == '1';
            std::free(value);
            if (! asked)
                continue;
        }
        RunVariant(variant, PerformanceFixtures::Fx8::ShapeOf(variant).fenceLines);
    }
    TestFx8DocumentVisibleRangeCaretReads();
}
} // namespace CodeBlocksFx8
