#pragma once
// W7.3 gate on FX8 (plan item W7.3, position-independent chunks and viewport-first preparation), through the editor on
// the embedded host, on the fixture's v0 variants: "source" (Source mode, chunks) and "multiplier" (Rich mode without
// Markdown rendering at line height 1.5, which laid one whole layout per keystroke before this item).
//   - chunks laid per one-character insert at most 2, per Enter at most 2, none per arrow key; no whole-document layout;
//   - before a preparation's first publication no chunk outside the viewport (and its one-viewport overscan) is laid, and
//     each width step publishes its viewport while the rest of the chunks are laid after it.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h; uses its Check.
#include "Embedded.Tests.PerformanceFixtures.h"

#include <iostream>
#include <string_view>

namespace ChunkPlacementFx8
{
inline void TestFx8ChunksPerEditAndViewportFirst()
{
    const auto& definition = PerformanceFixtures::Fx8::kDefinition;
    for (const auto& variant : PerformanceFixtures::Fx8::kVariants)
    {
        if (variant.receiptOnly || PerformanceFixtures::Fx8::PresentationOf(variant) == PerformanceFixtures::Fx8::Presentation::Semantic)
            continue;
        const auto run = PerformanceFixtures::RunEmbedded(definition, variant);
        {
            // The first load (warm) also publishes its viewport first: chunks without previous geometry are placeholders.
            const auto offscreen = PerformanceFixtures::Summarize(run.Phase("warm"), "rich.offscreenChunksLaidFirst");
            const auto published = PerformanceFixtures::Summarize(run.Phase("warm"), "rich.viewportPublications");
            std::cout << "  FX8 " << variant.name << " warm: off-screen chunks before the first publication " << offscreen.total << ", viewport publications "
                      << published.total << '\n';
            Check(offscreen.total == 0u && published.total >= 1u, "an FX8 first load publishes its viewport before any off-screen chunk");
        }
        for (const std::string_view phase : {"prose-insert", "enter", "arrow", "fence-insert", "table-insert", "resize"})
        {
            const auto summarize = [&](std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase(phase), counter); };
            const auto laid = summarize("rich.chunksLaid"), whole = summarize("rich.wholeLayoutsLaid"), offscreen = summarize("rich.offscreenChunksLaidFirst"),
                       published = summarize("rich.viewportPublications");
            std::cout << "  FX8 " << variant.name << ' ' << phase << " per step: chunks laid " << laid.minimum << ".." << laid.maximum << ", whole layouts "
                      << whole.maximum << ", off-screen chunks before the first publication " << offscreen.total << ", viewport publications "
                      << published.minimum << ".." << published.maximum << '\n';
            Check(whole.maximum == 0u, "an FX8 v0 step lays no whole-document layout");
            Check(offscreen.total == 0u, "no FX8 v0 step lays a chunk outside the viewport before it publishes the viewport");
            if (phase == "arrow")
                Check(laid.maximum == 0u, "an FX8 arrow key lays no chunk");
            else if (phase == "resize")
                Check(published.minimum == 1u && published.maximum == 1u, "each FX8 width step publishes its viewport before the other chunks");
            else
                Check(laid.maximum <= 2u, "an FX8 insert or Enter re-lays at most two chunks (W7.3)");
        }
    }
}
} // namespace ChunkPlacementFx8
