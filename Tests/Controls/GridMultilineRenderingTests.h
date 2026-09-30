#pragma once

// Pixel and shaping verification of the Grid's bounded multiline cells (plan GridTextOverflow_2026-09-21). It belongs to the
// Rendering suite: DxUiTests.Rendering.cpp includes it inside its anonymous namespace after the tests it extends, and its
// runner registers every test below. The copy, tooltip and UI Automation halves of the same contract are in the Grid and
// Accessibility suites, and the embedded host's in EmbeddedTests.cpp. Every comparison is between a scenario and a fresh twin
// painted in the same window (same device, same pixels), never against stored pixels.

#include "GridMultilineFixtures.h"

// One window at 96 dpi with a borrowed-model Grid over text cells: the bed the multiline pixel tests paint in. A 96-dpi host
// makes a device-independent pixel a pixel, so a scrolled offset of whole DIPs lands on whole pixels.
struct MultilineBed
{
    // The grid the window's host owns borrows the model, so the model outlives the window.
    GridMultilineFixtures::TextTableModel model;
    AttachedHostWindow window{DxUi::WindowHost::PresentationMode::CompositionSwapChain};
    DxUi::Grid* grid = nullptr;
    float rowHeightDip;
    uint32_t lineClamp;
    D2D1_RECT_F bounds;

    MultilineBed(std::vector<std::vector<std::wstring>> cells,
                 std::vector<float> widthsDip,
                 float rowHeightDipValue   = 64.0f,
                 uint32_t lineClampValue   = 2u,
                 D2D1_RECT_F boundsValue   = D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f),
                 D2D1_SIZE_F clientSizeDip = D2D1::SizeF(480.0f, 280.0f))
        : model(std::move(cells), std::move(widthsDip)),
          rowHeightDip(rowHeightDipValue),
          lineClamp(lineClampValue),
          bounds(boundsValue)
    {
        GridMultilineFixtures::ConfigureHostPlace(window, 96u, false, DxUi::Density::Standard, clientSizeDip);
        InstallFreshGrid();
    }

    // A new grid in the host, as a fresh attach makes it: the old one goes with every layout it held.
    DxUi::Grid* InstallFreshGrid()
    {
        auto root = std::make_unique<DxUi::Panel>();
        grid      = root->AddChild<DxUi::Grid>();
        grid->SetBounds(bounds);
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(rowHeightDip);
        grid->SetLineClamp(lineClamp);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
        return grid;
    }

    [[nodiscard]] WindowHostBitmapCapture Paint(const char* context)
    {
        return GridMultilineFixtures::CaptureWindow(window, context);
    }

    // One cell's value changed, then painted.
    [[nodiscard]] WindowHostBitmapCapture Paint(std::wstring text, const char* context, size_t rowIndex = 0u, size_t columnIndex = 0u)
    {
        model.SetText(rowIndex, columnIndex, std::move(text));
        grid->NotifyDataChanged();
        return Paint(context);
    }

    // The grid returns its layouts when it is hidden, so what is painted next starts cold, as a first paint does.
    void Cool()
    {
        grid->SetVisible(false);
        grid->SetVisible(true);
    }

    // The text and cell rectangles of one cell in physical pixels.
    [[nodiscard]] RECT TextPixels(size_t rowIndex = 0u, size_t columnIndex = 0u)
    {
        return GridMultilineFixtures::PixelRect(window.Host(), grid->GetCellLayoutMetrics(window.Host(), rowIndex, columnIndex).textRect);
    }

    [[nodiscard]] RECT CellPixels(size_t rowIndex = 0u, size_t columnIndex = 0u)
    {
        return GridMultilineFixtures::PixelRect(window.Host(), grid->GetCellLayoutMetrics(window.Host(), rowIndex, columnIndex).cellRect);
    }

    // Hovers the middle of a cell and reports the tooltip the grid offered ("" when it offered none).
    [[nodiscard]] std::wstring HoverTooltip(size_t rowIndex = 0u, size_t columnIndex = 0u)
    {
        window.Host().ClearTooltip();
        const D2D1_RECT_F cell = grid->GetCellLayoutMetrics(window.Host(), rowIndex, columnIndex).cellRect;
        Require(grid->OnMouseMove(window.Host(), D2D1::Point2F((cell.left + cell.right) * 0.5f, (cell.top + cell.bottom) * 0.5f), 0u), "the cell is hovered");
        std::wstring tooltip = window.Host().HasTooltip() ? std::wstring(window.Host().GetTooltipText()) : std::wstring{};
        // The pointer leaves again, so a hover fill does not reach the next paint.
        static_cast<void>(grid->OnMouseLeave(window.Host()));
        return tooltip;
    }
};

// Accented letters: a precomposed letter (é) and its decomposed form (e and a combining acute) are the same text. DirectWrite
// composes the pair, so a cell of either paints the same lines, the same omission marker and the same pixels, at every clamp,
// wrapped or on one line, and for a value far longer than the cell can show. Every word ends in an accented letter, so the
// marker follows one at clamps that wrap: a length taken in code units and not in characters (a mark cut from its letter) would
// paint a bare letter there.
void TestGridMultilineDecomposedAccentsPaintLikePrecomposed()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    MultilineBed bed({{L""}}, {300.0f});
    const auto empty     = bed.Paint("empty cell before the accents");
    const auto decompose = [](std::wstring_view text)
    {
        std::wstring decomposed;
        for (const wchar_t ch : text)
        {
            switch (ch)
            {
                case L'é': decomposed += L"e\x0301"; break;
                case L'à': decomposed += L"a\x0300"; break;
                default: decomposed.push_back(ch);
            }
        }
        return decomposed;
    };
    constexpr std::wstring_view words = L"été santé clarté beauté vérité qualité liberté égalité fraternité université générosité déjà voilà là décidé relié ";
    const std::wstring shortComposed  = RepeatToUnits(words, 420u);
    const std::wstring longComposed   = RepeatToUnits(words, 100000u);
    struct Value
    {
        const char* name;
        const std::wstring* composed;
    };
    for (const Value& value : {Value{"a 420-unit value", &shortComposed}, Value{"a 100,000-unit value", &longComposed}})
    {
        const std::wstring decomposed = decompose(*value.composed);
        Require(decomposed.size() > value.composed->size() && decomposed.find(L'\x0301') != std::wstring::npos,
                "the decomposed twin really holds combining marks");
        for (const uint32_t clamp : {1u, 2u, 3u})
        {
            bed.grid->SetLineClamp(clamp);
            const auto composedPaint   = bed.Paint(*value.composed, "precomposed accents");
            const auto decomposedPaint = bed.Paint(decomposed, "decomposed accents");
            Require(MeasureDifference(composedPaint, empty).pixels > 200u, "the precomposed value paints text");
            RequireIdentical(decomposedPaint, composedPaint, std::format("{} at clamp {}: decomposed accents paint like precomposed ones", value.name, clamp));
            // The grid keeps what it was given: a value is never normalized on its way to the layout or the tooltip.
            Require(bed.HoverTooltip() == decomposed, "the tooltip carries the decomposed value exactly");
        }
    }
}

// A value far longer than a cell shapes only a prefix. The end of that prefix can fall inside a surrogate pair, a zero-width-joiner
// sequence, or between a letter and its combining marks; it is never visible (the prefix is final once it holds a line beyond
// the ones that can show), so the cell must paint what a short twin with the same start paints, a twin short enough to be shaped
// whole. The same values also show the surrogate guard at work: shaping never hands DirectWrite half of a pair, which costs one
// unit of shaping in the alignments where the cut falls inside a pair and nothing in the others, so the shaped units of the
// alignments differ by one when the tail is made of pairs (by two at a clamp of one, where the unwrapped line is shaped a
// second time and the step back counts twice). Removing the guard changes no pixel, since the cut is invisible; the units are
// what shows it.
void TestGridMultilineShapedPrefixCutInsideAClusterPaintsLikeItsShortTwin()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    MultilineBed bed({{L""}}, {300.0f});
    struct Tail
    {
        const char* name;
        std::wstring tile;
        bool surrogates;
    };
    const std::array<Tail, 4> tails{{
        {"surrogate pairs", L"\xD83D\xDCF7", true},
        {"zero-width-joiner family sequences", L"\xD83D\xDC68\x200D\xD83D\xDC69\x200D\xD83D\xDC67", true},
        {"a letter and a combining accent", L"e\x0301", false},
        {"a letter and two combining marks", L"a\x0300\x0302", false},
    }};
    // Per clamp: the plain head that holds every line the clamp can show and the line after them (their breaks can depend on it),
    // and the length of the short twin, under the first prefix the grid shapes (from the font, (clamp + 2) lines of a quarter em
    // a unit, or one line unwrapped), so it is shaped whole.
    struct ClampCase
    {
        uint32_t clamp;
        size_t headUnits;
        size_t twinUnits;
    };
    const std::array<ClampCase, 3> clampCases{{{1u, 70u, 100u}, {2u, 160u, 300u}, {3u, 210u, 400u}}};
    const std::wstring plain = RepeatToUnits(L"mot suivant très long ", 400u);
    // What each alignment shaped, checked once every paint has been compared: the pixels say the cut is invisible, the units that
    // the guard stepped back.
    struct Observation
    {
        uint32_t clamp;
        const char* tail;
        bool surrogates;
        uint64_t least;
        uint64_t most;
    };
    std::vector<Observation> observations;
    for (const ClampCase& clampCase : clampCases)
    {
        bed.grid->SetLineClamp(clampCase.clamp);
        for (const Tail& tail : tails)
        {
            std::vector<uint64_t> shaped;
            uint64_t creations = 0u;
            for (size_t phase = 0u; phase < tail.tile.size(); ++phase)
            {
                // The filler moves the tail against the cut, one unit per phase, so every position in the tile is the cut once.
                const std::wstring value = plain.substr(0u, clampCase.headUnits) + std::wstring(phase, L'x') + RepeatToUnits(tail.tile, 100000u);
                std::wstring twin        = value.substr(0u, clampCase.twinUnits);
                if (IS_HIGH_SURROGATE(twin.back()))
                    twin.pop_back();
                bed.Cool();
                const auto before    = bed.grid->DebugGetTextLayoutStatistics();
                const auto longPaint = bed.Paint(value, "a 100,000-unit value with a cluster tail");
                const auto after     = bed.grid->DebugGetTextLayoutStatistics();
                shaped.push_back(after.shapedUnits - before.shapedUnits);
                if (phase == 0u)
                    creations = after.layoutCreations - before.layoutCreations;
                Require(after.layoutCreations - before.layoutCreations == creations, "every alignment lays out the same number of layouts");
                bed.Cool();
                const auto twinPaint = bed.Paint(twin, "the short twin");
                RequireIdentical(longPaint,
                                 twinPaint,
                                 std::format("clamp {}, {}, phase {}: a prefix cut in the tail paints like a short twin", clampCase.clamp, tail.name, phase));
            }
            const auto [least, most] = std::ranges::minmax(shaped);
            std::cout << "Grid shaped prefix cut, clamp " << clampCase.clamp << ", " << tail.name << ": shaped units " << least << ".." << most << '\n';
            observations.push_back({clampCase.clamp, tail.name, tail.surrogates, least, most});
        }
    }
    for (const Observation& observation : observations)
    {
        const std::string what = std::format("clamp {}, {}", observation.clamp, observation.tail);
        // Unwrapped, the omitted line is shaped again (the visible layout holds the whole measured line), so a step back counts twice.
        Require(observation.most - observation.least <= 2u,
                (what + ": the alignments of a tail shape at most a step back (one unit, or two where the line is shaped twice) apart").c_str());
        if (observation.surrogates)
            Require(observation.most > observation.least,
                    (what + ": in some alignment the cut falls inside a surrogate pair, and shaping steps back from it").c_str());
    }
}

// A right-to-left flow does not turn the Grid around: its cells lay out as they do in a left-to-right one. What must hold in
// both is what the text says: the omission marker ends Latin text on its right and Arabic text at its left end, where that
// text reads to; the ink of one cell never reaches its neighbour; and UI Automation keeps the complete values (Accessibility
// suite). Two columns, so a cell's ink can be told from the next one's.
void TestGridMultilineRightToLeftFlowKeepsMarkerSideAndClipping()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    const std::wstring latin       = L"Bonjour tout le monde\nsuite masquée";
    const std::wstring arabic      = L"مرحبا بالعالم الجميل\nالسطر الثاني";
    const std::wstring arabicEmoji = L"مرحبا بالعالم الجميل \xD83D\xDCF7\nالسطر الثاني";
    // Each twin is the visible line alone, so only the omission marker differs from it.
    const std::wstring latinTwin       = L"Bonjour tout le monde";
    const std::wstring arabicTwin      = L"مرحبا بالعالم الجميل";
    const std::wstring arabicEmojiTwin = L"مرحبا بالعالم الجميل \xD83D\xDCF7";
    MultilineBed bed({{L"", L""}}, {220.0f, 220.0f}, 40.0f, 1u, D2D1::RectF(20.0f, 20.0f, 470.0f, 120.0f));
    // The Grid reads no flow direction, so its right-to-left paint is its left-to-right one. Reported, not required: a grid that
    // mirrored would still satisfy everything below, which reads the rectangles the grid reports.
    bed.model.SetText(0u, 0u, latin);
    bed.model.SetText(0u, 1u, arabic);
    bed.grid->NotifyDataChanged();
    const auto leftToRight = bed.Paint("the cells in a left-to-right flow");
    bed.grid->SetFlowDirection(FlowDirection::RightToLeft);
    Require(bed.grid->GetFlowDirection() == FlowDirection::RightToLeft, "the grid's flow is right to left");
    std::cout << "Grid in a right-to-left flow against the same grid in a left-to-right flow: "
              << Describe(MeasureDifference(bed.Paint("the cells in a right-to-left flow"), leftToRight)) << '\n';
    struct Placement
    {
        LONG twinRight       = 0;
        uint64_t shiftedInk  = 0u; // Pixels that differ from the unmarked twin inside the twin's own ink span.
        uint64_t markerAfter = 0u; // Differing pixels right of that span.
    };
    const auto measure = [&](size_t column, const std::wstring& marked, const std::wstring& unmarked, const char* name)
    {
        // The neighbour keeps the other language while the cell under test changes, so its ink is the same in every capture.
        bed.model.SetText(0u, 1u - column, column == 0u ? arabic : latin);
        const auto emptyCell  = bed.Paint(L"", name, 0u, column);
        const auto withMarker = bed.Paint(marked, name, 0u, column);
        Require(bed.HoverTooltip(0u, column) == marked, "the tooltip carries the complete value");
        const auto twin = bed.Paint(unmarked, name, 0u, column);
        const RECT text = bed.TextPixels(0u, column);
        const RECT cell = bed.CellPixels(0u, column);
        // Clipping: nothing the cell painted lies outside its own rectangle, whatever the neighbour shows.
        for (const auto* capture : {&withMarker, &twin})
        {
            const Difference ink = MeasureDifference(*capture, emptyCell);
            Require(ink.pixels > 0u, "the cell paints its text");
            Require(ink.left >= text.left - 1 && ink.right <= text.right && ink.top >= cell.top && ink.bottom < cell.bottom,
                    "the cell's ink stays inside its text rectangle");
        }
        Placement placement;
        LONG left = LONG_MAX;
        for (LONG y = text.top; y < text.bottom; ++y)
            for (LONG x = text.left; x < text.right; ++x)
                if (PixelAt(twin, x, y) != PixelAt(emptyCell, x, y))
                {
                    left                = (std::min)(left, x);
                    placement.twinRight = (std::max)(placement.twinRight, x);
                }
        Require(left < placement.twinRight, "the unmarked twin paints its text");
        const LONG margin = static_cast<LONG>(bed.window.Host().DipsToPixels(3.0f));
        for (LONG y = text.top; y < text.bottom; ++y)
            for (LONG x = text.left; x < text.right; ++x)
                if (PixelAt(withMarker, x, y) != PixelAt(twin, x, y))
                {
                    if (x + margin < placement.twinRight)
                        ++placement.shiftedInk;
                    else if (x > placement.twinRight)
                        ++placement.markerAfter;
                }
        std::cout << "Grid omission marker in a right-to-left grid (" << name << "): twin ink " << left << ".." << placement.twinRight << ", changed inside "
                  << placement.shiftedInk << ", changed after " << placement.markerAfter << '\n';
        return placement;
    };
    const Placement latinPlacement = measure(0u, latin, latinTwin, "Latin text");
    Require(latinPlacement.shiftedInk == 0u && latinPlacement.markerAfter > 0u, "a Latin omission marker follows the text on its right");
    const Placement arabicPlacement = measure(1u, arabic, arabicTwin, "Arabic text");
    Require(arabicPlacement.shiftedInk > 0u, "an Arabic omission marker sits at the left end, where that text reads to");
    const Placement emojiPlacement = measure(1u, arabicEmoji, arabicEmojiTwin, "Arabic text ending in an emoji");
    Require(emojiPlacement.shiftedInk > 0u, "an Arabic omission marker after an emoji still sits at the left end");
}

// The lines DirectWrite breaks `value` into at the width the grid lays a cell out at, as the grid measures them (same text format,
// same width and height): each line's text, with its line break and, for the last, its trailing white space left out.
[[nodiscard]] std::vector<std::wstring> MeasureGridCellLines(DxUi::ControlHost& host, std::wstring_view value, float widthDip, float heightDip, bool wrap)
{
    auto* factory = host.GetWriteFactory();
    auto* format  = host.GetTextFormat(DxUi::FontRole::Body, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, wrap);
    Require(factory != nullptr && format != nullptr, "the host has a DirectWrite factory and a text format");
    wil::com_ptr<IDWriteTextLayout> layout;
    RequireSucceeded(factory->CreateTextLayout(value.data(), static_cast<UINT32>(value.size()), format, widthDip, heightDip, layout.put()),
                     "the cell's value is laid out");
    UINT32 count = 0u;
    static_cast<void>(layout->GetLineMetrics(nullptr, 0u, &count));
    std::vector<DWRITE_LINE_METRICS> metrics(count);
    RequireSucceeded(layout->GetLineMetrics(metrics.data(), count, &count), "the lines are measured");
    std::vector<std::wstring> lines;
    size_t offset = 0u;
    for (const DWRITE_LINE_METRICS& line : metrics)
    {
        const bool last = &line == &metrics.back();
        lines.emplace_back(value.substr(offset, line.length - (last ? line.trailingWhitespaceLength : line.newlineLength)));
        offset += line.length;
    }
    return lines;
}

// A cell narrower than one word: the word fits on no line, and DirectWrite cuts what the cell shows of it. At every clamp (one
// line unwrapped, or wrapped to two or three) the ink stays inside the cell's text rectangle, hovering offers the complete value,
// and the cell shows its visible lines followed by an ellipsis: it paints exactly what a value made of those lines and a literal
// ellipsis character (U+2026) paints, where the last line keeps as many characters as the ellipsis leaves room for. The lines are
// the ones DirectWrite breaks the word into at the cell's width, measured here the way the grid measures them.
void TestGridMultilineCellNarrowerThanItsWordKeepsInkInsideAndOffersTheValue()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    const std::wstring value = L"Extraordinairement incompréhensible";
    for (const float widthDip : {40.0f, 48.0f, 56.0f})
    {
        MultilineBed bed({{L""}}, {widthDip}, 64.0f, 1u);
        const auto empty = bed.Paint("empty narrow cell");
        const RECT text  = bed.TextPixels();
        Require(text.right - text.left == static_cast<LONG>(widthDip) - 16, "the text rectangle is the cell less its padding");
        const D2D1_RECT_F textDip = bed.grid->GetCellLayoutMetrics(bed.window.Host(), 0u, 0u).textRect;
        for (const uint32_t clamp : {1u, 2u, 3u})
        {
            bed.grid->SetLineClamp(clamp);
            const auto painted       = bed.Paint(value, "the narrow cell");
            const Difference ink     = MeasureDifference(painted, empty);
            const std::string detail = std::format("a {}-DIP cell at clamp {}", widthDip, clamp);
            Require(ink.pixels > 20u, (detail + " paints some of the word").c_str());
            Require(ink.left >= text.left - 1 && ink.right <= text.right && ink.top >= text.top - 1 && ink.bottom <= text.bottom,
                    (detail + ": the ink stays inside the text rectangle").c_str());
            Require(bed.HoverTooltip() == value, (detail + ": hovering offers the complete value").c_str());
            // The text the cell shows: its visible lines, then an ellipsis after as many characters of the last as the cell keeps.
            const std::vector<std::wstring> lines =
                MeasureGridCellLines(bed.window.Host(), value, textDip.right - textDip.left, textDip.bottom - textDip.top, clamp > 1u);
            Require(lines.size() >= clamp, (detail + ": the word takes at least as many lines as the clamp shows").c_str());
            std::wstring head;
            for (size_t line = 0u; line + 1u < clamp; ++line)
                head += lines[line] + L"\n";
            const std::wstring& last = lines[clamp - 1u];
            std::optional<size_t> shown;
            for (size_t kept = 0u; kept <= last.size() && ! shown; ++kept)
            {
                const auto twin = bed.Paint(head + last.substr(0u, kept) + L"\x2026", "the visible lines with a literal ellipsis");
                if (MeasureDifference(painted, twin, text).pixels == 0u)
                    shown = kept;
            }
            std::cout << "Grid narrow cell, " << detail << ": ink " << Describe(ink) << " in text " << text.left << ".." << text.right << " x " << text.top
                      << ".." << text.bottom << ", shows " << clamp << " line(s), the last with " << (shown ? static_cast<int>(*shown) : -1) << " of "
                      << last.size() << " characters and an ellipsis\n";
            Require(shown.has_value(), (detail + ": the cell paints its visible lines followed by an ellipsis").c_str());
            // A value that fits paints as itself and offers no tooltip.
            static_cast<void>(bed.Paint(L"Ab", "a value that fits"));
            Require(bed.HoverTooltip().empty(), (detail + ": a value that fits offers no tooltip").c_str());
        }
    }
}

// Clipping is not reflow. A cell that the viewport cuts (under the header or the left edge, or by the bottom or right edge)
// keeps the layout of its whole cell: what survives is the same pixels the whole cell paints there. The vertical scroll rests on
// whole rows, so a scrolled grid is a shifted crop of the unscrolled one, and so is the grid the scrollbar thumb is dragging, which
// stops between rows and so cuts a row under the header; whole DIPs at 96 dpi are whole pixels. A horizontal offset is continuous:
// fractional ones land between pixels, and there the cut grid is compared with a grid of the same offsets whose viewport does not
// cut the cell.
void TestGridMultilineCellCutByTheViewportPaintsAShiftedCropOfItsWholeSelf()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    constexpr size_t rowCount = 8u;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < rowCount; ++row)
        cells.push_back(
            {std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production prévue pour la semaine "
                         L"prochaine, puis confirmer auprès de l’équipe, relire le compte rendu de la réunion précédente, contrôler les sauvegardes de "
                         L"la nuit et prévenir les utilisateurs concernés avant la fin de la journée ouvrée.",
                         row),
             std::format(L"État {}.1 : synchronisation interrompue après trois tentatives, consulter le journal détaillé pour connaître la cause "
                         L"exacte de l’échec, puis relancer l’opération depuis le poste principal sans oublier de vérifier les droits d’accès du "
                         L"compte de service avant de poursuivre.",
                         row)});
    const D2D1_RECT_F cut   = D2D1::RectF(20.0f, 20.0f, 340.0f, 180.0f);
    const D2D1_RECT_F whole = D2D1::RectF(20.0f, 20.0f, 340.0f, 270.0f);
    MultilineBed bed(cells, {400.0f, 400.0f}, 64.0f, 2u, cut);
    const auto unscrolled = bed.Paint("unscrolled grid");
    Require(bed.grid->DebugGetTextLayoutStatistics().displayCapacity > 0u, "the cells are trimmed (their omitted tails were laid out)");
    // What the unscrolled viewport shows of the first column, from its first row to the cut second one: where a scrolled paint
    // can be compared with it.
    const D2D1_RECT_F firstRow  = *bed.grid->GetVisibleCellRect(0u, 0u);
    const D2D1_RECT_F secondRow = *bed.grid->GetVisibleCellRect(1u, 0u);
    // Two DIPs in from the edges of the viewport, where the border, the header and the scrollbars blend into the pixels.
    const D2D1_RECT_F viewport = D2D1::RectF(firstRow.left + 2.0f, firstRow.top + 2.0f, firstRow.right - 2.0f, secondRow.bottom - 2.0f);
    Require(secondRow.bottom - secondRow.top < 64.0f && firstRow.right - firstRow.left < 400.0f, "the viewport cuts the second row and the first column");
    // Compares what the scrolled grid paints of the text of rows 0 to 3 of the first column with the unscrolled paint moved by the
    // scroll, where both show the cell. The scroll is read back from where the first row's cell lies, since a dragged thumb sets
    // offsets nobody chose; a fractional one is tried at the two whole shifts around it (text is placed on whole pixels vertically).
    const auto shiftedCrop = [&](const WindowHostBitmapCapture& scrolled, const char* what)
    {
        const float verticalDip   = firstRow.top - bed.grid->GetCellLayoutMetrics(bed.window.Host(), 0u, 0u).cellRect.top;
        const float horizontalDip = firstRow.left - bed.grid->GetCellLayoutMetrics(bed.window.Host(), 0u, 0u).cellRect.left;
        uint64_t compared         = 0u;
        for (const size_t row : {0u, 1u, 2u, 3u})
        {
            const auto visible = bed.grid->GetVisibleCellRect(row, 0u);
            if (! visible)
                continue;
            // The text of the cell, cut to what the scrolled viewport shows of it and to what the unscrolled one showed.
            const D2D1_RECT_F text = bed.grid->GetCellLayoutMetrics(bed.window.Host(), row, 0u).textRect;
            const RECT region{static_cast<LONG>(std::ceil((std::max)({visible->left, text.left, viewport.left - horizontalDip}))),
                              static_cast<LONG>(std::ceil((std::max)({visible->top, text.top, viewport.top - verticalDip}))),
                              static_cast<LONG>(std::floor((std::min)({visible->right, text.right, viewport.right - horizontalDip}))),
                              static_cast<LONG>(std::floor((std::min)({visible->bottom, text.bottom, viewport.bottom - verticalDip})))};
            if (region.right <= region.left || region.bottom <= region.top)
                continue;
            std::set<uint32_t> colors;
            Difference best;
            best.pixels = UINT64_MAX;
            for (const LONG dy : {static_cast<LONG>(std::floor(verticalDip)), static_cast<LONG>(std::ceil(verticalDip))})
            {
                const LONG dx = static_cast<LONG>(std::lround(horizontalDip));
                Difference mismatch;
                mismatch.pixels = 0u;
                for (LONG y = region.top; y < region.bottom; ++y)
                    for (LONG x = region.left; x < region.right; ++x)
                    {
                        colors.insert(PixelAt(scrolled, x, y));
                        if (PixelAt(scrolled, x, y) == PixelAt(unscrolled, x + dx, y + dy))
                            continue;
                        ++mismatch.pixels;
                        mismatch.left   = (std::min)(mismatch.left, x);
                        mismatch.top    = (std::min)(mismatch.top, y);
                        mismatch.right  = (std::max)(mismatch.right, x);
                        mismatch.bottom = (std::max)(mismatch.bottom, y);
                    }
                if (mismatch.pixels < best.pixels)
                    best = mismatch;
            }
            std::cout << "Grid " << what << ", scrolled by (" << verticalDip << ", " << horizontalDip << "), row " << row << ": compared "
                      << region.right - region.left << " x " << region.bottom - region.top << " pixels of text at " << region.left << "," << region.top << " ("
                      << colors.size() << " colors), best shift " << Describe(best) << '\n';
            Require(best.pixels == 0u, "a cut cell paints the shifted pixels of the unscrolled one: clipping does not reflow it");
            compared += static_cast<uint64_t>(region.right - region.left) * static_cast<uint64_t>(region.bottom - region.top);
            Require(colors.size() > 3u, "the compared pixels hold text");
        }
        Require(compared > 2000u, "the scrolled grid shows enough of a cell to compare");
    };
    // Whole rows and whole DIPs: rows cut by the bottom edge in one paint are whole in the other, and the first column is cut under
    // the left edge.
    for (const auto [verticalDip, horizontalDip] : {std::pair{64.0f, 0.0f}, std::pair{0.0f, 40.0f}, std::pair{64.0f, 40.0f}, std::pair{64.0f, 100.0f}})
    {
        bed.grid->DebugSetScrollOffsets(verticalDip, horizontalDip);
        shiftedCrop(bed.Paint("scrolled grid"), "scrolled by whole rows and DIPs");
    }
    // The scrollbar thumb being dragged rests between rows, so a row is cut under the header. The pointer stays down while the
    // grid paints.
    bed.grid->DebugSetScrollOffsets(0.0f, 0.0f);
    static_cast<void>(bed.Paint("grid back at the top"));
    const ThemePalette theme = bed.window.Host().GetTheme();
    for (const float travelDip : {6.0f, 17.3f})
    {
        bed.grid->DebugSetScrollOffsets(0.0f, 0.0f);
        static_cast<void>(bed.Paint("grid back at the top"));
        const D2D1_RECT_F thumb  = bed.grid->DebugGetScrollbarVisualState(theme).verticalThumbRect;
        const D2D1_POINT_2F grab = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
        Require(bed.grid->OnMouseDown(bed.window.Host(), grab, false, 0u), "the vertical scrollbar thumb is grabbed");
        Require(bed.grid->OnMouseMove(bed.window.Host(), D2D1::Point2F(grab.x, grab.y + travelDip), 0u), "the thumb is dragged");
        const float cutAtTop = firstRow.top - bed.grid->GetCellLayoutMetrics(bed.window.Host(), 0u, 0u).cellRect.top;
        Require(cutAtTop > 1.0f && std::fabs(cutAtTop - std::round(cutAtTop)) > 0.001f, "the dragged thumb left a fractional vertical offset");
        shiftedCrop(bed.Paint("grid under a dragged thumb"), "under a dragged scrollbar thumb");
        static_cast<void>(bed.grid->OnMouseUp(bed.window.Host(), D2D1::Point2F(grab.x, grab.y + travelDip), false, 0u));
    }
    // Fractional horizontal offsets: the same offsets in a grid whose viewport shows the whole cells.
    constexpr std::array<std::pair<float, float>, 3> fractions{{{0.0f, 40.5f}, {64.0f, 57.75f}, {128.0f, 13.25f}}};
    for (const auto& [verticalDip, horizontalDip] : fractions)
    {
        bed.bounds = cut;
        bed.InstallFreshGrid();
        bed.grid->DebugSetScrollOffsets(verticalDip, horizontalDip);
        const auto clipped = bed.Paint("fractionally scrolled grid, cut by its viewport");
        bed.bounds         = whole;
        bed.InstallFreshGrid();
        bed.grid->DebugSetScrollOffsets(verticalDip, horizontalDip);
        const auto uncut = bed.Paint("fractionally scrolled grid, whole");
        // The body of the viewport of the cut grid: inside its scrollbars and border.
        const RECT body = PixelRect(bed.window.Host(), D2D1::RectF(cut.left + 2.0f, cut.top + 34.0f, cut.right - 18.0f, cut.bottom - 18.0f));
        RequireIdentical(
            clipped,
            uncut,
            body,
            std::format("scrolled by ({}, {}) DIP, the cut grid paints what the grid that shows the whole cells paints", verticalDip, horizontalDip));
        Require(MeasureDifference(clipped, unscrolled, body).pixels > 300u, "the fractional scroll moved the text");
    }
}

// A host whose dpi changes: the grid's layouts are in device-independent units and its cells keep their DIP geometry, and what
// it paints at the new dpi is what a fresh grid paints there, at the larger dpis and back at 96. Retained and fresh share one
// host so the two differ in nothing but the layouts one of them kept.
void TestGridMultilineRepaintsAtANewDpiLikeAFreshGrid()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < 4u; ++row)
        cells.push_back(
            {std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production.\nDeuxième paragraphe masqué.", row),
             std::format(L"État {}.1 : échec après trois tentatives, consulter le journal détaillé.\nNouvelle tentative prévue.", row)});
    const D2D1_SIZE_F clientDip = D2D1::SizeF(480.0f, 300.0f);
    MultilineBed bed(cells, {200.0f, 200.0f}, 64.0f, 2u, D2D1::RectF(10.0f, 10.0f, 470.0f, 290.0f), clientDip);
    const auto atBase = bed.Paint("the grid at 96 dpi");
    Require(bed.grid->DebugGetTextLayoutStatistics().displayCapacity > 0u, "the grid laid out omitted tails");
    for (const UINT dpi : {144u, 192u, 96u, 192u})
    {
        ConfigureHostPlace(bed.window, dpi, false, Density::Standard, clientDip);
        const auto before   = bed.grid->DebugGetTextLayoutStatistics();
        const auto retained = bed.Paint("the grid repainted at the new dpi");
        const auto after    = bed.grid->DebugGetTextLayoutStatistics();
        Require(retained.widthPx == static_cast<UINT>(clientDip.width * static_cast<float>(dpi) / 96.0f), "the capture is the client area at the new dpi");
        std::cout << "Grid at " << dpi << " dpi: layouts created by the repaint " << after.layoutCreations - before.layoutCreations << ", retained "
                  << after.retainedLayouts << '\n';
        bed.InstallFreshGrid();
        const auto fresh = bed.Paint("a fresh grid at the new dpi");
        RequireIdentical(retained, fresh, std::format("at {} dpi the grid that kept its layouts paints like a fresh one", dpi));
        if (dpi == 96u)
            RequireIdentical(fresh, atBase, "back at 96 dpi the pixels are the first ones");
        else
            Require(MeasureDifference(fresh, atBase).pixels > 0u || fresh.widthPx != atBase.widthPx, "the new dpi is another picture");
    }
}

// Theme, font and density changes with retained layouts. A theme change (light, dark, high contrast) repaints with the new
// colors and lays nothing out again; a font change (another family and size: the list item's Segoe UI Variable Small at 12 DIP,
// the body's Text at 13, body large at 18, a heavier subtitle) or a density change (compact rows) lays the cells out again.
// Each step is compared with a fresh attach in the same state, and the fresh grid then is the retained one of the next step.
void TestGridMultilineRepaintsAfterThemeFontAndDensityChangesLikeAFreshAttach()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < 3u; ++row)
        cells.push_back(
            {std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production.\nDeuxième paragraphe masqué.", row),
             std::format(L"État {}.1 : échec après trois tentatives, consulter le journal détaillé.\nNouvelle tentative prévue.", row)});
    MultilineBed bed(cells, {210.0f, 210.0f}, 64.0f, 2u, D2D1::RectF(10.0f, 10.0f, 470.0f, 270.0f));
    struct State
    {
        const char* name;
        bool dark;
        bool highContrast;
        Density density;
        FontRole role;
    };
    const std::array<State, 9> states{{
        {"light, body", false, false, Density::Standard, FontRole::Body},
        {"dark", true, false, Density::Standard, FontRole::Body},
        {"high contrast", false, true, Density::Standard, FontRole::Body},
        {"list item font (Variable Small, 12 DIP)", false, false, Density::Standard, FontRole::ListItem},
        {"body large font (18 DIP)", false, false, Density::Standard, FontRole::BodyLarge},
        {"dark subtitle font (heavier, 20 DIP)", true, false, Density::Standard, FontRole::Subtitle},
        {"compact density", false, false, Density::Compact, FontRole::Body},
        {"compact density in high contrast, header font", false, true, Density::Compact, FontRole::Header},
        {"light, body again", false, false, Density::Standard, FontRole::Body},
    }};
    FontRole role         = FontRole::Body;
    Density density       = Density::Standard;
    const auto paintFresh = [&](const char* context)
    {
        bed.InstallFreshGrid();
        bed.grid->SetCellTextFontRole(role);
        return bed.Paint(context);
    };
    static_cast<void>(paintFresh("the first state"));
    std::vector<WindowHostBitmapCapture> seen;
    for (const State& state : states)
    {
        ThemePalette theme = MakeDefaultThemePalette(state.dark);
        if (state.highContrast)
        {
            // The system's high-contrast colors: white on black, a cyan accent.
            ThemeColors colors{.sizeBytes = sizeof(ThemeColors)};
            colors.backgroundArgb          = 0xFF000000u;
            colors.textArgb                = 0xFFFFFFFFu;
            colors.selectionBackgroundArgb = 0xFF1AEBFFu;
            colors.selectionTextArgb       = 0xFF000000u;
            colors.accentArgb              = 0xFF1AEBFFu;
            colors.darkMode                = TRUE;
            colors.highContrast            = TRUE;
            colors.darkBase                = TRUE;
            theme                          = MakeThemePalette(colors);
        }
        theme.reducedMotion = true;
        theme.density       = state.density;
        bed.window.Host().SetTheme(theme);
        bed.grid->SetCellTextFontRole(state.role);
        const bool layoutChanged = state.role != role || state.density != density;
        role                     = state.role;
        density                  = state.density;
        const auto before        = bed.grid->DebugGetTextLayoutStatistics();
        const auto retained      = bed.Paint(state.name);
        const auto after         = bed.grid->DebugGetTextLayoutStatistics();
        const std::string what   = std::format("{}: the grid that kept its layouts paints like a fresh attach", state.name);
        Require(after.retainedLayouts > 0u, "the grid holds layouts after the change");
        Require((after.layoutCreations != before.layoutCreations) == layoutChanged,
                layoutChanged ? "a font or density change lays the cells out again" : "a theme change lays nothing out again");
        const auto fresh = paintFresh(state.name);
        RequireIdentical(retained, fresh, what);
        for (const WindowHostBitmapCapture& earlier : seen)
            if (earlier.bgraPixels == fresh.bgraPixels)
                std::cerr << "    [NOTE] " << state.name << " paints the pixels of an earlier state\n";
        seen.push_back(fresh);
    }
    // The states differ from one another: each paint is a picture of its own state, not one picture repeated.
    for (const size_t other : {1u, 2u, 3u, 4u, 5u, 6u, 7u})
        Require(seen[0].bgraPixels != seen[other].bgraPixels, states[other].name);
    Require(seen[1].bgraPixels != seen[2].bgraPixels, "dark and high contrast are different pictures");
    Require(seen[0].bgraPixels == seen[8].bgraPixels, "returning to the first state paints the first picture");
}

// Sharing a layout between long values is only right when the values lay out alike. A value is found by a hash of its first 4,096
// units and then confirmed against the prefix its entry shaped and how the value continues after it, so values that start the
// same way but differ where shaping stops must not share, even where the hash cannot tell them apart: a cell wide enough to
// shape more than 4,096 units (3,000 DIP at clamp three) keeps a prefix that runs past the hashed units, so a value that differs
// inside that prefix, or in the character after a paragraph that ends it, reaches the same set and only the confirmation refuses
// it. A value that differs only beyond the prefix, or after the same break, shares. The layouts each paint creates say which.
void TestGridMultilinePrefixSharingNeverCrossesTextThatLaysOutDifferently()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    MultilineBed bed({{L""}}, {3000.0f}, 64.0f, 3u);
    // Paints `first`, then `second` while the layout of `first` is still retained, and returns how many layouts `second` made.
    const auto secondMade = [&](const std::wstring& first, const std::wstring& second, WindowHostBitmapCapture& secondPaint)
    {
        bed.Cool();
        static_cast<void>(bed.Paint(first, "the first value"));
        const auto before = bed.grid->DebugGetTextLayoutStatistics();
        secondPaint       = bed.Paint(second, "the second value");
        return bed.grid->DebugGetTextLayoutStatistics().layoutCreations - before.layoutCreations;
    };
    const auto freshPaint = [&](const std::wstring& text)
    {
        bed.Cool();
        return bed.Paint(text, "the value on its own");
    };
    const auto check = [&](const std::wstring& first, const std::wstring& second, bool shares, const char* what)
    {
        WindowHostBitmapCapture painted;
        const uint64_t made = secondMade(first, second, painted);
        std::cout << "Grid prefix sharing (" << what << "): the second value made " << made << " layouts\n";
        Require(shares ? made == 0u : made > 0u, what);
        RequireIdentical(painted, freshPaint(second), std::string(what) + ": the paint is the value's own");
    };
    const std::wstring huge = RepeatToUnits(L"mot suivant très long ", 100000u);
    // The shaped prefix of this cell is about 4,630 units: (3 + 2) lines of 920 units (a quarter em each across 2,984 DIP) and 32.
    std::wstring differsInside = huge;
    differsInside[4400]        = L'X'; // Past the 4,096 hashed units, inside the shaped prefix.
    std::wstring differsBeyond = huge;
    differsBeyond[9000]        = L'X'; // Beyond the shaped prefix.
    check(huge, differsInside, false, "a value that differs inside the shaped prefix, past the hashed units, lays out for itself");
    check(huge, differsBeyond, true, "a value that differs only beyond the shaped prefix shares the retained layout");
    // A paragraph longer than the hashed units and shorter than the prefix ends the shaped prefix; how the next character
    // continues it (a line break, a CR LF pair, the same paragraph) changes what the lines after it look like.
    const std::wstring paragraph = RepeatToUnits(L"mot suivant très long ", 4500u);
    const std::wstring tail      = RepeatToUnits(L"Dernier paragraphe très long qui reste masqué. ", 6000u);
    const std::wstring base      = paragraph + L"\n" + tail;
    check(base, paragraph + L" et la suite\n" + tail, false, "a paragraph that continues where the retained one ended lays out for itself");
    check(base, paragraph + L"\r\n" + tail, false, "a CR LF pair where the retained paragraph ended in a line feed lays out for itself");
    check(base,
          paragraph + L"\n" + RepeatToUnits(L"Un autre paragraphe final, masqué aussi. ", 6000u),
          true,
          "the same break and another tail shares the retained layout");
}

// The layout tables have a ceiling (16,384 entries) that no test window holds enough cells to reach, so a hook lowers it. A
// grid with more distinct visible cells than the lowered ceiling holds keeps its tables at the ceiling, lays the cells that do
// not fit out again in every paint (the least recently used way of a full set is evicted), and paints the pixels a grid
// with the default ceiling paints, however often it repaints. Raising the ceiling restores reuse, and a table whose use drops below
// an eighth of its entries halves by one step a paint down to its first 32 entries.
void TestGridMultilineLayoutTablesStayAtALoweredCeilingAndPaintRightAfterEviction()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    AttachedHostWindow window(WindowHost::PresentationMode::CompositionSwapChain);
    ConfigureHostPlace(window, 96u, false, Density::Standard, D2D1::SizeF(760.0f, 640.0f));
    constexpr size_t columnCount = 6u;
    constexpr size_t rowCount    = 16u;
    constexpr float rowHeight    = 48.0f;
    DistinctMultilineGridModel model(rowCount, columnCount);
    const D2D1_RECT_F full = D2D1::RectF(10.0f, 10.0f, 10.0f + (static_cast<float>(columnCount) * 110.0f) + 20.0f, 10.0f + 30.0f + (12.0f * rowHeight) + 8.0f);
    Grid* grid             = nullptr;
    const auto install     = [&](const D2D1_RECT_F& bounds, size_t limit)
    {
        auto root = std::make_unique<Panel>();
        grid      = root->AddChild<Grid>();
        grid->SetBounds(bounds);
        grid->SetHeaderHeightDip(30.0f);
        grid->SetRowHeightDip(rowHeight);
        grid->SetLineClamp(2u);
        grid->DebugSetTextLayoutEntryLimit(limit);
        grid->SetModel(&model);
        window.Host().SetRoot(std::move(root));
    };
    const auto drawnCells = [&]
    {
        size_t cells = 0u;
        for (size_t row = 0u; row < rowCount; ++row)
            for (size_t column = 0u; column < columnCount; ++column)
                cells += grid->GetVisibleCellRect(row, column).has_value() ? 1u : 0u;
        return cells;
    };
    install(full, 0u);
    Require(grid->DebugGetTextLayoutEntryLimit() == 16384u, "the default ceiling is 16,384 entries");
    const auto reference = CaptureWindow(window, "reference paint with the default ceiling");
    const size_t cells   = drawnCells();
    const auto roomy     = grid->DebugGetTextLayoutStatistics();
    std::cout << "Grid lowered ceiling: " << cells << " drawn cells, default tables " << roomy.capacity << " / " << roomy.displayCapacity << " entries\n";
    Require(cells > 64u && roomy.capacity > 64u, "more than 64 distinct cells are drawn, and the default ceiling lets their table grow past 64");

    // The ceiling rounds down to a power of two of at least 32, the sizes a table takes.
    grid->DebugSetTextLayoutEntryLimit(100u);
    Require(grid->DebugGetTextLayoutEntryLimit() == 64u, "a ceiling of 100 entries rounds down to 64");
    grid->DebugSetTextLayoutEntryLimit(1u);
    Require(grid->DebugGetTextLayoutEntryLimit() == 32u, "a ceiling below the first table size is that size");
    grid->DebugSetTextLayoutEntryLimit(0u);
    Require(grid->DebugGetTextLayoutEntryLimit() == 16384u, "a ceiling of zero restores the default");

    install(full, 64u);
    Require(grid->DebugGetTextLayoutEntryLimit() == 64u, "the lowered ceiling is 64 entries");
    const auto capped = CaptureWindow(window, "first paint at a ceiling of 64 entries");
    const auto held   = grid->DebugGetTextLayoutStatistics();
    std::cout << "Grid lowered ceiling: tables " << held.capacity << " / " << held.displayCapacity << " entries, layouts " << held.retainedLayouts << " / "
              << held.displayLayouts << '\n';
    Require(held.capacity == 64u && held.displayCapacity <= 64u, "the tables stop growing at the ceiling");
    Require(held.retainedLayouts <= 64u && held.displayLayouts <= 64u, "they hold no more layouts than the ceiling");
    RequireIdentical(capped, reference, "a grid whose tables evict paints what a grid with room paints");
    for (int repaint = 0; repaint < 3; ++repaint)
    {
        const auto before = grid->DebugGetTextLayoutStatistics();
        const auto again  = CaptureWindow(window, "repaint at a ceiling of 64 entries");
        const auto after  = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid lowered ceiling: repaint " << repaint + 1 << " laid out " << after.layoutCreations - before.layoutCreations << " layouts for "
                  << cells << " cells\n";
        Require(after.capacity == 64u && after.displayCapacity <= 64u, "the tables stay at the ceiling");
        Require(after.layoutCreations - before.layoutCreations >= cells - 64u, "cells that do not fit are laid out again in every paint");
        RequireIdentical(again, reference, "a repaint after eviction paints the same pixels");
    }

    // Raising the ceiling restores reuse: the tables grow to hold the cells and later paints lay nothing out.
    grid->DebugSetTextLayoutEntryLimit(0u);
    static_cast<void>(CaptureWindow(window, "paint after raising the ceiling"));
    const auto grown = grid->DebugGetTextLayoutStatistics();
    Require(grown.capacity > 64u, "the table grows again once the ceiling allows it");
    const auto beforeSettled = grid->DebugGetTextLayoutStatistics();
    RequireIdentical(CaptureWindow(window, "settled paint"), reference, "the settled paint is the reference");
    Require(grid->DebugGetTextLayoutStatistics().layoutCreations == beforeSettled.layoutCreations, "with room for every cell, a repaint lays nothing out");

    // A table whose use drops below an eighth of its entries halves by one step per paint, down to its first 32 entries.
    grid->SetBounds(D2D1::RectF(10.0f, 10.0f, 10.0f + (3.0f * 110.0f) + 20.0f, 10.0f + 30.0f + rowHeight + 4.0f));
    size_t valueEntries   = grown.capacity;
    size_t displayEntries = grown.displayCapacity;
    for (int capture = 0; capture < 6; ++capture)
    {
        // A capture paints the window more than once (the repaint it asks for, then the read-back), so count the paints.
        const uint64_t paintsBefore = grid->DebugGetPaintCount();
        static_cast<void>(CaptureWindow(window, "paint of a few cells in tables that grew for many"));
        const uint64_t paints = grid->DebugGetPaintCount() - paintsBefore;
        Require(paints >= 1u, "the grid painted");
        for (uint64_t paint = 0u; paint < paints; ++paint)
        {
            valueEntries   = (std::max)(static_cast<size_t>(32u), valueEntries / 2u);
            displayEntries = (std::max)(static_cast<size_t>(32u), displayEntries / 2u);
        }
        const auto now = grid->DebugGetTextLayoutStatistics();
        std::cout << "Grid lowered ceiling: after " << paints << " paints the tables hold " << now.capacity << " / " << now.displayCapacity << " entries\n";
        Require(now.capacity == valueEntries && now.displayCapacity == displayEntries, "each paint of a few cells halves the tables, down to 32 entries");
        Require(now.retainedLayouts <= 8u && now.displayLayouts <= 8u, "only the cells in view keep layouts");
    }
    Require(valueEntries == 32u && displayEntries == 32u, "the tables reached their first size");
}

// Device loss with trimmed multiline cells: the host discards its device resources and brushes and builds them again at the next
// paint. The cells paint the pixels they painted before, loss after loss, and the layouts (DirectWrite objects, which belong to
// no device) are still the ones the grid held: nothing is laid out again.
void TestGridMultilineTrimmedCellsPaintTheSameAfterDeviceLoss()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < 3u; ++row)
        cells.push_back(
            {std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production.\nDeuxième paragraphe masqué.", row),
             std::format(L"État {}.1 : échec après trois tentatives, consulter le journal détaillé.\nNouvelle tentative prévue 👨‍👩‍👧 📷.",
                         row)});
    MultilineBed bed(cells, {210.0f, 210.0f}, 64.0f, 2u, D2D1::RectF(10.0f, 10.0f, 470.0f, 270.0f));
    const auto before = bed.Paint("trimmed cells before device loss");
    const auto held   = bed.grid->DebugGetTextLayoutStatistics();
    Require(held.retainedLayouts > 0u && held.displayCapacity > 0u, "the grid holds layouts and laid out omitted tails");
    for (int loss = 1; loss <= 3; ++loss)
    {
        bed.window.Host().DebugSimulateDeviceLoss();
        Require(! bed.window.Host().DebugHasD2DContext(), "the device resources are gone");
        bed.window.PumpMessages();
        const auto after = bed.Paint("trimmed cells after device loss");
        Require(bed.window.Host().DebugHasD2DContext(), "the device resources are back");
        RequireIdentical(after, before, std::format("after device loss {} the trimmed cells paint what they painted before", loss));
        const auto now = bed.grid->DebugGetTextLayoutStatistics();
        Require(now.layoutCreations == held.layoutCreations && now.retainedLayouts == held.retainedLayouts,
                "the layouts survive the loss: nothing is laid out again");
    }
    // A fresh grid painted on the recovered device agrees too.
    bed.InstallFreshGrid();
    RequireIdentical(bed.Paint("a fresh grid on the recovered device"), before, "a fresh grid on the recovered device paints the same pixels");
}

// A multiline grid moved to another host (another dpi, theme, density, or a Direct2D device made after it painted) arranges its
// cells, and paints them, as a grid created there does. The moved grid keeps the model and every setting, and lays out its cells
// again for wherever it now is.
void TestGridMultilineMovedBetweenHostsMatchesAFreshOne()
{
    using namespace DxUi;
    using namespace GridMultilineFixtures;
    using namespace DxUiTestMoved;
    std::vector<std::vector<std::wstring>> cells;
    for (size_t row = 0u; row < 4u; ++row)
        cells.push_back(
            {std::format(L"Élément {}.0 : vérifier la configuration du serveur principal avant la mise en production.\nDeuxième paragraphe masqué.", row),
             std::format(L"État {}.1 : échec après trois tentatives, consulter le journal détaillé.\nNouvelle tentative prévue 📷.", row)});
    TextTableModel model(std::move(cells), {200.0f, 200.0f});
    const auto configure = [&model](Grid& grid)
    {
        grid.SetHeaderHeightDip(30.0f);
        grid.SetRowHeightDip(64.0f);
        grid.SetLineClamp(2u);
        grid.SetModel(&model);
    };
    // Every rectangle the grid arranges for the visible cells: the cell as it is cut by the viewport, and the whole cell's
    // text area (the rectangle its layout is made for).
    const auto measure = [&model](Grid& grid, ControlHost& host)
    {
        NamedRects rects;
        for (size_t row = 0u; row < model.GetRowCount(); ++row)
            for (size_t column = 0u; column < model.GetColumnCount(); ++column)
            {
                const GridCellLayoutMetrics metrics = grid.GetCellLayoutMetrics(host, row, column);
                rects.emplace_back(std::format("cell {},{}", row, column), metrics.cellRect);
                rects.emplace_back(std::format("text {},{}", row, column), metrics.textRect);
                if (const auto visible = grid.GetVisibleCellRect(row, column))
                    rects.emplace_back(std::format("visible {},{}", row, column), *visible);
            }
        return rects;
    };
    ExpectMovedControlMatchesAFreshOne<Grid>("multiline grid", D2D1::SizeF(420.0f, 214.0f), kEveryPlace, configure, measure);
}
