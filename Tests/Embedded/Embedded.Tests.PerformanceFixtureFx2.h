#pragma once
// FX2 ComboBox (plan W0.4): an editable ComboBox of 500 items opened with F4, 60 hover moves over its popup rows, closed
// with Escape, then 20 characters typed into it while closed; then a second editable ComboBox of 100 items whose popup
// shows every row (SetMaxVisibleItems(100) in a view tall enough), opened, hovered and closed. Proves G3, G11.
//
// Embedded host only: in an EmbeddedHost the popup is drawn into the view, while a WindowHost opens it as a native popup
// window, which the ordinary suites do not create. Focus moves through ControlHost::SetFocusControl without a native focus
// transfer; nothing takes focus.
#include "Embedded.Tests.PerformanceFixture.h"
#include "Embedded.Tests.TextHistoryEdits.h"

#include <string>
#include <vector>

namespace PerformanceFixtures::Fx2
{
inline constexpr std::array kBudgets{std::string_view("G3"), std::string_view("G11")};
inline constexpr size_t kItems            = 500u;
inline constexpr size_t kFitItems         = 100u;
inline constexpr size_t kHoverMoves       = 60u;
inline constexpr size_t kFitHoverMoves    = 20u;
inline constexpr std::wstring_view kTyped = L"Item 42 and Item 499";
static_assert(kTyped.size() == 20u, "FX2 types 20 characters");
inline constexpr size_t kWarm     = 0u;
inline constexpr size_t kOpen     = 1u;
inline constexpr size_t kHover    = 2u;
inline constexpr size_t kClose    = 3u;
inline constexpr size_t kType     = 4u;
inline constexpr size_t kFitOpen  = 5u;
inline constexpr size_t kFitHover = 6u;
inline constexpr size_t kFitClose = 7u;
inline constexpr size_t kHistory  = 8u;
// The "history" variant's text (W9.3): 64 Ki UTF-16 units of seven-letter words, set on the 500-item ComboBox at build.
inline constexpr size_t kHistoryUnits = 65'536u;

// Tall enough for the 100-row popup below the second ComboBox. "history" (W9.3, receipt only): the 500-item ComboBox starts
// with kHistoryUnits of text and, after the other phases, the history phase makes 128 edits in it
// (Embedded.Tests.TextHistoryEdits.h), each its own undo step; in every other variant that phase does nothing.
inline constexpr std::array kVariants{Variant{"default", 800u, 3600u, 96.0f, false}, Variant{"history", 800u, 3600u, 96.0f, false, true}};
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "nothing changes: the first frames lay out both ComboBoxes"},
    Phase{"open", 1u, PhaseKind::Measured, "focus the 500-item ComboBox and press F4: its popup opens"},
    Phase{"hover", kHoverMoves, PhaseKind::Measured, "the pointer moves to the centre of popup row (step mod 8)"},
    Phase{"close", 1u, PhaseKind::Measured, "Escape closes the popup"},
    Phase{"type", kTyped.size(), PhaseKind::Measured, "one character typed into the closed editable ComboBox (popup does not auto-open)"},
    Phase{"fit-open", 1u, PhaseKind::Measured, "focus the 100-item ComboBox and press F4: its popup opens with every row visible"},
    Phase{"fit-hover", kFitHoverMoves, PhaseKind::Measured, "the pointer moves to the centre of popup row 5 x step"},
    Phase{"fit-close", 1u, PhaseKind::Measured, "Escape closes the popup"},
    Phase{"history",
          TextHistoryEdits::kEdits,
          PhaseKind::Measured,
          "history variant: the 500-item ComboBox takes focus again and step k makes edit k of 128 at its own place (W9.3); other "
          "variants: nothing"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx2.h"),
                                     std::string_view("Tests/Embedded/Embedded.Tests.TextHistoryEdits.h")};

[[nodiscard]] inline std::vector<DxUi::ComboBox::Item> MakeItems(size_t count)
{
    std::vector<DxUi::ComboBox::Item> items;
    items.reserve(count);
    for (size_t index = 0u; index < count; ++index)
    {
        std::wstring text = L"Item " + std::to_wstring(index);
        items.push_back({text, text});
    }
    return items;
}

class Scene final : public Scenario
{
public:
    HRESULT Build(Context& context) override
    {
        auto root = std::make_unique<DxUi::Panel>();
        combo_    = root->AddChild<DxUi::ComboBox>();
        combo_->SetEditable(true);
        combo_->SetAutoOpenOnTextInput(false);
        combo_->SetItems(MakeItems(kItems));
        combo_->SetBounds(D2D1::RectF(16.0f, 16.0f, 336.0f, 48.0f));
        if (context.variant.name == "history")
        {
            std::wstring text(kHistoryUnits, L'a');
            for (size_t index = 0u; index < text.size(); ++index)
                text[index] = (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(L'a' + (index % 26u));
            combo_->SetText(std::move(text));
            history_.reserve(TextHistoryEdits::kEdits);
        }
        fit_ = root->AddChild<DxUi::ComboBox>();
        fit_->SetEditable(true);
        fit_->SetAutoOpenOnTextInput(false);
        fit_->SetMaxVisibleItems(kFitItems);
        fit_->SetItems(MakeItems(kFitItems));
        fit_->SetBounds(D2D1::RectF(400.0f, 16.0f, 720.0f, 48.0f));
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        const float scale = context.variant.dpi / 96.0f;
        const auto key    = [&](UINT virtualKey)
        {
            static_cast<void>(context.port.Key(virtualKey, true));
            static_cast<void>(context.port.Key(virtualKey, false));
        };
        const auto hoverRow = [&](DxUi::ComboBox& combo, size_t row)
        {
            const D2D1_RECT_F rect = combo.DebugGetPopupItemRect(row, &context.controls);
            static_cast<void>(
                context.port.Pointer({DxUi::PointerAction::Move, (rect.left + rect.right) / 2.0f * scale, (rect.top + rect.bottom) / 2.0f * scale}));
        };
        switch (phase)
        {
            case kOpen:
                context.controls.SetFocusControl(combo_, false);
                key(VK_F4);
                break;
            case kHover: hoverRow(*combo_, step % 8u); break;
            case kClose:
            case kFitClose: key(VK_ESCAPE); break;
            case kType: static_cast<void>(context.port.Character(kTyped[step % kTyped.size()])); break;
            case kFitOpen:
                context.controls.SetFocusControl(fit_, false);
                key(VK_F4);
                break;
            case kFitHover: hoverRow(*fit_, (step * 5u) % kFitItems); break;
            case kHistory:
                if (context.variant.name != "history")
                    break;
                if (step == 0u)
                    context.controls.SetFocusControl(combo_, false);
                TextHistoryEdits::Apply(context, *combo_, step, history_);
                break;
            default: break;
        }
    }
    // The history variant's edits, in order (W9.3 gate).
    [[nodiscard]] const std::vector<TextHistoryEdits::Record>& History() const noexcept
    {
        return history_;
    }
    [[nodiscard]] DxUi::ComboBox& Combo() const noexcept
    {
        return *combo_;
    }

private:
    DxUi::ComboBox* combo_ = nullptr;
    DxUi::ComboBox* fit_   = nullptr;
    std::vector<TextHistoryEdits::Record> history_;
};

inline constexpr Definition kDefinition{
    .id         = "FX2",
    .title      = "ComboBox popup and typing",
    .scenario   = "A ComboBox with 500 items, open with 60 hover moves and closed with 20 typed characters; plus a 100-item popup in which every "
                  "row fits. Embedded host (popup drawn into the view). Proves G3, G11. The history variant starts the first ComboBox with 64 Ki "
                  "units of text and ends with 128 edits in it, each its own undo step: proves W9.3's text-history gate (G13).",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "hover",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx2
