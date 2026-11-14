#pragma once
// W7.1 gate on FX8 (plan item W7.1, block-incremental semantic projection): one-character inserts and Enter presses in the
// rich typing fixture rebuild only the edited top-level block of the semantic document. Included by
// Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h; uses its Check.
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
        const auto reused   = summarize(phase, "rich.semanticDisplayUnitsReused");
        const uint64_t full = summarize(phase, "rich.semanticFullBuildNoPrevious").total + summarize(phase, "rich.semanticFullBuildPresentation").total +
                              summarize(phase, "rich.semanticFullBuildFootnotes").total + summarize(phase, "rich.semanticFullBuildReferences").total +
                              summarize(phase, "rich.semanticFullBuildFrontMatter").total + summarize(phase, "rich.semanticFullBuildAbbreviations").total +
                              summarize(phase, "rich.semanticFullBuildCapacity").total + summarize(phase, "rich.semanticFullBuildStructure").total;
        const auto steps    = run.Phase(phase).steps.size();
        std::cout << "  FX8 " << phase << ": " << steps << " steps, incremental builds " << builds.total << ", full builds " << full
                  << ", elements built per step max " << elements.maximum << " (total " << elements.total << "), display units written max " << written.maximum
                  << " (total " << written.total << "), reused units total " << reused.total << '\n';
        Check(full == 0u && builds.minimum >= 1u, "every FX8 edit builds the semantic document incrementally");
        Check(elements.maximum <= (phase == "enter" ? kEnterElements : kInsertElements),
              "FX8 elements built per edit are at most those of the edited block (plus the document root)");
        Check(written.maximum < reused.minimum, "FX8 display units written per edit are a fraction of the reused ones");
    }
    const auto arrows = summarize("arrow", "rich.elementsBuilt");
    Check(arrows.maximum == 0u, "FX8 arrow keys build no semantic element");
}
} // namespace SemanticProjectionFx8
