#pragma once
#include "../../src/Controls/DxUi.RichImageResources.h"
#include "Scope.Tests.RichImage.h"
#include <DxUi/DxUi.h>
#include <cmath>
namespace RichImageResourceTests
{
inline std::shared_ptr<const DxUi::RichText::SemanticDocument> Semantic(std::wstring_view source)
{
    using namespace DxUi::RichText;
    MarkdownParser parser;
    parser.Begin(source, 1u, MarkdownProfile::RichMarkdown);
    while (parser.HasPendingPreparation())
        parser.Advance(17u);
    SemanticBuilder builder;
    builder.Begin(parser.Published());
    while (builder.HasPendingPreparation())
        builder.Advance(17u);
    return builder.Published();
}
inline std::shared_ptr<const DxUi::RichText::RichImage> Picture(unsigned width = 96u, unsigned height = 64u)
{
    std::vector<std::byte> bytes(static_cast<std::size_t>(width) * height * 4u);
    for (std::size_t at = 0u; at < bytes.size(); at += 4u)
    {
        bytes[at]      = std::byte(200u);
        bytes[at + 1u] = std::byte(80u);
        bytes[at + 2u] = std::byte(20u);
        bytes[at + 3u] = std::byte(255u);
    }
    std::shared_ptr<const DxUi::RichText::RichImage> result;
    THROW_IF_FAILED(DxUi::RichText::RichImage::CreatePbgra(width, height, bytes, result));
    return result;
}
inline std::shared_ptr<const DxUi::RichText::RichImage> Pattern()
{
    constexpr unsigned width = 96u, height = 64u;
    constexpr std::array<std::array<unsigned, 4u>, 4u> colors{{{20u, 40u, 220u, 255u}, {40u, 220u, 60u, 255u}, {220u, 60u, 40u, 255u}, {200u, 80u, 20u, 255u}}};
    std::vector<std::byte> pixels(width * height * 4u);
    for (unsigned y = 0u; y < height; ++y)
        for (unsigned x = 0u; x < width; ++x)
            for (unsigned channel = 0u; channel < 4u; ++channel)
                pixels[(static_cast<std::size_t>(y) * width + x) * 4u + channel] =
                    std::byte(colors[(y >= height / 2u ? 2u : 0u) + (x >= width / 2u ? 1u : 0u)][channel]);
    std::shared_ptr<const DxUi::RichText::RichImage> result;
    THROW_IF_FAILED(DxUi::RichText::RichImage::CreatePbgra(width, height, pixels, result));
    return result;
}
template <typename Check> inline void Admission(Check&& check)
{
    using namespace DxUi::RichText;
    using namespace DxUi::RichText::Detail;
    const auto finish = [&](RichImageResources& resources)
    {
        unsigned turns{};
        while (++turns < 128u)
        {
            if (resources.Advance())
                return turns;
        }
        throw std::runtime_error("image resource preparation did not settle");
    };
    std::wstring source(0u, L' ');
    for (unsigned i = 0u; i < 129u; ++i)
        source.append(L"![image](same.png) ");
    const auto document = Semantic(source);
    RichImageResources resources;
    check(! resources.Begin(document, L"C:\\docs\\note.md", 1u, 1u) && finish(resources) == 32u,
          "resource path preparation advances at most four image candidates per turn");
    std::vector<RichImageRequest> requests(0u);
    resources.Take(requests);
    check(requests.size() == 1u && resources.Statistics().imageElements == 128u,
          "native image placement ceiling retains excess alternatives while coalescing duplicate files");
    const auto picture = Picture();
    check(resources.Complete(requests[0], picture) && resources.Snapshot()->placements.size() == 128u,
          "one immutable image supplies every admitted duplicate placement");
    source.clear();
    for (unsigned i = 0u; i < 33u; ++i)
        source.append(L"![image](source").append(std::to_wstring(i)).append(L".png) ");
    resources.Begin(Semantic(source), L"C:\\docs\\note.md", 1u, 1u);
    finish(resources);
    resources.Take(requests);
    check(requests.size() == 32u, "unique file request ceiling is independent of image placement count");
    for (const auto& request : requests)
        check(resources.Complete(request, picture), "bounded duplicate-pixel source completion");
    check(resources.Statistics().readySources == 32u && resources.Statistics().decodedBytes == picture->Pixels().size(),
          "shared CPU pixels are accounted once across different source paths");
    source.clear();
    for (unsigned i = 0u; i < 5u; ++i)
        source.append(L"![image](capacity").append(std::to_wstring(i)).append(L".png) ");
    resources.Begin(Semantic(source), L"C:\\docs\\note.md", 1u, 1u);
    finish(resources);
    resources.Take(requests);
    std::array<std::shared_ptr<const RichImage>, 4u> large{};
    for (unsigned i = 0u; i < 4u; ++i)
    {
        large[i] = Picture(4096u, 1024u);
        check(resources.Complete(requests[i], large[i]), "maximum-size immutable image fits the declared cache reservation");
    }
    check(resources.Statistics().decodedBytes == RichImageResources::MaximumDecodedBytes, "decoded CPU cache reaches its exact declared byte ceiling");
    check(resources.Complete(requests[4u], Picture(1u, 1u)) && resources.Statistics().readySources == 4u && resources.Statistics().failedSources == 1u &&
              resources.Statistics().decodedBytes == RichImageResources::MaximumDecodedBytes,
          "over-capacity completion keeps an alternative without growing retained bytes");
    resources.Clear();
    check(resources.Statistics().decodedBytes == 0u && ! resources.Snapshot(), "explicit resource reset releases retained cache ownership");
    RichImageRequest retired;
    {
        RichImageResources transient;
        transient.Begin(document, L"C:\\docs\\note.md", 1u, 1u);
        finish(transient);
        transient.Take(requests);
        retired = requests[0];
        check(! resources.Complete(retired, picture), "a foreign resource state cannot consume another editor's request");
    }
    check(retired.Cancelled(), "resource destruction cancels caller-held tokens without callbacks");
}
template <typename Prepare, typename Check> inline void ReadyPicture(DxUi::RichTextEditor& editor, Prepare&& prepare, Check&& check, bool patterned = false)
{
    using namespace DxUi::RichText;
    editor.SetVisible(true);
    editor.SetEnabled(true);
    editor.SetReadOnly(false);
    editor.SetViewMode(ViewMode::Rich);
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    editor.SetDocumentFilePath(L"C:\\docs\\note.md");
    editor.SetText(L"![sample](image.png)");
    prepare();
    std::vector<RichImageRequest> requests(0u);
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u && editor.CompleteImageRequest(requests[0], patterned ? Pattern() : Picture()),
          "ready raster fixture publishes current owned pixels");
    prepare();
}
template <typename Check>
inline void Pixel(const DxUi::RichTextEditor& editor,
                  const DxUi::ControlHost& host,
                  std::span<const std::uint8_t> pixels,
                  unsigned width,
                  unsigned height,
                  float scale,
                  Check&& check,
                  bool selected = false)
{
    using namespace DxUi::RichText;
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    check(static_cast<bool>(document), "image raster uses current acknowledged geometry");
    const auto element = std::ranges::find(document->Elements(), SemanticRole::Image, &SemanticElement::role);
    check(element != document->Elements().end(), "raster image retains semantic alternative");
    const auto bounds = editor.GetSemanticElementBounds(host, static_cast<std::size_t>(element - document->Elements().begin()));
    check(bounds.has_value(), "raster image has visible bounds");
    const auto x = static_cast<unsigned>((bounds->left + bounds->right) * 0.5f * scale),
               y = static_cast<unsigned>((bounds->top + bounds->bottom) * 0.5f * scale);
    check(x < width && y < height && pixels.size() == static_cast<std::size_t>(width) * height * 4u, "raster oracle addresses a real retained target pixel");
    const auto at    = (static_cast<std::size_t>(y) * width + x) * 4u;
    const bool exact = pixels[at] == 200u && pixels[at + 1u] == 80u && pixels[at + 2u] == 20u && pixels[at + 3u] == 255u;
    check(selected ? ! exact : exact, selected ? "selection visibly tints an opaque image" : "actual host raster contains exact known image pixels");
}
template <typename Check>
inline void PatternPixels(const DxUi::RichTextEditor& editor,
                          const DxUi::ControlHost& host,
                          std::span<const std::uint8_t> pixels,
                          unsigned width,
                          unsigned height,
                          float scale,
                          Check&& check,
                          bool selected = false)
{
    using namespace DxUi::RichText;
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    check(static_cast<bool>(document), "pattern raster uses acknowledged source");
    const auto element = std::ranges::find(document->Elements(), SemanticRole::Image, &SemanticElement::role);
    check(element != document->Elements().end(), "pattern has an image element");
    const auto bounds = editor.GetSemanticElementBounds(host, static_cast<std::size_t>(element - document->Elements().begin()));
    check(bounds.has_value(), "pattern has visible geometry");
    constexpr std::array<std::array<unsigned, 4u>, 4u> expected{
        {{20u, 40u, 220u, 255u}, {40u, 220u, 60u, 255u}, {220u, 60u, 40u, 255u}, {200u, 80u, 20u, 255u}}};
    for (unsigned quadrant = 0u; quadrant < 4u; ++quadrant)
    {
        const auto x = static_cast<unsigned>((bounds->left + (bounds->right - bounds->left) * (quadrant % 2u ? 0.75f : 0.25f)) * scale);
        const auto y = static_cast<unsigned>((bounds->top + (bounds->bottom - bounds->top) * (quadrant / 2u ? 0.75f : 0.25f)) * scale);
        check(x < width && y < height && pixels.size() == static_cast<std::size_t>(width) * height * 4u, "pattern oracle addresses actual target pixels");
        bool exact = true;
        for (unsigned channel = 0u; channel < 4u; ++channel)
            exact = exact && pixels[(static_cast<std::size_t>(y) * width + x) * 4u + channel] == expected[quadrant][channel];
        check(selected ? ! exact : exact,
              selected ? "selection visibly tints every image quadrant" : "native image raster preserves independent corner colors and orientation");
    }
}
template <typename Prepare, typename Check> inline void Retained(DxUi::RichTextEditor& editor, DxUi::ControlHost& host, Prepare&& prepare, Check&& check)
{
    using namespace DxUi;
    using namespace DxUi::RichText;
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    editor.SetDocumentFilePath(L"C:\\docs\\note.md");
    const std::wstring source = L"before ![sample](image.png) after\n\n![same](image.png)\n";
    editor.SetText(source);
    editor.SetSelection({4u, 2u});
    prepare();
    std::vector<RichImageRequest> requests(0u);
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u && requests[0].Id() && ! requests[0].Cancelled() && requests[0].FilePath() == L"C:\\docs\\image.png",
          "duplicate local destinations coalesce into one caller-owned job");
    std::vector<RichImageRequest> repeated(0u);
    editor.TakePendingImageRequests(repeated);
    check(repeated.empty(), "taking requests cannot dispatch a pending file twice");
    const auto revision  = editor.Revision();
    const auto selection = editor.GetSelection();
    std::size_t notices{};
    editor.SetOnTextChanged([&] { ++notices; });
    const auto picture = Picture();
    check(editor.CompleteImageRequest(requests[0], picture) && requests[0].Cancelled() && ! editor.CompleteImageRequest(requests[0], picture),
          "current completion publishes once and retires its job token");
    prepare();
    check(editor.GetText() == source && editor.Revision() == revision && editor.GetSelection() == selection && ! editor.CanUndo() && ! notices,
          "image publication preserves source, backward selection, history and text notifications");
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    check(static_cast<bool>(document), "image geometry has an acknowledged source document");
    unsigned images{};
    for (std::size_t index = 0u; index < document->Elements().size(); ++index)
        if (document->Elements()[index].role == SemanticRole::Image)
        {
            ++images;
            const auto bounds = editor.GetSemanticElementBounds(host, index);
            check(bounds && std::abs(bounds->right - bounds->left - 96.f) < 0.02f && std::abs(bounds->bottom - bounds->top - 64.f) < 0.02f,
                  "each loaded image uses actual pixel aspect and baseline geometry");
        }
    const auto statistics = editor.GetImageResourceStatistics();
    check(images == 2u && statistics.readySources == 1u && statistics.pendingSources == 0u && statistics.decodedBytes == 96u * 64u * 4u,
          "duplicate placements share one bounded decoded resource");
    editor.SetOnTextChanged({});
    // Unrelated text edits reuse immutable ready pixels; no repeat file read.
    editor.SetSelection({0u, 0u});
    check(editor.ReplaceSelection(L"new "), "image reuse fixture changes source");
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.empty() && editor.GetImageResourceStatistics().readySources == 1u,
          "unchanged image paths survive ordinary source revisions without another job");
    editor.ReloadImages();
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u, "explicit reload reissues current resources");
    const auto old = requests[0];
    editor.SetDocumentFilePath(L"D:\\other\\note.md");
    check(old.Cancelled() && ! editor.CompleteImageRequest(old, picture), "base-location changes cancel and reject an in-flight image");
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u && requests[0].FilePath() == L"D:\\other\\image.png", "new image request uses only the new explicit base");
    const auto changedSource = requests[0];
    editor.SetText(L"![other](b.png)");
    check(changedSource.Cancelled() && ! editor.CompleteImageRequest(changedSource, picture),
          "source replacement rejects a prior image completion immediately");
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u, "new source emits its own job");
    const auto hidden = requests[0];
    editor.SetVisible(false);
    check(hidden.Cancelled() && ! editor.CompleteImageRequest(hidden, picture), "hidden editor cancels its external job without callbacks or waits");
    editor.SetVisible(true);
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u, "shown editor can explicitly retry the current image");
    editor.SetSelection({0u, 0u});
    editor.SetReadOnly(true);
    const auto sourceRevision = editor.Revision();
    check(editor.CompleteImageRequest(requests[0], picture), "read-only source may still receive current image pixels");
    prepare();
    check(editor.Revision() == sourceRevision && editor.GetText() == L"![other](b.png)" && ! editor.CanUndo(),
          "visual image completion never bypasses read-only source policy");
    editor.SetReadOnly(false);
    editor.ReloadImages();
    prepare();
    editor.TakePendingImageRequests(requests);
    const auto mode = requests[0];
    editor.SetViewMode(ViewMode::Source);
    check(mode.Cancelled() && ! editor.CompleteImageRequest(mode, picture), "Source mode retires pending rendered-image work");
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.empty(), "Source mode does not issue file jobs");
    editor.SetViewMode(ViewMode::Rich);
    prepare();
    editor.TakePendingImageRequests(requests);
    check(requests.size() == 1u, "Rich mode restores current image admission");
    check(editor.CompleteImageRequest(requests[0], {}), "missing/unsupported decode is an accepted fallback result");
    editor.TakePendingImageRequests(requests);
    check(requests.empty() && editor.GetImageResourceStatistics().failedSources == 1u, "failed image keeps its alternative without an idle retry loop");
}
} // namespace RichImageResourceTests
