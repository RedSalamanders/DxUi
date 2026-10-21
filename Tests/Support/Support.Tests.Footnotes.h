#pragma once
#include <DxUi/MarkdownSyntax.h>
#include <DxUi/RichSemanticDocument.h>
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
inline constexpr ExpectedNote container[]  = {{L"n", L"n", 1u, 1u}};
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
    {L"- [^n]: note\n\nmain[^n]\n", container},
    {L"> [^n]: note\n\nmain[^n]\n", container},
    {L"> - [^n]: note\n\nmain[^n]\n", container},
    {L"- outer\n  [^n]: note\n\nmain[^n]\n", container},
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
template <typename Check> inline void CheckSemantic(const SemanticDocument& document, Check&& check)
{
    const auto& syntax = *document.Syntax();
    std::size_t footerCount{}, expectedFooters{}, expectedReferences{};
    for (const auto& note : syntax.Footnotes())
    {
        expectedFooters += note.ordinal != 0u;
        expectedReferences += note.references.size();
    }
    const auto elements = document.Elements();
    for (std::size_t index = 0u; index < elements.size(); ++index)
    {
        const auto& element = elements[index];
        const auto target   = document.NavigationTarget(index);
        if (element.role == SemanticRole::Footnote)
        {
            check(element.parent == 0u && element.source == syntax.Nodes()[element.syntaxNode].block.source,
                  "each used primary definition is a root footer retaining its exact source envelope");
            ++footerCount;
        }
        if (element.role == SemanticRole::FootnoteReference)
        {
            const auto raw    = syntax.Slice(element.source);
            const auto ranges = document.Map().DisplayRanges(element.source);
            check(target.has_value() && raw.starts_with(L"[^") && raw.ends_with(L"]") && ranges.size() == 1u && ranges[0u] == element.display,
                  "a numbered reference atomically owns its entire raw Unicode marker");
        }
        if (element.role == SemanticRole::FootnoteBacklink)
        {
            check(target.has_value() && syntax.Slice(*target).starts_with(L"[^") && element.source.first == element.source.last &&
                      elements[element.parent].source == element.source && document.Map().SourceRanges(element.display).empty(),
                  "a generated backlink owns no raw text and carries a separate exact reference navigation target");
        }
    }
    check(footerCount == expectedFooters && document.Navigation().size() == expectedReferences * 2u,
          "only reachable primary notes render once with one source link and backlink for every reachable occurrence");
}
template <typename Editor, typename Host, typename Prepare, typename Check>
inline void ExerciseNavigation(Editor& editor, Host& host, Prepare&& prepare, Check&& check, unsigned controlModifier, unsigned shiftModifier)
{
    const std::wstring source = L"[^z]: Z body\n[^a]: A body\n\nmain[^a] and[^z] again[^a].\n";
    editor.SetText(source);
    prepare();
    const auto document = editor.GetAcknowledgedSemanticDocument(host);
    check(document != nullptr, "footnote navigation starts with acknowledged native geometry");
    CheckSemantic(*document, check);
    const auto revision = editor.Revision();
    const auto before   = host.GetFocusControl();
    for (const auto& action : document->Navigation())
    {
        check(editor.NavigateSemanticElement(host, document, action.element) && editor.GetSelection() == Selection{action.target.first, action.target.last},
              "native/embedded navigation selects each independent exact target");
        check(editor.GetText() == source && editor.Revision() == revision && ! editor.CanUndo() && host.GetFocusControl() == before,
              "navigation preserves source, revision, history and logical keyboard focus");
    }
    editor.SetReadOnly(true);
    check(editor.NavigateSemanticElement(host, document, document->Navigation().front().element), "read-only footnotes still allow internal navigation");
    editor.SetReadOnly(false);
    editor.SetEnabled(false);
    const auto heldSelection = editor.GetSelection();
    check(! editor.NavigateSemanticElement(host, document, document->Navigation().front().element) && editor.GetSelection() == heldSelection,
          "disabled internal navigation cannot change selection");
    editor.SetEnabled(true);
    prepare();
    const auto& forward     = document->Navigation().front();
    const auto rawReference = document->Elements()[forward.element].source;
    host.SetFocusControl(&editor, false);
    prepare();
    editor.SetSelection({rawReference.first, rawReference.first});
    check(editor.OnKeyDown(host, L'\r', controlModifier) && editor.GetSelection() == Selection{forward.target.first, forward.target.last},
          "Ctrl+Enter at a reference uses the same source-exact guarded definition target");
    check(editor.ScrollSourceRangeIntoView(host, rawReference.first, rawReference.last, true), "prepare reference pointer visibility");
    const auto bounds = editor.GetSemanticElementBounds(host, forward.element);
    check(bounds.has_value(), "numbered source reference has visible native hit bounds");
    check(editor.OnMouseDown(host, {(bounds->left + bounds->right) * .5f, (bounds->top + bounds->bottom) * .5f}, false, controlModifier) &&
              editor.GetSelection() == Selection{forward.target.first, forward.target.last},
          "Ctrl+click activates the internal reference without editing source");
    const auto body = source.find(L"A body"), main = source.find(L"main");
    // Navigation may have scrolled the main line above a short viewport.
    // Reveal the drag origin before sampling both coordinates: pressing there
    // must not invalidate the precomputed destination by revealing the caret.
    check(editor.ScrollSourceRangeIntoView(host, main, main + 1u, true), "stabilize the visible drag origin after footer navigation");
    const auto start = editor.TryGetTextInputCaretRect(host, main + 1u), finish = editor.TryGetTextInputCaretRect(host, body + 1u);
    check(start && finish, "main/footer drag endpoints have current source caret geometry");
    check(editor.OnMouseDown(host, {start->left, (start->top + start->bottom) * .5f}, false, 0u) &&
              editor.OnMouseMove(host, {finish->left, (finish->top + finish->bottom) * .5f}, 0u) &&
              editor.OnMouseUp(host, {finish->left, (finish->top + finish->bottom) * .5f}, false, 0u),
          "retained native/embedded pointer drag crosses the reordered main/footer boundary");
    check(editor.GetSelection() == Selection{main + 1u, body + 1u}, "cross-footer drag retains its actual raw anchor and caret");
    const auto painted = editor.TryGetTextInputRangeRects(host, body + 1u, main + 1u);
    check(painted && painted->size() >= 2u, "one source-contiguous cross-footer selection exposes disjoint visible highlight fragments");
    // Win32 VK_DOWN/VK_DELETE values keep this CPU fixture header independent
    // of Windows/UI headers while exercising the real retained input methods.
    constexpr unsigned downKey = 0x28u, deleteKey = 0x2eu;
    editor.SetSelection({main + 1u, main + 1u});
    bool reachedFooter{};
    for (unsigned step = 0u; step < 8u && ! reachedFooter; ++step)
    {
        check(editor.OnKeyDown(host, downKey, shiftModifier), "Shift+Down extends selection through reordered native lines");
        const auto selection = editor.GetSelection();
        check(selection.anchor == main + 1u, "vertical selection keeps its original raw anchor");
        reachedFooter = selection.caret >= body && selection.caret <= body + 6u;
    }
    check(reachedFooter, "vertical keyboard selection reaches the first displayed footer body");
    const auto erase = editor.GetSelection();
    check(editor.OnKeyDown(host, deleteKey, 0u), "keyboard deletion handles cross-footer source selection as one transaction");
    check(editor.GetText() == source.substr(0u, erase.Start()) + source.substr(erase.End()) && editor.CanUndo(),
          "reordered display never changes the raw interval chosen by source selection");
    check(! editor.NavigateSemanticElement(host, document, document->Navigation().front().element),
          "an edited source immediately rejects held footer navigation");
    check(editor.Undo() && editor.GetText() == source, "cross-footer deletion restores exact source in one undo");
    prepare();
    check(editor.GetAcknowledgedSemanticDocument(host) != document, "undo prepares a fresh footer publication instead of resurrecting stale actions");
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
              "hosted resolution preserves exact source/history with acknowledged semantic geometry");
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
