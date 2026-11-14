#pragma once

// FX6, the accessibility fixture of the performance and motion plan: 100 mutations of a few controls, made with no UI
// Automation client and with one. The controls are a status label, a named text field and a named slider; mutation i
// sets the label to "Status i" (i % 3 == 0), the field to "Value i" (i % 3 == 1) or the slider to i (i % 3 == 2). None
// moves the focus or changes the tree's structure. The window host and the embedded view build the same controls.

#include "Controls.Tests.DxUiTestHelpers.h"

#include <cstddef>
#include <string>

namespace Fx6
{
inline constexpr size_t kMutations = 100u;
inline constexpr float kWidthDip   = 320.0f;
inline constexpr float kHeightDip  = 200.0f;

struct Controls final
{
    DxUi::Label* label     = nullptr;
    DxUi::TextField* field = nullptr;
    DxUi::Slider* slider   = nullptr;

    // Builds the controls under a new panel and gives the panel to `host`.
    void Build(DxUi::ControlHost& host)
    {
        auto root = std::make_unique<DxUi::Panel>();
        label     = root->AddChild<DxUi::Label>(L"Status");
        label->SetBounds(D2D1::RectF(8.0f, 8.0f, 240.0f, 32.0f));
        field = root->AddChild<DxUi::TextField>(L"Value");
        field->SetBounds(D2D1::RectF(8.0f, 40.0f, 240.0f, 68.0f));
        field->SetAccessibleName(L"Entry");
        slider = root->AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(8.0f, 76.0f, 240.0f, 112.0f));
        slider->SetAccessibleName(L"Level");
        slider->SetMinimum(0.0);
        slider->SetMaximum(200.0);
        host.SetRoot(std::move(root));
    }

    void Mutate(size_t index) const
    {
        switch (index % 3u)
        {
            case 0u: label->SetText(L"Status " + std::to_wstring(index)); break;
            case 1u: field->SetText(L"Value " + std::to_wstring(index)); break;
            default: slider->SetValue(static_cast<double>(index)); break;
        }
    }
};

// What the last of the 100 mutations left: the label's, field's and slider's values.
inline const std::wstring kFinalLabel = L"Status 99";
inline const std::wstring kFinalField = L"Value 97";
inline constexpr double kFinalSlider  = 98.0;
} // namespace Fx6
