#pragma once
// Hover and hit index (performance and motion plan W7.9). A reusable scenario shared by the native and embedded hosts: a
// large semantic document with links, footnotes, titled images and abbreviations, and pointer paths over a link and over
// plain text. The host callbacks keep the scenario independent of how each host delivers pointer moves and ticks.
#include "Controls/DxUi.RetainedCaches.h"
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Support.Tests.RichSemantic.h"
#include "Support/WorkCounters.h"
#include <DxUi/DxUi.h>
#include <DxUi/RichSemanticDocument.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace RichHoverIndexTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using RichSemanticTests::Check;

// Sections of the large hover document: each adds a paragraph with plain text, a titled link, an abbreviation, a titled
// image and a footnote reference, and a footnote whose body holds another titled link.
inline constexpr std::size_t kHoverScenarioSections = 60u;
inline constexpr std::size_t kHoverPathMoves        = 8u;
inline constexpr std::size_t kHoverPathTicks        = 10u;

[[nodiscard]] inline std::wstring HoverScenarioSource(std::size_t sections = kHoverScenarioSections)
{
    std::wstring source(0u, L' ');
    for (std::size_t section = 0u; section < sections; ++section)
        source += std::format(L"Plain words open section {0} with more plain words to hover over here. [link {0}](https://example.test/{0} \"Link title {0}\") "
                              L"and HTML with ![image {0}](image{0}.png \"Image title {0}\") and a note[^n{0}].\n\n",
                              section);
    for (std::size_t section = 0u; section < sections; ++section)
        source += std::format(L"[^n{0}]: Footnote {0} with [inner link {0}](https://example.test/f{0} \"Footnote link {0}\").\n\n", section);
    source += L"*[HTML]: Hyper Text Markup Language\n";
    return source;
}

// A read-only rich editor (titles need no Ctrl) whose links underline on hover, so a pointer move resolves both the hovered
// link range and the hover title.
inline RichTextEditor* InstallHoverScenarioEditor(ControlHost& host, std::wstring_view source)
{
    auto owner   = std::make_unique<RichTextEditor>(std::wstring(source));
    auto* editor = owner.get();
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownAnalysisExtensions(MarkdownExtensions::RichAll | MarkdownExtensions::Abbreviations);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetReadOnly(true);
    editor->SetLinkUnderlinePolicy(RichTextLinkUnderlinePolicy::HoverOrCaret);
    host.SetRoot(std::move(owner));
    return editor;
}

[[nodiscard]] inline std::optional<D2D1_RECT_F> TitledElementBounds(ControlHost& host, RichTextEditor& editor, SemanticRole role, std::wstring_view title)
{
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    Check(document != nullptr, "hover scenario has an acknowledged semantic document");
    const auto elements = document->Elements();
    const auto item     = std::ranges::find_if(elements, [&](const auto& element) { return element.role == role && element.title == title; });
    Check(item != elements.end(), "hover scenario contains the titled element");
    return editor.GetSemanticElementBounds(host, static_cast<std::size_t>(item - elements.begin()));
}

// Points inside a rectangle along its horizontal centre line, kept two DIPs from either side.
[[nodiscard]] inline std::vector<D2D1_POINT_2F> HorizontalPath(D2D1_RECT_F bounds, std::size_t count)
{
    std::vector<D2D1_POINT_2F> points(0u);
    const float left = bounds.left + 2.f, right = std::max(left, bounds.right - 2.f), y = (bounds.top + bounds.bottom) * .5f;
    for (std::size_t index = 0u; index < count; ++index)
        points.push_back(D2D1::Point2F(left + (right - left) * static_cast<float>(index) / static_cast<float>(std::max<std::size_t>(1u, count - 1u)), y));
    return points;
}

struct HoverGateCounts
{
    std::uint64_t linkMoves{}, linkScans{}, linkTitleCalls{};
    std::uint64_t ticks{}, tickTitleCalls{}, tickScans{};
    std::uint64_t plainMoves{}, plainInvalidations{}, plainScans{};
};

// Move(point, modifiers) delivers one pointer move and returns whether it invalidated the host (a native invalidation, or
// an embedded view that needs preparation; the callback prepares it again). Tick() advances one animation tick with the
// pointer unchanged and returns whether the host ran it.
template <typename Prepare, typename Move, typename Tick>
HoverGateCounts MeasureHoverGate(ControlHost& host, const char* hostName, Prepare&& prepare, Move&& move, Tick&& tick)
{
    const auto source = HoverScenarioSource();
    auto* editor      = InstallHoverScenarioEditor(host, source);
    prepare();
    const auto document = editor->GetAcknowledgedSemanticDocument(host);
    Check(document != nullptr && document->Elements().size() > 8u * kHoverScenarioSections, "the large hover document has many semantic elements");
    const auto link = TitledElementBounds(host, *editor, SemanticRole::Link, L"Link title 0");
    Check(link && link->right - link->left > 12.f && link->bottom > link->top, "the first link has visible bounds");
    const auto plainFirst = source.find(L"more plain words");
    const auto plainRects = editor->TryGetTextInputRangeRects(host, plainFirst, plainFirst + std::wstring_view(L"more plain words").size());
    Check(plainRects && ! plainRects->empty() && plainRects->front().right - plainRects->front().left > 12.f, "the plain words have a visible rectangle");

    HoverGateCounts counts{};
    const auto linkPath  = HorizontalPath(*link, kHoverPathMoves + 1u);
    const auto plainPath = HorizontalPath(plainRects->front(), kHoverPathMoves + 1u);

    static_cast<void>(move(linkPath.front(), 0u));
    Check(host.DebugGetPendingTooltipText() == L"Link title 0", "entering the link schedules its authored title");
    DxUi::Detail::WorkCounterScope scope;
    for (std::size_t index = 1u; index < linkPath.size(); ++index)
    {
        static_cast<void>(move(linkPath[index], 0u));
        ++counts.linkMoves;
    }
    auto delta            = scope.Delta();
    counts.linkScans      = delta.rich.hoverScanElements;
    counts.linkTitleCalls = delta.rich.hoveredTitleCalls;
    Check(host.DebugGetPendingTooltipText() == L"Link title 0", "the link title stays pending while the pointer moves over the link");

    scope.Restart();
    for (std::size_t index = 0u; index < kHoverPathTicks; ++index)
        counts.ticks += tick() ? 1u : 0u;
    delta                 = scope.Delta();
    counts.tickTitleCalls = delta.rich.hoveredTitleCalls;
    counts.tickScans      = delta.rich.hoverScanElements;
    Check(host.DebugGetPendingTooltipText() == L"Link title 0" || host.GetTooltipText() == L"Link title 0", "ticks keep the same link title");

    static_cast<void>(move(plainPath.front(), 0u));
    Check(host.DebugGetPendingTooltipText().empty() && ! host.HasTooltip(), "plain text has no title tooltip");
    scope.Restart();
    for (std::size_t index = 1u; index < plainPath.size(); ++index)
    {
        counts.plainInvalidations += move(plainPath[index], 0u) ? 1u : 0u;
        ++counts.plainMoves;
    }
    counts.plainScans = scope.Delta().rich.hoverScanElements;

    std::cerr << "    [W7.9] " << hostName << ": elements " << document->Elements().size() << "; over a link: " << counts.linkMoves << " moves, "
              << counts.linkScans << " element scans, " << counts.linkTitleCalls << " HoveredTitle calls; unchanged pointer: " << counts.ticks << " ticks, "
              << counts.tickTitleCalls << " HoveredTitle calls, " << counts.tickScans << " element scans; over plain text: " << counts.plainMoves << " moves, "
              << counts.plainInvalidations << " invalidations, " << counts.plainScans << " element scans\n";
    return counts;
}

template <typename Prepare, typename Move, typename Tick> void HoverGate(ControlHost& host, const char* hostName, Prepare&& prepare, Move&& move, Tick&& tick)
{
    const auto counts = MeasureHoverGate(host, hostName, prepare, move, tick);
    Check(counts.linkMoves == kHoverPathMoves && counts.linkScans == 0u, "over a link, element scans per pointer move are 0");
    Check(counts.tickTitleCalls == 0u && counts.tickScans == 0u, "HoveredTitle calls per tick with an unchanged pointer are 0");
    Check(counts.plainMoves == kHoverPathMoves && counts.plainInvalidations == 0u && counts.plainScans == 0u,
          "over plain text, invalidations and element scans per pointer move are 0");
    host.SetRoot(std::make_unique<Label>(L"replacement"));
}

// The tooltip text the host would show now: the pending request, or the shown tooltip once its delay passed.
[[nodiscard]] inline std::wstring CurrentTooltipText(ControlHost& host)
{
    const std::wstring pending(host.DebugGetPendingTooltipText());
    return pending.empty() && host.HasTooltip() ? std::wstring(host.GetTooltipText()) : pending;
}

[[nodiscard]] inline std::wstring StalenessSource(std::wstring_view firstTitle = L"Title 00")
{
    std::wstring source(0u, L' ');
    for (std::size_t index = 0u; index < 30u; ++index)
        source += std::format(L"[link {0:02}](https://example.test/{0} \"{1}\")\n\n", index, index == 0u ? firstTitle : std::format(L"Title {:02}", index));
    return source;
}

// The host remembers the last resolved title for (point, modifiers, the editor's hover key). Each change below alters what
// lies under an unmoved pointer, or could; after it, a move to the same point and a tick must agree with a resolution made
// from nothing (every retained cache dropped and the view prepared again), and the change must make the host resolve again.
template <typename Prepare, typename Move, typename Tick> void HoverStaleness(ControlHost& host, Prepare&& prepare, Move&& move, Tick&& tick)
{
    auto root         = std::make_unique<Panel>();
    auto* editor      = root->AddChild<RichTextEditor>(StalenessSource());
    const auto bounds = D2D1::RectF(8.f, 8.f, 400.f, 228.f);
    editor->SetBounds(bounds);
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    editor->SetReadOnly(true);
    host.SetRoot(std::move(root));
    const auto theme = host.GetTheme();
    prepare();
    const auto first  = TitledElementBounds(host, *editor, SemanticRole::Link, L"Title 00");
    const auto second = TitledElementBounds(host, *editor, SemanticRole::Link, L"Title 01");
    Check(first && second && second->top > first->top, "the staleness links are stacked");
    const auto point = D2D1::Point2F((first->left + first->right) * .5f, (first->top + first->bottom) * .5f);
    const auto away  = D2D1::Point2F(bounds.right - 40.f, point.y);
    const auto fresh = [&]
    {
        host.DebugDropRetainedStateForTest();
        prepare();
        static_cast<void>(move(point, 0u));
        return CurrentTooltipText(host);
    };
    struct Change
    {
        const char* name;
        std::function<void()> apply;
        bool changesTitle, resolvesAgain;
    };
    const float lineStep = second->top - first->top;
    const std::vector<Change> changes{
        {"document edit", [&] { editor->SetText(StalenessSource(L"Edited 00")); }, true, true},
        {"scroll", [&] { Check(editor->SetScrollOffsetDip(lineStep), "the staleness editor scrolls by one link"); }, true, true},
        {"zoom", [&] { editor->SetZoom(1.5f); }, false, true},
        {"width", [&] { editor->SetBounds(D2D1::RectF(bounds.left, bounds.top, bounds.right - 120.f, bounds.bottom)); }, false, true},
        {"density theme",
         [&]
    {
        auto compact    = theme;
        compact.density = Density::Compact;
        host.SetTheme(compact);
    },
         false,
         true},
        // A colour-only theme leaves every input of the title unchanged: the remembered title stays valid.
        {"colour theme",
         [&]
    {
        auto dark = theme;
        dark.dark = ! theme.dark;
        host.SetTheme(dark);
    },
         false,
         false},
        // The same content prepared again through the plain projection and back: a new geometry generation.
        {"re-preparation",
         [&]
    {
        editor->SetMarkdownRenderingEnabled(false);
        prepare();
        editor->SetMarkdownRenderingEnabled(true);
    },
         false,
         true},
    };
    for (const auto& change : changes)
    {
        editor->SetText(StalenessSource());
        editor->SetZoom(1.f);
        editor->SetBounds(bounds);
        host.SetTheme(theme);
        prepare();
        static_cast<void>(editor->SetScrollOffsetDip(0.f));
        prepare();
        static_cast<void>(move(away, 0u));
        static_cast<void>(move(point, 0u));
        const auto before = CurrentTooltipText(host);
        Check(before == L"Title 00", "the unmoved pointer starts over the first link");

        change.apply();
        prepare();
        DxUi::Detail::WorkCounterScope scope;
        static_cast<void>(tick());
        const auto ticked = CurrentTooltipText(host);
        static_cast<void>(move(point, 0u));
        const auto resolutions = scope.Delta().rich.hoveredTitleCalls;
        const auto moved       = CurrentTooltipText(host);
        const auto expected    = fresh();
        std::wcerr << L"    [W7.9] " << change.name << L": before \"" << before << L"\", tick \"" << ticked << L"\", same-point move \"" << moved
                   << L"\", fresh \"" << expected << L"\", HoveredTitle calls " << resolutions << L'\n';
        Check(moved == expected, "after the change, a move to the same point shows the title a fresh resolution shows");
        Check(ticked.empty() || ticked == expected, "after the change, a tick never publishes a stale title");
        Check(! change.changesTitle || expected != before, "the change alters the title under the unmoved pointer");
        Check(! change.resolvesAgain || resolutions > 0u, "the change makes the host resolve the remembered title again");
    }
    // Without a change, the same point resolves nothing again.
    host.SetTheme(theme);
    editor->SetText(StalenessSource());
    prepare();
    static_cast<void>(move(point, 0u));
    DxUi::Detail::WorkCounterScope unchanged;
    static_cast<void>(move(point, 0u));
    static_cast<void>(tick());
    Check(unchanged.Delta().rich.hoveredTitleCalls == 0u && CurrentTooltipText(host) == L"Title 00",
          "an unchanged pointer and editor reuse the remembered title");
    // The remembered title is a registered retained cache: populated now, empty after the oracle's drop.
    const auto rememberedEntries = [&]
    {
        std::vector<RetainedCacheCount> counts(0u);
        RetainedCacheAccess::Count(host, counts);
        const auto row = std::ranges::find(counts, std::string_view("ControlHost._richTitleHoverCache"), &RetainedCacheCount::name);
        Check(row != counts.end(), "the remembered hover title has a retained-cache row");
        return row->entries;
    };
    Check(rememberedEntries() == 1u, "the remembered hover title counts as a retained cache entry");
    host.DebugDropRetainedStateForTest();
    Check(rememberedEntries() == 0u, "the retained-cache drop clears the remembered hover title");
    prepare();
    host.SetRoot(std::make_unique<Label>(L"replacement"));
}

// The full scans the editor ran before the hover index: the oracle of the differential tests.
[[nodiscard]] inline std::optional<std::size_t> ScanHoverTitle(const SemanticDocument& document, std::size_t display)
{
    const auto elements = document.Elements();
    std::optional<std::size_t> best;
    for (std::size_t index = 0u; index < elements.size(); ++index)
    {
        const auto& element = elements[index];
        if ((element.role == SemanticRole::Link || element.role == SemanticRole::Image || element.role == SemanticRole::Abbreviation) &&
            ! element.title.empty() && ! document.IsSourceEditingElement(index) && element.display.first <= display && display < element.display.last &&
            (! best || element.display.last - element.display.first <= elements[*best].display.last - elements[*best].display.first))
            best = index;
    }
    return best;
}
[[nodiscard]] inline std::optional<std::size_t> ScanHoverLink(const SemanticDocument& document, std::size_t display)
{
    const auto elements = document.Elements();
    for (std::size_t index = 0u; index < elements.size(); ++index)
    {
        const auto& element = elements[index];
        if ((element.role == SemanticRole::Link || element.role == SemanticRole::FootnoteReference || element.role == SemanticRole::FootnoteBacklink) &&
            element.display.first <= display && display < element.display.last)
            return index;
    }
    return std::nullopt;
}

struct SeededRandom
{
    std::uint64_t state;
    std::size_t Next(std::size_t bound) noexcept
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::size_t>((state >> 33u) % bound);
    }
};

// A seeded Markdown document mixing titled and untitled links, titled images, an image nested in a link, abbreviations
// (alone and inside link text), adjacent links, an empty link, a long link that wraps, right-to-left links and text, and
// footnotes whose bodies hold titled links and images.
[[nodiscard]] inline std::wstring SeededHoverSource(std::uint64_t seed)
{
    SeededRandom random{seed};
    std::wstring body(0u, L' '), notes(0u, L' ');
    const std::size_t pieces = 24u + random.Next(24u);
    for (std::size_t k = 0u; k < pieces; ++k)
    {
        switch (random.Next(13u))
        {
            case 0u: body += L"plain words here "; break;
            case 1u: body += std::format(L"[link {0}](https://example.test/{0} \"Title {0}\") ", k); break;
            case 2u: body += std::format(L"[untitled {0}](https://example.test/u{0}) ", k); break;
            case 3u: body += std::format(L"![image {0}](i{0}.png \"Image {0}\") ", k); break;
            case 4u: body += std::format(L"[![nested {0}](n{0}.png \"Inner {0}\")](https://example.test/n{0} \"Outer {0}\") ", k); break;
            case 5u: body += L"HTML "; break;
            case 6u: body += std::format(L"[HTML {0}](https://example.test/a{0} \"Abbreviated link {0}\") ", k); break;
            case 7u:
                body += std::format(L"[a{0}](https://example.test/a{0} \"Adjacent A {0}\")[b{0}](https://example.test/b{0} \"Adjacent B {0}\") ", k);
                break;
            case 8u: body += std::format(L"[](https://example.test/e{0} \"Empty {0}\") ", k); break;
            case 9u:
                body += std::format(L"[a long link text that wraps over several lines in a narrow column {0}](https://example.test/w{0} \"Wrapped {0}\") ", k);
                break;
            case 10u:
                body += std::format(
                    L"\u05de\u05d9\u05dc\u05d9\u05dd [\u05e9\u05dc\u05d5\u05dd \u05e2\u05d5\u05dc\u05dd {0}](https://example.test/r{0} \"RTL {0}\") "
                    L"\u0645\u0631\u062d\u0628\u0627 ",
                    k);
                break;
            case 11u:
                body += std::format(L"note[^f{0}] ", k);
                notes += std::format(L"[^f{0}]: Footnote {0} with [inner {0}](https://example.test/f{0} \"Footnote link {0}\") and "
                                     L"![picture {0}](f{0}.png \"Footnote image {0}\").\n\n",
                                     k);
                break;
            default: body += std::format(L"**bold [strong {0}](https://example.test/s{0} \"Bold {0}\")** ", k); break;
        }
        if (random.Next(5u) == 0u)
            body += L"\n\n";
    }
    return body + L"\n\n" + notes + L"*[HTML]: Hyper Text Markup Language\n";
}

[[nodiscard]] inline std::shared_ptr<const MarkdownSyntax> ParseHover(const Document& model, std::optional<MarkdownSourceRegion> region = std::nullopt)
{
    MarkdownParser parser;
    const auto extensions = MarkdownExtensions::RichAll | MarkdownExtensions::Abbreviations;
    if (region)
        parser.BeginSourceRegion(model.Snapshot(), model.Revision(), *region, MarkdownProfile::RichMarkdown, extensions);
    else
        parser.Begin(model.Snapshot(), model.Revision(), MarkdownProfile::RichMarkdown, extensions);
    while (parser.HasPendingPreparation())
        parser.Advance(4096u);
    return parser.Published();
}

[[nodiscard]] inline std::shared_ptr<const SemanticDocument> BuildHover(const std::shared_ptr<const MarkdownSyntax>& syntax)
{
    SemanticBuilder builder;
    builder.Begin(syntax);
    while (builder.HasPendingPreparation())
        static_cast<void>(builder.Advance(4096u));
    return builder.Published();
}

// The prepared hover index answers every display offset exactly as the full scans did.
inline void HoverIndexMatchesFullScan(IDWriteFactory* factory)
{
    std::size_t documents{}, offsets{}, nested{}, excluded{}, empty{}, titled{};
    for (std::uint64_t seed = 0x5EED0790u; seed < 0x5EED0790u + 12u; ++seed)
    {
        const Document model{SeededHoverSource(seed)};
        const auto syntax = ParseHover(model);
        auto document     = BuildHover(syntax);
        Check(document != nullptr, "a seeded hover document builds");
        std::vector<std::shared_ptr<const SemanticDocument>> variants{document};
        // The same source with one inline link expanded for source editing: its title must no longer be shown.
        const auto elements = document->Elements();
        const auto link     = std::ranges::find_if(elements, [](const auto& element) { return element.role == SemanticRole::Link && ! element.title.empty(); });
        if (link != elements.end())
            variants.push_back(
                BuildHover(ParseHover(model, MarkdownSourceRegion{link->source, 79u, MarkdownSourceRegionKind::Inline, MarkdownInlineSourceKind::Link})));
        for (const auto& variant : variants)
            for (const float width : {120.f, 640.f})
            {
                DxUi::RichText::Detail::PreparedSemanticGeometry geometry;
                LayoutOptions options;
                options.widthDip = width;
                geometry.Begin(factory, variant, options);
                while (geometry.Pending())
                    static_cast<void>(geometry.Advance({16u, 64u}));
                Check(geometry.Ready(), "seeded hover geometry is prepared");
                ++documents;
                const auto& prepared = geometry.Document();
                for (std::size_t index = 0u; index < prepared.Elements().size(); ++index)
                {
                    const auto& element = prepared.Elements()[index];
                    const bool titledRole =
                        element.role == SemanticRole::Link || element.role == SemanticRole::Image || element.role == SemanticRole::Abbreviation;
                    if (! titledRole || element.title.empty())
                        continue;
                    ++titled;
                    excluded += prepared.IsSourceEditingElement(index) ? 1u : 0u;
                    empty += element.display.first == element.display.last ? 1u : 0u;
                    nested += std::ranges::count_if(prepared.Elements(),
                                                    [&](const auto& other)
                    {
                        return &other != &element && ! other.title.empty() && other.display.first <= element.display.first &&
                               element.display.last <= other.display.last && element.display.first < element.display.last;
                    }) > 0
                                  ? 1u
                                  : 0u;
                }
                const auto size = prepared.Map().Display().size();
                for (std::size_t display = 0u; display <= size + 1u; ++display, ++offsets)
                {
                    Check(geometry.HoverTitleElement(display) == ScanHoverTitle(prepared, display), "the hover title index matches the full scan");
                    Check(geometry.HoverLinkElement(display) == ScanHoverLink(prepared, display), "the hover link index matches the full scan");
                }
            }
    }
    std::cerr << "    [W7.9] differential: " << documents << " prepared documents, " << offsets << " display offsets, " << titled << " titled spans, " << nested
              << " nested, " << empty << " empty, " << excluded << " expanded source owners\n";
    Check(nested > 0u && excluded > 0u && empty > 0u, "the seeded corpus covers nested, empty and expanded-source titled spans");
}

// The index itself against a brute-force answer over random overlapping, nested, adjacent and empty spans.
inline void HoverSpanIndexMatchesBruteForce()
{
    SeededRandom random{0x5EED0791u};
    for (std::size_t round = 0u; round < 400u; ++round)
    {
        std::vector<DxUi::RichText::Detail::HoverSpan> spans(0u);
        const std::size_t count = random.Next(12u);
        for (std::size_t index = 0u; index < count; ++index)
        {
            const auto first = random.Next(40u);
            spans.push_back({first, first + random.Next(12u), random.Next(3u) == 0u ? index : index * 2u});
        }
        for (const auto pick : {DxUi::RichText::Detail::HoverSpanPick::Smallest, DxUi::RichText::Detail::HoverSpanPick::First})
        {
            DxUi::RichText::Detail::HoverSpanIndex index;
            index.Build(spans, pick);
            for (std::size_t display = 0u; display < 60u; ++display)
            {
                std::optional<DxUi::RichText::Detail::HoverSpan> best;
                for (const auto& span : spans)
                {
                    if (span.first > display || display >= span.last)
                        continue;
                    const bool better = ! best || (pick == DxUi::RichText::Detail::HoverSpanPick::First
                                                       ? span.element < best->element
                                                       : (span.last - span.first < best->last - best->first ||
                                                          (span.last - span.first == best->last - best->first && span.element >= best->element)));
                    if (better)
                        best = span;
                }
                Check(index.At(display) == (best ? std::optional<std::size_t>(best->element) : std::nullopt), "the hover span index matches brute force");
            }
        }
    }
}
} // namespace RichHoverIndexTests
