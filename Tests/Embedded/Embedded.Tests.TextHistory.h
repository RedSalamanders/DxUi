#pragma once
// Text history by bytes (W9.3 of the performance and motion plan; Specs/Core/Core_PerformanceAndResources.md, "Single-line text
// history"). The gate on the FX7 and FX2 "history" variants: a 64 Ki-unit TextField and editable ComboBox after 128 edits keep
// at most 4 MiB of history (EmbeddedStatistics::largestTextHistoryBytes, the G13 text-history counter), and every undo and redo
// restores the text, caret and selection it must. Also: an IME commit through the TSF doubles is one undo step, and the
// history's byte budget, entry costs and eviction order on the history itself. Included by Embedded.Tests.Embedded.cpp after
// Embedded.Tests.PerformanceFixtures.h and Embedded.Tests.ImeCopies.h.
#include "Controls/DxUi.TextEditHistory.h"

#include <iostream>
#include <string>
#include <vector>

namespace TextHistoryTests
{
using PerformanceFixtures::TextHistoryEdits::Record;
using PerformanceFixtures::TextHistoryEdits::TextState;

inline constexpr uint64_t kBudgetBytes = 4ull * 1024ull * 1024ull;

struct GateResult
{
    uint64_t afterEdits = 0u;
    uint64_t peak       = 0u;
    size_t undos        = 0u;
};

// After the history phase: undo as far as the history goes, then redo all of it. Undo j restores the state edit (128 - j)
// started from (its place selected, its text before it); redo returns the state its undo started from. The history bytes are
// read after the edits and after every undo and redo.
template <typename Control>
[[nodiscard]] GateResult CheckUndoRedo(PerformanceFixtures::EmbeddedInstance& instance,
                                       Control& control,
                                       const std::vector<Record>& history,
                                       std::string_view fixture)
{
    using PerformanceFixtures::TextHistoryEdits::ReadState;
    const auto key = [&](UINT virtualKey)
    {
        const bool handled = instance.Key(virtualKey, true, MK_CONTROL);
        static_cast<void>(instance.Key(virtualKey, false, MK_CONTROL));
        return handled;
    };
    GateResult result;
    Check(history.size() == PerformanceFixtures::TextHistoryEdits::kEdits, "the history variant made its 128 edits");
    Check(ReadState(control) == history.back().after, "the control shows the last edit's state");
    result.afterEdits = instance.View().GetStatistics().largestTextHistoryBytes;
    result.peak       = result.afterEdits;
    std::vector<TextState> beforeUndo;
    while (true)
    {
        const TextState before = ReadState(control);
        if (! key('Z'))
            break;
        ++result.undos;
        Check(result.undos <= history.size(), "undo stops at the oldest kept edit");
        const Record& undone = history[history.size() - result.undos];
        Check(ReadState(control) == undone.before, "undo restores the text, caret and selection the edit started from");
        result.peak = (std::max)(result.peak, instance.View().GetStatistics().largestTextHistoryBytes);
        beforeUndo.push_back(before);
    }
    Check(ReadState(control) == history[history.size() - result.undos].before, "a refused undo changes nothing");
    for (size_t index = beforeUndo.size(); index > 0u; --index)
    {
        Check(key('Y'), "redo replays every undone edit");
        Check(ReadState(control) == beforeUndo[index - 1u], "redo restores the text, caret and selection its undo started from");
        result.peak = (std::max)(result.peak, instance.View().GetStatistics().largestTextHistoryBytes);
    }
    Check(! key('Y') && ReadState(control) == history.back().after, "after every redo the control shows the last edit's state again");
    std::cout << "TEXT_HISTORY_GATE fixture=" << fixture << " editsBytes=" << result.afterEdits << " peakBytes=" << result.peak << " undos=" << result.undos
              << '\n';
    return result;
}

template <typename Scene, typename Control>
void RunGate(const PerformanceFixtures::Definition& definition, Control& (Scene::*control)() const, std::string_view fixture)
{
    const PerformanceFixtures::Variant* variant = definition.FindVariant("history");
    Check(variant && variant->receiptOnly, "the fixture declares its receipt-only history variant");
    PerformanceFixtures::RunOptions options;
    GateResult result;
    options.afterPhases = [&](PerformanceFixtures::EmbeddedInstance& instance, GraphicsFixture&)
    {
        auto& scene = dynamic_cast<Scene&>(instance.GetScenario());
        result      = CheckUndoRedo(instance, (scene.*control)(), scene.History(), fixture);
    };
    const PerformanceFixtures::RunRecord run         = PerformanceFixtures::RunEmbedded(definition, *variant, options);
    const auto results                               = PerformanceFixtures::EvaluateG13(run);
    const PerformanceFixtures::BudgetResult& history = results[4];
    Check(history.line->id == "text-history" && history.value.has_value(), "the run counts the G13 text-history line");
    std::cout << "TEXT_HISTORY_GATE fixture=" << fixture << " g13TextHistoryBytes=" << history.value.value_or(0u) << '\n';
    // The count cap keeps the 64 newest of the 128 edits.
    Check(result.undos == 64u, "the 64 newest edits are undoable: only the entry count, not the byte budget, limits this history");
    Check(result.afterEdits <= kBudgetBytes && result.peak <= kBudgetBytes, "the 64 Ki-unit editor keeps at most 4 MiB of history after 128 edits");
    Check(history.verdict == PerformanceFixtures::BudgetVerdict::Met && ! history.exception, "the G13 text-history line is met");
}

// The gate: FX7's TextField and FX2's editable ComboBox, 64 Ki units and 128 edits each.
__declspec(noinline) inline void TestFx7AndFx2TextHistoryGate()
{
    RunGate<PerformanceFixtures::Fx7::Scene, DxUi::TextField>(PerformanceFixtures::Fx7::kDefinition, &PerformanceFixtures::Fx7::Scene::Field, "FX7");
    RunGate<PerformanceFixtures::Fx2::Scene, DxUi::ComboBox>(PerformanceFixtures::Fx2::kDefinition, &PerformanceFixtures::Fx2::Scene::Combo, "FX2");
}

// One IME commit is one undo step, driven through the production TSF store and the in-process doubles (no IME, no focus).
template <typename Control> void CheckImeCommitIsOneUndoStep(Control& control, ImeCopies::HeadlessHost& fixture, std::string_view name)
{
    const auto select = [&](size_t anchor, size_t caret)
    {
        if constexpr (std::is_same_v<Control, DxUi::ComboBox>)
            control.SetEditableSelectionRange(anchor, caret);
        else
            control.SetSelectionRange(anchor, caret);
    };
    fixture.host.SetFocusControl(&control, false);
    select(5u, 5u);
    Check(control.OnChar(fixture.host, L'X', 0u) && control.GetText() == L"base Xof the field", "an edit before the composition is recorded");
    select(9u, 9u);
    static_cast<void>(ImeCopies::ComposeTenSyllables(fixture.host, control, 9u));
    const std::wstring committed = ImeCopies::ExpectedText(L"base Xof the field", 9u);
    Check(control.GetText() == committed, "the composition commits its ten syllables");
    Check(control.OnKeyDown(fixture.host, 'Z', MK_CONTROL) && control.GetText() == L"base Xof the field",
          "one undo removes the whole composition: an IME commit is one undo step");
    Check(control.OnKeyDown(fixture.host, 'Z', MK_CONTROL) && control.GetText() == L"base of the field", "the next undo removes the edit before it");
    Check(control.OnKeyDown(fixture.host, 'Y', MK_CONTROL) && control.OnKeyDown(fixture.host, 'Y', MK_CONTROL) && control.GetText() == committed,
          "two redos restore the edit and the committed composition");
    std::cout << "IME commit is one undo step: " << name << '\n';
}

__declspec(noinline) inline void TestImeCommitIsOneUndoStep()
{
    {
        ImeCopies::HeadlessHost fixture;
        auto* const field = fixture.root->AddChild<DxUi::TextField>(L"base of the field");
        field->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 32.0f));
        CheckImeCommitIsOneUndoStep(*field, fixture, "TextField");
    }
    {
        ImeCopies::HeadlessHost fixture;
        auto* const combo = fixture.root->AddChild<DxUi::ComboBox>();
        combo->SetEditable(true);
        combo->SetAutoOpenOnTextInput(false);
        combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 32.0f));
        combo->SetText(L"base of the field");
        CheckImeCommitIsOneUndoStep(*combo, fixture, "ComboBox");
    }
}

// ---- The history itself (Controls/DxUi.TextEditHistory.h), on an entry with no view state ----------------------------------

struct Entry
{
    DxUi::SharedText text;
    size_t position       = 0u;
    size_t replacedLength = 0u;
    bool sealed           = false;
    size_t caret          = 0u;
};

class History
{
public:
    explicit History(std::wstring text) : _text(DxUi::MakeSharedText(std::move(text)))
    {
    }
    // One recorded edit: [start, end) of the text replaced by `with`.
    void Edit(size_t start, size_t end, std::wstring_view with)
    {
        DxUi::TextEditHistory::Record(_undo, _redo, *_text, Entry{_text, 0u, 0u, false, start}, kMaxEntries);
        std::wstring next = *_text;
        next.replace(start, end - start, with);
        _text = DxUi::MakeSharedText(std::move(next));
    }
    [[nodiscard]] bool Undo()
    {
        Entry applied;
        bool changed = false;
        return DxUi::TextEditHistory::Undo(_undo, _redo, _text, Entry{_text, 0u, 0u, false, 0u}, kMaxEntries, applied, changed);
    }
    [[nodiscard]] bool Redo()
    {
        Entry applied;
        bool changed = false;
        return DxUi::TextEditHistory::Redo(_undo, _redo, _text, Entry{_text, 0u, 0u, false, 0u}, kMaxEntries, applied, changed);
    }
    [[nodiscard]] uint64_t Bytes() const noexcept
    {
        return DxUi::TextEditHistory::Bytes(_undo, _redo, _text.get());
    }
    [[nodiscard]] uint64_t BudgetedBytes() const noexcept
    {
        return DxUi::TextEditHistory::BudgetedBytes(_undo, _redo);
    }
    [[nodiscard]] const std::wstring& Text() const noexcept
    {
        return *_text;
    }
    [[nodiscard]] size_t UndoCount() const noexcept
    {
        return _undo.size();
    }
    [[nodiscard]] size_t RedoCount() const noexcept
    {
        return _redo.size();
    }
    [[nodiscard]] const Entry& UndoTop() const noexcept
    {
        return _undo.back();
    }
    [[nodiscard]] const Entry& RedoTop() const noexcept
    {
        return _redo.back();
    }
    // Storage of both stacks, which every entry counts against the budget.
    [[nodiscard]] uint64_t SlotBytes() const noexcept
    {
        return uint64_t(_undo.capacity() + _redo.capacity()) * sizeof(Entry);
    }

    static constexpr size_t kMaxEntries = 64u;

private:
    DxUi::SharedText _text;
    std::vector<Entry> _undo;
    std::vector<Entry> _redo;
};

[[nodiscard]] inline std::wstring Words(size_t units, wchar_t first)
{
    std::wstring text(units, first);
    for (size_t index = 0u; index < units; ++index)
        text[index] = (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(first + (index % 20u));
    return text;
}

// What an entry costs once sealed, on a 64 Ki-unit text: a typed character, a 64 Ki-unit paste, a replace-all, and the
// inverse an undo pushes.
__declspec(noinline) inline void TestTextHistoryEntryCosts()
{
    constexpr size_t kUnits = 65'536u;
    History history(Words(kUnits, L'a'));
    history.Edit(100u, 100u, L"k"); // open: the entry still shares the text before the edit
    const uint64_t open = history.Bytes();
    Check(! history.UndoTop().sealed && open >= kUnits * sizeof(wchar_t), "an open entry keeps the text before its edit, outside the budget");
    Check(history.BudgetedBytes() == history.SlotBytes(), "an open entry counts against the budget by its slot only");
    history.Edit(200u, 200u, L"k"); // seals the first: a typed character is an insertion, which keeps no text
    Check(history.BudgetedBytes() == history.SlotBytes(), "a sealed typed character keeps no text: its slot is its cost");
    const uint64_t typed = sizeof(Entry);

    const std::wstring paste = Words(kUnits, L'A');
    history.Edit(300u, 300u, paste); // a 64 Ki-unit paste: an insertion, which keeps no text either
    history.Edit(10u, 10u, L"k");
    Check(history.BudgetedBytes() == history.SlotBytes(), "a sealed paste over no selection keeps no text");
    Check(history.Undo() && history.Undo(), "undo the edit after the paste and the paste");
    const uint64_t pasteRedo = DxUi::TextEditHistory::SpanHeapBytes(history.RedoTop().text);
    Check(history.RedoTop().sealed && history.RedoTop().text && history.RedoTop().text->size() == kUnits && pasteRedo >= kUnits * sizeof(wchar_t),
          "undoing the paste pushes a redo entry holding the pasted text");
    Check(history.Redo() && history.Redo(), "redo the paste and the edit after it");

    const std::wstring before = history.Text();
    history.Edit(0u, before.size(), L"Z"); // replace-all
    history.Edit(1u, 1u, L"k");
    Check(history.Undo(), "undo the edit after the replace-all");
    const Entry& replaceAll = history.UndoTop();
    Check(replaceAll.sealed && replaceAll.text && *replaceAll.text == before && replaceAll.position == 0u && replaceAll.replacedLength == 1u,
          "a replace-all keeps the replaced text once, as the span that one character replaced");
    const uint64_t replaceAllBytes = DxUi::TextEditHistory::SpanHeapBytes(replaceAll.text);
    Check(history.Undo() && history.Text() == before, "undoing the replace-all restores the text");
    std::cout << "TEXT_HISTORY_COSTS entryBytes=" << typed << " openEntryBytes=" << open << " pasteRedoBytes=" << pasteRedo
              << " replaceAllBytes=" << replaceAllBytes << '\n';
}

// The budget: edits whose spans exceed 4 MiB evict the oldest first, undo before redo; the newest entry is never evicted, so a
// single edit larger than the budget is still applied and stays undoable while it is the newest step.
__declspec(noinline) inline void TestTextHistoryBudgetEvictsOldestFirst()
{
    constexpr size_t kUnits   = 65'536u;
    const std::wstring first  = Words(kUnits, L'a');
    const std::wstring second = Words(kUnits, L'A');
    History history(first);
    for (size_t edit = 0u; edit < 48u; ++edit)
    {
        const std::wstring& with = edit % 2u == 0u ? second : first;
        history.Edit(0u, history.Text().size(), with);
        Check(history.Bytes() <= kBudgetBytes, "the reported history, open text included, never exceeds 4 MiB");
    }
    history.Edit(0u, 0u, L"k"); // seals the last replace-all
    Check(history.Bytes() <= kBudgetBytes && history.BudgetedBytes() <= kBudgetBytes,
          "48 whole-text replacements of a 64 Ki-unit text fit the 4 MiB budget with the open step's text");
    const size_t kept = history.UndoCount();
    Check(kept < 49u && kept >= 28u, "the oldest replacements were evicted to fit the budget");
    // The newest steps undo in order and restore the text each replaced (replacement 47, the last, put `first` back).
    Check(history.Undo() && history.Text() == first, "undo removes the last edit");
    for (size_t index = 1u; index < kept; ++index)
    {
        Check(history.Undo(), "every kept step undoes");
        Check(history.Text() == (index % 2u == 1u ? second : first), "each undo restores the text that replacement replaced");
        Check(history.Bytes() <= kBudgetBytes, "undo keeps the budget");
    }
    Check(! history.Undo(), "the evicted steps are gone");
    // Redo side: every undone step is a redo entry holding the text its undo put back; they fit, oldest evicted first.
    Check(history.RedoCount() == kept && history.BudgetedBytes() <= kBudgetBytes, "the undone steps are redo entries within the budget");
    for (size_t index = 0u; index < kept; ++index)
        Check(history.Redo(), "every undone step redoes");
    Check(history.Text().size() == kUnits + 1u && history.Text().front() == L'k', "redo returns to the last edit");

    // An edit larger than the budget: applied, undoable while it is the newest step, then evicted with everything older.
    constexpr size_t kLarge = 3u * 1024u * 1024u; // 6 MiB of text
    History large(Words(kLarge, L'a'));
    large.Edit(0u, 0u, L"k");
    large.Edit(0u, kLarge + 1u, L"Z"); // replaces 6 MiB
    large.Edit(1u, 1u, L"y");          // seals it: larger than the budget, it is evicted, oldest first
    Check(large.UndoCount() == 1u && large.BudgetedBytes() <= kBudgetBytes, "a sealed entry larger than the budget is evicted once it is not the newest");
    Check(large.Undo() && large.Text() == L"Z" && ! large.Undo(), "the newer step undoes; the oversized one is gone");
    History oversized(Words(kLarge, L'a'));
    oversized.Edit(0u, kLarge, L"Z");
    Check(oversized.Undo() && oversized.Text().size() == kLarge, "the newest step undoes however large it is");
    Check(oversized.Redo() && oversized.Text() == L"Z" && oversized.UndoCount() == 1u, "redo keeps the oversized entry it pushes: it is the newest");
    Check(oversized.Undo() && oversized.Text().size() == kLarge, "and it undoes again");

    // An open text alone larger than the budget: each edit of a 6 MiB text records it whole, so that step stays, as the
    // newest, and every older step goes, however small.
    History open(Words(kLarge, L'a'));
    open.Edit(0u, 0u, L"a");
    open.Edit(5u, 5u, L"b");
    const std::wstring beforeLast = open.Text();
    open.Edit(9u, 9u, L"c");
    Check(open.UndoCount() == 1u && ! open.UndoTop().sealed, "an open text over the budget is the only step kept");
    Check(open.BudgetedBytes() == open.SlotBytes(), "every older sealed step was dropped");
    Check(open.Undo() && open.Text() == beforeLast && ! open.Undo(), "the newest step undoes; the older ones are gone");
    Check(open.Redo() && open.Text().size() == kLarge + 3u, "and redoes");
    std::cout << "TEXT_HISTORY_BUDGET keptReplaceAlls=" << kept << " of 48, budgetedBytes=" << history.BudgetedBytes() << '\n';
}
} // namespace TextHistoryTests
