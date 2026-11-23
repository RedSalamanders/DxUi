#pragma once
// FX8 Rich typing (plan W0.4; proves G12): a RichTextEditor holding a Markdown document near the 64 Ki-unit cap with 3 math
// blocks, 2 Mermaid diagrams, 2 raster images, 1 SVG, a 200x5 table and a numbered 2,000-line code fence, edited by 100
// one-character inserts (in prose, inside the fence and next to the table), 10 Enter presses, 50 arrow keys and a 30-step
// width resize. Embedded host only: G12 is content work, the same on both hosts, and the embedded runner adds the C++
// allocation sink.
//
// Cooperative preparation: the editor prepares a change over many owner turns, and paints nothing until it is done. The
// definition sets settlePreparation, so the embedded runner repeats a step's frame until the host needs no further
// preparation: each step is one action and every turn it causes, and rich.preparationTurns counts those turns.
//
// The raster images are the W7.8 decode scenarios (Support.Tests.RichImage.h) decoded at build time; the application side of
// image loading (TakePendingImageRequests, CompleteImageRequest) is serviced before every step's action, as an application
// would after each preparation.
#include "Embedded.Tests.PerformanceFixture.h"
#include "Support.Tests.RichImage.h"

#include <DxUi/RichImage.h>

#include <wil/result.h>

namespace PerformanceFixtures::Fx8
{
inline constexpr std::array kBudgets{std::string_view("G12")};

inline constexpr size_t kWarmSteps    = 4u;
inline constexpr size_t kProseInserts = 40u;
inline constexpr size_t kEnters       = 10u;
inline constexpr size_t kArrows       = 50u;
inline constexpr size_t kFenceInserts = 30u;
inline constexpr size_t kTableInserts = 30u;
inline constexpr size_t kResizeSteps  = 30u;

// Phase indices, in kPhases order.
inline constexpr size_t kWarm        = 0u;
inline constexpr size_t kProseInsert = 1u;
inline constexpr size_t kEnter       = 2u;
inline constexpr size_t kArrow       = 3u;
inline constexpr size_t kSeekFence   = 4u;
inline constexpr size_t kFenceInsert = 5u;
inline constexpr size_t kSeekTable   = 6u;
inline constexpr size_t kTableInsert = 7u;
inline constexpr size_t kResize      = 8u;

inline constexpr std::array kPhases{
    Phase{"warm", kWarmSteps, PhaseKind::Warmup, "initial preparation of the whole document; image requests completed from the decoded fixtures"},
    Phase{"prose-insert", kProseInserts, PhaseKind::Measured, "40 one-character inserts at the end of the first prose paragraph"},
    Phase{"enter", kEnters, PhaseKind::Measured, "10 Enter presses (new paragraph) at the same caret, above every later block"},
    Phase{"arrow", kArrows, PhaseKind::Measured, "25 Right then 25 Down arrow keys from that caret"},
    Phase{"seek-fence", 1u, PhaseKind::Measured, "the caret moves into the middle line of the numbered code fence (SetSelection)"},
    Phase{"fence-insert", kFenceInserts, PhaseKind::Measured, "30 one-character inserts inside the numbered code fence"},
    Phase{"seek-table", 1u, PhaseKind::Measured, "the caret moves to the start of the paragraph right after the table (SetSelection)"},
    Phase{"table-insert", kTableInserts, PhaseKind::Measured, "30 one-character inserts in the paragraph next to the table"},
    Phase{"resize", kResizeSteps, PhaseKind::Measured, "30 width steps, each 1/64 of the variant width narrower"},
};

// The document's scale. "full" is the plan's document; "default", run by the ordinary suites, keeps every object and every
// action with an eighth of the text, so that the determinism gate stays affordable.
struct DocumentShape
{
    size_t units      = 0u; // initial source units; the inserts and Enter presses stay below RichText::Document::MaximumUnits
    size_t fenceLines = 0u;
    size_t tableRows  = 0u;
};
inline constexpr DocumentShape kFullShape{65536u - 256u, 2000u, 200u};
inline constexpr DocumentShape kSmallShape{8192u - 256u, 250u, 25u};

// How the editor lays the document out. "default" and "full" are the rendered semantic view (Rich mode, Markdown
// rendering, Subtle presentation): semantic geometry, no v0 chunks. W7.3 is about the v0 document layout, which runs only
// without a semantic document: "source" is Source mode, laid out in v0 chunks of about 1,024 units with absolute origins;
// "multiplier" is Rich mode without Markdown rendering and a line-height multiplier of 1.5, where the v0 layout skips
// chunks and builds one whole-document layout (rich.wholeLayoutsLaid). Each comes suite-sized and full-sized.
enum class Presentation : uint8_t
{
    Semantic,
    Source,
    Multiplier,
};

inline constexpr std::array kVariants{
    Variant{"default", 800u, 600u, 96.0f, false},
    Variant{"full", 1280u, 720u, 96.0f, false, true}, // receipt only: too heavy for the ordinary suites
    Variant{"source", 800u, 600u, 96.0f, false},
    Variant{"source-full", 1280u, 720u, 96.0f, false, true},
    Variant{"multiplier", 800u, 600u, 96.0f, false},
    Variant{"multiplier-full", 1280u, 720u, 96.0f, false, true},
};

[[nodiscard]] inline const DocumentShape& ShapeOf(const Variant& variant) noexcept
{
    return variant.receiptOnly ? kFullShape : kSmallShape;
}
[[nodiscard]] inline Presentation PresentationOf(const Variant& variant) noexcept
{
    return variant.name.starts_with("source")       ? Presentation::Source
           : variant.name.starts_with("multiplier") ? Presentation::Multiplier
                                                    : Presentation::Semantic;
}

// Markers the seeks look for; each occurs once in the document.
inline constexpr std::wstring_view kAfterTableMarker = L"After the table";

// The fixture's Markdown source for a shape: deterministic, exactly shape.units UTF-16 units.
[[nodiscard]] inline std::wstring BuildSource(const DocumentShape& shape)
{
    std::wstring source;
    source.reserve(shape.units);
    source += L"# Rich typing fixture\n\nTyping starts at the end of this paragraph, which the first inserts and Enter presses extend";
    source += L".\n\n";
    const auto prose = [&](size_t index)
    {
        source += L"Paragraph ";
        source += std::to_wstring(index);
        source += L" carries **strong** and *emphasised* words, `inline code` and a [link](https://example.com/";
        source += std::to_wstring(index);
        source += L") so that every edit crosses styled runs of an ordinary semantic paragraph.\n\n";
    };
    for (size_t index = 0u; index < 4u; ++index)
        prose(index);
    source += L"$$\n\\frac{a+b}{c^2} + \\sqrt{x_1^2 + x_2^2}\n$$\n\n";
    source += L"```mermaid\nflowchart TD\nA[Draft]-->B[Review]\nB-->C[Ready]\nB-->A\n```\n\n";
    source += L"![first raster](first.png)\n\n";
    source += L"$$\n\\sum_{k=1}^{n} k^2 = \\frac{n(n+1)(2n+1)}{6}\n$$\n\n";
    source += L"| Name | Kind | Size | Owner | State |\n| --- | --- | --- | --- | --- |\n";
    for (size_t row = 0u; row < shape.tableRows; ++row)
    {
        const std::wstring number = std::to_wstring(row);
        source +=
            L"| item" + number + L" | k" + std::to_wstring(row % 7u) + L" | " + std::to_wstring(row * 3u) + L" | o" + std::to_wstring(row % 5u) + L" | ok |\n";
    }
    source += L"\n";
    source += kAfterTableMarker;
    source += L" comes this paragraph, whose start receives the inserts next to the table.\n\n";
    source += L"```mermaid\nsequenceDiagram\nparticipant A\nparticipant B\nA->>B: request\nB-->>A: reply\n```\n\n";
    source += L"![second raster](second.jpg)\n\n";
    source += L"$$\n\\int_0^1 x^2 \\, dx = \\frac{1}{3}\n$$\n\n";
    source += L"![vector](shape.svg)\n\n";
    source += L"```cpp\n";
    for (size_t line = 0u; line < shape.fenceLines; ++line)
        source += L"v" + std::to_wstring(line) + L";\n";
    source += L"```\n\n";
    for (size_t index = 4u; source.size() + 600u < shape.units; ++index)
        prose(index);
    // Pad the last paragraph to the exact unit count.
    std::wstring tail = L"Final paragraph";
    while (source.size() + tail.size() + 2u < shape.units)
        tail += tail.size() % 10u == 9u ? L' ' : L'z';
    source += tail;
    source += L".\n";
    source.resize(shape.units, L'\n');
    return source;
}

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        width_  = context.variant.width;
        height_ = context.variant.height;
        RETURN_IF_FAILED(DecodeImages());
        const DocumentShape& shape = ShapeOf(context.variant);
        fenceLine_                 = L"\nv" + std::to_wstring(shape.fenceLines / 2u) + L";";
        auto owned                 = std::make_unique<DxUi::RichTextEditor>(BuildSource(shape));
        editor_                    = owned.get();
        editor_->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::RichMarkdown);
        const Presentation presentation = PresentationOf(context.variant);
        editor_->SetMarkdownRenderingEnabled(presentation == Presentation::Semantic);
        auto appearance                 = editor_->GetAppearance();
        appearance.markdownPresentation = DxUi::RichText::MarkdownPresentationStyle::Subtle; // numbered fences, measured tables
        if (presentation == Presentation::Multiplier)
            appearance.lineHeightMultiplier = 1.5f;
        editor_->SetAppearance(appearance);
        if (presentation == Presentation::Source)
            editor_->SetViewMode(DxUi::RichText::ViewMode::Source);
        editor_->SetDocumentFilePath(L"C:\\fixture\\fx8.md"); // resolves relative image sources; never opened
        editor_->SetSvgImagesEnabled(true);
        context.controls.SetRoot(std::move(owned));
        context.controls.SetFocusControl(editor_, false);
        const std::wstring text = editor_->GetText();
        const size_t prose      = text.find(L" extend.");
        if (prose == std::wstring::npos)
            return E_UNEXPECTED;
        editor_->SetSelection({prose + 7u, prose + 7u});
        return S_OK;
    }

    void Step(Context& context, size_t phase, size_t step) override
    {
        ServiceImages();
        if (phase == kWarm)
            return;
        const size_t action = step;
        switch (phase)
        {
            case kProseInsert:
            case kFenceInsert:
            case kTableInsert: static_cast<void>(context.port.Character(static_cast<wchar_t>(L'a' + action % 26u))); break;
            case kEnter:
                static_cast<void>(context.port.Key(VK_RETURN, true));
                static_cast<void>(context.port.Character(L'\r'));
                static_cast<void>(context.port.Key(VK_RETURN, false));
                break;
            case kArrow:
            {
                const UINT key = action % kArrows < kArrows / 2u ? VK_RIGHT : VK_DOWN;
                static_cast<void>(context.port.Key(key, true));
                static_cast<void>(context.port.Key(key, false));
                break;
            }
            case kSeekFence: Seek(fenceLine_, fenceLine_.size() - 1u); break; // before the line's ';'
            case kSeekTable: Seek(kAfterTableMarker, 0u); break;
            case kResize:
            {
                const UINT narrower = static_cast<UINT>(((action % kResizeSteps) + 1u) * (width_ / 64u));
                context.port.Resize(width_ - narrower, height_, context.variant.dpi);
                break;
            }
            default: break;
        }
    }

    [[nodiscard]] DxUi::RichTextEditor& Editor() const noexcept
    {
        return *editor_;
    }

private:
    HRESULT DecodeImages()
    try
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const auto uninitialize   = wil::scope_exit([&]() noexcept
        {
            if (SUCCEEDED(initialized))
                CoUninitialize();
        });
        wil::com_ptr_nothrow<IWICImagingFactory> factory;
        RETURN_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.put())));
        const auto scenarios = RichImageTests::DecodeScenarios(factory.get());
        // The W7.8 PNG and the first EXIF-oriented JPEG.
        RETURN_IF_FAILED(RichImageTests::Decode(factory.get(), scenarios.at(0u).encoded, png_));
        RETURN_IF_FAILED(RichImageTests::Decode(factory.get(), scenarios.at(1u).encoded, jpeg_));
        constexpr std::string_view svg = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 40 30'><rect width='40' height='30' fill='#2060c0'/>"
                                         "<circle cx='20' cy='15' r='9' fill='#f0c020'/></svg>";
        return DxUi::RichText::RichImage::CreateSvg(80u, 60u, std::as_bytes(std::span(svg)), svg_);
    }
    CATCH_RETURN();

    // The application's half of image loading: completes every request the last preparation exposed.
    void ServiceImages()
    {
        requests_.clear();
        editor_->TakePendingImageRequests(requests_);
        for (const DxUi::RichText::RichImageRequest& request : requests_)
        {
            const std::wstring_view source = request.Source();
            const auto image               = source.ends_with(L"first.png") ? png_ : source.ends_with(L"second.jpg") ? jpeg_ : svg_;
            static_cast<void>(editor_->CompleteImageRequest(request, image));
        }
    }

    // Places the caret at `offset` units into the first occurrence of `marker`.
    void Seek(std::wstring_view marker, size_t offset)
    {
        const std::wstring text = editor_->GetText();
        const size_t at         = text.find(marker);
        if (at != std::wstring::npos)
            editor_->SetSelection({at + offset, at + offset});
    }

    DxUi::RichTextEditor* editor_ = nullptr;
    std::shared_ptr<const DxUi::RichText::RichImage> png_, jpeg_, svg_;
    std::vector<DxUi::RichText::RichImageRequest> requests_;
    std::wstring fenceLine_;
    UINT width_  = 0u;
    UINT height_ = 0u;
};

inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx8.h"),
                                     std::string_view("Tests/Support/Support.Tests.RichImage.h")};

inline constexpr Definition kDefinition{
    .id         = "FX8",
    .title      = "Rich typing",
    .scenario   = "A 64 Ki-unit document with 3 math blocks, 2 diagrams, 2 raster images, 1 SVG, a 200x5 table and a numbered code fence of 2,000 lines; 100 "
                  "one-character inserts, 10 Enter presses, 50 arrow keys, a 30-step width resize. Proves G12. RichTextEditor, RichMarkdown profile, Subtle "
                  "presentation, embedded host; each step is one action settled through every owner turn of its preparation. Variant full is the plan's document "
                  "(65,280 units); default keeps every object and action at 7,936 units, a 250-line fence and a 25-row table. Variants source and "
                  "multiplier (each with a -full form) lay the same document out through the v0 document layout: Source mode in chunks, and Rich mode "
                  "without Markdown rendering at line height 1.5 as one whole layout.",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "prose-insert",
    .sources    = kSources,
    .reducedMotion     = true,
    .create            = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
    .settlePreparation = true,
};
} // namespace PerformanceFixtures::Fx8
