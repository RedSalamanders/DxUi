#pragma once
// FX10 Live resize (plan W0.4): a representative screen laid out by the library's own panels (a vertical FlexPanel root
// holding a title, a wrapping paragraph, a body of a two-column FormPanel, a Grid list and a read-only RichTextEditor, and
// a wrapping row of actions) swept from 800x600 to 3840x2160 device pixels in 30 steps. Every step resizes the host and
// renders one frame, so each step re-runs the layout, text wrapping and painting a live window resize costs. The window
// host is the subject (W2.9, W3.6, W6.1, W6.2); the embedded host runs the same sweep for C++ allocations, memory and
// hardware timing.
#include "Embedded.Tests.PerformanceFixture.h"
#include "Embedded.Tests.PerformanceFixtureFx9.h"

#include <wil/result.h>

#include <format>

namespace PerformanceFixtures::Fx10
{
inline constexpr UINT kStartWidth   = 800u;
inline constexpr UINT kStartHeight  = 600u;
inline constexpr size_t kSweepSteps = 30u;
inline constexpr size_t kListRows   = 500u;

inline constexpr std::array kVariants{
    // The suites' variant: the same screen swept from 800x600 to 1280x720.
    Variant{"default", 1280u, 720u, 96.0f, false, false},
    // The plan's sweep: 800x600 to 3840x2160 (a 4K client area also on a smaller desktop; the window runner sizes its
    // off-screen, non-activating window beyond the desktop).
    Variant{"uhd", 3840u, 2160u, 96.0f, false, true},
};
inline constexpr size_t kSweep = 1u;
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "the host is resized to 800x600; one unchanged frame"},
    Phase{"sweep", kSweepSteps, PhaseKind::Measured, "step k resizes the host to 800x600 plus (k + 1) / 30 of the way to the variant's size"},
    Phase{"rest", 3u, PhaseKind::Measured, "unchanged frames at the variant's size"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx10.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx9.h")};

// The size at sweep step `step`: steps 0 to 29 grow the host in 30 equal increments from the start size (the warm-up
// size) to the variant's size; later steps (the timing harness) sweep back and forth.
[[nodiscard]] inline D2D1_SIZE_U SweepSize(const Variant& variant, size_t step) noexcept
{
    const size_t position  = step % (2u * kSweepSteps);
    const size_t k         = position < kSweepSteps ? position + 1u : 2u * kSweepSteps - 1u - position;
    const auto interpolate = [k](UINT from, UINT to) { return static_cast<UINT>(from + (static_cast<uint64_t>(to - from) * k) / kSweepSteps); };
    return D2D1_SIZE_U{interpolate(kStartWidth, variant.width), interpolate(kStartHeight, variant.height)};
}

inline constexpr const wchar_t* kParagraph =
    L"This review collects the quarter's operational figures. The form on the left edits the selected site, the list shows every "
    L"shipment recorded this quarter and the notes on the right explain the variances. Resizing the window reflows this paragraph, "
    L"the form's two columns, the list's visible rows and the notes, and wraps the actions below.";

inline constexpr const wchar_t* kNotes =
    L"# Variance notes\n\n"
    L"Shipments rose **12%** over the previous quarter while returns fell to *1.8%*. The northern depot carried most of the "
    L"increase; the coastal depot was closed for two weeks of maintenance.\n\n"
    L"## Actions\n\n"
    L"- Rebalance carrier contracts before the holiday peak.\n"
    L"- Move the weekly audit to Tuesdays so it no longer collides with the inventory count.\n"
    L"- Publish the revised returns policy in all three languages.\n\n"
    L"> Figures are provisional until the finance team closes the quarter.\n";

class Scene final : public Scenario
{
public:
    Scene() : list_(kListRows)
    {
    }
    HRESULT Build(Context& context) override
    {
        auto root = std::make_unique<DxUi::FlexPanel>();
        RETURN_IF_FAILED(root->SetOrientation(DxUi::StackOrientation::Vertical));
        RETURN_IF_FAILED(root->SetGap(12.0f));
        RETURN_IF_FAILED(root->SetPadding(16.0f, 16.0f, 16.0f, 16.0f));

        auto* title = root->AddChild<DxUi::Label>(L"Quarterly operations review");
        RETURN_IF_FAILED(title->SetTypographyOverride(DxUi::LabelTypography{22.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 0.0f}));
        auto* paragraph = root->AddChild<DxUi::Label>(kParagraph);
        paragraph->SetMultiline(true);

        auto* body = root->AddChild<DxUi::FlexPanel>();
        RETURN_IF_FAILED(body->SetOrientation(DxUi::StackOrientation::Horizontal));
        RETURN_IF_FAILED(body->SetGap(12.0f));
        RETURN_IF_FAILED(root->SetChildOptions(body, DxUi::FlexChildOptions{.grow = 1.0f}));

        auto* form = body->AddChild<DxUi::FormPanel>();
        RETURN_IF_FAILED(form->SetColumns(2u));
        RETURN_IF_FAILED(form->SetGap(12.0f, 8.0f));
        static constexpr std::array<const wchar_t*, 4> kTextFields{L"Site name", L"Manager", L"Address", L"Contact e-mail"};
        static constexpr std::array<const wchar_t*, 4> kTextValues{L"Northern depot", L"Dana Okafor", L"14 Harbour Road, Unit 3", L"depot.north@example.com"};
        for (size_t index = 0u; index < kTextFields.size(); ++index)
        {
            auto* field = form->AddField(std::format(L"site.text{}", index), kTextFields[index]);
            if (! field || ! field->AddEditor<DxUi::TextField>(kTextValues[index]))
                return E_FAIL;
        }
        auto* region = form->AddField(L"site.region", L"Region");
        if (! region || ! region->AddEditor<DxUi::ComboBox>())
            return E_FAIL;
        region->SetDescription(L"Regions group sites for the quarterly totals.");
        auto* capacity = form->AddField(L"site.capacity", L"Capacity (pallets)");
        auto* stepper  = capacity ? capacity->AddEditor<DxUi::NumericStepper>() : nullptr;
        if (! stepper)
            return E_FAIL;
        stepper->SetValue(1'200.0);
        auto* audited = form->AddField(L"site.audited", L"Audit");
        if (! audited || ! audited->AddEditor<DxUi::Checkbox>(L"Audited this quarter"))
            return E_FAIL;
        RETURN_IF_FAILED(body->SetChildOptions(form, DxUi::FlexChildOptions{.constraints = {.minimumDip = D2D1::SizeF(320.0f, 0.0f)}, .grow = 1.0f}));

        auto* list = body->AddChild<DxUi::Grid>();
        list->SetModel(&list_);
        RETURN_IF_FAILED(body->SetChildOptions(list, DxUi::FlexChildOptions{.explicitSizeDip = D2D1::SizeF(320.0f, 200.0f), .grow = 2.0f}));

        auto* notes = body->AddChild<DxUi::RichTextEditor>(kNotes);
        notes->SetReadOnly(true);
        RETURN_IF_FAILED(body->SetChildOptions(notes, DxUi::FlexChildOptions{.explicitSizeDip = D2D1::SizeF(280.0f, 200.0f), .grow = 1.0f}));

        auto* actions = root->AddChild<DxUi::FlexPanel>();
        RETURN_IF_FAILED(actions->SetOrientation(DxUi::StackOrientation::Horizontal));
        RETURN_IF_FAILED(actions->SetWrap(true));
        RETURN_IF_FAILED(actions->SetGap(8.0f));
        static constexpr std::array<const wchar_t*, 7> kActions{
            L"Save site", L"Revert changes", L"Export shipments", L"Print summary", L"Open audit log", L"Share review", L"Close quarter"};
        for (const wchar_t* action : kActions)
            actions->AddChild<DxUi::Button>(action);
        actions->AddChild<DxUi::Label>(L"Last saved 14 November 2026 at 09:30");

        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        const Variant& variant = context.variant;
        if (phase == 0u && step == 0u)
            context.port.Resize(kStartWidth, kStartHeight, variant.dpi);
        else if (phase == kSweep)
        {
            const D2D1_SIZE_U size = SweepSize(variant, step);
            context.port.Resize(size.width, size.height, variant.dpi);
        }
    }

private:
    Fx9::SyntheticGridModel list_;
};

inline constexpr Definition kDefinition{
    .id         = "FX10",
    .title      = "Live resize",
    .scenario   = "A representative screen laid out by the library's panels: a vertical FlexPanel holding a title, a wrapping paragraph, a body (a "
                  "two-column FormPanel of seven fields with TextField, ComboBox, NumericStepper and Checkbox editors, a Grid over a synthetic 500-row "
                  "model and a read-only RichTextEditor with headings, emphasis, a list and a quote) and a wrapping row of seven Buttons and a Label. "
                  "The host is resized from 800x600 to the variant's size (uhd: 3840x2160) in 30 linear steps at 96 DPI, one frame per step; the "
                  "window host's off-screen, non-activating window takes a client area beyond the desktop. Reduced motion.",
    .hosts      = kBothHosts,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "sweep",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx10
