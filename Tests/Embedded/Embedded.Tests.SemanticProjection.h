#pragma once
// W7.1 gate on FX8 (plan item W7.1, block-incremental semantic projection, part 1): one-character inserts and Enter
// presses in the rich typing fixture build only the edited top-level block of the semantic document. The unchanged blocks
// are still copied, O(document) per edit; the copy counters are printed (and recorded by the receipts), not bounded here.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h; uses its Check.
#include "Embedded.Tests.PerformanceFixtureFx8.h"
#include "Embedded.Tests.PerformanceFixtures.h"

#include <iostream>

namespace SemanticProjectionFx8
{
// The edited blocks of FX8: the first prose paragraph (one Paragraph element, plus the document root every build creates),
// the numbered code fence (one Code element) and the paragraph after the table. Enter splits a paragraph into two.
inline constexpr uint64_t kInsertElements = 2u;
inline constexpr uint64_t kEnterElements  = 3u;

inline void TestFx8RebuildsOnlyTheEditedBlock()
{
    const auto& definition = PerformanceFixtures::Fx8::kDefinition;
    const auto run         = PerformanceFixtures::RunEmbedded(definition, PerformanceFixtures::Fx8::kVariants[0]);
    const auto summarize   = [&](std::string_view phase, std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase(phase), counter); };
    for (const std::string_view phase : {"prose-insert", "fence-insert", "table-insert", "enter"})
    {
        const auto builds   = summarize(phase, "rich.semanticIncrementalBuilds");
        const auto elements = summarize(phase, "rich.elementsBuilt");
        const auto written  = summarize(phase, "rich.displayUnitsWritten");
        const auto units    = summarize(phase, "rich.semanticDisplayUnitsCopied");
        const auto copied   = summarize(phase, "rich.semanticElementsCopied");
        const auto bytes    = summarize(phase, "rich.semanticBytesCopied");
        const uint64_t full = summarize(phase, "rich.semanticFullBuildNoPrevious").total + summarize(phase, "rich.semanticFullBuildPresentation").total +
                              summarize(phase, "rich.semanticFullBuildFootnotes").total + summarize(phase, "rich.semanticFullBuildReferences").total +
                              summarize(phase, "rich.semanticFullBuildFrontMatter").total + summarize(phase, "rich.semanticFullBuildAbbreviations").total +
                              summarize(phase, "rich.semanticFullBuildCapacity").total + summarize(phase, "rich.semanticFullBuildStructure").total;
        const auto steps    = run.Phase(phase).steps.size();
        std::cout << "  FX8 " << phase << ": " << steps << " steps, incremental builds " << builds.total << ", full builds " << full
                  << "; built per step: elements max " << elements.maximum << ", display units max " << written.maximum << "; copied per step: elements max "
                  << copied.maximum << ", display units max " << units.maximum << ", bytes max " << bytes.maximum << " (bytes total " << bytes.total << ")\n";
        Check(full == 0u && builds.minimum >= 1u, "every FX8 edit builds the semantic document incrementally");
        Check(elements.maximum <= (phase == "enter" ? kEnterElements : kInsertElements),
              "FX8 builder work per edit is at most the edited block (plus the document root)");
        Check(written.maximum < units.minimum, "FX8 display units built per edit are a fraction of the copied ones");
        Check(copied.minimum > 0u && bytes.minimum > 0u, "FX8 copied work is counted (it remains O(document) per edit)");
    }
    const auto arrows = summarize("arrow", "rich.elementsBuilt");
    Check(arrows.maximum == 0u, "FX8 arrow keys build no semantic element");
}
} // namespace SemanticProjectionFx8
