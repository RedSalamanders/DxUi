#pragma once
// ComboBox popup layout (plan item W2.2), shared by the native and embedded suites: seeded item sets that stress the measured
// popup width (empty, one item, a very long item, CJK, emoji, right-to-left, mixed scripts, 500 items), and the "combo-popup"
// script of the incremental-versus-fresh oracle. Every step of that script changes one input of the popup's retained widths
// or placement (items, filter, maximum rows, row height, DPI, density, flow, bounds, no-matches text, scroll, hover); the
// fresh scene drops both caches before drawing, so a stale width or placement is a pixel difference.
// Source text stays ASCII: non-ASCII strings are written as escapes.
#include "Support.Tests.IncrementalParity.h"

#include <DxUi/DxUi.h>

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace ComboBoxPopupLayoutTests
{
using IncrementalParity::Rng;

enum class ItemSet : uint32_t
{
    Empty,
    One,
    VeryLong,
    Cjk,
    Emoji,
    RightToLeft,
    Mixed,
    FiveHundred,
    Count,
};

[[nodiscard]] constexpr std::string_view ItemSetName(ItemSet set) noexcept
{
    switch (set)
    {
        case ItemSet::Empty: return "empty";
        case ItemSet::One: return "one item";
        case ItemSet::VeryLong: return "very long item";
        case ItemSet::Cjk: return "CJK";
        case ItemSet::Emoji: return "emoji";
        case ItemSet::RightToLeft: return "right-to-left";
        case ItemSet::Mixed: return "mixed scripts";
        case ItemSet::FiveHundred: return "500 items";
        case ItemSet::Count: break;
    }
    return "?";
}

// Pieces that change glyph runs, shaping and fallback: Latin, accents, CJK, emoji (surrogate pairs, a ZWJ sequence),
// Hebrew and Arabic, digits and spaces.
inline constexpr std::array<std::wstring_view, 14u> kPieces{
    L"Item",
    L"a",
    L"\u00E9t\u00E9",
    L"\u00DF",
    L"\u6771\u4EAC",
    L"\u4E2D\u6587\u5B57",
    L"\U0001F600",
    L"\U0001F469\u200D\U0001F4BB",
    L"\u05E9\u05DC\u05D5\u05DD",
    L"\u0645\u0631\u062D\u0628\u0627",
    L"0",
    L"42",
    L" ",
    L"W",
};

[[nodiscard]] inline std::wstring SeededText(Rng& rng, std::span<const size_t> pieces, size_t count)
{
    std::wstring text;
    for (size_t index = 0u; index < count; ++index)
        text += kPieces[pieces[rng.Below(static_cast<uint32_t>(pieces.size()))]];
    return text;
}

// The items of `set`, the same for the same seed on every machine. Values and display texts differ in some sets, so the
// popup filter (which matches either) and the width (which measures the display text) are both exercised.
[[nodiscard]] inline std::vector<DxUi::ComboBox::Item> MakeItems(ItemSet set, uint64_t seed)
{
    Rng rng{seed};
    std::vector<DxUi::ComboBox::Item> items;
    const auto add = [&](std::wstring value, std::wstring display) { items.push_back({std::move(value), std::move(display)}); };
    switch (set)
    {
        case ItemSet::Empty: break;
        case ItemSet::One: add(L"one", L"Item one"); break;
        case ItemSet::VeryLong:
        {
            add(L"short", L"Short");
            std::wstring longText;
            for (size_t index = 0u; index < 40u; ++index)
                longText += std::format(L"Very long item {} ", index);
            add(L"long", std::move(longText));
            add(L"tail", L"Item tail");
            break;
        }
        case ItemSet::Cjk:
        {
            constexpr std::array<size_t, 3u> pieces{4u, 5u, 10u};
            for (size_t index = 0u; index < 24u; ++index)
                add(std::format(L"cjk{}", index), SeededText(rng, pieces, 1u + rng.Below(6u)));
            break;
        }
        case ItemSet::Emoji:
        {
            constexpr std::array<size_t, 4u> pieces{6u, 7u, 1u, 12u};
            for (size_t index = 0u; index < 24u; ++index)
                add(std::format(L"emoji{}", index), SeededText(rng, pieces, 1u + rng.Below(6u)));
            break;
        }
        case ItemSet::RightToLeft:
        {
            constexpr std::array<size_t, 4u> pieces{8u, 9u, 12u, 11u};
            for (size_t index = 0u; index < 24u; ++index)
            {
                std::wstring text = SeededText(rng, pieces, 1u + rng.Below(5u));
                add(text, text);
            }
            break;
        }
        case ItemSet::Mixed:
        {
            constexpr std::array<size_t, 14u> pieces{0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u};
            for (size_t index = 0u; index < 60u; ++index)
            {
                std::wstring text = SeededText(rng, pieces, 1u + rng.Below(7u));
                // Half the items share the display text as their value: the filter then matches the display prefix.
                add(index % 2u == 0u ? text : std::format(L"v{}", index), text);
            }
            break;
        }
        case ItemSet::FiveHundred:
            for (size_t index = 0u; index < 500u; ++index)
            {
                std::wstring text = L"Item " + std::to_wstring(index);
                add(text, text);
            }
            break;
        case ItemSet::Count: break;
    }
    return items;
}

// Characters typed into the field: prefixes of the sets above, a character no item starts with, and the halves of a
// surrogate pair (typed one after the other).
inline constexpr std::array<std::wstring_view, 10u> kTyped{
    L"I",
    L"t",
    L"4",
    L"2",
    L"\u6771",
    L"\u05E9",
    L"a",
    L"\U0001F600",
    L"q",
    L" ",
};

// ---- The "combo-popup" incremental-versus-fresh script ---------------------------------------------------------------

enum ComboSlot : size_t
{
    kComboRoot,
    kCombo,
    kComboSlotCount,
};

template <typename Scene> void BuildComboPopupScene(Scene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root               = std::make_unique<DxUi::Panel>();
    scene.slots[kComboRoot] = root.get();
    auto* combo             = root->AddChild<DxUi::ComboBox>();
    combo->SetEditable(true);
    combo->SetAutoOpenOnTextInput(true);
    combo->SetItems(MakeItems(ItemSet::Mixed, 0x5EED0201u));
    combo->SetBounds(D2D1::RectF(40.0f, 24.0f, 260.0f, 56.0f));
    scene.slots[kCombo] = combo;
    scene.SetRoot(std::move(root));
    scene.Host().SetFocusControl(combo, false);
}

template <typename Scene> void StepComboPopup(Scene& scene, Rng& rng, size_t /*step*/)
{
    auto* combo         = static_cast<DxUi::ComboBox*>(scene.slots[kCombo]);
    DxUi::Control* root = scene.slots[kComboRoot];
    switch (rng.Below(14u))
    {
        case 0u:
        case 1u:
            IncrementalParity::NoteStep("open or close");
            scene.Host().SetFocusControl(combo, false);
            scene.Key(combo->IsPopupOpen() ? VK_ESCAPE : VK_F4);
            break;
        case 2u:
        {
            IncrementalParity::NoteStep("hover a popup row");
            if (! combo->IsPopupOpen())
                break;
            const D2D1_RECT_F row = combo->DebugGetPopupItemRect(rng.Below(12u), &scene.Host());
            if (row.right > row.left)
                scene.Pointer(IncrementalParity::ParityPointer::Move, D2D1::Point2F((row.left + row.right) * 0.5f, (row.top + row.bottom) * 0.5f));
            break;
        }
        case 3u:
        case 4u:
        {
            IncrementalParity::NoteStep("type into the filter");
            scene.Host().SetFocusControl(combo, false);
            for (const wchar_t unit : kTyped[rng.Below(static_cast<uint32_t>(kTyped.size()))])
                scene.Character(unit);
            break;
        }
        case 5u:
            IncrementalParity::NoteStep("backspace");
            scene.Host().SetFocusControl(combo, false);
            scene.Key(VK_BACK);
            break;
        case 6u:
            IncrementalParity::NoteStep("replace the items");
            combo->SetItems(MakeItems(static_cast<ItemSet>(rng.Below(static_cast<uint32_t>(ItemSet::Count))), rng.Next()));
            break;
        case 7u:
            IncrementalParity::NoteStep("maximum visible rows");
            combo->SetMaxVisibleItems(rng.Below(12u));
            break;
        case 8u:
            IncrementalParity::NoteStep("minimum row height");
            combo->SetMinimumPopupItemHeight(rng.Below(2u) ? 0.0f : 36.0f);
            break;
        case 9u:
        {
            IncrementalParity::NoteStep("dpi");
            constexpr std::array<UINT, 3u> dpis{96u, 144u, 192u};
            scene.SetDpi(dpis[rng.Below(3u)]);
            break;
        }
        case 10u:
        {
            IncrementalParity::NoteStep("density or flow direction");
            if (rng.Below(2u))
            {
                DxUi::ThemePalette theme = scene.Host().GetTheme();
                theme.density            = theme.density == DxUi::Density::Compact ? DxUi::Density::Standard : DxUi::Density::Compact;
                scene.Host().SetTheme(theme);
            }
            else
                root->SetFlowDirection(root->IsRightToLeft() ? DxUi::FlowDirection::LeftToRight : DxUi::FlowDirection::RightToLeft);
            break;
        }
        case 11u:
        {
            IncrementalParity::NoteStep("field bounds");
            const float left  = 8.0f + 200.0f * rng.Unit();
            const float top   = 8.0f + 380.0f * rng.Unit();
            const float width = 80.0f + 300.0f * rng.Unit();
            combo->SetBounds(D2D1::RectF(left, top, left + width, top + 32.0f));
            break;
        }
        case 12u:
            IncrementalParity::NoteStep("no-matches text");
            combo->SetNoMatchesText(rng.Below(2u) ? std::wstring() : std::wstring(L"Aucun r\u00E9sultat pour cette recherche"));
            break;
        default:
        {
            IncrementalParity::NoteStep("wheel over the popup");
            if (! combo->IsPopupOpen())
                break;
            const D2D1_RECT_F row  = combo->DebugGetPopupItemRect(0u, &scene.Host());
            const D2D1_RECT_F hit  = combo->GetHitBounds();
            const D2D1_POINT_2F at = row.right > row.left ? D2D1::Point2F((row.left + row.right) * 0.5f, (row.top + row.bottom) * 0.5f)
                                                          : D2D1::Point2F(hit.left + 4.0f, hit.bottom - 4.0f);
            scene.Pointer(IncrementalParity::ParityPointer::Wheel, at, rng.Below(2u) ? -120.0f : 120.0f);
            break;
        }
    }
}

inline constexpr std::array<uint64_t, 3u> kComboPopupSeeds{0x5EED0211u, 0x5EED0212u, 0x5EED0213u};

template <typename Scene> [[nodiscard]] IncrementalParity::ParityScript<Scene> ComboPopupScript() noexcept
{
    return {"combo-popup", kComboPopupSeeds, 40u, &BuildComboPopupScene<Scene>, &StepComboPopup<Scene>};
}
} // namespace ComboBoxPopupLayoutTests
