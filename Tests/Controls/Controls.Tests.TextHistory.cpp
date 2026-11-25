#include "Controls.Tests.TextHistory.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.TextHistoryExpected.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// The script and its pinned sequence (W9.3). The expected sequence was recorded on the full-text snapshot history of the
// plan's base commit: with DXUI_TEXT_HISTORY_SCRIPT_OUT set to a file path, the test writes the sequence it observes there as
// Controls.Tests.TextHistoryExpected.h instead of comparing, which is how that header was made. Every observation is one
// 64-bit FNV-1a digest of the text, the caret, the selection anchor and whether the control handled the operation.
namespace
{
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// The import an IME commit and a TSF preview reach, as the native and embedded hosts call it.
class ScriptComboBox final : public DxUi::ComboBox
{
public:
    using DxUi::ComboBox::ExportTextInputState;
    using DxUi::ComboBox::ImportTextInputState;
};
#pragma warning(pop)

// A fixed linear congruential sequence: the script depends on nothing but its seed and the control's own state.
struct ScriptRandom
{
    uint64_t state = 0x9E3779B97F4A7C15ull;
    uint32_t Next() noexcept
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<uint32_t>(state >> 33u);
    }
    size_t Below(size_t bound) noexcept
    {
        return bound == 0u ? 0u : static_cast<size_t>(Next()) % bound;
    }
};

struct ScriptState
{
    std::wstring text;
    size_t caret = 0u;
    std::optional<size_t> anchor;
    bool operator==(const ScriptState&) const = default;
};

[[nodiscard]] uint64_t Digest(const ScriptState& state, bool handled) noexcept
{
    uint64_t hash        = 1469598103934665603ull;
    const auto mixByte   = [&hash](uint8_t value) noexcept { hash = (hash ^ value) * 1099511628211ull; };
    const auto mixNumber = [&](uint64_t value) noexcept
    {
        for (int shift = 0; shift < 64; shift += 8)
            mixByte(static_cast<uint8_t>(value >> shift));
    };
    mixNumber(state.text.size());
    for (const wchar_t unit : state.text)
    {
        mixByte(static_cast<uint8_t>(unit & 0xFFu));
        mixByte(static_cast<uint8_t>((static_cast<uint32_t>(unit) >> 8u) & 0xFFu));
    }
    mixNumber(state.caret);
    mixNumber(state.anchor ? *state.anchor : ~uint64_t{0});
    mixByte(handled ? 1u : 0u);
    return hash;
}

constexpr UINT kCtrl  = MK_CONTROL;
constexpr UINT kShift = MK_SHIFT;

// One control under the script: TextField and the editable ComboBox through the same operations.
template <typename ControlType> class ScriptTarget
{
public:
    explicit ScriptTarget(DxUi::WindowHost& host) : _host(host)
    {
        _control.SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 28.0f));
        if constexpr (std::is_same_v<ControlType, ScriptComboBox>)
        {
            _control.SetEditable(true);
            _control.SetAutoOpenOnTextInput(false);
            _control.SetItems({{L"alpha", L"alpha"}, {L"alphabet", L"alphabet"}, {L"beta", L"beta"}, {L"gamma \U0001F600", L"gamma \U0001F600"}});
        }
    }
    ScriptTarget(const ScriptTarget&)            = delete;
    ScriptTarget& operator=(const ScriptTarget&) = delete;
    ScriptTarget(ScriptTarget&&)                 = delete;
    ScriptTarget& operator=(ScriptTarget&&)      = delete;
    ~ScriptTarget()                              = default;

    [[nodiscard]] ScriptState State()
    {
        DxUi::TextInputState exported;
        Require(_control.ExportTextInputState(exported), "the script control exports its text input state");
        return ScriptState{exported.text, exported.caretIndex, exported.selectionAnchorIndex};
    }
    void SetText(std::wstring text)
    {
        _control.SetText(std::move(text));
    }
    bool Type(wchar_t character)
    {
        return _control.OnChar(_host, character, 0u);
    }
    bool Key(UINT virtualKey, UINT modifiers)
    {
        return _control.OnKeyDown(_host, virtualKey, modifiers);
    }
    void Select(size_t anchor, size_t caret)
    {
        if constexpr (std::is_same_v<ControlType, ScriptComboBox>)
            _control.SetEditableSelectionRange(anchor, caret);
        else
            _control.SetSelectionRange(anchor, caret);
    }
    bool Paste(std::wstring_view text)
    {
        Require(_host.CopyTextToClipboard(text), "the script clipboard accepts the paste text");
        return Key('V', kCtrl);
    }
    // TextField: the programmatic replacement an application makes; the ComboBox has none and pastes instead.
    bool Replace(std::wstring_view text)
    {
        if constexpr (std::is_same_v<ControlType, ScriptComboBox>)
            return Paste(text);
        else
        {
            _control.ReplaceSelectionAndNotify(text);
            return true;
        }
    }
    bool Import(const ScriptState& state, bool notify)
    {
        DxUi::TextInputState imported;
        Require(_control.ExportTextInputState(imported), "the script control exports before an import");
        imported.text                 = state.text;
        imported.caretIndex           = state.caret;
        imported.selectionAnchorIndex = state.anchor;
        return _control.ImportTextInputState(_host, imported, notify);
    }
    // An IME composition as a host commits it: previews that create no history, the composition-entry text restored, then the
    // committed text as one notifying import.
    bool ComposeAndCommit(std::wstring_view composition)
    {
        const ScriptState base = State();
        size_t start           = base.caret;
        size_t end             = base.caret;
        if (base.anchor)
        {
            start = (std::min)(*base.anchor, base.caret);
            end   = (std::max)(*base.anchor, base.caret);
        }
        for (size_t length = 1u; length <= composition.size(); ++length)
        {
            ScriptState preview = base;
            preview.text.replace(start, end - start, composition.substr(0u, length));
            preview.caret = start + length;
            preview.anchor.reset();
            Require(Import(preview, false), "an IME preview imports without history");
        }
        Require(Import(base, false), "the composition-entry text is restored before the commit");
        ScriptState committed = base;
        committed.text.replace(start, end - start, composition);
        committed.caret = start + composition.size();
        committed.anchor.reset();
        return Import(committed, true);
    }
    // A preview a composition left in place (focus left, restore refused): a silent text change that records no history.
    bool LeavePreview(std::wstring_view preview)
    {
        ScriptState state = State();
        const size_t at   = state.caret;
        state.text.insert(at, preview);
        state.caret = at + preview.size();
        state.anchor.reset();
        return Import(state, false);
    }

private:
    DxUi::WindowHost& _host;
    ControlType _control;
};

struct ScriptRecord
{
    std::vector<uint64_t> digests;
    std::vector<std::string> operations;
};

template <typename ControlType> ScriptRecord RunScript(uint64_t seed, size_t operations)
{
    ClipboardHostWindow window;
    ScriptTarget<ControlType> target(window.Host());
    ScriptRandom random{seed};
    ScriptRecord record;
    const auto observe = [&](std::string name, bool handled)
    {
        record.digests.push_back(Digest(target.State(), handled));
        record.operations.push_back(std::move(name));
    };
    target.SetText(L"Hello w\u00f6rld e\u0301t\u00e9 \U0001F600 \u6771\u4eac \U0001F469\u200D\U0001F4BB end.");
    observe("set-text", true);

    static constexpr std::array<std::wstring_view, 9> kTyped{
        L"a", L"b", L" ", L"\u00e9", L"\u0301", L"\U0001F600", L"\u6771", L".", L"\U0001F469\u200D\U0001F4BB"};
    const std::wstring longPaste = [&]()
    {
        std::wstring text;
        for (size_t index = 0u; text.size() < 300u; ++index)
            text += index % 5u == 4u ? std::wstring(L"\U0001F680 ") : std::wstring(L"word") + std::to_wstring(index) + L' ';
        return text;
    }();
    const std::array<std::wstring_view, 5> pastes{L"pasted", L"line\r\nbreak", std::wstring_view(longPaste), L"\U0001F680x", L"\u05e9\u05dc\u05d5\u05dd"};
    static constexpr std::array<std::wstring_view, 3> kCompositions{L"\ud55c\uad6d", L"\u65e5\u672c\u8a9e", L"x"};

    for (size_t step = 0u; step < operations; ++step)
    {
        const size_t size = target.State().text.size();
        const size_t roll = random.Below(200u);
        if (roll < 50u)
        {
            const std::wstring_view typed = kTyped[random.Below(kTyped.size())];
            bool handled                  = true;
            for (const wchar_t unit : typed)
                handled = target.Type(unit) && handled;
            observe("type", handled);
        }
        else if (roll < 64u)
            observe("backspace", target.Key(VK_BACK, 0u));
        else if (roll < 76u)
            observe("delete", target.Key(VK_DELETE, 0u));
        else if (roll < 82u)
            observe("ctrl-backspace", target.Key(VK_BACK, kCtrl));
        else if (roll < 86u)
            observe("ctrl-delete", target.Key(VK_DELETE, kCtrl));
        else if (roll < 98u)
        {
            const size_t at = random.Below(size + 1u);
            target.Select(at, at);
            observe("move", true);
        }
        else if (roll < 104u)
        {
            static constexpr std::array<UINT, 4> kKeys{VK_LEFT, VK_RIGHT, VK_HOME, VK_END};
            const UINT key       = kKeys[random.Below(kKeys.size())];
            const UINT modifiers = (random.Below(2u) ? kShift : 0u) | (random.Below(3u) == 0u ? kCtrl : 0u);
            observe("arrow", target.Key(key, modifiers));
        }
        else if (roll < 114u)
        {
            const size_t anchor = random.Below(size + 1u);
            const size_t caret  = random.Below(size + 1u);
            target.Select(anchor, caret);
            observe("select", true);
        }
        else if (roll < 117u)
            observe("select-all", target.Key('A', kCtrl));
        else if (roll < 125u)
            observe("paste", target.Paste(pastes[random.Below(pastes.size())]));
        else if (roll < 129u)
            observe("cut", target.Key('X', kCtrl));
        else if (roll < 131u)
            observe("shift-delete", target.Key(VK_DELETE, kShift));
        else if (roll < 133u)
        {
            Require(window.Host().CopyTextToClipboard(L"ins"), "the script clipboard accepts the Shift+Insert text");
            observe("shift-insert", target.Key(VK_INSERT, kShift));
        }
        else if (roll < 138u)
            observe("replace", target.Replace(pastes[random.Below(pastes.size())]));
        else if (roll < 144u)
            observe("ime-commit", target.ComposeAndCommit(kCompositions[random.Below(kCompositions.size())]));
        else if (roll < 147u)
            observe("leave-preview", target.LeavePreview(L"~"));
        else if (roll < 149u)
        {
            // An editable ComboBox commits an item with Page Down while closed: the text changes without history.
            if constexpr (std::is_same_v<ControlType, ScriptComboBox>)
                observe("page-down", target.Key(VK_NEXT, 0u));
            else
                observe("leave-preview", target.LeavePreview(L"+"));
        }
        else if (roll < 176u)
            observe("undo", target.Key('Z', kCtrl));
        else if (roll < 198u)
            observe("redo", target.Key('Y', kCtrl));
        else if (random.Below(16u) == 0u)
        {
            target.SetText(L"Reset \U0001F600 text");
            observe("set-text", true);
        }
        else
            observe("undo", target.Key('Z', kCtrl));
    }

    // Undo as far as the history goes, then redo all of it: every redo returns the state the matching undo started from.
    std::vector<ScriptState> beforeUndo;
    for (size_t guard = 0u; guard < 200u; ++guard)
    {
        ScriptState before = target.State();
        const bool undone  = target.Key('Z', kCtrl);
        observe("final-undo", undone);
        if (! undone)
            break;
        beforeUndo.push_back(std::move(before));
    }
    for (size_t index = beforeUndo.size(); index > 0u; --index)
    {
        const bool redone = target.Key('Y', kCtrl);
        observe("final-redo", redone);
        Require(redone && target.State() == beforeUndo[index - 1u], "redo restores the text, caret and selection the matching undo started from");
    }
    observe("final-redo", target.Key('Y', kCtrl));
    return record;
}

void WriteExpected(std::ostream& output, std::string_view name, const ScriptRecord& record)
{
    output << "inline constexpr std::array<uint64_t, " << record.digests.size() << "u> " << name << "{";
    for (size_t index = 0u; index < record.digests.size(); ++index)
    {
        output << (index % 4u == 0u ? "\n    " : " ") << "0x" << std::hex << std::setw(16) << std::setfill('0') << record.digests[index] << std::dec << "ull,";
    }
    output << "\n};\n";
}

constexpr uint64_t kTextFieldSeed      = 0x5733u;
constexpr uint64_t kComboBoxSeed       = 0xC0B0u;
constexpr size_t kScriptOperations     = 700u;
constexpr const char* kScriptOutputEnv = "DXUI_TEXT_HISTORY_SCRIPT_OUT";

// Writes the header when the generation variable names a file (both controls, so it runs from the TextField test only).
[[nodiscard]] bool GenerateExpected()
{
    char* path        = nullptr;
    size_t pathLength = 0u;
    if (_dupenv_s(&path, &pathLength, kScriptOutputEnv) != 0 || ! path)
        return false;
    const std::string target(path);
    std::free(path);
    const ScriptRecord field = RunScript<ExposedTextField>(kTextFieldSeed, kScriptOperations);
    const ScriptRecord combo = RunScript<ScriptComboBox>(kComboBoxSeed, kScriptOperations);
    std::ofstream output(target, std::ios::binary);
    output << "#pragma once\n"
              "// Generated by Controls.Tests.TextHistory.cpp with DXUI_TEXT_HISTORY_SCRIPT_OUT on the full-text snapshot history of the\n"
              "// performance and motion plan's base commit 714e1f77e (W9.3). Do not edit: one digest per scripted operation, undo and redo.\n"
              "#include <array>\n#include <cstdint>\n\n";
    WriteExpected(output, "kTextFieldHistoryScript", field);
    output << '\n';
    WriteExpected(output, "kComboBoxHistoryScript", combo);
    Require(output.good(), "the expected text history sequence is written");
    std::cout << "TEXT_HISTORY_SCRIPT written " << target << ": " << field.digests.size() << " TextField and " << combo.digests.size()
              << " ComboBox observations\n";
    return true;
}

void Compare(const ScriptRecord& record, std::span<const uint64_t> expected, std::string_view control)
{
    Require(! expected.empty(), "the expected text history sequence is pinned");
    for (size_t index = 0u; index < (std::min)(record.digests.size(), expected.size()); ++index)
    {
        if (record.digests[index] != expected[index])
        {
            std::cerr << control << " text history differs from the snapshot history at observation " << index << " (" << record.operations[index] << ")\n";
            Require(false, "the text history matches the snapshot history at every observation");
        }
    }
    Require(record.digests.size() == expected.size(), "the text history script makes as many observations as the snapshot history");
}
} // namespace

void TestTextFieldHistoryMatchesSnapshotHistory()
{
    if (GenerateExpected())
        return;
    const ScriptRecord record = RunScript<ExposedTextField>(kTextFieldSeed, kScriptOperations);
    Compare(record, kTextFieldHistoryScript, "TextField");
    std::cout << "TextField history script: " << record.digests.size() << " observations match the snapshot history\n";
}

void TestComboBoxHistoryMatchesSnapshotHistory()
{
    const ScriptRecord record = RunScript<ScriptComboBox>(kComboBoxSeed, kScriptOperations);
    Compare(record, kComboBoxHistoryScript, "ComboBox");
    std::cout << "ComboBox history script: " << record.digests.size() << " observations match the snapshot history\n";
}
