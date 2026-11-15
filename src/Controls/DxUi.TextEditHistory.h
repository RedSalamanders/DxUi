#pragma once
// Undo and redo history of TextField and the editable ComboBox (Specs/Core/Core_PerformanceAndResources.md, "Single-line text
// history"; plan item W9.3). An entry restores one undo step: the text, which the control keeps in its own entry type with the
// caret, selection and scroll state to restore.
//
// Text of an entry, in one of two forms (fields `text`, `position`, `replacedLength`, `sealed` of the control's entry):
// - open: `text` is the whole text the entry restores, shared with the text it was captured from (no copy). Only the newest
//   entry of a stack is ever open: an edit records an open entry before it changes the text, and a typing run keeps adding to
//   the text that entry restores until the run ends.
// - sealed: applying the entry replaces `replacedLength` units at `position` of the text it applies to with `text` (null when
//   nothing is put back). The next history operation seals an open entry against the text then current: the changed span is
//   the units between the common prefix and the common suffix of the two texts, so the restored text is exact, unit for unit,
//   whatever the edit was (a span edge may fall inside a surrogate pair or a grapheme cluster; the units are put back as they
//   were).
// Undo and redo apply the newest entry of one stack and push its inverse, sealed, onto the other. A sealed entry describes the
// text the entry above it produces, so a text change that records no history (a composition preview left in place, an item a
// ComboBox commits) first reopens the sealed top of each stack against the text before the change (NoteUnrecordedChange).
//
// Memory: entries are evicted oldest first, the undo side before the redo side, until the sealed entries, the stacks' storage
// and the open entries' whole texts fit kByteBudget; the entry an operation just added and an open entry are never evicted, so
// the newest step stays undoable however large it is (it alone may then exceed the budget, with every older step gone).
#include "DxUi.Internal.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace DxUi::TextEditHistory
{
// G13's text-history line: history per editor, undo and redo together.
inline constexpr uint64_t kByteBudget = 4ull * 1024ull * 1024ull;
// The shared_ptr control block MakeSharedText allocates beside each span (reference counts, the deleter and the pointer).
inline constexpr uint64_t kSharedTextBlockBytes = 3u * sizeof(void*);

[[nodiscard]] inline size_t SpanLength(const SharedText& text) noexcept
{
    return text ? text->size() : 0u;
}

// A sealed entry's span buffer on the heap: the control block, the string object and its characters unless they fit inside it.
[[nodiscard]] inline uint64_t SpanHeapBytes(const SharedText& text) noexcept
{
    if (! text)
        return 0u;
    const std::wstring& span = *text;
    const auto* object       = reinterpret_cast<const unsigned char*>(&span);
    const auto* data         = reinterpret_cast<const unsigned char*>(span.data());
    const bool inSitu        = data >= object && data < object + sizeof(std::wstring);
    return kSharedTextBlockBytes + sizeof(std::wstring) + (inSitu ? 0u : (uint64_t(span.capacity()) + 1u) * sizeof(wchar_t));
}

// A span of `units`, or null when it is empty. Wiped on release like every shared buffer. May throw std::bad_alloc.
[[nodiscard]] inline SharedText MakeSpan(std::wstring_view units)
{
    return units.empty() ? SharedText{} : MakeSharedText(std::wstring(units));
}

// The units `before` and `after` share at their start and, after that, at their end.
[[nodiscard]] inline std::pair<size_t, size_t> CommonEnds(std::wstring_view before, std::wstring_view after) noexcept
{
    if (before.data() == after.data() && before.size() == after.size())
        return {before.size(), 0u};
    const size_t limit = (std::min)(before.size(), after.size());
    size_t prefix      = 0u;
    while (prefix < limit && before[prefix] == after[prefix])
        ++prefix;
    size_t suffix = 0u;
    while (suffix < limit - prefix && before[before.size() - 1u - suffix] == after[after.size() - 1u - suffix])
        ++suffix;
    return {prefix, suffix};
}

// Seals an open entry against `current`, the text it will be applied to. May throw std::bad_alloc before changing the entry.
template <typename Entry> void Seal(Entry& entry, std::wstring_view current)
{
    const std::wstring_view restored = SharedTextView(entry.text);
    const auto [prefix, suffix]      = CommonEnds(current, restored);
    SharedText span                  = MakeSpan(restored.substr(prefix, restored.size() - prefix - suffix));
    entry.position                   = prefix;
    entry.replacedLength             = current.size() - prefix - suffix;
    entry.text                       = std::move(span);
    entry.sealed                     = true;
}

// Opens a sealed entry again: its whole restored text, from `base`, the text it applies to. False when the span does not fit
// `base` (the history no longer describes the text). May throw std::bad_alloc before changing the entry.
template <typename Entry> [[nodiscard]] bool Reopen(Entry& entry, std::wstring_view base)
{
    if (entry.position > base.size() || entry.replacedLength > base.size() - entry.position)
        return false;
    const std::wstring_view span = SharedTextView(entry.text);
    std::wstring restored;
    restored.reserve(base.size() - entry.replacedLength + span.size());
    restored.append(base.substr(0u, entry.position));
    restored.append(span);
    restored.append(base.substr(entry.position + entry.replacedLength));
    entry.text           = MakeSharedText(std::move(restored));
    entry.position       = 0u;
    entry.replacedLength = 0u;
    entry.sealed         = false;
    return true;
}

// The bytes the budget bounds: both stacks' element storage and every sealed entry's span.
template <typename Entry> [[nodiscard]] uint64_t BudgetedBytes(const std::vector<Entry>& undo, const std::vector<Entry>& redo) noexcept
{
    uint64_t bytes = uint64_t(undo.capacity() + redo.capacity()) * sizeof(Entry);
    for (const std::vector<Entry>* side : {&undo, &redo})
        for (const Entry& entry : *side)
            if (entry.sealed)
                bytes += SpanHeapBytes(entry.text);
    return bytes;
}

// Everything the history holds beyond `current` (the control's text): the budgeted bytes and each open entry's text buffer
// that the current text does not share. EmbeddedStatistics::largestTextHistoryBytes reports this. Allocates nothing.
template <typename Entry> [[nodiscard]] uint64_t Bytes(const std::vector<Entry>& undo, const std::vector<Entry>& redo, const std::wstring* current) noexcept
{
    uint64_t bytes = BudgetedBytes(undo, redo);
    for (const std::vector<Entry>* side : {&undo, &redo})
        for (const Entry& entry : *side)
            if (! entry.sealed && entry.text && entry.text.get() != current)
                bytes += (uint64_t(entry.text->capacity()) + 1u) * sizeof(wchar_t);
    return bytes;
}

// The whole texts the open entries keep, whether or not the current text still shares them: an open entry's text stops being
// shared as soon as its edit changes the text.
template <typename Entry> [[nodiscard]] uint64_t OpenTextBytes(const std::vector<Entry>& undo, const std::vector<Entry>& redo) noexcept
{
    uint64_t bytes = 0u;
    for (const std::vector<Entry>* side : {&undo, &redo})
        for (const Entry& entry : *side)
            if (! entry.sealed && entry.text)
                bytes += (uint64_t(entry.text->capacity()) + 1u) * sizeof(wchar_t);
    return bytes;
}

// Evicts the oldest entries, undo side first, until the budgeted bytes fit kByteBudget less the open entries' texts, so the
// reported bytes (Bytes) never exceed kByteBudget. The newest entry of `added` (the stack an operation just pushed onto) and an
// open entry are never evicted: an open text larger than the budget alone stays, as the newest step, and every older step goes.
template <typename Entry> void Enforce(std::vector<Entry>& undo, std::vector<Entry>& redo, const std::vector<Entry>* added) noexcept
{
    const uint64_t open  = OpenTextBytes(undo, redo);
    const uint64_t limit = open >= kByteBudget ? 0u : kByteBudget - open;
    uint64_t bytes       = BudgetedBytes(undo, redo);
    while (bytes > limit)
    {
        std::vector<Entry>* side = nullptr;
        for (std::vector<Entry>* candidate : {&undo, &redo})
        {
            if (! side && candidate->size() > (candidate == added ? 1u : 0u) && candidate->front().sealed)
                side = candidate;
        }
        if (! side)
            return;
        bytes -= SpanHeapBytes(side->front().text);
        side->erase(side->begin());
    }
}

// Records `captured` (open: the current text and the state to restore) as the newest undo step and clears redo. The previous
// open entry is sealed against `current` first. May throw std::bad_alloc.
template <typename Entry> void Record(std::vector<Entry>& undo, std::vector<Entry>& redo, std::wstring_view current, Entry captured, size_t maxEntries)
{
    if (! undo.empty() && ! undo.back().sealed)
        Seal(undo.back(), current);
    undo.push_back(std::move(captured));
    if (undo.size() > maxEntries)
        undo.erase(undo.begin());
    redo.clear();
    Enforce(undo, redo, &undo);
}

// Applies the newest entry of `from` to `text` and pushes its inverse onto `to`, with `inverse`'s state (the state current
// before the step). `applied` receives the entry, whose state the caller restores; `changed` whether the text changed. False
// when `from` is empty, or when its entry does not fit the text, which drops both stacks. May throw std::bad_alloc.
template <typename Entry>
[[nodiscard]] bool Step(std::vector<Entry>& from, std::vector<Entry>& to, SharedText& text, Entry inverse, size_t maxEntries, Entry& applied, bool& changed)
{
    if (from.empty())
        return false;
    if (! to.empty() && ! to.back().sealed)
        Seal(to.back(), SharedTextView(text));
    Entry& entry                    = from.back();
    const std::wstring_view current = SharedTextView(text);
    inverse.sealed                  = true;
    if (! entry.sealed)
    {
        const std::wstring_view restored = SharedTextView(entry.text);
        const auto [prefix, suffix]      = CommonEnds(current, restored);
        inverse.text                     = MakeSpan(current.substr(prefix, current.size() - prefix - suffix));
        inverse.position                 = prefix;
        inverse.replacedLength           = restored.size() - prefix - suffix;
        changed                          = inverse.replacedLength != 0u || inverse.text;
        text                             = std::move(entry.text);
    }
    else
    {
        if (entry.position > current.size() || entry.replacedLength > current.size() - entry.position)
        {
            from.clear();
            to.clear();
            return false;
        }
        const size_t inserted  = SpanLength(entry.text);
        inverse.text           = MakeSpan(current.substr(entry.position, entry.replacedLength));
        inverse.position       = entry.position;
        inverse.replacedLength = inserted;
        changed                = entry.replacedLength != 0u || inserted != 0u;
        if (changed)
        {
            // The inverse no longer holds the current buffer, so a buffer nothing else shares is edited in place.
            std::wstring& writable = MutableSharedText(text, current.size() - entry.replacedLength + inserted);
            writable.replace(entry.position, entry.replacedLength, SharedTextView(entry.text));
        }
    }
    applied = std::move(entry);
    from.pop_back();
    to.push_back(std::move(inverse));
    if (to.size() > maxEntries)
        to.erase(to.begin());
    return true;
}

template <typename Entry>
[[nodiscard]] bool Undo(std::vector<Entry>& undo, std::vector<Entry>& redo, SharedText& text, Entry current, size_t maxEntries, Entry& applied, bool& changed)
{
    if (! Step(undo, redo, text, std::move(current), maxEntries, applied, changed))
        return false;
    Enforce(undo, redo, &redo);
    return true;
}

template <typename Entry>
[[nodiscard]] bool Redo(std::vector<Entry>& undo, std::vector<Entry>& redo, SharedText& text, Entry current, size_t maxEntries, Entry& applied, bool& changed)
{
    if (! Step(redo, undo, text, std::move(current), maxEntries, applied, changed))
        return false;
    Enforce(undo, redo, &undo);
    return true;
}

// The text changed from `previous` without recording history: each stack's sealed newest entry, which describes `previous`,
// is opened against it so that it still restores its own text. Drops both stacks if an entry does not fit. May throw
// std::bad_alloc.
template <typename Entry> void NoteUnrecordedChange(std::vector<Entry>& undo, std::vector<Entry>& redo, std::wstring_view previous)
{
    for (std::vector<Entry>* side : {&undo, &redo})
    {
        if (! side->empty() && side->back().sealed && ! Reopen(side->back(), previous))
        {
            undo.clear();
            redo.clear();
            return;
        }
    }
    // A reopened text counts against the budget like any open entry's.
    Enforce(undo, redo, static_cast<const std::vector<Entry>*>(nullptr));
}
} // namespace DxUi::TextEditHistory
