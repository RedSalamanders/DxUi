#pragma once
// W7.7 round 2: the strips an SVG raster is drawn in follow the budget of the turn that draws them. A caller passes
// LayoutWorkBudget::maximumUploadBytes at every Advance, and it may pass less than it did at the turn that drew the first strip:
// preparation still completes, within a bounded number of turns, with the pixels of a single-budget run, and no turn uploads
// more than its budget. Geometry-level, on a WARP Direct2D device context, without a host.
#include "Support.Tests.RichReuseAcrossEdits.h"

#include <iostream>
#include <string_view>

namespace RichSvgStripTests
{
using namespace DxUi::RichText;
using DxUi::RichText::Detail::PreparedSemanticGeometry;
using DxUi::RichText::Detail::RichImageResourceSet;
using RichSemanticTests::Check;

struct Outcome
{
    unsigned turns{};
    bool ready{};
    std::size_t largestUpload{};
    bool overBudget{};
    std::vector<std::uint8_t> pixels;
};

inline void StripsFollowTheTurnBudget()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    RichReuseAcrossEditsTests::WarpCanvas canvas;
    const auto write = RichSemanticGeometryTests::Factory();
    // 600x400 nominal: 240,000 pixels, more than one strip at any budget below 960,000 bytes.
    constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 600 400'><rect width='600' height='400' fill='#2060c0'/>"
                                     "<circle cx='300' cy='200' r='150' fill='#f0c020'/>"
                                     "<path d='M40 360 L300 40 L560 360 Z' fill='none' stroke='#ffffff' stroke-width='6'/></svg>";
    std::shared_ptr<const RichImage> vector;
    THROW_IF_FAILED(RichImage::CreateSvg(600u, 400u, std::as_bytes(std::span(svg)), vector));
    const auto document = RichReuseAcrossEditsTests::Build(L"![vector](shape.svg)\n");
    LayoutOptions options;
    options.widthDip = 640.f;
    auto set         = std::make_shared<RichImageResourceSet>();
    for (const std::size_t element : Detail::SemanticExtensionAccess::Images(*document))
        set->placements.push_back({element, vector});
    const std::shared_ptr<const RichImageResourceSet> images = set;
    const auto run                                           = [&](auto&& budgetFor)
    {
        PreparedSemanticGeometry geometry;
        geometry.Begin(write.get(), document, options, canvas.factory.get(), {1u, 1u}, images, canvas.Raster());
        Outcome outcome;
        while (geometry.Pending() && outcome.turns < 4000u)
        {
            const LayoutWorkBudget budget = budgetFor(outcome.turns);
            const auto advance            = geometry.Advance(budget);
            Check(advance.status != LayoutPreparationStatus::Failed, "SVG strip preparation does not fail");
            outcome.largestUpload = std::max(outcome.largestUpload, advance.uploadedBytes);
            outcome.overBudget    = outcome.overBudget || advance.uploadedBytes > budget.maximumUploadBytes;
            ++outcome.turns;
        }
        outcome.ready = geometry.Ready();
        if (outcome.ready)
            outcome.pixels = canvas.Render(geometry);
        return outcome;
    };
    const auto whole = run([](unsigned) { return LayoutWorkBudget{}; });
    // The first turn has room for a large strip (512 KiB: 218 rows of 600 pixels) but not for the whole picture; then three
    // turns of the smallest budget a caller may pass (one pixel), then 64 KiB turns (27 rows).
    const auto varied = run([](unsigned turn)
    {
        const std::size_t bytes = turn == 0u ? 512u * 1024u : turn <= 3u ? 4u : 64u * 1024u;
        return LayoutWorkBudget{4u, 4096u, bytes};
    });
    std::cout << "  SVG strips: whole budget " << whole.turns << " turns; 512 KiB, 3 x 4 B, then 64 KiB: " << varied.turns << " turns, largest upload "
              << varied.largestUpload << " B\n";
    Check(whole.ready && ! whole.overBudget, "a 240,000-pixel SVG prepares at the default budget");
    Check(varied.ready && varied.turns < 200u,
          "an SVG whose later turns have less budget than the turn that drew its first strip still completes, in few turns");
    Check(! varied.overBudget && varied.largestUpload <= 512u * 1024u, "no turn uploads more than its own budget");
    Check(varied.pixels == whole.pixels, "strips drawn under varying budgets give the pixels of a single-budget run");
}
} // namespace RichSvgStripTests
