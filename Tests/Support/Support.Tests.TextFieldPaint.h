#pragma once
// Text field paint (plan item W2.7), shared by the native and embedded suites: the "text-field" script of the
// incremental-versus-fresh oracle. A multiline TextField whose text wraps, a single-line field and a masked field, and steps
// that change every input of the multiline field's retained layout and line metrics (text, width, font role, DPI, density,
// padding, masking and reveal) and every path that reads them (paint, the caret, sub-notch and whole wheel notches, PageUp and
// PageDown, arrow keys, Home and End, clicks and drags). The fresh scene drops every registered cache before it draws, so a
// stale layout, stale line metrics or a stale fallback line height is a pixel difference.
// Source text stays ASCII: non-ASCII strings are written as escapes.
#include "Support.Tests.IncrementalParity.h"

#include <DxUi/DxUi.h>

#include <array>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace TextFieldPaintTests
{
using IncrementalParity::Rng;

// Lines of mixed length (some wrap at the field's width), with accents, CJK, a surrogate pair and Hebrew.
[[nodiscard]] inline std::wstring ParityMultilineText()
{
    constexpr std::array<std::wstring_view, 6u> kPieces{
        L"The quick brown fox jumps over the lazy dog",
        L"\u00E9t\u00E9 caf\u00E9",
        L"\u6771\u4EAC\u306E\u5929\u6C17",
        L"\U0001F600 smile",
        L"\u05E9\u05DC\u05D5\u05DD",
        L"a long line that keeps going well past the width of the field so that it wraps more than once at any width the script picks",
    };
    std::wstring text;
    for (size_t line = 0u; line < 60u; ++line)
    {
        text += std::format(L"{:02} ", line);
        text += kPieces[line % kPieces.size()];
        if (line + 1u < 60u)
            text += L'\n';
    }
    return text;
}

enum TextFieldSlot : size_t
{
    kTextRoot,
    kTextMultiline,
    kTextSingle,
    kTextMasked,
    kTextSlotCount,
};

template <typename Scene> void BuildTextFieldScene(Scene& scene)
{
    scene.Host().SetTheme(IncrementalParity::ParityTheme(false));
    auto root              = std::make_unique<DxUi::Panel>();
    scene.slots[kTextRoot] = root.get();
    auto* multiline        = root->AddChild<DxUi::TextField>();
    multiline->SetMultiline(true);
    multiline->SetText(ParityMultilineText());
    multiline->SetBounds(D2D1::RectF(16.0f, 16.0f, 360.0f, 220.0f));
    scene.slots[kTextMultiline] = multiline;
    auto* single                = root->AddChild<DxUi::TextField>(L"Single line field");
    single->SetBounds(D2D1::RectF(16.0f, 236.0f, 300.0f, 268.0f));
    scene.slots[kTextSingle] = single;
    auto* masked             = root->AddChild<DxUi::TextField>(L"secret pass phrase");
    masked->SetMasked(true);
    masked->SetBounds(D2D1::RectF(16.0f, 280.0f, 300.0f, 312.0f));
    scene.slots[kTextMasked] = masked;
    scene.SetRoot(std::move(root));
    scene.Host().SetFocusControl(multiline, false);
}

template <typename Scene> void StepTextField(Scene& scene, Rng& rng, size_t /*step*/)
{
    auto* multiline          = static_cast<DxUi::TextField*>(scene.slots[kTextMultiline]);
    auto* single             = static_cast<DxUi::TextField*>(scene.slots[kTextSingle]);
    auto* masked             = static_cast<DxUi::TextField*>(scene.slots[kTextMasked]);
    const D2D1_RECT_F bounds = multiline->GetBounds();
    const D2D1_POINT_2F centre{(bounds.left + bounds.right) * 0.5f, (bounds.top + bounds.bottom) * 0.5f};
    const auto focus = [&](DxUi::TextField* field) { scene.Host().SetFocusControl(field, false); };
    switch (rng.Below(17u))
    {
        case 0u:
        case 1u:
        {
            IncrementalParity::NoteStep("type into the multiline field");
            focus(multiline);
            constexpr std::array<std::wstring_view, 5u> kTyped{L"x", L"\u00E9", L"\u6771", L"\U0001F600", L"ab cd"};
            for (const wchar_t unit : kTyped[rng.Below(static_cast<uint32_t>(kTyped.size()))])
                scene.Character(unit);
            break;
        }
        case 2u:
            IncrementalParity::NoteStep("Enter or Backspace");
            focus(multiline);
            if (rng.Below(2u))
                scene.Character(L'\r');
            else
                scene.Key(VK_BACK);
            break;
        case 3u:
        case 4u:
        {
            IncrementalParity::NoteStep("sub-notch wheel");
            constexpr std::array<float, 4u> kDeltas{-30.0f, -45.0f, 30.0f, -60.0f};
            scene.Pointer(IncrementalParity::ParityPointer::Move, centre);
            scene.Pointer(IncrementalParity::ParityPointer::Wheel, centre, kDeltas[rng.Below(static_cast<uint32_t>(kDeltas.size()))]);
            break;
        }
        case 5u:
            IncrementalParity::NoteStep("whole wheel notch");
            scene.Pointer(IncrementalParity::ParityPointer::Move, centre);
            scene.Pointer(IncrementalParity::ParityPointer::Wheel, centre, rng.Below(3u) ? -120.0f : 120.0f);
            break;
        case 6u:
            IncrementalParity::NoteStep("PageDown or PageUp");
            focus(multiline);
            scene.Key(rng.Below(3u) ? VK_NEXT : VK_PRIOR);
            break;
        case 7u:
        {
            IncrementalParity::NoteStep("arrow, Home or End");
            focus(multiline);
            constexpr std::array<UINT, 6u> kKeys{VK_UP, VK_DOWN, VK_DOWN, VK_HOME, VK_END, VK_RIGHT};
            scene.Key(kKeys[rng.Below(static_cast<uint32_t>(kKeys.size()))]);
            break;
        }
        case 8u:
        {
            IncrementalParity::NoteStep("click or drag in the multiline field");
            const D2D1_POINT_2F from = IncrementalParity::RandomPointIn(*multiline, rng);
            const D2D1_POINT_2F to   = IncrementalParity::RandomPointIn(*multiline, rng);
            scene.Pointer(IncrementalParity::ParityPointer::Move, from);
            scene.Pointer(IncrementalParity::ParityPointer::Down, from);
            if (rng.Below(2u))
                scene.Pointer(IncrementalParity::ParityPointer::Move, to);
            scene.Pointer(IncrementalParity::ParityPointer::Up, rng.Below(2u) ? to : from);
            break;
        }
        case 9u:
        {
            IncrementalParity::NoteStep("resize the multiline field");
            const float width  = 120.0f + 360.0f * rng.Unit();
            const float height = 60.0f + 180.0f * rng.Unit();
            multiline->SetBounds(D2D1::RectF(bounds.left, bounds.top, bounds.left + width, bounds.top + height));
            break;
        }
        case 10u:
        {
            IncrementalParity::NoteStep("font role");
            constexpr std::array<DxUi::FontRole, 4u> kRoles{DxUi::FontRole::Body, DxUi::FontRole::Small, DxUi::FontRole::Title, DxUi::FontRole::Monospace};
            const DxUi::FontRole role = kRoles[rng.Below(static_cast<uint32_t>(kRoles.size()))];
            multiline->SetFontRole(role);
            single->SetFontRole(role);
            break;
        }
        case 11u:
        {
            IncrementalParity::NoteStep("dpi");
            constexpr std::array<UINT, 3u> kDpis{96u, 144u, 192u};
            scene.SetDpi(kDpis[rng.Below(3u)]);
            break;
        }
        case 12u:
        {
            IncrementalParity::NoteStep("density or padding");
            if (rng.Below(2u))
                scene.slots[kTextRoot]->SetDensity(scene.slots[kTextRoot]->IsCompactDensity() ? DxUi::Density::Standard : DxUi::Density::Compact);
            else
                multiline->SetHorizontalTextPadding(4.0f + 12.0f * rng.Unit(), 4.0f + 12.0f * rng.Unit());
            break;
        }
        case 13u:
            IncrementalParity::NoteStep("mask or unmask the multiline field");
            multiline->SetMasked(! multiline->IsMasked());
            break;
        case 14u:
            IncrementalParity::NoteStep("reveal or hide a masked field");
            masked->SetPasswordRevealState(masked->GetPasswordRevealState() == DxUi::PasswordRevealState::Visible ? DxUi::PasswordRevealState::Hidden
                                                                                                                  : DxUi::PasswordRevealState::Visible);
            if (multiline->IsMasked())
                multiline->SetPasswordRevealState(multiline->GetPasswordRevealState() == DxUi::PasswordRevealState::Visible
                                                      ? DxUi::PasswordRevealState::Hidden
                                                      : DxUi::PasswordRevealState::Visible);
            break;
        case 15u:
        {
            IncrementalParity::NoteStep("type into a single-line field");
            DxUi::TextField* field = rng.Below(2u) ? single : masked;
            focus(field);
            scene.Character(rng.Below(2u) ? L'k' : L'\u00E9');
            if (rng.Below(3u) == 0u)
                scene.Key(VK_LEFT);
            break;
        }
        default:
            IncrementalParity::NoteStep("set or undo the multiline text");
            if (rng.Below(2u))
                multiline->SetText(rng.Below(2u) ? ParityMultilineText() : std::wstring(L"short\ntext"));
            else
            {
                focus(multiline);
                scene.Key(VK_END);
            }
            break;
    }
}

inline constexpr std::array<uint64_t, 3u> kTextFieldSeeds{0x5EED0271u, 0x5EED0272u, 0x5EED0273u};

template <typename Scene> [[nodiscard]] IncrementalParity::ParityScript<Scene> TextFieldScript() noexcept
{
    return {"text-field", kTextFieldSeeds, 48u, &BuildTextFieldScene<Scene>, &StepTextField<Scene>};
}
} // namespace TextFieldPaintTests
