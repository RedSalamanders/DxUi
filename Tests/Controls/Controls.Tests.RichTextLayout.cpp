#include "Controls.Tests.RichTextLayout.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../Support/Support.Tests.PreparedFlowchart.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichTextLayout.h>
#include <cmath>
#include <limits>
#include <wil/result.h>
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
void RunRichTextLayoutTests()
{
    DXUI_RUN_TEST(TestPreparedFlowchartSdkBudgetAndLifetime);
    DXUI_RUN_TEST(TestPreparedFlowchartIndependentSdkRasterOracle);
    DXUI_RUN_TEST(TestPreparedFlowchartBorrowedWarpTargets);
    DXUI_RUN_TEST(TestPreparedFlowchartEdgeLabelsCannotBeObscured);
    DXUI_RUN_TEST(TestRichTextLayoutCopyMoveOwnership);
    DXUI_RUN_TEST(TestRichTextLayoutTrailingEmptyLineBoundary);
    DXUI_RUN_TEST(TestRichTextLayoutPrototypeContracts);
    DXUI_RUN_TEST(TestRichTextLayoutOptionsFailureAtomicityAndRetirement);
    DXUI_RUN_TEST(TestRichTextLayoutBorrowedWarpTargetsAndDeviceReplacement);
    DXUI_RUN_TEST(TestRichTextLayoutHostLocaleAndReadingDirection);
    DXUI_RUN_TEST(TestRichTextLayoutUnicode17NavigationAndHitStops);
    DXUI_RUN_TEST(TestRichTextLayoutDeletionRangesAndRetirement);
}
