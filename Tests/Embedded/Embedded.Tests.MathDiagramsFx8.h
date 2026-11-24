#pragma once
// W7.7 gate on FX8 (plan item W7.7, math and diagrams), through the editor on the embedded host, on the fixture's default
// variant, whose first formula is colored, whose first diagram is styled (a 3 px colored edge and a dashed back edge with a
// bend) and whose document holds two SVG pictures:
//   - math font reads: none in any phase after the warm-up, the formula-insert phase (which prepares the first formula again
//     at every step) included; the warm-up reads the font at most once;
//   - luminance evaluations (each up to three std::pow) in a steady paint: none, in every phase that prepares no colored
//     object; the formula-insert steps prepare the colored formula again, whose first paint decides its contrast once;
//   - connector draws per edge per paint: one DrawGeometry of the edge's prepared path, never a DrawLine per segment;
//   - Direct2D device contexts created per preparation: at most one, with both SVG pictures rasterized in it.
// Every clause is evaluated before the test fails, so a run on code that misses several clauses reports each of them.
// Included by Embedded.Tests.Embedded.cpp after Embedded.Tests.PerformanceFixtures.h; uses its Check.
#include "Controls/DxUi.PreparedMath.h"
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Embedded.Tests.PerformanceFixtures.h"

#include <array>
#include <iostream>
#include <string_view>

namespace MathDiagramsFx8
{
inline void TestFx8MathAndDiagramWork()
{
    const auto& definition = PerformanceFixtures::Fx8::kDefinition;
    const auto run         = PerformanceFixtures::RunEmbedded(definition, PerformanceFixtures::Fx8::kVariants[0]);
    const auto summarize   = [&](std::string_view phase, std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase(phase), counter); };
    unsigned failed        = 0u;
    const auto expect      = [&](bool holds, const char* clause)
    {
        if (! holds)
        {
            ++failed;
            std::cerr << "  CLAUSE NOT MET: " << clause << '\n';
        }
    };
    {
        const auto reads     = summarize("warm", "rich.mathFontReads");
        const auto maths     = summarize("warm", "rich.mathPreparations");
        const auto svgs      = summarize("warm", "rich.svgRasterizations");
        const auto contexts  = summarize("warm", "resources.deviceContext.created");
        const auto luminance = summarize("warm", "rich.luminanceEvaluations");
        std::cout << "  FX8 warm: math font reads " << reads.total << " for " << maths.total << " formulas, SVG rasterizations " << svgs.total
                  << ", device contexts created per step (max) " << contexts.maximum << ", luminance evaluations " << luminance.total << '\n';
        expect(maths.total == 3u && reads.total <= 1u, "the FX8 warm-up prepares its three formulas from at most one math font read");
        expect(svgs.total >= 2u, "the FX8 warm-up rasterizes both SVG pictures (so that one device context serves two placements)");
        expect(contexts.maximum <= 1u, "an FX8 preparation creates at most one Direct2D device context for its SVG pictures (W7.7)");
    }
    for (const std::string_view phase : {"prose-insert", "enter", "arrow", "fence-insert", "table-insert", "resize", "seek-formula", "formula-insert"})
    {
        const auto reads      = summarize(phase, "rich.mathFontReads");
        const auto maths      = summarize(phase, "rich.mathPreparations");
        const auto luminance  = summarize(phase, "rich.luminanceEvaluations");
        const auto connectors = summarize(phase, "rich.edgeConnectorDraws");
        const auto edges      = summarize(phase, "rich.edgesDrawn");
        const auto contexts   = summarize(phase, "resources.deviceContext.created");
        std::cout << "  FX8 " << phase << " (" << run.Phase(phase).steps.size() << " steps): math font reads " << reads.total << ", formulas prepared "
                  << maths.total << ", luminance evaluations " << luminance.total << ", edge connector draws " << connectors.total << " for " << edges.total
                  << " edges drawn, device contexts created (max per step) " << contexts.maximum << '\n';
        expect(reads.total == 0u, "an FX8 edit prepares no formula from a new math font read (W7.7)");
        expect(connectors.total == edges.total, "an FX8 paint strokes each edge connector with one draw call, a DrawGeometry of its prepared path (W7.7)");
        expect(contexts.maximum <= 1u, "an FX8 preparation creates at most one Direct2D device context (W7.7)");
        if (phase != "formula-insert")
            expect(luminance.total == 0u, "an FX8 paint of a colored formula and a styled diagram runs no luminance evaluation, so no std::pow (W7.7)");
        if (phase == "formula-insert")
            expect(maths.minimum >= 1u, "every FX8 formula-insert step prepares the first formula again");
    }
    {
        // The paints exist: the edit phases draw the diagram's edges, so the equalities above do not hold vacuously.
        const auto edges = summarize("prose-insert", "rich.edgesDrawn");
        expect(edges.minimum > 0u, "every FX8 prose insert paints the flowchart's edges");
    }
    Check(failed == 0u, "the FX8 math and diagram work gates of W7.7 hold");
}

// The cache of math font data (W7.7) serves a family from its entry while the system font collection is the one the entry was
// read from, and reads the font again, replacing the entry, when it is another collection. A second isolated DirectWrite
// factory has its own system collection with the same fonts: that is what a changed set of system fonts looks like.
inline void TestMathFontDataFollowsTheFontCollection()
{
#if DXUI_ENABLE_DIAGNOSTICS
    using namespace DxUi::RichText;
    wil::com_ptr<IDWriteFactory> factory, other;
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())), "math font cache factory");
    Hr(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(other.put())),
       "math font cache second factory");
    wil::com_ptr<IDWriteFontCollection> first, again, second;
    Hr(factory->GetSystemFontCollection(first.put(), FALSE), "system font collection");
    Hr(factory->GetSystemFontCollection(again.put(), FALSE), "system font collection again");
    Hr(other->GetSystemFontCollection(second.put(), FALSE), "second system font collection");
    Check(first.get() == again.get() && first.get() != second.get(), "a factory answers its current system font collection as one object");
    const auto expression = std::make_shared<const MathExpression>(ParseMath(L"x^2"));
    using Math            = DxUi::RichText::Detail::PreparedMath;
    // The cache keeps no data of its own: a font's data lives while a prepared formula holds it. `held` is the formula a
    // generation carries; each preparation replaces it, so the previous one is alive while the next is prepared.
    std::unique_ptr<Math> held;
    const auto prepare = [&]
    {
        const DxUi::Detail::WorkCounterScope scope;
        auto next = std::make_unique<Math>(factory.get(), expression, 24.f);
        while (! next->Advance())
        {
        }
        held = std::move(next);
        return scope.Delta().rich.mathFontReads;
    };
    Math::DropFontCache();
    Check(Math::FontCacheEntries() == 0u, "the dropped cache of math font data is empty");
    Check(prepare() == 1u && Math::FontCacheEntries() == 1u, "the first formula reads the math font once and the cache knows it");
    Check(prepare() == 0u && prepare() == 0u, "every later formula, with no donor formula, reads no font while a formula holds the data");
    Check(Math::FontCacheBytes() > 0u, "the cache reports the bytes of the data it knows");
    DxUi::RichText::Detail::testRichMathCollection = second.get();
    const auto restore                             = wil::scope_exit([]() noexcept { DxUi::RichText::Detail::testRichMathCollection = nullptr; });
    Check(prepare() == 1u && Math::FontCacheEntries() == 1u, "another system font collection replaces the entry: the font is read again");
    Check(prepare() == 0u, "the replacement serves the new collection");
    DxUi::RichText::Detail::testRichMathCollection = nullptr;
    Check(prepare() == 1u && prepare() == 0u && Math::FontCacheEntries() == 1u, "the first collection again is another change and reads once more");
    Math::DropFontCache();
    Check(Math::FontCacheEntries() == 0u && prepare() == 1u, "a dropped cache reads the font again");
    held.reset();
    Check(Math::FontCacheEntries() == 0u && Math::FontCacheBytes() == 0u, "with no formula left the cache holds nothing");
#endif
}

// How many strips an SVG raster is drawn in follows from its pixels and its elements (PlanSvgStrips), within a turn's budget.
inline void TestSvgStripPlan()
{
    using DxUi::RichText::Detail::PlanSvgStrips;
    using DxUi::RichText::Detail::SvgElementEstimate;
    constexpr size_t turn = 4u * 1024u * 1024u; // the default LayoutWorkBudget upload bytes: 1,048,576 pixels
    const auto strips     = [](uint32_t height, DxUi::RichText::Detail::SvgStripPlan plan) { return (height + plan.rows - 1u) / plan.rows; };
    const auto tiny       = PlanSvgStrips(80u, 60u, 3u, turn);
    Check(tiny.columns == 80u && tiny.rows == 60u && strips(60u, tiny) == 1u, "a small SVG raster is one strip");
    const auto large = PlanSvgStrips(4096u, 4096u, 100u, turn);
    Check(large.columns == 4096u && large.rows == 254u && strips(4096u, large) == 17u, "a 16-megapixel SVG with 100 elements takes 17 strips of 254 rows");
    const auto dense = PlanSvgStrips(4096u, 4096u, 2048u, turn);
    Check(dense.columns == 4096u && dense.rows == 224u && strips(4096u, dense) == 19u, "walking 2,048 elements shortens each strip: 19 strips of 224 rows");
    Check(dense.columns * dense.rows * 4u <= turn && large.columns * large.rows * 4u <= turn, "no strip exceeds a turn's upload budget");
    const auto bound = PlanSvgStrips(4096u, 4096u, 100000u, turn);
    Check(bound.rows == 128u, "a document whose walk costs more than the budget still gets strips of at least half of it");
    const auto wide = PlanSvgStrips(2'000'000u, 4u, 0u, turn);
    Check(wide.columns == 1'048'576u && wide.rows == 1u, "a row wider than a strip is drawn in column strips of one row");
    const auto least = PlanSvgStrips(10u, 10u, 0u, 4u);
    Check(least.columns == 1u && least.rows == 1u, "the smallest budget, one pixel, draws one-pixel strips");
    const std::string_view document = "<?xml version='1.0'?><svg xmlns='x'><rect/><g><path d='M0 0'/></g><!-- c --></svg>";
    Check(SvgElementEstimate(std::as_bytes(std::span(document))) == 4u, "the element estimate counts the tags that open a name: svg, rect, g, path");
}

inline void Run()
{
    TestSvgStripPlan();
    TestMathFontDataFollowsTheFontCollection();
    TestFx8MathAndDiagramWork();
}
} // namespace MathDiagramsFx8
