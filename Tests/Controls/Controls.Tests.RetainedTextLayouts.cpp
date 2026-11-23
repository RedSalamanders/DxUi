#include "Controls.Tests.RetainedTextLayouts.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support/WorkCounters.h"

#include "GraphicsFixture.h"

#include <array>
#include <cmath>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace DxUi;

namespace
{
// ---- The equivalence matrix ------------------------------------------------------------------------------------------
// Every case draws one string into one box of one cell, as its callers do: DrawCenteredText (kTextDrawOptions, the format
// of a role, alignment, paragraph alignment, wrapping and reading direction), the host text of a control that draws with
// D2D1_DRAW_TEXT_OPTIONS_CLIP alone (ThroughputGraph's labels), and DrawCenteredText under a rotation (a disclosure chevron
// in motion). Each cell clips to itself, so a case that overflows its box cannot reach another cell.
enum class DrawMode : uint8_t
{
    Centered,
    ClipOnly,
    Rotated,
};

struct TextCase
{
    std::wstring_view text;
    FontRole role                        = FontRole::Body;
    DWRITE_TEXT_ALIGNMENT alignment      = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT paragraph = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    bool wrap                            = false;
    FlowDirection flow                   = FlowDirection::LeftToRight;
    D2D1_RECT_F box                      = {}; // relative to the cell origin
    DrawMode mode                        = DrawMode::Centered;
};

constexpr float kCellWidthDip   = 112.0f;
constexpr float kCellHeightDip  = 48.0f;
constexpr size_t kColumns       = 10u;
constexpr size_t kRows          = 16u;
constexpr size_t kCellsPerFrame = kColumns * kRows;

// The strings: Latin text, an empty string, a line wider than every box, a paragraph that wraps, tabs, ampersands (DirectWrite
// has no prefix processing: they draw as written), colour emoji with a skin-tone modifier and a variation selector, CJK and
// Hangul, combining marks, Arabic with digits, hard line breaks taller than most boxes, and an icon-font glyph.
constexpr std::array<std::wstring_view, 12u> kTexts{
    L"Hamburgefonstiv",
    L"",
    L"The quick brown fox jumps over the lazy dog and keeps running past the edge",
    L"Wrapped words that need several lines to fit in a narrow box",
    L"Tab\tseparated\tcolumns",
    L"&File &Edit &&View",
    L"\U0001F600 \U0001F44D\U0001F3FD \u2764\uFE0F",
    L"\u6F22\u5B57\u304B\u306A\u30AB\u30CA \uD55C\uAD6D\uC5B4",
    L"e\u0301a\u0300o\u0308 Z\u0335\u0336 n\u0303",
    L"\u0645\u0631\u062D\u0628\u0627 \u0628\u0627\u0644\u0639\u0627\u0644\u0645 123",
    L"First line\r\nSecond line\nThird",
    L"\uE70D",
};

// The boxes, relative to the cell: whole DIPs, a fractional origin, a fractional size, a box narrower and one shorter than
// its text, zero width, zero height and a negative width.
constexpr std::array<D2D1_RECT_F, 8u> kBoxes{
    D2D1_RECT_F{4.0f, 4.0f, 104.0f, 30.0f},
    D2D1_RECT_F{4.3f, 5.7f, 104.3f, 31.7f},
    D2D1_RECT_F{4.0f, 4.0f, 94.45f, 24.6f},
    D2D1_RECT_F{2.5f, 2.25f, 40.75f, 20.125f},
    D2D1_RECT_F{4.0f, 4.0f, 108.0f, 10.5f},
    D2D1_RECT_F{4.0f, 4.0f, 4.0f, 30.0f},
    D2D1_RECT_F{4.0f, 4.0f, 100.0f, 4.0f},
    D2D1_RECT_F{60.0f, 4.0f, 20.0f, 30.0f},
};

constexpr std::array kAlignments{DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_TRAILING};
constexpr std::array kParagraphs{DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR};

// Boxes that share their width or their height with kBoxes[0], drawn first, in one frame, with the same text and format:
// a key that ignored the height or the width would draw one box's layout in another, which wraps or places the text
// differently, and the oracle would see it.
constexpr std::array<D2D1_RECT_F, 3u> kKeyPairBoxes{
    D2D1_RECT_F{4.0f, 4.0f, 104.0f, 30.0f}, // as kBoxes[0]
    D2D1_RECT_F{4.0f, 4.0f, 104.0f, 20.0f}, // same width, another height
    D2D1_RECT_F{4.0f, 4.0f, 64.0f, 30.0f},  // same height, another width
};
constexpr size_t kKeyPairCases = 2u * 3u * 3u * kKeyPairBoxes.size();

std::vector<TextCase> BuildCases()
{
    std::vector<TextCase> cases;
    for (const bool wrap : {true, false})
        for (const DWRITE_TEXT_ALIGNMENT alignment : kAlignments)
            for (const DWRITE_PARAGRAPH_ALIGNMENT paragraph : kParagraphs)
                for (const D2D1_RECT_F& box : kKeyPairBoxes)
                {
                    TextCase value;
                    value.text      = wrap ? kTexts[3] : kTexts[2]; // a paragraph that wraps; a line wider than every box
                    value.alignment = alignment;
                    value.paragraph = paragraph;
                    value.wrap      = wrap;
                    value.box       = box;
                    cases.push_back(value);
                }
    size_t index = 0u;
    for (size_t text = 0u; text < kTexts.size(); ++text)
        for (size_t box = 0u; box < kBoxes.size(); ++box)
            for (size_t layout = 0u; layout < 4u; ++layout)
            {
                // Half of the wrapping and reading-direction pairs per string and box, alternating, so each pair meets every
                // string and every box.
                if ((text + box + layout) % 2u != 0u)
                    continue;
                for (const DWRITE_TEXT_ALIGNMENT alignment : kAlignments)
                    for (const DWRITE_PARAGRAPH_ALIGNMENT paragraph : kParagraphs)
                    {
                        TextCase value;
                        value.text      = kTexts[text];
                        value.role      = text == kTexts.size() - 1u ? FontRole::Icon : (text % 3u == 1u ? FontRole::Small : FontRole::Body);
                        value.alignment = alignment;
                        value.paragraph = paragraph;
                        value.wrap      = (layout & 1u) != 0u;
                        value.flow      = (layout & 2u) != 0u ? FlowDirection::RightToLeft : FlowDirection::LeftToRight;
                        value.box       = kBoxes[box];
                        value.mode      = index % 7u == 3u ? DrawMode::ClipOnly : (index % 11u == 5u ? DrawMode::Rotated : DrawMode::Centered);
                        cases.push_back(value);
                        ++index;
                    }
            }
    return cases;
}

// Paints cases [first, first + kCellsPerFrame) of the matrix, one per cell.
class TextCaseSheet final : public Control
{
public:
    TextCaseSheet()                                = default;
    TextCaseSheet(const TextCaseSheet&)            = delete;
    TextCaseSheet& operator=(const TextCaseSheet&) = delete;
    TextCaseSheet(TextCaseSheet&&)                 = delete;
    TextCaseSheet& operator=(TextCaseSheet&&)      = delete;
    ~TextCaseSheet() override                      = default;

    std::span<const TextCase> cases;

    void Paint(ControlHost& host) const override
    {
        ID2D1DeviceContext* const dc = host.GetDeviceContext();
        if (! dc)
            return;
        const D2D1_COLOR_F ink = host.GetTheme().text;
        for (size_t index = 0u; index < cases.size() && index < kCellsPerFrame; ++index)
        {
            const TextCase& value  = cases[index];
            const float left       = static_cast<float>(index % kColumns) * kCellWidthDip;
            const float top        = static_cast<float>(index / kColumns) * kCellHeightDip;
            const D2D1_RECT_F cell = D2D1::RectF(left, top, left + kCellWidthDip, top + kCellHeightDip);
            const D2D1_RECT_F rect = D2D1::RectF(left + value.box.left, top + value.box.top, left + value.box.right, top + value.box.bottom);
            dc->PushAxisAlignedClip(cell, D2D1_ANTIALIAS_MODE_ALIASED);
            switch (value.mode)
            {
                case DrawMode::Centered:
                    DrawCenteredText(host, value.text, rect, value.role, ink, value.alignment, value.paragraph, value.wrap, value.flow);
                    break;
                case DrawMode::ClipOnly:
                    if (auto* format = host.GetTextFormat(value.role, value.alignment, value.paragraph, value.wrap, ResolveReadingDirection(value.flow)))
                        if (auto* brush = host.GetSolidBrush(ink))
                            DrawHostText(host, dc, value.text, format, rect, brush, D2D1_DRAW_TEXT_OPTIONS_CLIP);
                    break;
                case DrawMode::Rotated:
                {
                    D2D1_MATRIX_3X2_F previous{};
                    dc->GetTransform(&previous);
                    dc->SetTransform(D2D1::Matrix3x2F::Rotation(30.0f, D2D1::Point2F(left + kCellWidthDip * 0.5f, top + kCellHeightDip * 0.5f)) * previous);
                    DrawCenteredText(host, value.text, rect, value.role, ink, value.alignment, value.paragraph, value.wrap, value.flow);
                    dc->SetTransform(previous);
                    break;
                }
            }
            dc->PopAxisAlignedClip();
        }
    }
};

std::string DescribeCase(const TextCase& value)
{
    std::string text;
    for (const wchar_t unit : value.text)
        text += unit >= 0x20 && unit < 0x7F ? static_cast<char>(unit) : '?';
    return std::format("text \"{}\" role {} alignment {} paragraph {} wrap {} rtl {} box ({},{},{},{}) mode {}",
                       text,
                       static_cast<int>(value.role),
                       static_cast<int>(value.alignment),
                       static_cast<int>(value.paragraph),
                       value.wrap,
                       value.flow == FlowDirection::RightToLeft,
                       value.box.left,
                       value.box.top,
                       value.box.right,
                       value.box.bottom,
                       static_cast<int>(value.mode));
}

// The first case whose cell differs between two frames of the same cases, or none.
std::optional<size_t> FirstDifferingCase(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, UINT width, float scale, size_t cases)
{
    for (size_t index = 0u; index < cases; ++index)
    {
        const auto x0 = static_cast<size_t>(std::floor(static_cast<float>(index % kColumns) * kCellWidthDip * scale));
        const auto y0 = static_cast<size_t>(std::floor(static_cast<float>(index / kColumns) * kCellHeightDip * scale));
        const auto x1 = static_cast<size_t>(std::ceil(static_cast<float>(index % kColumns + 1u) * kCellWidthDip * scale));
        const auto y1 = static_cast<size_t>(std::ceil(static_cast<float>(index / kColumns + 1u) * kCellHeightDip * scale));
        for (size_t y = y0; y < y1; ++y)
            for (size_t x = x0; x < (std::min)(x1, static_cast<size_t>(width)); ++x)
            {
                const size_t offset = (y * width + x) * 4u;
                if (offset + 4u <= a.size() && std::memcmp(a.data() + offset, b.data() + offset, 4u) != 0)
                    return index;
            }
    }
    return std::nullopt;
}

size_t CountDifferingPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) noexcept
{
    size_t count = 0u;
    for (size_t offset = 0u; offset + 4u <= a.size() && offset + 4u <= b.size(); offset += 4u)
        if (std::memcmp(a.data() + offset, b.data() + offset, 4u) != 0)
            ++count;
    return count;
}

struct MatrixTotals
{
    size_t cases       = 0u;
    size_t frames      = 0u;
    uint64_t created   = 0u; // layouts the table created
    uint64_t hits      = 0u;
    uint64_t bypass    = 0u;
    size_t formatTrims = 0u; // retained frames that began by trimming the host's format cache
};

// Draws `cases` on an embedded view at `dpi` three times per frame of cells: through DrawText (the seam), through the table
// on a frame whose strings are all new to it (every layout created), and again (every layout retained); all three must be
// byte-identical.
void RunMatrixOnEmbedded(std::span<const TextCase> cases, float dpi, MatrixTotals& totals)
{
    const float scale = dpi / 96.0f;
    const auto width  = static_cast<UINT>(std::ceil(kCellWidthDip * static_cast<float>(kColumns) * scale));
    const auto height = static_cast<UINT>(std::ceil(kCellHeightDip * static_cast<float>(kRows) * scale));
    GraphicsFixture gpu;
    gpu.width  = width;
    gpu.height = height;
    RequireSucceeded(gpu.Create(), "equivalence matrix WARP device");
    std::shared_ptr<GraphicsDevice> graphics;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "equivalence matrix graphics device");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(std::move(graphics), StoppedTestClock()), "equivalence matrix view attaches");
    auto theme          = MakeDefaultThemePalette(true);
    theme.reducedMotion = true;
    view.Controls().SetTheme(theme);
    auto sheetOwner      = std::make_unique<TextCaseSheet>();
    TextCaseSheet& sheet = *sheetOwner;
    view.Controls().SetRoot(std::move(sheetOwner));
    ControlHost& host = view.Controls();
    const auto frame  = [&](bool retained, Detail::WorkCounters& work)
    {
        host.DebugSetTextLayoutCacheEnabledForTest(retained);
        view.MarkDirty();
        const Detail::WorkCounterScope scope;
        RequireSucceeded(view.Prepare(width, height, dpi), "equivalence matrix preparation");
        work = scope.Delta();
        gpu.Bind();
        RequireSucceeded(view.Composite(gpu.context.get(), gpu.Viewport()), "equivalence matrix composition");
        std::vector<uint8_t> pixels;
        RequireSucceeded(gpu.Read(pixels), "equivalence matrix readback");
        return pixels;
    };
    for (size_t first = 0u; first < cases.size(); first += kCellsPerFrame)
    {
        const size_t count = (std::min)(kCellsPerFrame, cases.size() - first);
        sheet.cases        = cases.subspan(first, count);
        Detail::WorkCounters drawText{}, created{}, retained{};
        const std::vector<uint8_t> oracle = frame(false, drawText);
        Require(drawText.text.textLayoutCacheHits == 0u && drawText.text.textLayoutCacheMisses == 0u && drawText.resources.textLayout.created == 0u,
                "with the seam off every string is drawn through DrawText");
        const std::vector<uint8_t> fresh = frame(true, created);
        const std::vector<uint8_t> again = frame(true, retained);
        for (const auto& [name, candidate] : {std::pair{"created", &fresh}, std::pair{"retained", &again}})
        {
            if (*candidate == oracle)
                continue;
            const std::optional<size_t> differing = FirstDifferingCase(oracle, *candidate, width, scale, count);
            std::cerr << std::format("    [W2.1] {} DPI, {} layouts: {} pixels differ; first differing case {}: {}\n",
                                     dpi,
                                     name,
                                     CountDifferingPixels(oracle, *candidate),
                                     differing ? first + *differing : 0u,
                                     differing ? DescribeCase(cases[first + *differing]) : std::string("(outside every cell)"));
            Require(false, "a layout sized to the box and drawn at its origin draws exactly what DrawText draws");
        }
        Require(retained.resources.implicitTextLayout.created == retained.text.textLayoutCacheBypasses + retained.text.textLayoutCacheOverflows,
                "only bypassed strings still lay out inside DrawText");
        Require(retained.text.textLayoutCacheHits + retained.text.textLayoutCacheMisses ==
                    created.text.textLayoutCacheHits + created.text.textLayoutCacheMisses,
                "the table serves the same strings on both frames");
        // The matrix uses 108 format keys, more than the host's format cache keeps (96): a frame that starts by trimming it
        // gets new format objects, and a key holds its format, so those strings are laid out again (W2.8 bounds that cache).
        if (retained.text.formatCacheTrims == 0u && retained.text.textLayoutCacheMisses != 0u)
            std::cerr << std::format("    [W2.1] {} DPI frame {}: retained misses {} hits {}; created misses {} hits {} trims {}; entries {} capacity {}\n",
                                     dpi,
                                     first / kCellsPerFrame,
                                     retained.text.textLayoutCacheMisses,
                                     retained.text.textLayoutCacheHits,
                                     created.text.textLayoutCacheMisses,
                                     created.text.textLayoutCacheHits,
                                     created.text.formatCacheTrims,
                                     host.DebugGetTextLayoutCacheEntries(),
                                     host.DebugGetTextLayoutCacheCapacity());
        if (retained.text.formatCacheTrims == 0u)
            Require(retained.resources.textLayout.created == 0u && retained.text.textLayoutCacheMisses == 0u,
                    "a frame of the same strings and formats creates no layout");
        else
            ++totals.formatTrims;
        totals.created += created.text.textLayoutCacheMisses;
        totals.hits += retained.text.textLayoutCacheHits;
        totals.bypass += retained.text.textLayoutCacheBypasses;
        totals.cases += count;
        ++totals.frames;
    }
}

// The same matrix on a native window at the system DPI: the opaque chain draws ClearType text where the embedded surface
// draws grayscale.
void RunMatrixOnWindow(std::span<const TextCase> cases, MatrixTotals& totals)
{
    std::jthread fixture([&]
    {
        const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        WindowHost& host    = window.Host();
        auto theme          = MakeDefaultThemePalette(false);
        theme.reducedMotion = true;
        host.SetTheme(theme);
        auto sheetOwner      = std::make_unique<TextCaseSheet>();
        TextCaseSheet& sheet = *sheetOwner;
        host.SetRoot(std::move(sheetOwner));
        const float scale = static_cast<float>(GetDpiForWindow(window.Hwnd())) / 96.0f;
        RECT rect{0,
                  0,
                  static_cast<LONG>(std::ceil(kCellWidthDip * static_cast<float>(kColumns) * scale)),
                  static_cast<LONG>(std::ceil(kCellHeightDip * static_cast<float>(kRows) * scale))};
        const DWORD style   = static_cast<DWORD>(GetWindowLongPtrW(window.Hwnd(), GWL_STYLE));
        const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE));
        Require(AdjustWindowRectEx(&rect, style, FALSE, exStyle) != FALSE, "equivalence window rect adjusts");
        const SIZE outer{rect.right - rect.left, rect.bottom - rect.top};
        window.AllowOuterSizeBeyondDesktop(outer);
        Require(SetWindowPos(window.Hwnd(), nullptr, -32000, -32000, outer.cx, outer.cy, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "equivalence window resizes");
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        const auto capture = [&](bool retained)
        {
            host.DebugSetTextLayoutCacheEnabledForTest(retained);
            WindowHostBitmapCapture bitmap;
            Require(host.DebugCaptureBitmap(bitmap), "the equivalence window renders and captures a frame");
            return bitmap;
        };
        for (size_t first = 0u; first < cases.size(); first += kCellsPerFrame)
        {
            const size_t count                    = (std::min)(kCellsPerFrame, cases.size() - first);
            sheet.cases                           = cases.subspan(first, count);
            const WindowHostBitmapCapture oracle  = capture(false);
            const WindowHostBitmapCapture created = capture(true);
            const WindowHostBitmapCapture again   = capture(true);
            for (const WindowHostBitmapCapture* candidate : {&created, &again})
            {
                if (candidate->bgraPixels == oracle.bgraPixels)
                    continue;
                const std::optional<size_t> differing = FirstDifferingCase(oracle.bgraPixels, candidate->bgraPixels, oracle.widthPx, scale, count);
                std::cerr << std::format("    [W2.1] window ({} DPI): {} pixels differ; first differing case {}: {}\n",
                                         96.0f * scale,
                                         CountDifferingPixels(oracle.bgraPixels, candidate->bgraPixels),
                                         differing ? first + *differing : 0u,
                                         differing ? DescribeCase(cases[first + *differing]) : std::string("(outside every cell)"));
                Require(false, "on a native window too, a retained layout draws exactly what DrawText draws");
            }
            totals.cases += count;
            ++totals.frames;
        }
    });
}
} // namespace

void TestRetainedTextLayoutsDrawWhatDrawTextDraws()
{
    const std::vector<TextCase> cases = BuildCases();
    size_t clipOnly = 0u, rotated = 0u;
    for (const TextCase& value : cases)
    {
        clipOnly += value.mode == DrawMode::ClipOnly ? 1u : 0u;
        rotated += value.mode == DrawMode::Rotated ? 1u : 0u;
    }
    Require(cases.size() == kKeyPairCases + kTexts.size() * kBoxes.size() * 2u * 9u && kKeyPairCases <= kCellsPerFrame && clipOnly > 0u && rotated > 0u,
            "the matrix covers every string and box with half of the wrapping and direction pairs, every alignment pair and each draw mode");
    // 100%, 125%, 150%, 175% and 200%.
    for (const float dpi : {96.0f, 120.0f, 144.0f, 168.0f, 192.0f})
    {
        MatrixTotals totals;
        RunMatrixOnEmbedded(cases, dpi, totals);
        std::cerr << std::format("    [W2.1] embedded {} DPI: {} cases in {} frames identical to DrawText ({} layouts created, {} retained draws, {} bypassed, "
                                 "{} frames after a format trim)\n",
                                 dpi,
                                 totals.cases,
                                 totals.frames,
                                 totals.created,
                                 totals.hits,
                                 totals.bypass,
                                 totals.formatTrims);
        Require(totals.created > 0u && totals.hits > 0u && totals.bypass > 0u && totals.formatTrims < totals.frames,
                "the matrix exercises created, retained and bypassed strings");
    }
    MatrixTotals window;
    RunMatrixOnWindow(cases, window);
    std::cerr << std::format("    [W2.1] native window: {} cases in {} frames identical to DrawText\n", window.cases, window.frames);
}

namespace
{
// Draws a list of strings, one per row, each at the same box size.
class StringRows final : public Control
{
public:
    StringRows()                             = default;
    StringRows(const StringRows&)            = delete;
    StringRows& operator=(const StringRows&) = delete;
    StringRows(StringRows&&)                 = delete;
    StringRows& operator=(StringRows&&)      = delete;
    ~StringRows() override                   = default;

    std::vector<std::wstring> strings;
    // Draw the strings as a single-line field does when its own layout could not be created (a null layout): that
    // fallback may show a revealed password, so it must retain nothing in the host's table.
    bool singleLineFallback = false;

    void Paint(ControlHost& host) const override
    {
        if (singleLineFallback)
        {
            for (size_t index = 0u; index < strings.size(); ++index)
            {
                const float top = 2.0f + static_cast<float>(index) * 20.0f;
                DrawSingleLineTextClippedWithLayout(host,
                                                    strings[index],
                                                    D2D1::RectF(2.0f, top, 200.0f, top + 18.0f),
                                                    FontRole::Body,
                                                    host.GetTheme().text,
                                                    0.0f,
                                                    DWRITE_READING_DIRECTION_LEFT_TO_RIGHT,
                                                    nullptr);
            }
            return;
        }
        for (size_t index = 0u; index < strings.size(); ++index)
        {
            const float top  = 2.0f + static_cast<float>(index % 40u) * 14.0f;
            const float left = 2.0f + static_cast<float>(index / 40u) * 150.0f;
            DrawCenteredText(
                host, strings[index], D2D1::RectF(left, top, left + 140.0f, top + 14.0f), FontRole::Small, host.GetTheme().text, DWRITE_TEXT_ALIGNMENT_LEADING);
        }
    }
};

struct LifetimeScene
{
    static constexpr UINT kWidth  = 640u;
    static constexpr UINT kHeight = 600u;
    GraphicsFixture gpu;
    EmbeddedHost view;
    StringRows* rows = nullptr;

    LifetimeScene()
    {
        gpu.width  = kWidth;
        gpu.height = kHeight;
        RequireSucceeded(gpu.Create(), "lifetime WARP device");
        std::shared_ptr<GraphicsDevice> graphics;
        RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "lifetime graphics device");
        RequireSucceeded(view.Attach(std::move(graphics), StoppedTestClock()), "lifetime view attaches");
        auto theme          = MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view.Controls().SetTheme(theme);
        auto owner = std::make_unique<StringRows>();
        rows       = owner.get();
        view.Controls().SetRoot(std::move(owner));
    }
    LifetimeScene(const LifetimeScene&)            = delete;
    LifetimeScene& operator=(const LifetimeScene&) = delete;
    LifetimeScene(LifetimeScene&&)                 = delete;
    LifetimeScene& operator=(LifetimeScene&&)      = delete;
    ~LifetimeScene()                               = default;

    // One frame of `strings` at `dpi`; its work and pixels.
    std::vector<uint8_t> Frame(std::vector<std::wstring> strings, Detail::WorkCounters& work, float dpi = 96.0f)
    {
        rows->strings = std::move(strings);
        view.MarkDirty();
        const Detail::WorkCounterScope scope;
        RequireSucceeded(view.Prepare(kWidth, kHeight, dpi), "lifetime preparation");
        work = scope.Delta();
        gpu.Bind();
        RequireSucceeded(view.Composite(gpu.context.get(), gpu.Viewport()), "lifetime composition");
        std::vector<uint8_t> pixels;
        RequireSucceeded(gpu.Read(pixels), "lifetime readback");
        return pixels;
    }
    // The same frame through DrawText. That paint draws nothing from the table, which then holds nothing; the frame after
    // it draws the strings again through the table, so the table holds them as before.
    std::vector<uint8_t> Oracle(const std::vector<std::wstring>& strings, float dpi = 96.0f)
    {
        Detail::WorkCounters work{};
        view.Controls().DebugSetTextLayoutCacheEnabledForTest(false);
        std::vector<uint8_t> pixels = Frame(strings, work, dpi);
        Require(view.Controls().DebugGetTextLayoutCacheEntries() == 0u, "a paint through DrawText alone leaves the table empty");
        view.Controls().DebugSetTextLayoutCacheEnabledForTest(true);
        static_cast<void>(Frame(strings, work, dpi));
        return pixels;
    }
    [[nodiscard]] size_t Entries() const noexcept
    {
        return const_cast<EmbeddedHost&>(view).Controls().DebugGetTextLayoutCacheEntries();
    }
    [[nodiscard]] size_t Capacity() const noexcept
    {
        return const_cast<EmbeddedHost&>(view).Controls().DebugGetTextLayoutCacheCapacity();
    }
};

// Pumps this thread's messages for `durationMs` of real time, blocking between them.
void PumpFor(uint64_t durationMs)
{
    const uint64_t end = GetTickCount64() + durationMs;
    for (uint64_t now = GetTickCount64(); now < end; now = GetTickCount64())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}

std::vector<std::wstring> Numbered(std::wstring_view prefix, size_t count, size_t first = 0u)
{
    std::vector<std::wstring> strings;
    for (size_t index = 0u; index < count; ++index)
        strings.push_back(std::format(L"{}{}", prefix, first + index));
    return strings;
}
} // namespace

void TestRetainedTextLayoutsKeyBoundAndLifetime()
{
    LifetimeScene scene;
    Detail::WorkCounters work{};

    // Key: text, format and box. Three strings, then the same three again: created once, then retained.
    const std::vector<std::wstring> abc{L"Alpha", L"Beta", L"Gamma"};
    static_cast<void>(scene.Frame(abc, work));
    Require(work.text.textLayoutCacheMisses == 3u && work.resources.textLayout.created == 3u && work.resources.implicitTextLayout.created == 0u &&
                scene.Entries() == 3u,
            "three new strings create three retained layouts and lay out nothing inside DrawText");
    const std::vector<uint8_t> retained = scene.Frame(abc, work);
    Require(work.text.textLayoutCacheHits == 3u && work.resources.textLayout.created == 0u && scene.Entries() == 3u, "the same strings again create nothing");
    Require(retained == scene.Oracle(abc), "the retained frame equals the DrawText frame");

    // Resident entries are the strings the last paint drew: a paint that drops two strings releases their layouts.
    static_cast<void>(scene.Frame({L"Alpha", L"Delta"}, work));
    Require(work.text.textLayoutCacheMisses == 1u && work.text.textLayoutCacheHits == 1u && scene.Entries() == 2u,
            "after a paint of two strings, one of them new, the table holds those two");

    // Strings that differ in one unit, a string and its prefix, and the same string drawn twice never share an entry, and a
    // repeated string at the same box is one entry.
    const std::vector<std::wstring> similar{L"Item 1", L"Item 2", L"Item", L"Item 1", L"Item 10"};
    static_cast<void>(scene.Frame(similar, work));
    Require(scene.Entries() == 4u && work.text.textLayoutCacheMisses == 4u && work.text.textLayoutCacheHits == 1u,
            "distinct strings get distinct entries; a repeated string is one entry, hit by its second draw");
    Require(scene.Frame(similar, work) == scene.Oracle(similar), "similar strings draw their own text");

    // A string longer than the retained bound is laid out by DrawText, as before, and counted.
    const std::vector<std::wstring> longText{std::wstring(5000u, L'x'), std::wstring(65536u, L'y'), L"Short"};
    static_cast<void>(scene.Frame(longText, work));
    Require(work.text.textLayoutCacheBypasses == 2u && work.resources.implicitTextLayout.created == 2u && scene.Entries() == 1u,
            "strings over 4,096 units are drawn through DrawText and counted, never retained");
    Require(scene.Frame(longText, work) == scene.Oracle(longText), "long strings draw what DrawText draws");

    // The bound: a table lowered to one set of 32 ways keeps the first 32 strings of a 40-string paint and draws the other 8
    // through DrawText; the next paint creates nothing and evicts nothing the paint uses (no thrash within a frame).
    scene.view.Controls().DebugSetTextLayoutCacheLimitForTest(32u);
    static_cast<void>(scene.Frame({}, work));
    Require(scene.Entries() == 0u, "an empty paint releases every layout");
    const std::vector<std::wstring> forty = Numbered(L"Row ", 40u);
    static_cast<void>(scene.Frame(forty, work));
    Require(scene.Capacity() == 32u && scene.Entries() == 32u && work.text.textLayoutCacheMisses == 32u && work.text.textLayoutCacheOverflows == 8u,
            "at the bound a full set keeps its ways and the overflow is drawn without retention");
    const std::vector<uint8_t> bounded = scene.Frame(forty, work);
    Require(work.resources.textLayout.created == 0u && work.text.textLayoutCacheHits == 32u && work.text.textLayoutCacheOverflows == 8u &&
                scene.Entries() == 32u,
            "the next paint of the same strings creates no layout and evicts none");
    Require(bounded == scene.Oracle(forty), "strings beyond the bound still draw exactly");
    scene.view.Controls().DebugSetTextLayoutCacheLimitForTest(0u);

    // Growth: 300 distinct strings grow the table past one set and are all retained.
    const std::vector<std::wstring> many = Numbered(L"Cell ", 300u);
    static_cast<void>(scene.Frame(many, work));
    Require(scene.Entries() == 300u && work.text.textLayoutCacheOverflows == 0u && scene.Capacity() >= 512u, "the table grows to hold a paint's strings");
    static_cast<void>(scene.Frame(many, work));
    Require(work.resources.textLayout.created == 0u && work.text.textLayoutCacheHits == 300u, "a grown table serves the whole next paint");
    static_cast<void>(scene.Frame(abc, work));
    Require(scene.Entries() == 3u, "a paint of fewer strings releases the rest");

    // A DPI change keeps the table: layouts are in DIPs, so the same strings at the same DIP boxes are retained, and they
    // draw what DrawText draws at the new DPI.
    static_cast<void>(scene.Frame(abc, work, 144.0f));
    Require(work.resources.textLayout.created == 0u && work.text.textLayoutCacheHits == 3u, "a DPI change keeps every retained layout");
    Require(scene.Frame(abc, work, 144.0f) == scene.Oracle(abc, 144.0f), "retained layouts draw exactly at the new DPI");
    static_cast<void>(scene.Frame(abc, work));

    // Hidden: the view keeps nothing, not even the table's storage; shown again, it creates the strings once more.
    scene.view.SetVisible(false);
    Require(scene.Entries() == 0u && scene.Capacity() == 0u, "a hidden view keeps no retained layout and no table");
    scene.view.SetVisible(true);
    static_cast<void>(scene.Frame(abc, work));
    Require(work.text.textLayoutCacheMisses == 3u && scene.Entries() == 3u, "a shown view creates its strings again");

    // A zero-sized preparation releases the surface and the table with it.
    RequireSucceeded(scene.view.Prepare(0u, 0u), "zero-sized preparation");
    Require(scene.Entries() == 0u && scene.Capacity() == 0u, "a zero-sized view keeps no retained layout");
    static_cast<void>(scene.Frame(abc, work));

    // Device replacement (a new device and DirectWrite factory, a new typography revision) releases the table; the next
    // paint creates the strings with the new factory.
    std::shared_ptr<GraphicsDevice> replacement;
    RequireSucceeded(GraphicsDevice::Create(scene.gpu.device.get(), replacement), "replacement graphics device");
    RequireSucceeded(scene.view.ReplaceDevice(std::move(replacement)), "device replacement");
    Require(scene.Entries() == 0u, "a device replacement releases every retained layout");
    const std::vector<uint8_t> replaced = scene.Frame(abc, work);
    Require(work.text.textLayoutCacheMisses == 3u && replaced == scene.Oracle(abc), "after a replacement the strings are created again and draw exactly");

    // A single-line field's fallback (its own layout failed) may draw a revealed password: it draws through DrawText and
    // the host's table, whose released entries keep their strings, never sees the text.
    scene.rows->singleLineFallback = true;
    static_cast<void>(scene.Frame({L"correct horse battery staple", L"hunter2"}, work));
    Require(scene.Entries() == 0u && work.text.textLayoutCacheMisses == 0u && work.text.textLayoutCacheHits == 0u &&
                work.resources.implicitTextLayout.created == 2u,
            "the single-line fallback retains nothing in the host's table");
    scene.rows->singleLineFallback = false;
    static_cast<void>(scene.Frame(abc, work));

    // Composition only composites: a preparation of all-new strings creates their layouts, the composition none.
    scene.rows->strings = Numbered(L"Fresh ", 20u);
    scene.view.MarkDirty();
    Detail::WorkCounterScope prepare;
    RequireSucceeded(scene.view.Prepare(LifetimeScene::kWidth, LifetimeScene::kHeight), "all-new preparation");
    const Detail::WorkCounters prepared = prepare.Delta();
    scene.gpu.Bind();
    const Detail::WorkCounterScope compose;
    RequireSucceeded(scene.view.Composite(scene.gpu.context.get(), scene.gpu.Viewport()), "all-new composition");
    const Detail::WorkCounters composed = compose.Delta();
    Require(prepared.text.textLayoutCacheMisses == 20u && prepared.resources.textLayout.created == 20u, "the preparation creates the new strings' layouts");
    Require(Detail::TotalCreated(composed) == 0u && composed.text.textLayoutCacheMisses == 0u && composed.text.textLayoutCacheHits == 0u,
            "the composition creates nothing and looks nothing up");

    // Detached: nothing is kept.
    scene.view.Detach();
    Require(scene.Entries() == 0u && scene.Capacity() == 0u, "a detached view keeps no retained layout");
    std::cerr << "    [W2.1] key, bound, DPI, hide, zero size, device replacement, composition and detach behave as specified\n";
}

// W2.1 review F1: a native host in a child window receives no message when its top-level is minimized or its parent is
// hidden with SWP_HIDEWINDOW (ShowWindow does send the child WM_SHOWWINDOW); it finds out at its next schedule wake (a focused caret keeps one armed here) and
// releases its table there. A host resized to 0 px releases it in WM_SIZE. A parent's redraw block sends WM_SHOWWINDOW
// FALSE, which releases it too. Each time the next paint lays the strings out again and draws what DrawText draws.
void TestRetainedTextLayoutsReleasedWhenAChildHostCannotPaint()
{
    enum class Hide
    {
        ParentHidden,
        ParentHiddenWithoutMessage,
        ParentRedrawBlock,
        ZeroWidth,
    };
    for (const Hide hide : {Hide::ParentHidden, Hide::ParentHiddenWithoutMessage, Hide::ParentRedrawBlock, Hide::ZeroWidth})
    {
        wil::unique_hwnd parent(CreateWindowExW(WS_EX_NOACTIVATE,
                                                L"STATIC",
                                                L"retained text layout parent",
                                                WS_OVERLAPPED,
                                                -32000,
                                                -32000,
                                                360,
                                                240,
                                                nullptr,
                                                nullptr,
                                                GetModuleHandleW(nullptr),
                                                nullptr));
        Require(parent != nullptr, "the parent window is created");
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        SetWindowLongPtrW(window.Hwnd(), GWL_STYLE, WS_CHILD | WS_VISIBLE);
        Require(SetParent(window.Hwnd(), parent.get()) != nullptr || GetLastError() == ERROR_SUCCESS, "the host window becomes a child");
        SetWindowPos(window.Hwnd(), nullptr, 0, 0, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        WindowHost& host = window.Host();
        host.DebugSetCaretBlinkSettingsForTest(ControlHost::CaretBlinkSettingsForTest{100u, 0u});
        auto root    = std::make_unique<Panel>();
        auto* first  = root->AddChild<Label>(L"Retained alpha");
        auto* second = root->AddChild<Label>(L"Retained beta");
        auto* field  = root->AddChild<TextField>(L"caret");
        first->SetBounds(D2D1::RectF(8.0f, 8.0f, 208.0f, 32.0f));
        second->SetBounds(D2D1::RectF(8.0f, 40.0f, 208.0f, 64.0f));
        field->SetBounds(D2D1::RectF(8.0f, 72.0f, 208.0f, 104.0f));
        host.SetRoot(std::move(root));
        ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        PumpFor(200u);
        host.SetFocusControl(field, false);
        PumpFor(150u);
        WindowHostBitmapCapture painted;
        Require(host.DebugCaptureBitmap(painted) && host.DebugGetTextLayoutCacheEntries() >= 2u, "the child host retains its labels' layouts");
        if (hide == Hide::ParentHidden)
            ShowWindow(parent.get(), SW_HIDE);
        else if (hide == Hide::ParentHiddenWithoutMessage) // SWP_HIDEWINDOW: no WM_SHOWWINDOW to the parent or the child
            SetWindowPos(parent.get(), nullptr, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        else if (hide == Hide::ParentRedrawBlock)
            SendMessageW(parent.get(), WM_SETREDRAW, FALSE, 0);
        else
            SetWindowPos(window.Hwnd(), nullptr, 0, 0, 0, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        PumpFor(1'500u); // the hidden host's probe wake comes within a second
        Require(host.DebugGetTextLayoutCacheEntries() == 0u && host.DebugGetTextLayoutCacheCapacity() == 0u,
                "a child host that cannot paint (parent hidden, parent redraw block, 0 px wide) keeps no retained layout");
        if (hide == Hide::ParentHidden || hide == Hide::ParentHiddenWithoutMessage)
            ShowWindow(parent.get(), SW_SHOWNOACTIVATE);
        else if (hide == Hide::ParentRedrawBlock)
            SendMessageW(parent.get(), WM_SETREDRAW, TRUE, 0);
        else
            SetWindowPos(window.Hwnd(), nullptr, 0, 0, 320, 200, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
        PumpFor(1'500u);
        WindowHostBitmapCapture refilled;
        Require(host.DebugCaptureBitmap(refilled), "the shown child host paints");
        const uint64_t entries = host.DebugGetTextLayoutCacheEntries();
        host.DebugSetTextLayoutCacheEnabledForTest(false);
        WindowHostBitmapCapture oracle;
        Require(host.DebugCaptureBitmap(oracle), "the shown child host paints through DrawText");
        host.DebugSetTextLayoutCacheEnabledForTest(true);
        Require(entries >= 2u && refilled.bgraPixels == oracle.bgraPixels, "the next paint refills the table and draws what DrawText draws");
        std::cerr << "    [W2.1] child host " << static_cast<int>(hide) << ": released while it could not paint, refilled with " << entries << " layouts\n";
        host.SetFocusControl(nullptr, false);
        SetParent(window.Hwnd(), nullptr);
    }
}
