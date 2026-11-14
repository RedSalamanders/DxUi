#pragma once
// The tooltip hover path (W1.6), shared by the Tooltip suite and the FX14 performance fixture: the pointer enters one
// tooltip-bearing control and then makes 50 further moves inside it. Points are view pixels; the path stays inside a
// control placed at (8, 8)-(200, 100) DIPs at any DPI from 96 to 192. Host-neutral: DxUi and Windows types only.
#include <DxUi/DxUi.h>

#include <memory>
#include <string>
#include <string_view>

inline constexpr size_t kTooltipHoverPathMoves = 50u;

[[nodiscard]] inline POINT TooltipHoverPathPixel(size_t move) noexcept
{
    return POINT{static_cast<LONG>(20u + 2u * move), static_cast<LONG>(20u + move)};
}

[[nodiscard]] inline std::unique_ptr<DxUi::Panel> MakeTooltipHoverPathRoot(std::wstring_view tooltipText)
{
    auto root  = std::make_unique<DxUi::Panel>();
    auto* help = root->AddChild<DxUi::Label>(L"Help");
    help->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 100.0f));
    help->SetTooltipText(std::wstring(tooltipText));
    return root;
}
