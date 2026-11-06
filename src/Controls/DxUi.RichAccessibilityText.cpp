#include <DxUi/Failure.h>
#include <DxUi/RichAccessibilityText.h>
#include <DxUi/RichDocument.h>
#include <iterator>
#include <stdexcept>

namespace DxUi::RichText
{
namespace
{
using Id                               = AccessibilityTextId;
constexpr std::wstring_view defaults[] = {L"Invoke or press F2 to edit exact source.",
                                          L"Press Esc to leave exact source editing. Edits remain undoable.",
                                          L"Editing exact equation source.",
                                          L"Mathematical expression. Exact TeX source is available as document text.",
                                          L"Equation cannot be prepared natively for this view. Exact source is preserved.",
                                          L"Equation exceeds native object capacity. Exact source is preserved.",
                                          L"Unsupported native equation. Exact source is preserved.",
                                          L"Footnote {0}",
                                          L"Go to footnote {0}.",
                                          L"Return to reference {0} for footnote {1}",
                                          L"Return to reference {0} for footnote {1}",
                                          L"Too many diagrams. Source is preserved.",
                                          L"Diagram fence options are unsupported. Source is preserved.",
                                          L"Missing closing diagram fence. Source is preserved.",
                                          L"Diagram source is too large. Source is preserved.",
                                          L"Diagram alternative is too large. Source is preserved.",
                                          L"Unsupported diagram syntax. Source is preserved.",
                                          L"Diagram exceeds its capacity. Source is preserved.",
                                          L"Malformed diagram. Source is preserved.",
                                          L"Empty flowchart. Source is preserved.",
                                          L"Native object capacity exceeded. Source is preserved.",
                                          L"State diagram, ",
                                          L"Class diagram, ",
                                          L"Flowchart, ",
                                          L"top to bottom",
                                          L"bottom to top",
                                          L"left to right",
                                          L"right to left",
                                          L". Nodes: ",
                                          L" Groups: ",
                                          L" Connections: ",
                                          L" note: ",
                                          L" implements ",
                                          L" inherits ",
                                          L" is part of ",
                                          L" is aggregated by ",
                                          L"State diagram. Source is available as document text.",
                                          L"Class diagram. Source is available as document text.",
                                          L"Flowchart. Source is available as document text.",
                                          L"Editing exact block source.",
                                          L"Diagram exceeds native geometry capacity for this view. Exact source is preserved.",
                                          L"Missing closing YAML front matter fence.",
                                          L"Missing closing TOML front matter fence.",
                                          L"Invalid TOML front matter.",
                                          L"TOML front matter exceeds the supported validation capacity.",
                                          L"Invalid YAML front matter.",
                                          L"YAML front matter has a duplicate mapping key.",
                                          L"YAML front matter contains an unresolved alias.",
                                          L"YAML front matter exceeds the supported validation capacity.",
                                          L"Sequence diagram, ",
                                          L"Sequence diagram. Source is available as document text.",
                                          L" sends ",
                                          L" asynchronously sends ",
                                          L" to ",
                                          L" (dotted); ",
                                          L"Note: ",
                                          L"Empty pie chart. Source is preserved.",
                                          L"Empty Gantt chart. Source is preserved.",
                                          L"Empty mind map. Source is preserved.",
                                          L"Empty sequence diagram. Source is preserved.",
                                          L"Empty entity relationship diagram. Source is preserved.",
                                          L"Pie chart: ",
                                          L". Slices: ",
                                          L"Pie chart. Source is available as document text.",
                                          L"Entity relationship diagram, ",
                                          L"Entity relationship diagram. Source is available as document text.",
                                          L"] identifying ",
                                          L"] non-identifying ",
                                          L"Gantt chart: ",
                                          L". Tasks: ",
                                          L"Gantt chart. Source is available as document text.",
                                          L"Mind map. Nodes: ",
                                          L"Mind map. Source is available as document text.",
                                          L"Missing closing display math delimiter. Exact source is preserved.",
                                          L"Missing closing math fence. Exact source is preserved.",
                                          L"Annotation",
                                          L"Highlight",
                                          L"Spelling error",
                                          L"Grammar error",
                                          L"Properties"};
static_assert(std::size(defaults) == static_cast<std::size_t>(Id::Count));
unsigned ArgumentCount(Id id) noexcept
{
    return id == Id::ReturnToReference || id == Id::ReturnToReferenceHelp ? 2u : id == Id::FootnoteName || id == Id::GoToFootnote ? 1u : 0u;
}
SourceResult ValidateLabels(std::span<const AccessibilityTextOverride> entries) noexcept
{
    std::array<bool, static_cast<std::size_t>(Id::Count)> seen{};
    if (entries.size() > seen.size())
        return SourceResult::CapacityExceeded;
    for (const auto& entry : entries)
    {
        const auto index = static_cast<std::size_t>(entry.id);
        if (index >= seen.size() || seen[index] || entry.text.empty() || entry.text.size() > AccessibilityLabels::MaximumTextUnits ||
            ! IsValidUtf16(entry.text))
            return SourceResult::InvalidSource;
        const auto count = ArgumentCount(entry.id);
        if (count)
        {
            unsigned arguments{};
            for (std::size_t at = 0u; at < entry.text.size(); ++at)
            {
                if (entry.text[at] == L'}')
                    return SourceResult::InvalidSource;
                if (entry.text[at] != L'{')
                    continue;
                if (entry.text.size() - at < 3u || entry.text[at + 2u] != L'}' || entry.text[at + 1u] < L'0' ||
                    entry.text[at + 1u] >= L'0' + static_cast<wchar_t>(count))
                    return SourceResult::InvalidSource;
                const auto bit = 1u << static_cast<unsigned>(entry.text[at + 1u] - L'0');
                if (arguments & bit)
                    return SourceResult::InvalidSource;
                arguments |= bit;
                at += 2u;
            }
            if (arguments != (1u << count) - 1u)
                return SourceResult::InvalidSource;
        }
        seen[index] = true;
    }
    return SourceResult::Success;
}
} // namespace
AccessibilityLabels::AccessibilityLabels(std::span<const AccessibilityTextOverride> entries)
{
    Failure::Scope failureScope;
    if (ValidateLabels(entries) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    for (const auto& entry : entries)
        values_[static_cast<std::size_t>(entry.id)].emplace(entry.text);
}
SourceResult AccessibilityLabels::TryCreate(std::span<const AccessibilityTextOverride> entries, std::shared_ptr<const AccessibilityLabels>& output)
{
    Failure::Scope failureScope;
    const auto result = ValidateLabels(entries);
    if (result != SourceResult::Success)
        return result;
    auto candidate = std::make_shared<const AccessibilityLabels>(entries);
    output.swap(candidate);
    return SourceResult::Success;
}
std::wstring_view AccessibilityLabels::Default(Id id) noexcept
{
    const auto index = static_cast<std::size_t>(id);
    return index < std::size(defaults) ? defaults[index] : std::wstring_view{};
}
std::wstring_view AccessibilityLabels::Get(Id id) const noexcept
{
    const auto index = static_cast<std::size_t>(id);
    return index < values_.size() && values_[index] ? std::wstring_view(values_[index].value()) : Default(id);
}
std::wstring_view AccessibilityLabels::Text(const AccessibilityLabels* labels, Id id) noexcept
{
    return labels ? labels->Get(id) : Default(id);
}
SourceResult AccessibilityLabels::TryNumber(const AccessibilityLabels* labels, Id id, std::size_t number, std::size_t second, std::wstring& output)
{
    Failure::Scope failureScope;
    const auto pattern = Text(labels, id);
    if (! ArgumentCount(id))
        return SourceResult::InvalidSource;
    std::wstring result(0u, L' ');
    std::size_t first{};
    for (auto marker = pattern.find(L'{'); marker != pattern.npos; marker = pattern.find(L'{', first))
    {
        result.append(pattern.substr(first, marker - first));
        result.append(std::to_wstring(pattern[marker + 1u] == L'0' ? number : second));
        first = marker + 3u;
    }
    result.append(pattern.substr(first));
    output.swap(result);
    return SourceResult::Success;
}
std::wstring AccessibilityLabels::Number(const AccessibilityLabels* labels, Id id, std::size_t number, std::size_t second)
{
    Failure::Scope failureScope;
    std::wstring output(0u, L' ');
    if (TryNumber(labels, id, number, second, output) != SourceResult::Success)
        Failure::FailContract(E_INVALIDARG);
    return std::wstring(output);
}
} // namespace DxUi::RichText
