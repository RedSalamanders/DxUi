#include "Scope.Tests.RichDocumentLayout.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <limits>
#include <wil/result.h>

namespace
{
using namespace DxUi::RichText;
wil::com_ptr<IDWriteFactory> DocumentFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    return factory;
}
void Near(float actual, float expected, const char* message)
{
    if (std::abs(actual - expected) > 0.125f)
        std::cerr << message << " actual=" << actual << " expected=" << expected << '\n';
    Require(std::abs(actual - expected) <= 0.125f, message);
}
void CompareDocumentGeometry(PreparedDocumentLayout& prepared, PreparedLayout& flat, std::size_t stride = 1u)
{
    const auto& projection = flat.Content();
    Require(prepared.SourceLength() == projection.source.size() && prepared.SourceMatches(projection.source),
            "chunked source comparison matches every source unit");
    Require(prepared.DisplayText() == projection.text && prepared.Revision() == projection.revision, "chunked text and published revision match flat v0");
    Near(prepared.Height(), flat.Height(), "complete chunked height matches DirectWrite whole-document metrics");
    for (std::size_t position = 0u; position <= projection.source.size(); ++position)
        Require(prepared.ToDisplay(position) == projection.ToDisplay(position), "chunked source mapping covers every ACP");
    for (std::size_t position = 0u;; position = std::min(projection.text.size(), position + stride))
    {
        const auto boundary = static_cast<std::size_t>(std::lower_bound(projection.sourceToDisplay.begin(), projection.sourceToDisplay.end(), position) -
                                                       projection.sourceToDisplay.begin());
        Require(prepared.SourceBoundary(position) == boundary, "visual-line source boundary preserves the earliest hidden-marker affinity");
        if (position < projection.text.size())
            Require(prepared.DisplayUnit(position) == projection.text[position], "indexed display unit matches flat UTF16 including interior ACPs");
        const auto actual = prepared.Caret(position), expected = flat.Caret(position);
        Near(actual.x, expected.x, "chunked caret x matches whole layout");
        Near(actual.y, expected.y, "chunked caret y matches whole layout");
        Near(actual.height, expected.height, "chunked caret height matches whole layout");
        for (const bool trailing : {false, true})
            Require(prepared.ToSource(position, trailing) == projection.ToSource(position, trailing),
                    "chunked display mapping preserves both source affinities");
        Require(prepared.PreviousCluster(position) == flat.PreviousCluster(position) && prepared.NextCluster(position) == flat.NextCluster(position),
                "chunked Unicode navigation matches whole logical elements at all ACPs");
        for (const bool backwards : {false, true})
            Require(prepared.DeletionRange(position, backwards) == flat.DeletionRange(position, backwards), "chunked deletion preserves whole grapheme policy");
        Require(prepared.LineBoundary(position, false) == flat.LineBoundary(position, false) &&
                    prepared.LineBoundary(position, true) == flat.LineBoundary(position, true),
                "chunked wrapped-line navigation matches whole layout");
        if (position == projection.text.size())
            break;
    }
    for (float top = -20.0f; top < flat.Height() + 20.0f; top += std::max(7.0f, flat.Height() / 41.0f))
    {
        const auto actualRange = prepared.VisibleRange(top, top + 57.0f), expectedRange = flat.VisibleRange(top, top + 57.0f);
        if (actualRange != expectedRange)
            std::cerr << "visible top=" << top << " bottom=" << top + 57.0f << " actual=" << actualRange.first << ',' << actualRange.second
                      << " expected=" << expectedRange.first << ',' << expectedRange.second << '\n';
        Require(actualRange == expectedRange, "chunked visible lines match the complete layout");
        for (float x : {-20.0f, 0.0f, 17.0f, 103.0f, 1000.0f})
        {
            const auto actual = prepared.HitTest(x, top), expected = flat.HitTest(x, top);
            Require(actual.position == expected.position && actual.trailing == expected.trailing,
                    "chunked hit tests match whole layout inside/outside viewport");
        }
    }
    for (std::size_t first = 0u; first < projection.text.size(); first += std::max(std::size_t(1u), projection.text.size() / 23u))
    {
        const auto last   = std::min(projection.text.size(), first + 1131u);
        const auto actual = prepared.Range(first, last), expected = flat.Range(first, last);
        Require(actual.size() == expected.size(), "cross-chunk selection has the same rectangle count");
        for (std::size_t index = 0u; index < actual.size(); ++index)
        {
            Near(actual[index].left, expected[index].left, "selection left matches whole layout");
            Near(actual[index].right, expected[index].right, "selection right matches whole layout");
            Near(actual[index].top, expected[index].top, "selection top matches whole layout");
            Near(actual[index].bottom, expected[index].bottom, "selection bottom matches whole layout");
        }
    }
}
void TestRichDocumentLayoutWholeGeometryOracle()
{
    const auto factory         = DocumentFactory();
    const std::wstring pattern = L"# Heading\r\n**bold** a\u0301 \U0001f469\u200d\U0001f4bb and `code`\n"
                                 L"\u05d0\u05d1\u05d2 mixed\rline\u2028separator\u2029paragraph\n\n";
    std::wstring seed;
    for (std::size_t repeat = 0u; repeat < 29u; ++repeat)
        seed += pattern;
    for (const auto ending : {L"end", L"\n", L"\n# ", L"\n**last**\r\n"})
        for (const auto mode : {ViewMode::Source, ViewMode::Rich})
            for (const auto width : {80.0f, 417.0f})
                for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
                {
                    Document document(seed + ending);
                    LayoutOptions options;
                    options.widthDip         = width;
                    options.readingDirection = direction;
                    PreparedDocumentLayout prepared;
                    PreparedLayout flat;
                    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), mode, options);
                    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), mode), options);
                    std::cout << "rich geometry oracle mode=" << static_cast<unsigned>(mode) << " width=" << width << " direction=" << direction
                              << " sourceUnits=" << document.Length() << " endingUnits=" << std::wstring_view(ending).size() << '\n';
                    Require(prepared.LastPreparation().preparedChunks > 1u,
                            "whole-geometry oracle actually prepares multiple chunks before compatibility checks");
                    CompareDocumentGeometry(prepared, flat);
                }
}
void TestRichDocumentLayoutEditsAndFailurePublication()
{
    const auto factory = DocumentFactory();
    std::wstring seed;
    for (std::size_t line = 0u; line < 512u; ++line)
        seed += L"line **bold** \U0001f600\r\n";
    Document document(seed);
    LayoutOptions options;
    options.widthDip = 310;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    const auto originalHeight = prepared.Height();
    for (std::size_t edit = 0u; edit < 160u; ++edit)
    {
        document.Replace(seed.size() / 2u, seed.size() / 2u + 1u, edit % 2u ? L"x" : L"y", {0u, 0u});
        prepared.Invalidate();
        Require(! prepared.Ready() && prepared.Height() == 0.0f, "invalidation hides stale geometry while retaining reusable resources");
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        Require(prepared.LastPreparation().reusedChunks > 0u && prepared.LastPreparation().preparedSourceUnits <= 2048u && prepared.ChunkCount() <= 128u,
                "single-line edit reuses unchanged DirectWrite geometry and bounds rebuilt source");
        if (edit % 31u == 0u)
        {
            PreparedLayout flat;
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
            CompareDocumentGeometry(prepared, flat, 11u);
        }
    }
    Near(prepared.Height(), originalHeight, "nonwrapping source edit retains height");
    const auto generation = prepared.Generation();
    auto invalid          = options;
    invalid.widthDip      = std::numeric_limits<float>::quiet_NaN();
    bool rejected         = false;
    try
    {
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && prepared.Ready() && prepared.Generation() == generation && prepared.Revision() == document.Revision(),
            "invalid geometry options leave the published source/geometry pair unchanged");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().preparedChunks == 0u && prepared.LastPreparation().reusedChunks == prepared.ChunkCount(),
            "unchanged source/options reuse the complete prepared graph");
    Require(document.Undo(), "chunked document undo fixture");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
    CompareDocumentGeometry(prepared, flat, 7u);
    prepared.Clear();
    Require(! prepared.Ready() && prepared.ChunkCount() == 0u && prepared.Height() == 0.0f, "full retirement discards the complete chunk graph");
    rejected = false;
    try
    {
        static_cast<void>(prepared.Caret(0u));
    }
    catch (const std::logic_error&)
    {
        rejected = true;
    }
    Require(rejected, "retired geometry is unavailable");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    Require(prepared.Ready() && prepared.LastPreparation().reusedChunks == 0u, "retired resources are rebuilt on resumed preparation");
}
void TestRichDocumentLayoutLongHeightAndChunkBudget()
{
    const auto factory = DocumentFactory();
    for (const std::wstring seed : {std::wstring(Document::MaximumUnits, L'\n'), std::wstring(Document::MaximumUnits, L'a')})
    {
        Document document(seed);
        LayoutOptions options;
        options.widthDip = 321.25f;
        PreparedDocumentLayout prepared;
        PreparedLayout flat;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        Require(prepared.ChunkCount() <= 128u, "maximum source bounds the entire live document geometry graph");
        if (seed.front() == L'\n')
            Require(prepared.UsesWholeGeometry() && prepared.LastPreparation().wholeLayoutsPrepared == 1u,
                    "rounding-sensitive maximum document explicitly uses canonical whole geometry rather than hiding drift");
        CompareDocumentGeometry(prepared, flat, 257u);
    }
}
void TestRichDocumentLayoutWarpRasterOracle()
{
    const auto factory = DocumentFactory();
    std::wstring seed;
    for (std::size_t line = 0u; line < 130u; ++line)
        seed +=
            L"# Heading\n**bold** a\u0301\u0301\u0301\u0301\u0301\u0301\u0301\u0301 *italic* ~~strike~~ \U0001f600 `code`\n\u05d0\u05d1\u05d2 mixed text\r\n";
    GraphicsFixture graphics;
    THROW_IF_FAILED(graphics.Create());
    wil::com_ptr<ID2D1Factory1> d2dFactory;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(d2dFactory.put())));
    wil::com_ptr<IDXGIDevice> dxgiDevice;
    THROW_IF_FAILED(graphics.device.query_to(dxgiDevice.put()));
    wil::com_ptr<ID2D1Device> device;
    THROW_IF_FAILED(d2dFactory->CreateDevice(dxgiDevice.get(), device.put()));
    wil::com_ptr<ID2D1DeviceContext> context;
    THROW_IF_FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
    wil::com_ptr<IDXGISurface> surface;
    THROW_IF_FAILED(graphics.target.query_to(surface.put()));
    const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    THROW_IF_FAILED(context->CreateBitmapFromDxgiSurface(surface.get(), &properties, bitmap.put()));
    context->SetTarget(bitmap.get());
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush.put()));
    for (unsigned profile = 0u; profile < 3u; ++profile)
    {
        auto input = seed;
        if (profile == 2u)
        {
            for (auto position = input.find(L"# "); position != std::wstring::npos; position = input.find(L"# ", position))
                input.erase(position, 2u);
        }
        Document document(input);
        const auto mode = profile == 1u ? ViewMode::Source : ViewMode::Rich;
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            LayoutOptions options;
            options.widthDip         = 417.0f;
            options.fontSizeDip      = profile ? 16.0f : 18.0f;
            options.readingDirection = direction;
            PreparedDocumentLayout prepared;
            PreparedLayout flat;
            prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), mode, options);
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), mode), options);
            if (profile)
                Require(! prepared.UsesWholeGeometry() && prepared.ChunkCount() > 1u,
                        "safe source/styled raster fixture actually retains multiple incremental chunks");
            for (unsigned variant = 0u; variant < 3u; ++variant)
            {
                context->SetDpi(variant == 1u ? 144.0f : 96.0f, variant == 1u ? 144.0f : 96.0f);
                context->SetTransform(variant == 1u   ? D2D1::Matrix3x2F::Scale(0.75f, 0.75f)
                                      : variant == 2u ? D2D1::Matrix3x2F::Skew(6.0f, 0.0f)
                                                      : D2D1::Matrix3x2F::Identity());
                const auto viewportHeight = static_cast<float>(graphics.height) / (variant == 1u ? 1.125f : 1.0f);
                size_t viewportIndex      = 0u;
                for (const float top : {0.0f, flat.Height() * 0.5f, std::max(0.0f, flat.Height() - viewportHeight)})
                {
                    const auto raster = [&](bool chunked)
                    {
                        context->BeginDraw();
                        context->Clear(D2D1::ColorF(0, 0));
                        const auto origin = D2D1::Point2F(8.0f, -top);
                        if (chunked)
                            prepared.Draw(context.get(), brush.get(), origin, top, top + viewportHeight);
                        else
                            flat.Draw(context.get(), brush.get(), origin);
                        THROW_IF_FAILED(context->EndDraw());
                        std::vector<std::uint8_t> pixels;
                        THROW_IF_FAILED(graphics.Read(pixels));
                        return pixels;
                    };
                    const auto expected = raster(false), actual = raster(true);
                    const auto capturePath = std::filesystem::path(L".build/reports/rich-document-raster") /
                                             (L"profile-" + std::to_wstring(profile) + L"-direction-" + std::to_wstring(static_cast<unsigned>(direction)) +
                                              L"-variant-" + std::to_wstring(variant) + L"-viewport-" + std::to_wstring(viewportIndex++));
                    Require(SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-expected.png", {graphics.width, graphics.height, expected}) &&
                                SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-actual.png", {graphics.width, graphics.height, actual}),
                            "retain actual and reference WARP pixels with this native test receipt");
                    if (actual != expected)
                    {
                        std::size_t differences = 0u;
                        for (std::size_t index = 0u; index < actual.size(); ++index)
                            differences += actual[index] != expected[index];
                        std::cerr << "chunk raster profile=" << profile << " direction=" << direction << " variant=" << variant << " top=" << top
                                  << " differingBytes=" << differences << '\n';
                    }
                    Require(actual == expected, "chunked visible raster matches whole-document DirectWrite pixels exactly on WARP");
                    if (profile && variant == 0u && viewportIndex == 1u)
                        Require(! prepared.UsesWholeRaster(), "first axis-aligned safe source/styled raster actually exercises chunk drawing");
                }
            }
        }
    }
}
void TestRichDocumentLayoutBranchesOptionsAndCompaction()
{
    const auto factory = DocumentFactory();
    // Integer font metrics under 2^24 keep this fragmentation fixture in the
    // exact-placement path. Rounding-sensitive fallback is tested separately.
    Document document(std::wstring(4096u, L'\n'));
    LayoutOptions options;
    options.widthDip    = 321.25f;
    options.fontSizeDip = 2048.0f;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    bool compacted = false;
    for (size_t edit = 0u; edit < 512u; ++edit)
    {
        const auto position = (edit * 997u + 512u) % document.Length();
        document.Replace(position, position, L"x\n", {0u, 0u});
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        compacted = compacted || prepared.LastPreparation().preparedChunks > prepared.ChunkCount();
        Require(prepared.ChunkCount() <= 128u && prepared.SourceMatches(document.Text()),
                "fragmenting edits keep the published COM graph and exact source bounded");
    }
    Require(compacted, "fragmentation fixture actually exercises bounded compaction rather than only its count predicate");
    Document sibling = document;
    document.Replace(0u, 1u, L"a", {0u, 0u});
    sibling.Replace(2u, 3u, L"b", {0u, 0u});
    Require(document.Revision() == sibling.Revision(), "geometry sibling fixture has equal numeric revisions");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.SourceMatches(sibling.Text()) && ! prepared.SourceMatches(document.Text()),
            "geometry compatibility checks source identity despite equal caller revision");
    for (size_t setting = 0u; setting < 6u; ++setting)
    {
        auto changed = options;
        switch (setting)
        {
            case 0u: changed.widthDip += 1.0f; break;
            case 1u: changed.fontSizeDip += 1.0f; break;
            case 2u: changed.fontFamily = L"Consolas"; break;
            case 3u: changed.codeFontFamily = L"Segoe UI"; break;
            case 4u: changed.locale = L"fr-FR"; break;
            default: changed.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT; break;
        }
        prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, changed);
        Require(prepared.LastPreparation().reusedChunks == 0u && prepared.LastPreparation().preparedSourceUnits == sibling.Length(),
                "all typography/width/locale/direction changes invalidate incompatible geometry");
        prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    }
    wil::com_ptr<IDWriteFactory> isolated;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(isolated.put())));
    prepared.Prepare(isolated.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().reusedChunks == 0u, "different retained factory identity cannot reuse previous geometry");
    for (size_t edit = 0u; edit < 160u; ++edit)
        sibling.Replace(0u, 1u, edit % 2u ? L"c" : L"d", {0u, 0u});
    prepared.Prepare(isolated.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().reusedChunks == 0u && prepared.LastPreparation().preparedSourceUnits == sibling.Length(),
            "expired bounded source ancestry falls back to complete preparation");
}
} // namespace
void RunRichDocumentLayoutTests()
{
    DXUI_RUN_TEST(TestRichDocumentLayoutWholeGeometryOracle);
    DXUI_RUN_TEST(TestRichDocumentLayoutEditsAndFailurePublication);
    DXUI_RUN_TEST(TestRichDocumentLayoutLongHeightAndChunkBudget);
    DXUI_RUN_TEST(TestRichDocumentLayoutWarpRasterOracle);
    DXUI_RUN_TEST(TestRichDocumentLayoutBranchesOptionsAndCompaction);
}
