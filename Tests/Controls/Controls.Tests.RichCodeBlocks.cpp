#include "Controls.Tests.RichCodeBlocks.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.PreparedLayoutAccess.h"
#include "Controls/DxUi.RichCodeLayoutCache.h"
#include "Controls/DxUi.RichLayoutOptions.h"
#include "GraphicsFixture.h"
#include "Support.Tests.RichSemanticGeometry.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include "Support/WorkCounters.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <dwrite_3.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <wil/result.h>

// W7.6 Code blocks: PreparedLayout::TryVisibleRange without a caret read per line (this file), the semantic visible-range
// queries through the paint index, the line-number and badge layout cache
// (the geometry tests that follow, in the same file).
namespace
{
using namespace DxUi::RichText;

wil::com_ptr<IDWriteFactory> CodeBlockFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    return factory;
}

// ---------------------------------------------------------------------------------------------------------------------
// PreparedLayout::TryVisibleRange.

// The algorithm before W7.6, verbatim: every line's first offset is read from DirectWrite's hit test, from line 0, until a
// line starts at or below `bottom`. The line starts are the cumulative line lengths, as the layout keeps them.
HRESULT ReferenceVisibleRange(const PreparedLayout& layout, float top, float bottom, std::pair<std::size_t, std::size_t>& output)
{
    if (! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        return E_INVALIDARG;
    std::vector<std::size_t> lines{0u};
    for (const auto& line : Detail::PreparedLayoutAccess::LineMetrics(layout))
        lines.push_back(lines.back() + line.length);
    std::optional<std::size_t> first;
    std::size_t last = 0u;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        CaretGeometry caret{};
        const auto hr = layout.TryCaret(lines[index], caret);
        if (FAILED(hr))
            return hr;
        if (caret.y >= bottom)
            break;
        if (caret.y + caret.height > top)
        {
            if (! first)
                first = lines[index];
            last = index + 1u < lines.size() ? lines[index + 1u] : layout.Content().text.size();
        }
    }
    output = {first.value_or(0u), last};
    return S_OK;
}

struct SweepResult
{
    std::size_t queries = 0u, mismatches = 0u, lines = 0u;
    std::uint64_t maximumHitTests  = 0u;
    std::uint64_t totalHitTests    = 0u;
    std::uint64_t referenceMaximum = 0u, referenceTotal = 0u; // caret reads of the line-by-line reference
};

bool CompareQuery(const PreparedLayout& layout, float top, float bottom, SweepResult& result, const char* name)
{
    std::pair<std::size_t, std::size_t> expected{7u, 7u}, actual{7u, 7u};
    const DxUi::Detail::WorkCounterScope referenceScope;
    const auto expectedStatus = ReferenceVisibleRange(layout, top, bottom, expected);
    const auto referenceReads = referenceScope.Delta().rich.caretHitTests;
    result.referenceMaximum   = std::max(result.referenceMaximum, referenceReads);
    result.referenceTotal += referenceReads;
    const DxUi::Detail::WorkCounterScope scope;
    const auto actualStatus = layout.TryVisibleRange(top, bottom, actual);
    const auto hitTests     = scope.Delta().rich.caretHitTests;
    ++result.queries;
    result.maximumHitTests = std::max(result.maximumHitTests, hitTests);
    result.totalHitTests += hitTests;
    if (expectedStatus == actualStatus && (FAILED(expectedStatus) || expected == actual))
        return true;
    if (result.mismatches++ < 6u)
        std::cout << std::setprecision(10) << "  MISMATCH " << name << " top=" << top << " bottom=" << bottom << " expected=" << expected.first << ','
                  << expected.second << " (" << std::hex << expectedStatus << std::dec << ") actual=" << actual.first << ',' << actual.second << " ("
                  << std::hex << actualStatus << std::dec << ")\n";
    return false;
}

// Every line boundary of the layout (the caret box's top and bottom of the lines at `stride`, with the first and last
// lines always), the value one single-precision unit below and above it, and windows that end or begin there with a
// length of 0, one unit, one line and two and a half lines: every one is compared with the reference.
SweepResult Sweep(const char* name, const PreparedLayout& layout, std::size_t stride)
{
    SweepResult result;
    std::vector<std::size_t> starts{0u};
    for (const auto& line : Detail::PreparedLayoutAccess::LineMetrics(layout))
        starts.push_back(starts.back() + line.length);
    result.lines             = starts.size();
    constexpr float infinity = std::numeric_limits<float>::infinity();
    const auto probe         = [&](float value, float height)
    {
        for (const float center : {std::nextafter(value, -infinity), value, std::nextafter(value, infinity)})
            for (const float length : {0.0f, std::nextafter(center, infinity) - center, height, height * 2.5f})
            {
                CompareQuery(layout, center, center + length, result, name);
                CompareQuery(layout, center - length, center, result, name);
            }
    };
    for (std::size_t index = 0u; index < starts.size(); ++index)
    {
        const bool edge = index < 3u || index + 3u >= starts.size();
        if (! edge && index % stride != 0u)
            continue;
        CaretGeometry caret{};
        Require(SUCCEEDED(layout.TryCaret(starts[index], caret)), "the sweep reads a line's caret");
        probe(caret.y, caret.height);
        probe(caret.y + caret.height, caret.height);
    }
    const float height = layout.Height();
    for (const float value : {-1000000.0f, -1.0f, 0.0f, height, height + 1.0f, 1000000.0f})
        probe(value, std::max(1.0f, layout.Caret(0).height));
    CompareQuery(layout, -1.0e30f, 1.0e30f, result, name);
    // Refusals are the same: a reversed window and a non-finite end.
    std::pair<std::size_t, std::size_t> ignored{};
    Require(layout.TryVisibleRange(2.0f, 1.0f, ignored) == E_INVALIDARG && layout.TryVisibleRange(std::nanf(""), 1.0f, ignored) == E_INVALIDARG &&
                layout.TryVisibleRange(0.0f, infinity, ignored) == E_INVALIDARG,
            "a reversed or non-finite window is refused");
    std::cout << "  visible range " << name << ": " << result.lines << " lines, " << result.queries << " queries, " << result.mismatches
              << " mismatches, hit tests per query max " << result.maximumHitTests << " mean " << (result.queries ? result.totalHitTests / result.queries : 0u)
              << "; line-by-line reference max " << result.referenceMaximum << " mean " << (result.queries ? result.referenceTotal / result.queries : 0u)
              << '\n';
    return result;
}

std::unique_ptr<PreparedLayout> Prepared(IDWriteFactory* factory, const std::wstring& text, LayoutOptions options, ViewMode mode = ViewMode::Source)
{
    Document document(text);
    auto layout       = std::make_unique<PreparedLayout>();
    const auto status = layout->TryPrepare(factory, Project(document.Snapshot(), document.Revision(), mode), options);
    Require(SUCCEEDED(status) && layout->Ready(), "the visible-range fixture prepares");
    return layout;
}

std::wstring LatinLines(int count)
{
    std::wstring text;
    for (int i = 0; i < count; ++i)
        text += L"line " + std::to_wstring(i) + L" the quick brown fox jumps over the lazy dog again and again and once more\n";
    return text;
}

LayoutOptions NarrowOptions(float multiplier, DWRITE_READING_DIRECTION direction = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT)
{
    LayoutOptions options;
    options.widthDip             = 173.3f;
    options.fontSizeDip          = 13.7f;
    options.lineHeightMultiplier = multiplier;
    options.readingDirection     = direction;
    return options;
}

void TestRichCodeBlockVisibleRangeEqualsTheReference()
{
    const auto factory = CodeBlockFactory();
    SweepResult total;
    const auto run = [&](const char* name, const std::wstring& text, LayoutOptions options, std::size_t stride = 1u)
    {
        const auto layout = Prepared(factory.get(), text, options);
        const auto result = Sweep(name, *layout, stride);
        total.queries += result.queries;
        total.mismatches += result.mismatches;
    };
    // Wrapped Latin, without and with a line-height multiplier (the caret box is then the glyph box inside a taller line).
    run("latin wrapped", LatinLines(120), NarrowOptions(0.0f));
    run("latin wrapped, multiplier 1.65", LatinLines(120), NarrowOptions(1.65f));
    run("latin wrapped, multiplier 1.0", LatinLines(60), NarrowOptions(1.0f));
    // Right-to-left and mixed bidirectional text, in both reading directions.
    std::wstring hebrew, mixed;
    for (int i = 0; i < 80; ++i)
    {
        hebrew += L"\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd \u05d6\u05d4\u05d5 \u05d8\u05e7\u05e1\u05d8 \u05d0\u05e8\u05d5\u05da "
                  L"\u05de\u05d0\u05d5\u05d3 \u05e9\u05e0\u05d2\u05dc\u05e9 " +
                  std::to_wstring(i) + L"\n";
        mixed += L"abc \u05e9\u05dc\u05d5\u05dd def \u0645\u0631\u062d\u0628\u0627 " + std::to_wstring(i) +
                 L" and a few more words that wrap around the narrow width\n";
    }
    run("hebrew, right to left", hebrew, NarrowOptions(0.0f, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    run("hebrew, left to right", hebrew, NarrowOptions(1.65f));
    run("mixed bidi, left to right", mixed, NarrowOptions(0.0f));
    run("mixed bidi, right to left, multiplier", mixed, NarrowOptions(1.5f, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    // Empty lines, a trailing newline (the empty last line), one line, and no text.
    run("empty lines", L"a\n\n\nb\n\n\n\nc\n\n", NarrowOptions(0.0f));
    run("empty lines, multiplier", L"a\n\n\nb\n\n\n\nc\n\n", NarrowOptions(1.65f));
    run("trailing newline", L"abc\ndef\n", NarrowOptions(0.0f));
    run("trailing newline, multiplier", L"abc\ndef\n", NarrowOptions(1.65f));
    run("only a newline", L"\n", NarrowOptions(0.0f));
    run("one line", L"abc", NarrowOptions(0.0f));
    run("empty", L"", NarrowOptions(0.0f));
    // A layout of more lines than the hint keeps sums for, with a trailing newline.
    std::wstring many;
    for (int i = 0; i < 200; ++i)
        many += (i % 7 == 3 ? L"\n" : L"x\n");
    run("many short lines, trailing newline", many, NarrowOptions(0.0f));
    run("many short lines, multiplier", many, NarrowOptions(1.65f));
    Require(total.mismatches == 0u && total.queries > 20000u, "TryVisibleRange answers every swept window as the line-by-line reference does");
}

// The largest layouts the code allows: a source of Document::MaximumUnits (65,536) units of newlines, the most lines one
// layout holds, and the semantic display's own maximum of twice that. The reference reads a caret per line up to the
// window, so the windows are sampled across the layout; their ends sit on and one unit either side of the line boundaries.
void TestRichCodeBlockVisibleRangeOfTheLargestLayouts()
{
    const auto factory = CodeBlockFactory();
    SweepResult total;
    {
        LayoutOptions options;
        options.widthDip    = 300.0f;
        options.fontSizeDip = 14.0f;
        const auto layout   = Prepared(factory.get(), std::wstring(Document::MaximumUnits, L'\n'), options);
        const auto result   = Sweep("65,536 newlines", *layout, 10007u);
        total.queries += result.queries;
        total.mismatches += result.mismatches;
        Require(result.maximumHitTests <= 40u, "a window of the largest layout reads a bounded number of caret boxes");
    }
    {
        LayoutOptions options;
        options.widthDip             = 300.0f;
        options.fontSizeDip          = 14.0f;
        options.lineHeightMultiplier = 1.65f;
        std::wstring text;
        for (std::size_t line = 0u; text.size() + 2u <= Document::MaximumUnits; ++line)
            text += line % 3u == 0u ? L"x\n" : L"\n";
        const auto layout = Prepared(factory.get(), text, options);
        const auto result = Sweep("65,536 units, multiplier", *layout, 6007u);
        total.queries += result.queries;
        total.mismatches += result.mismatches;
        Require(result.maximumHitTests <= 40u, "a window of the largest multiplier layout reads a bounded number of caret boxes");
    }
    {
        // The semantic display's maximum: twice the source maximum, laid out through the semantic entry point.
        LayoutOptions options;
        options.widthDip    = 300.0f;
        options.fontSizeDip = 14.0f;
        Detail::PreparedLayoutAccess::SemanticInput input;
        input.text.assign(Document::MaximumUnits * 2u, L'\n');
        input.revision = 1u;
        PreparedLayout layout;
        const auto status = Detail::PreparedLayoutAccess::PrepareSemantic(layout, factory.get(), input, options);
        Require(SUCCEEDED(status) && layout.Ready(), "the largest semantic display layout prepares");
        const auto result = Sweep("131,072 semantic newlines", layout, 20011u);
        total.queries += result.queries;
        total.mismatches += result.mismatches;
        Require(result.maximumHitTests <= 40u, "a window of the largest semantic layout reads a bounded number of caret boxes");
    }
    Require(total.mismatches == 0u && total.queries > 250u, "TryVisibleRange equals the reference on the largest layouts");
}

// Why the sums of the line heights are a hint and not the answer: DirectWrite's caret tops are not those sums. With a
// line-height multiplier the caret box is the glyph box inside the line (its height is not the line's and its top is
// offset), and without one the tops are DirectWrite's own single-precision accumulation, which differs from a float sum of
// the line metrics in the last unit. Printed, so the claim stays checkable; only the structural fact is required.
void TestRichCodeBlockLineHeightSumsAreNotCaretTops()
{
    const auto factory = CodeBlockFactory();
    for (const float multiplier : {0.0f, 1.65f})
    {
        const auto layout  = Prepared(factory.get(), LatinLines(300), NarrowOptions(multiplier));
        const auto metrics = Detail::PreparedLayoutAccess::LineMetrics(*layout);
        float sum          = 0.0f;
        std::size_t start = 0u, topMismatches = 0u, heightMismatches = 0u;
        std::optional<std::size_t> firstTop;
        for (std::size_t line = 0u; line < metrics.size(); ++line)
        {
            const auto caret = layout->Caret(start);
            if (caret.y != sum)
            {
                ++topMismatches;
                if (! firstTop)
                {
                    firstTop = line;
                    std::cout << std::setprecision(9) << "  multiplier " << multiplier << " first line whose caret top is not the float sum: line " << line
                              << " caret.y " << caret.y << " sum " << sum << " line height " << metrics[line].height << " caret height " << caret.height
                              << '\n';
                }
            }
            if (caret.height != metrics[line].height)
                ++heightMismatches;
            sum += metrics[line].height;
            start += metrics[line].length;
        }
        std::cout << "  multiplier " << multiplier << ": " << metrics.size() << " lines, caret top differs from the float sum on " << topMismatches
                  << ", caret height differs from the line height on " << heightMismatches << '\n';
        if (multiplier > 0.0f)
            Require(heightMismatches == metrics.size(), "with a line-height multiplier the caret box is not the line box");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Semantic geometry: visible-range queries, the layout cache.

using DxUi::RichText::Detail::PreparedSemanticGeometry;

std::shared_ptr<const SemanticDocument> SemanticOf(std::wstring_view source)
{
    return RichSemanticDocumentTests::Build(source, MarkdownProfile::RichMarkdown);
}

std::wstring Fence(const wchar_t* language, std::size_t lines)
{
    std::wstring text = L"```";
    text += language;
    text += L"\n";
    for (std::size_t line = 0u; line < lines; ++line)
        text += L"call(" + std::to_wstring(line) + L");\n";
    text += L"```\n\n";
    return text;
}

// Prose, three fences (two of one language) and a list, a table, a quote and an image: every kind of box the semantic
// queries visit.
std::wstring CodeSource(std::size_t first, std::size_t second, std::size_t third, std::wstring_view prose = L"alpha")
{
    std::wstring text = L"# Heading\n\nParagraph " + std::wstring(prose) + L" with **bold** text.\n\n" + Fence(L"cpp", first) + L"Between the fences.\n\n" +
                        Fence(L"cpp", second) + Fence(L"python", third) + L"- item one\n- item two\n  - nested\n\n1. first\n2. second\n\n> quoted text\n\n" +
                        L"| a | b |\n| - | - |\n| 1 | 2 |\n| 3 | 4 |\n\n![alt text](picture.png)\n\nEnd paragraph.\n";
    return text;
}

LayoutOptions CodeOptions()
{
    LayoutOptions options;
    options.widthDip             = 420.0f;
    options.fontSizeDip          = 15.0f;
    options.codeFontSizeDip      = 14.0f;
    options.markdownPresentation = MarkdownPresentationStyle::Subtle;
    return options;
}

struct CodeLayoutWork
{
    std::uint64_t numbers = 0u, badges = 0u;
};
CodeLayoutWork Prepare(PreparedSemanticGeometry& geometry,
                       IDWriteFactory* factory,
                       const std::shared_ptr<const SemanticDocument>& document,
                       const LayoutOptions& options)
{
    const DxUi::Detail::WorkCounterScope scope;
    geometry.Begin(factory, document, options);
    RichSemanticGeometryTests::Complete(geometry);
    const auto delta = scope.Delta();
    return {delta.rich.codeNumberLayouts, delta.rich.codeBadgeLayouts};
}

std::wstring Replace(std::wstring text, std::wstring_view from, std::wstring_view to)
{
    const auto at = text.find(from);
    Require(at != std::wstring::npos, "the edit's anchor exists");
    text.replace(at, from.size(), to);
    return text;
}

bool SameBits(float a, float b) noexcept
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

// Two geometries draw the same line numbers: the same layouts (text, metrics, caret box, ink) at the same origins.
bool SameNumbers(const PreparedSemanticGeometry& a, const PreparedSemanticGeometry& b)
{
    if (a.CodeNumberCount() != b.CodeNumberCount())
        return false;
    for (std::size_t index = 0u; index < a.CodeNumberCount(); ++index)
    {
        const auto &x = a.CodeNumberLayout(index), &y = b.CodeNumberLayout(index);
        const auto ox = a.CodeNumberOrigin(index), oy = b.CodeNumberOrigin(index);
        const auto cx = x.Caret(0u), cy = y.Caret(0u);
        const auto hx = Detail::PreparedLayoutAccess::HorizontalTextBounds(x), hy = Detail::PreparedLayoutAccess::HorizontalTextBounds(y);
        const auto vx = Detail::PreparedLayoutAccess::DrawingVerticalBounds(x), vy = Detail::PreparedLayoutAccess::DrawingVerticalBounds(y);
        if (x.Content().text != y.Content().text || ! SameBits(x.Height(), y.Height()) || ! SameBits(cx.x, cy.x) || ! SameBits(cx.y, cy.y) ||
            ! SameBits(cx.height, cy.height) || ! SameBits(hx.first, hy.first) || ! SameBits(hx.second, hy.second) || ! SameBits(vx.first, vy.first) ||
            ! SameBits(vx.second, vy.second) || ! SameBits(ox.x, oy.x) || ! SameBits(ox.y, oy.y))
            return false;
    }
    return true;
}

// The visible-range queries find boxes through the paint index: identical answers to visiting every box, for scroll
// positions across the document and window lengths from empty to the whole document, in both presentations and reading
// directions.
void TestRichCodeBlockSemanticVisibleRangesEqualTheFullScan()
{
    const auto factory   = RichSemanticGeometryTests::Factory();
    const auto document  = SemanticOf(CodeSource(30u, 6u, 9u));
    std::size_t compared = 0u, nonEmpty = 0u, mismatches = 0u;
    for (const auto style : {MarkdownPresentationStyle::Default, MarkdownPresentationStyle::Subtle})
        for (const bool rtl : {false, true})
        {
            auto options                 = CodeOptions();
            options.markdownPresentation = style;
            options.readingDirection     = rtl ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
            PreparedSemanticGeometry geometry;
            geometry.Begin(factory.get(), document, options);
            RichSemanticGeometryTests::Complete(geometry);
            const float height = geometry.Height();
            Require(height > 400.0f, "the document is taller than a viewport");
            for (float top = -20.0f; top < height + 30.0f; top += 3.7f)
                for (const float length : {0.0f, 0.001f, 40.0f, 120.0f, 333.0f, height + 100.0f})
                {
                    std::pair<std::size_t, std::size_t> indexed{1u, 1u}, scanned{2u, 2u}, indexedSource{1u, 1u}, scannedSource{2u, 2u};
                    const auto a  = geometry.TryVisibleRange(top, top + length, indexed);
                    const auto b  = geometry.TryVisibleRangeByFullScan(top, top + length, scanned);
                    const auto c  = geometry.TryVisibleSourceRange(top, top + length, indexedSource);
                    const auto d  = geometry.TryVisibleSourceRangeByFullScan(top, top + length, scannedSource);
                    const bool ok = a == b && c == d && (FAILED(a) || indexed == scanned) && (FAILED(c) || indexedSource == scannedSource);
                    ++compared;
                    nonEmpty += indexed.first != indexed.second;
                    if (! ok && mismatches++ < 5u)
                        std::cout << "  MISMATCH top=" << top << " length=" << length << " indexed " << indexed.first << ',' << indexed.second << " scanned "
                                  << scanned.first << ',' << scanned.second << " source " << indexedSource.first << ',' << indexedSource.second << " vs "
                                  << scannedSource.first << ',' << scannedSource.second << '\n';
                }
            std::pair<std::size_t, std::size_t> ignored{};
            Require(geometry.TryVisibleRange(2.0f, 1.0f, ignored) == E_INVALIDARG && geometry.TryVisibleRangeByFullScan(2.0f, 1.0f, ignored) == E_INVALIDARG &&
                        geometry.TryVisibleSourceRange(std::nanf(""), 1.0f, ignored) == E_INVALIDARG,
                    "a reversed or non-finite window is refused by both");
        }
    std::cout << "  semantic visible ranges: " << compared << " windows compared, " << nonEmpty << " non-empty, " << mismatches << " mismatches\n";
    Require(mismatches == 0u && compared > 4000u && nonEmpty > 1000u, "the indexed visible ranges equal the full scans at every scroll position");
}

// Line numbers and badges are built once per distinct number and language, and carried across edits.
void TestRichCodeBlockLayoutsAreSharedAcrossEdits()
{
    const auto factory = RichSemanticGeometryTests::Factory();
    const auto options = CodeOptions();
    PreparedSemanticGeometry geometry;
    const auto base = Prepare(geometry, factory.get(), SemanticOf(CodeSource(40u, 5u, 10u)), options);
    std::cout << "  first preparation: " << geometry.CodeNumberCount() << " numbers drawn, " << base.numbers << " number layouts, " << base.badges
              << " badge layouts, " << geometry.CodeLayoutCacheEntries() << " cache entries\n";
    Require(geometry.CodeNumberCount() == 55u && base.numbers == 40u && base.badges == 2u,
            "the first preparation builds one layout per distinct number (1 to 40, shared by the three fences) and one badge per language");
    Require(geometry.CodeLayoutCacheEntries() == 42u && geometry.CodeLayoutCacheBytes() > 0u, "the cache holds those layouts");
    // A prose insert builds none; every later publication carries them.
    const auto prose = Prepare(geometry, factory.get(), SemanticOf(CodeSource(40u, 5u, 10u, L"alphax")), options);
    Require(prose.numbers == 0u && prose.badges == 0u && geometry.CodeNumberCount() == 55u, "a prose insert builds no number and no badge layout");
    // A character typed inside a fence builds none.
    const auto character = Prepare(geometry, factory.get(), SemanticOf(Replace(CodeSource(40u, 5u, 10u), L"call(20);", L"call(20)x;")), options);
    Require(character.numbers == 0u && character.badges == 0u, "a character typed in a fence builds no number and no badge layout");
    // A line added to the second fence takes the number 6, which the first fence's numbers include.
    const auto sixth = Prepare(geometry, factory.get(), SemanticOf(CodeSource(40u, 6u, 10u)), options);
    Require(geometry.CodeNumberCount() == 56u && sixth.numbers == 0u && sixth.badges == 0u,
            "a line added to a short fence builds only the numbers no fence has");
    // A line added to the first fence is the new number 41: one layout.
    const auto forty1 = Prepare(geometry, factory.get(), SemanticOf(CodeSource(41u, 6u, 10u)), options);
    Require(geometry.CodeNumberCount() == 57u && forty1.numbers == 1u && forty1.badges == 0u,
            "a line added to the longest fence builds exactly the new number");
    // Each line removed leaves the layouts for the next edit that adds it back, until a publication drops what it did not use.
    const auto removed = Prepare(geometry, factory.get(), SemanticOf(CodeSource(40u, 6u, 10u)), options);
    Require(removed.numbers == 0u && removed.badges == 0u, "a line removed builds no layout");
    // Nothing was built by an identical document either.
    const auto same = Prepare(geometry, factory.get(), SemanticOf(CodeSource(40u, 6u, 10u)), options);
    Require(same.numbers == 0u && same.badges == 0u, "an unchanged document builds none");
    // Fences of different gutter formats do not share: a 1,200-line fence (four digits, a wider gutter) next to a short one.
    PreparedSemanticGeometry wide;
    const auto mixed = Prepare(wide, factory.get(), SemanticOf(CodeSource(1200u, 5u, 3u)), options);
    std::cout << "  1,200-line fence beside a 5-line one: " << mixed.numbers << " number layouts\n";
    Require(mixed.numbers == 1200u + 5u, "a wider gutter is another format: the short fences' numbers are built again for it");
}

// A change of anything a number or badge layout is made from is a different layout, never a stale one: after each change the
// geometry draws exactly the numbers a geometry with no cache draws.
void TestRichCodeBlockLayoutsAreNeverStale()
{
    const auto factory  = RichSemanticGeometryTests::Factory();
    const auto document = SemanticOf(CodeSource(40u, 5u, 10u));
    struct Change
    {
        const char* name;
        void (*apply)(LayoutOptions&);
        bool rebuilds;
    };
    const Change changes[] = {
        {"code font family", [](LayoutOptions& options) { options.codeFontFamily = L"Courier New"; }, true},
        {"code font size (gutter format)", [](LayoutOptions& options) { options.codeFontSizeDip = 26.0f; }, true},
        {"code font size (same gutter)", [](LayoutOptions& options) { options.codeFontSizeDip = 12.0f; }, true},
        {"dpi", [](LayoutOptions& options) { options.dpi = 144.0f; }, true},
        {"reading direction", [](LayoutOptions& options) { options.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT; }, true},
        {"locale", [](LayoutOptions& options) { options.locale = L"ar-sa"; }, true},
        {"line height", [](LayoutOptions& options) { options.lineHeightMultiplier = 1.6f; }, true},
        {"width (the gutter stays, the badges' width changes)", [](LayoutOptions& options) { options.widthDip = 333.0f; }, false},
        {"body font family (not a number's font)", [](LayoutOptions& options) { options.fontFamily = L"Arial"; }, false},
        {"code copy action (not a layout input)", [](LayoutOptions& options) { options.codeCopyAction = true; }, false},
    };
    for (const auto& change : changes)
    {
        PreparedSemanticGeometry cached;
        static_cast<void>(Prepare(cached, factory.get(), document, CodeOptions()));
        auto options = CodeOptions();
        change.apply(options);
        const auto rebuilt = Prepare(cached, factory.get(), document, options);
        PreparedSemanticGeometry fresh;
        static_cast<void>(Prepare(fresh, factory.get(), document, options));
        std::cout << "  " << change.name << ": " << rebuilt.numbers << " number layouts, " << rebuilt.badges << " badge layouts rebuilt\n";
        Require(SameNumbers(cached, fresh), "after the change the cached geometry draws the numbers a fresh geometry draws");
        if (change.rebuilds)
            Require(rebuilt.numbers > 0u, "a change of what a number layout is made from builds new number layouts");
        else
            Require(rebuilt.numbers == 0u, "a change that is no input of a number layout builds none");
    }
}

// The cache's bound: the entries of a generation, and what a publication drops.
void TestRichCodeBlockLayoutCacheIsBounded()
{
    const auto factory = RichSemanticGeometryTests::Factory();
    auto options       = CodeOptions();
    PreparedSemanticGeometry geometry;
    // Two fences of 2,500 lines: 5,000 numbered lines, of which the geometry draws the first 4,096, from 2,500 distinct numbers.
    const auto source = Fence(L"cpp", 2500u) + Fence(L"cpp", 2500u);
    const auto first  = Prepare(geometry, factory.get(), SemanticOf(source), options);
    std::cout << "  5,000 numbered lines: " << geometry.CodeNumberCount() << " drawn, " << first.numbers << " layouts built, "
              << geometry.CodeLayoutCacheEntries() << " cache entries, " << geometry.CodeLayoutCacheBytes() << " bytes\n";
    Require(geometry.CodeNumberCount() == 4096u && first.numbers == 2500u && geometry.CodeLayoutCacheEntries() == 2501u,
            "a geometry draws at most 4,096 numbers, built from the distinct ones");
    // A new format of every number: while it is prepared the cache holds both generations up to its bound, and the
    // publication keeps only what the new generation uses.
    options.codeFontFamily = L"Courier New";
    const auto second      = Prepare(geometry, factory.get(), SemanticOf(source), options);
    Require(second.numbers == 2500u && geometry.CodeLayoutCacheEntries() == 2501u, "a publication drops the entries its generation did not use");
    Require(geometry.CodeLayoutCacheEntries() <= Detail::CodeLayoutCache::MaximumNumbers + Detail::CodeLayoutCache::MaximumBadges,
            "the cache never exceeds its bound");
    // Clearing the geometry releases the cache.
    geometry.Clear();
    Require(geometry.CodeLayoutCacheEntries() == 0u && geometry.CodeLayoutCacheBytes() == 0u, "clearing releases the cache");
}

// What bounds one Advance call when every number is a cache hit: `while (result.blockSteps < budget.maximumBlockSteps)`
// at the head of PreparedSemanticGeometry::Advance's loop, and a number placed (hit or built) ends its iteration in
// `++result.blockSteps`; only a miss also spends `maximumLayouts`. On a 2,000-line fence, every number a hit.
void TestRichCodeBlockTurnsAreBoundedWhenEveryNumberHits()
{
    const auto factory = RichSemanticGeometryTests::Factory();
    const auto options = CodeOptions();
    PreparedSemanticGeometry geometry;
    static_cast<void>(Prepare(geometry, factory.get(), SemanticOf(CodeSource(2000u, 5u, 10u)), options));
    for (const std::size_t steps : {std::size_t{16u}, std::size_t{64u}, std::size_t{256u}})
    {
        const DxUi::Detail::WorkCounterScope scope;
        geometry.Begin(factory.get(), SemanticOf(CodeSource(2000u, 5u, 10u, L"alphax")), options);
        std::size_t turns = 0u, largestSteps = 0u, totalSteps = 0u, largestLayouts = 0u;
        while (geometry.Pending())
        {
            const auto result = geometry.Advance({1u, steps});
            ++turns;
            largestSteps   = std::max(largestSteps, result.blockSteps);
            largestLayouts = std::max(largestLayouts, result.layoutsPrepared);
            totalSteps += result.blockSteps;
            Require(turns < 100000u, "the preparation finishes");
        }
        const auto delta = scope.Delta();
        std::cout << "  2,000-line fence, every number a hit, " << steps << " block steps per turn: " << turns << " owner turns, " << totalSteps
                  << " block steps, at most " << largestSteps << " in one turn, " << largestLayouts << " layouts, " << delta.rich.codeNumberLayouts
                  << " number layouts built\n";
        Require(geometry.Ready() && delta.rich.codeNumberLayouts == 0u && delta.rich.codeBadgeLayouts == 0u, "every number and badge was a hit");
        Require(largestSteps <= steps && largestLayouts <= 1u,
                "a turn of hits stays within the block-step budget (and the layout budget of the paragraphs it still builds)");
        Require(turns >= 2000u / steps, "the 2,000 numbers need at least that many turns of the budget's size");
    }
}

// The cache serves layouts of the factory it holds only: bound to another factory it drops everything, and an address a
// released factory had cannot return its layouts.
void TestRichCodeBlockCacheFollowsItsFactory()
{
    const auto isolated = []
    {
        wil::com_ptr<IDWriteFactory> factory;
        THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
        return factory;
    };
    const auto document = SemanticOf(CodeSource(40u, 5u, 10u));
    const auto options  = CodeOptions();
    PreparedSemanticGeometry geometry;
    auto first = isolated();
    Require(Prepare(geometry, first.get(), document, options).numbers == 40u && geometry.CodeLayoutCacheEntries() == 42u,
            "the first factory builds the layouts");
    const void* const firstAddress = first.get();
    auto second                    = isolated();
    Require(second.get() != first.get(), "an isolated factory is a distinct object");
    const auto switched = Prepare(geometry, second.get(), document, options);
    Require(switched.numbers == 40u && switched.badges == 2u && geometry.CodeLayoutCacheEntries() == 42u,
            "another factory builds every layout again and the first factory's entries are gone");
    // Release both and make others: whatever address they get, the cache holds its factory, so none is the address of a
    // released factory it still names.
    first.reset();
    int reused = 0;
    for (int round = 0; round < 8; ++round)
    {
        auto next = isolated();
        reused += next.get() == firstAddress;
        PreparedSemanticGeometry fresh;
        const auto built = Prepare(geometry, next.get(), document, options);
        static_cast<void>(Prepare(fresh, next.get(), document, options));
        Require(built.numbers == 40u && SameNumbers(geometry, fresh), "a new factory, whatever its address, never gets a layout of another");
        second = std::move(next);
    }
    std::cout << "  factories: the first factory's address was handed to a later factory " << reused << " of 8 times\n";
}

// PreparedLayout::TryVisibleRange decides at preparation whether its caret boxes are stacked (searchable) or not (stepped),
// and each layout takes the path its fixture is built for. Layouts of mixed run sizes (a heading line after a body line and
// the other way), at line-height multipliers 0, 0.8, 1.0 and 1.65, are stepped and equal the reference; made to search
// (the negative control), they report how often the search would have been wrong.
void TestRichCodeBlockStackedGateAndMixedRunSizes()
{
    const auto factory = CodeBlockFactory();
    std::wstring mixed;
    for (int i = 0; i < 40; ++i)
        mixed += L"body text of line " + std::to_wstring(i) + L" that wraps around the narrow width of the layout\n# Heading " + std::to_wstring(i) +
                 L"\nsmall\n\n# A\n";
    {
        // Preparation refuses a multiplier below 1, so no layout has a line shorter than the font's own height that way. The
        // multipliers 1.0 and 1.15 are where the glyph box (about 1.33 em in Segoe UI) is still taller than the line.
        PreparedLayout refused;
        Document probe(L"abc\ndef");
        Require(FAILED(refused.TryPrepare(factory.get(), Project(probe.Snapshot(), probe.Revision(), ViewMode::Source), NarrowOptions(0.8f))),
                "a multiplier below 1 is refused");
    }
    SweepResult stepped;
    for (const float multiplier : {0.0f, 1.0f, 1.15f, 1.65f})
    {
        const auto layout = Prepared(factory.get(), mixed, NarrowOptions(multiplier), ViewMode::Rich);
        Require(! Detail::PreparedLayoutAccess::CaretBoxesStacked(*layout), "a layout of mixed run sizes is not decided to be stacked");
        const auto result = Sweep("mixed run sizes (stepped)", *layout, 2u);
        stepped.queries += result.queries;
        stepped.mismatches += result.mismatches;
        Detail::testRichForceVisibleRangeSearch = true;
        const auto restore                      = wil::scope_exit([]() noexcept { Detail::testRichForceVisibleRangeSearch = false; });
        const auto forced                       = Sweep("mixed run sizes, search forced", *layout, 2u);
        std::cout << "  multiplier " << multiplier << ": a search forced on mixed run sizes differs from the reference in " << forced.mismatches << " of "
                  << forced.queries << " queries\n";
    }
    Require(stepped.mismatches == 0u && stepped.queries > 4000u, "mixed run sizes equal the reference at multipliers 0, 0.8, 1.0 and 1.65");
    // Lines whose first glyph comes from a fallback font (CJK, emoji, Hebrew) among ASCII lines, in uniform spacing: the
    // other way a caret box can move inside its line. Stepped by the gate; the forced search is the negative control.
    std::wstring fallback;
    for (int i = 0; i < 60; ++i)
        fallback += L"abc ordinary line\n中文字 line\n\U0001F600 emoji line\nאב text\nxyz\n";
    for (const float multiplier : {1.0f, 1.15f, 1.65f})
    {
        const auto layout = Prepared(factory.get(), fallback, NarrowOptions(multiplier));
        Require(! Detail::PreparedLayoutAccess::CaretBoxesStacked(*layout), "fallback fonts in uniform spacing are not decided to be stacked");
        const auto result = Sweep("fallback fonts (stepped)", *layout, 3u);
        Require(result.mismatches == 0u, "fallback fonts equal the reference");
        Detail::testRichForceVisibleRangeSearch = true;
        const auto restore                      = wil::scope_exit([]() noexcept { Detail::testRichForceVisibleRangeSearch = false; });
        const auto forced                       = Sweep("fallback fonts, search forced", *layout, 3u);
        std::cout << "  multiplier " << multiplier << ": a search forced on fallback fonts differs from the reference in " << forced.mismatches << " of "
                  << forced.queries << " queries\n";
    }
    // The gate: which layouts search.
    const std::wstring ascii = LatinLines(30);
    for (const float multiplier : {0.0f, 1.15f, 1.0f, 1.65f})
        Require(Detail::PreparedLayoutAccess::CaretBoxesStacked(*Prepared(factory.get(), ascii, NarrowOptions(multiplier))),
                "one style of printable ASCII searches at every multiplier");
    const std::wstring hebrew = L"שלום עולם זהו טקסט\n";
    Require(Detail::PreparedLayoutAccess::CaretBoxesStacked(*Prepared(factory.get(), hebrew + hebrew, NarrowOptions(0.0f))),
            "one style in natural spacing searches whatever its script");
    Require(! Detail::PreparedLayoutAccess::CaretBoxesStacked(*Prepared(factory.get(), hebrew + hebrew, NarrowOptions(1.65f))),
            "one style in uniform spacing searches only printable ASCII");
}

// The document-level query: chunks whose caret boxes are stacked are searched, the others stepped; both equal stepping
// through every chunk's lines from the first, at every line boundary and one unit either side, for Source and
// multiplier documents of many chunks and for a document of mixed run sizes.
void TestRichCodeBlockDocumentVisibleRangeEqualsStepping()
{
    const auto factory = CodeBlockFactory();
    std::wstring ascii, mixed;
    for (int i = 0; i < 160; ++i)
    {
        ascii += L"line " + std::to_wstring(i) + L" carries ordinary words so that it wraps at narrow widths\n";
        mixed += L"body line " + std::to_wstring(i) + L" of ordinary words\n# Heading " + std::to_wstring(i) + L"\n";
    }
    struct Case
    {
        const char* name;
        const std::wstring* text;
        ViewMode mode;
        float multiplier;
    };
    const Case cases[]       = {{"source", &ascii, ViewMode::Source, 0.0f},
                                {"multiplier 1.5", &ascii, ViewMode::Source, 1.5f},
                                {"rich multiplier 1.5", &ascii, ViewMode::Rich, 1.5f},
                                {"mixed run sizes", &mixed, ViewMode::Rich, 0.0f},
                                {"mixed run sizes, multiplier 1.15", &mixed, ViewMode::Rich, 1.15f}};
    constexpr float infinity = std::numeric_limits<float>::infinity();
    for (const auto& item : cases)
    {
        Document document(*item.text);
        PreparedDocumentLayout prepared;
        LayoutOptions options = NarrowOptions(item.multiplier);
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), item.mode, options);
        Require(prepared.Ready() && prepared.ChunkCount() > 1u, "the document is laid out in several chunks");
        // Line starts: the first offset of the line at every 41st display offset.
        std::vector<std::size_t> starts;
        for (std::size_t position = 0u; position <= prepared.DisplayLength(); position += 41u)
        {
            const auto start = prepared.LineBoundary(position, false);
            if (starts.empty() || starts.back() != start)
                starts.push_back(start);
        }
        std::size_t queries = 0u, mismatches = 0u;
        std::uint64_t searchMax = 0u, searchTotal = 0u, stepMax = 0u, stepTotal = 0u;
        const auto compare = [&](float top, float bottom)
        {
            std::pair<std::size_t, std::size_t> found{3u, 3u}, expected{4u, 4u};
            const DxUi::Detail::WorkCounterScope searchScope;
            const auto a     = prepared.TryVisibleRange(top, bottom, found);
            const auto reads = searchScope.Delta().rich.caretHitTests;
            const DxUi::Detail::WorkCounterScope stepScope;
            const auto b     = Detail::PreparedDocumentLayoutAccess::VisibleRange(prepared, top, bottom, expected, true);
            const auto steps = stepScope.Delta().rich.caretHitTests;
            ++queries;
            searchMax = std::max(searchMax, reads);
            searchTotal += reads;
            stepMax = std::max(stepMax, steps);
            stepTotal += steps;
            if (a != b || (SUCCEEDED(a) && found != expected))
            {
                if (mismatches++ < 5u)
                    std::cout << "  MISMATCH " << item.name << " top=" << top << " bottom=" << bottom << " found " << found.first << ',' << found.second
                              << " expected " << expected.first << ',' << expected.second << '\n';
            }
        };
        for (const auto start : starts)
        {
            const auto caret = prepared.Caret(start);
            for (const float value : {caret.y, caret.y + caret.height})
                for (const float center : {std::nextafter(value, -infinity), value, std::nextafter(value, infinity)})
                    for (const float length : {0.0f, std::nextafter(center, infinity) - center, caret.height, caret.height * 2.5f, 300.0f})
                    {
                        compare(center, center + length);
                        compare(center - length, center);
                    }
        }
        compare(-1.0e30f, 1.0e30f);
        std::cout << "  document " << item.name << ": " << prepared.ChunkCount() << " chunks, " << queries << " queries, " << mismatches
                  << " mismatches; caret reads per query: search max " << searchMax << " mean " << searchTotal / queries << ", stepping max " << stepMax
                  << " mean " << stepTotal / queries << '\n';
        Require(mismatches == 0u && queries > 1000u, "the document-level visible range equals stepping through every chunk's lines");
    }
}
} // namespace

void RunRichCodeBlockTests()
{
    DXUI_RUN_TEST(TestRichCodeBlockLineHeightSumsAreNotCaretTops);
    DXUI_RUN_TEST(TestRichCodeBlockVisibleRangeEqualsTheReference);
    DXUI_RUN_TEST(TestRichCodeBlockVisibleRangeOfTheLargestLayouts);
    DXUI_RUN_TEST(TestRichCodeBlockSemanticVisibleRangesEqualTheFullScan);
    DXUI_RUN_TEST(TestRichCodeBlockLayoutsAreSharedAcrossEdits);
    DXUI_RUN_TEST(TestRichCodeBlockLayoutsAreNeverStale);
    DXUI_RUN_TEST(TestRichCodeBlockLayoutCacheIsBounded);
    DXUI_RUN_TEST(TestRichCodeBlockTurnsAreBoundedWhenEveryNumberHits);
    DXUI_RUN_TEST(TestRichCodeBlockCacheFollowsItsFactory);
    DXUI_RUN_TEST(TestRichCodeBlockStackedGateAndMixedRunSizes);
    DXUI_RUN_TEST(TestRichCodeBlockDocumentVisibleRangeEqualsStepping);
}
