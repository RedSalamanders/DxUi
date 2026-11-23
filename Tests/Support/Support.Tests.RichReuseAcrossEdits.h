#pragma once
// W2.3 Reuse across edits (plan Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md): semantic geometry carries prepared
// objects of its published generation into the next one by content, and a fresh preparation of the same document gives
// the same geometry and the same WARP pixels. Geometry-level tests on a WARP Direct2D device context, without a host.
#include "Support.Tests.RichSemanticGeometry.h"
#include "Support/WorkCounters.h"
#include <array>
#include <cstring>
#include <d3d11.h>
#include <format>
#include <string>
#include <vector>

namespace RichReuseAcrossEditsTests
{
using namespace DxUi::RichText;
using DxUi::RichText::Detail::FlowchartResourceEpoch;
using DxUi::RichText::Detail::PreparedSemanticGeometry;
using DxUi::RichText::Detail::RichImageResourceSet;
using RichSemanticTests::Check;

// A WARP Direct2D device context drawing into its own 640x900 bitmap, with a CPU copy for readback.
struct WarpCanvas
{
    static constexpr UINT32 kWidth = 640u, kHeight = 900u;
    wil::com_ptr<ID3D11Device> d3d;
    wil::com_ptr<ID2D1Factory1> factory;
    wil::com_ptr<ID2D1Device> device;
    wil::com_ptr<ID2D1DeviceContext> context;
    wil::com_ptr<ID2D1Bitmap1> target, readback;
    wil::com_ptr<ID2D1SolidColorBrush> text, decoration, fill;
    WarpCanvas()
    {
        THROW_IF_FAILED(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0u, D3D11_SDK_VERSION, d3d.put(), nullptr, nullptr));
        THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.put()));
        const auto dxgi = d3d.query<IDXGIDevice>();
        THROW_IF_FAILED(factory->CreateDevice(dxgi.get(), device.put()));
        THROW_IF_FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
        const auto format = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
        const auto drawn  = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, format, 96.f, 96.f);
        THROW_IF_FAILED(context->CreateBitmap(D2D1::SizeU(kWidth, kHeight), nullptr, 0u, &drawn, target.put()));
        const auto cpu = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, format, 96.f, 96.f);
        THROW_IF_FAILED(context->CreateBitmap(D2D1::SizeU(kWidth, kHeight), nullptr, 0u, &cpu, readback.put()));
        context->SetTarget(target.get());
        THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.15f), text.put()));
        THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.3f, 0.7f), decoration.put()));
        THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.95f, 0.9f), fill.put()));
    }
    [[nodiscard]] ID2D1RenderTarget* Raster() const noexcept
    {
        return context.get();
    }
    // Draws the published geometry on white and returns the BGRA pixels.
    [[nodiscard]] std::vector<std::uint8_t> Render(const PreparedSemanticGeometry& geometry)
    {
        const DxUi::RichText::Detail::DiagramColorContext colors{D2D1::ColorF(0.1f, 0.1f, 0.15f), D2D1::ColorF(0.2f, 0.3f, 0.7f), D2D1::ColorF(1.f, 1.f, 1.f)};
        context->BeginDraw();
        context->Clear(D2D1::ColorF(1.f, 1.f, 1.f));
        geometry.Draw(context.get(), text.get(), decoration.get(), {}, 0.f, static_cast<float>(kHeight), fill.get(), 1.f, nullptr, nullptr, &colors);
        THROW_IF_FAILED(context->EndDraw());
        const D2D1_POINT_2U origin{};
        const D2D1_RECT_U all{0u, 0u, kWidth, kHeight};
        THROW_IF_FAILED(readback->CopyFromBitmap(&origin, target.get(), &all));
        D2D1_MAPPED_RECT mapped{};
        THROW_IF_FAILED(readback->Map(D2D1_MAP_OPTIONS_READ, &mapped));
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kWidth) * kHeight * 4u);
        for (UINT32 row = 0u; row < kHeight; ++row)
            std::memcpy(pixels.data() + static_cast<std::size_t>(row) * kWidth * 4u, mapped.bits + static_cast<std::size_t>(row) * mapped.pitch, kWidth * 4u);
        THROW_IF_FAILED(readback->Unmap());
        return pixels;
    }
};

// Two immutable decoded pictures the tests bind by destination: a 6x4 raster and an SVG of nominal 40x30.
struct Pictures
{
    std::shared_ptr<const RichImage> raster, vector;
    Pictures()
    {
        std::array<std::byte, 6u * 4u * 4u> pixels{};
        for (std::size_t index = 0u; index < pixels.size(); index += 4u)
        {
            pixels[index]      = std::byte{static_cast<unsigned char>(index * 9u % 256u)};
            pixels[index + 1u] = std::byte{0x80};
            pixels[index + 2u] = std::byte{static_cast<unsigned char>(255u - index * 5u % 256u)};
            pixels[index + 3u] = std::byte{0xff};
        }
        THROW_IF_FAILED(RichImage::CreatePbgra(6u, 4u, pixels, raster));
        constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 40 30'><rect width='40' height='30' fill='#2060c0'/>"
                                         "<circle cx='20' cy='15' r='9' fill='#f0c020'/></svg>";
        const HRESULT apartment        = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED); // CreateSvg needs COM on this thread.
        const auto uninitialize        = wil::scope_exit([&]() noexcept
        {
            if (SUCCEEDED(apartment))
                CoUninitialize();
        });
        THROW_IF_FAILED(RichImage::CreateSvg(40u, 30u, std::as_bytes(std::span(svg)), vector));
    }
    // The application's image resources for a document: every image element bound to its picture.
    [[nodiscard]] std::shared_ptr<const RichImageResourceSet> Bind(const SemanticDocument& document) const
    {
        auto set = std::make_shared<RichImageResourceSet>();
        for (const std::size_t element : Detail::SemanticExtensionAccess::Images(document))
            set->placements.push_back({element, document.Elements()[element].destination.ends_with(L".svg") ? vector : raster});
        return set;
    }
};

struct Work
{
    std::uint64_t maths{}, svgs{}, diagrams{}, uploadBytes{};
    [[nodiscard]] bool None() const noexcept
    {
        return ! maths && ! svgs && ! diagrams && ! uploadBytes;
    }
};
[[nodiscard]] inline Work Measure(const DxUi::Detail::WorkCounterScope& scope)
{
    const auto delta = scope.Delta();
    return {delta.rich.mathPreparations, delta.rich.svgRasterizations, delta.rich.diagramPreparations, delta.rich.imageUploadBytes};
}

struct Inputs
{
    LayoutOptions options;
    FlowchartResourceEpoch epochs{1u, 1u};
};

// Prepares `document` into `geometry` to publication and returns the object work it took.
inline Work Prepare(PreparedSemanticGeometry& geometry,
                    IDWriteFactory* write,
                    WarpCanvas& canvas,
                    const Pictures& pictures,
                    const std::shared_ptr<const SemanticDocument>& document,
                    const Inputs& inputs,
                    ID2D1RenderTarget* raster = nullptr)
{
    const DxUi::Detail::WorkCounterScope scope;
    geometry.Begin(write, document, inputs.options, canvas.factory.get(), inputs.epochs, pictures.Bind(*document), raster ? raster : canvas.Raster());
    RichSemanticGeometryTests::Complete(geometry);
    if (auto* tracker = DxUi::Detail::g_resourceLifetimeTracker)
        static_cast<void>(tracker->Sweep());
    return Measure(scope);
}

// The carried generation equals a fresh preparation of the same document: element bounds, extent and WARP pixels.
inline void CheckEqualsFresh(PreparedSemanticGeometry& carried,
                             IDWriteFactory* write,
                             WarpCanvas& canvas,
                             const Pictures& pictures,
                             const std::shared_ptr<const SemanticDocument>& document,
                             const Inputs& inputs,
                             const char* what);
// Whether the carried generation equals a fresh preparation of the same document: element bounds, extent and WARP pixels.
[[nodiscard]] inline bool EqualsFresh(PreparedSemanticGeometry& carried,
                                      IDWriteFactory* write,
                                      WarpCanvas& canvas,
                                      const Pictures& pictures,
                                      const std::shared_ptr<const SemanticDocument>& document,
                                      const Inputs& inputs,
                                      const char* what)
{
    PreparedSemanticGeometry fresh;
    static_cast<void>(Prepare(fresh, write, canvas, pictures, document, inputs));
    bool same = fresh.Height() == carried.Height() && fresh.Width() == carried.Width() && fresh.BoxCount() == carried.BoxCount() &&
                fresh.RejectedNativeObjects().size() == carried.RejectedNativeObjects().size();
    for (std::size_t element = 0u; same && element < document->Elements().size(); ++element)
    {
        const auto a = carried.ElementBounds(element), b = fresh.ElementBounds(element);
        same = a.has_value() == b.has_value() && (! a || (a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom));
    }
    const bool pixels = same && canvas.Render(carried) == canvas.Render(fresh);
    if (! same || ! pixels)
        std::cerr << "  W2.3 carried versus fresh differs (" << (same ? "pixels" : "geometry") << "): " << what << '\n';
    return same && pixels;
}
inline void CheckEqualsFresh(PreparedSemanticGeometry& carried,
                             IDWriteFactory* write,
                             WarpCanvas& canvas,
                             const Pictures& pictures,
                             const std::shared_ptr<const SemanticDocument>& document,
                             const Inputs& inputs,
                             const char* what)
{
    Check(EqualsFresh(carried, write, canvas, pictures, document, inputs, what),
          "a generation with carried objects equals a fresh preparation of the same document");
}

[[nodiscard]] inline std::shared_ptr<const SemanticDocument> Build(std::wstring_view source)
{
    return RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
}

// The base document: prose, two identical display formulas, a flowchart, an inline formula, a raster and an SVG.
inline constexpr std::wstring_view kIntro   = L"Intro paragraph.\n\n";
inline constexpr std::wstring_view kFormula = L"$$\nx + y\n$$\n\n";
inline constexpr std::wstring_view kDiagram = L"```mermaid\nflowchart TD\nA[Start]-->B[Stop]\nstyle A fill:#204080,color:#ffffff\n```\n\n";
inline constexpr std::wstring_view kInline  = L"Inline $a^2$ formula.\n\n";
inline constexpr std::wstring_view kRaster  = L"![raster](picture.png)\n\n";
inline constexpr std::wstring_view kVector  = L"![vector](shape.svg)\n\n";
inline constexpr std::wstring_view kOutro   = L"Last paragraph.\n";
[[nodiscard]] inline std::wstring Source(std::wstring_view intro   = kIntro,
                                         std::wstring_view first   = kFormula,
                                         std::wstring_view second  = kFormula,
                                         std::wstring_view diagram = kDiagram,
                                         std::wstring_view inlined = kInline,
                                         std::wstring_view raster  = kRaster,
                                         std::wstring_view vector  = kVector)
{
    std::wstring text(intro);
    text.append(first).append(second).append(diagram).append(inlined).append(raster).append(vector).append(kOutro);
    return text;
}

inline void CarryAcrossEdits()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        THROW_IF_FAILED(initialized);
    const auto apartment    = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    const auto liveMaths    = DxUi::RichText::Detail::testRichLiveMathPreparations;
    const auto liveDiagrams = DxUi::RichText::Detail::testRichLiveDiagramPreparations;
    {
        WarpCanvas canvas;
        const Pictures pictures;
        const auto write = RichSemanticGeometryTests::Factory();
        std::vector<DxUi::Detail::TrackedResource> storage(16384u);
        DxUi::Detail::ResourceLifetimeTracker tracker(storage);
        Inputs inputs;
        inputs.options.widthDip = 600.f;
        PreparedSemanticGeometry geometry;
        auto document     = Build(Source());
        const auto warmed = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(warmed.maths == 3u && warmed.diagrams == 1u && warmed.svgs >= 1u && warmed.uploadBytes == 6u * 4u * 4u,
              "the base document prepares three formulas (two equal ones each their own), one diagram, one SVG raster and one upload");
        Check(DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths + 3u &&
                  DxUi::RichText::Detail::testRichLiveDiagramPreparations == liveDiagrams + 1u,
              "the published generation holds one prepared object per formula and diagram");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "base document");

        // Lifetime of the formula keys: each step replaces `document`, so the previous document is held only by the
        // geometry's published State while the next generation compares formula sources through views of its element
        // names. Under ASan Debug a view read after that document's release fails this test; Begin also refuses (fatally)
        // a carried geometry whose owner is not the published State's document.
        // Edits that leave every object unchanged carry all of them: before, between and after objects, a block inserted
        // before them (which shifts every element index), and a block deleted before them.
        struct Edit
        {
            const char* name;
            std::wstring source;
        };
        const Edit carried[] = {
            {"prose edit before the objects", Source(L"Intro paragraph, edited.\n\n")},
            {"block inserted before every object", Source(L"New first paragraph.\n\nIntro paragraph, edited.\n\n")},
            {"block deleted before every object", Source(L"Intro paragraph, edited.\n\n")},
            {"prose edit between objects", Source(L"Intro paragraph, edited.\n\n", kFormula, kFormula, kDiagram, L"Inline $a^2$ formula, edited.\n\n")},
        };
        for (const auto& edit : carried)
        {
            document        = Build(edit.source);
            const auto work = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
            if (! work.None())
                std::cerr << "  W2.3 " << edit.name << ": maths " << work.maths << ", svgs " << work.svgs << ", diagrams " << work.diagrams << ", upload bytes "
                          << work.uploadBytes << '\n';
            Check(work.None(), "an edit outside every object carries every formula, diagram, SVG raster and image upload");
            Check(DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths + 3u &&
                      DxUi::RichText::Detail::testRichLiveDiagramPreparations == liveDiagrams + 1u,
                  "carrying keeps one prepared object per formula and diagram");
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, edit.name);
        }

        // An edit inside one formula or diagram rebuilds exactly that object; the old one goes at publication.
        const auto editedFormula = Source(L"Intro paragraph, edited.\n\n", L"$$\nx + z\n$$\n\n");
        document                 = Build(editedFormula);
        auto work                = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.maths == 1u && ! work.diagrams && ! work.svgs && ! work.uploadBytes &&
                  DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths + 3u,
              "an edit inside a formula rebuilds that formula only and releases the replaced one");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "edit inside a formula");
        const auto editedDiagram = Source(L"Intro paragraph, edited.\n\n",
                                          L"$$\nx + z\n$$\n\n",
                                          kFormula,
                                          L"```mermaid\nflowchart TD\nA[Start]-->B[Halt]\nstyle A fill:#204080,color:#ffffff\n```\n\n");
        document                 = Build(editedDiagram);
        work                     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.diagrams == 1u && ! work.maths && ! work.svgs && ! work.uploadBytes &&
                  DxUi::RichText::Detail::testRichLiveDiagramPreparations == liveDiagrams + 1u,
              "an edit inside a diagram rebuilds that diagram only and releases the replaced one");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "edit inside a diagram");

        // Moving a formula into a heading changes its font size, a key field: it is rebuilt. Display and inline formulas
        // of equal source differ in mode, also a key field.
        document = Build(Source(L"Intro paragraph, edited.\n\n", L"$$\nx + z\n$$\n\n", kFormula, kDiagram, L"# Inline $a^2$ formula.\n\n"));
        work     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        // The diagram also returns to its original source here: that model was released two generations ago, so it is
        // prepared again (nothing outlives the generation that last held it).
        Check(work.maths == 1u && work.diagrams == 1u, "a formula whose font size changes (moved into a heading) is rebuilt");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "formula moved into a heading");
        document = Build(Source(L"Intro paragraph, edited.\n\n", L"$$\nx + z\n$$\n\n", kFormula, kDiagram, L"Inline $x + y$ formula.\n\n"));
        work     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.maths == 1u, "an inline formula with a display formula's source is its own object (display mode is a key field)");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "inline formula equal in source to a display one");

        // Back to the inline a^2 formula: only it is prepared again.
        document = Build(Source(L"Intro paragraph, edited.\n\n", L"$$\nx + z\n$$\n\n", kFormula));
        work     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.maths == 1u && ! work.diagrams, "restoring the inline formula prepares only that formula");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "inline formula restored");

        // Duplicates: a third equal formula gets its own object; two equal formulas each carry their own.
        const auto three = Source(L"Intro paragraph, edited.\n\n$$\nx + y\n$$\n\n", L"$$\nx + z\n$$\n\n", kFormula);
        document         = Build(three);
        work             = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.maths == 1u && DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths + 4u,
              "an inserted formula equal to an existing one is prepared once, as a fresh preparation would");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "duplicate formula inserted");

        // Deletion releases at publication, not before: the previous generation still draws until the new one is published.
        // Bitmaps are observed through the resource tracker (destroyed at the sweep after their last owner released them),
        // formulas and diagrams through their live counts.
        const auto deleteAtPublication = [&](std::wstring_view source, const std::size_t& live, std::size_t expected, const char* what)
        {
            static_cast<void>(tracker.Sweep());
            const DxUi::Detail::WorkCounterScope scope;
            const auto next = Build(source);
            geometry.Begin(write.get(), next, inputs.options, canvas.factory.get(), inputs.epochs, pictures.Bind(*next), canvas.Raster());
            const auto before     = live;
            const auto generation = geometry.Generation();
            bool heldWhilePending = true;
            while (geometry.Pending())
            {
                static_cast<void>(tracker.Sweep());
                heldWhilePending = heldWhilePending && live == before && scope.Delta().resources.bitmap.destroyed == 0u;
                static_cast<void>(geometry.Advance({1u, 3u}));
            }
            static_cast<void>(tracker.Sweep());
            const auto released = Measure(scope);
            const auto bitmaps  = scope.Delta().resources.bitmap.destroyed;
            Check(geometry.Ready() && geometry.Generation() == generation + 1u, "the deletion publishes one generation");
            if (! heldWhilePending || live != expected)
                std::cerr << "  W2.3 " << what << ": live " << live << " (expected " << expected << "), held while pending " << heldWhilePending << '\n';
            Check(heldWhilePending && released.None() && live == expected, "deleting an object releases its prepared object at publication, not before");
            document = next;
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, what);
            return bitmaps;
        };
        std::wstring withoutFormula = three;
        withoutFormula.erase(withoutFormula.find(L"$$\nx + z\n$$\n\n"), std::wstring_view(L"$$\nx + z\n$$\n\n").size());
        Check(deleteAtPublication(withoutFormula, DxUi::RichText::Detail::testRichLiveMathPreparations, liveMaths + 3u, "formula deleted") == 0u,
              "deleting a formula releases no bitmap");
        std::wstring withoutDiagram = withoutFormula;
        withoutDiagram.erase(withoutDiagram.find(L"```mermaid"), kDiagram.size());
        Check(deleteAtPublication(withoutDiagram, DxUi::RichText::Detail::testRichLiveDiagramPreparations, liveDiagrams, "diagram deleted") == 0u,
              "deleting a diagram releases no bitmap");
        std::wstring withoutRaster = withoutDiagram;
        withoutRaster.erase(withoutRaster.find(kRaster), kRaster.size());
        Check(deleteAtPublication(withoutRaster, DxUi::RichText::Detail::testRichLiveMathPreparations, liveMaths + 3u, "raster image deleted") == 1u,
              "deleting a raster image releases its bitmap at publication");
        std::wstring withoutVector = withoutRaster;
        withoutVector.erase(withoutVector.find(kVector), kVector.size());
        Check(deleteAtPublication(withoutVector, DxUi::RichText::Detail::testRichLiveMathPreparations, liveMaths + 3u, "SVG image deleted") == 1u,
              "deleting an SVG image releases its raster at publication");

        // Re-inserting a deleted object prepares it again: nothing outlives the generation that last held it.
        document = Build(three);
        work     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
        Check(work.maths == 1u && work.diagrams == 1u && work.svgs >= 1u && work.uploadBytes == 6u * 4u * 4u,
              "re-inserted objects are prepared again (no cache outlives two generations)");
        CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "objects re-inserted");

        // Two edits between publications: the abandoned preparation's own objects are released, the carry reads only the
        // published generation, and the result equals a fresh preparation.
        {
            const auto abandoned = Build(Source(L"Intro.\n\n$$\nq + r\n$$\n\n"));
            const auto published = DxUi::RichText::Detail::testRichLiveMathPreparations;
            geometry.Begin(write.get(), abandoned, inputs.options, canvas.factory.get(), inputs.epochs, pictures.Bind(*abandoned), canvas.Raster());
            for (std::size_t turn = 0u; turn < 8u && geometry.Pending(); ++turn)
                static_cast<void>(geometry.Advance({1u, 3u}));
            Check(geometry.Pending() && DxUi::RichText::Detail::testRichLiveMathPreparations > published,
                  "the abandoned preparation built an object of its own before the next edit");
            document = Build(three + L"Typed again.\n");
            work     = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
            Check(work.None() && DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths + 4u &&
                      DxUi::RichText::Detail::testRichLiveDiagramPreparations == liveDiagrams + 1u,
                  "a superseded preparation releases what it built and the next one carries from the publication");
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "two edits between publications");
        }

        // Key fields other than content: each change rebuilds what reads it, and a width change carries everything whose
        // pixels do not depend on width.
        struct Change
        {
            const char* name;
            void (*apply)(Inputs&);
            bool maths, diagrams, rasters;
        };
        const Change changes[] = {
            {"width", [](Inputs& value) { value.options.widthDip = 560.f; }, false, false, false},
            {"font size", [](Inputs& value) { value.options.fontSizeDip += 2.f; }, true, true, true},
            {"DPI", [](Inputs& value) { value.options.dpi = 144.f; }, true, false, true},
            {"math font family", [](Inputs& value) { value.options.mathFontFamily = L"Segoe UI"; }, true, false, true},
            {"math absolute-unit scale", [](Inputs& value) { value.options.mathAbsoluteUnitScale = 1.5f; }, true, true, true},
            {"locale", [](Inputs& value) { value.options.locale = L"fr-FR"; }, true, true, true},
            {"reading direction", [](Inputs& value) { value.options.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT; }, true, true, true},
            {"diagram label width", [](Inputs& value) { value.options.diagramLabelWidthDip = 200.f; }, true, true, true},
            {"typography revision", [](Inputs& value) { ++value.epochs.typography; }, true, true, true},
            {"resource generation", [](Inputs& value) { ++value.epochs.resources; }, true, true, true},
        };
        for (const auto& change : changes)
        {
            Inputs changed = inputs;
            change.apply(changed);
            work = Prepare(geometry, write.get(), canvas, pictures, document, changed);
            if ((work.maths != 0u) != change.maths || (work.diagrams != 0u) != change.diagrams || (work.uploadBytes != 0u) != change.rasters)
                std::cerr << "  W2.3 key change " << change.name << ": maths " << work.maths << ", diagrams " << work.diagrams << ", svgs " << work.svgs
                          << ", upload bytes " << work.uploadBytes << '\n';
            Check((work.maths != 0u) == change.maths && (work.diagrams != 0u) == change.diagrams && (work.uploadBytes != 0u) == change.rasters,
                  "a change of a key field rebuilds exactly the kinds that read it");
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, changed, change.name);
            work = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "key field restored");
        }

        // A replaced raster target (a device change) never carries a GPU-bound object: bitmaps, SVG rasters and target-bound
        // palettes are rebuilt, and the result equals a fresh preparation on that target.
        {
            WarpCanvas other;
            work = Prepare(geometry, write.get(), canvas, pictures, document, inputs, other.Raster());
            Check(work.maths == 4u && work.svgs >= 1u && work.uploadBytes == 6u * 4u * 4u, "a replaced raster target rebuilds every target-bound object");
            work = Prepare(geometry, write.get(), canvas, pictures, document, inputs);
            Check(work.maths == 4u && work.uploadBytes == 6u * 4u * 4u, "returning to the first target rebuilds again: no object outlives two generations");
            CheckEqualsFresh(geometry, write.get(), canvas, pictures, document, inputs, "raster target replaced and restored");
        }

        // Negative control: a deliberately wrong key (the formula source ignored) carries a stale formula into an edit
        // inside it, and the comparison with a fresh preparation must report it.
        {
            PreparedSemanticGeometry wrong;
            const auto before = Build(Source());
            static_cast<void>(Prepare(wrong, write.get(), canvas, pictures, before, inputs));
            const auto edited                                      = Build(Source(kIntro, L"$$\nx + w + v\n$$\n\n"));
            DxUi::RichText::Detail::testRichMathCarryIgnoresSource = true;
            const auto restore = wil::scope_exit([]() noexcept { DxUi::RichText::Detail::testRichMathCarryIgnoresSource = false; });
            work               = Prepare(wrong, write.get(), canvas, pictures, edited, inputs);
            DxUi::RichText::Detail::testRichMathCarryIgnoresSource = false;
            std::cerr << "  W2.3 negative control, a difference is expected next:\n";
            Check(work.maths == 0u && ! EqualsFresh(wrong, write.get(), canvas, pictures, edited, inputs, "formula source ignored"),
                  "a carry that ignores the formula source is caught by the comparison with a fresh preparation");
        }

        // Destroying the geometry while a preparation is pending releases both generations.
        auto pending = std::make_unique<PreparedSemanticGeometry>();
        static_cast<void>(Prepare(*pending, write.get(), canvas, pictures, document, inputs));
        const auto edited = Build(three + L"Pending edit $$\nnew\n$$\n");
        pending->Begin(write.get(), edited, inputs.options, canvas.factory.get(), inputs.epochs, pictures.Bind(*edited), canvas.Raster());
        for (std::size_t turn = 0u; turn < 64u && pending->Pending(); ++turn)
            static_cast<void>(pending->Advance({1u, 3u}));
        pending.reset();
    }
    Check(DxUi::RichText::Detail::testRichLiveMathPreparations == liveMaths && DxUi::RichText::Detail::testRichLiveDiagramPreparations == liveDiagrams,
          "destroying geometry, published and pending, releases every prepared formula and diagram");
}
} // namespace RichReuseAcrossEditsTests
