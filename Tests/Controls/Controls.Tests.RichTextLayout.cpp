#include "Controls.Tests.RichTextLayout.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../../src/Controls/DxUi.RichSourceInk.h"
#include "../../src/Controls/DxUi.RichTextSourceInkRenderer.h"
#include "../Support/Support.Tests.PreparedFlowchart.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <limits>
#include <wil/result.h>
#include <wrl/implements.h>
namespace
{
using namespace DxUi::RichText;
template <typename F> void Reject(F&& operation, const char* description)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const std::exception&)
    {
        rejected = true;
    }
    Require(rejected, description);
}

class SourceInkTestPositioning final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
public:
    HRESULT STDMETHODCALLTYPE Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        *metrics = {0.0f, 20.0f, 10.0f, FALSE};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        *metrics = {};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) noexcept override
    {
        if (! before || ! after)
            return E_POINTER;
        *before = *after = DWRITE_BREAK_CONDITION_NEUTRAL;
        return S_OK;
    }
};

void TestRichTextLayoutPrototypeContracts()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    Document document(L"# Heading\nRegular **bold** \u03b2 \U0001f600 "
                      L"a\u0301\n\u05d0\u05d1\u05d2 mixed text");
    PreparedLayout layout;
    layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 300);
    Require(layout.Ready() && layout.Height() > 50, "real mixed-style DirectWrite layout");
    const auto& text = layout.Content().text;
    auto emoji       = text.find(L"\U0001f600");
    Require(layout.NextCluster(emoji) == emoji + 2, "surrogate cluster navigation");
    Require(layout.PreviousCluster(emoji + 2) == emoji, "surrogate reverse navigation");
    const auto accent = text.find(L"a\u0301");
    Require(layout.NextCluster(accent) == accent + 2, "combining cluster navigation");
    const auto caret = layout.Caret(0);
    Require(std::isfinite(caret.x) && caret.height > 18, "heading caret geometry");
    const auto hit = layout.HitTest(caret.x + 0.1f, caret.y + caret.height / 2);
    Require(hit.position == 0, "caret hit test");
    const auto rectangles = layout.Range(0, text.size());
    Require(rectangles.size() >= 3, "multiline bidi selection geometry");
    for (const auto& r : rectangles)
        Require(r.right >= r.left && r.bottom > r.top, "valid selection bounds");
    Require(layout.LineBoundary(0, false) == 0 && layout.LineBoundary(0, true) == 7, "line home and end");
    const auto generation = layout.Generation();
    layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 80);
    Require(layout.Generation() == generation + 1, "new width prepares new layout");
    Reject([&] { layout.Prepare(nullptr, {}, 100); }, "null borrowed factory rejected");
    Require(layout.Generation() == generation + 1 && layout.Ready(), "failed preparation retains previous consistent layout");
    Reject([&] { layout.Caret(text.size() + 1000); }, "invalid caret rejected");
    Document plain(L"MMMM"), strong(L"**MMMM**");
    PreparedLayout regular, bold;
    regular.Prepare(factory.get(), Project(plain.Snapshot(), 1, ViewMode::Rich), 500);
    bold.Prepare(factory.get(), Project(strong.Snapshot(), 1, ViewMode::Rich), 500);
    Require(bold.Caret(4).x > regular.Caret(4).x, "bold style changes actual glyph advances");
    Document empty;
    layout.Prepare(factory.get(), Project(empty.Snapshot(), 1, ViewMode::Rich), 100);
    Require(layout.Caret(0).height > 0, "empty document caret");
}
wil::com_ptr<IDWriteFactory> CreateRichTextTestFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    return factory;
}

void TestRichTextLayoutOptionsFailureAtomicityAndRetirement()
{
    const auto factory = CreateRichTextTestFactory();
    Document document(L"Plain **bold** \U0001f600");
    const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
    PreparedLayout layout;
    Reject([&] { layout.PreviousCluster(0); }, "unprepared reverse navigation rejects");
    Reject([&] { layout.NextCluster(0); }, "unprepared navigation rejects");
    Reject([&] { layout.LineBoundary(0, false); }, "unprepared line navigation rejects");
    Reject([&] { layout.HitTest(0, 0); }, "unprepared hit test rejects");
    Reject([&] { layout.Range(0, 0); }, "unprepared selection geometry rejects");
    Reject([&] { layout.Draw(nullptr, nullptr, {}); }, "unprepared raster preparation rejects");
    LayoutOptions options;
    options.widthDip       = 360;
    options.fontFamily     = L"Consolas";
    options.codeFontFamily = L"Segoe UI";
    options.locale         = L"fr-FR";
    options.fontSizeDip    = 16;
    layout.Prepare(factory.get(), projection, options);
    const auto smallAdvance = layout.Caret(5).x;
    options.fontSizeDip     = 32;
    layout.Prepare(factory.get(), projection, options);
    Require(layout.Caret(5).x > smallAdvance * 1.5f, "caller font settings change real prepared geometry");
    const auto generation = layout.Generation();
    auto invalid          = options;
    invalid.widthDip      = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "nonfinite geometry rejected");
    invalid = options;
    invalid.fontFamily.clear();
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "empty font family rejected");
    invalid        = options;
    invalid.locale = std::wstring(L"fr\0FR", 5);
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "embedded NUL locale rejected");
    invalid                            = options;
    invalid.paragraphSpacingAfterLines = std::numeric_limits<float>::quiet_NaN();
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "nonfinite semantic paragraph spacing rejected at public layout admission");
    invalid                    = options;
    invalid.listItemSpacingDip = 257.0f;
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "out-of-range semantic list spacing rejected at public layout admission");
    auto malformed                = projection;
    malformed.runs.front().length = projection.text.size() + 1;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "out-of-range public style rejected");
    malformed                      = projection;
    malformed.runs.front().heading = 7;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "invalid heading rejected");
    malformed = projection;
    malformed.text.assign(1, 0xd800);
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "invalid display UTF-16 rejected");
    malformed                         = projection;
    malformed.sourceToDisplay.front() = projection.text.size() + 1;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "invalid source/display map rejected");
    malformed                   = projection;
    malformed.units.front().end = projection.source.size() + 1;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "invalid display/source map rejected");
    malformed = projection;
    malformed.units.clear();
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "missing display mapping rejected");
    malformed                    = projection;
    malformed.runs.front().flags = 0x80;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "unknown style bits rejected");
    malformed                     = projection;
    malformed.runs.front().start  = projection.text.size() - 1;
    malformed.runs.front().length = 1;
    Reject([&] { layout.Prepare(factory.get(), malformed, options); }, "surrogate-splitting style rejected");
    Require(layout.Generation() == generation && layout.Caret(5).x > smallAdvance * 1.5f,
            "failed preparations retain matching previous projection and geometry");
    const auto source = document.Text();
    layout.Clear();
    Require(! layout.Ready() && layout.Height() == 0 && layout.Generation() == generation + 1, "clear retires layout and generation");
    Reject([&] { layout.Caret(0); }, "retired caret rejected");
    Reject([&] { layout.PreviousCluster(0); }, "retired navigation rejected");
    Require(document.Text() == source, "retiring prepared resources preserves document");
    layout.Prepare(factory.get(), projection, options);
    Require(layout.Ready() && layout.Content().revision == document.Revision(), "prepare resumes after retirement");
}

void TestRichTextLayoutBorrowedWarpTargetsAndDeviceReplacement()
{
    const auto factory = CreateRichTextTestFactory();
    Document document(L"# Shared layout\n**Unicode** \u03b2 \U0001f600");
    document.SetSelection({2, 2});
    document.ReplaceSelection(L"Owned ");
    const auto source    = document.Text();
    const auto selection = document.GetSelection();
    PreparedLayout layout;
    layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 420);
    const auto generation = layout.Generation();
    // All device/target ownership is in this caller fixture. The same prepared
    // DirectWrite state must work with a second unrelated WARP device/target.
    for (unsigned view = 0; view < 2; ++view)
    {
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
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0, 0));
        layout.Draw(context.get(), brush.get(), D2D1::Point2F(8, 8));
        THROW_IF_FAILED(context->EndDraw());
        std::vector<uint8_t> pixels;
        THROW_IF_FAILED(graphics.Read(pixels));
        std::size_t visible = 0;
        for (std::size_t i = 3; i < pixels.size(); i += 4)
            visible += pixels[i] != 0;
        Require(visible > 150, "actual borrowed-target text produces visible WARP pixels");
        Require(layout.Generation() == generation && document.Text() == source && document.GetSelection() == selection,
                "target replacement does not recreate layout or modify document");
    }
    Require(document.Undo() && document.Redo() && document.Text() == source, "history survives caller device retirement");
}

void TestRichTextSourceInkRendererMatchesCanonicalRaster()
{
    const auto factory = CreateRichTextTestFactory();
    Document document(L"alpha **beta** omega");
    const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Source);
    PreparedLayout layout;
    LayoutOptions options;
    options.widthDip       = 420.0f;
    options.fontSizeDip    = 20.0f;
    options.fontFamily     = L"Consolas";
    options.codeFontFamily = L"Consolas";
    layout.Prepare(factory.get(), projection, options);
    PreparedLayout shared(layout);

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
    wil::com_ptr<ID2D1SolidColorBrush> white, red, green;
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white.put()));
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), red.put()));
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green), green.put()));

    const std::array ink{TextInkRange{{0u, 5u}, TextInkRole::Accent}, TextInkRange{{6u, 14u}, TextInkRole::Link}};
    LinkDrawing drawing;
    drawing.brush                                                = red.get(); // RTE supplies its accent brush even when Source has no link runs.
    drawing.inkRanges                                            = ink;
    drawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    drawing.inkBrushes[static_cast<size_t>(TextInkRole::Link)]   = green.get();
    LinkDrawing legacy                                           = drawing;
    // A present, empty emphasis interval is inert in the legacy path and
    // deliberately opts out of the narrow source-only renderer.
    legacy.emphasis   = MarkdownRange{0u, 0u};
    const auto render = [&](const PreparedLayout& value, const LinkDrawing& presentation, D2D1_POINT_2F origin)
    {
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0u, 0.0f));
        value.Draw(context.get(), white.get(), origin, &presentation);
        THROW_IF_FAILED(context->EndDraw());
        std::vector<uint8_t> pixels;
        THROW_IF_FAILED(graphics.Read(pixels));
        return pixels;
    };

    // Exercise the exact renderer helper used by PreparedLayout::Draw so the
    // oracle proves admission, fallback, and colored output rather than merely
    // comparing two paths that might both have fallen back.
    Document bidiDocument(L"alpha \u05d0\u05d1\u05d2 **beta** omega");
    const auto bidiProjection = Project(bidiDocument.Snapshot(), bidiDocument.Revision(), ViewMode::Source);
    auto positioned           = Microsoft::WRL::Make<SourceInkTestPositioning>();
    Require(positioned.Get() != nullptr, "test positioning object is allocated");
    const auto makeTestLayout = [&](const Projection& value, const LayoutOptions& layoutOptions, bool withPositioning)
    {
        wil::com_ptr<IDWriteTextFormat> format;
        THROW_IF_FAILED(factory->CreateTextFormat(layoutOptions.fontFamily.c_str(),
                                                  nullptr,
                                                  DWRITE_FONT_WEIGHT_NORMAL,
                                                  DWRITE_FONT_STYLE_NORMAL,
                                                  DWRITE_FONT_STRETCH_NORMAL,
                                                  layoutOptions.fontSizeDip,
                                                  L"en-us",
                                                  format.put()));
        THROW_IF_FAILED(format->SetReadingDirection(layoutOptions.readingDirection));
        THROW_IF_FAILED(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
        THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
        std::wstring layoutText;
        if (withPositioning)
            layoutText.assign(L"\ufffc\n");
        layoutText.append(value.text);
        wil::com_ptr<IDWriteTextLayout> result;
        THROW_IF_FAILED(
            factory->CreateTextLayout(layoutText.data(), static_cast<UINT32>(layoutText.size()), format.get(), layoutOptions.widthDip, 10000.0f, result.put()));
        if (withPositioning)
        {
            THROW_IF_FAILED(result->SetFontSize(0.01f, {0u, 2u}));
            THROW_IF_FAILED(result->SetInlineObject(positioned.Get(), {0u, 1u}));
        }
        return result;
    };
    auto directOptions             = options;
    directOptions.fontFamily       = L"Consolas";
    directOptions.fontSizeDip      = 20.0f;
    directOptions.readingDirection = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    auto directDrawing             = drawing;
    directDrawing.displayOrigin    = 7u;
    const auto betaStart           = bidiProjection.text.find(L"**beta**");
    Require(betaStart != std::wstring::npos, "bidi source fixture includes the second colored segment");
    const auto tailStart = bidiProjection.text.size() - 2u;
    const std::array directInk{TextInkRange{{7u, 12u}, TextInkRole::Accent},
                               TextInkRange{{7u + betaStart, 7u + betaStart + 8u}, TextInkRole::Link},
                               TextInkRange{{7u + tailStart, 7u + bidiProjection.text.size() + 4u}, TextInkRole::Accent}};
    directDrawing.inkRanges                                            = directInk;
    directDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    directDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Link)]   = green.get();
    directDrawing.brush                                                = red.get();
    auto directLayout                                                  = makeTestLayout(bidiProjection, directOptions, true);
    context->SetDpi(120.0f, 120.0f);
    context->SetTransform(D2D1::Matrix3x2F(1.25f, 0.0f, 0.0f, 1.25f, 0.37f, 0.63f));
    const auto directOrigin = D2D1::Point2F(8.375f, 8.625f);
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0u, 0.0f));
    const auto directResult =
        Detail::DrawSourceInkIfSupported(context.get(), white.get(), directLayout.get(), positioned.Get(), 2u, bidiProjection, directDrawing, directOrigin);
    Require(directResult == Detail::SourceInkDrawResult::Drawn, "supported positioned Hebrew mixed-bidi source actually takes the direct renderer pass");
    THROW_IF_FAILED(context->EndDraw());
    std::vector<uint8_t> directPixels;
    THROW_IF_FAILED(graphics.Read(directPixels));
    std::size_t coloredPixels = 0u;
    for (std::size_t index = 0u; index + 3u < directPixels.size(); index += 4u)
        coloredPixels += directPixels[index + 3u] != 0u &&
                         (directPixels[index + 2u] > directPixels[index + 1u] + 30u || directPixels[index + 1u] > directPixels[index + 2u] + 30u);
    Require(coloredPixels != 0u, "direct source pass paints visible role-colored glyph pixels");

    auto canonicalLayout = makeTestLayout(bidiProjection, directOptions, true);
    const auto betaLocal = betaStart + 2u;
    THROW_IF_FAILED(canonicalLayout->SetDrawingEffect(red.get(), {2u, 5u}));
    THROW_IF_FAILED(canonicalLayout->SetDrawingEffect(green.get(), {static_cast<UINT32>(betaLocal), 8u}));
    THROW_IF_FAILED(canonicalLayout->SetDrawingEffect(red.get(), {static_cast<UINT32>(tailStart + 2u), 2u}));
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0u, 0.0f));
    context->DrawTextLayout(directOrigin, canonicalLayout.get(), white.get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    THROW_IF_FAILED(context->EndDraw());
    std::vector<uint8_t> canonicalPixels;
    THROW_IF_FAILED(graphics.Read(canonicalPixels));
    Require(directPixels == canonicalPixels, "direct subruns match canonical DirectWrite colored glyph pixels with prefix and nonzero display origin");

    const auto requirePreflightRejectsWithoutPainting = [&](const Projection& candidateProjection,
                                                            const LinkDrawing& candidateDrawing,
                                                            IDWriteTextLayout* candidateLayout,
                                                            IDWriteInlineObject* candidatePositioning,
                                                            UINT32 candidateTextOffset,
                                                            Detail::SourceInkDrawResult expected,
                                                            const char* message)
    {
        context->SetDpi(120.0f, 120.0f);
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0u, 0.0f));
        const auto result = Detail::DrawSourceInkIfSupported(context.get(),
                                                             white.get(),
                                                             candidateLayout,
                                                             candidatePositioning,
                                                             candidateTextOffset,
                                                             candidateProjection,
                                                             candidateDrawing,
                                                             D2D1::Point2F(8.0f, 8.0f));
        Require(result == expected, message);
        THROW_IF_FAILED(context->EndDraw());
        std::vector<uint8_t> pixels;
        THROW_IF_FAILED(graphics.Read(pixels));
        bool empty = true;
        for (std::size_t index = 3u; index < pixels.size(); index += 4u)
            empty = empty && pixels[index] == 0u;
        Require(empty, "unsupported source renderer conditions reject before painting any glyphs");
    };

    Document boundaryDocument(L"a\u0301 fi");
    const auto boundaryProjection = Project(boundaryDocument.Snapshot(), boundaryDocument.Revision(), ViewMode::Source);
    const std::array boundaryInk{TextInkRange{{0u, 1u}, TextInkRole::Accent}};
    LinkDrawing boundaryDrawing;
    boundaryDrawing.inkRanges                                            = boundaryInk;
    boundaryDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    auto boundaryLayout                                                  = makeTestLayout(boundaryProjection, directOptions, false);
    requirePreflightRejectsWithoutPainting(boundaryProjection,
                                           boundaryDrawing,
                                           boundaryLayout.get(),
                                           nullptr,
                                           0u,
                                           Detail::SourceInkDrawResult::Rejected,
                                           "combining-cluster range boundary rejects before painting");

    Document tooManySpansDocument(L"0123456789abcdefgh");
    const auto tooManySpansProjection = Project(tooManySpansDocument.Snapshot(), tooManySpansDocument.Revision(), ViewMode::Source);
    std::array<TextInkRange, 9> tooManyInk{};
    for (std::size_t index = 0u; index < tooManyInk.size(); ++index)
        tooManyInk[index] = {{index * 2u, index * 2u + 1u}, index % 2u ? TextInkRole::Link : TextInkRole::Accent};
    Require(tooManyInk.back().display.first < tooManySpansProjection.text.size() && tooManyInk.back().display.last <= tooManySpansProjection.text.size(),
            "all nine clipped ink spans intersect the admitted text");
    LinkDrawing tooManyDrawing;
    tooManyDrawing.inkRanges                                            = tooManyInk;
    tooManyDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    tooManyDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Link)]   = green.get();
    auto tooManyLayout                                                  = makeTestLayout(tooManySpansProjection, directOptions, false);
    requirePreflightRejectsWithoutPainting(tooManySpansProjection,
                                           tooManyDrawing,
                                           tooManyLayout.get(),
                                           nullptr,
                                           0u,
                                           Detail::SourceInkDrawResult::Rejected,
                                           "more than eight clipped ink spans reject before painting");

    auto selectionDrawing         = directDrawing;
    selectionDrawing.inkExclusion = MarkdownRange{8u, 12u};
    requirePreflightRejectsWithoutPainting(bidiProjection,
                                           selectionDrawing,
                                           directLayout.get(),
                                           positioned.Get(),
                                           2u,
                                           Detail::SourceInkDrawResult::NotApplicable,
                                           "selected source ink stays on the canonical renderer");

    Document colorDocument(L"plain \U0001f600 text");
    const auto colorProjection = Project(colorDocument.Snapshot(), colorDocument.Revision(), ViewMode::Source);
    const std::array colorInk{TextInkRange{{0u, 5u}, TextInkRole::Accent}};
    LinkDrawing colorDrawing;
    colorDrawing.inkRanges                                            = colorInk;
    colorDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    auto colorLayout                                                  = makeTestLayout(colorProjection, directOptions, false);
    requirePreflightRejectsWithoutPainting(
        colorProjection, colorDrawing, colorLayout.get(), nullptr, 0u, Detail::SourceInkDrawResult::Rejected, "color-font source falls back before painting");

    auto proportionalOptions       = directOptions;
    proportionalOptions.fontFamily = L"Segoe UI";
    Document proportionalDocument(L"plain proportional source");
    const auto proportionalProjection = Project(proportionalDocument.Snapshot(), proportionalDocument.Revision(), ViewMode::Source);
    auto proportionalLayout           = makeTestLayout(proportionalProjection, proportionalOptions, false);
    requirePreflightRejectsWithoutPainting(proportionalProjection,
                                           colorDrawing,
                                           proportionalLayout.get(),
                                           nullptr,
                                           0u,
                                           Detail::SourceInkDrawResult::Rejected,
                                           "proportional source font rejects before painting");

    Document overflowDocument(std::wstring(300u, L'a'));
    const auto overflowProjection = Project(overflowDocument.Snapshot(), overflowDocument.Revision(), ViewMode::Source);
    auto overflowOptions          = directOptions;
    overflowOptions.widthDip      = 10000.0f;
    auto overflowLayout           = makeTestLayout(overflowProjection, overflowOptions, false);
    const std::array overflowInk{TextInkRange{{0u, 4u}, TextInkRole::Accent}};
    LinkDrawing overflowDrawing;
    overflowDrawing.inkRanges                                            = overflowInk;
    overflowDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    requirePreflightRejectsWithoutPainting(overflowProjection,
                                           overflowDrawing,
                                           overflowLayout.get(),
                                           nullptr,
                                           0u,
                                           Detail::SourceInkDrawResult::Rejected,
                                           "more than 256 glyphs reject before painting");

    context->SetDpi(120.0f, 144.0f);
    context->BeginDraw();
    context->Clear(D2D1::ColorF(0u, 0.0f));
    const auto asymmetricDpiResult =
        Detail::DrawSourceInkIfSupported(context.get(), white.get(), directLayout.get(), positioned.Get(), 2u, bidiProjection, directDrawing, directOrigin);
    Require(asymmetricDpiResult == Detail::SourceInkDrawResult::Rejected, "nonuniform DPI rejects before fast drawing");
    THROW_IF_FAILED(context->EndDraw());
    std::vector<uint8_t> asymmetricDpiPixels;
    THROW_IF_FAILED(graphics.Read(asymmetricDpiPixels));
    bool asymmetricEmpty = true;
    for (std::size_t index = 3u; index < asymmetricDpiPixels.size(); index += 4u)
        asymmetricEmpty = asymmetricEmpty && asymmetricDpiPixels[index] == 0u;
    Require(asymmetricEmpty, "nonuniform DPI fallback leaves the fast renderer target untouched");

    for (const auto family : {L"Consolas", L"Cascadia Mono"})
    {
        options.fontFamily     = family;
        options.codeFontFamily = family;
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            options.readingDirection = direction;
            layout.Prepare(factory.get(), projection, options);
            shared = layout;
            for (const FLOAT dpi : {96.0f, 120.0f, 144.0f, 192.0f})
            {
                context->SetDpi(dpi, dpi);
                for (const auto transform : {D2D1::Matrix3x2F::Identity(), D2D1::Matrix3x2F(1.25f, 0.0f, 0.0f, 1.25f, 0.37f, 0.63f)})
                {
                    context->SetTransform(transform);
                    const auto fastPixels   = render(layout, drawing, D2D1::Point2F(8.375f, 8.625f));
                    const auto legacyPixels = render(layout, legacy, D2D1::Point2F(8.375f, 8.625f));
                    Require(fastPixels == legacyPixels,
                            "source glyph splitting matches canonical DirectWrite pixels across font, direction, DPI, transform and fractional origin");
                    Require(render(shared, drawing, D2D1::Point2F(8.375f, 8.625f)) == fastPixels,
                            "copied prepared layouts draw the same source ink without retained brushes or altered canonical pixels");
                }
            }
        }
    }

    const auto requireLegacyPixels = [&](Document& candidateDocument, LayoutOptions candidateOptions, MarkdownRange inkRange, bool selected)
    {
        auto candidateProjection = Project(candidateDocument.Snapshot(), candidateDocument.Revision(), ViewMode::Source);
        layout.Prepare(factory.get(), candidateProjection, candidateOptions);
        const std::array candidateInk{TextInkRange{inkRange, TextInkRole::Accent}};
        LinkDrawing candidateDrawing;
        candidateDrawing.inkRanges                                            = candidateInk;
        candidateDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
        if (selected)
            candidateDrawing.inkExclusion = inkRange;
        auto canonicalDrawing     = candidateDrawing;
        canonicalDrawing.emphasis = MarkdownRange{0u, 0u};
        Require(render(layout, candidateDrawing, D2D1::Point2F(8.375f, 8.625f)) == render(layout, canonicalDrawing, D2D1::Point2F(8.375f, 8.625f)),
                "unsupported source ink shape preserves the whole-layout canonical DirectWrite raster");
    };
    Document combining(L"a\u0301 fi alpha");
    requireLegacyPixels(combining, options, {0u, 1u}, false); // The ink boundary cuts one DirectWrite cluster.
    Document ligature(L"office fi");
    auto ligatureOptions       = options;
    ligatureOptions.fontFamily = L"Cascadia Code";
    requireLegacyPixels(ligature, ligatureOptions, {3u, 4u}, false);
    Document colorGlyph(L"plain \U0001f600 text");
    requireLegacyPixels(colorGlyph, options, {0u, 5u}, false);
    Document selectedSource(L"plain **selected** text");
    requireLegacyPixels(selectedSource, options, {6u, 14u}, true);
    auto proportional       = options;
    proportional.fontFamily = L"Segoe UI";
    Document proportionalSource(L"plain source text");
    requireLegacyPixels(proportionalSource, proportional, {0u, 5u}, false);
    Document overflow(std::wstring(300u, L'a'));
    auto wide     = options;
    wide.widthDip = 10000.0f;
    requireLegacyPixels(overflow, wide, {0u, 4u}, false);
    Document styledSource(L"plain styled source");
    auto styledProjection = Project(styledSource.Snapshot(), styledSource.Revision(), ViewMode::Source);
    styledProjection.runs.push_back({0u, 5u, Bold, 0u});
    layout.Prepare(factory.get(), styledProjection, options);
    const std::array styledInk{TextInkRange{{0u, 5u}, TextInkRole::Accent}};
    LinkDrawing styledDrawing;
    styledDrawing.inkRanges                                            = styledInk;
    styledDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    auto styledRawLayout                                               = makeTestLayout(styledProjection, options, false);
    requirePreflightRejectsWithoutPainting(styledProjection,
                                           styledDrawing,
                                           styledRawLayout.get(),
                                           nullptr,
                                           0u,
                                           Detail::SourceInkDrawResult::NotApplicable,
                                           "styled Source projection is not admitted to the direct renderer");
    auto styledLegacy     = styledDrawing;
    styledLegacy.emphasis = MarkdownRange{0u, 0u};
    Require(render(layout, styledDrawing, D2D1::Point2F(8.375f, 8.625f)) == render(layout, styledLegacy, D2D1::Point2F(8.375f, 8.625f)),
            "styled Source projections retain their canonical fallback raster");
    PreparedLayout styledCopy(layout);
    Require(render(styledCopy, styledLegacy, D2D1::Point2F(8.375f, 8.625f)) == render(layout, styledLegacy, D2D1::Point2F(8.375f, 8.625f)),
            "style changes keep shared copies on the canonical fallback raster");
}
} // namespace

void TestRichTextLayoutHostLocaleAndReadingDirection()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    Document document(L"abc");
    const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Source);
    PreparedLayout layout;
    LayoutOptions options;
    options.widthDip = 240;
    options.locale.clear(); // ControlHost uses DirectWrite's valid default locale.
    layout.Prepare(factory.get(), projection, options);
    const auto leadingX      = layout.Caret(0).x;
    options.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    layout.Prepare(factory.get(), projection, options);
    Require(layout.Caret(0).x > leadingX + 100, "host RTL direction changes real paragraph placement");
    const auto generation    = layout.Generation();
    options.readingDirection = static_cast<DWRITE_READING_DIRECTION>(99);
    Reject([&] { layout.Prepare(factory.get(), projection, options); }, "invalid reading direction rejected");
    Require(layout.Generation() == generation && layout.Ready(), "direction failure keeps prior prepared geometry");
}

void TestRichTextLayoutUnicode17NavigationAndHitStops()
{
    const auto factory = CreateRichTextTestFactory();
    for (const std::wstring cluster : {L"\u1100\u1161\u11a8", L"\u0915\u094d\u0915", L"\U0001f469\u200d\U0001f469\u200d\U0001f467", L"a\u200d"})
    {
        Document document(L"A" + cluster + L"B");
        PreparedLayout layout;
        layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), 600);
        const auto end = 1u + cluster.size();
        Require(layout.NextCluster(1) == end && layout.PreviousCluster(end) == 1,
                "prepared navigation uses complete Unicode graphemes independently of font glyph clusters");
        const auto first = layout.Caret(1), last = layout.Caret(end);
        for (size_t part = 0; part <= 12; ++part)
        {
            const auto hit = layout.HitTest(first.x + (last.x - first.x) * static_cast<float>(part) / 12.0f, first.y + first.height * 0.5f);
            Require(hit.position <= 1u || hit.position >= end, "mouse hit never stops inside a Unicode grapheme");
        }
        const auto scalarAcp = DxUi::StepToNextCodePoint(layout.Content().text, 1u);
        Require(layout.Caret(scalarAcp).height > 0 && ! layout.Range(1, end).empty(), "explicit scalar ACP geometry remains available to text services");
    }
}

void TestRichTextLayoutDeletionRangesAndRetirement()
{
    const auto factory = CreateRichTextTestFactory();
    PreparedLayout layout;
    Document document(L"Aa\u0301B");
    layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), 240);
    for (const bool backwards : {false, true})
        Require(layout.DeletionRange(2u, backwards) == std::pair<size_t, size_t>{1u, 3u},
                "prepared deletion encloses an interior ACP independent of direction");
    Require(layout.DeletionRange(1u, true) == std::pair<size_t, size_t>{0u, 1u} && layout.DeletionRange(3u, false) == std::pair<size_t, size_t>{3u, 4u},
            "exact boundary deletion uses its preceding or following complete element");
    Reject([&] { static_cast<void>(layout.DeletionRange(5u, true)); }, "prepared deletion validates its display offset");
    layout.Clear();
    Reject([&] { static_cast<void>(layout.DeletionRange(0u, false)); }, "retirement invalidates prepared deletion queries");
}
void TestRichTextLayoutTrailingEmptyLineBoundary()
{
    const auto factory = CreateRichTextTestFactory();
    for (const auto text : {L"", L"a\n", L"a\r\n", L"# Heading\n# "})
    {
        Document document(text);
        PreparedLayout layout;
        layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 240.0f);
        const auto& content = layout.Content();
        const auto end      = content.text.size();
        Require(layout.LineBoundary(end, false) == end && layout.LineBoundary(end, true) == end,
                "Home and End of a trailing empty display line stay at its caret, including hidden-only final source");
        const auto* retainedMember = &layout.Content();
        const auto* retainedText   = &layout.Content().text;
        layout.Clear();
        Require(&layout.Content() == retainedMember && &layout.Content().text == retainedText && retainedText->empty(),
                "retirement preserves projection member identity while releasing its active data");
        layout.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 240.0f);
        Require(&layout.Content() == retainedMember && &layout.Content().text == retainedText, "preparation preserves projection object reference identity");
    }
}

void TestRichTextLayoutCopyMoveOwnership()
{
    using namespace DxUi::RichText;
    const auto factory = CreateRichTextTestFactory();
    Document document(L"**owned** \U0001f600\nsecond");
    PreparedLayout original;
    original.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), 240.0f);
    const auto height = original.Height();
    PreparedLayout copied(original), assigned;
    assigned = copied;
    original.Clear();
    copied.Clear();
    Require(assigned.Ready() && assigned.Height() == height && assigned.Content().source == document.Text(),
            "copied layout owns independent source/index/retirement state and keeps immutable DirectWrite geometry alive");
    const auto* member = &assigned.Content();
    PreparedLayout moved(std::move(assigned));
    Require(! assigned.Ready() && assigned.Content().text.empty() && moved.Ready() && moved.Height() == height && &assigned.Content() == member,
            "move construction preserves member identity and leaves a safely retired source");
    PreparedLayout target;
    const auto* targetMember = &target.Content();
    target                   = std::move(moved);
    Require(target.Ready() && ! moved.Ready() && moved.Content().text.empty() && &target.Content() == targetMember,
            "move assignment transfers geometry without moving the destination projection member");
    auto* self = &target;
    target     = *self;
    target     = std::move(*self);
    Require(target.Ready() && target.Height() == height && target.Content().source == document.Text(), "self copy/move preserve the complete layout");
}
void TestPreparedFlowchartSdkBudgetAndLifetime()
{
    PreparedFlowchartTests::BudgetAndLifecycle(Require);
}
void TestPreparedFlowchartIndependentSdkRasterOracle()
{
    PreparedFlowchartTests::RasterOracle(Require);
}
void TestPreparedFlowchartBorrowedWarpTargets()
{
    PreparedFlowchartTests::BorrowedTargets(Require);
}
void TestPreparedFlowchartEdgeLabelsCannotBeObscured()
{
    PreparedFlowchartTests::EdgeLabelRasterOracle(Require);
}
void TestRichTextLayoutTransientLinkInkAndUnderlineRestoreSharedState()
{
    const auto factory = CreateRichTextTestFactory();
    Document document(L"link plain");
    auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
    projection.runs.clear();
    projection.runs.push_back({0u, 4u, Link, 0u});
    projection.runs.push_back({4u, 6u, Regular, 0u});
    PreparedLayout layout;
    layout.Prepare(factory.get(), projection, 420.0f, 24.0f);
    PreparedLayout shared(layout);
    const auto generation    = layout.Generation();
    const auto originalCaret = layout.Caret(4u);
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
    wil::com_ptr<ID2D1SolidColorBrush> white, red;
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), white.put()));
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), red.put()));
    const auto render = [&](const PreparedLayout& value, const LinkDrawing* drawing)
    {
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0u, 0.0f));
        value.Draw(context.get(), white.get(), D2D1::Point2F(8.0f, 8.0f), drawing);
        THROW_IF_FAILED(context->EndDraw());
        std::vector<uint8_t> pixels;
        THROW_IF_FAILED(graphics.Read(pixels));
        return pixels;
    };
    const auto persistent = render(layout, nullptr);
    LinkDrawing dormant;
    dormant.underline    = false;
    const auto unadorned = render(layout, &dormant);
    Require(persistent != unadorned, "native DirectWrite removes the actual link underline in dormant presentation");
    Require(render(shared, nullptr) == persistent, "drawing restores canonical styles on the SDK layout shared with copies");
    dormant.hover = MarkdownRange{0u, 4u};
    Require(render(layout, &dormant) == persistent, "hover draws the exact native persistent underline");
    dormant.hover.reset();
    dormant.caret = MarkdownRange{0u, 4u};
    Require(render(layout, &dormant) == persistent, "caret draws the same native underline without hand-painted geometry");
    dormant.caret.reset();
    dormant.brush = red.get();
    Require(render(layout, &dormant) != unadorned && render(shared, nullptr) == persistent,
            "link ink is borrowed only during synchronous rasterization and never leaks to another view");
    dormant.brush         = nullptr;
    dormant.emphasis      = MarkdownRange{5u, 10u};
    dormant.emphasisBrush = red.get();
    Require(render(layout, &dormant) != unadorned && render(shared, nullptr) == persistent,
            "active paragraph ink is borrowed and restored with no duplicate glyph overpainting");
    Require(layout.Generation() == generation && layout.Caret(4u).x == originalCaret.x && layout.Caret(4u).height == originalCaret.height,
            "decoration and emphasis preserve all prepared glyph geometry");
    projection.runs.front().flags |= Underline;
    layout.Prepare(factory.get(), projection, 420.0f, 24.0f);
    dormant.emphasis.reset();
    dormant.emphasisBrush = nullptr;
    Require(render(layout, &dormant) == render(layout, nullptr), "explicit underline survives hover-only link policy");
    const auto normal = render(layout, nullptr);
    const std::array inkRanges{TextInkRange{{5u, 10u}, TextInkRole::Accent}};
    dormant.inkRanges                                            = inkRanges;
    dormant.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    Require(render(layout, &dormant) != normal && render(layout, nullptr) == normal,
            "prepared source ink borrows brushes and restores shared DirectWrite drawing state");

    const auto plainProjection = Project(document.Snapshot(), document.Revision(), ViewMode::Source);
    Require(std::ranges::none_of(plainProjection.runs, [](const auto& run) { return (run.flags & Link) != 0u; }),
            "plain source fixture has no link drawing transaction");
    PreparedLayout plain;
    plain.Prepare(factory.get(), plainProjection, 420.0f, 24.0f);
    PreparedLayout plainShared(plain);
    const auto plainGeneration      = plain.Generation();
    const auto plainCaret           = plain.Caret(4u);
    const auto plainRange           = plain.Range(0u, plainProjection.text.size());
    const auto plainPixels          = render(plain, nullptr);
    const auto requirePlainGeometry = [&]
    {
        const auto caret = plain.Caret(4u);
        const auto range = plain.Range(0u, plainProjection.text.size());
        Require(plain.Generation() == plainGeneration && plain.Content().source == document.Text() && plainShared.Content().source == document.Text() &&
                    caret.x == plainCaret.x && caret.y == plainCaret.y && caret.height == plainCaret.height && range.size() == plainRange.size(),
                "plain decoration preserves source, shared copy and prepared caret geometry");
        for (size_t index = 0u; index < range.size(); ++index)
            Require(range[index].left == plainRange[index].left && range[index].top == plainRange[index].top && range[index].right == plainRange[index].right &&
                        range[index].bottom == plainRange[index].bottom,
                    "plain decoration preserves every source range rectangle");
    };
    const auto requirePlainPixels = [&](const LinkDrawing& drawing, const char* message)
    {
        Require(render(plain, &drawing) == plainPixels && render(plainShared, nullptr) == plainPixels, message);
        requirePlainGeometry();
    };
    LinkDrawing plainDrawing;
    plainDrawing.underline = false;
    requirePlainPixels(plainDrawing, "dormant link policy leaves plain paragraph pixels canonical");
    plainDrawing.brush = red.get();
    requirePlainPixels(plainDrawing, "accent link policy without link runs leaves plain paragraph pixels canonical");
    plainDrawing.emphasis      = MarkdownRange{plainProjection.text.size(), plainProjection.text.size() + 4u};
    plainDrawing.emphasisBrush = red.get();
    requirePlainPixels(plainDrawing, "nonintersecting emphasis leaves plain paragraph pixels canonical");
    plainDrawing.emphasis.reset();
    plainDrawing.emphasisBrush = nullptr;
    plainDrawing.inkRanges     = inkRanges;
    requirePlainPixels(plainDrawing, "prepared ink without an effective role brush leaves plain paragraph pixels canonical");
    plainDrawing.inkBrushes[static_cast<size_t>(TextInkRole::Accent)] = red.get();
    plainDrawing.inkExclusion                                         = MarkdownRange{5u, 10u};
    requirePlainPixels(plainDrawing, "fully excluded prepared ink leaves plain paragraph and shared-copy pixels canonical");
    plainDrawing.inkExclusion   = MarkdownRange{7u, 10u};
    const auto partialInkPixels = render(plain, &plainDrawing);
    const std::array partialInkRanges{TextInkRange{{5u, 7u}, TextInkRole::Accent}};
    plainDrawing.inkRanges = partialInkRanges;
    plainDrawing.inkExclusion.reset();
    Require(partialInkPixels != plainPixels && render(plainShared, &plainDrawing) == partialInkPixels && render(plainShared, nullptr) == plainPixels,
            "partially excluded valid ink changes only its surviving range and restores shared canonical pixels");
    requirePlainGeometry();
    plain.Clear();
    Require(! plain.Ready() && plainShared.Ready() && render(plainShared, nullptr) == plainPixels && plainShared.Content().source == document.Text(),
            "retiring the plain owner preserves the shared layout's restored pixels and source");
}
void TestRichTextSourceInkPreparationRolesAndCapacity()
{
    const auto parse = [](std::wstring_view source)
    {
        MarkdownParser parser;
        parser.Begin(source, 1u, MarkdownProfile::Gfm029);
        for (size_t turn = 0u; turn < 100000u; ++turn)
            if (parser.Advance().status == MarkdownPreparationStatus::Complete)
                return parser.Published();
        throw std::runtime_error("source ink syntax fixture did not complete");
    };
    const std::wstring source = L"# Heading\r\n\r\n- **strong** [label](https://example.invalid/a\\(b\\))\r\n";
    const auto syntax         = parse(source);
    Detail::RichSourceInk ink;
    size_t turns = 0u;
    while (! ink.Advance(syntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn, "source ink shares one bounded preparation budget");
        ++turns;
        Require(ink.Ranges().empty() && turns < 1000u, "staged source ink never publishes partial ranges");
    }
    Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn, "final source ink turn stays within its work budget");
    const auto roleAt = [&](size_t offset) -> std::optional<TextInkRole>
    {
        for (const auto& range : ink.Ranges())
            if (range.display.first <= offset && offset < range.display.last)
                return range.role;
        return std::nullopt;
    };
    Require(roleAt(0u) == TextInkRole::Accent, "ATX heading marker receives accent ink");
    Require(roleAt(source.find(L"- ")) == TextInkRole::Accent, "list marker receives accent ink");
    Require(roleAt(source.find(L"**")) == TextInkRole::Subtle, "strong delimiter receives subtle ink");
    Require(roleAt(source.find(L"https:")) == TextInkRole::Link && roleAt(source.find(L"\\(")) == TextInkRole::Link,
            "escaped explicit-link destination receives exact source link ink");
    Require(! roleAt(source.find(L"Heading")) && ! roleAt(source.find(L"strong")), "ordinary heading and strong content retain their normal ink");
    size_t previous = 0u;
    for (const auto& range : ink.Ranges())
    {
        Require(previous <= range.display.first && range.display.first < range.display.last && range.display.last <= source.size(),
                "published source ink is bounded, sorted and disjoint");
        previous = range.display.last;
    }

    const std::wstring listFixture = L"- outer\n  - nested\n-\t- tab nested\n12. ordered\n> - quoted\n- [x] task\n";
    const auto listSyntax          = parse(listFixture);
    const std::array<std::wstring_view, 7> markers{L"-", L"-", L"-", L"-", L"12.", L"-", L"-"};
    const std::array<size_t, 7> markerOffsets{listFixture.find(L"- outer"),
                                              listFixture.find(L"- nested"),
                                              listFixture.find(L"-\t- tab nested"),
                                              listFixture.find(L"- tab nested"),
                                              listFixture.find(L"12."),
                                              listFixture.find(L"- quoted"),
                                              listFixture.find(L"- [x]")};
    for (size_t index = 0u; index < markers.size(); ++index)
    {
        Require(markerOffsets[index] != std::wstring::npos, "list marker fixture offset exists");
        const auto found = std::ranges::find_if(listSyntax->Nodes(),
                                                [&](const auto& node)
        {
            return node.block.kind == MarkdownBlockKind::ListItem && node.block.info.first == markerOffsets[index] &&
                   node.block.info.last == markerOffsets[index] + markers[index].size() &&
                   listFixture.substr(node.block.info.first, node.block.info.last - node.block.info.first) == markers[index];
        });
        Require(found != listSyntax->Nodes().end(), "list parser publishes exact source ranges for nested, ordered, quote, tab and task markers");
    }
    turns = 0u;
    while (! ink.Advance(listSyntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty(),
                "list marker preparation remains bounded and unpublished while pending");
        ++turns;
    }
    for (const auto offset : markerOffsets)
    {
        const auto found = std::ranges::any_of(
            ink.Ranges(), [&](const auto& range) { return range.role == TextInkRole::Accent && range.display.first <= offset && offset < range.display.last; });
        Require(found, "each parsed list marker receives accent ink at its exact source offset");
    }

    std::wstring longCode(700u, L'`');
    longCode += L"x";
    longCode.append(700u, L'`');
    const auto longCodeSyntax = parse(longCode);
    turns                     = 0u;
    while (! ink.Advance(longCodeSyntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty(),
                "long inline delimiter scan yields without partial ranges or budget overflow");
        ++turns;
    }
    const auto codeRoleAt = [&](size_t offset)
    {
        return std::ranges::any_of(
            ink.Ranges(), [&](const auto& range) { return range.role == TextInkRole::Subtle && range.display.first <= offset && offset < range.display.last; });
    };
    Require(turns >= 2u && ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && codeRoleAt(0u) && codeRoleAt(699u) && ! codeRoleAt(700u),
            "long code delimiter runs are colored exactly after resumable scanning");

    std::wstring manyChildren = L"[";
    for (size_t i = 0u; i < 300u; ++i)
        manyChildren.append(L"**x** ");
    manyChildren.append(L"](https://example.invalid/");
    manyChildren.append(1400u, L'a');
    manyChildren.append(L")");
    const auto manyChildrenSyntax = parse(manyChildren);
    turns                         = 0u;
    while (! ink.Advance(manyChildrenSyntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty(), "link label and destination scans share the bounded turn budget");
        ++turns;
    }
    const auto linkedRoleAt = [&](size_t offset)
    {
        return std::ranges::any_of(
            ink.Ranges(), [&](const auto& range) { return range.role == TextInkRole::Link && range.display.first <= offset && offset < range.display.last; });
    };
    const auto destination = manyChildren.find(L"https:");
    Require(turns >= 3u && ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && linkedRoleAt(destination) && linkedRoleAt(manyChildren.find_last_of(L'a')),
            "many-child link and long destination publish exact link ink after yielding");

    std::wstring mergeFixture;
    for (size_t i = 0u; i < 6000u; ++i)
        mergeFixture.append(L"```\nx\n```\n");
    const auto mergeSyntax = parse(mergeFixture);
    turns                  = 0u;
    while (! ink.Advance(mergeSyntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty(),
                "incremental event merge does not publish partial ranges or exceed its per-turn budget");
        ++turns;
    }
    Require(turns > 200u && ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ! ink.Ranges().empty(),
            "multiple merge passes complete incrementally for a dense admitted source");

    std::wstring crowded;
    for (size_t i = 0u; i < 10000u; ++i)
        crowded.append(L"# `x`\n");
    Require(crowded.size() <= Document::MaximumUnits, "source ink overflow fixture stays within the admitted Markdown source limit");
    const auto crowdedSyntax = parse(crowded);
    turns                    = 0u;
    while (! ink.Advance(crowdedSyntax, [](size_t value) { return value; }))
    {
        Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty(), "capacity-bound source ink yields without partial publication");
        Require(++turns < 10000u, "capacity fallback remains bounded");
    }
    Require(ink.LastWork() <= Detail::RichSourceInk::WorkPerTurn && ink.Ranges().empty() && ! ink.Pending(),
            "source ink capacity overflow falls back to the complete ordinary view");
    Require(ink.Advance(crowdedSyntax, [](size_t value) { return value; }) && ink.Ranges().empty(), "capacity rejection coalesces for exact syntax identity");
    ink.Clear();
    Require(! ink.Pending() && ink.Ranges().empty(), "source retirement cancels staged and published ink");
}
void TestRichTextLayoutExplicitTypographyAndInvalidOptions()
{
    const auto factory = CreateRichTextTestFactory();
    Document document(L"# Heading\nbody `code`");
    const auto projection = Project(document.Snapshot(), document.Revision(), ViewMode::Rich);
    PreparedLayout layout;
    LayoutOptions options;
    options.widthDip              = 500.0f;
    options.fontSizeDip           = 16.0f;
    options.headingFontSizeDip[0] = 34.0f;
    options.codeFontSizeDip       = 13.5f;
    options.lineHeightMultiplier  = 1.65f;
    layout.Prepare(factory.get(), projection, options);
    const auto generation        = layout.Generation();
    const auto height            = layout.Height();
    auto invalid                 = options;
    invalid.lineHeightMultiplier = std::numeric_limits<float>::infinity();
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "nonfinite line spacing rejected");
    invalid                       = options;
    invalid.headingFontSizeDip[4] = -1.0f;
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "invalid explicit heading size rejected");
    invalid                                = options;
    invalid.headingLineHeightMultiplier[0] = 0.5f;
    Reject([&] { layout.Prepare(factory.get(), projection, invalid); }, "clipping-prone heading line spacing rejected");
    Require(layout.Generation() == generation && layout.Height() == height, "invalid typography retains prior source/geometry publication");
}
void RunRichTextLayoutTests()
{
    DXUI_RUN_TEST(TestRichTextLayoutTransientLinkInkAndUnderlineRestoreSharedState);
    DXUI_RUN_TEST(TestRichTextLayoutExplicitTypographyAndInvalidOptions);
    DXUI_RUN_TEST(TestRichTextSourceInkPreparationRolesAndCapacity);
    DXUI_RUN_TEST(TestPreparedFlowchartSdkBudgetAndLifetime);
    DXUI_RUN_TEST(TestPreparedFlowchartIndependentSdkRasterOracle);
    DXUI_RUN_TEST(TestPreparedFlowchartBorrowedWarpTargets);
    DXUI_RUN_TEST(TestPreparedFlowchartEdgeLabelsCannotBeObscured);
    DXUI_RUN_TEST(TestRichTextLayoutCopyMoveOwnership);
    DXUI_RUN_TEST(TestRichTextLayoutTrailingEmptyLineBoundary);
    DXUI_RUN_TEST(TestRichTextLayoutPrototypeContracts);
    DXUI_RUN_TEST(TestRichTextLayoutOptionsFailureAtomicityAndRetirement);
    DXUI_RUN_TEST(TestRichTextLayoutBorrowedWarpTargetsAndDeviceReplacement);
    DXUI_RUN_TEST(TestRichTextSourceInkRendererMatchesCanonicalRaster);
    DXUI_RUN_TEST(TestRichTextLayoutHostLocaleAndReadingDirection);
    DXUI_RUN_TEST(TestRichTextLayoutUnicode17NavigationAndHitStops);
    DXUI_RUN_TEST(TestRichTextLayoutDeletionRangesAndRetirement);
}
