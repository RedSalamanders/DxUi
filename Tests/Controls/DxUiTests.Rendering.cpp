#include "DxUiTestHelpers.h"

namespace
{

using DxUi::WindowHostBitmapCapture;

class TransientSurfaceProbe final : public DxUi::Control
{
public:
    void SetPressed(bool pressed) noexcept
    {
        _pressed = pressed;
        RequestInvalidate();
    }

    void SetBackdropCapture(DxUi::WindowHostBitmapCapture capture) noexcept
    {
        _backdrop.SetCapture(std::move(capture));
        RequestInvalidate();
    }

    void ClearBackdrop() noexcept
    {
        _backdrop.Reset();
        RequestInvalidate();
    }

    void Paint(DxUi::WindowHost& host) const override
    {
        DxUi::PaintTransientSurface(host,
                                    D2D1::RectF(80.0f, 50.0f, 560.0f, 150.0f),
                                    DxUi::TransientSurfaceOptions{
                                        .cornerRadiusDip = 18.0f,
                                        .drawShadow      = true,
                                        .pressed         = _pressed,
                                        .backdrop        = &_backdrop,
                                    });
    }

private:
    bool _pressed = false;
    mutable DxUi::TransientSurfaceBackdrop _backdrop;
};

[[nodiscard]] uint8_t CaptureAlpha(const WindowHostBitmapCapture& capture, UINT x, UINT y) noexcept
{
    if (x >= capture.widthPx || y >= capture.heightPx)
    {
        return 0u;
    }
    const size_t offset = ((static_cast<size_t>(y) * static_cast<size_t>(capture.widthPx)) + static_cast<size_t>(x)) * 4u + 3u;
    return offset < capture.bgraPixels.size() ? capture.bgraPixels[offset] : 0u;
}

[[nodiscard]] uint32_t CaptureBgra(const WindowHostBitmapCapture& capture, UINT x, UINT y) noexcept
{
    if (x >= capture.widthPx || y >= capture.heightPx)
    {
        return 0u;
    }
    const size_t offset = ((static_cast<size_t>(y) * static_cast<size_t>(capture.widthPx)) + static_cast<size_t>(x)) * 4u;
    if (offset + 3u >= capture.bgraPixels.size())
    {
        return 0u;
    }
    return static_cast<uint32_t>(capture.bgraPixels[offset]) | (static_cast<uint32_t>(capture.bgraPixels[offset + 1u]) << 8u) |
           (static_cast<uint32_t>(capture.bgraPixels[offset + 2u]) << 16u) | (static_cast<uint32_t>(capture.bgraPixels[offset + 3u]) << 24u);
}

[[nodiscard]] uint64_t CountVisiblePixels(const WindowHostBitmapCapture& capture, uint8_t threshold = 8u) noexcept
{
    uint64_t count = 0u;
    for (size_t offset = 3u; offset < capture.bgraPixels.size(); offset += 4u)
    {
        count += capture.bgraPixels[offset] > threshold ? 1u : 0u;
    }
    return count;
}

WindowHostBitmapCapture CaptureAttachedHostWindowBitmap(AttachedHostWindow& window, const char* context)
{
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), context);
    return capture;
}

void TestGridMultilineClampPreservesCompleteModelText()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 620, 550, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    GridCellData cell{};
    cell.text =
        L"Première ligne française\r\nDeuxième ligne é\r\nTroisième ligne 📷 🍊\r\nQuatrième ligne 👨‍👩‍👧\r\nDernière ligne complète";
    cell.multiline = true;
    SingleCellGridModel emptyModel(GridCellData{});
    SingleCellGridModel textModel(cell);
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 230.0f));
    grid->SetRowHeightDip(160.0f);
    grid->SetHeaderHeightDip(30.0f);
    grid->SetLineClamp(8u);
    grid->SetModel(&emptyModel);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    const auto empty = CaptureAttachedHostWindowBitmap(window, "empty grid clamp reference");
    grid->SetModel(&textModel);
    grid->ApplyColumnLayout(columns);
    const auto full        = CaptureAttachedHostWindowBitmap(window, "eight-line Unicode grid reference");
    size_t warmGlyphPixels = 0u;
    for (size_t offset = 0u; offset + 3u < full.bgraPixels.size(); offset += 4u)
    {
        const auto blue  = full.bgraPixels[offset];
        const auto green = full.bgraPixels[offset + 1u];
        const auto red   = full.bgraPixels[offset + 2u];
        if (red >= 170u && green >= 40u && green <= 220u && blue <= 120u && static_cast<int>(red) >= green + 20 && static_cast<int>(green) >= blue + 15)
            ++warmGlyphPixels;
    }
    Require(warmGlyphPixels >= 24u, "multiline grid preserves color-font emoji rather than silently rendering them monochrome");
    grid->SetLineClamp(2u);
    const auto clamped = CaptureAttachedHostWindowBitmap(window, "two-line grid clamp witness");
    const auto inkRows = [&](const auto& capture)
    {
        UINT rows     = 0u;
        const auto px = [&](float dip) { return static_cast<UINT>(window.Host().DipsToPixels(dip)); };
        for (UINT y = px(54.0f); y < px(207.0f); ++y)
            for (UINT x = px(29.0f); x < px(311.0f); ++x)
                if (CaptureBgra(capture, x, y) != CaptureBgra(empty, x, y))
                {
                    ++rows;
                    break;
                }
        return rows;
    };
    const auto fullRows    = inkRows(full);
    const auto clampedRows = inkRows(clamped);
    std::cout << "Grid clamp ink rows: full=" << fullRows << " clamped=" << clampedRows << '\n';
    Require(fullRows > 0u && clampedRows > 0u && clampedRows * 2u < fullRows,
            "two-line clamp must visibly omit later text with an ellipsis, instead of painting all wrapped lines");
    GridCellData prefix = cell;
    prefix.text         = L"Première ligne française\nDeuxième ligne é";
    SingleCellGridModel prefixModel(prefix);
    grid->SetModel(&prefixModel);
    grid->ApplyColumnLayout(columns);
    const auto prefixCapture = CaptureAttachedHostWindowBitmap(window, "two complete lines without omitted content");
    uint64_t omissionPixels  = 0u;
    const auto px            = [&](float dip) { return static_cast<UINT>(window.Host().DipsToPixels(dip)); };
    for (UINT y = px(54.0f); y < px(207.0f); ++y)
        for (UINT x = px(29.0f); x < px(311.0f); ++x)
            omissionPixels += CaptureBgra(prefixCapture, x, y) != CaptureBgra(clamped, x, y) ? 1u : 0u;
    Require(omissionPixels > 0u, "omitted content must paint a visible ellipsis beyond the same two complete prefix lines");
    grid->SetModel(&textModel);
    grid->ApplyColumnLayout(columns);
    grid->GetSelectionModel().SetSingle(textModel.GetStableRowId(0u));
    Require(grid->BuildSelectionTsv().find(L"Dernière ligne complète") != std::wstring::npos,
            "visible trimming must not remove the final original line from copied model text");
    grid->GetSelectionModel().Clear();
    const auto beforeClip = CaptureAttachedHostWindowBitmap(window, "full-cell layout before viewport clipping");
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 132.0f));
    const auto clipped     = CaptureAttachedHostWindowBitmap(window, "partially visible cell keeps its full layout");
    uint64_t changedPixels = 0u;
    for (UINT y = px(54.0f); y < px(123.0f); ++y)
        for (UINT x = px(29.0f); x < px(311.0f); ++x)
            changedPixels += CaptureBgra(beforeClip, x, y) != CaptureBgra(clipped, x, y) ? 1u : 0u;
    Require(changedPixels == 0u, "viewport clipping must not reflow or recenter the surviving portion of a cell");

    // Reuse the same model through text, font, width, height and clamp changes.
    // Each cached result must equal a fresh model attachment. The last two steps
    // keep text, width and height, and so the cache slot, fixed and change only
    // the font role, then only the clamp: a layout reused without comparing that
    // key field would paint stale content.
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 230.0f));
    struct CacheVariant
    {
        bool oversizedText;
        FontRole fontRole;
        float rowHeightDip;
        uint32_t lineClamp;
        float columnWidthDip;
    };
    constexpr CacheVariant variants[] = {
        {false, FontRole::Body, 96.0f, 1u, 270.0f},
        {false, FontRole::Subtitle, 96.0f, 3u, 150.0f},
        {false, FontRole::Body, 38.0f, 3u, 270.0f},
        {true, FontRole::Body, 96.0f, 3u, 270.0f},
        {false, FontRole::Body, 96.0f, 3u, 270.0f},
        {false, FontRole::Subtitle, 96.0f, 3u, 270.0f},
        {false, FontRole::Subtitle, 96.0f, 2u, 270.0f},
    };
    for (const CacheVariant& variant : variants)
    {
        auto changed = cell;
        changed.text = variant.oversizedText ? std::wstring(5000u, L'é') + L"\nFin 📷" : L"Valeur modifiée 👨‍👩‍👧\nDeuxième ligne\nFin complète";
        textModel    = SingleCellGridModel(changed);
        grid->NotifyDataChanged();
        grid->SetCellTextFontRole(variant.fontRole);
        grid->SetRowHeightDip(variant.rowHeightDip);
        grid->SetLineClamp(variant.lineClamp);
        const std::array<GridColumnLayoutEntry, 1> changedColumns{{{L"status", 0u, variant.columnWidthDip}}};
        grid->ApplyColumnLayout(changedColumns);
        const auto cached = CaptureAttachedHostWindowBitmap(window, "updated retained multiline cell");
        grid->SetModel(nullptr);
        grid->SetModel(&textModel);
        grid->ApplyColumnLayout(changedColumns);
        const auto fresh = CaptureAttachedHostWindowBitmap(window, "fresh multiline layout reference");
        Require(cached.bgraPixels == fresh.bgraPixels, "text/font/width/height/clamp updates must match a fresh layout without stale cached content");
        grid->GetSelectionModel().SetSingle(textModel.GetStableRowId(0u));
        Require(grid->BuildSelectionTsv().find(changed.text) != std::wstring::npos, "even oversized uncached text must retain its complete copy value");
        grid->GetSelectionModel().Clear();
    }
}

void TestGridMultilineShortRowsPaintClippedFirstLine()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    GridCellData cell{};
    cell.text      = L"Quelques glyphes jpgy qui descendent\nParagraphe omis";
    cell.multiline = true;
    SingleCellGridModel emptyModel(GridCellData{});
    SingleCellGridModel textModel(cell);
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetModel(&emptyModel);
    window.Host().SetRoot(std::move(root));
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    struct ShortRowCase
    {
        const char* name;
        Density density;
        float rowHeightDip;
        FontRole fontRole;
    };
    // The minimum 20-DIP row leaves a 14-DIP text area, less than one Body line.
    // Compact density shrinks the default 28-DIP row the same way. A Title line
    // is taller than the whole cell, so only clipping keeps it out of other rows.
    constexpr ShortRowCase cases[] = {
        {"minimum row, Body", Density::Standard, 20.0f, FontRole::Body},
        {"compact default row, Body", Density::Compact, 28.0f, FontRole::Body},
        {"minimum row, Title taller than the cell", Density::Standard, 20.0f, FontRole::Title},
    };
    for (const ShortRowCase& shortRow : cases)
    {
        auto theme          = window.Host().GetTheme();
        theme.reducedMotion = true;
        theme.density       = shortRow.density;
        window.Host().SetTheme(theme);
        grid->SetRowHeightDip(shortRow.rowHeightDip);
        grid->SetCellTextFontRole(shortRow.fontRole);
        grid->SetModel(&emptyModel);
        grid->ApplyColumnLayout(columns);
        const auto empty = CaptureAttachedHostWindowBitmap(window, "short multiline row empty reference");
        grid->SetModel(&textModel);
        grid->ApplyColumnLayout(columns);
        const auto painted = CaptureAttachedHostWindowBitmap(window, "short multiline row with text");
        Require(painted.widthPx == empty.widthPx && painted.heightPx == empty.heightPx, "short multiline row captures share one extent");
        const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(window.Host(), 0u, 0u);
        const auto px                       = [&](float dip) { return window.Host().DipsToPixels(dip); };
        uint64_t textPixels                 = 0u;
        uint64_t strayPixels                = 0u;
        for (UINT y = 0u; y < painted.heightPx; ++y)
            for (UINT x = 0u; x < painted.widthPx; ++x)
            {
                if (CaptureBgra(painted, x, y) == CaptureBgra(empty, x, y))
                    continue;
                const float centerX = static_cast<float>(x) + 0.5f;
                const float centerY = static_cast<float>(y) + 0.5f;
                if (centerX >= px(metrics.textRect.left) && centerX <= px(metrics.textRect.right) && centerY >= px(metrics.textRect.top) &&
                    centerY <= px(metrics.textRect.bottom))
                    ++textPixels;
                else if (centerX < px(metrics.cellRect.left) - 1.0f || centerX > px(metrics.cellRect.right) + 1.0f ||
                         centerY < px(metrics.cellRect.top) - 1.0f || centerY > px(metrics.cellRect.bottom) + 1.0f)
                    ++strayPixels;
            }
        std::cout << "Grid short multiline row (" << shortRow.name << "): text=" << textPixels << " outside=" << strayPixels << '\n';
        Require(textPixels > 0u, "a multiline cell too short for one complete line still paints its first line");
        Require(strayPixels == 0u, "a multiline first line taller than its text area never paints outside the cell");
    }
}

void TestGridMultilineTrailingSeparatorsMatchTrimmedTwin()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    SingleCellGridModel model(GridCellData{});
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    // A 58-DIP text area fits three Body lines, so a phantom empty line after a
    // trailing separator would fit too and move the centred text instead of hiding.
    grid->SetRowHeightDip(64.0f);
    grid->SetModel(&model);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    const auto capture = [&](std::wstring text, uint32_t lineClamp, const char* context)
    {
        GridCellData cell{};
        cell.text      = std::move(text);
        cell.multiline = true;
        model          = SingleCellGridModel(cell);
        grid->NotifyDataChanged();
        grid->SetLineClamp(lineClamp);
        return CaptureAttachedHostWindowBitmap(window, context);
    };
    struct TwinCase
    {
        const wchar_t* text;
        const wchar_t* twin;
        uint32_t lineClamp;
        const char* name;
    };
    // Each value paints exactly like its twin without the trailing separators or
    // the space before the omission marker: no false ellipsis and no offset.
    const TwinCase cases[] = {
        {L"abc\r\n", L"abc", 1u, "single paragraph ending in CRLF, clamp 1"},
        {L"A\r\nB\r\n", L"A\r\nB", 2u, "two paragraphs ending in CRLF, clamp 2"},
        {L"A\r\nB\r\n", L"A\r\nB", 3u, "two paragraphs ending in CRLF, clamp 3"},
        {L"Fin\u2029", L"Fin", 2u, "trailing paragraph separator"},
        {L"Fin\u2028\u2028", L"Fin", 2u, "trailing line separators"},
        {L"Fin \r\nsuite", L"Fin\r\nsuite", 1u, "space before an omitted paragraph"},
        {L"A \r\nB \r\nC", L"A \r\nB\r\nC", 2u, "space ending the last visible line"},
        // DirectWrite also breaks on NEL, VT and FF, and a blank last line is as empty as a missing one.
        {L"abc\x85", L"abc", 1u, "trailing next-line character, clamp 1"},
        {L"abc\v", L"abc", 2u, "trailing vertical tab, clamp 2"},
        {L"abc\f", L"abc", 2u, "trailing form feed, clamp 2"},
        {L"abc\r\n   ", L"abc", 2u, "blank last line of spaces"},
        {L"abc\n\t", L"abc", 1u, "blank last line of a tab"},
        {L"abc\n \r\n", L"abc", 2u, "blank last line of a no-break space"},
        // Any other space, and the characters that paint nothing, leave a line as blank.
        {L"abc\n\x2003", L"abc", 2u, "blank last line of an em space"},
        {L"abc\n\x202F\x2009", L"abc", 3u, "blank last line of narrow and thin spaces"},
        {L"abc\n\x200B", L"abc", 2u, "blank last line of a zero-width space"},
        {L"abc\n\xFEFF\x2060", L"abc", 3u, "blank last line of a byte order mark and a word joiner"},
        {L"abc\r\n\x200E", L"abc", 2u, "blank last line of a directional mark"},
        {L"abc\n\x00AD", L"abc", 2u, "blank last line of a soft hyphen"},
        {L"abc\n\x061C", L"abc", 2u, "blank last line of an Arabic letter mark"},
        {L"abc\n\x202C\x2069", L"abc", 2u, "blank last line closing an embedding and an isolate"},
        {L"abc\n\x034F\xFE0F", L"abc", 3u, "blank last line of a grapheme joiner and a variation selector"},
    };
    for (const TwinCase& twinCase : cases)
    {
        const auto trailing = capture(twinCase.text, twinCase.lineClamp, twinCase.name);
        const auto twin     = capture(twinCase.twin, twinCase.lineClamp, twinCase.name);
        std::cout << "Grid multiline trailing-separator twin: " << twinCase.name << '\n';
        Require(trailing.bgraPixels == twin.bgraPixels, "trailing separators or a trailing space must not add an ellipsis or shift centred lines");
    }
    const auto empty = capture(L"", 2u, "empty multiline value");
    for (const wchar_t* separatorsOnly : {L"\r\n", L"\r\n\r\n", L"\u2029", L"\x85\f", L"  \r\n "})
    {
        const auto blank = capture(separatorsOnly, 2u, "separator-only multiline value");
        Require(blank.bgraPixels == empty.bgraPixels, "a multiline value made only of separators paints nothing");
    }
}

// Distinct two-paragraph text in every cell, as in a real table: a layout cache keyed by value must hold one layout
// per visible cell. The first paragraph wraps past a two-line clamp, so every cell also takes the omission path.
class DistinctMultilineGridModel final : public DxUi::IGridModel
{
public:
    DistinctMultilineGridModel(size_t rowCount, size_t columnCount, bool multiline = true)
        : _rowCount(rowCount),
          _columnCount(columnCount),
          _multiline(multiline)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnCount;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"c" + std::to_wstring(columnIndex);
        column.title    = L"Colonne " + std::to_wstring(columnIndex);
        column.widthDip = 110.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        outCell.text      = L"Élément " + std::to_wstring(rowIndex) + L"." + std::to_wstring(columnIndex) +
                            L" : vérifier la configuration du serveur principal.\nDeuxième paragraphe masqué.";
        outCell.multiline = _multiline;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId < _rowCount ? std::optional<size_t>(static_cast<size_t>(rowId)) : std::nullopt;
    }

private:
    size_t _rowCount    = 0u;
    size_t _columnCount = 0u;
    bool _multiline     = true;
};

// A repaint of unchanged cells lays nothing out again, however many distinct values are visible: 24 values that a
// direct-mapped table crowded into a few slots, more values than its 32 slots, and exactly the 32 values that fill the
// first table. Scrolling a row either way lays out only that row: scrolling up draws the entering row first, before
// the rows still in view, and must not evict their layouts to make room.
void TestGridMultilineLayoutsSurviveRepaintForEveryDistinctVisibleCell()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 static_cast<int>(window.Host().DipsToPixels(760.0f)),
                 static_cast<int>(window.Host().DipsToPixels(640.0f)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    struct Geometry
    {
        size_t rowCount;
        size_t columnCount;
        float rowHeight; // 52.48 (compact density's 1.64 x 32) is not a sum of powers of two: scrolled rows round apart.
    };
    for (const auto& [rowCount, columnCount, rowHeight] :
         {Geometry{6u, 4u, 48.0f}, Geometry{7u, 4u, 48.0f}, Geometry{10u, 6u, 48.0f}, Geometry{6u, 4u, 52.48f}})
    {
        DistinctMultilineGridModel model(rowCount + 4u, columnCount);
        auto root  = std::make_unique<Panel>();
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(
            10.0f, 10.0f, 10.0f + (static_cast<float>(columnCount) * 110.0f) + 20.0f, 10.0f + 30.0f + (static_cast<float>(rowCount) * rowHeight) + 20.0f));
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(rowHeight);
        grid->SetLineClamp(2u);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
        const auto first    = CaptureAttachedHostWindowBitmap(window, "distinct multiline cells, first paint");
        size_t visibleCells = 0u;
        for (size_t row = 0u; row < rowCount; ++row)
            for (size_t column = 0u; column < columnCount; ++column)
                visibleCells += grid->GetVisibleCellRect(row, column).has_value() ? 1u : 0u;
        const auto warmed    = grid->DebugGetTextLayoutStatistics();
        const auto second    = CaptureAttachedHostWindowBitmap(window, "distinct multiline cells, repaint");
        const auto repainted = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid distinct multiline cells " << rowCount << "x" << columnCount << ": visible " << visibleCells << ", repaint lookups "
                  << repainted.lookups - warmed.lookups << ", hits " << repainted.hits - warmed.hits << ", new layouts "
                  << repainted.layoutCreations - warmed.layoutCreations << ", retained " << repainted.retainedLayouts << " of " << repainted.capacity << '\n';
        Require(visibleCells == rowCount * columnCount, "every distinct multiline cell is visible");
        Require(repainted.lookups - warmed.lookups >= visibleCells, "every visible multiline cell asks for its layout on repaint");
        Require(repainted.hits - warmed.hits == repainted.lookups - warmed.lookups, "every repaint lookup of an unchanged cell hits");
        Require(repainted.layoutCreations == warmed.layoutCreations, "a repaint of unchanged distinct cells lays nothing out again");
        Require(repainted.retainedLayouts >= visibleCells, "each visible distinct value keeps its layout");
        Require(second.bgraPixels == first.bgraPixels, "reused layouts paint exactly as the first paint");
        // Scrolling one row keeps every value still in view: only the row that enters is laid out (a measure and a
        // display layout per cell).
        grid->DebugSetScrollOffsets(rowHeight, 0.0f);
        const auto beforeScroll = grid->DebugGetTextLayoutStatistics();
        static_cast<void>(CaptureAttachedHostWindowBitmap(window, "distinct multiline cells, scrolled one row"));
        const auto scrolled = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid distinct multiline cells " << rowCount << "x" << columnCount << ": one-row scroll lays out "
                  << scrolled.layoutCreations - beforeScroll.layoutCreations << " layouts\n";
        Require(scrolled.layoutCreations - beforeScroll.layoutCreations <= 2u * columnCount, "scrolling one row lays out only the row that entered");
        // A few rows further down, the rows' layouts sit wherever the entering rows found room; scrolling back up
        // then draws the entering top row before rows still in view whose layouts share its sets.
        for (const float offset : {2.0f * rowHeight, 3.0f * rowHeight})
        {
            grid->DebugSetScrollOffsets(offset, 0.0f);
            static_cast<void>(CaptureAttachedHostWindowBitmap(window, "distinct multiline cells, scrolled down a row"));
        }
        const auto beforeScrollUp = grid->DebugGetTextLayoutStatistics();
        grid->DebugSetScrollOffsets(2.0f * rowHeight, 0.0f);
        static_cast<void>(CaptureAttachedHostWindowBitmap(window, "distinct multiline cells, scrolled back up one row"));
        const auto scrolledUp = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid distinct multiline cells " << rowCount << "x" << columnCount << ": one-row scroll up lays out "
                  << scrolledUp.layoutCreations - beforeScrollUp.layoutCreations << " layouts\n";
        Require(scrolledUp.layoutCreations - beforeScrollUp.layoutCreations <= 2u * columnCount,
                "scrolling back up one row lays out only the row that entered, not the rows still in view");
        window.Host().SetRoot(nullptr);
    }
}

// Single-line captions keep their layouts too: a repaint of unchanged cells lays nothing out, and scrolling one row
// either way lays out only the entering row, including at a row height scrolling cannot place on whole floats.
void TestGridSingleLineLayoutsSurviveRepaintAndScroll()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 static_cast<int>(window.Host().DipsToPixels(760.0f)),
                 static_cast<int>(window.Host().DipsToPixels(640.0f)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    for (const float rowHeight : {28.0f, 22.96f})
    {
        constexpr size_t rowCount    = 10u;
        constexpr size_t columnCount = 4u;
        DistinctMultilineGridModel model(rowCount + 8u, columnCount, false);
        auto root  = std::make_unique<Panel>();
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(
            10.0f, 10.0f, 10.0f + (static_cast<float>(columnCount) * 110.0f) + 20.0f, 10.0f + 30.0f + (static_cast<float>(rowCount) * rowHeight) + 20.0f));
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(rowHeight);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
        const auto first   = CaptureAttachedHostWindowBitmap(window, "single-line cells, first paint");
        const auto warmed  = grid->DebugGetTextLayoutStatistics();
        const auto second  = CaptureAttachedHostWindowBitmap(window, "single-line cells, repaint");
        const auto painted = grid->DebugGetTextLayoutStatistics();
        Require(painted.lookups > warmed.lookups && painted.hits - warmed.hits == painted.lookups - warmed.lookups,
                "every repaint lookup of an unchanged single-line cell hits");
        Require(painted.layoutCreations == warmed.layoutCreations, "a repaint of unchanged single-line cells lays nothing out again");
        Require(second.bgraPixels == first.bgraPixels, "retained single-line layouts paint exactly as the first paint");
        uint64_t creations = painted.layoutCreations;
        for (const float offset : {rowHeight, 2.0f * rowHeight, 3.0f * rowHeight, 2.0f * rowHeight})
        {
            grid->DebugSetScrollOffsets(offset, 0.0f);
            static_cast<void>(CaptureAttachedHostWindowBitmap(window, "single-line cells, scrolled one row"));
            const uint64_t now = grid->DebugGetTextLayoutStatistics().layoutCreations;
            std::cout << "Grid single-line cells at " << rowHeight << " DIP rows: a one-row scroll lays out " << now - creations << " layouts\n";
            Require(now - creations <= columnCount, "scrolling single-line cells one row lays out only the row that entered");
            creations = now;
        }
        window.Host().SetRoot(nullptr);
    }
}

// A painting grid keeps one layout per cell it drew last, however far it has scrolled: the tables never accumulate the
// rows it left. (The omitted-tail table holds a layout only in the paint that builds it, to share it among values with
// the same visible text; the value entries keep the layouts, and the next paint that hits them releases the tail table's,
// which keeps the storage of its entries alone.) Prints what a painting grid holds for the fixtures above: live layouts
// and entries per table, the string storage their keys hold (UTF-16 units, two bytes each) and the bytes of the tables,
// fresh and after scrolling through every row.
void TestGridPaintedLayoutsAreThoseOfItsVisibleCells()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 static_cast<int>(window.Host().DipsToPixels(760.0f)),
                 static_cast<int>(window.Host().DipsToPixels(640.0f)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    struct Geometry
    {
        const char* kind;
        bool multiline;
        size_t rowCount;
        size_t columnCount;
        float rowHeight;
    };
    for (const auto& [kind, multiline, rowCount, columnCount, rowHeight] : {Geometry{"multiline", true, 6u, 4u, 48.0f},
                                                                            Geometry{"multiline", true, 7u, 4u, 48.0f},
                                                                            Geometry{"multiline", true, 10u, 6u, 48.0f},
                                                                            Geometry{"single-line", false, 10u, 4u, 28.0f},
                                                                            Geometry{"single-line", false, 10u, 4u, 22.96f}})
    {
        const size_t modelRows = rowCount + 8u;
        DistinctMultilineGridModel model(modelRows, columnCount, multiline);
        auto root  = std::make_unique<Panel>();
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(
            10.0f, 10.0f, 10.0f + (static_cast<float>(columnCount) * 110.0f) + 20.0f, 10.0f + 30.0f + (static_cast<float>(rowCount) * rowHeight) + 20.0f));
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(rowHeight);
        grid->SetLineClamp(2u);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
        const auto countDrawnCells = [&]
        {
            size_t cells = 0u;
            for (size_t row = 0u; row < modelRows; ++row)
                for (size_t column = 0u; column < columnCount; ++column)
                    cells += grid->GetVisibleCellRect(row, column).has_value() ? 1u : 0u;
            return cells;
        };
        const auto describe = [](const Grid::GridDebugTextLayoutStatistics& held)
        {
            std::ostringstream text;
            text << "value table " << held.retainedLayouts << " layouts of " << held.capacity << " entries, omitted-tail table " << held.displayLayouts
                 << " of " << held.displayCapacity << ", keys " << held.textUnits << " units (" << held.textUnits * sizeof(wchar_t) << " bytes), tables "
                 << held.tableBytes << " bytes, ellipsis sign " << (held.ellipsis ? "held" : "none");
            return text.str();
        };
        static_cast<void>(CaptureAttachedHostWindowBitmap(window, "painted grid, first paint"));
        const size_t drawnCells = countDrawnCells();
        const auto fresh        = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid holds, " << kind << " " << rowCount << "x" << columnCount << " distinct cells at " << rowHeight << " DIP rows (" << drawnCells
                  << " drawn cells): " << describe(fresh) << '\n';
        Require(fresh.retainedLayouts == drawnCells, "a painting grid keeps one value layout per cell it drew");
        Require((fresh.displayCapacity > 0u) == multiline, "only multiline cells with an omitted tail use the tail table");
        // Scrolling through every row leaves the layouts of the cells then in view only, not those of the rows it passed.
        size_t mostLayouts = 0u;
        for (size_t row = 1u; row + rowCount <= modelRows; ++row)
        {
            grid->DebugSetScrollOffsets(static_cast<float>(row) * rowHeight, 0.0f);
            static_cast<void>(CaptureAttachedHostWindowBitmap(window, "painted grid, scrolled"));
            const auto scrolled = grid->DebugGetTextLayoutStatistics();
            mostLayouts         = std::max({mostLayouts, scrolled.retainedLayouts, scrolled.displayLayouts});
        }
        const auto scrolled = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid holds, " << kind << " " << rowCount << "x" << columnCount << " after scrolling through every row: " << describe(scrolled) << '\n';
        Require(mostLayouts <= drawnCells + columnCount, "scrolling never leaves more layouts than the cells in view and a partly visible row");
        window.Host().SetRoot(nullptr);
    }
}

using GridLayoutStatistics = DxUi::Grid::GridDebugTextLayoutStatistics;

// Nothing left: no layout in either table, no table entry, no string storage and no ellipsis sign.
void RequireNoRetainedGridLayouts(const GridLayoutStatistics& statistics, const char* context)
{
    Require(statistics.retainedLayouts == 0u && statistics.displayLayouts == 0u && statistics.capacity == 0u && statistics.displayCapacity == 0u &&
                statistics.textUnits == 0u && statistics.tableBytes == 0u && ! statistics.ellipsis,
            context);
}

// Removes its subtree from the host without destroying it, which is what ClearChildren, SetRoot and PageHost do on their
// way to destroying it: the grids in it then stop painting while they are still alive.
class HostDetachingPanel final : public DxUi::Panel
{
public:
    void DetachFromHost() noexcept
    {
        PropagateHost(nullptr);
    }

    void AttachToHost(DxUi::ControlHost& host) noexcept
    {
        PropagateHost(&host);
    }
};

// A grid that stops painting keeps no layouts: hidden itself, under a hidden panel or page host, removed from its host or
// given another model, it returns its layouts, their string storage and its tables at once, since no later paint will
// release the ones its last paint used. Showing it again lays out only what it shows, exactly as its first paint did, and
// paints the same pixels; the paths that paint are unchanged.
void TestGridReleasesItsLayoutsWhenItStopsPainting()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 static_cast<int>(window.Host().DipsToPixels(760.0f)),
                 static_cast<int>(window.Host().DipsToPixels(640.0f)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    constexpr size_t rowCount    = 6u;
    constexpr size_t columnCount = 4u;
    constexpr float rowHeight    = 48.0f;
    // The tree of every step: root > outer panel > page host > page > grid.
    struct Scene
    {
        HostDetachingPanel* root = nullptr;
        Panel* outer             = nullptr;
        PageHost* pages          = nullptr;
        Grid* grid               = nullptr;
        ControlHost* host        = nullptr;
        IGridModel* model        = nullptr;
        IGridModel* otherModel   = nullptr;
    };
    struct Step
    {
        const char* name;
        bool paintedWhileStopped; // The window still paints around what is hidden; a detached or re-modelled grid would paint again.
        void (*stop)(const Scene&);
        void (*resume)(const Scene&);
    };
    const std::array<Step, 5> steps{{
        {"hidden itself", true, [](const Scene& scene) { scene.grid->SetVisible(false); }, [](const Scene& scene) { scene.grid->SetVisible(true); }},
        {"under a hidden page host",
         true,
         [](const Scene& scene) { scene.pages->SetVisible(false); },
         [](const Scene& scene) { scene.pages->SetVisible(true); }},
        {"under a hidden panel", true, [](const Scene& scene) { scene.outer->SetVisible(false); }, [](const Scene& scene) { scene.outer->SetVisible(true); }},
        {"detached from its host",
         false,
         [](const Scene& scene) { scene.root->DetachFromHost(); },
         [](const Scene& scene) { scene.root->AttachToHost(*scene.host); }},
        {"given another model",
         false,
         [](const Scene& scene) { scene.grid->SetModel(scene.otherModel); },
         [](const Scene& scene) { scene.grid->SetModel(scene.model); }},
    }};
    for (const bool multiline : {true, false})
    {
        for (const Step& step : steps)
        {
            const std::string context = std::string(multiline ? "multiline grid " : "single-line grid ") + step.name;
            const auto require        = [&context](bool condition, const char* what) { Require(condition, (context + ": " + what).c_str()); };
            DistinctMultilineGridModel model(rowCount + 4u, columnCount, multiline);
            DistinctMultilineGridModel otherModel(rowCount + 4u, columnCount, multiline);
            auto root = std::make_unique<HostDetachingPanel>();
            Scene scene{.root = root.get(), .host = &window.Host(), .model = &model, .otherModel = &otherModel};
            scene.outer = root->AddChild<Panel>();
            scene.pages = scene.outer->AddChild<PageHost>();
            scene.pages->SetBounds(D2D1::RectF(0.0f, 0.0f, 760.0f, 640.0f));
            window.Host().SetRoot(std::move(root));
            auto page  = std::make_unique<Panel>();
            scene.grid = page->AddChild<Grid>();
            scene.grid->SetBounds(D2D1::RectF(
                10.0f, 10.0f, 10.0f + (static_cast<float>(columnCount) * 110.0f) + 20.0f, 10.0f + 30.0f + (static_cast<float>(rowCount) * rowHeight) + 20.0f));
            scene.grid->SetHeaderHeightDip(30.0f);
            scene.grid->SetRowHeightDip(rowHeight);
            scene.grid->SetLineClamp(2u);
            scene.grid->SetModel(&model);
            scene.pages->SetPage(std::move(page));
            const auto first = CaptureAttachedHostWindowBitmap(window, "grid before it stops painting");
            const auto cold  = scene.grid->DebugGetTextLayoutStatistics(); // What a first paint lays out and keeps.
            // A scroll a row down and back leaves entries released by the paints between, whose string storage later keys
            // reuse (the scratch string of an omitted tail takes some): what a grid that has painted for a while holds.
            for (const float offset : {rowHeight, 0.0f})
            {
                scene.grid->DebugSetScrollOffsets(offset, 0.0f);
                static_cast<void>(CaptureAttachedHostWindowBitmap(window, "grid scrolled a row and back"));
            }
            const auto held = scene.grid->DebugGetTextLayoutStatistics();
            require(held.retainedLayouts == cold.retainedLayouts && held.retainedLayouts > 0u && held.capacity > 0u && held.textUnits > 0u &&
                        held.tableBytes > 0u && (held.displayCapacity > 0u) == multiline && held.ellipsis == multiline,
                    "a painted grid holds layouts and their storage");

            step.stop(scene);
            RequireNoRetainedGridLayouts(scene.grid->DebugGetTextLayoutStatistics(), (context + ": it returns every layout, string and table").c_str());
            if (step.paintedWhileStopped)
            {
                static_cast<void>(CaptureAttachedHostWindowBitmap(window, "grid while stopped"));
                const auto stopped = scene.grid->DebugGetTextLayoutStatistics();
                RequireNoRetainedGridLayouts(stopped, (context + ": nothing rebuilds them while the window paints around it").c_str());
                require(stopped.lookups == held.lookups, "a grid that is not painted asks for no layout");
            }
            step.resume(scene);
            const auto beforePaint = scene.grid->DebugGetTextLayoutStatistics();
            const auto shown       = CaptureAttachedHostWindowBitmap(window, "grid painting again");
            const auto rebuilt     = scene.grid->DebugGetTextLayoutStatistics();
            require(rebuilt.layoutCreations - beforePaint.layoutCreations == cold.layoutCreations, "painting again lays out only what its first paint did");
            require(rebuilt.retainedLayouts == cold.retainedLayouts && rebuilt.capacity == cold.capacity && rebuilt.displayCapacity == cold.displayCapacity,
                    "painting again keeps the layouts of what it shows in tables of the size its first paint needed");
            require(shown.bgraPixels == first.bgraPixels, "painting again draws the pixels of the first paint");
            static_cast<void>(CaptureAttachedHostWindowBitmap(window, "grid repainted"));
            require(scene.grid->DebugGetTextLayoutStatistics().layoutCreations == rebuilt.layoutCreations, "a repaint of unchanged cells lays nothing out");
            window.Host().SetRoot(nullptr);
        }
    }
}

// A tab page that is not selected is hidden by its tab control: its grid (the page itself, or inside a panel page)
// returns its layouts when the selection moves away and lays out what it shows when it returns.
void TestGridInAnUnselectedTabReleasesItsLayouts()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 static_cast<int>(window.Host().DipsToPixels(760.0f)),
                 static_cast<int>(window.Host().DipsToPixels(640.0f)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    DistinctMultilineGridModel model(6u, 4u);
    auto root  = std::make_unique<Panel>();
    auto* tabs = root->AddChild<TabControl>();
    tabs->SetBounds(D2D1::RectF(10.0f, 10.0f, 750.0f, 630.0f));
    auto* pageGrid  = tabs->AddTab<Grid>(L"Grid page");
    auto* panel     = tabs->AddTab<Panel>(L"Panel page");
    auto* panelGrid = panel->AddChild<Grid>();
    panelGrid->SetBounds(D2D1::RectF(20.0f, 60.0f, 500.0f, 400.0f));
    for (Grid* grid : {pageGrid, panelGrid})
    {
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(48.0f);
        grid->SetLineClamp(2u);
        grid->SetModel(&model);
    }
    window.Host().SetRoot(std::move(root));
    const auto paintSelected = [&](const char* context) { static_cast<void>(CaptureAttachedHostWindowBitmap(window, context)); };
    tabs->SetSelectedIndex(0u);
    paintSelected("the grid page is selected");
    const auto pageHeld = pageGrid->DebugGetTextLayoutStatistics();
    Require(pageHeld.retainedLayouts > 0u && pageHeld.displayCapacity > 0u && pageHeld.textUnits > 0u,
            "the selected grid page holds layouts and their storage");
    RequireNoRetainedGridLayouts(panelGrid->DebugGetTextLayoutStatistics(), "a grid on a page that was never selected holds none");
    tabs->SetSelectedIndex(1u);
    RequireNoRetainedGridLayouts(pageGrid->DebugGetTextLayoutStatistics(), "selecting another tab returns the layouts of the grid page");
    paintSelected("the panel page is selected");
    const auto panelHeld = panelGrid->DebugGetTextLayoutStatistics();
    Require(panelHeld.retainedLayouts > 0u && panelHeld.displayCapacity > 0u && panelHeld.textUnits > 0u,
            "the grid on the selected panel page holds layouts and their storage");
    RequireNoRetainedGridLayouts(pageGrid->DebugGetTextLayoutStatistics(), "the unselected grid page keeps none while the window paints");
    tabs->SetSelectedIndex(0u);
    RequireNoRetainedGridLayouts(panelGrid->DebugGetTextLayoutStatistics(), "selecting another tab returns the layouts of the grid on the panel page");
    const uint64_t creations = pageGrid->DebugGetTextLayoutStatistics().layoutCreations;
    paintSelected("the grid page is selected again");
    const auto pageBack = pageGrid->DebugGetTextLayoutStatistics();
    Require(pageBack.retainedLayouts == pageHeld.retainedLayouts && pageBack.capacity == pageHeld.capacity &&
                pageBack.displayCapacity == pageHeld.displayCapacity,
            "selecting the grid page again keeps the layouts of what it shows in tables of the size its first paint needed");
    Require(pageBack.layoutCreations - creations == pageHeld.layoutCreations, "selecting the grid page again lays out what its first paint did");
    window.Host().SetRoot(nullptr);
}

// A leading-aligned single-line caption far longer than its cell shapes only the prefix that overflows the cell (the
// rest is clipped away) and paints exactly like a twin shaped whole that still overflows the cell; a caption the table
// keeps repaints without laying anything out.
void TestGridSingleLineOversizedCaptionShapesOnlyItsVisiblePrefix()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    SingleCellGridModel model(GridCellData{});
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetRowHeightDip(28.0f);
    grid->SetModel(&model);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    std::wstring huge;
    while (huge.size() < 100000u)
        huge += L"mot suivant très long ";
    const auto capture = [&](std::wstring text, const char* context)
    {
        GridCellData cell{};
        cell.text = std::move(text);
        model     = SingleCellGridModel(cell);
        grid->NotifyDataChanged();
        const auto before = grid->DebugGetTextLayoutStatistics();
        auto bitmap       = CaptureAttachedHostWindowBitmap(window, context);
        const auto after  = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid single-line caption (" << context << "): shaped " << after.shapedUnits - before.shapedUnits << " UTF-16 units\n";
        return std::pair(std::move(bitmap), after.shapedUnits - before.shapedUnits);
    };
    const auto [hugeBitmap, hugeShaped] = capture(huge, "100,000 units");
    const auto [twinBitmap, twinShaped] = capture(huge.substr(0u, 110u), "110-unit twin, shaped whole");
    const uint64_t before               = grid->DebugGetTextLayoutStatistics().layoutCreations;
    static_cast<void>(CaptureAttachedHostWindowBitmap(window, "110-unit twin, repaint"));
    const uint64_t repainted = grid->DebugGetTextLayoutStatistics().layoutCreations - before;
    Require(hugeShaped < 5000u, "a 100,000-unit single-line caption shapes only the prefix its cell can show");
    Require(repainted == 0u, "a retained single-line caption repaints without laying anything out");
    Require(hugeBitmap.bgraPixels == twinBitmap.bgraPixels, "the shaped prefix paints what a caption shaped whole paints");
    static_cast<void>(twinShaped);
}

// A value far longer than any cell can show shapes only a prefix: paint cost no longer grows with the value, and the
// cell paints exactly like a much shorter twin with the same visible start.
void TestGridMultilineOversizedValueShapesOnlyItsVisiblePrefix()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    SingleCellGridModel model(GridCellData{});
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetRowHeightDip(64.0f);
    grid->SetLineClamp(2u);
    grid->SetModel(&model);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    std::wstring huge;
    while (huge.size() < 100000u)
        huge += L"mot suivant très long ";
    const auto capture = [&](std::wstring text, const char* context)
    {
        GridCellData cell{};
        cell.text      = std::move(text);
        cell.multiline = true;
        model          = SingleCellGridModel(cell);
        grid->NotifyDataChanged();
        const auto before = grid->DebugGetTextLayoutStatistics();
        auto bitmap       = CaptureAttachedHostWindowBitmap(window, context);
        const auto after  = grid->DebugGetTextLayoutStatistics();
        const auto paints = std::max<uint64_t>(1u, after.layoutCreations - before.layoutCreations);
        const auto shaped = after.shapedUnits - before.shapedUnits;
        std::cout << "Grid oversized multiline value (" << context << "): shaped " << shaped << " UTF-16 units in " << paints << " layouts\n";
        return std::pair(std::move(bitmap), shaped);
    };
    // The layout is kept under the prefix it shaped, so a repaint of the long value lays nothing out again.
    const auto repaintCreations = [&](const char* context)
    {
        const auto before = grid->DebugGetTextLayoutStatistics();
        static_cast<void>(CaptureAttachedHostWindowBitmap(window, context));
        return grid->DebugGetTextLayoutStatistics().layoutCreations - before.layoutCreations;
    };
    // A clamp far beyond what the 64-DIP row can show sizes shaping by the lines that fit, not by the clamp.
    for (const uint32_t lineClamp : {2u, 1u, 1000u})
    {
        grid->SetLineClamp(lineClamp);
        const std::string clampName         = "clamp " + std::to_string(lineClamp);
        const auto [hugeBitmap, hugeShaped] = capture(huge, ("100,000 units, " + clampName).c_str());
        const uint64_t repainted            = repaintCreations("100,000 units, repaint");
        // Another long value that starts the same way is the same prefix to shape, so it shares the retained layout.
        const auto [otherBitmap, otherShaped] = capture(huge.substr(0u, 90000u) + L" et une autre fin", ("another long value, " + clampName).c_str());
        const auto [twinBitmap, twinShaped]   = capture(huge.substr(0u, 3000u), ("3,000-unit twin, " + clampName).c_str());
        std::cout << "Grid oversized multiline value, " << clampName << ": repaint lays out " << repainted << " layouts, another long value shapes "
                  << otherShaped << " units\n";
        Require(hugeShaped < 20000u, "a 100,000-unit value shapes only a bounded prefix per paint, wrapped, on one line or with a huge clamp");
        Require(repainted == 0u, "a repaint of a 100,000-unit value lays nothing out again");
        Require(otherShaped == 0u && otherBitmap.bgraPixels == hugeBitmap.bgraPixels, "another long value with the same start shares the retained layout");
        Require(hugeBitmap.bgraPixels == twinBitmap.bgraPixels, "the shaped prefix paints the same lines and omission marker as a short twin");
        static_cast<void>(twinShaped);
    }

    // A very wide cell shows (and shapes) more than 4,096 units in three lines; it still keeps its layout.
    const std::array<GridColumnLayoutEntry, 1> wideColumns{{{L"status", 0u, 3000.0f}}};
    grid->ApplyColumnLayout(wideColumns);
    grid->SetLineClamp(3u);
    const auto [wideBitmap, wideShaped] = capture(huge, "100,000 units in a 3,000-DIP cell");
    const uint64_t wideRepainted        = repaintCreations("100,000 units in a 3,000-DIP cell, repaint");
    std::cout << "Grid oversized multiline value in a 3,000-DIP cell: shaped " << wideShaped << " units, repaint lays out " << wideRepainted << " layouts\n";
    Require(wideShaped > 4096u && wideRepainted == 0u, "a wide cell keeps a layout whose shaped text is longer than 4,096 units");
    static_cast<void>(wideBitmap);
}

// The omission marker ends the text in that text's own direction: in a left-to-right cell it sits after the last
// word of Latin text (right) but at the left end of Arabic text, which reads right to left.
void TestGridMultilineOmissionMarkerFollowsTheTextDirection()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    SingleCellGridModel model(GridCellData{});
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f));
    grid->SetHeaderHeightDip(30.0f);
    grid->SetRowHeightDip(40.0f);
    grid->SetLineClamp(1u);
    grid->SetModel(&model);
    const std::array<GridColumnLayoutEntry, 1> columns{{{L"status", 0u, 300.0f}}};
    grid->ApplyColumnLayout(columns);
    window.Host().SetRoot(std::move(root));
    const auto capture = [&](const wchar_t* text, const char* context)
    {
        GridCellData cell{};
        cell.text      = text;
        cell.multiline = true;
        model          = SingleCellGridModel(cell);
        grid->NotifyDataChanged();
        return CaptureAttachedHostWindowBitmap(window, context);
    };
    const auto empty = capture(L"", "empty cell for the omission marker");
    struct Placement
    {
        UINT left            = UINT_MAX;
        UINT right           = 0u;
        uint64_t shiftedInk  = 0u; // Pixels that differ from the unmarked twin inside the twin's own ink span.
        uint64_t markerAfter = 0u; // Differing pixels right of that span.
    };
    const auto measure = [&](const wchar_t* marked, const wchar_t* unmarked, const char* name)
    {
        const auto withMarker = capture(marked, name);
        const auto twin       = capture(unmarked, name);
        Placement placement;
        for (UINT y = 0u; y < twin.heightPx; ++y)
            for (UINT x = 0u; x < twin.widthPx; ++x)
                if (CaptureBgra(twin, x, y) != CaptureBgra(empty, x, y))
                {
                    placement.left  = std::min(placement.left, x);
                    placement.right = std::max(placement.right, x);
                }
        Require(placement.left < placement.right, "the unmarked twin paints its text");
        const UINT margin = static_cast<UINT>(window.Host().DipsToPixels(3.0f));
        for (UINT y = 0u; y < twin.heightPx; ++y)
            for (UINT x = 0u; x < twin.widthPx; ++x)
                if (CaptureBgra(withMarker, x, y) != CaptureBgra(twin, x, y))
                {
                    if (x + margin < placement.right)
                        ++placement.shiftedInk;
                    else if (x > placement.right)
                        ++placement.markerAfter;
                }
        std::cout << "Grid omission marker (" << name << "): twin ink " << placement.left << ".." << placement.right << ", changed inside "
                  << placement.shiftedInk << ", changed after " << placement.markerAfter << '\n';
        return placement;
    };
    const Placement latin = measure(L"Bonjour tout le monde\nsuite masquée", L"Bonjour tout le monde", "Latin text");
    Require(latin.shiftedInk == 0u && latin.markerAfter > 0u, "a Latin omission marker follows the text on its right");
    const Placement arabic = measure(L"مرحبا بالعالم الجميل\nالسطر الثاني", L"مرحبا بالعالم الجميل", "Arabic text");
    Require(arabic.shiftedInk > 0u, "an Arabic omission marker sits at the left end, where that text reads to");
    // An emoji (a surrogate pair, U+1F4F7) has no direction: the text it ends still reads right to left.
    const Placement arabicEmoji =
        measure(L"مرحبا بالعالم الجميل \xD83D\xDCF7\nالسطر الثاني", L"مرحبا بالعالم الجميل \xD83D\xDCF7", "Arabic text ending in an emoji");
    Require(arabicEmoji.shiftedInk > 0u, "an Arabic omission marker after an emoji still sits at the left end");
}

void TestMultilineButtonPaintUsesMultipleTextRows()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 360, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    auto theme          = window.Host().GetTheme();
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>();
    button->SetBounds(D2D1::RectF(20.0f, 20.0f, 180.0f, 180.0f));
    window.Host().SetRoot(std::move(root));
    const auto empty = CaptureAttachedHostWindowBitmap(window, "empty button reference capture");
    button->SetText(L"Conserver les deux versions du document et poursuivre cette opération");
    const auto single = CaptureAttachedHostWindowBitmap(window, "single-line button capture");
    button->SetMultiline(true);
    const auto wrapped  = CaptureAttachedHostWindowBitmap(window, "wrapped French button capture");
    const auto textRows = [&](const auto& capture)
    {
        UINT rows         = 0;
        const auto left   = static_cast<UINT>(window.Host().DipsToPixels(34.0f));
        const auto right  = static_cast<UINT>(window.Host().DipsToPixels(166.0f));
        const auto top    = static_cast<UINT>(window.Host().DipsToPixels(30.0f));
        const auto bottom = static_cast<UINT>(window.Host().DipsToPixels(170.0f));
        for (UINT y = top; y < bottom; ++y)
        {
            for (UINT x = left; x < right; ++x)
            {
                if (CaptureBgra(capture, x, y) != CaptureBgra(empty, x, y))
                {
                    ++rows;
                    break;
                }
            }
        }
        return rows;
    };
    const UINT singleRows = textRows(single);
    Require(singleRows > 0 && textRows(wrapped) > singleRows * 2, "wrapped French button paints more than two single-line text heights");
}

void TestSharedTransientSurfaceRendersOrdinaryPressedAndHighContrastPolicies()
{
    using namespace DxUi;

    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 640, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    ThemePalette ordinary    = MakeAnimatedTestThemePalette(true);
    ordinary.overlayMaterial = OverlayMaterial::Solid;
    window.Host().SetTheme(ordinary);
    auto root                          = std::make_unique<TransientSurfaceProbe>();
    TransientSurfaceProbe* const probe = root.get();
    window.Host().SetRoot(std::move(root));

    const WindowHostBitmapCapture ordinaryCapture = CaptureAttachedHostWindowBitmap(window, "ordinary shared transient-surface capture succeeds");
    Require(CaptureAlpha(ordinaryCapture, 320u, 100u) > 240u, "ordinary shared transient surface has an opaque center");
    Require(CaptureAlpha(ordinaryCapture, 0u, 0u) == 0u, "ordinary shared transient surface preserves transparent outer pixels");

    WindowHostBitmapCapture backdropCapture{};
    backdropCapture.widthPx  = static_cast<UINT>(std::lround(window.Host().DipsToPixels(480.0f)));
    backdropCapture.heightPx = static_cast<UINT>(std::lround(window.Host().DipsToPixels(100.0f)));
    backdropCapture.bgraPixels.resize(static_cast<size_t>(backdropCapture.widthPx) * static_cast<size_t>(backdropCapture.heightPx) * 4u);
    for (UINT y = 0u; y < backdropCapture.heightPx; ++y)
    {
        for (UINT x = 0u; x < backdropCapture.widthPx; ++x)
        {
            const size_t offset                     = ((static_cast<size_t>(y) * backdropCapture.widthPx) + x) * 4u;
            const bool alternate                    = ((x / 24u) + (y / 16u)) % 2u != 0u;
            backdropCapture.bgraPixels[offset]      = alternate ? 0x20u : 0xD0u;
            backdropCapture.bgraPixels[offset + 1u] = alternate ? 0xB0u : 0x30u;
            backdropCapture.bgraPixels[offset + 2u] = alternate ? 0xF0u : 0x40u;
            backdropCapture.bgraPixels[offset + 3u] = 0xFFu;
        }
    }
    probe->SetBackdropCapture(std::move(backdropCapture));

    ordinary.overlayMaterial = OverlayMaterial::Mica;
    window.Host().SetTheme(ordinary);
    const WindowHostBitmapCapture micaCapture = CaptureAttachedHostWindowBitmap(window, "Mica shared transient-surface capture succeeds");
    ordinary.overlayMaterial                  = OverlayMaterial::MicaAlt;
    window.Host().SetTheme(ordinary);
    const WindowHostBitmapCapture micaAltCapture = CaptureAttachedHostWindowBitmap(window, "MicaAlt shared transient-surface capture succeeds");
    ordinary.overlayMaterial                     = OverlayMaterial::Acrylic;
    window.Host().SetTheme(ordinary);
    const WindowHostBitmapCapture acrylicCapture = CaptureAttachedHostWindowBitmap(window, "Acrylic shared transient-surface capture succeeds");
    probe->ClearBackdrop();
    const WindowHostBitmapCapture acrylicWithoutBackdrop =
        CaptureAttachedHostWindowBitmap(window, "Acrylic shared transient surface falls back without a backdrop capture");
    Require(CaptureBgra(micaCapture, 320u, 100u) != CaptureBgra(micaAltCapture, 320u, 100u) &&
                CaptureBgra(micaAltCapture, 320u, 100u) != CaptureBgra(acrylicCapture, 320u, 100u) &&
                CaptureBgra(micaCapture, 320u, 100u) != CaptureBgra(acrylicCapture, 320u, 100u),
            "shared transient materials preserve visibly distinct Mica, MicaAlt, and Acrylic treatments");
    Require(CaptureBgra(acrylicCapture, 320u, 100u) != CaptureBgra(acrylicWithoutBackdrop, 320u, 100u),
            "shared transient Acrylic renders its captured app backdrop instead of only a translucent fill");
    Require(CaptureAlpha(micaCapture, 0u, 0u) == 0u && CaptureAlpha(micaAltCapture, 0u, 0u) == 0u && CaptureAlpha(acrylicCapture, 0u, 0u) == 0u,
            "app-rendered transient materials preserve transparent HWND gutters");

    ordinary.overlayMaterial = OverlayMaterial::Solid;
    window.Host().SetTheme(ordinary);
    probe->SetPressed(true);
    const WindowHostBitmapCapture pressedCapture = CaptureAttachedHostWindowBitmap(window, "pressed shared transient-surface capture succeeds");
    Require(CaptureBgra(pressedCapture, 320u, 100u) != CaptureBgra(ordinaryCapture, 320u, 100u),
            "pressed shared transient surface applies its canonical pressed overlay");

    ThemePalette highContrast = ordinary;
    highContrast.highContrast = true;
    window.Host().SetTheme(highContrast);
    probe->SetPressed(false);
    const WindowHostBitmapCapture highContrastCapture = CaptureAttachedHostWindowBitmap(window, "High Contrast shared transient-surface capture succeeds");
    Require(CaptureAlpha(highContrastCapture, 320u, 100u) > 240u, "High Contrast shared transient surface has an opaque solid center");
    Require(CaptureAlpha(highContrastCapture, 0u, 0u) == 0u, "High Contrast shared transient surface preserves transparent outer pixels");
    Require(CaptureAlpha(highContrastCapture, 80u, 50u) > 0u, "High Contrast shared transient surface uses a rectangular visible border");
    Require(CountVisiblePixels(ordinaryCapture) != CountVisiblePixels(highContrastCapture),
            "ordinary shadow/rounding and High Contrast solid geometry produce distinct captures");
}

void TestThroughputGraphBandsStayBelowHistoryLine()
{
    using namespace DxUi;

    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 640, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();

    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    auto root   = std::make_unique<Panel>();
    auto* graph = root->AddChild<ThroughputGraph>();
    graph->SetBounds(D2D1::RectF(40.0f, 20.0f, 600.0f, 180.0f));

    std::array<ThroughputGraphSample, 2u> samples{};
    samples[0].value          = 100.0;
    samples[0].hueDegrees     = 20.0f;
    samples[0].hueWeights[0]  = ThroughputGraphHueWeight{20.0f, 1.0, 0u};
    samples[0].hueWeightCount = 1u;
    samples[1].value          = 20.0;
    samples[1].hueDegrees     = 20.0f;
    samples[1].hueWeights[0]  = ThroughputGraphHueWeight{20.0f, 1.0, 0u};
    samples[1].hueWeightCount = 1u;
    graph->SetSamples(samples);
    window.Host().SetRoot(std::move(root));

    graph->SetPerStreamBands(false);
    const WindowHostBitmapCapture unfilled = CaptureAttachedHostWindowBitmap(window, "unfilled throughput graph capture succeeds");
    graph->SetPerStreamBands(true);
    const WindowHostBitmapCapture oneStream = CaptureAttachedHostWindowBitmap(window, "single-stream default throughput graph capture succeeds");
    ThroughputGraphDebugState state         = graph->GetDebugState();
    Require(! state.bandsActive && state.renderedGeometryCount == 1u, "default one-stream throughput graph uses one base-accent geometry instead of hue bands");
    Require(std::abs(state.bandFillAlpha - 0.22f) < 0.001f, "dark-theme throughput graph area fill remains translucent");

    graph->SetCurrentValueMarker(80.0, L"80 B/s");
    const WindowHostBitmapCapture currentValueMarker = CaptureAttachedHostWindowBitmap(window, "current-value throughput graph marker capture succeeds");
    graph->SetCurrentValueMarker(80.0, L"80 B/s", L"Remaining: 00:20");
    Require(graph->GetDebugState().currentValueTrailingLabelVisible, "throughput graph exposes its trailing current-value label in debug state");
    const WindowHostBitmapCapture currentValueMarkerWithEta =
        CaptureAttachedHostWindowBitmap(window, "current-value throughput graph marker with ETA capture succeeds");

    const auto pixelAtDip = [&](const WindowHostBitmapCapture& capture, float x, float y) noexcept
    {
        return CaptureBgra(
            capture, static_cast<UINT>(std::lround(window.Host().DipsToPixels(x))), static_cast<UINT>(std::lround(window.Host().DipsToPixels(y))));
    };

    Require(pixelAtDip(oneStream, 180.0f, 30.0f) == pixelAtDip(unfilled, 180.0f, 30.0f),
            "throughput graph band fill stays above-free on the high side of a descending history segment");
    Require(pixelAtDip(oneStream, 180.0f, 110.0f) != pixelAtDip(unfilled, 180.0f, 110.0f),
            "throughput graph band fill remains visible below a descending history segment");
    Require(pixelAtDip(oneStream, 400.0f, 120.0f) != pixelAtDip(unfilled, 400.0f, 120.0f),
            "throughput graph band fill reaches the history curve across the low-side half of a descending segment");
    Require(pixelAtDip(currentValueMarker, 520.0f, 64.0f) != pixelAtDip(oneStream, 520.0f, 64.0f),
            "throughput graph paints the current effective-bandwidth horizontal marker over the history area");

    uint64_t trailingLabelChangedPixels = 0u;
    const UINT trailingLeftPx           = static_cast<UINT>(std::lround(window.Host().DipsToPixels(450.0f)));
    const UINT trailingTopPx            = static_cast<UINT>(std::lround(window.Host().DipsToPixels(22.0f)));
    const UINT trailingRightPx          = std::min(currentValueMarker.widthPx, static_cast<UINT>(std::lround(window.Host().DipsToPixels(596.0f))));
    const UINT trailingBottomPx         = std::min(currentValueMarker.heightPx, static_cast<UINT>(std::lround(window.Host().DipsToPixels(46.0f))));
    for (UINT y = trailingTopPx; y < trailingBottomPx; ++y)
    {
        for (UINT x = trailingLeftPx; x < trailingRightPx; ++x)
        {
            if (CaptureBgra(currentValueMarker, x, y) != CaptureBgra(currentValueMarkerWithEta, x, y))
            {
                ++trailingLabelChangedPixels;
            }
        }
    }
    Require(trailingLabelChangedPixels > 0u, "throughput graph paints the trailing ETA in the right-aligned half of the current-value label row");

    samples[0].hueWeights[1]  = ThroughputGraphHueWeight{220.0f, 1.0, 1u};
    samples[0].hueWeightCount = 2u;
    samples[1].hueWeights[1]  = ThroughputGraphHueWeight{220.0f, 1.0, 1u};
    samples[1].hueWeightCount = 2u;
    graph->SetSamples(samples);
    const WindowHostBitmapCapture concurrent = CaptureAttachedHostWindowBitmap(window, "concurrent-stream default throughput graph capture succeeds");
    state                                    = graph->GetDebugState();
    Require(state.bandsActive && state.activeColorSlotCount == 2u && state.renderedGeometryCount == 1u && state.renderedQuadCount == 2u,
            "default concurrent throughput graph renders fixed-slot bands through one bounded raster clip geometry");
    Require(std::abs(state.bandFillAlpha - 0.22f) < 0.001f, "per-stream hue bands preserve the translucent area-fill alpha");
    Require(pixelAtDip(concurrent, 400.0f, 135.0f) != pixelAtDip(oneStream, 400.0f, 135.0f),
            "default concurrent throughput graph exposes a stream-colored band pixel");

    samples[0].hueWeightCount = 1u;
    samples[1].hueWeightCount = 1u;
    graph->SetSamples(samples);
    graph->SetRainbowMode(true);
    static_cast<void>(CaptureAttachedHostWindowBitmap(window, "single-stream Rainbow throughput graph capture succeeds"));
    state = graph->GetDebugState();
    Require(state.bandsActive && state.activeColorSlotCount == 1u && state.renderedGeometryCount == 1u,
            "Rainbow one-stream throughput graph uses its admitted color slot");

    theme.highContrast = true;
    window.Host().SetTheme(theme);
    static_cast<void>(CaptureAttachedHostWindowBitmap(window, "High Contrast throughput graph capture succeeds"));
    state = graph->GetDebugState();
    Require(! state.bandsActive && state.renderedGeometryCount == 1u, "High Contrast throughput graph suppresses hue bands and keeps one base geometry");
}

void TestThroughputGraphHueChurnPerformanceScenario()
{
    wchar_t enabled[2]{};
    if (GetEnvironmentVariableW(L"DXUI_GRAPH_PERF", enabled, static_cast<DWORD>(std::size(enabled))) != 1u || enabled[0] != L'1')
    {
        return;
    }

    using namespace DxUi;

    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 640, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();

    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    auto root   = std::make_unique<Panel>();
    auto* graph = root->AddChild<ThroughputGraph>();
    graph->SetBounds(D2D1::RectF(40.0f, 20.0f, 600.0f, 180.0f));
    graph->SetPerStreamBands(true);

    std::array<ThroughputGraphSample, 180u> samples{};
    for (size_t sampleIndex = 0u; sampleIndex < samples.size(); ++sampleIndex)
    {
        auto& sample          = samples[sampleIndex];
        sample.value          = 80.0 + static_cast<double>(sampleIndex % 80u);
        sample.hueDegrees     = static_cast<float>(sampleIndex * ThroughputGraphSample::kMaxHueWeights);
        sample.hueWeightCount = sample.hueWeights.size();
        for (size_t hueIndex = 0u; hueIndex < sample.hueWeights.size(); ++hueIndex)
        {
            sample.hueWeights[hueIndex] = ThroughputGraphHueWeight{
                static_cast<float>(sampleIndex * ThroughputGraphSample::kMaxHueWeights + hueIndex),
                1.0,
                static_cast<uint8_t>(hueIndex),
            };
        }
    }
    graph->SetSamples(samples);
    window.Host().SetRoot(std::move(root));

    // Warm device resources before measuring the five deterministic captures.
    static_cast<void>(CaptureAttachedHostWindowBitmap(window, "throughput graph hue-churn warmup capture succeeds"));
    constexpr uint64_t kExpectedLegacyGeometryCount = (samples.size() - 1u) * ThroughputGraphSample::kMaxHueWeights;
    for (uint64_t run = 0u; run < 5u; ++run)
    {
        const auto startedAt = std::chrono::steady_clock::now();
        static_cast<void>(CaptureAttachedHostWindowBitmap(window, "throughput graph hue-churn measured capture succeeds"));
        DxUi::Debug::Perf::Emit(L"dxui.throughput_graph.hue_churn_capture_us",
                                L"180x16",
                                DxUi::Debug::Perf::ElapsedUs(startedAt),
                                samples.size(),
                                kExpectedLegacyGeometryCount,
                                S_OK);
        const ThroughputGraphDebugState runState = graph->GetDebugState();
        DxUi::Debug::Perf::Emit(
            L"dxui.throughput_graph.band_render_us", L"180x16", runState.bandRenderDurationUs, runState.sampleCount, runState.renderedQuadCount, S_OK);
        DxUi::Debug::Perf::Emit(
            L"dxui.throughput_graph.band_geometry_count", L"180x16", 0u, runState.renderedGeometryCount, runState.activeColorSlotCount, S_OK);
    }
    const ThroughputGraphDebugState state = graph->GetDebugState();
    Require(state.bandsActive && state.sampleCount == samples.size() && state.renderedQuadCount == 2864u,
            "180x16 throughput graph churn emits the complete bounded quad workload");
    Require(state.activeColorSlotCount == ThroughputGraphSample::kMaxHueWeights && state.renderedGeometryCount <= ThroughputGraphSample::kMaxHueWeights,
            "180x16 throughput graph churn caps active slots and geometries at the admitted stream bound");
}

struct CoreControlScene
{
    CoreControlScene()                                   = default;
    CoreControlScene(const CoreControlScene&)            = delete;
    CoreControlScene(CoreControlScene&&)                 = default;
    CoreControlScene& operator=(const CoreControlScene&) = delete;
    CoreControlScene& operator=(CoreControlScene&&)      = default;

    std::unique_ptr<DxUi::Panel> root;
    ExposedButton* hoverButton    = nullptr;
    ExposedButton* pressedButton  = nullptr;
    ExposedButton* focusButton    = nullptr;
    DxUi::ProgressBar* marqueeBar = nullptr;
};

CoreControlScene BuildCoreControlScene()
{
    using namespace DxUi;

    CoreControlScene scene{};
    scene.root = std::make_unique<Panel>();

    auto* restButton     = scene.root->AddChild<ExposedButton>(L"Rest");
    scene.hoverButton    = scene.root->AddChild<ExposedButton>(L"Hover");
    scene.pressedButton  = scene.root->AddChild<ExposedButton>(L"Pressed");
    scene.focusButton    = scene.root->AddChild<ExposedButton>(L"Primary");
    auto* disabledButton = scene.root->AddChild<ExposedButton>(L"Disabled");
    auto* toggle         = scene.root->AddChild<Toggle>(L"Wi-Fi");
    auto* checkbox       = scene.root->AddChild<Checkbox>(L"Updates");
    auto* radio          = scene.root->AddChild<RadioButton>(L"Daily");
    auto* field          = scene.root->AddChild<TextField>(L"");
    auto* progress       = scene.root->AddChild<ProgressBar>();
    scene.marqueeBar     = scene.root->AddChild<ProgressBar>();

    restButton->SetBounds(D2D1::RectF(16.0f, 16.0f, 112.0f, 48.0f));
    scene.hoverButton->SetBounds(D2D1::RectF(124.0f, 16.0f, 220.0f, 48.0f));
    scene.pressedButton->SetBounds(D2D1::RectF(232.0f, 16.0f, 328.0f, 48.0f));
    scene.focusButton->SetBounds(D2D1::RectF(340.0f, 16.0f, 456.0f, 48.0f));
    scene.focusButton->SetPrimary(true);
    disabledButton->SetBounds(D2D1::RectF(468.0f, 16.0f, 580.0f, 48.0f));
    disabledButton->SetEnabled(false);

    toggle->SetBounds(D2D1::RectF(16.0f, 70.0f, 210.0f, 102.0f));
    toggle->SetChecked(true);
    checkbox->SetBounds(D2D1::RectF(16.0f, 110.0f, 210.0f, 142.0f));
    checkbox->SetIndeterminate(true);
    radio->SetBounds(D2D1::RectF(16.0f, 150.0f, 210.0f, 182.0f));
    radio->SetChecked(true);

    field->SetBounds(D2D1::RectF(232.0f, 70.0f, 580.0f, 102.0f));
    field->SetPlaceholder(L"Search");
    progress->SetBounds(D2D1::RectF(232.0f, 116.0f, 580.0f, 138.0f));
    progress->SetValue(0.62);
    scene.marqueeBar->SetBounds(D2D1::RectF(232.0f, 150.0f, 580.0f, 172.0f));
    scene.marqueeBar->SetIndeterminate(true);

    return scene;
}

std::unique_ptr<DxUi::Panel> BuildAnimatedPage(std::wstring title, const D2D1_RECT_F& heroBounds, std::wstring_view heroKey)
{
    using namespace DxUi;

    auto page    = std::make_unique<Panel>();
    auto* label  = page->AddChild<Label>(std::move(title));
    auto* button = page->AddChild<Button>(L"Open");
    label->SetBounds(D2D1::RectF(12.0f, 12.0f, 240.0f, 40.0f));
    button->SetBounds(heroBounds);
    button->SetPrimary(true);
    button->SetConnectedAnimationKey(std::wstring(heroKey));
    return page;
}

void TestProgressBarReducedMotionIndeterminateCaptureIsStatic()
{
    using namespace DxUi;

    AttachedHostWindow window;
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 480, 200, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    ThemePalette theme  = MakeDefaultThemePalette(false);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    auto root = std::make_unique<Panel>();
    auto* bar = root->AddChild<ProgressBar>();
    bar->SetBounds(D2D1::RectF(20.0f, 30.0f, 260.0f, 46.0f));
    bar->SetIndeterminate(true);
    window.Host().SetRoot(std::move(root));

    const WindowHostBitmapCapture first = CaptureAttachedHostWindowBitmap(window, "reduced-motion indeterminate progress capture succeeds");
    Require(! window.Host().DebugHasActiveAnimationSubscription(), "reduced-motion indeterminate progress leaves the host animation timer idle");

    // Explicit ticks and a pump that outlives several timer intervals leave the captured frame unchanged.
    static_cast<void>(bar->Tick(window.Host(), 300u));
    static_cast<void>(bar->Tick(window.Host(), 1300u));
    Sleep(40);
    const WindowHostBitmapCapture second = CaptureAttachedHostWindowBitmap(window, "repeated reduced-motion indeterminate progress capture succeeds");
    Require(first.widthPx == second.widthPx && first.heightPx == second.heightPx && first.bgraPixels == second.bgraPixels,
            "reduced-motion indeterminate progress captures the same frame every time");

    // The resting segment spans 30%..70% of the track: [92, 188] DIP on the [20, 260] DIP track.
    const auto px          = [&](float dip) { return static_cast<UINT>(window.Host().DipsToPixels(dip)); };
    const UINT y           = px(38.0f);
    const uint32_t segment = CaptureBgra(first, px(140.0f), y);
    Require(CaptureBgra(first, px(100.0f), y) == segment && CaptureBgra(first, px(180.0f), y) == segment,
            "the resting indeterminate segment fills its centered span");
    Require(CaptureBgra(first, px(50.0f), y) != segment && CaptureBgra(first, px(230.0f), y) != segment,
            "the track outside the resting indeterminate segment stays unfilled");
}

void TestDxUiCoreControlsDarkVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ThemePalette theme  = MakeAnimatedTestThemePalette(true);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    CoreControlScene scene = BuildCoreControlScene();
    window.Host().SetRoot(std::move(scene.root));
    scene.hoverButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->SetPressed(true);
    window.Host().SetFocusControl(scene.focusButton);
    static_cast<void>(scene.marqueeBar->Tick(window.Host(), 250u));

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "dark core controls visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("dark core controls visual baseline matches", L"core_controls_dark.png", capture);
}

void TestDxUiCoreControlsLightVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ThemePalette theme  = MakeAnimatedTestThemePalette(false);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    CoreControlScene scene = BuildCoreControlScene();
    window.Host().SetRoot(std::move(scene.root));
    scene.hoverButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->SetPressed(true);
    window.Host().SetFocusControl(scene.focusButton);
    static_cast<void>(scene.marqueeBar->Tick(window.Host(), 250u));

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "light core controls visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("light core controls visual baseline matches", L"core_controls_light.png", capture);
}

void TestDxUiHighContrastVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ThemePalette theme  = MakeAnimatedTestThemePalette(false);
    theme.highContrast  = true;
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);

    CoreControlScene scene = BuildCoreControlScene();
    window.Host().SetRoot(std::move(scene.root));
    scene.hoverButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->OnHoverChanged(window.Host(), true);
    scene.pressedButton->SetPressed(true);
    window.Host().SetFocusControl(scene.focusButton);

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "high-contrast visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("high-contrast visual baseline matches", L"core_controls_high_contrast.png", capture);
}

void TestDxUiPopupAndBarsVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTheme(MakeAnimatedTestThemePalette(true));

    auto root     = std::make_unique<Panel>();
    auto* menuBar = root->AddChild<MenuBar>();
    auto* toolbar = root->AddChild<Toolbar>();
    auto* combo   = root->AddChild<ComboBox>();
    auto* strip   = root->AddChild<StatusStrip>(L"Ready");

    menuBar->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 32.0f));
    menuBar->SetItems({
        MenuBarItem{.text = L"File", .mnemonic = L'F', .enabled = true},
        MenuBarItem{.text = L"Edit", .mnemonic = L'E', .enabled = true},
        MenuBarItem{.text = L"View", .mnemonic = L'V', .enabled = true},
    });

    toolbar->SetBounds(D2D1::RectF(12.0f, 44.0f, 252.0f, 84.0f));
    toolbar->AddButton(L"Refresh", L"\xE72C");
    toolbar->AddButton(L"Copy", L"\xE8C8");
    toolbar->AddButton(L"Delete", L"\xE74D");

    combo->SetBounds(D2D1::RectF(280.0f, 48.0f, 520.0f, 80.0f));
    combo->SetItems({
        ComboBox::Item{L"one", L"One"},
        ComboBox::Item{L"two", L"Two"},
        ComboBox::Item{L"three", L"Three"},
        ComboBox::Item{L"four", L"Four"},
    });
    combo->SetSelectedIndex(1u);

    strip->SetBounds(D2D1::RectF(0.0f, 178.0f, 640.0f, 200.0f));
    strip->SetSections({
        StatusStrip::Section{.text = L"Ready", .widthDip = 0.0f},
        StatusStrip::Section{.text = L"UTF-8", .widthDip = 80.0f},
        StatusStrip::Section{.text = L"Ln 8, Col 42", .widthDip = 120.0f},
    });

    window.Host().SetRoot(std::move(root));

    Require(combo->OnMouseDown(window.Host(), D2D1::Point2F(500.0f, 64.0f), false, 0), "popup and bars baseline opens the combo popup");
    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "popup and bars visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("popup and bars visual baseline matches", L"popup_and_bars_dark.png", capture);
}

void TestDxUiPopupAndBarsAcrylicLightVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ThemePalette theme    = MakeAnimatedTestThemePalette(false);
    theme.overlayMaterial = OverlayMaterial::Acrylic;
    window.Host().SetTheme(theme);

    auto root     = std::make_unique<Panel>();
    auto* menuBar = root->AddChild<MenuBar>();
    auto* toolbar = root->AddChild<Toolbar>();
    auto* combo   = root->AddChild<ComboBox>();
    auto* strip   = root->AddChild<StatusStrip>(L"Ready");

    menuBar->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 32.0f));
    menuBar->SetItems({
        MenuBarItem{.text = L"File", .mnemonic = L'F', .enabled = true},
        MenuBarItem{.text = L"Edit", .mnemonic = L'E', .enabled = true},
        MenuBarItem{.text = L"View", .mnemonic = L'V', .enabled = true},
    });

    toolbar->SetBounds(D2D1::RectF(12.0f, 44.0f, 252.0f, 84.0f));
    toolbar->AddButton(L"Refresh", L"\xE72C");
    toolbar->AddButton(L"Copy", L"\xE8C8");
    toolbar->AddButton(L"Delete", L"\xE74D");

    combo->SetBounds(D2D1::RectF(280.0f, 48.0f, 520.0f, 80.0f));
    combo->SetItems({
        ComboBox::Item{L"system", L"System"},
        ComboBox::Item{L"on", L"On"},
        ComboBox::Item{L"off", L"Off"},
    });
    combo->SetSelectedIndex(1u);

    strip->SetBounds(D2D1::RectF(0.0f, 178.0f, 640.0f, 200.0f));
    strip->SetSections({
        StatusStrip::Section{.text = L"Ready", .widthDip = 0.0f},
        StatusStrip::Section{.text = L"UTF-8", .widthDip = 80.0f},
        StatusStrip::Section{.text = L"Ln 8, Col 42", .widthDip = 120.0f},
    });

    window.Host().SetRoot(std::move(root));

    Require(combo->OnMouseDown(window.Host(), D2D1::Point2F(500.0f, 64.0f), false, 0), "acrylic popup and bars baseline opens the combo popup");
    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "acrylic popup and bars visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("acrylic popup and bars visual baseline matches", L"popup_and_bars_acrylic_light.png", capture);
}

void TestDxUiPageTransitionVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTheme(MakeAnimatedTestThemePalette(true));

    auto root      = std::make_unique<Panel>();
    auto* pageHost = root->AddChild<PageHost>();
    pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 200.0f));
    window.Host().SetRoot(std::move(root));

    pageHost->SetPage(BuildAnimatedPage(L"Overview", D2D1::RectF(24.0f, 48.0f, 160.0f, 92.0f), L"hero"));
    pageHost->SetPage(BuildAnimatedPage(L"Details", D2D1::RectF(420.0f, 112.0f, 592.0f, 156.0f), L"hero"), L"hero");
    pageHost->DebugFreezeTransitionProgress(0.5f);

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "page transition visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("page transition visual baseline matches", L"page_transition_dark.png", capture);
}

void TestDxUiAdvancedControlsVisualBaseline()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetTheme(MakeAnimatedTestThemePalette(true));
    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 820, 280, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();

    auto root        = std::make_unique<Panel>();
    auto* tabControl = root->AddChild<TabControl>();
    tabControl->SetBounds(D2D1::RectF(16.0f, 16.0f, 392.0f, 220.0f));
    tabControl->AddTab<Label>(L"Overview", L"Overview page");
    tabControl->AddTab<Label>(L"Details", L"Details page");
    tabControl->AddTab<Label>(L"Activity", L"Activity page");
    tabControl->AddTab<Label>(L"Permissions", L"Permissions page");
    tabControl->AddTab<Label>(L"Diagnostics", L"Diagnostics page");
    tabControl->AddTab<Label>(L"Advanced", L"Advanced page");
    tabControl->SetTabClosable(4u, true);
    tabControl->SetSelectedIndex(3u);

    auto* horizontalSlider = root->AddChild<Slider>();
    horizontalSlider->SetBounds(D2D1::RectF(440.0f, 40.0f, 760.0f, 72.0f));
    horizontalSlider->SetValue(68.0);
    horizontalSlider->SetTickMarks({0.0, 25.0, 50.0, 75.0, 100.0});

    auto* rtlSlider = root->AddChild<Slider>();
    rtlSlider->SetBounds(D2D1::RectF(440.0f, 108.0f, 760.0f, 140.0f));
    rtlSlider->SetFlowDirection(FlowDirection::RightToLeft);
    rtlSlider->SetValue(28.0);
    rtlSlider->SetTickMarks({0.0, 25.0, 50.0, 75.0, 100.0});

    auto* verticalSlider = root->AddChild<Slider>();
    verticalSlider->SetOrientation(SliderOrientation::Vertical);
    verticalSlider->SetBounds(D2D1::RectF(780.0f, 28.0f, 812.0f, 220.0f));
    verticalSlider->SetValue(74.0);

    auto* rtlStack = root->AddChild<StackPanel>();
    rtlStack->SetBounds(D2D1::RectF(440.0f, 172.0f, 760.0f, 212.0f));
    rtlStack->SetOrientation(StackOrientation::Horizontal);
    rtlStack->SetGap(8.0f);
    rtlStack->SetFlowDirection(FlowDirection::RightToLeft);
    auto* primaryButton = rtlStack->AddChild<Button>(L"Primary");
    primaryButton->SetPrimary(true);
    auto* secondaryButton = rtlStack->AddChild<Button>(L"Secondary");
    rtlStack->SetChildExtent(primaryButton, 112.0f);
    rtlStack->SetChildExtent(secondaryButton, 112.0f);
    rtlStack->ApplyLayout();

    window.Host().SetRoot(std::move(root));

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmap(window, "advanced controls visual baseline capture succeeds");
    VerifyOrUpdateBaselineForTest("advanced controls visual baseline matches", L"advanced_controls_dark.png", capture);
}

void TestAttachedComboBoxPopupHoverDoesNotRepaintWhenHoveredItemStaysTheSame()
{
    using namespace DxUi;

    std::cerr << "    [TRACE] rendering combo hover: begin\n" << std::flush;
    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(12.0f, 12.0f, 192.0f, 40.0f));
    combo->SetItems({
        ComboBox::Item{L"one", L"One"},
        ComboBox::Item{L"two", L"Two"},
        ComboBox::Item{L"three", L"Three"},
        ComboBox::Item{L"four", L"Four"},
    });
    window.Host().SetRoot(std::move(root));
    std::cerr << "    [TRACE] rendering combo hover: root attached\n" << std::flush;

    std::cerr << "    [TRACE] rendering combo hover: before ShowWindow\n" << std::flush;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    std::cerr << "    [TRACE] rendering combo hover: after ShowWindow\n" << std::flush;
    MSG showMsg{};
    while (PeekMessageW(&showMsg, nullptr, 0, 0, PM_REMOVE) != FALSE)
    {
        wchar_t className[128]{};
        const BOOL isWindow = showMsg.hwnd != nullptr ? IsWindow(showMsg.hwnd) : FALSE;
        if (isWindow != FALSE)
        {
            static_cast<void>(GetClassNameW(showMsg.hwnd, className, static_cast<int>(std::size(className))));
        }
        std::cerr << "    [TRACE] rendering combo hover: dispatching initial msg=" << showMsg.message << " hwnd=" << showMsg.hwnd
                  << " isWindow=" << static_cast<int>(isWindow) << " class=";
        if (isWindow != FALSE)
        {
            std::wcerr << className;
        }
        else
        {
            std::cerr << "<invalid>";
        }
        std::cerr << " wp=" << showMsg.wParam << " lp=" << showMsg.lParam << '\n' << std::flush;
        if (DispatchQueuedMessageForTest(showMsg))
        {
            std::cerr << "    [TRACE] rendering combo hover: dispatched initial msg=" << showMsg.message << " hwnd=" << showMsg.hwnd << '\n' << std::flush;
        }
        else
        {
            std::cerr << "    [TRACE] rendering combo hover: skipped stale initial msg=" << showMsg.message << " hwnd=" << showMsg.hwnd << '\n' << std::flush;
        }
    }
    std::cerr << "    [TRACE] rendering combo hover: window shown\n" << std::flush;

    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONDOWN, 0, MAKELPARAM(172, 24), handled));
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(172, 24), handled));
    window.PumpMessages();
    std::cerr << "    [TRACE] rendering combo hover: popup toggle handled\n" << std::flush;
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "attached combo popup opens before hover repaint stability test");
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();
    std::cerr << "    [TRACE] rendering combo hover: initial redraw complete\n" << std::flush;
    const D2D1_RECT_F firstPopupItemRect  = combo->DebugGetPopupItemRect(0u, &window.Host());
    const D2D1_RECT_F secondPopupItemRect = combo->DebugGetPopupItemRect(1u, &window.Host());
    RequireRectHasArea(firstPopupItemRect, "attached combo popup exposes the first popup item geometry");
    RequireRectHasArea(secondPopupItemRect, "attached combo popup exposes the second popup item geometry");
    std::cerr << "    [TRACE] rendering combo hover: popup geometry ready\n" << std::flush;
    const D2D1_POINT_2F firstHoverPoint           = D2D1::Point2F(firstPopupItemRect.left + ((firstPopupItemRect.right - firstPopupItemRect.left) * 0.35f),
                                                                  (firstPopupItemRect.top + firstPopupItemRect.bottom) * 0.5f);
    const D2D1_POINT_2F nextRowHoverPoint         = D2D1::Point2F(secondPopupItemRect.left + ((secondPopupItemRect.right - secondPopupItemRect.left) * 0.35f),
                                                                  (secondPopupItemRect.top + secondPopupItemRect.bottom) * 0.5f);
    const D2D1_POINT_2F secondRowRepeatHoverPoint = D2D1::Point2F(secondPopupItemRect.left + ((secondPopupItemRect.right - secondPopupItemRect.left) * 0.65f),
                                                                  (secondPopupItemRect.top + secondPopupItemRect.bottom) * 0.5f);

#ifdef _DEBUG
    const uint64_t initialInvalidateCount = window.Host().DebugGetInvalidateCount();
    const uint64_t initialRenderCount     = window.Host().DebugGetRenderCount();
#endif
    Require(combo->DebugGetHoveredPopupIndex().has_value(), "attached combo popup opens with an initial hovered popup item");
    const size_t initialHoveredPopupIndex = combo->DebugGetHoveredPopupIndex().value();

    Require(combo->OnMouseMove(window.Host(), firstHoverPoint, 0), "attached combo popup first hover is handled");
    UpdateWindow(window.Hwnd());
    window.PumpMessages();
    std::cerr << "    [TRACE] rendering combo hover: first hover complete\n" << std::flush;
    Require(combo->DebugGetHoveredPopupIndex().has_value(), "attached combo popup first hover targets a popup item");
    Require(combo->DebugGetHoveredPopupIndex().value() == initialHoveredPopupIndex,
            "attached combo popup first hover stays on the initially highlighted popup item");

#ifdef _DEBUG
    Require(window.Host().DebugGetInvalidateCount() == initialInvalidateCount,
            "attached combo popup hover does not invalidate when the pointer stays on the initial popup item");
    Require(window.Host().DebugGetRenderCount() == initialRenderCount,
            "attached combo popup hover does not repaint when the pointer stays on the initial popup item");
#endif

    Require(combo->OnMouseMove(window.Host(), nextRowHoverPoint, 0), "attached combo popup next-row hover is handled");
    UpdateWindow(window.Hwnd());
    window.PumpMessages();
    std::cerr << "    [TRACE] rendering combo hover: second-row hover complete\n" << std::flush;
    Require(combo->DebugGetHoveredPopupIndex().has_value() && combo->DebugGetHoveredPopupIndex().value() != initialHoveredPopupIndex,
            "attached combo popup hover moves to the second popup item when the pointer changes rows");

#ifdef _DEBUG
    const uint64_t invalidateCountAfterRowChange = window.Host().DebugGetInvalidateCount();
    Require(invalidateCountAfterRowChange > initialInvalidateCount, "attached combo popup hover invalidates when the hovered popup item changes rows");
    const uint64_t renderCountAfterRowChange = window.Host().DebugGetRenderCount();
    Require(renderCountAfterRowChange > initialRenderCount, "attached combo popup hover repaints when the hovered popup item changes rows");
#endif

    const size_t hoveredPopupIndexAfterRowChange = combo->DebugGetHoveredPopupIndex().value();
    Require(combo->OnMouseMove(window.Host(), secondRowRepeatHoverPoint, 0), "attached combo popup repeated next-row hover is handled");
    UpdateWindow(window.Hwnd());
    window.PumpMessages();
    std::cerr << "    [TRACE] rendering combo hover: repeat hover complete\n" << std::flush;
    Require(combo->DebugGetHoveredPopupIndex().has_value() && combo->DebugGetHoveredPopupIndex().value() == hoveredPopupIndexAfterRowChange,
            "attached combo popup repeated next-row hover stays on the same popup item");

#ifdef _DEBUG
    Require(window.Host().DebugGetInvalidateCount() == invalidateCountAfterRowChange,
            "attached combo popup hover does not invalidate when the pointer stays on the second popup item");
    Require(window.Host().DebugGetRenderCount() == renderCountAfterRowChange,
            "attached combo popup hover does not repaint when the pointer stays on the second popup item");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached combo popup hover repaint stability does not cause DX host resize failures");
#endif
}

void TestAttachedGridPaintHandlesDegenerateScrollbarTracks()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 24.0f, 40.0f));

    LargeGridModel model(40u, 8u, 120.0f);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.hasVerticalScrollbar, "tiny attached grid still reports a vertical scrollbar when rows overflow");
    Require(metrics.hasHorizontalScrollbar, "tiny attached grid still reports a horizontal scrollbar when columns overflow");

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount,
            "tiny attached grid paints successfully when scrollbar tracks are smaller than the minimum thumb size");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "tiny attached grid paint does not cause DX host resize failures");
#endif
}

void TestGridIncludesBottomClippedTrailingRow()
{
    using namespace DxUi;

    Grid grid;
    grid.SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 170.0f));
    grid.SetRowHeightDip(28.0f);

    MultiRowGridModel model(10u);
    grid.SetModel(&model);

    Require(grid.GetVisibleRowCount() == 5u, "grid counts a bottom-clipped trailing row as visible");
    Require(grid.GetVisibleRowAt(3u).has_value() && grid.GetVisibleRowAt(3u).value() == 3u, "grid keeps the last fully visible row addressable");
    Require(grid.GetVisibleRowAt(4u).has_value() && grid.GetVisibleRowAt(4u).value() == 4u, "grid exposes a partially clipped trailing row as visible");
    const std::optional<D2D1_RECT_F> clippedRowRect = grid.GetVisibleRowRect(4u);
    Require(clippedRowRect.has_value(), "grid reports geometry for a bottom-clipped trailing row");
    Require(clippedRowRect->bottom > clippedRowRect->top, "grid bottom-clipped trailing row geometry has area");
}

class LargeIconBadgeGridModel final : public DxUi::IGridModel
{
public:
    LargeIconBadgeGridModel(size_t rowCount, size_t columnCount, float columnWidthDip)
        : _rowCount(rowCount),
          _columnCount(columnCount),
          _columnWidthDip(columnWidthDip)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnCount;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = std::format(L"state-{}", columnIndex);
        column.title    = std::format(L"State {}", columnIndex);
        column.widthDip = _columnWidthDip;
        column.kind     = (columnIndex % 3u == 0u) ? DxUi::GridColumnKind::StateImage : DxUi::GridColumnKind::Text;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        outCell.kind      = DxUi::GridCellKind::IconText;
        outCell.iconText  = (columnIndex % 2u == 0u) ? L"*" : L"!";
        outCell.text      = std::format(L"Plugin {}:{}", rowIndex, columnIndex);
        outCell.badgeText = (rowIndex % 2u == 0u) ? L"Beta" : L"Live";
        outCell.badgeTone = (columnIndex % 2u == 0u) ? DxUi::AdornmentTone::Info : DxUi::AdornmentTone::Warning;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }
        return static_cast<size_t>(rowId);
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex);
    }

private:
    size_t _rowCount      = 0u;
    size_t _columnCount   = 0u;
    float _columnWidthDip = 96.0f;
};

class CellDataStorageProbeGridModel final : public DxUi::IGridModel
{
public:
    CellDataStorageProbeGridModel(size_t rowCount, size_t columnCount, float columnWidthDip)
        : _rowCount(rowCount),
          _columnCount(columnCount),
          _columnWidthDip(columnWidthDip),
          _payload(512u, L'x')
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnCount;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"probe-" + std::to_wstring(columnIndex);
        column.title    = L"Probe " + std::to_wstring(columnIndex);
        column.widthDip = _columnWidthDip;
        return column;
    }

    void GetCellData(size_t /*rowIndex*/, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        ++_cellDataReadCount;
        if (outCell.text.capacity() < _payload.size())
        {
            ++_freshTextStorageCount;
        }

        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = _payload;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId);
    }

    void ResetProbe() const noexcept
    {
        _cellDataReadCount     = 0u;
        _freshTextStorageCount = 0u;
    }

    [[nodiscard]] uint64_t CellDataReadCount() const noexcept
    {
        return _cellDataReadCount;
    }

    [[nodiscard]] uint64_t FreshTextStorageCount() const noexcept
    {
        return _freshTextStorageCount;
    }

private:
    size_t _rowCount      = 0u;
    size_t _columnCount   = 0u;
    float _columnWidthDip = 96.0f;
    std::wstring _payload;
    mutable uint64_t _cellDataReadCount     = 0u;
    mutable uint64_t _freshTextStorageCount = 0u;
};

void TestAttachedLargeGridVisibleWorkStaysBoundedAfterScroll()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    LargeGridModel model(1'000'000u, 64u, 96.0f);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    for (int i = 0; i < 24; ++i)
    {
        Require(grid->OnMouseWheel(window.Host(), D2D1::Point2F(24.0f, 48.0f), -static_cast<float>(WHEEL_DELTA), 0),
                "attached large grid handles wheel scrolling");
    }

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 5u, "attached large grid keeps visible row count bounded after scrolling");
    Require(metrics.visibleColumnCount == 4u, "attached large grid keeps visible column count bounded after scrolling");
    Require(metrics.visibleCellCount == 20u, "attached large grid keeps visible cell work bounded after scrolling");
    Require(metrics.hasVerticalScrollbar, "attached large grid still reports a vertical scrollbar after scrolling");
    Require(metrics.hasHorizontalScrollbar, "attached large grid still reports a horizontal scrollbar after scrolling");

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached large grid repaints after scrolling");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large grid scrolling and repainting does not cause DX host resize failures");
#endif
}

void TestAttachedLargeGridPaintReusesCellDataStringStorage()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    CellDataStorageProbeGridModel model(1'000'000u, 64u, 96.0f);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    model.ResetProbe();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

    const uint64_t cellDataReads = model.CellDataReadCount();
    const uint64_t freshStorage  = model.FreshTextStorageCount();
    Require(cellDataReads > 1u, "attached large grid paint storage probe reads multiple visible cells");
    Require(freshStorage > 0u, "attached large grid paint storage probe observes initial text storage growth");
    Require(freshStorage < cellDataReads, "attached large grid paint reuses GridCellData text storage across visible cells");

#ifdef _DEBUG
    const uint64_t renderCountDelta = window.Host().DebugGetRenderCount() - initialRenderCount;
    Require(renderCountDelta > 0u, "attached large grid paint storage probe repaints");
    Require(freshStorage <= renderCountDelta, "attached large grid paint grows cell-data text storage at most once per repaint");
#endif
}

void TestAttachedLargeIconBadgeGridVisibleWorkStaysBoundedAfterScroll()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    LargeIconBadgeGridModel model(250'000u, 48u, 96.0f);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    for (int i = 0; i < 24; ++i)
    {
        Require(grid->OnMouseWheel(window.Host(), D2D1::Point2F(24.0f, 48.0f), -static_cast<float>(WHEEL_DELTA), 0),
                "attached large icon/badge grid handles wheel scrolling");
    }

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 5u, "attached large icon/badge grid keeps visible row count bounded after scrolling");
    Require(metrics.visibleColumnCount == 4u, "attached large icon/badge grid keeps visible column count bounded after scrolling");
    Require(metrics.visibleCellCount == 20u, "attached large icon/badge grid keeps visible cell work bounded after scrolling");
    Require(metrics.visibleIconCellCount == metrics.visibleCellCount, "attached large icon/badge grid reports bounded visible icon work");
    Require(metrics.visibleBadgeCellCount == metrics.visibleCellCount, "attached large icon/badge grid reports bounded visible badge work");
    Require(metrics.hasVerticalScrollbar, "attached large icon/badge grid still reports a vertical scrollbar after scrolling");
    Require(metrics.hasHorizontalScrollbar, "attached large icon/badge grid still reports a horizontal scrollbar after scrolling");

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached large icon/badge grid repaints after scrolling");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large icon/badge grid scrolling and repainting does not cause DX host resize failures");
#endif
}

void TestAttachedGridBottomScrollKeepsFirstVisibleRowFlushWithHeader()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 170.0f));
    grid->SetRowHeightDip(28.0f);

    MultiRowGridModel model(10u);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    bool reachedScrollEdge = false;
    for (int stepIndex = 0; stepIndex < 8 && ! reachedScrollEdge; ++stepIndex)
    {
        reachedScrollEdge = ! grid->OnMouseWheel(window.Host(), D2D1::Point2F(24.0f, 48.0f), -static_cast<float>(WHEEL_DELTA), 0);
    }
    Require(reachedScrollEdge, "attached grid bottom-alignment test reaches the bottom scroll edge");

    const std::optional<D2D1_RECT_F> headerRect = grid->GetVisibleColumnHeaderRect(0u);
    Require(headerRect.has_value(), "attached grid bottom-alignment test exposes the visible header rect");

    const std::optional<size_t> firstVisibleRowIndex = grid->GetVisibleRowAt(0u);
    Require(firstVisibleRowIndex.has_value(), "attached grid bottom-alignment test exposes the first visible row");
    Require(firstVisibleRowIndex.value() == 6u, "attached grid bottom-alignment test snaps the trailing viewport to the next full row boundary");

    const std::optional<D2D1_RECT_F> firstVisibleRowRect = grid->GetVisibleRowRect(firstVisibleRowIndex.value());
    Require(firstVisibleRowRect.has_value(), "attached grid bottom-alignment test exposes the first visible row rect");
    RequireFloatNear(firstVisibleRowRect->top,
                     headerRect->bottom,
                     0.01f,
                     "attached grid bottom-alignment test keeps the first visible row flush with the header at bottom scroll");

    const std::optional<D2D1_RECT_F> lastRowRect = grid->GetVisibleRowRect(9u);
    Require(lastRowRect.has_value(), "attached grid bottom-alignment test keeps the trailing row visible");
    Require(lastRowRect->bottom <= grid->GetBounds().bottom + 0.01f, "attached grid bottom-alignment test keeps the trailing row fully visible");
}

void TestAttachedLargeGridLongRunScrollingStaysBoundedWithoutResizeChurn()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    LargeGridModel model(1'000'000u, 64u, 96.0f);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    const D2D1_POINT_2F wheelPoint = D2D1::Point2F(24.0f, 48.0f);
#ifdef _DEBUG
    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    uint64_t lastRenderCount          = window.Host().DebugGetRenderCount();
#endif

    for (int chunkIndex = 0; chunkIndex < 12; ++chunkIndex)
    {
        for (int stepIndex = 0; stepIndex < 24; ++stepIndex)
        {
            Require(grid->OnMouseWheel(window.Host(), wheelPoint, -static_cast<float>(WHEEL_DELTA), 0),
                    "attached large grid handles sustained repeated wheel scrolling");
        }

        const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
        Require(metrics.visibleRowCount == 5u, "attached large grid long-run scrolling keeps visible row count bounded");
        Require(metrics.visibleColumnCount == 4u, "attached large grid long-run scrolling keeps visible column count bounded");
        Require(metrics.visibleCellCount == 20u, "attached large grid long-run scrolling keeps visible cell work bounded");
        Require(metrics.hasVerticalScrollbar, "attached large grid long-run scrolling keeps the vertical scrollbar active");
        Require(metrics.hasHorizontalScrollbar, "attached large grid long-run scrolling keeps the horizontal scrollbar active");

        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        window.PumpMessages();

#ifdef _DEBUG
        const uint64_t renderCount = window.Host().DebugGetRenderCount();
        Require(renderCount > lastRenderCount, "attached large grid long-run scrolling repaints after each sustained chunk");
        Require(window.Host().DebugGetResizeCount() == initialResizeCount, "attached large grid long-run scrolling does not churn swapchain resizes");
        Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large grid long-run scrolling does not cause DX host resize failures");
        lastRenderCount = renderCount;
#endif
    }
}

void TestAttachedLargeGroupedGridVisibleWorkStaysBoundedAfterScroll()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);

    GroupedGridModel model(2'000u);
    std::vector<GroupedGridModel::Group> groups;
    groups.reserve(100u);
    for (size_t groupIndex = 0; groupIndex < 100u; ++groupIndex)
    {
        groups.push_back(GroupedGridModel::Group{
            .stableId      = static_cast<uint64_t>(groupIndex + 1u),
            .title         = std::format(L"Group {:03}", groupIndex),
            .startRowIndex = groupIndex * 20u,
            .rowCount      = 20u,
        });
    }
    model.SetGroups(std::move(groups));
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    for (int i = 0; i < 24; ++i)
    {
        Require(grid->OnMouseWheel(window.Host(), D2D1::Point2F(24.0f, 48.0f), -static_cast<float>(WHEEL_DELTA), 0),
                "attached large grouped grid handles wheel scrolling");
    }

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount > 0u && metrics.visibleRowCount <= 7u, "attached large grouped grid keeps visible row count bounded after scrolling");
    Require(metrics.visibleGroupHeaderCount <= 2u, "attached large grouped grid keeps visible group-header count bounded after scrolling");
    Require(metrics.visibleColumnCount == 1u, "attached large grouped grid keeps visible column count bounded after scrolling");
    Require(metrics.visibleCellCount == metrics.visibleRowCount,
            "attached large grouped grid keeps visible cell work proportional to visible rows after scrolling");
    Require(metrics.hasVerticalScrollbar, "attached large grouped grid still reports a vertical scrollbar after scrolling");
    Require(! metrics.hasHorizontalScrollbar, "attached large grouped grid avoids a horizontal scrollbar when the single visible column fits");

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached large grouped grid repaints after scrolling");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large grouped grid scrolling and repainting does not cause DX host resize failures");
#endif
}

void TestAttachedLargeCheckboxGridVisibleWorkStaysBoundedAfterScroll()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    LargeCheckboxGridModel model(100'000u, 1u);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    for (int i = 0; i < 24; ++i)
    {
        Require(grid->OnMouseWheel(window.Host(), D2D1::Point2F(24.0f, 48.0f), -static_cast<float>(WHEEL_DELTA), 0),
                "attached large checkbox grid handles wheel scrolling");
    }

    const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
    Require(metrics.visibleRowCount == 5u, "attached large checkbox grid keeps visible row count bounded after scrolling");
    Require(metrics.visibleColumnCount == 2u, "attached large checkbox grid keeps visible column count bounded after scrolling");
    Require(metrics.visibleCellCount == 10u, "attached large checkbox grid keeps visible cell work bounded after scrolling");
    Require(metrics.hasVerticalScrollbar, "attached large checkbox grid still reports a vertical scrollbar after scrolling");

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached large checkbox grid repaints after scrolling");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large checkbox grid scrolling and repainting does not cause DX host resize failures");
#endif
}

void TestAttachedLargeGroupedGridLongRunScrollingStaysBoundedWithoutResizeChurn()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);

    GroupedGridModel model(2'000u);
    std::vector<GroupedGridModel::Group> groups;
    groups.reserve(100u);
    for (size_t groupIndex = 0; groupIndex < 100u; ++groupIndex)
    {
        groups.push_back(GroupedGridModel::Group{
            .stableId      = static_cast<uint64_t>(groupIndex + 1u),
            .title         = std::format(L"Group {:03}", groupIndex),
            .startRowIndex = groupIndex * 20u,
            .rowCount      = 20u,
        });
    }
    model.SetGroups(std::move(groups));
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    const D2D1_POINT_2F wheelPoint = D2D1::Point2F(24.0f, 48.0f);
#ifdef _DEBUG
    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    uint64_t lastRenderCount          = window.Host().DebugGetRenderCount();
#endif

    for (int chunkIndex = 0; chunkIndex < 12; ++chunkIndex)
    {
        for (int stepIndex = 0; stepIndex < 24; ++stepIndex)
        {
            Require(grid->OnMouseWheel(window.Host(), wheelPoint, -static_cast<float>(WHEEL_DELTA), 0),
                    "attached large grouped grid handles sustained repeated wheel scrolling");
        }

        const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
        Require(metrics.visibleRowCount > 0u && metrics.visibleRowCount <= 7u,
                "attached large grouped grid long-run scrolling keeps visible row count bounded");
        Require(metrics.visibleGroupHeaderCount <= 2u, "attached large grouped grid long-run scrolling keeps visible group-header count bounded");
        Require(metrics.visibleColumnCount == 1u, "attached large grouped grid long-run scrolling keeps visible column count bounded");
        Require(metrics.visibleCellCount == metrics.visibleRowCount,
                "attached large grouped grid long-run scrolling keeps visible cell work proportional to visible rows");
        Require(metrics.hasVerticalScrollbar, "attached large grouped grid long-run scrolling keeps the vertical scrollbar active");
        Require(! metrics.hasHorizontalScrollbar, "attached large grouped grid long-run scrolling avoids horizontal scrollbar churn");

        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        window.PumpMessages();

#ifdef _DEBUG
        const uint64_t renderCount = window.Host().DebugGetRenderCount();
        Require(renderCount > lastRenderCount, "attached large grouped grid long-run scrolling repaints after each sustained chunk");
        Require(window.Host().DebugGetResizeCount() == initialResizeCount, "attached large grouped grid long-run scrolling does not churn swapchain resizes");
        Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large grouped grid long-run scrolling does not cause DX host resize failures");
        lastRenderCount = renderCount;
#endif
    }
}

void TestAttachedLargeCheckboxGridLongRunScrollingStaysBoundedWithoutResizeChurn()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    grid->SetRowHeightDip(24.0f);

    LargeCheckboxGridModel model(100'000u, 1u);
    grid->SetModel(&model);
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    const D2D1_POINT_2F wheelPoint = D2D1::Point2F(24.0f, 48.0f);
#ifdef _DEBUG
    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    uint64_t lastRenderCount          = window.Host().DebugGetRenderCount();
#endif

    for (int chunkIndex = 0; chunkIndex < 12; ++chunkIndex)
    {
        for (int stepIndex = 0; stepIndex < 24; ++stepIndex)
        {
            Require(grid->OnMouseWheel(window.Host(), wheelPoint, -static_cast<float>(WHEEL_DELTA), 0),
                    "attached large checkbox grid handles sustained repeated wheel scrolling");
        }

        const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
        Require(metrics.visibleRowCount == 5u, "attached large checkbox grid long-run scrolling keeps visible row count bounded");
        Require(metrics.visibleColumnCount == 2u, "attached large checkbox grid long-run scrolling keeps visible column count bounded");
        Require(metrics.visibleCellCount == 10u, "attached large checkbox grid long-run scrolling keeps visible cell work bounded");
        Require(metrics.hasVerticalScrollbar, "attached large checkbox grid long-run scrolling keeps the vertical scrollbar active");

        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        window.PumpMessages();

#ifdef _DEBUG
        const uint64_t renderCount = window.Host().DebugGetRenderCount();
        Require(renderCount > lastRenderCount, "attached large checkbox grid long-run scrolling repaints after each sustained chunk");
        Require(window.Host().DebugGetResizeCount() == initialResizeCount, "attached large checkbox grid long-run scrolling does not churn swapchain resizes");
        Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached large checkbox grid long-run scrolling does not cause DX host resize failures");
        lastRenderCount = renderCount;
#endif
    }
}

void TestAttachedComboBoxPopupScrollingStaysStable()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));

    std::vector<ComboBox::Item> items;
    items.push_back(ComboBox::Item{L"long-entry", L"Very long popup entry that must keep the dropdown width stable while attached scrolling runs"});
    for (size_t index = 0u; index < 47u; ++index)
    {
        items.push_back(ComboBox::Item{std::format(L"value-{:02}", index), std::format(L"Item {:02}", index)});
    }
    combo->SetItems(std::move(items));
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    Require(combo->OnMouseDown(window.Host(), D2D1::Point2F(172.0f, 12.0f), false, 0), "attached combo opens for popup scrolling stability test");
    const D2D1_RECT_F openBounds = combo->GetHitBounds();
    const float openWidthDip     = openBounds.right - openBounds.left;
    Require(openBounds.bottom > combo->GetBounds().bottom, "attached combo popup is open before repeated wheel scrolling");

    const D2D1_POINT_2F firstVisibleItemPoint = D2D1::Point2F(16.0f, 42.0f);
    for (int i = 0; i < 24; ++i)
    {
        Require(combo->OnMouseWheel(window.Host(), firstVisibleItemPoint, -static_cast<float>(WHEEL_DELTA), 0),
                "attached combo popup handles repeated wheel scrolling");
    }

    const D2D1_RECT_F scrolledBounds = combo->GetHitBounds();
    Require(scrolledBounds.bottom > combo->GetBounds().bottom, "attached combo popup remains open after repeated wheel scrolling");
    RequireFloatNear(
        scrolledBounds.right - scrolledBounds.left, openWidthDip, 0.5f, "attached combo popup width remains stable after repeated wheel scrolling");

    Require(combo->OnMouseDown(window.Host(), firstVisibleItemPoint, false, 0),
            "attached combo click selects the first visible popup item after repeated scrolling");
    Require(combo->GetSelectedIndex().has_value() && combo->GetSelectedIndex().value() == 24u,
            "attached combo repeated scrolling changes which popup item the first visible row selects");

#ifdef _DEBUG
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached combo popup scrolling repaints after selection");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached combo popup scrolling and repainting does not cause DX host resize failures");
#endif
}

void TestAttachedComboBoxPopupLongRunScrollingStaysStable()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));

    std::vector<ComboBox::Item> items;
    items.push_back(ComboBox::Item{L"long-entry", L"Very long popup entry that must keep the dropdown width stable while attached scrolling runs"});
    for (size_t index = 0u; index < 47u; ++index)
    {
        items.push_back(ComboBox::Item{std::format(L"value-{:02}", index), std::format(L"Item {:02}", index)});
    }
    combo->SetItems(std::move(items));
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    Require(combo->OnMouseDown(window.Host(), D2D1::Point2F(172.0f, 12.0f), false, 0), "attached combo opens for long-run popup scrolling stability test");
    const D2D1_RECT_F openBounds = combo->GetHitBounds();
    const float openWidthDip     = openBounds.right - openBounds.left;
    Require(openBounds.bottom > combo->GetBounds().bottom, "attached combo popup is open before long-run scrolling");

    const D2D1_POINT_2F firstVisibleItemPoint = D2D1::Point2F(16.0f, 42.0f);
#ifdef _DEBUG
    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    uint64_t lastRenderCount          = window.Host().DebugGetRenderCount();
#endif

    for (int cycleIndex = 0; cycleIndex < 4; ++cycleIndex)
    {
        for (int stepIndex = 0; stepIndex < 12; ++stepIndex)
        {
            Require(combo->OnMouseWheel(window.Host(), firstVisibleItemPoint, -static_cast<float>(WHEEL_DELTA), 0),
                    "attached combo popup handles sustained downward wheel scrolling");
        }
        for (int stepIndex = 0; stepIndex < 10; ++stepIndex)
        {
            Require(combo->OnMouseWheel(window.Host(), firstVisibleItemPoint, static_cast<float>(WHEEL_DELTA), 0),
                    "attached combo popup handles sustained upward wheel scrolling");
        }

        const D2D1_RECT_F scrolledBounds = combo->GetHitBounds();
        Require(scrolledBounds.bottom > combo->GetBounds().bottom, "attached combo popup remains open during long-run scrolling");
        RequireFloatNear(scrolledBounds.right - scrolledBounds.left, openWidthDip, 0.5f, "attached combo popup width remains stable during long-run scrolling");

        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        window.PumpMessages();

#ifdef _DEBUG
        const uint64_t renderCount = window.Host().DebugGetRenderCount();
        Require(renderCount > lastRenderCount, "attached combo popup long-run scrolling repaints after each sustained cycle");
        Require(window.Host().DebugGetResizeCount() == initialResizeCount, "attached combo popup long-run scrolling does not churn swapchain resizes");
        Require(window.Host().DebugGetResizeFailureCount() == 0u, "attached combo popup long-run scrolling does not cause DX host resize failures");
        lastRenderCount = renderCount;
#endif
    }

    Require(combo->OnMouseDown(window.Host(), firstVisibleItemPoint, false, 0), "attached combo click selects the first visible item after long-run scrolling");
    Require(combo->GetSelectedIndex().has_value() && combo->GetSelectedIndex().value() > 0u,
            "attached combo long-run scrolling keeps the popup selection target away from the initial top item");
}

void TestAttachedHostSameSizeRepaintDoesNotResizeSwapChain()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"alpha");
    label->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 28.0f));
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();
#endif

    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "attached host repaint renders again without resizing");
    Require(window.Host().DebugGetResizeCount() == initialResizeCount, "attached host repaint at the same size does not call ResizeBuffers");
#endif

    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 420, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetResizeCount() == initialResizeCount + 1u, "attached host resize performs one swapchain resize");
#endif
}

void TestAttachedHostResizeDoesNotFlushD2DInWrongState()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"resize");
    label->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    const uint64_t initialFlushFailureCount = window.Host().DebugGetSwapChainPrepareD2DFlushFailureCount();
#endif

    SetWindowPos(window.Hwnd(), nullptr, 0, 0, 420, 240, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetSwapChainPrepareD2DFlushFailureCount() == initialFlushFailureCount,
            "attached host resize releases the D2D target without flushing in a wrong state");
#endif
}

void TestAttachedHostRecoversAfterSimulatedDeviceLoss()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();

    // Build a rich control tree so recovery exercises Grid, Tree, TextField, and Label painting
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 80.0f));
    MultiRowGridModel gridModel(3u);
    grid->SetModel(&gridModel);

    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 80.0f, 280.0f, 140.0f));
    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });
    tree->SetModel(&treeModel);

    auto* field = root->AddChild<TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(0.0f, 140.0f, 280.0f, 168.0f));

    auto* label = root->AddChild<Label>(L"beta");
    label->SetBounds(D2D1::RectF(0.0f, 168.0f, 280.0f, 196.0f));

    window.Host().SetRoot(std::move(root));

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    // Verify resources are populated after the initial render
    Require(window.Host().DebugHasD2DContext(), "device loss: D2D context exists before device loss");
    Require(window.Host().DebugHasFallbackBrush(), "device loss: fallback brush exists before device loss");
    Require(window.Host().DebugGetBrushCacheSize() > 0u, "device loss: brush cache is populated before device loss");
    const size_t textFormatCountBefore = window.Host().DebugGetConfiguredTextFormatCount();

    const uint64_t initialResizeCount = window.Host().DebugGetResizeCount();
    const uint64_t initialRenderCount = window.Host().DebugGetRenderCount();

    // Simulate device loss — caches must be cleared before recovery render
    window.Host().DebugSimulateDeviceLoss();

    Require(! window.Host().DebugHasD2DContext(), "device loss: D2D context is null after device loss");
    Require(! window.Host().DebugHasFallbackBrush(), "device loss: fallback brush is null after device loss");
    Require(window.Host().DebugGetBrushCacheSize() == 0u, "device loss: brush cache is empty after device loss");

    window.PumpMessages();
#endif

    // Force a full recovery render with all four control types
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

#ifdef _DEBUG
    Require(window.Host().DebugGetRenderCount() > initialRenderCount, "device loss: host repaints successfully after simulated device loss");
    Require(window.Host().DebugGetResizeCount() == initialResizeCount, "device loss: no swap chain resize churn during recovery");
    Require(window.Host().DebugGetResizeFailureCount() == 0u, "device loss: recovery does not introduce resize failures");

    // Verify D2D context and caches were rebuilt
    Require(window.Host().DebugHasD2DContext(), "device loss: D2D context is restored after recovery");
    Require(window.Host().DebugHasFallbackBrush(), "device loss: fallback brush is restored after recovery");
    Require(window.Host().DebugGetBrushCacheSize() > 0u, "device loss: brush cache is repopulated after recovery");

    // DWrite text formats are device-independent and must survive device loss
    Require(window.Host().DebugGetConfiguredTextFormatCount() == textFormatCountBefore,
            "device loss: configured text format cache persists through device loss recovery");
#endif
}

} // namespace

void RunRenderingTests()
{
    DXUI_RUN_TEST(TestGridMultilineClampPreservesCompleteModelText);
    DXUI_RUN_TEST(TestGridMultilineShortRowsPaintClippedFirstLine);
    DXUI_RUN_TEST(TestGridMultilineTrailingSeparatorsMatchTrimmedTwin);
    DXUI_RUN_TEST(TestGridMultilineLayoutsSurviveRepaintForEveryDistinctVisibleCell);
    DXUI_RUN_TEST(TestGridMultilineOversizedValueShapesOnlyItsVisiblePrefix);
    DXUI_RUN_TEST(TestGridSingleLineLayoutsSurviveRepaintAndScroll);
    DXUI_RUN_TEST(TestGridPaintedLayoutsAreThoseOfItsVisibleCells);
    DXUI_RUN_TEST(TestGridReleasesItsLayoutsWhenItStopsPainting);
    DXUI_RUN_TEST(TestGridInAnUnselectedTabReleasesItsLayouts);
    DXUI_RUN_TEST(TestGridSingleLineOversizedCaptionShapesOnlyItsVisiblePrefix);
    DXUI_RUN_TEST(TestGridMultilineOmissionMarkerFollowsTheTextDirection);
    DXUI_RUN_TEST(TestMultilineButtonPaintUsesMultipleTextRows);

    DXUI_RUN_TEST(TestSharedTransientSurfaceRendersOrdinaryPressedAndHighContrastPolicies);
    DXUI_RUN_TEST(TestThroughputGraphBandsStayBelowHistoryLine);
    DXUI_RUN_TEST(TestThroughputGraphHueChurnPerformanceScenario);
    DXUI_RUN_TEST(TestDxUiCoreControlsDarkVisualBaseline);
    DXUI_RUN_TEST(TestDxUiCoreControlsLightVisualBaseline);
    DXUI_RUN_TEST(TestDxUiHighContrastVisualBaseline);
    DXUI_RUN_TEST(TestProgressBarReducedMotionIndeterminateCaptureIsStatic);
    DXUI_RUN_TEST(TestDxUiPopupAndBarsVisualBaseline);
    DXUI_RUN_TEST(TestDxUiPopupAndBarsAcrylicLightVisualBaseline);
    DXUI_RUN_TEST(TestDxUiPageTransitionVisualBaseline);
    DXUI_RUN_TEST(TestDxUiAdvancedControlsVisualBaseline);
    DXUI_RUN_TEST(TestAttachedComboBoxPopupHoverDoesNotRepaintWhenHoveredItemStaysTheSame);
    DXUI_RUN_TEST(TestAttachedGridPaintHandlesDegenerateScrollbarTracks);
    DXUI_RUN_TEST(TestGridIncludesBottomClippedTrailingRow);
    DXUI_RUN_TEST(TestAttachedLargeGridVisibleWorkStaysBoundedAfterScroll);
    DXUI_RUN_TEST(TestAttachedLargeGridPaintReusesCellDataStringStorage);
    DXUI_RUN_TEST(TestAttachedLargeIconBadgeGridVisibleWorkStaysBoundedAfterScroll);
    DXUI_RUN_TEST(TestAttachedGridBottomScrollKeepsFirstVisibleRowFlushWithHeader);
    DXUI_RUN_TEST(TestAttachedLargeGridLongRunScrollingStaysBoundedWithoutResizeChurn);
    DXUI_RUN_TEST(TestAttachedLargeGroupedGridVisibleWorkStaysBoundedAfterScroll);
    DXUI_RUN_TEST(TestAttachedLargeCheckboxGridVisibleWorkStaysBoundedAfterScroll);
    DXUI_RUN_TEST(TestAttachedLargeGroupedGridLongRunScrollingStaysBoundedWithoutResizeChurn);
    DXUI_RUN_TEST(TestAttachedLargeCheckboxGridLongRunScrollingStaysBoundedWithoutResizeChurn);
    DXUI_RUN_TEST(TestAttachedComboBoxPopupScrollingStaysStable);
    DXUI_RUN_TEST(TestAttachedComboBoxPopupLongRunScrollingStaysStable);
    DXUI_RUN_TEST(TestAttachedHostSameSizeRepaintDoesNotResizeSwapChain);
    DXUI_RUN_TEST(TestAttachedHostResizeDoesNotFlushD2DInWrongState);
    DXUI_RUN_TEST(TestAttachedHostRecoversAfterSimulatedDeviceLoss);
}
