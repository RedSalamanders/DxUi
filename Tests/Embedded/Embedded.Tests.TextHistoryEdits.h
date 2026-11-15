#pragma once
// The W9.3 text-history edits of the FX7 and FX2 "history" variants: 128 edits of a 64 Ki-unit TextField or editable ComboBox,
// each one undo step at its own place, so the count cap (64 entries) holds the history and a full-text snapshot history would
// keep 64 copies of the text. Step k selects the place of edit k with the control's selection API, then makes the edit through
// the host's key and character input; the scene records the text state before and after each edit, which the W9.3 gate
// (Embedded.Tests.TextHistory.h) compares with every undo and redo. A scene includes this after Embedded.Tests.PerformanceFixture.h.
#include "Controls/DxUi.Internal.h"

#include <DxUi/Failure.h>
#include <optional>
#include <string_view>
#include <vector>

namespace PerformanceFixtures::TextHistoryEdits
{
inline constexpr size_t kEdits = 128u;
// The 64 units edit 6 (kind 6) puts in place of 64 selected units; a TextField replaces them programmatically, a ComboBox by typing.
inline constexpr std::wstring_view kReplacement = L"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-+";
static_assert(kReplacement.size() == 64u);

// The observable editing state: a digest of the text, its length, the caret and the selection anchor.
struct TextState
{
    uint64_t digest = 0u;
    size_t units    = 0u;
    size_t caret    = 0u;
    std::optional<size_t> anchor;
    bool operator==(const TextState&) const = default;
};

[[nodiscard]] inline TextState ReadState(const DxUi::Control& control)
{
    DxUi::SharedTextInputState state;
    if (! DxUi::SharedTextInputAccess::Export(control, state) || ! state.text)
        DxUi::Failure::FailContract(E_UNEXPECTED);
    uint64_t hash = 1469598103934665603ull;
    for (const wchar_t unit : state.View())
    {
        hash = (hash ^ (static_cast<uint32_t>(unit) & 0xFFu)) * 1099511628211ull;
        hash = (hash ^ (static_cast<uint32_t>(unit) >> 8u)) * 1099511628211ull;
    }
    return TextState{hash, state.text->size(), state.caretIndex, state.selectionAnchorIndex};
}

// Where edit k happens: a spread of places over the whole text.
[[nodiscard]] constexpr size_t PlaceOf(size_t edit, size_t units) noexcept
{
    return static_cast<size_t>((static_cast<uint64_t>(edit) * 2654435761ull + 977u) % (static_cast<uint64_t>(units) + 1u));
}

struct Record
{
    TextState before; // after the edit's place is selected: what undoing the edit restores
    TextState after;
};

// Edit k of 128 on `control` (a focused TextField or editable ComboBox): kinds 0-7 in turn, and the last edit replaces all text.
template <typename Control> void Apply(Context& context, Control& control, size_t edit, std::vector<Record>& records)
{
    const auto key = [&](UINT virtualKey, UINT modifiers = 0u)
    {
        static_cast<void>(context.port.Key(virtualKey, true, modifiers));
        static_cast<void>(context.port.Key(virtualKey, false, modifiers));
    };
    const auto select = [&](size_t anchor, size_t caret)
    {
        if constexpr (std::is_same_v<Control, DxUi::ComboBox>)
            control.SetEditableSelectionRange(anchor, caret);
        else
            control.SetSelectionRange(anchor, caret);
    };
    const size_t units = control.GetText().size();
    const size_t at    = PlaceOf(edit, units);
    const size_t kind  = edit + 1u == kEdits ? 8u : edit % 8u;
    switch (kind)
    {
        case 0u: select(at, (std::min)(units, at + 1000u)); break; // a 1,000-unit selection replaced by one character
        case 6u: select(at, (std::min)(units, at + 64u)); break;
        case 8u: key('A', MK_CONTROL); break;
        default: select(at, at); break;
    }
    Record record{ReadState(control), {}};
    switch (kind)
    {
        case 0u: static_cast<void>(context.port.Character(L'X')); break;
        case 1u: key(VK_BACK); break;
        case 2u: key(VK_DELETE); break;
        case 3u: static_cast<void>(context.port.Character(L'k')); break;
        case 4u: key(VK_BACK, MK_CONTROL); break;
        case 5u: key(VK_DELETE, MK_CONTROL); break;
        case 6u:
            if constexpr (std::is_same_v<Control, DxUi::ComboBox>)
                static_cast<void>(context.port.Character(L'r'));
            else
                control.ReplaceSelectionAndNotify(kReplacement);
            break;
        case 7u:
            static_cast<void>(context.port.Character(L'\xD83D'));
            static_cast<void>(context.port.Character(L'\xDE00'));
            break;
        default: static_cast<void>(context.port.Character(L'Z')); break; // replaces the whole text
    }
    record.after = ReadState(control);
    records.push_back(record);
}
} // namespace PerformanceFixtures::TextHistoryEdits
