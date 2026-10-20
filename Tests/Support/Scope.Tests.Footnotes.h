#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <algorithm>

namespace FootnoteResolutionTests
{
using namespace DxUi::RichText;
struct ExpectedNote
{
    std::wstring_view label, spelling;
    std::size_t ordinal{}, references{};
};
inline constexpr ExpectedNote order[]            = {{L"a", L"a", 1u, 2u}, {L"z", L"z", 2u, 1u}};
inline constexpr ExpectedNote unicode[]          = {{L"strasse", L"Stra\u00dfe", 1u, 1u}};
inline constexpr ExpectedNote whitespace[]       = {{L"a b", L" A\tB ", 1u, 1u}};
inline constexpr ExpectedNote duplicate[]        = {{L"x", L"X", 1u, 1u}};
inline constexpr ExpectedNote cycle[]            = {{L"a", L"a", 2u, 1u}, {L"b", L"b", 1u, 2u}, {L"unused", L"unused", 0u, 0u}};
inline constexpr ExpectedNote mainBeforeBodies[] = {
    {L"a", L"a", 1u, 1u}, {L"b", L"b", 3u, 1u}, {L"c", L"c", 2u, 1u}, {L"d", L"d", 4u, 1u}, {L"e", L"e", 5u, 1u}};
inline constexpr ExpectedNote unused[]     = {{L"a", L"a", 0u, 0u}, {L"unused", L"unused", 0u, 0u}};
inline constexpr ExpectedNote precedence[] = {{L"n", L"n", 0u, 0u}};
inline constexpr ExpectedNote table[]      = {{L"a\\|b", L"a\\|b", 1u, 1u}};
inline constexpr ExpectedNote escaped[]    = {{L"a\\]b", L"A\\]B", 1u, 1u}};
inline constexpr ExpectedNote emphasis[]   = {{L"a_b", L"a_b", 1u, 1u}};
inline constexpr ExpectedNote noNfc[]      = {{L"\u00e9", L"\u00e9", 0u, 0u}};
struct Scenario
{
    std::wstring_view source;
    std::span<const ExpectedNote> notes;
    std::size_t missing{}, duplicates{};
};
inline constexpr Scenario scenarios[] = {
    {L"[^z]: Z\n[^a]: A\n\nfirst[^a] then[^z] and[^a].\n", order},
    {L"[^Stra\u00dfe]: body\n\nref[^STRASSE]\n", unicode},
    {L"[^ A\tB ]: body\n\nref[^a   b]\n", whitespace},
    {L"[^X]: first\n[^x]: second\n\nref[^x]\n", duplicate, 0u, 1u},
    {L"missing[^none]\n", {}, 1u},
    {L"main[^b]\n\n[^a]: A[^b]\n[^b]: B[^a]\n[^unused]: hidden[^never]\n", cycle, 1u},
    {L"main[^a] and[^c]\n\n[^a]: to[^b]\n[^b]: to[^e]\n[^c]: to[^d]\n[^d]: D\n[^e]: E\n", mainBeforeBodies},
    {L"[^unused]: hidden[^a]\n[^a]: A\n", unused},
    {L"[^n]: note\n\n[^n](local) ![alt [^n]](image) [label [^n]](local) `[^n]` \\[^n]\n", precedence},
    {L"[^a\\|b]: note\n\n| head |\n| --- |\n| [^a\\|b] |\n", table},
    {L"[^A\\]B]: body\n\nref[^a\\]b]\n", escaped},
    {L"[^a_b]: body\n\n*ref[^a_b]*\n", emphasis},
    {L"[^\u00e9]: body\n\nref[^e\u0301]\n", noNfc, 1u},
    {L"---\n[^n]: metadata\n---\n\n```\n[^n]\n```\n\n<div>\n[^n]\n</div>\n", {}},
};
inline std::shared_ptr<const MarkdownSyntax> Complete(MarkdownParser& parser, std::size_t budget = 1u)
{
    while (parser.HasPendingPreparation())
        parser.Advance(budget);
    return parser.Published();
}
template <typename Check> inline void CheckSyntax(const MarkdownSyntax& syntax, const Scenario& scenario, Check&& check)
{
    check(syntax.Source() == scenario.source && syntax.Footnotes().size() == scenario.notes.size(),
          "footnote binding preserves exact source and every independent unique-label expectation");
    const auto notes    = syntax.Footnotes();
    const auto& inlines = syntax.Inlines();
    for (std::size_t i = 0u; i < notes.size(); ++i)
    {
        const auto& actual   = notes[i];
        const auto& expected = scenario.notes[i];
        check(actual.label == expected.label && actual.ordinal == expected.ordinal && actual.references.size() == expected.references,
              "normalized label, reachable first-reference number and repeat count match independent fixtures");
        check(actual.definition < syntax.Nodes().size() && syntax.Nodes()[actual.definition].block.kind == MarkdownBlockKind::FootnoteDefinition &&
                  syntax.Slice(syntax.Nodes()[actual.definition].label) == expected.spelling,
              "the first source definition wins without normalizing its original spelling");
        for (std::size_t occurrence = 0u; occurrence < actual.references.size(); ++occurrence)
        {
            const auto index    = actual.references[occurrence];
            const auto* binding = syntax.FootnoteReferenceFor(index);
            check(index < inlines.size() && inlines[index].kind == MarkdownInlineKind::FootnoteReference && binding && binding->footnote == i &&
                      binding->occurrence == occurrence + 1u && actual.ordinal > 0u,
                  "reachable references bind final dense inline indexes with unique sequential occurrence numbers");
        }
    }
    std::size_t referenceCount{};
    for (std::size_t index = 0u; index < inlines.size(); ++index)
    {
        const auto& node    = inlines[index];
        const auto* binding = syntax.FootnoteReferenceFor(index);
        if (node.kind == MarkdownInlineKind::FootnoteReference)
        {
            ++referenceCount;
            check(binding != nullptr, "every surviving footnote token has optional final-index binding data");
            check(node.value == syntax.Slice(node.source) && node.firstChild == MarkdownNoNode,
                  "footnote reference owns its complete raw marker including escapes and Unicode");
            if (binding->footnote == MarkdownNoNode)
                check(binding->occurrence == 0u, "undefined references have no fabricated occurrence");
            else
                check(binding->footnote < notes.size() && (notes[binding->footnote].ordinal != 0u || binding->occurrence == 0u),
                      "unreachable definition-body references do not influence visible numbering");
        }
        else
            check(binding == nullptr, "ordinary inline nodes do not acquire optional extension metadata");
    }
    check(referenceCount == syntax.FootnoteReferences().size(), "the optional binding index has no retired delimiter/bracket-shell records");
    for (const auto& binding : syntax.FootnoteReferences())
        check(syntax.FootnoteReferenceFor(binding.inlineNode) == &binding,
              "immutable binding lookup returns its retained record without allocating or copying");
    std::size_t missing{}, duplicates{}, previous{};
    for (const auto& issue : syntax.Diagnostics())
    {
        check(issue.node < syntax.Nodes().size() && issue.source.first >= previous && issue.source.first <= issue.source.last &&
                  issue.source.last <= syntax.Source().size(),
              "footnote diagnostics are ordered raw source ranges with valid revision-local owners");
        previous = issue.source.first;
        if (issue.code == MarkdownDiagnosticCode::UndefinedFootnote)
        {
            ++missing;
            check(syntax.Slice(issue.source).starts_with(L"[^") && syntax.Slice(issue.source).ends_with(L"]"),
                  "missing-label diagnostic points at the whole raw reference");
        }
        else if (issue.code == MarkdownDiagnosticCode::DuplicateFootnoteDefinition)
        {
            ++duplicates;
            check(issue.source == syntax.Nodes()[issue.node].label, "duplicate diagnostic preserves the later definition's exact label range");
        }
    }
    check(missing == scenario.missing && duplicates == scenario.duplicates, "missing and duplicate definitions match independently counted diagnostics");
}
template <typename Editor, typename Prepare, typename IsCurrent, typename Check>
inline void ExerciseSourceEdits(Editor& editor, Prepare&& prepare, IsCurrent&& isCurrent, Check&& check)
{
    for (const auto& scenario : scenarios)
    {
        editor.SetText(std::wstring(scenario.source));
        prepare();
        const auto syntax = editor.GetMarkdownSyntax();
        check(syntax != nullptr, "hosted footnote resolution finishes on owner turns");
        CheckSyntax(*syntax, scenario, check);
        check(isCurrent(syntax) && editor.GetText() == scenario.source && ! editor.CanUndo(),
              "hosted resolution preserves exact source/history with an acknowledged literal fallback");
    }
    const std::wstring source = L"main[^a]\n\n[^a]: body\n";
    editor.SetText(source);
    prepare();
    const auto held = editor.GetMarkdownSyntax();
    const auto at   = source.find(L"[^a]") + 2u;
    editor.SetSelection({at, at + 1u});
    check(editor.ReplaceSelection(L"b"), "hosted raw reference-label replacement");
    prepare();
    check(editor.GetMarkdownSyntax()->Footnotes()[0u].ordinal == 0u && editor.GetMarkdownSyntax()->Diagnostics().size() == 1u &&
              held->Footnotes()[0u].ordinal == 1u,
          "hosted label edits reject stale binding without mutating a held syntax");
    check(editor.Undo() && editor.GetText() == source, "hosted reference-label undo preserves exact source");
    prepare();
    check(editor.GetMarkdownSyntax()->Footnotes()[0u].ordinal == 1u && editor.GetMarkdownSyntax()->Diagnostics().empty() && editor.Redo(),
          "hosted undo re-resolves numbering without losing redo");
}
} // namespace FootnoteResolutionTests
