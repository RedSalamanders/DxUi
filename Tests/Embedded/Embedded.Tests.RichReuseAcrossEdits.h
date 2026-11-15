#pragma once
// W2.3 Reuse across edits (plan Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md) on the embedded host: the FX8 gate (a
// one-character prose insert prepares no formula, SVG raster or diagram and uploads no image byte), the other FX8 phases,
// and a device replacement between two edits, which carries no GPU-bound object. Included by Embedded.Tests.Embedded.cpp
// after Embedded.Tests.PerformanceFixtures.h; uses its Check, Hr and PrepareRichEmbeddedContent.
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Embedded.Tests.IncrementalParity.h"
#include "Embedded.Tests.PerformanceFixtureFx8.h"
#include "Embedded.Tests.PerformanceFixtures.h"
#include "Support.Tests.IncrementalParity.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include "Support/WorkCounters.h"

#include <iostream>

namespace RichReuseAcrossEditsEmbedded
{
struct ObjectWork
{
    uint64_t maths = 0u, svgs = 0u, diagrams = 0u, uploadBytes = 0u;
    [[nodiscard]] bool None() const noexcept
    {
        return ! maths && ! svgs && ! diagrams && ! uploadBytes;
    }
};
[[nodiscard]] inline ObjectWork Objects(const DxUi::Detail::WorkCounters& delta) noexcept
{
    return {delta.rich.mathPreparations, delta.rich.svgRasterizations, delta.rich.diagramPreparations, delta.rich.imageUploadBytes};
}
inline std::ostream& operator<<(std::ostream& out, const ObjectWork& work)
{
    return out << "maths " << work.maths << ", SVG rasterizations " << work.svgs << ", diagrams " << work.diagrams << ", upload bytes " << work.uploadBytes;
}

// G12 on FX8 (default variant): per one-character prose insert, 0 formulas, SVG rasterizations and diagrams prepared and 0
// image bytes uploaded. Enter, arrow keys, inserts in the code fence and next to the table and the width steps carry every
// object too: none of them changes an object's content or a key field (the SVG's raster size depends on the width only when
// the width is below its 80 DIP, which the steps never reach).
inline void TestFx8CarriesObjectsAcrossEdits()
{
    const auto& definition = PerformanceFixtures::Fx8::kDefinition;
    const auto run         = PerformanceFixtures::RunEmbedded(definition, PerformanceFixtures::Fx8::kVariants[0]);
    const auto warm        = [&](std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase("warm"), counter).total; };
    Check(warm("rich.mathPreparations") == 3u && warm("rich.diagramPreparations") == 2u && warm("rich.svgRasterizations") >= 1u &&
              warm("rich.imageUploadBytes") > 0u,
          "FX8 warm-up prepares its three formulas, two diagrams, the SVG raster and both raster images");
    for (const std::string_view phase : {"prose-insert", "enter", "arrow", "fence-insert", "table-insert", "resize"})
    {
        const auto summarize = [&](std::string_view counter) { return PerformanceFixtures::Summarize(run.Phase(phase), counter); };
        const auto maths = summarize("rich.mathPreparations"), svgs = summarize("rich.svgRasterizations"), diagrams = summarize("rich.diagramPreparations"),
                   uploads = summarize("rich.imageUploadBytes"), created = summarize("resources.bitmap.created"),
                   destroyed = summarize("resources.bitmap.destroyed");
        std::cout << "  FX8 " << phase << " per step (max): maths " << maths.maximum << ", SVG rasterizations " << svgs.maximum << ", diagrams "
                  << diagrams.maximum << ", upload bytes " << uploads.maximum << "; bitmaps created " << created.total << ", destroyed " << destroyed.total
                  << " over the phase\n";
        Check(maths.maximum == 0u && svgs.maximum == 0u && diagrams.maximum == 0u && uploads.maximum == 0u,
              "an FX8 edit, arrow key or width step prepares no formula, SVG raster or diagram and uploads no image byte (G12)");
        // A width step replaces the view's surface bitmap (one per step); no other bitmap is created or released.
        const uint64_t surfaces = phase == "resize" ? run.Phase(phase).steps.size() : 0u;
        Check(created.total == surfaces && destroyed.total == surfaces, "an FX8 edit creates and releases no rich bitmap");
    }
    for (const auto& sample : run.memory)
        if (sample.retained)
            std::cout << "  FX8 memory " << sample.at << (sample.phase.empty() ? "" : " ") << sample.phase << ": retained bitmaps "
                      << sample.retained->retainedBitmaps << " (" << sample.retained->retainedBitmapBytes << " B), retained cache entries "
                      << sample.retained->retainedCacheEntries << " (" << sample.retained->retainedCacheBytes << " B), tracked live C++ bytes "
                      << sample.trackedLiveBytes.value_or(0) << '\n';
}

// The richObjects oracle with a deliberately wrong carry key (the formula source ignored) must differ at the step that
// edits inside a formula, so its "0 differences" on the real key means the comparison sees carried objects.
inline void TestRichObjectsOracleCatchesAWrongCarry(GraphicsFixture& gpu)
{
    DxUi::RichText::Detail::testRichMathCarryIgnoresSource = true;
    const auto restore                                     = wil::scope_exit([]() noexcept { DxUi::RichText::Detail::testRichMathCarryIgnoresSource = false; });
    const auto make                                        = [&] { return std::make_unique<EmbeddedParity::EmbeddedParityScene>(gpu.device.get()); };
    const std::string difference                           = IncrementalParity::RunParityScriptOnce(
        IncrementalParity::RichObjectsScript<EmbeddedParity::EmbeddedParityScene>(), IncrementalParity::kRichObjectsSeeds[0], make);
    std::cout << "  embedded wrong-carry negative control: " << difference << '\n';
    Check(difference.starts_with("step 4 ") && difference.find("inside the first formula") != std::string::npos,
          "a formula carried under a wrong key differs from a fresh preparation at the edit inside it (embedded)");
}

// A document with every object kind, its images completed by the application as FX8 does.
class ObjectScene final
{
public:
    explicit ObjectScene(ID3D11Device* device)
    {
        Hr(DxUi::GraphicsDevice::Create(device, graphics_), "object scene graphics");
        Hr(view_.Attach(graphics_), "object scene view");
        auto owned = std::make_unique<DxUi::RichTextEditor>(L"Intro paragraph.\n\n$$\nx + y\n$$\n\n```mermaid\nflowchart TD\nA[Start]-->B[Stop]\nstyle A "
                                                            L"fill:#204080,color:#ffffff\n```\n\nInline $a^2$ formula.\n\n"
                                                            L"![raster](picture.png)\n\n![vector](shape.svg)\n\nLast paragraph.\n");
        editor_    = owned.get();
        editor_->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
        editor_->SetMarkdownRenderingEnabled(true);
        editor_->SetDocumentFilePath(L"C:\\fixture\\objects.md"); // resolves relative image sources; never opened
        editor_->SetSvgImagesEnabled(true);
        view_.Controls().SetRoot(std::move(owned));
        view_.Controls().SetFocusControl(editor_, false);
        Prepare();
        Prepare(); // the second preparation draws the completed images
    }
    ~ObjectScene()
    {
        view_.Detach();
    }
    ObjectScene(const ObjectScene&)            = delete;
    ObjectScene& operator=(const ObjectScene&) = delete;
    [[nodiscard]] DxUi::EmbeddedHost& View() noexcept
    {
        return view_;
    }
    [[nodiscard]] DxUi::RichTextEditor& Editor() noexcept
    {
        return *editor_;
    }
    void Prepare()
    {
        Hr(PrepareRichEmbeddedContent(view_, kWidth, kHeight), "object scene preparation");
        std::vector<DxUi::RichText::RichImageRequest> requests;
        editor_->TakePendingImageRequests(requests);
        for (const auto& request : requests)
            static_cast<void>(editor_->CompleteImageRequest(request,
                                                            request.Source().ends_with(L".svg") ? IncrementalParity::RichObjectPictures().vector
                                                                                                : IncrementalParity::RichObjectPictures().raster));
        if (! requests.empty())
            Hr(PrepareRichEmbeddedContent(view_, kWidth, kHeight), "object scene preparation with completed images");
    }
    // One prose character typed at the end of the intro paragraph, prepared; returns the object work it took.
    ObjectWork TypeInProse()
    {
        const DxUi::Detail::WorkCounterScope scope;
        const auto text = editor_->GetText();
        const auto at   = text.find(L"Intro paragraph") + 15u;
        editor_->SetSelection({at, at});
        Check(editor_->ReplaceSelection(L"x"), "object scene prose edit");
        Prepare();
        return Objects(scope.Delta());
    }
    [[nodiscard]] std::vector<uint8_t> Surface()
    {
        DxUi::WindowHostBitmapCapture surface;
        Hr(view_.DebugReadSurfaceForTest(surface), "object scene surface read");
        return std::move(surface.bgraPixels);
    }
    // The surface of a fresh preparation of the same document: every retained cache dropped and nothing carried.
    [[nodiscard]] std::vector<uint8_t> FreshSurface()
    {
        DxUi::Detail::SetFreshPreparationForTest(true);
        const auto restore = wil::scope_exit([] { DxUi::Detail::SetFreshPreparationForTest(false); });
        view_.DebugDropRetainedStateForTest();
        Hr(PrepareRichEmbeddedContent(view_, kWidth, kHeight), "object scene fresh preparation");
        return Surface();
    }
    static constexpr UINT kWidth = 640u, kHeight = 720u;

private:
    std::shared_ptr<DxUi::GraphicsDevice> graphics_;
    DxUi::EmbeddedHost view_;
    DxUi::RichTextEditor* editor_ = nullptr;
};

// A device replacement between two edits: the resource generation changes, so no bitmap, SVG raster, target-bound palette
// or formula is carried across it; the next edit carries again; pixels equal a fresh preparation throughout. A theme change
// is not a key of any prepared object (colors are bound at paint): it carries everything.
inline void TestEmbeddedCarryAcrossDeviceReplacement(GraphicsFixture& gpu)
{
    ObjectScene scene(gpu.device.get());
    auto work = scene.TypeInProse();
    std::cout << "  embedded object scene, prose edit: " << work << '\n';
    Check(work.None(), "a prose edit carries every prepared object on the embedded host");
    auto carried = scene.Surface();
    Check(carried == scene.FreshSurface(), "carried objects draw the pixels of a fresh preparation (embedded)");

    GraphicsFixture replacement;
    replacement.width  = gpu.width;
    replacement.height = gpu.height;
    Hr(replacement.Create(), "object scene replacement WARP device");
    std::shared_ptr<DxUi::GraphicsDevice> next;
    Hr(DxUi::GraphicsDevice::Create(replacement.device.get(), next), "object scene replacement pool");
    Hr(scene.View().ReplaceDevice(next), "object scene device replacement between two edits");
    work = scene.TypeInProse();
    std::cout << "  embedded object scene, edit after ReplaceDevice: " << work << '\n';
    Check(work.maths == 2u && work.diagrams == 1u && work.svgs >= 1u && work.uploadBytes > 0u,
          "after a device replacement every formula, diagram, SVG raster and image is prepared again on the new device");
    std::vector<DxUi::RichText::RichImageRequest> requests;
    scene.Editor().TakePendingImageRequests(requests);
    Check(requests.empty(), "the replacement re-uploads held pixels without asking the application again");
    carried = scene.Surface();
    Check(carried == scene.FreshSurface(), "objects prepared after a device replacement draw the pixels of a fresh preparation");
    work = scene.TypeInProse();
    Check(work.None(), "the next edit on the new device carries every object again");

    auto theme = scene.View().Controls().GetTheme();
    scene.View().Controls().SetTheme(DxUi::MakeDefaultThemePalette(! theme.dark));
    const DxUi::Detail::WorkCounterScope themed;
    scene.Prepare();
    work                 = scene.TypeInProse();
    const auto themeWork = Objects(themed.Delta());
    std::cout << "  embedded object scene, theme change and edit: " << themeWork << '\n';
    Check(themeWork.None(), "a theme change carries every prepared object: none of them binds a theme color");
    carried = scene.Surface();
    Check(carried == scene.FreshSurface(), "carried objects after a theme change draw the pixels of a fresh preparation");
}
} // namespace RichReuseAcrossEditsEmbedded
