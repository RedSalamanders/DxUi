#include "Scope.Tests.RichTextAccessibility.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../Embedded/Embedded.Tests.EmbeddedUiaBridge.h"
#include "../Support/Scope.Tests.RichImageResources.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <cmath>
#include <limits>
#include <thread>

namespace
{
using namespace DxUi;
constexpr std::wstring_view EditorId = L"rich.source";
class RichAccessibilityEditor final : public RichTextEditor
{
public:
    using RichTextEditor::ExportTextInputState;
    using RichTextEditor::GetTextInputRangeRects;
    using RichTextEditor::GetTextInputViewportRect;
    using RichTextEditor::HitTestTextInputPoint;
    using RichTextEditor::ImportTextInputState;
    using RichTextEditor::RichTextEditor;
    // Exercise the same protected binding boundary used by retained owners,
    // without moving the desktop or replacing the source/CPU semantic graph.
    void RebindForTest(ControlHost* host) noexcept
    {
        PropagateHost(host);
    }
};

struct RichAccessibilityFixture
{
    explicit RichAccessibilityFixture(bool embedded, bool markdownAnalysis = false, bool semanticRendering = false) : embedded(embedded)
    {
        if (embedded)
        {
            RequireSucceeded(gpu.Create(), "rich UIA borrowed WARP device");
            std::shared_ptr<GraphicsDevice> graphics;
            RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), graphics), "rich UIA shared graphics");
            RequireSucceeded(view.Attach(graphics), "attach rich UIA embedded view");
            controls = &view.Controls();
        }
        else
        {
            window   = std::make_unique<AttachedHostWindow>();
            controls = &window->Host();
        }
        auto root = std::make_unique<RichAccessibilityEditor>(L"**alpha**\r\nbeta");
        editor    = root.get();
        if (markdownAnalysis || semanticRendering)
            editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::CommonMark0312);
        if (semanticRendering)
            editor->SetMarkdownRenderingEnabled(true);
        editor->SetAccessibleName(L"Markdown editor");
        editor->SetAccessibleAutomationId(std::wstring(EditorId));
        controls->SetRoot(std::move(root));
        if (embedded)
        {
            RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare rich UIA view");
            bridge = std::make_unique<EmbeddedUiaTest::Bridge>(view, 480, 240);
            RequireSucceeded(bridge->Attach(false, 0x524943480001ull), "attach rich application accessibility bridge");
            RequireSucceeded(view.GetAccessibilityProvider(provider.put()), "embedded rich root provider");
        }
        else
        {
            Require(PrepareNativeContentForTests(*controls), "prepare native rich UIA pixels");
            provider.attach(CreateWindowHostAccessibilityProvider(window->Hwnd()));
        }
        controls->SetFocusControl(editor, false);
        Publish();
    }
    ~RichAccessibilityFixture()
    {
        provider.reset();
        bridge.reset();
        view.Detach();
    }
    void Publish()
    {
        if (embedded)
        {
            RequireSucceeded(PrepareEmbeddedContentForTests(view, 480, 240), "prepare rich source publication");
            RequireSucceeded(bridge->Update(), "publish rich embedded semantics");
        }
        else
        {
            Require(PrepareNativeContentForTests(*controls), "prepare native rich source publication");
            controls->RefreshAccessibilitySnapshot();
        }
        Pump();
    }
    void Pump() const
    {
        if (window)
            window->PumpMessages();
        else
            bridge->Pump();
    }
    HWND Hwnd() const
    {
        return window ? window->Hwnd() : bridge->Hwnd();
    }
    wil::com_ptr_nothrow<IRawElementProviderSimple> Simple()
    {
        // Semantic point hit-testing returns the innermost fragment. Acquire
        // the document by its application identity instead of guessing a pixel
        // which happens to avoid a heading/link/table cell.
        if (editor->IsMarkdownRenderingEnabled())
        {
            wil::com_ptr_nothrow<IRawElementProviderSimple> root;
            RequireSucceeded(provider.query_to(root.put()), "semantic root simple provider");
            wil::unique_variant id;
            RequireSucceeded(root->GetPropertyValue(UIA_AutomationIdPropertyId, &id), "semantic root identity");
            if (id.vt == VT_BSTR && std::wstring_view(id.bstrVal, SysStringLen(id.bstrVal)) == EditorId)
                return root;
            wil::com_ptr_nothrow<IRawElementProviderFragment> child;
            wil::com_ptr_nothrow<IRawElementProviderFragment> rootFragment;
            RequireSucceeded(provider.query_to(rootFragment.put()), "application root fragment");
            RequireSucceeded(rootFragment->Navigate(NavigateDirection_FirstChild, child.put()), "application document child");
            Require(child != nullptr, "embedded document is reachable before its semantic parts");
            root.reset();
            RequireSucceeded(child.query_to(root.put()), "document simple provider");
            return root;
        }
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        if (window)
            fragment = GetProviderAtDipPoint(window->Hwnd(), *controls, *provider.get(), 30, 20, "native rich point provider");
        else
            RequireSucceeded(provider->ElementProviderFromPoint(-31970, -31980, fragment.put()), "embedded rich point provider");
        Require(fragment != nullptr, "rich editor is reachable from its root");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(fragment.query_to(simple.put()), "rich simple provider");
        return simple;
    }
    bool embedded;
    GraphicsFixture gpu;
    EmbeddedHost view;
    std::unique_ptr<AttachedHostWindow> window;
    std::unique_ptr<EmbeddedUiaTest::Bridge> bridge;
    ControlHost* controls  = nullptr;
    RichTextEditor* editor = nullptr;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
};

void SourceAndRanges(bool embedded, bool markdownAnalysis = false)
{
    RichAccessibilityFixture fixture(embedded, markdownAnalysis);
    auto simple = fixture.Simple();
    Require(ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "rich UIA role") == UIA_DocumentControlTypeId,
            "rich editor has Document semantics");
    fixture.editor->SetAccessibilityRole(AccessibilityRole::Status);
    fixture.Publish();
    Require(ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "explicit rich UIA role") == UIA_StatusBarControlTypeId,
            "rich editor preserves the base control's explicit accessibility role override");
    fixture.editor->SetAccessibilityRole(AccessibilityRole::Default);
    fixture.Publish();
    wil::com_ptr_nothrow<ITextProvider> text;
    wil::com_ptr_nothrow<ITextProvider2> text2;
    wil::com_ptr_nothrow<ITextEditProvider> edits;
    RequireSucceeded(simple.query_to(text.put()), "rich TextPattern");
    RequireSucceeded(simple.query_to(text2.put()), "rich TextPattern2");
    RequireSucceeded(simple.query_to(edits.put()), "rich TextEditPattern");
    wil::com_ptr_nothrow<IUnknown> value;
    RequireSucceeded(simple->GetPatternProvider(UIA_ValuePatternId, value.put()), "query rich multiline Value availability");
    Require(! value, "multiline document edits use TextPattern rather than a duplicate Value");
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    RequireSucceeded(text->get_DocumentRange(document.put()), "rich source document range");
    Require(ReadTextRangeText(*document.get(), -1, "read exact source") == fixture.editor->GetText(), "UIA text preserves markers and CRLF");
    if (embedded)
    {
        EmbeddedTextInputSnapshot snapshot;
        Require(fixture.view.ReadTextInput(snapshot) == S_OK, "read rich composition source identity");
        auto preview = snapshot.state;
        preview.text.insert(4, L"\u4eee");
        preview.caretIndex = 5;
        preview.selectionAnchorIndex.reset();
        preview.compositionStartIndex      = 4;
        preview.compositionEndIndex        = 5;
        preview.conversionTargetStartIndex = 4;
        preview.conversionTargetEndIndex   = 5;
        Require(fixture.view.ApplyTextInput(snapshot.revision, preview, EmbeddedTextInputAction::Preview) == S_OK, "preview rich UIA composition");
        fixture.Publish();
        wil::com_ptr_nothrow<ITextRangeProvider> composition, conversion;
        RequireSucceeded(edits->GetActiveComposition(composition.put()), "rich TextEdit active composition");
        RequireSucceeded(edits->GetConversionTarget(conversion.put()), "rich TextEdit conversion target");
        Require(composition && conversion && ReadTextRangeText(*composition.get(), -1, "composition source") == L"\u4eee" &&
                    ReadTextRangeText(*conversion.get(), -1, "conversion source") == L"\u4eee",
                "TextEdit ranges use exact source ACPs through the rich projection");
        Require(fixture.view.ReadTextInput(snapshot) == S_OK, "read composition cancellation revision");
        Require(fixture.view.ApplyTextInput(snapshot.revision, {}, EmbeddedTextInputAction::Cancel) == S_OK, "cancel rich UIA composition");
        fixture.Publish();
        RequireSucceeded(edits->GetActiveComposition(composition.put()), "read retired composition range");
        Require(! composition, "cancelled composition leaves no active TextEdit range");
    }
    wil::com_ptr_nothrow<ITextRangeProvider> hidden;
    RequireSucceeded(document->Clone(hidden.put()), "clone source range");
    RequireSucceeded(hidden->MoveEndpointByRange(TextPatternRangeEndpoint_End, document.get(), TextPatternRangeEndpoint_Start), "collapse cloned range");
    int moved = 0;
    RequireSucceeded(hidden->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 2, &moved), "select hidden opening marker");
    Require(ReadTextRangeText(*hidden.get(), -1, "hidden source") == L"**", "hidden source remains accessible");
    SAFEARRAY* bounds = nullptr;
    RequireSucceeded(hidden->GetBoundingRectangles(&bounds), "hidden marker geometry");
    Require(bounds && bounds->rgsabound[0].cElements == 0, "hidden marker range has no invented pixel rectangle");
    SafeArrayDestroy(bounds);
    fixture.editor->SetViewMode(RichText::ViewMode::Source);
    fixture.Publish();
    bounds = nullptr;
    RequireSucceeded(hidden->GetBoundingRectangles(&bounds), "visible marker geometry");
    Require(bounds && bounds->rgsabound[0].cElements >= 4, "source mode exposes marker rectangles");
    SafeArrayDestroy(bounds);
    RequireSucceeded(hidden->Select(), "UIA selects exact source range");
    Require(fixture.editor->GetSelection() == RichText::Selection{0, 2}, "UIA selection uses source ACPs");
    fixture.editor->SetSelection({8, 2});
    fixture.Publish();
    BOOL active = FALSE;
    wil::com_ptr_nothrow<ITextRangeProvider> caret;
    RequireSucceeded(text2->GetCaretRange(&active, caret.put()), "backward selection caret range");
    if (embedded)
    {
        Require(! active, "embedded caret follows the application's inactive keyboard-focus acknowledgement");
        RequireSucceeded(fixture.bridge->Update(true), "application acknowledges embedded keyboard focus without taking desktop focus");
        RequireSucceeded(text2->GetCaretRange(&active, caret.put()), "acknowledged embedded caret range");
    }
    Require(active && ReadTextRangeText(*caret.get(), -1, "caret text").empty(), "active caret is degenerate");
    RequireSucceeded(caret->CompareEndpoints(TextPatternRangeEndpoint_Start, hidden.get(), TextPatternRangeEndpoint_End, &moved), "compare source caret");
    Require(moved == 0, "backward selection caret follows caret rather than sorted selection end");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    VARIANT attribute{};
    RequireSucceeded(document->GetAttributeValue(UIA_IsReadOnlyAttributeId, &attribute), "read rich read-only text attribute");
    Require(attribute.vt == VT_BOOL && attribute.boolVal, "read-only text state follows the control");
    VariantClear(&attribute);
    RequireSucceeded(hidden->Select(), "read-only document still permits selection");
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(hidden->Select() == UIA_E_ELEMENTNOTENABLED, "disabled editor rejects UIA selection action");
    fixture.editor->SetEnabled(true);
    fixture.editor->SetReadOnly(false);
    fixture.editor->SetText(L"\U0001f600\r\nx");
    fixture.Publish();
    Require(ReadTextRangeText(*document.get(), -1, "retained source range after replacement") == fixture.editor->GetText(),
            "retained source range reads current published text with bounded offsets");
    Require(ReadTextRangeText(*document.get(), 1, "bounded supplementary text").empty(), "bounded UIA text never splits a supplementary scalar");
    Require(ReadTextRangeText(*hidden.get(), -1, "retained two-unit source range") == L"\U0001f600",
            "retained range source and projected geometry share the current revision");
    Require(hidden->MoveEndpointByRange(TextPatternRangeEndpoint_Start, nullptr, TextPatternRangeEndpoint_End) == E_INVALIDARG,
            "endpoint movement rejects an unrelated/missing range");
    std::wstring lines;
    for (int index = 0; index < 80; ++index)
        lines += L"a long wrapped line alpha beta gamma delta epsilon\r\n";
    fixture.editor->SetText(lines);
    fixture.Publish();
    SAFEARRAY* visible = nullptr;
    RequireSucceeded(text->GetVisibleRanges(&visible), "visible source ranges");
    Require(visible && visible->rgsabound[0].cElements == 1, "one contiguous set of visible wrapped lines");
    LONG zero = 0;
    wil::com_ptr_nothrow<IUnknown> unknown;
    RequireSucceeded(SafeArrayGetElement(visible, &zero, unknown.put_void()), "read visible source range");
    SafeArrayDestroy(visible);
    wil::com_ptr_nothrow<ITextRangeProvider> visibleRange;
    RequireSucceeded(unknown.query_to(visibleRange.put()), "visible source range interface");
    const auto visibleText = ReadTextRangeText(*visibleRange.get(), -1, "visible source text");
    Require(! visibleText.empty() && visibleText.size() < lines.size(), "GetVisibleRanges excludes scrolled-off document lines");
    fixture.controls->SetRoot(std::make_unique<Label>(L"successor"));
    fixture.Publish();
    BSTR retired = nullptr;
    Require(document->GetText(-1, &retired) == UIA_E_ELEMENTNOTAVAILABLE && ! retired, "retained range disconnects after source owner replacement");
    Require(text2->GetCaretRange(&active, caret.put()) == UIA_E_ELEMENTNOTAVAILABLE, "retained caret provider never binds to successor");
}

UiaTest::ElementId FindEditor(UiaTest::Client& client, UiaTest::ElementId element, size_t depth = 0)
{
    Require(depth < 5, "rich UIA client tree walk is bounded");
    if (client.Describe(element).automationId == EditorId)
        return element;
    for (const auto child : client.Children(element))
        if (const auto found = FindEditor(client, child, depth + 1))
            return found;
    return 0;
}

void ClientNavigationAndEvents(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    UiaTest::Client client(fixture.Hwnd(), {.automationEvents = {UIA_Text_TextChangedEventId, UIA_Text_TextSelectionChangedEventId}}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    Require(editor != 0 && client.Describe(editor).controlType == UIA_DocumentControlTypeId, "real client reaches rich Document through application parents");
    const auto enclosing = client.TextEnclosingElement(editor);
    Require(enclosing && client.Same(editor, *enclosing), "client document range encloses the same reachable editor");
    const auto mark = client.Mark();
    fixture.editor->SetSelection({2, 7});
    Require(fixture.editor->ReplaceSelection(L"bravo"), "commit rich edit while UIA client listens");
    fixture.Publish();
    Require(client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                             [&]
    {
        const auto events = client.DistinctSince(mark);
        return std::ranges::any_of(events, [](const auto& event) { return event.id == UIA_Text_TextChangedEventId && event.automationId == EditorId; });
    }),
            "client hears committed source TextChanged on the reachable editor");
    const auto source = client.DocumentText(editor);
    Require(source == fixture.editor->GetText(), "real client reads the committed Unicode Markdown source");
    Require(client.TextCaretPosition(editor) == fixture.editor->GetSelection().caret,
            "real TextPattern2 client reads the mapped source caret through endpoint movement");
    const auto selectionChanged = [&](uint64_t since)
    {
        return client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                                [&]
        {
            const auto events = client.DistinctSince(since);
            return std::ranges::any_of(events,
                                       [](const auto& event) { return event.id == UIA_Text_TextSelectionChangedEventId && event.automationId == EditorId; });
        });
    };
    auto selectionMark = client.Mark();
    RequireSucceeded(client.ExecuteTextRangeCommand(editor, L"beta", UiaTest::TextRangeCommand::AddToSelection), "client adds a source selection");
    fixture.Publish();
    Require(selectionChanged(selectionMark) && client.TextSelectionText(editor) == L"beta", "client reads added selection and hears its event");
    selectionMark = client.Mark();
    RequireSucceeded(client.ExecuteTextRangeCommand(editor, L"beta", UiaTest::TextRangeCommand::RemoveFromSelection), "client removes source selection");
    fixture.Publish();
    Require(selectionChanged(selectionMark) && client.TextSelectionText(editor).empty(), "client reads removed selection and hears its event");
    RequireSucceeded(client.ExecuteTextRangeCommand(editor, L"beta", UiaTest::TextRangeCommand::ScrollToTop), "client scrolls source range through transport");
    fixture.Publish();
    Require(client.DocumentText(editor) == source, "client scrolling preserves source text");
}

wil::com_ptr_nothrow<ITextProvider> TextProvider(RichAccessibilityFixture& fixture)
{
    wil::com_ptr_nothrow<ITextProvider> text;
    RequireSucceeded(fixture.Simple().query_to(text.put()), "rich command text provider");
    return text;
}
wil::com_ptr_nothrow<ITextRangeProvider> AsciiRange(ITextProvider& text, size_t first, size_t last)
{
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text.get_DocumentRange(range.put()), "command document range");
    RequireSucceeded(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start), "collapse command range");
    int moved = 0;
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, static_cast<int>(last), &moved), "command range end");
    Require(moved == static_cast<int>(last), "ASCII range end moved exactly");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, static_cast<int>(first), &moved), "command range start");
    Require(moved == static_cast<int>(first), "ASCII range start moved exactly");
    return range;
}
wil::com_ptr_nothrow<ITextRangeProvider> FindRange(ITextRangeProvider& document, std::wstring_view needle, bool backward = false, bool ignoreCase = false)
{
    wil::unique_bstr value(SysAllocStringLen(needle.data(), static_cast<UINT>(needle.size())));
    wil::com_ptr_nothrow<ITextRangeProvider> found;
    RequireSucceeded(document.FindText(value.get(), backward, ignoreCase, found.put()), "find source range");
    return found;
}
size_t RectangleValues(ITextRangeProvider& range)
{
    SAFEARRAY* rectangles = nullptr;
    RequireSucceeded(range.GetBoundingRectangles(&rectangles), "range rectangle array");
    Require(rectangles != nullptr, "rectangle array is present even when empty");
    const size_t count = rectangles->rgsabound[0].cElements;
    SafeArrayDestroy(rectangles);
    return count;
}

void SelectionCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    fixture.editor->SetText(L"0123456789ABCDEFG");
    fixture.editor->ReplaceSelection(L"x");
    Require(fixture.editor->Undo() && fixture.editor->CanRedo(), "selection fixture retains redo history");
    fixture.Publish();
    auto text = TextProvider(fixture);
    SupportedTextSelection supported{};
    RequireSucceeded(text->get_SupportedTextSelection(&supported), "single-selection capability");
    Require(supported == SupportedTextSelection_Single, "range commands do not claim disjoint selection support");
    auto left   = AsciiRange(*text.get(), 2, 4);
    auto right  = AsciiRange(*text.get(), 6, 8);
    auto middle = AsciiRange(*text.get(), 4, 6);
    auto whole  = AsciiRange(*text.get(), 0, 12);
    auto caret  = AsciiRange(*text.get(), 10, 10);
    int changes = 0;
    fixture.editor->SetOnTextChanged([&] { ++changes; });
    const auto revision = fixture.editor->Revision();
    fixture.editor->SetSelection({6, 4});
    fixture.Publish();
    RequireSucceeded(left->AddToSelection(), "add touching prefix to backward selection");
    Require(fixture.editor->GetSelection() == RichText::Selection{6, 2}, "prefix union preserves backward caret");
    fixture.Publish();
    RequireSucceeded(right->AddToSelection(), "add touching suffix");
    Require(fixture.editor->GetSelection() == RichText::Selection{8, 2}, "suffix union preserves backward caret");
    fixture.Publish();
    RequireSucceeded(left->AddToSelection(), "adding contained range is idempotent");
    Require(caret->RemoveFromSelection() == S_OK, "removing empty range is a no-op");
    Require(middle->RemoveFromSelection() == UIA_E_INVALIDOPERATION, "middle subtraction rejects two islands");
    Require(fixture.editor->GetSelection() == RichText::Selection{8, 2}, "rejected subtraction leaves selection intact");
    RequireSucceeded(left->RemoveFromSelection(), "remove selected prefix");
    Require(fixture.editor->GetSelection() == RichText::Selection{8, 4}, "prefix subtraction preserves direction");
    fixture.Publish();
    RequireSucceeded(right->RemoveFromSelection(), "remove selected suffix");
    Require(fixture.editor->GetSelection() == RichText::Selection{6, 4}, "suffix subtraction preserves direction");
    fixture.Publish();
    Require(caret->AddToSelection() == S_OK && fixture.editor->GetSelection() == RichText::Selection{10, 10}, "degenerate add moves insertion point");
    fixture.Publish();
    RequireSucceeded(left->AddToSelection(), "adding to insertion point creates a selection");
    fixture.Publish();
    Require(right->AddToSelection() == UIA_E_INVALIDOPERATION, "disjoint addition is rejected");
    Require(right->RemoveFromSelection() == S_OK && fixture.editor->GetSelection() == RichText::Selection{2, 4}, "nonoverlap subtraction changes nothing");
    RequireSucceeded(whole->RemoveFromSelection(), "remove entire selection");
    Require(fixture.editor->GetSelection() == RichText::Selection{4, 4}, "whole subtraction keeps the active insertion point");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    RequireSucceeded(left->AddToSelection(), "read-only permits source selection");
    fixture.Publish();
    RequireSucceeded(left->RemoveFromSelection(), "read-only permits selection removal");
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(left->AddToSelection() == UIA_E_ELEMENTNOTENABLED && left->RemoveFromSelection() == UIA_E_ELEMENTNOTENABLED,
            "disabled rejects both selection commands");
    fixture.editor->SetEnabled(true);
    fixture.editor->SetSelection({3, 6});
    fixture.Publish();
    auto overlap = AsciiRange(*text.get(), 5, 9);
    RequireSucceeded(overlap->AddToSelection(), "partially overlapping addition");
    Require(fixture.editor->GetSelection() == RichText::Selection{3, 9}, "overlap extends the existing contiguous selection");
    fixture.Publish();
    RequireSucceeded(overlap->RemoveFromSelection(), "partially overlapping subtraction");
    Require(fixture.editor->GetSelection() == RichText::Selection{3, 5}, "overlap truncates only the selected suffix");
    fixture.editor->SetVisible(false);
    fixture.Publish();
    Require(left->AddToSelection() == UIA_E_ELEMENTNOTAVAILABLE && left->RemoveFromSelection() == UIA_E_ELEMENTNOTAVAILABLE,
            "hidden editor has no actionable source range");
    Require(fixture.editor->Revision() == revision && fixture.editor->CanRedo() && ! fixture.editor->CanUndo() && changes == 0,
            "selection-only commands preserve source revision, redo and notifications");
    fixture.controls->SetRoot(std::make_unique<RichTextEditor>(L"replacement"));
    fixture.Publish();
    Require(left->AddToSelection() == UIA_E_ELEMENTNOTAVAILABLE && left->RemoveFromSelection() == UIA_E_ELEMENTNOTAVAILABLE,
            "retained selection commands cannot reach same-path replacement");
}

void SearchCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    fixture.editor->SetText(L"**Alpha** alpha ALPHA\r\n\u03a9mega \U0001f600");
    fixture.Publish();
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    RequireSucceeded(text->get_DocumentRange(document.put()), "search document");
    auto first = FindRange(*document.get(), L"alpha", false, true);
    auto last  = FindRange(*document.get(), L"alpha", true, true);
    auto exact = FindRange(*document.get(), L"alpha");
    Require(first && last && exact, "forward/backward/exact search finds source matches");
    Require(ReadTextRangeText(*first.get(), -1, "first match") == L"Alpha" && ReadTextRangeText(*last.get(), -1, "last match") == L"ALPHA" &&
                ReadTextRangeText(*exact.get(), -1, "case-sensitive match") == L"alpha",
            "ordinal search respects direction and case");
    Require(! FindRange(*exact.get(), L"Alpha") && ! FindRange(*document.get(), L"absent"), "search is confined to its range and no-match returns null");
    Require(FindRange(*document.get(), L"**") != nullptr, "search includes hidden Markdown delimiters");
    Require(FindRange(*document.get(), L"\u03c9MEGA", false, true) != nullptr, "case-insensitive search handles Unicode outside ASCII");
    Require(FindRange(*document.get(), L"\U0001f600") != nullptr && ! FindRange(*document.get(), L"\r"), "search preserves scalar and CRLF boundaries");
    wil::com_ptr_nothrow<ITextRangeProvider> found;
    Require(document->FindText(nullptr, FALSE, FALSE, found.put()) == E_INVALIDARG && ! found, "null/empty search is rejected");
    wil::unique_bstr malformed(SysAllocStringLen(L"\xd800", 1));
    Require(document->FindText(malformed.get(), FALSE, FALSE, found.put()) == E_INVALIDARG && ! found, "malformed Unicode search is rejected");
    Require(document->FindText(nullptr, FALSE, FALSE, nullptr) == E_POINTER, "search validates output pointer");
    VARIANT attribute{};
    attribute.vt      = VT_BOOL;
    attribute.boolVal = VARIANT_FALSE;
    RequireSucceeded(exact->FindAttribute(UIA_IsReadOnlyAttributeId, attribute, FALSE, found.put()), "uniform writable attribute search");
    BOOL same = FALSE;
    Require(found && SUCCEEDED(found->Compare(exact.get(), &same)) && same, "attribute match returns the entire bounded range");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    RequireSucceeded(document->FindAttribute(UIA_IsReadOnlyAttributeId, attribute, TRUE, found.put()), "attribute mismatch");
    Require(! found, "read-only mismatch returns no range");
    attribute.boolVal = VARIANT_TRUE;
    RequireSucceeded(document->FindAttribute(UIA_IsReadOnlyAttributeId, attribute, TRUE, found.put()), "uniform read-only attribute search");
    Require(found != nullptr, "attribute search reads the current publication");
    Require(document->FindAttribute(UIA_FontNameAttributeId, attribute, FALSE, found.put()) == UIA_E_NOTSUPPORTED && ! found,
            "unsupported rich attribute search is explicit");
    attribute.vt = VT_I4;
    Require(document->FindAttribute(UIA_IsReadOnlyAttributeId, attribute, FALSE, found.put()) == E_INVALIDARG, "attribute type is validated");
    fixture.editor->SetText(L"delta");
    fixture.Publish();
    Require(FindRange(*document.get(), L"delta") != nullptr, "retained search range uses bounded current source");
    fixture.controls->SetRoot(std::make_unique<Label>(L"successor"));
    fixture.Publish();
    Require(document->FindAttribute(UIA_IsReadOnlyAttributeId, attribute, FALSE, found.put()) == UIA_E_ELEMENTNOTAVAILABLE &&
                document->FindText(nullptr, FALSE, FALSE, found.put()) == UIA_E_ELEMENTNOTAVAILABLE,
            "search disconnects before accessing a retired owner");
}

void ScrollingCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    std::wstring source;
    for (int index = 0; index < 80; ++index)
        source += L"**line" + std::to_wstring(index) + L"** alpha beta gamma delta\r\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    const auto prepared = fixture.editor->DebugGetState();
    Require(prepared.retainedGeometryChunks > 1u && ! prepared.wholeGeometryFallback && ! prepared.wholeRasterFallback && prepared.wholeLayoutsPrepared == 0u,
            "native and embedded UIA scrolling fixtures actually use default-font positioned chunks");
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    RequireSucceeded(text->get_DocumentRange(document.put()), "scroll document");
    Require(ReadTextRangeText(*document.get(), -1, "positioned source document") == source,
            "private chunk-origin units never enter native or embedded UIA document text");
    auto target = FindRange(*document.get(), L"line40");
    Require(target && RectangleValues(*target.get()) == 0, "offscreen target starts without visible rectangles");
    const auto selection = fixture.editor->GetSelection();
    const auto revision  = fixture.editor->Revision();
    RequireSucceeded(target->ScrollIntoView(TRUE), "align source range to top");
    fixture.Publish();
    const float topOffset = fixture.editor->DebugGetState().scrollOffsetDip;
    Require(topOffset > 0 && RectangleValues(*target.get()) >= 4, "top scroll reveals mapped source geometry");
    RequireSucceeded(target->ScrollIntoView(FALSE), "align source range to bottom");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip < topOffset && RectangleValues(*target.get()) >= 4, "bottom alignment uses the lower range edge");
    auto hidden       = FindRange(*document.get(), L"**");
    const auto offset = fixture.editor->DebugGetState().scrollOffsetDip;
    Require(hidden && SUCCEEDED(hidden->ScrollIntoView(TRUE)), "hidden-only range scroll succeeds without fabricated geometry");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip == offset && RectangleValues(*hidden.get()) == 0,
            "offscreen hidden markers do not jump the viewport");
    RequireSucceeded(document->ScrollIntoView(TRUE), "oversized range aligns first edge");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip == 0, "document top is clamped");
    RequireSucceeded(document->ScrollIntoView(FALSE), "oversized range aligns last edge");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip > topOffset, "oversized range bottom reveals end of document");
    wil::com_ptr_nothrow<ITextProvider2> text2;
    RequireSucceeded(text.query_to(text2.put()), "scroll caret provider");
    wil::com_ptr_nothrow<ITextRangeProvider> caret;
    BOOL active = FALSE;
    RequireSucceeded(text2->GetCaretRange(&active, caret.put()), "source insertion point to reveal");
    RequireSucceeded(caret->ScrollIntoView(TRUE), "scroll degenerate source range");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip == 0, "caret scroll reveals first line without moving insertion point");
    fixture.editor->SetViewMode(RichText::ViewMode::Source);
    fixture.editor->SetFlowDirection(FlowDirection::RightToLeft);
    fixture.Publish();
    RequireSucceeded(target->ScrollIntoView(FALSE), "scroll source mode with RTL flow");
    fixture.Publish();
    Require(RectangleValues(*target.get()) >= 4, "RTL source range is visible after scrolling");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    RequireSucceeded(target->ScrollIntoView(TRUE), "read-only document remains scrollable");
    fixture.Publish();
    Require(fixture.editor->GetSelection() == selection && fixture.editor->Revision() == revision && fixture.editor->GetText() == source,
            "scrolling changes neither source nor selection nor history");
    Require(! fixture.editor->ScrollSourceRangeIntoView(*fixture.controls, source.size() + 1, source.size() + 1, true),
            "public scroll rejects invalid source bounds");
    fixture.editor->SetText(L"");
    fixture.Publish();
    RequireSucceeded(text->get_DocumentRange(document.put()), "empty source range");
    RequireSucceeded(document->ScrollIntoView(TRUE), "empty document scroll to top");
    RequireSucceeded(document->ScrollIntoView(FALSE), "empty document scroll to bottom");
    fixture.Publish();
    Require(fixture.editor->DebugGetState().scrollOffsetDip == 0, "empty document remains clamped at origin");
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(target->ScrollIntoView(TRUE) == UIA_E_ELEMENTNOTENABLED, "disabled editor rejects range scrolling");
    fixture.controls->SetRoot(std::make_unique<RichTextEditor>(L"replacement"));
    fixture.Publish();
    Require(target->ScrollIntoView(TRUE) == UIA_E_ELEMENTNOTAVAILABLE, "retained scroll range never acts on replacement");
}

void RemainingRangeCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    fixture.editor->SetText(std::wstring(180, L'x') + L" tail\r\nlast\r\n");
    fixture.Publish();
    auto text  = TextProvider(fixture);
    auto range = AsciiRange(*text.get(), 0, 1);
    RequireSucceeded(range->ExpandToEnclosingUnit(TextUnit_Line), "expand prepared wrapped visual line");
    const auto firstLine = ReadTextRangeText(*range.get(), -1, "first wrapped line");
    Require(! firstLine.empty() && firstLine.size() < 180, "Line unit follows wrapping rather than the whole logical paragraph");
    int moved = 0;
    RequireSucceeded(range->Move(TextUnit_Line, 1, &moved), "move a wrapped visual line");
    Require(moved == 1, "visual line move reports one unit");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Line, 1, &moved), "move endpoint by visual line");
    Require(moved == 1, "visual endpoint move reports one unit");
    wil::com_ptr_nothrow<ITextRangeProvider> clone;
    RequireSucceeded(range->Clone(clone.put()), "clone moved source range");
    BOOL same = FALSE;
    RequireSucceeded(range->Compare(clone.get(), &same), "compare cloned range");
    Require(same, "clone preserves endpoints");
    RequireSucceeded(clone->MoveEndpointByRange(TextPatternRangeEndpoint_Start, range.get(), TextPatternRangeEndpoint_End), "collapse crossing start endpoint");
    Require(ReadTextRangeText(*clone.get(), -1, "collapsed endpoint").empty(), "crossing endpoints collapse range");
    SAFEARRAY* children = nullptr;
    RequireSucceeded(range->GetChildren(&children), "source range children");
    Require(children && children->rgsabound[0].cElements == 0, "source-only range has no invented embedded children");
    SafeArrayDestroy(children);
    BSTR invalid = nullptr;
    Require(range->GetText(-2, &invalid) == E_INVALIDARG && ! invalid, "invalid text length rejected");
    Require(range->ExpandToEnclosingUnit(static_cast<TextUnit>(99)) == E_INVALIDARG && range->Move(static_cast<TextUnit>(99), 1, &moved) == E_INVALIDARG &&
                range->MoveEndpointByUnit(static_cast<TextPatternRangeEndpoint>(99), TextUnit_Word, 1, &moved) == E_INVALIDARG,
            "unit and endpoint enums are validated");
    fixture.editor->SetText(L"\U0001f600\r\nalpha beta");
    fixture.Publish();
    RequireSucceeded(text->get_DocumentRange(range.put()), "Unicode command range");
    RequireSucceeded(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start), "Unicode caret start");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 2, &moved), "Unicode/CRLF character movement");
    Require(moved == 2 && ReadTextRangeText(*range.get(), -1, "Unicode unit text") == L"\U0001f600\r\n", "character movement preserves scalar and CRLF units");
    RequireSucceeded(range->ExpandToEnclosingUnit(TextUnit_Document), "expand document");
    Require(ReadTextRangeText(*range.get(), -1, "whole document") == fixture.editor->GetText(), "document expansion uses current exact source");
    RequireSucceeded(range->Move(TextUnit_Document, 1, &moved), "whole document cannot move beyond itself");
    Require(moved == 0, "document movement reports actual bounded count");
    auto word = FindRange(*range.get(), L"alpha");
    Require(word != nullptr, "word movement fixture starts at alpha");
    RequireSucceeded(word->ExpandToEnclosingUnit(TextUnit_Format), "unsupported format promotes to word");
    Require(ReadTextRangeText(*word.get(), -1, "expanded word").starts_with(L"alpha"), "format fallback encloses the source word");
    RequireSucceeded(word->Move(TextUnit_Word, 1, &moved), "move nondegenerate word range");
    Require(moved == 1 && ReadTextRangeText(*word.get(), -1, "next word").starts_with(L"beta"), "word movement selects next word and reports count");
    for (const auto unit : {TextUnit_Paragraph, TextUnit_Page})
    {
        RequireSucceeded(word->ExpandToEnclosingUnit(unit), "unsupported paragraph/page promotes to document");
        Require(ReadTextRangeText(*word.get(), -1, "promoted unit") == fixture.editor->GetText(), "coarser unit fallback encloses exact source document");
    }
    Require(range->Compare(nullptr, &same) == E_INVALIDARG &&
                range->CompareEndpoints(static_cast<TextPatternRangeEndpoint>(99), word.get(), TextPatternRangeEndpoint_Start, &moved) == E_INVALIDARG,
            "comparison validates target and endpoint arguments");
}

void ThreadedRangeCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    fixture.editor->SetText(L"0123456789ABCDEFG");
    fixture.Publish();
    auto text              = TextProvider(fixture);
    auto range             = AsciiRange(*text.get(), 2, 5);
    std::atomic<bool> done = false;
    std::array<HRESULT, 3> results{};
    std::jthread worker([&]
    {
        results[0] = range->AddToSelection();
        results[1] = range->RemoveFromSelection();
        results[2] = range->ScrollIntoView(TRUE);
        done.store(true, std::memory_order_release);
    });
    const auto deadline = GetTickCount64() + 12000;
    while (! done.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        fixture.Pump();
        Sleep(1);
    }
    Require(done.load(std::memory_order_acquire), "foreign-thread range commands finish within bounded dispatch");
    worker.join();
    for (const auto result : results)
        Require(result == (embedded ? RPC_E_WRONG_THREAD : S_OK), "native dispatches to owner; embedded refuses foreign-thread mutation");
    Require(fixture.editor->GetSelection() == (embedded ? RichText::Selection{0, 0} : RichText::Selection{5, 5}),
            "thread ownership preserves expected selection");
}

void EmbeddedCase(void (*test)(bool))
{
    std::jthread owner([test]
    {
        RequireSucceeded(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "rich embedded application STA");
        const auto apartment = wil::scope_exit([] { CoUninitialize(); });
        TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "rich embedded tests refuse desktop activation");
        test(true);
    });
}
void TestRichTextAccessibilityWindowSourceAndRanges()
{
    SourceAndRanges(false);
}
void TestRichTextAccessibilityEmbeddedSourceAndRanges()
{
    EmbeddedCase([](bool embedded) { SourceAndRanges(embedded); });
}
void TestRichTextAccessibilityWindowClientNavigationAndEvents()
{
    ClientNavigationAndEvents(false);
}
void SourceTransactionClient(bool embedded, bool semanticRendering = false)
{
    RichAccessibilityFixture fixture(embedded, true, semanticRendering);
    Require(! semanticRendering || fixture.editor->GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic,
            "semantic client fixture has acknowledged current native geometry");
    UiaTest::Client client(fixture.Hwnd(), {.automationEvents = {UIA_Text_TextChangedEventId}}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    Require(editor != 0, "transaction client reaches the editor through application parents");
    const auto original  = fixture.editor->Snapshot();
    const auto revision  = fixture.editor->Revision();
    const auto oldSyntax = fixture.editor->GetMarkdownSyntax();
    size_t notifications = 0u;
    fixture.editor->SetOnTextChanged([&] { ++notifications; });
    const std::array edits{RichText::SourceEdit{{2u, 7u}, L"bravo"}, RichText::SourceEdit{{11u, 15u}, L"GAMM"}};
    const auto mark = client.Mark();
    Require(fixture.editor->ApplySourceEdits(original, revision, edits, {7u, 2u}) && notifications == 1u,
            "two source replacements notify once before client publication");
    fixture.Publish();
    Require(client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                             [&]
    {
        const auto events = client.DistinctSince(mark);
        return std::ranges::any_of(events, [](const auto& event) { return event.id == UIA_Text_TextChangedEventId && event.automationId == EditorId; });
    }),
            "real native/embedded client hears the atomic source change");
    Require(client.DocumentText(editor) == L"**bravo**\r\nGAMM" && client.TextSelectionText(editor) == L"bravo" && client.TextCaretPosition(editor) == 2u,
            "client text, backward selection and caret share the complete committed source");
    const auto syntax = fixture.editor->GetMarkdownSyntax();
    Require(syntax && syntax->Source() == fixture.editor->GetText() && oldSyntax && oldSyntax->Source() == original.Text(),
            "UIA publication and new syntax agree while the prior immutable source survives");
    Require(fixture.editor->Undo() && notifications == 2u, "client transaction has one undo entry");
    fixture.Publish();
    Require(client.DocumentText(editor) == original.Text() && ! fixture.editor->CanUndo(), "one undo restores both client-visible source replacements");
    if (semanticRendering)
    {
        fixture.editor->SetSelection({7u, 2u});
        const auto structuralMark = client.Mark();
        RichText::MarkdownCommandRequest request;
        request.command = RichText::MarkdownCommand::ToggleStyle;
        Require(fixture.editor->ExecuteMarkdownCommand(request) == RichText::MarkdownCommandStatus::Changed && notifications == 3u,
                "native/embedded structural command notifies once before UIA publication");
        fixture.Publish();
        Require(client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                                 [&]
        {
            const auto events = client.DistinctSince(structuralMark);
            return std::ranges::any_of(events, [](const auto& event) { return event.id == UIA_Text_TextChangedEventId && event.automationId == EditorId; });
        }),
                "actual application UIA client receives structural command event");
        Require(client.DocumentText(editor) == L"alpha\r\nbeta" && client.TextSelectionText(editor) == L"alpha" && client.TextCaretPosition(editor) == 0u,
                "UIA structural selection exposes exact raw source and backward caret after marker removal");
        Require(fixture.editor->Undo(), "UIA structural one-step undo");
        fixture.Publish();
        Require(client.DocumentText(editor) == original.Text() && ! fixture.editor->CanUndo(), "UIA undo restores hidden source spelling");
    }
}
void TestRichTextAccessibilityWindowSourceTransactionClient()
{
    SourceTransactionClient(false);
}
void TestRichTextAccessibilityEmbeddedSourceTransactionClient()
{
    EmbeddedCase([](bool embedded) { SourceTransactionClient(embedded); });
}
void TestRichTextAccessibilityWindowSemanticRenderingClient()
{
    SourceTransactionClient(false, true);
}
void TestRichTextAccessibilityEmbeddedSemanticRenderingClient()
{
    EmbeddedCase([](bool embedded) { SourceTransactionClient(embedded, true); });
}
void SourceCaretSitesClient(bool embedded)
{
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    const std::wstring source = L"\r\n>\r\n- \r\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    Require(editor != 0 && client.DocumentText(editor) == source, "real UIA client reaches exact empty-container source");
    const auto screenPoint = [&](D2D1_POINT_2F point)
    {
        if (embedded)
            return POINT{static_cast<LONG>(std::lround(point.x - 32000.0f)), static_cast<LONG>(std::lround(point.y - 32000.0f))};
        return ClientPointToScreenForTest(fixture.Hwnd(),
                                          {static_cast<LONG>(std::lround(fixture.window->Host().DipsToPixels(point.x))),
                                           static_cast<LONG>(std::lround(fixture.window->Host().DipsToPixels(point.y)))},
                                          "empty source UIA point transport");
    };
    const auto emptySyntax = fixture.editor->GetMarkdownSyntax();
    for (const auto& line : emptySyntax->EmptyLines())
    {
        fixture.editor->SetSelection({line.insertion, line.insertion});
        fixture.Publish();
        const auto caret = fixture.editor->TryGetTextInputCaretRect(*fixture.controls, line.insertion);
        Require(caret.has_value(), "empty source UIA caret has current geometry");
        const auto point = screenPoint(D2D1::Point2F(caret->left, (caret->top + caret->bottom) * .5f));
        Require(client.TextCaretPosition(editor) == line.insertion && client.TextPositionFromPoint(editor, point) == line.insertion,
                "real native/embedded UIA caret and RangeFromPoint preserve exact empty-line ACPs");
        Require(fixture.editor->OnMouseDoubleClick(*fixture.controls, D2D1::Point2F(caret->left, (caret->top + caret->bottom) * .5f), false, 0u),
                "double-click empty source line through native/embedded control");
        fixture.Publish();
        Require(client.TextCaretPosition(editor) == line.insertion && client.TextSelectionText(editor).empty() && client.DocumentText(editor) == source,
                "real UIA client sees a collapsed empty-line double-click without adjacent word selection or source mutation");
    }
    const std::wstring table = L"| a | b | c |\n| - | - | - |\n| value |\n";
    fixture.editor->SetText(table);
    fixture.Publish();
    const auto viewport   = fixture.editor->TryGetTextInputViewportRect();
    const auto value      = table.find(L"value");
    const auto rectangles = fixture.editor->TryGetTextInputRangeRects(*fixture.controls, value, value + 5u);
    Require(viewport && rectangles && ! rectangles->empty(), "padded UIA cell fixture has native glyph bounds");
    const auto point = D2D1::Point2F(viewport->left + (viewport->right - viewport->left) * .8f, rectangles->front().top + 5.0f);
    Require(fixture.editor->OnMouseDown(*fixture.controls, point, false, 0u), "choose last padded UIA cell");
    static_cast<void>(fixture.editor->OnMouseUp(*fixture.controls, point, false, 0u));
    fixture.Publish();
    Require(fixture.editor->OnMouseDoubleClick(*fixture.controls, point, false, 0u), "double-click last padded UIA cell");
    fixture.Publish();
    Require(client.DocumentText(editor) == table && client.TextCaretPosition(editor) == table.rfind(L'\n') &&
                client.TextPositionFromPoint(editor, screenPoint(point)) == table.rfind(L'\n') && ! fixture.editor->CanUndo(),
            "real UIA client observes source-exact padded cell text, caret and pointer with unchanged history");
}
void TestRichTextAccessibilityWindowSourceCaretSitesClient()
{
    SourceCaretSitesClient(false);
}
void NativeHtmlAndMarkersClient(bool embedded)
{
    RichAccessibilityFixture fixture(embedded, true, true);
    const std::wstring source = L"9. [Markdown](local)\r\n10. <b>native</b>\r\n\r\n<p><a href='#target' title='hint'>label</a><br/>&fjlig;</p>\r\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    Require(editor != 0 && client.DocumentText(editor) == source, "real UIA client sees complete raw HTML/list/link source rather than native display");
    const auto label = source.find(L"label");
    fixture.editor->SetSelection({label, label + 5u});
    fixture.Publish();
    Require(client.TextSelectionText(editor) == L"label", "native HTML link selection retains exact source text in the actual client");
    fixture.editor->SetSelection({label, label});
    fixture.Publish();
    const auto caret = fixture.editor->TryGetTextInputCaretRect(*fixture.controls, label);
    Require(caret.has_value(), "native HTML client caret geometry exists");
    const auto point    = D2D1::Point2F(caret->left + .1f, caret->top + (caret->bottom - caret->top) * .5f);
    const auto toScreen = [&](D2D1_POINT_2F location)
    {
        if (embedded)
            return POINT{static_cast<LONG>(std::lround(location.x - 32000.0f)), static_cast<LONG>(std::lround(location.y - 32000.0f))};
        return ClientPointToScreenForTest(fixture.Hwnd(),
                                          {static_cast<LONG>(std::lround(fixture.window->Host().DipsToPixels(location.x))),
                                           static_cast<LONG>(std::lround(fixture.window->Host().DipsToPixels(location.y)))},
                                          "native HTML client source point");
    };
    Require(client.TextCaretPosition(editor) == label && client.TextPositionFromPoint(editor, toScreen(point)) == label,
            "actual UIA caret and RangeFromPoint share native HTML source geometry");
    const auto revision = fixture.editor->Revision();
    Require(fixture.editor->OnMouseDoubleClick(*fixture.controls, point, false, 0u), "double-click native HTML link label");
    fixture.Publish();
    Require(client.TextSelectionText(editor) == L"label" && client.DocumentText(editor) == source && fixture.editor->Revision() == revision &&
                ! fixture.editor->CanUndo(),
            "HTML link double-click selects its raw label without activation/source mutation");
    fixture.editor->SetSelection({0u, 0u});
    fixture.Publish();
    RequireSucceeded(client.ExecuteTextRangeCommand(editor, L"href='#target'", UiaTest::TextRangeCommand::AddToSelection),
                     "select hidden HTML destination through actual TextPattern");
    fixture.Publish();
    Require(client.TextSelectionText(editor) == L"href='#target'", "hidden HTML attributes remain searchable/selectable raw UIA source");
    RequireSucceeded(client.ExecuteTextRangeCommand(editor, L"label", UiaTest::TextRangeCommand::ScrollToTop), "scroll native HTML label source range");
    fixture.Publish();
    Require(client.DocumentText(editor) == source && ! fixture.editor->CanUndo(), "native HTML source-range scrolling preserves source/history");
    const std::wstring empty = L"<p><b></b></p>\n";
    fixture.editor->SetText(empty);
    fixture.Publish();
    fixture.editor->SetSelection({6u, 6u});
    fixture.Publish();
    const auto emptyCaret = fixture.editor->TryGetTextInputCaretRect(*fixture.controls, 6u);
    Require(emptyCaret.has_value(), "empty native HTML client caret geometry exists");
    const auto emptyPoint = D2D1::Point2F(emptyCaret->left, emptyCaret->top + (emptyCaret->bottom - emptyCaret->top) * .5f);
    Require(fixture.editor->OnMouseDoubleClick(*fixture.controls, emptyPoint, false, 0u), "double-click empty native HTML insertion");
    fixture.Publish();
    Require(client.DocumentText(editor) == empty && client.TextCaretPosition(editor) == 6u &&
                client.TextPositionFromPoint(editor, toScreen(emptyPoint)) == 6u && client.TextSelectionText(editor).empty() && ! fixture.editor->CanUndo(),
            "real native/embedded client observes the interior empty HTML insertion with no invented source/history");
}
void TestRichTextAccessibilityWindowNativeHtmlAndMarkersClient()
{
    NativeHtmlAndMarkersClient(false);
}
void TestRichTextAccessibilityEmbeddedNativeHtmlAndMarkersClient()
{
    EmbeddedCase(NativeHtmlAndMarkersClient);
}
void TestRichTextAccessibilityEmbeddedSourceCaretSitesClient()
{
    EmbeddedCase(SourceCaretSitesClient);
}
void TestRichTextAccessibilityEmbeddedClientNavigationAndEvents()
{
    EmbeddedCase(ClientNavigationAndEvents);
}
void TestRichTextAccessibilityWindowSelectionCommands()
{
    SelectionCommands(false);
}
void TestRichTextAccessibilityEmbeddedSelectionCommands()
{
    EmbeddedCase(SelectionCommands);
}
void TestRichTextAccessibilityWindowSearchCommands()
{
    SearchCommands(false);
}
void TestRichTextAccessibilityEmbeddedSearchCommands()
{
    EmbeddedCase(SearchCommands);
}
void TestRichTextAccessibilityWindowScrollingCommands()
{
    ScrollingCommands(false);
}
void TestRichTextAccessibilityEmbeddedScrollingCommands()
{
    EmbeddedCase(ScrollingCommands);
}
void TestRichTextAccessibilityWindowRemainingRangeCommands()
{
    RemainingRangeCommands(false);
}
void TestRichTextAccessibilityEmbeddedRemainingRangeCommands()
{
    EmbeddedCase(RemainingRangeCommands);
}
void TestRichTextAccessibilityWindowThreadedRangeCommands()
{
    ThreadedRangeCommands(false);
}
void TestRichTextAccessibilityEmbeddedThreadedRangeCommands()
{
    EmbeddedCase(ThreadedRangeCommands);
}
void UnicodeGraphemeCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    for (const std::wstring cluster : {L"\u1100\u1161\u11a8", L"\u0915\u094d\u0915", L"\U0001f469\u200d\U0001f469\u200d\U0001f467", L"a\u200d", L"\u0600x"})
    {
        const std::wstring source = L"A" + cluster + L"B";
        fixture.editor->SetViewMode(RichText::ViewMode::Source);
        fixture.editor->SetText(source);
        fixture.Publish();
        auto simple = fixture.Simple();
        wil::com_ptr_nothrow<ITextProvider> text;
        RequireSucceeded(simple.query_to(text.put()), "Unicode17 rich TextPattern");
        wil::com_ptr_nothrow<ITextRangeProvider> range;
        RequireSucceeded(text->get_DocumentRange(range.put()), "Unicode17 exact source range");
        RequireSucceeded(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start),
                         "collapse source range before Unicode characters");
        int moved = 0;
        RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, 1, &moved), "move source start past ASCII prefix");
        Require(moved == 1, "one prefix grapheme moved");
        RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved), "extend one complete Unicode grapheme");
        wil::unique_bstr selected;
        RequireSucceeded(range->GetText(-1, selected.put()), "read Unicode source unit");
        Require(moved == 1 && std::wstring_view(selected.get(), SysStringLen(selected.get())) == cluster,
                "UIA Character uses the shared Unicode17 boundary policy");
        RequireSucceeded(range->Select(), "select complete source grapheme");
        const auto end = 1u + cluster.size();
        Require(fixture.editor->GetSelection() == RichText::Selection{1u, end}, "UIA selection preserves complete grapheme source offsets");
        fixture.editor->SetSelection({end, end});
        fixture.Publish();
        Require(fixture.editor->OnKeyDown(*fixture.controls, VK_LEFT, 0) && fixture.editor->GetSelection().caret == 1u,
                "native/embedded rich left crosses the whole grapheme");
        fixture.editor->SetSelection({end, end});
        Require(fixture.editor->OnKeyDown(*fixture.controls, VK_BACK, 0) && fixture.editor->GetText() == L"AB",
                "native/embedded backspace deletes exactly one grapheme");
        Require(fixture.editor->Undo() && fixture.editor->GetText() == source, "Unicode deletion is one reversible source transaction");
    }
}
void TestRichTextAccessibilityWindowUnicode17Commands()
{
    UnicodeGraphemeCommands(false);
}
void TestRichTextAccessibilityEmbeddedUnicode17Commands()
{
    EmbeddedCase(UnicodeGraphemeCommands);
}
void InteriorAcpDeletion(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    fixture.editor->SetViewMode(RichText::ViewMode::Source);
    for (const std::wstring element : {L"a\u0301", L"\u1100\u1161\u11a8", L"\u0915\u094d\u0915", L"\U0001f469\u200d\U0001f4bb", L"\U0001f1e6\U0001f1e7"})
    {
        const auto source   = L"A" + element + L"B";
        const auto interior = 1u + StepToNextCodePoint(element, 0u);
        Require(interior < 1u + element.size(), "fixture contains a valid scalar ACP inside a complete grapheme");
        for (const auto key : {VK_BACK, VK_DELETE})
        {
            fixture.editor->SetText(source);
            fixture.editor->SetSelection({interior, interior});
            fixture.Publish();
            Require(fixture.editor->GetSelection() == RichText::Selection{interior, interior},
                    "explicit source ACP remains unsnapped before keyboard deletion");
            Require(fixture.editor->OnKeyDown(*fixture.controls, key, 0), "rich keyboard handles interior ACP deletion");
            std::cout << "rich interior embedded=" << embedded << " key=" << key << " resultUnits=" << fixture.editor->GetText().size() << '\n';
            Require(fixture.editor->GetText() == L"AB" && fixture.editor->GetSelection() == RichText::Selection{1u, 1u},
                    "collapsed rich deletion removes the complete containing grapheme");
            Require(fixture.editor->Undo() && fixture.editor->GetText() == source && fixture.editor->GetSelection() == RichText::Selection{interior, interior},
                    "rich undo restores both source and the original interior ACP");
            Require(fixture.editor->Redo() && fixture.editor->GetText() == L"AB", "rich redo repeats the one grapheme transaction");
        }
    }
    // Explicit noncollapsed source intervals keep their exact scalar meaning.
    fixture.editor->SetText(L"Aa\u0301B");
    fixture.editor->SetSelection({2u, 3u});
    fixture.Publish();
    Require(fixture.editor->OnKeyDown(*fixture.controls, VK_DELETE, 0) && fixture.editor->GetText() == L"AaB",
            "explicit noncollapsed source selection may remove a combining mark without expanding");
}
void TestRichTextAccessibilityWindowInteriorAcpDeletion()
{
    InteriorAcpDeletion(false);
}
void TestRichTextAccessibilityEmbeddedInteriorAcpDeletion()
{
    EmbeddedCase(InteriorAcpDeletion);
}
void BulkCharacterCommands(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    std::wstring source;
    constexpr int elements = 32768;
    for (int index = 0; index < elements; ++index)
        source += L"a\u0301";
    fixture.editor->SetText(source);
    fixture.editor->SetViewMode(RichText::ViewMode::Source);
    fixture.Publish();
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text->get_DocumentRange(range.put()), "maximum-source character range");
    RequireSucceeded(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start), "collapse maximum-source range");
    int moved = 0;
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, INT_MAX, &moved), "bulk character end reaches source limit");
    Require(moved == elements && ReadTextRangeText(*range.get(), -1, "maximum character range text") == source,
            "provider reports actual complete graphemes rather than requested count or UTF16 units");
    Require(fixture.editor->GetSelection() == RichText::Selection{0, 0} && ! fixture.editor->CanUndo(),
            "moving range endpoints changes neither editor selection nor history");
    RequireSucceeded(range->Select(), "select maximum character range");
    Require(fixture.editor->GetSelection() == RichText::Selection{0, source.size()}, "explicit selection applies the full source range");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, INT_MAX, &moved), "bulk start collapses at end");
    Require(moved == elements && ReadTextRangeText(*range.get(), -1, "collapsed end").empty(), "crossing start collapses the provider range at end");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Character, INT_MIN, &moved), "bulk end returns to source start");
    Require(moved == -elements && ReadTextRangeText(*range.get(), -1, "collapsed start").empty(),
            "INT_MIN movement preserves the exact negative count and collapse");
    RequireSucceeded(range->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Character, INT_MIN, &moved), "bulk start stays bounded at start");
    Require(moved == 0, "a boundary endpoint reports zero actual movement");
    Require(fixture.editor->GetText() == source && fixture.editor->GetSelection() == RichText::Selection{0, source.size()} && ! fixture.editor->CanUndo(),
            "bulk range commands preserve exact source, explicit selection and empty history in either host");
}
void TestRichTextAccessibilityWindowBulkCharacterCommands()
{
    BulkCharacterCommands(false);
}
void TestRichTextAccessibilityEmbeddedBulkCharacterCommands()
{
    EmbeddedCase(BulkCharacterCommands);
}
void PendingPreparationGeometry(bool embedded, bool markdownAnalysis = false)
{
    RichAccessibilityFixture fixture(embedded, markdownAnalysis);
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text->get_DocumentRange(range.put()), "retain document range before cooperative source change");
    std::wstring source;
    for (size_t line = 0u; line < 1024u; ++line)
        source += L"line **bold** a\u0301 \U0001f600 text\r\n";
    fixture.editor->SetText(source);
    if (embedded)
        Require(fixture.view.Prepare(480, 240) == S_FALSE, "embedded UIA fixture yields pending source");
    else
        Require(fixture.controls->RenderInitialFrameForShow(), "native UIA fixture advances once");
    const auto advances       = fixture.editor->DebugGetState().preparationAdvances;
    const auto parserAdvances = fixture.editor->DebugGetState().markdownPreparationAdvances;
    if (markdownAnalysis)
        Require(fixture.editor->DebugGetState().markdownPreparationPending && ! fixture.editor->GetMarkdownSyntax(),
                "analysis-enabled UIA fixture actually awaits parser completion");
    Require(fixture.editor->DebugGetState().preparationPending, "UIA fixture really has uncompleted glyph work");
    using UniqueArray = wil::unique_any<SAFEARRAY*, decltype(&::SafeArrayDestroy), ::SafeArrayDestroy>;
    UniqueArray bounds;
    RequireSucceeded(range->GetBoundingRectangles(bounds.put()), "pending UIA range bounds query");
    Require(bounds && bounds.get()->cDims == 1u && bounds.get()->rgsabound[0].cElements == 0u,
            "pending rich ranges never fabricate viewport-sized glyph bounds");
    Require(range->ScrollIntoView(TRUE) == UIA_E_INVALIDOPERATION, "pending range scrolling reports unavailable geometry");
    if (embedded)
    {
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> fresh;
        Require(fixture.view.GetAccessibilityProvider(fresh.put()) == S_FALSE && ! fresh,
                "embedded current provider acquisition waits for coherent preparation");
    }
    else
    {
        std::atomic<bool> done = false;
        HRESULT result         = E_FAIL;
        ULONG elements         = 1u;
        std::jthread worker([&]
        {
            UniqueArray dispatched;
            result = range->GetBoundingRectangles(dispatched.put());
            if (dispatched)
                elements = dispatched.get()->rgsabound[0].cElements;
            done.store(true, std::memory_order_release);
        });
        const auto deadline = GetTickCount64() + 12000u;
        while (! done.load(std::memory_order_acquire) && GetTickCount64() < deadline)
        {
            fixture.Pump();
            Sleep(1);
        }
        Require(done.load(std::memory_order_acquire), "native pending bounds dispatch completes within its independent deadline");
        worker.join();
        Require(result == S_OK && elements == 0u, "foreign-thread native bounds also avoid the generic viewport fallback");
    }
    Require(fixture.editor->DebugGetState().preparationAdvances == advances && fixture.editor->GetText() == source,
            "UIA owner/foreign-thread queries neither advance staging nor rewrite source");
    Require(fixture.editor->DebugGetState().markdownPreparationAdvances == parserAdvances, "UIA commands do not drain the independent lossless parser");
    fixture.Publish();
    RequireSucceeded(text->get_DocumentRange(range.put()), "read document range after current source publication");
    Require(ReadTextRangeText(*range.get(), -1, "current cooperative UIA source") == source, "published UIA text is the latest exact source");
    bounds.reset();
    RequireSucceeded(range->GetBoundingRectangles(bounds.put()), "current UIA glyph bounds");
    Require(bounds && bounds.get()->rgsabound[0].cElements > 0u, "current geometry replaces unavailable pending bounds");
}
void TestRichTextAccessibilityWindowPendingPreparationGeometry()
{
    PendingPreparationGeometry(false);
}
void TestRichTextAccessibilityEmbeddedPendingPreparationGeometry()
{
    EmbeddedCase([](bool embedded) { PendingPreparationGeometry(embedded); });
}
void TestRichTextAccessibilityWindowMarkdownSourceAndRanges()
{
    SourceAndRanges(false, true);
    PendingPreparationGeometry(false, true);
}
void TestRichTextAccessibilityEmbeddedMarkdownSourceAndRanges()
{
    EmbeddedCase([](bool embedded)
    {
        SourceAndRanges(embedded, true);
        PendingPreparationGeometry(embedded, true);
    });
}
wil::com_ptr_nothrow<IRawElementProviderSimple> FindSemanticPart(IRawElementProviderSimple& root, CONTROLTYPEID type, unsigned depth = 0u)
{
    Require(depth < 16u, "semantic provider tree has bounded ownership depth");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment, child;
    RequireSucceeded(root.QueryInterface(IID_PPV_ARGS(fragment.put())), "semantic fragment interface");
    RequireSucceeded(fragment->Navigate(NavigateDirection_FirstChild, child.put()), "first semantic child");
    while (child)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(child.query_to(simple.put()), "semantic child simple interface");
        if (ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "semantic child role") == type)
            return simple;
        auto nested = FindSemanticPart(*simple.get(), type, depth + 1u);
        if (nested)
            return nested;
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(child->Navigate(NavigateDirection_NextSibling, next.put()), "next semantic sibling");
        child = std::move(next);
    }
    return {};
}
UiaTest::ElementId FindClientPart(UiaTest::Client& client, UiaTest::ElementId parent, CONTROLTYPEID type, unsigned depth = 0u)
{
    Require(depth < 16u, "actual client semantic walk is bounded");
    for (const auto child : client.Children(parent))
    {
        const auto owner = client.Navigate(child, UiaTest::Direction::Parent);
        Require(owner && client.Same(*owner, parent), "actual semantic child navigates back to its application-reachable parent");
        if (client.Describe(child).controlType == type)
            return child;
        if (const auto found = FindClientPart(client, child, type, depth + 1u))
            return found;
    }
    return 0u;
}
void RichEditorPresentationAndLinkCues(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded, true, true);
    const std::wstring source = L"lead \U0001f600 before [alpha **bold** omega](local) after.\r\n\r\n"
                                L"<a href=\"local\"><u>marked</u> label</a>\r\n\r\n# Heading\r\n\r\n`code`\r\n\r\nlast paragraph";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto* editor     = static_cast<RichAccessibilityEditor*>(fixture.editor);
    const auto alpha = source.find(L"alpha"), bold = source.find(L"bold"), marked = source.find(L"marked");
    Require(editor->GetLinkUnderlinePolicy() == RichTextLinkUnderlinePolicy::Always && editor->IsLinkUnderlineVisible(alpha),
            "default presentation preserves persistent link underline");
    Require(! editor->IsLinkUnderlineVisible(alpha - 1u) && ! editor->IsLinkUnderlineVisible(0u),
            "plain source gaps and syntax never inherit the following run's underline");
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto owner = FindEditor(client, client.Root());
    Require(owner && FindClientPart(client, owner, UIA_HyperlinkControlTypeId), "actual client reaches inert hyperlink children");
    const auto initialGeneration = editor->DebugGetState().layoutGeneration;
    editor->SetLinkUnderlinePolicy(RichTextLinkUnderlinePolicy::HoverOrCaret);
    fixture.Publish();
    Require(! editor->IsLinkUnderlineVisible(alpha) && editor->IsLinkUnderlineVisible(marked),
            "hover policy suppresses link decoration while preserving explicit nested underline");
    auto info = client.InspectTextRange(owner, L"alpha", {UIA_UnderlineStyleAttributeId});
    Require(info.attributes[0].number == static_cast<double>(TextDecorationLineStyle_None), "actual client observes inactive link underline None");
    editor->SetSelection({bold + 1u, bold + 1u});
    fixture.Publish();
    Require(editor->IsLinkUnderlineVisible(alpha) && editor->IsLinkUnderlineVisible(bold), "caret inside one style segment decorates the whole semantic link");
    info = client.InspectTextRange(owner, L"alpha", {UIA_UnderlineStyleAttributeId});
    Require(info.attributes[0].number == static_cast<double>(TextDecorationLineStyle_Single), "actual client sees caret-dependent underline");
    editor->SetSelection({0u, 0u});
    fixture.Publish();
    const auto rectangles = editor->GetTextInputRangeRects(*fixture.controls, alpha, alpha + 5u);
    Require(rectangles && ! rectangles->empty(), "prepared source glyph geometry remains available");
    const auto rect  = rectangles->front();
    const auto point = D2D1::Point2F(rect.left + .1f, (rect.top + rect.bottom) * .5f);
    const auto hit   = editor->HitTestTextInputPoint(*fixture.controls, point);
    Require(hit && hit.value() == alpha, "native/embedded glyph hit uses unchanged exact source coordinates");
    static_cast<void>(editor->OnMouseMove(*fixture.controls, point, 0u));
    fixture.Publish();
    Require(editor->IsLinkUnderlineVisible(alpha), "actual glyph hover decorates the whole link");
    info = client.InspectTextRange(owner, L"bold", {UIA_UnderlineStyleAttributeId});
    Require(info.attributes[0].number == static_cast<double>(TextDecorationLineStyle_Single), "actual client sees hover across another style segment");
    const auto viewport = editor->GetTextInputViewportRect();
    Require(viewport.has_value(), "rich text viewport");
    static_cast<void>(editor->OnMouseMove(*fixture.controls, D2D1::Point2F(viewport->right - .1f, point.y), 0u));
    fixture.Publish();
    Require(! editor->IsLinkUnderlineVisible(alpha), "blank space beside a link is not a glyph hover");
    static_cast<void>(editor->OnMouseMove(*fixture.controls, point, 0u));
    static_cast<void>(editor->OnMouseLeave(*fixture.controls));
    fixture.Publish();
    Require(! editor->IsLinkUnderlineVisible(alpha), "pointer leave retires the link cue");
    Require(editor->DebugGetState().layoutGeneration == initialGeneration && editor->GetText() == source && ! editor->CanUndo(),
            "pointer/caret decoration preserves geometry generation, source and history");
    auto palette               = fixture.controls->GetTheme();
    const auto originalPalette = palette;
    palette.highContrast       = true;
    fixture.controls->SetTheme(palette);
    editor->SetFocusModeEnabled(true);
    fixture.Publish();
    Require(editor->IsLinkUnderlineVisible(alpha), "high contrast forces persistent link cues");
    info = client.InspectTextRange(owner, L"alpha", {UIA_UnderlineStyleAttributeId});
    Require(info.attributes[0].number == static_cast<double>(TextDecorationLineStyle_Single), "high contrast UIA agrees with visible link cues");
    fixture.controls->SetTheme(originalPalette);
    auto appearance                           = editor->GetAppearance();
    appearance.drawBorder                     = false;
    appearance.blendWithWindowBackground      = true;
    appearance.maximumTextWidthDip            = 160.0f;
    appearance.paddingDip                     = 12.0f;
    appearance.bodyFontFamily                 = L"Georgia";
    appearance.codeFontFamily                 = L"Consolas";
    appearance.bodyFontSizeDip                = 16.0f;
    appearance.codeFontSizeDip                = 13.5f;
    appearance.lineHeightMultiplier           = 1.65f;
    appearance.headingFontSizeDip[0]          = 34.0f;
    appearance.headingFontSizeDip[1]          = 23.0f;
    appearance.headingLineHeightMultiplier[0] = 1.2f;
    appearance.headingLineHeightMultiplier[1] = 1.3f;
    appearance.useAccentLinkColor             = true;
    auto heldLink                             = FindSemanticPart(*fixture.Simple().get(), UIA_HyperlinkControlTypeId);
    Require(heldLink != nullptr, "hold a reachable hyperlink before typography reflow");
    editor->SetAppearance(appearance);
    Require(! editor->DebugGetState().layoutReady, "typography/column changes retire old native geometry");
    fixture.Publish();
    Require(ReadProviderLongProperty(*heldLink.get(), UIA_ControlTypePropertyId, "held hyperlink after typography reflow") == UIA_HyperlinkControlTypeId,
            "appearance-only reflow preserves held semantic provider identity");
    const auto column = editor->GetTextInputViewportRect();
    Require(column && std::abs(column->right - column->left - 160.0f) < .01f, "shared viewport owns bounded text width");
    const auto bodyInfo    = client.InspectTextRange(owner, L"lead", {UIA_FontSizeAttributeId, UIA_FontNameAttributeId});
    const auto codeInfo    = client.InspectTextRange(owner, L"code", {UIA_FontSizeAttributeId, UIA_FontNameAttributeId});
    const auto headingInfo = client.InspectTextRange(owner, L"Heading", {UIA_FontSizeAttributeId});
    Require(std::abs(bodyInfo.attributes[0].number - 12.0) < .001 && bodyInfo.attributes[1].text == L"Georgia" &&
                std::abs(codeInfo.attributes[0].number - 10.125) < .001 && codeInfo.attributes[1].text == L"Consolas" &&
                std::abs(headingInfo.attributes[0].number - 25.5) < .001,
            "actual UIA font metrics match body/code/per-level heading options");
    const auto generation = editor->DebugGetState().layoutGeneration;
    const std::array highlights{RichTextSourceHighlight{alpha, 5u, false}, RichTextSourceHighlight{bold, 4u, true}};
    editor->SetSourceHighlights(highlights);
    fixture.Publish();
    Require(editor->GetSourceHighlights().size() == 2u && editor->DebugGetState().layoutGeneration == generation,
            "find highlights and focus paint reuse native geometry");
    auto invalid            = appearance;
    invalid.bodyFontSizeDip = std::numeric_limits<float>::quiet_NaN();
    bool rejected           = false;
    try
    {
        editor->SetAppearance(invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && editor->GetAppearance() == appearance, "invalid appearance is atomically rejected");
    rejected = false;
    const std::array malformed{RichTextSourceHighlight{source.find(L"\U0001f600") + 1u, 1u, false}};
    try
    {
        editor->SetSourceHighlights(malformed);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && editor->GetSourceHighlights().size() == 2u, "highlight cannot split a UTF-16 scalar");
    editor->SetSelection({alpha, alpha});
    Require(editor->ReplaceSelection(L"!") && editor->GetSourceHighlights().empty(), "source mutation retires all stale highlights");
    Require(editor->Undo() && editor->GetText() == source, "presentation preserves atomic source undo");
    fixture.Publish();
    editor->SetVisible(false);
    fixture.Publish();
    Require(! editor->IsLinkUnderlineVisible(alpha), "hidden editor exposes no stale link cue");
    editor->SetVisible(true);
    fixture.Publish();
    editor->SetViewMode(ViewMode::Source);
    fixture.Publish();
    Require(editor->GetText() == source && ! editor->GetAcknowledgedSemanticDocument(*fixture.controls),
            "source mode preserves exact raw text and retires semantic decoration");
    appearance.colorizeMarkdownSource = true;
    editor->SetAppearance(appearance);
    fixture.Publish();
    Require(editor->DebugGetState().sourceInkRangeCount > 0u && client.DocumentText(owner) == source,
            "native/embedded source ink keeps actual UIA text exact, including delimiters and original line endings");
    editor->SetSelection({alpha, alpha + 5u});
    fixture.Publish();
    Require(client.TextSelectionText(owner) == L"alpha" && client.TextCaretPosition(owner) == alpha + 5u && ! editor->CanUndo(),
            "syntax ink never replaces source selection, text-service offsets or history");
}
void TestRichTextAccessibilityWindowPresentationAndLinkCues()
{
    RichEditorPresentationAndLinkCues(false);
}
void TestRichTextAccessibilityEmbeddedPresentationAndLinkCues()
{
    EmbeddedCase(RichEditorPresentationAndLinkCues);
}
void RichEditorSelectionNotifications(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto owner = FindEditor(client, client.Root());
    Require(owner != 0u, "selection callback fixture is application-reachable");
    size_t calls        = 0u;
    bool pointerGesture = false;
    RichText::Selection published{};
    fixture.editor->SetOnSelectionChanged([&]
    {
        ++calls;
        published      = fixture.editor->GetSelection();
        pointerGesture = fixture.editor->IsSelectionGestureActive();
    });
    RequireSucceeded(client.ExecuteTextRangeCommand(owner, L"beta", UiaTest::TextRangeCommand::AddToSelection), "actual client selects source beta");
    const auto detail =
        std::format("UIA source selection callback: calls={}, anchor={}, caret={}, gesture={}", calls, published.anchor, published.caret, pointerGesture);
    Require(calls == 1u && ! pointerGesture && published == RichText::Selection{11u, 15u}, detail.c_str());
    fixture.Publish();
    Require(client.TextSelectionText(owner) == L"beta", "actual client reads source selection after owner snapshot publication");
    RequireSucceeded(client.ExecuteTextRangeCommand(owner, L"beta", UiaTest::TextRangeCommand::AddToSelection), "actual client repeats beta selection");
    Require(calls == 1u, "identical UIA selection does not duplicate consumer notification");
    RequireSucceeded(client.ExecuteTextRangeCommand(owner, L"beta", UiaTest::TextRangeCommand::RemoveFromSelection), "actual client removes source beta");
    Require(calls == 2u && published.anchor == published.caret && ! fixture.editor->CanUndo(),
            "UIA deselection uses canonical publication without changing source history");
    fixture.Publish();
    Require(client.TextSelectionText(owner).empty(), "actual client reads deselection from the published owner snapshot");
}
void TestRichTextAccessibilityWindowSelectionNotifications()
{
    RichEditorSelectionNotifications(false);
}
void TestRichTextAccessibilityEmbeddedSelectionNotifications()
{
    EmbeddedCase(RichEditorSelectionNotifications);
}
void RichEditorAcknowledgementIsPerEditor(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded);
    const std::wstring source = L"inline $x$ formula\r\n\r\nsecond paragraph";
    auto panel                = std::make_unique<Panel>();
    auto* editor              = panel->AddChild<RichAccessibilityEditor>(source);
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 440.0f, 120.0f));
    editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor->SetMarkdownRenderingEnabled(true);
    auto* preview = panel->AddChild<RichTextEditor>(L"preview");
    preview->SetBounds(D2D1::RectF(0.0f, 130.0f, 440.0f, 230.0f));
    fixture.editor = editor;
    fixture.controls->SetRoot(std::move(panel));
    fixture.controls->SetFocusControl(editor, false);
    fixture.Publish();
    const auto document = editor->GetAcknowledgedSemanticDocument(*fixture.controls);
    Require(document != nullptr, "two-editor owner fixture acknowledges initial semantic geometry");
    const auto math =
        std::find_if(document->Elements().begin(), document->Elements().end(), [](const auto& element) { return element.role == SemanticRole::Math; });
    Require(math != document->Elements().end() && source.substr(math->source.first, math->source.last - math->source.first) == L"$x$",
            "acknowledged semantic math retains exact inline source envelope");
    const auto index = static_cast<size_t>(math - document->Elements().begin());
    editor->SetOnSelectionChanged([&] { preview->SetText(L"pending equation preview"); });
    editor->SetSelection({math->source.first, math->source.first});
    Require(fixture.controls->HasPendingContentPreparation() && editor->GetAcknowledgedSemanticDocument(*fixture.controls) == document &&
                editor->GetSemanticElementBounds(*fixture.controls, index).has_value(),
            "selection-triggered sibling preparation cannot revoke current main semantic geometry or math source");
    const auto ranges = editor->GetTextInputRangeRects(*fixture.controls, 0u, 6u);
    Require(ranges && ! ranges->empty(), "own source geometry stays usable while a sibling is pending");
    Require(editor->OnKeyDown(*fixture.controls, VK_DOWN, 0u) && editor->GetSelection().caret > math->source.first,
            "own prepared vertical navigation remains available during sibling work");
    ControlHost foreign;
    Require(! editor->GetAcknowledgedSemanticDocument(foreign), "foreign host never acknowledges the editor's geometry");
    editor->SetOnSelectionChanged({});
    editor->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 120.0f));
    Require(! editor->GetAcknowledgedSemanticDocument(*fixture.controls), "own width reflow still rejects stale semantic geometry");
    fixture.Publish();
    editor->SetSelection({0u, 0u});
    Require(editor->ReplaceSelection(L"!") && ! editor->GetAcknowledgedSemanticDocument(*fixture.controls),
            "own source mutation immediately rejects old geometry even when a sibling is also preparing");
    Require(editor->Undo() && editor->GetText() == source && ! editor->GetAcknowledgedSemanticDocument(*fixture.controls),
            "undo retains exact source but still needs own current geometry acknowledgement");
    fixture.Publish();
    Require(editor->GetAcknowledgedSemanticDocument(*fixture.controls) != nullptr, "owner preparation restores acknowledgement without depending on siblings");
}
void TestRichTextAccessibilityWindowAcknowledgementIsPerEditor()
{
    RichEditorAcknowledgementIsPerEditor(false);
}
void TestRichTextAccessibilityEmbeddedAcknowledgementIsPerEditor()
{
    EmbeddedCase(RichEditorAcknowledgementIsPerEditor);
}
void SemanticAttributesAndFormats(bool embedded)
{
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    const std::wstring source = L"# Head\r\n\r\n**bold &fjlig; \U0001f600** plain *slant* ~~gone~~ `code` [label](#local) **last**\r\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto simple = fixture.Simple();
    wil::com_ptr_nothrow<ITextProvider> pattern;
    RequireSucceeded(simple.query_to(pattern.put()), "semantic text pattern");
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    RequireSucceeded(pattern->get_DocumentRange(document.put()), "semantic source document range");
    const auto find = [&](std::wstring_view value)
    {
        wil::unique_bstr needle(SysAllocStringLen(value.data(), static_cast<UINT>(value.size())));
        wil::com_ptr_nothrow<ITextRangeProvider> range;
        RequireSucceeded(document->FindText(needle.get(), FALSE, FALSE, range.put()), "find exact formatted source");
        Require(range != nullptr, "expected formatted source found");
        return range;
    };
    auto bold = find(L"bold");
    RequireSucceeded(bold->ExpandToEnclosingUnit(TextUnit_Format), "expand prepared source Format");
    Require(ReadTextRangeText(*bold.get(), -1, "raw Format text") == L"bold &fjlig; \U0001f600", "Format retains full raw entity and supplementary owner");
    VARIANT weight{};
    weight.vt   = VT_I4;
    weight.lVal = 700;
    wil::com_ptr_nothrow<ITextRangeProvider> found;
    RequireSucceeded(document->FindAttribute(UIA_FontWeightAttributeId, weight, FALSE, found.put()), "find bold source attribute");
    Require(found && ReadTextRangeText(*found.get(), -1, "found bold source") == L"bold &fjlig; \U0001f600", "FindAttribute uses exact source spans");
    found.reset();
    RequireSucceeded(document->FindAttribute(UIA_FontWeightAttributeId, weight, TRUE, found.put()), "backward attribute search");
    Require(found && ReadTextRangeText(*found.get(), -1, "last bold source") == L"last", "backward attribute search finds the final matching span");
    weight.vt = VT_BOOL;
    Require(document->FindAttribute(UIA_FontWeightAttributeId, weight, FALSE, found.put()) == E_INVALIDARG && ! found, "attribute type mismatch clears result");
    Require(document->FindAttribute(UIA_AnimationStyleAttributeId, weight, FALSE, found.put()) == UIA_E_NOTSUPPORTED, "unsupported attributes stay explicit");
    auto caret = find(L"bold");
    RequireSucceeded(caret->MoveEndpointByRange(TextPatternRangeEndpoint_End, caret.get(), TextPatternRangeEndpoint_Start), "collapse source Format caret");
    int moved = 0;
    RequireSucceeded(caret->MoveEndpointByUnit(TextPatternRangeEndpoint_End, TextUnit_Format, INT_MAX, &moved), "bounded Format endpoint to source end");
    Require(moved > 0 && ReadTextRangeText(*caret.get(), -1, "Format suffix") == source.substr(source.find(L"bold")),
            "large Format movement reaches exact source end");
    RequireSucceeded(caret->MoveEndpointByUnit(TextPatternRangeEndpoint_Start, TextUnit_Format, INT_MIN, &moved), "bounded Format endpoint to source start");
    Require(moved < 0 && ReadTextRangeText(*caret.get(), -1, "full Format source") == source, "INT_MIN Format movement has no overflow");
    RequireSucceeded(bold->Move(TextUnit_Format, 1, &moved), "next source format run");
    Require(moved == 1 && ReadTextRangeText(*bold.get(), -1, "regular delimiter Format").starts_with(L"** plain *"),
            "Format movement includes hidden syntax as regular source");
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    Require(editor != 0u, "formatted client reaches source document");
    const auto boldInfo = client.InspectTextRange(editor, L"bold", {UIA_FontWeightAttributeId, UIA_IsItalicAttributeId, UIA_StyleIdAttributeId}, true);
    Require(boldInfo.text == L"bold &fjlig; \U0001f600" && boldInfo.attributes[0].type == VT_I4 && boldInfo.attributes[0].number == 700.0 &&
                boldInfo.attributes[1].type == VT_BOOL && boldInfo.attributes[1].number == 0.0 && boldInfo.attributes[2].number == StyleId_Normal,
            "actual UIA client reads typed style values and raw Format expansion");
    const auto heading = client.InspectTextRange(editor, L"Head", {UIA_StyleIdAttributeId, UIA_FontWeightAttributeId, UIA_FontSizeAttributeId});
    const auto plain   = client.InspectTextRange(editor, L"plain", {UIA_FontSizeAttributeId, UIA_FontNameAttributeId});
    Require(heading.attributes[0].number == StyleId_Heading1 && heading.attributes[1].number == 600.0 &&
                std::abs(heading.attributes[2].number / plain.attributes[0].number - 1.9) < 0.001,
            "heading level, native semibold and point size agree with rendered typography");
    const auto italic = client.InspectTextRange(editor, L"slant", {UIA_IsItalicAttributeId});
    const auto strike = client.InspectTextRange(editor, L"gone", {UIA_StrikethroughStyleAttributeId});
    const auto code   = client.InspectTextRange(editor, L"code", {UIA_FontNameAttributeId});
    const auto link   = client.InspectTextRange(editor, L"label", {UIA_UnderlineStyleAttributeId});
    Require(italic.attributes[0].number == 1.0 && strike.attributes[0].number == static_cast<double>(TextDecorationLineStyle_Single) &&
                ! code.attributes[0].text.empty() && code.attributes[0].text != plain.attributes[1].text &&
                link.attributes[0].number == static_cast<double>(TextDecorationLineStyle_Single),
            "real client observes italic, strike, monospace and link underline");
    const auto all = client.InspectTextRange(editor, {}, {UIA_FontWeightAttributeId, UIA_AnimationStyleAttributeId});
    Require(all.text == source && all.attributes[0].mixed && all.attributes[1].unsupported, "mixed and unsupported UIA sentinels survive actual transport");
    Require(ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "source provider stays document") == UIA_DocumentControlTypeId,
            "structured children do not replace the Document text provider");
    fixture.editor->SetViewMode(RichText::ViewMode::Source);
    fixture.Publish();
    const auto sourceInfo = client.InspectTextRange(editor, L"bold", {UIA_FontWeightAttributeId});
    Require(sourceInfo.attributes[0].unsupported && ! fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls),
            "source mode retires semantic attributes without inventing style for raw source pixels");
}
void SemanticFrontMatterDiagnostics(bool embedded)
{
    for (const bool yaml : {true, false})
    {
        RichAccessibilityFixture fixture(embedded, true, true);
        auto* sourceAdapter = static_cast<RichAccessibilityEditor*>(fixture.editor);
        fixture.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::RichMarkdown);
        const std::wstring source            = yaml ? L"\ufeff---\r\nkey: value\r\n" : L"\ufeff+++\r\nkey = 'value'\r\n";
        const std::wstring_view expectedHelp = yaml ? L"Missing closing YAML front matter fence." : L"Missing closing TOML front matter fence.";
        const auto fence                     = yaml ? L"---" : L"+++";
        fixture.editor->SetText(source);
        fixture.Publish();
        auto simple   = fixture.Simple();
        auto metadata = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        Require(metadata && ReadProviderStringProperty(*metadata.get(), UIA_HelpTextPropertyId, "metadata diagnostic help") == expectedHelp,
                "native/embedded semantic metadata exposes an actual missing-fence explanation");
        UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
        const auto editor = FindEditor(client, client.Root());
        const auto child  = FindClientPart(client, editor, UIA_GroupControlTypeId);
        Require(child && client.StringProperty(child, UIA_HelpTextPropertyId) == expectedHelp && client.TextRangeFromChild(editor, child) == source,
                "actual native/application-bridged clients reach diagnostic help and the exact raw metadata range");
        const auto closed = source + fence + L"\r\n# Body\r\n";
        fixture.editor->SetText(closed);
        const auto retired = [](IRawElementProviderSimple& element)
        {
            VARIANT value{};
            const auto clear = wil::scope_exit([&] noexcept { VariantClear(&value); });
            return element.GetPropertyValue(UIA_HelpTextPropertyId, &value) == UIA_E_ELEMENTNOTAVAILABLE;
        };
        Require(retired(*metadata.get()), "source change immediately retires the old diagnostic fragment");
        {
            // Query the provider directly before pumping the client/host queue, so
            // asynchronous owner preparation cannot erase this pending-state case.
            wil::com_ptr_nothrow<ITextProvider> text;
            RequireSucceeded(simple.query_to(text.put()), "pending source text provider");
            wil::com_ptr_nothrow<ITextRangeProvider> range;
            RequireSucceeded(text->get_DocumentRange(range.put()), "pending source document range");
            SAFEARRAY* children = nullptr;
            VARIANT font{};
            const auto clear = wil::scope_exit([&] noexcept
            {
                if (children)
                    SafeArrayDestroy(children);
                VariantClear(&font);
            });
            RequireSucceeded(range->GetChildren(&children), "pending semantic children");
            RequireSucceeded(range->GetAttributeValue(UIA_FontNameAttributeId, &font), "pending semantic font");
            wil::com_ptr_nothrow<IUnknown> unsupported;
            RequireSucceeded(UiaGetReservedNotSupportedValue(unsupported.put()), "reserved unsupported attribute");
            Require((! children || ! children->rgsabound[0].cElements) && font.vt == VT_UNKNOWN && font.punkVal == unsupported.get(),
                    "pending source withdraws semantic children and formatting without rebuilding a snapshot on the input path");
        }
        UiaTest::ElementInfo retiredInfo;
        Require(client.TryDescribe(child, retiredInfo) == UIA_E_ELEMENTNOTAVAILABLE, "actual foreign UIA queries reject an older metadata epoch");
        fixture.Publish();
        const auto replacement = FindClientPart(client, editor, UIA_GroupControlTypeId);
        Require(replacement && client.StringProperty(replacement, UIA_HelpTextPropertyId) == L"Invoke or press F2 to edit exact source." &&
                    fixture.editor->GetMarkdownSyntax()->Diagnostics().empty() && fixture.editor->GetText() == closed && ! fixture.editor->CanUndo(),
                "closing metadata replaces its diagnostic with source-entry help without rewriting source or fabricating history");
        metadata        = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        const auto held = fixture.editor->GetSemanticDocument();
        fixture.editor->SetViewMode(RichText::ViewMode::Source);
        Require(retired(*metadata.get()), "presentation retirement invalidates semantic fragments before publication");
        fixture.editor->SetViewMode(RichText::ViewMode::Rich);
        fixture.Publish();
        Require(fixture.editor->GetSemanticDocument() == held && retired(*metadata.get()),
                "reusing an unchanged CPU document cannot resurrect a fragment from an older presentation epoch");
        metadata         = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        const auto value = closed.find(L"value");
        fixture.editor->SetSelection({value, value + 5u});
        Require(fixture.editor->ReplaceSelection(L"changed") && retired(*metadata.get()),
                "ordinary source transactions retire semantic fragments without waiting for glyph preparation");
        fixture.Publish();
        metadata = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        Require(fixture.editor->Undo() && fixture.editor->GetText() == closed && retired(*metadata.get()),
                "undo also retires the previous semantic epoch immediately");
        fixture.Publish();
        metadata = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        TextInputState entry;
        Require(sourceAdapter->ExportTextInputState(entry), "export metadata source for a synthetic input-service preview");
        const bool canRedo         = fixture.editor->CanRedo();
        entry.selectionAnchorIndex = entry.caretIndex = 0u;
        Require(sourceAdapter->ImportTextInputState(*fixture.controls, entry, false) && ! retired(*metadata.get()),
                "selection-only service imports preserve a current semantic identity");
        auto preview = entry;
        preview.text.replace(value, 5u, L"preview");
        Require(sourceAdapter->ImportTextInputState(*fixture.controls, preview, false) && retired(*metadata.get()),
                "non-notifying input-service previews retire semantic source without a text-change delegate");
        Require(sourceAdapter->ImportTextInputState(*fixture.controls, entry, false) && fixture.editor->GetText() == closed &&
                    fixture.editor->CanRedo() == canRedo,
                "preview cancellation restores exact metadata and preserves redo");
        fixture.Publish();
        metadata = FindSemanticPart(*simple.get(), UIA_GroupControlTypeId);
        sourceAdapter->RebindForTest(nullptr);
        Require(retired(*metadata.get()), "host retirement invalidates fragments even before another snapshot is built");
        sourceAdapter->RebindForTest(fixture.controls);
        fixture.Publish();
        Require(retired(*metadata.get()), "same-host rebinding cannot resurrect an older semantic epoch");
    }
}
wil::com_ptr_nothrow<IRawElementProviderSimple> FindNamedSemanticPart(IRawElementProviderSimple& root, std::wstring_view name, unsigned depth = 0u)
{
    Require(depth < 24u, "named semantic provider walk is bounded");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment, child;
    RequireSucceeded(root.QueryInterface(IID_PPV_ARGS(fragment.put())), "named semantic fragment");
    RequireSucceeded(fragment->Navigate(NavigateDirection_FirstChild, child.put()), "named first child");
    while (child)
    {
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(child.query_to(simple.put()), "named child simple");
        if (ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "named semantic part") == name)
            return simple;
        auto nested = FindNamedSemanticPart(*simple.get(), name, depth + 1u);
        if (nested)
            return nested;
        wil::com_ptr_nothrow<IRawElementProviderFragment> next;
        RequireSucceeded(child->Navigate(NavigateDirection_NextSibling, next.put()), "named next sibling");
        child = std::move(next);
    }
    return {};
}
void SemanticDiagramAlternatives(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    const std::wstring source = L"```mermaid\ngraph TD;A[\"Unicode \u03b2\"]-->B\n```\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto simple = fixture.Simple();
    auto image  = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    Require(image != nullptr, "both hosts expose a native diagram Image child after geometry acknowledgement");
    const auto name = ReadProviderStringProperty(*image.get(), UIA_NamePropertyId, "diagram alternative");
    Require(name.starts_with(L"Flowchart,") && name.find(L"Unicode \u03b2") != std::wstring::npos && name.find(L"Connections:") != std::wstring::npos,
            "diagram alternative describes Unicode labels and graph relationships");
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text->RangeFromChild(image.get(), range.put()), "native diagram source range");
    Require(ReadTextRangeText(*range.get(), -1, "diagram exact raw fence") == source, "diagram UIA child owns the complete exact original fence");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    RequireSucceeded(image.query_to(fragment.put()), "diagram fragment");
    UiaRect bounds{};
    RequireSucceeded(fragment->get_BoundingRectangle(&bounds), "diagram measured bounds");
    Require(bounds.width > 0.0 && bounds.height > 0.0, "native diagram UIA bounds are measured geometry");
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    const auto actual = FindClientPart(client, editor, UIA_ImageControlTypeId);
    Require(actual && client.TextRangeFromChild(editor, actual) == source, "actual native/application-bridged clients read original source behind the graphic");
    fixture.editor->SetText(source + L"tail\n");
    wil::unique_variant retired;
    Require(image->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "source changes immediately retire old native image providers");
    fixture.Publish();
    image = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    Require(image != nullptr, "a replacement graph exposes a new live provider");
    fixture.editor->SetMarkdownRenderingEnabled(false);
    Require(image->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "presentation withdrawal retires native graphic providers");
    fixture.editor->SetMarkdownRenderingEnabled(true);
    fixture.Publish();
    Require(image->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE,
            "restoring presentation cannot resurrect an old graphic provider");
}
void SemanticDecodedImages(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded, true, true);
    if (fixture.window)
    {
        const auto scale = fixture.controls->GetDpi() / 96.f;
        Require(
            SetWindowPos(
                fixture.Hwnd(), nullptr, 0, 0, static_cast<int>(640.f * scale), static_cast<int>(320.f * scale), SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER) !=
                FALSE,
            "decoded image UIA fixture viewport");
    }
    fixture.editor->SetDocumentFilePath(L"C:\\docs\\note.md");
    const std::wstring source = L"![\u6771\U0001f600](image.png)";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto simple = fixture.Simple();
    auto old    = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    Require(old != nullptr, "image alternative is accessible before decoding");
    std::vector<RichImageRequest> requests(0u);
    fixture.editor->TakePendingImageRequests(requests);
    Require(requests.size() == 1u && fixture.editor->CompleteImageRequest(requests[0], RichImageResourceTests::Picture()),
            "UIA fixture accepts current decoded pixels");
    wil::unique_variant retired;
    Require(old->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE,
            "image geometry publication retires old fragments at the same source revision");
    fixture.Publish();
    auto image = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    Require(image && ReadProviderStringProperty(*image.get(), UIA_NamePropertyId, "decoded image alternative") == L"\u6771\U0001f600",
            "decoded image keeps its Unicode accessible alternative");
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    RequireSucceeded(image.query_to(fragment.put()), "decoded image fragment");
    UiaRect bounds{};
    RequireSucceeded(fragment->get_BoundingRectangle(&bounds), "decoded image measured UIA bounds");
    const auto scale = fixture.controls->GetDpi() / 96.0;
    Require(std::abs(bounds.width - 96.0 * scale) < 0.25 && std::abs(bounds.height - 64.0 * scale) < 0.25,
            "UIA reports actual image pixels instead of alt glyph or full line-cell dimensions");
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text->RangeFromChild(image.get(), range.put()), "decoded image exact source range");
    Require(ReadTextRangeText(*range.get(), -1, "decoded image raw syntax") == source, "image text ranges retain exact Markdown source");
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    const auto actual = FindClientPart(client, editor, UIA_ImageControlTypeId);
    Require(actual && client.TextRangeFromChild(editor, actual) == source,
            "actual native and application-bridged UIA clients retain source behind decoded pixels");
    fixture.editor->SetText(source + L" tail");
    Require(image->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "source change retires decoded-image children immediately");
}
void SemanticObjectSourceEditing(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    const std::wstring fence = L"```mermaid\r\ngraph TD;A-->B\r\n```\r\n";
    const auto source        = fence + L"\r\nafter **bold**\r\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto simple = fixture.Simple();
    auto image  = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    wil::com_ptr_nothrow<IUnknown> pattern;
    RequireSucceeded(image->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "diagram source-edit Invoke pattern");
    Require(pattern != nullptr, "source-bearing diagram advertises exact source entry");
    wil::com_ptr_nothrow<IInvokeProvider> invoke;
    RequireSucceeded(pattern.query_to(invoke.put()), "diagram source-entry action");
    const auto revision    = fixture.editor->Revision();
    auto* focus            = fixture.controls->GetFocusControl();
    std::atomic<bool> done = false;
    HRESULT dispatched     = E_FAIL;
    std::jthread worker([&]
    {
        dispatched = invoke->Invoke();
        done.store(true, std::memory_order_release);
    });
    const auto deadline = GetTickCount64() + 12000u;
    while (! done.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        fixture.Pump();
        Sleep(1u);
    }
    Require(done.load(std::memory_order_acquire), "source Invoke owner dispatch completes within the independent deadline");
    worker.join();
    Require(dispatched == (embedded ? RPC_E_WRONG_THREAD : S_OK),
            "native source actions dispatch to their owner; embedded actions reject foreign-thread mutation");
    if (embedded)
        RequireSucceeded(invoke->Invoke(), "owner-thread embedded source entry");
    Require(fixture.editor->GetSemanticSourceRegion() && fixture.editor->GetText() == source && fixture.editor->Revision() == revision &&
                ! fixture.editor->CanUndo() && fixture.controls->GetFocusControl() == focus,
            "source Invoke changes presentation without source/history or focus mutation");
    wil::unique_variant retired;
    Require(image->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE && invoke->Invoke() == UIA_E_ELEMENTNOTAVAILABLE,
            "entry immediately retires old semantic providers before new geometry or a new snapshot is published");
    fixture.Publish();
    auto raw = FindNamedSemanticPart(*simple.get(), fence);
    Require(raw != nullptr, "active source editor has an independently reachable named semantic child");
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> range;
    RequireSucceeded(text->RangeFromChild(raw.get(), range.put()), "raw object source range");
    Require(ReadTextRangeText(*range.get(), -1, "raw source object text") == fence, "raw source UIA range includes exact fences and CRLF");
    pattern.reset();
    RequireSucceeded(raw->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "raw editor finish Invoke pattern");
    Require(pattern != nullptr, "the active source child advertises a reverse presentation action");
    invoke.reset();
    RequireSucceeded(pattern.query_to(invoke.put()), "raw editor finish action");
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(invoke->Invoke() == UIA_E_ELEMENTNOTENABLED, "disabled raw source actions cannot mutate owner presentation");
    fixture.editor->SetEnabled(true);
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    RequireSucceeded(invoke->Invoke(), "read-only source inspection can finish through UIA");
    Require(! fixture.editor->GetSemanticSourceRegion() && fixture.editor->GetText() == source && ! fixture.editor->CanUndo() &&
                raw->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE,
            "finishing source inspection retires its raw provider without rewriting source");
    fixture.Publish();
    image = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    pattern.reset();
    RequireSucceeded(image->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "restored image source pattern");
    invoke.reset();
    RequireSucceeded(pattern.query_to(invoke.put()), "restored image source action");
    RequireSucceeded(invoke->Invoke(), "enabled read-only image permits source inspection");
    fixture.Publish();
    raw           = FindNamedSemanticPart(*simple.get(), fence);
    auto* adapter = static_cast<RichAccessibilityEditor*>(fixture.editor);
    adapter->RebindForTest(nullptr);
    Require(raw->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "detachment immediately retires a source editor provider");
    adapter->RebindForTest(fixture.controls);
    fixture.Publish();
    Require(raw->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "rebinding cannot resurrect old source presentation providers");
}
void SemanticFootnoteNavigation(bool embedded)
{
    using namespace RichText;
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    const std::wstring source = L"[^n]: **body**\n\nmain[^n] again[^n]\n";
    fixture.editor->SetText(source);
    fixture.Publish();
    auto simple    = fixture.Simple();
    auto reference = FindSemanticPart(*simple.get(), UIA_HyperlinkControlTypeId);
    auto backlink1 = FindNamedSemanticPart(*simple.get(), L"Return to reference 1 for footnote 1");
    auto backlink2 = FindNamedSemanticPart(*simple.get(), L"Return to reference 2 for footnote 1");
    Require(reference && backlink1 && backlink2, "both hosts expose independently reachable footnote reference and repeated backlink providers");
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> referenceRange, backlinkRange;
    RequireSucceeded(text->RangeFromChild(reference.get(), referenceRange.put()), "numbered reference raw range");
    Require(ReadTextRangeText(*referenceRange.get(), -1, "raw reference spelling") == L"[^n]", "superscript UIA child retains the exact raw marker");
    RequireSucceeded(text->RangeFromChild(backlink2.get(), backlinkRange.put()), "generated backlink raw range");
    Require(ReadTextRangeText(*backlinkRange.get(), -1, "generated backlink source").empty(), "backlinks invent no raw text or replacement interval");
    wil::com_ptr_nothrow<IRawElementProviderSimple> enclosing;
    RequireSucceeded(backlinkRange->GetEnclosingElement(enclosing.put()), "explicit backlink enclosing identity");
    Require(ReadProviderStringProperty(*enclosing.get(), UIA_NamePropertyId, "explicit backlink identity") == L"Return to reference 2 for footnote 1",
            "equal-ACP generated children retain explicit RangeFromChild identity");
    wil::com_ptr_nothrow<IUnknown> invocation;
    RequireSucceeded(reference->GetPatternProvider(UIA_InvokePatternId, invocation.put()), "footnote Invoke availability");
    wil::com_ptr_nothrow<IInvokeProvider> forward, back1, back2;
    Require(invocation != nullptr, "internal footnote links advertise Invoke");
    RequireSucceeded(invocation.query_to(forward.put()), "forward Invoke");
    RequireSucceeded(backlink1.query_to(back1.put()), "first backlink Invoke");
    RequireSucceeded(backlink2.query_to(back2.put()), "second backlink Invoke");
    const auto revision = fixture.editor->Revision();
    const auto focus    = fixture.controls->GetFocusControl();
    const auto body     = source.find(L"**body**");
    const auto first = source.find(L"[^n]", source.find(L"main")), second = source.find(L"[^n]", first + 1u);
    RequireSucceeded(forward->Invoke(), "forward internal navigation");
    Require(fixture.editor->GetSelection() == Selection{body, body}, "forward navigation enters the exact definition content source");
    RequireSucceeded(back1->Invoke(), "first repeated backlink");
    Require(fixture.editor->GetSelection() == Selection{first, first + 4u}, "first backlink selects its complete original marker");
    RequireSucceeded(back2->Invoke(), "second repeated backlink");
    Require(fixture.editor->GetSelection() == Selection{second, second + 4u}, "second backlink selects its distinct original marker");
    Require(fixture.editor->Revision() == revision && ! fixture.editor->CanUndo() && fixture.controls->GetFocusControl() == focus,
            "UIA navigation preserves source/history and logical focus");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    RequireSucceeded(forward->Invoke(), "read-only footnote navigation");
    fixture.editor->SetReadOnly(false);
    fixture.Publish();
    std::atomic<bool> done = false;
    HRESULT dispatched     = E_FAIL;
    std::jthread worker([&]
    {
        dispatched = back2->Invoke();
        done.store(true, std::memory_order_release);
    });
    const auto deadline = GetTickCount64() + 12000u;
    while (! done.load(std::memory_order_acquire) && GetTickCount64() < deadline)
    {
        fixture.Pump();
        Sleep(1u);
    }
    Require(done.load(std::memory_order_acquire), "footnote Invoke dispatch completes within its independent deadline");
    worker.join();
    Require(dispatched == (embedded ? RPC_E_WRONG_THREAD : S_OK), "native footnote Invoke dispatches to owner while embedded refuses foreign mutation");
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(forward->Invoke() == UIA_E_ELEMENTNOTENABLED, "disabled footnote Invoke is unavailable");
    fixture.editor->SetEnabled(true);
    fixture.Publish();
    UiaTest::Client client(fixture.Hwnd(), {}, [&] { fixture.Pump(); });
    const auto editor = FindEditor(client, client.Root());
    const auto link   = FindClientPart(client, editor, UIA_HyperlinkControlTypeId);
    Require(link && client.TextRangeFromChild(editor, link) == L"[^n]",
            "actual native/application-bridged clients see exact source behind superscript references");
    fixture.editor->SetMarkdownRenderingEnabled(false);
    Require(forward->Invoke() == UIA_E_ELEMENTNOTAVAILABLE && back2->Invoke() == UIA_E_ELEMENTNOTAVAILABLE,
            "presentation withdrawal immediately retires internal navigation providers");
    fixture.editor->SetMarkdownRenderingEnabled(true);
    fixture.Publish();
    reference = FindSemanticPart(*simple.get(), UIA_HyperlinkControlTypeId);
    forward.reset();
    RequireSucceeded(reference.query_to(forward.put()), "navigation after presentation re-enable");
    const auto publication = fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls);
    auto* sourceAdapter    = static_cast<RichAccessibilityEditor*>(fixture.editor);
    sourceAdapter->RebindForTest(nullptr);
    sourceAdapter->RebindForTest(fixture.controls);
    fixture.Publish();
    Require(fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls) == publication && forward->Invoke() == UIA_E_ELEMENTNOTAVAILABLE,
            "owner rebinding retires actions even when immutable CPU footnotes are reused");
    reference = FindSemanticPart(*simple.get(), UIA_HyperlinkControlTypeId);
    backlink2 = FindNamedSemanticPart(*simple.get(), L"Return to reference 2 for footnote 1");
    forward.reset();
    back2.reset();
    RequireSucceeded(reference.query_to(forward.put()), "navigation from rebound owner");
    RequireSucceeded(backlink2.query_to(back2.put()), "backlink from rebound owner");
    fixture.editor->SetText(source + L"changed");
    Require(forward->Invoke() == UIA_E_ELEMENTNOTAVAILABLE && back2->Invoke() == UIA_E_ELEMENTNOTAVAILABLE,
            "source mutation immediately retires old reference and backlink actions before another publication");
    for (const std::wstring_view nested : {L"- [^n]: note\n\nmain[^n]\n", L"> - [^n]: note\n\nmain[^n]\n", L"- outer\n  [^n]: note\n\nmain[^n]\n"})
    {
        fixture.editor->SetText(std::wstring(nested));
        fixture.Publish();
        auto sourceText  = TextProvider(fixture);
        const auto label = nested.find(L"[^n]");
        auto hidden      = AsciiRange(*sourceText.get(), static_cast<int>(label), static_cast<int>(label + 1u));
        wil::com_ptr_nothrow<IRawElementProviderSimple> owner;
        RequireSucceeded(hidden->GetEnclosingElement(owner.put()), "moved definition hidden-label source owner");
        bool inFooter{};
        for (unsigned depth = 0u; owner && depth < 12u; ++depth)
        {
            if (ReadProviderStringProperty(*owner.get(), UIA_NamePropertyId, "moved source ancestry") == L"Footnote 1")
            {
                inFooter = true;
                break;
            }
            wil::com_ptr_nothrow<IRawElementProviderFragment> fragment, parent;
            RequireSucceeded(owner.query_to(fragment.put()), "moved source fragment");
            RequireSucceeded(fragment->Navigate(NavigateDirection_Parent, parent.put()), "moved source semantic parent");
            owner.reset();
            if (parent)
                RequireSucceeded(parent.query_to(owner.put()), "moved source parent provider");
        }
        Require(inFooter, "hidden definition source belongs to the footer tree instead of its former declaration list");
        const auto outer = nested.find(L"outer");
        if (outer != std::wstring_view::npos)
        {
            const auto note = nested.find(L"note");
            auto mixed      = AsciiRange(*sourceText.get(), static_cast<int>(outer), static_cast<int>(note + 1u));
            owner.reset();
            RequireSucceeded(mixed->GetEnclosingElement(owner.put()), "source selection spanning retained list and moved footer");
            Require(ReadProviderLongProperty(*owner.get(), UIA_ControlTypePropertyId, "mixed source common ancestor") == UIA_DocumentControlTypeId,
                    "a source range containing original list text and moved footer text has the document as its common display ancestor");
        }
    }
    const std::wstring overlapping = L"one <a href='#x'>known[^n]</a>\n\n[^n]: note\n";
    fixture.editor->SetText(overlapping);
    fixture.Publish();
    auto literalRoot = fixture.Simple();
    auto onlyLink    = FindSemanticPart(*literalRoot.get(), UIA_HyperlinkControlTypeId);
    Require(onlyLink &&
                ReadProviderStringProperty(*onlyLink.get(), UIA_NamePropertyId, "nonoverlapping fallback action") == L"Return to reference 1 for footnote 1",
            "native/embedded overlapping HTML anchors retain literal source and expose only the footer's independent backlink action");
    wil::com_ptr_nothrow<IInvokeProvider> fallbackBacklink;
    RequireSucceeded(onlyLink.query_to(fallbackBacklink.put()), "literal source backlink Invoke");
    RequireSucceeded(fallbackBacklink->Invoke(), "backlink to literal overlapping anchor source");
    const auto marker = overlapping.find(L"[^n]");
    Require(fixture.editor->GetSelection() == Selection{marker, marker + 4u}, "literal fallback navigation still selects the exact original raw marker");
}
void SemanticStructureAndTasks(bool embedded)
{
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    const std::wstring source = L"# Head\n\n[label](#local) ![image alt](image)\n\n- [ ] task\n\n| A | B | C |\n| --- | --- | --- |\n| value |\n";
    fixture.editor->SetText(source);
    fixture.editor->SetSelection({5u, 2u});
    fixture.Publish();
    auto simple = fixture.Simple();
    auto task   = FindSemanticPart(*simple.get(), UIA_CheckBoxControlTypeId);
    auto link   = FindSemanticPart(*simple.get(), UIA_HyperlinkControlTypeId);
    auto image  = FindSemanticPart(*simple.get(), UIA_ImageControlTypeId);
    auto table  = FindSemanticPart(*simple.get(), UIA_TableControlTypeId);
    Require(task && link && image && table, "native and embedded provider trees contain task/link/image/table structure");
    wil::com_ptr_nothrow<IUnknown> invocation;
    RequireSucceeded(link->GetPatternProvider(UIA_InvokePatternId, invocation.put()), "inert link pattern query");
    Require(! invocation && ReadProviderStringProperty(*image.get(), UIA_NamePropertyId, "image alternative") == L"image alt",
            "link destinations stay inert and images expose their native alternatives");
    wil::com_ptr_nothrow<ITextProvider> text;
    RequireSucceeded(simple.query_to(text.put()), "structured source pattern");
    wil::com_ptr_nothrow<ITextRangeProvider> taskRange, paddedRange;
    RequireSucceeded(text->RangeFromChild(task.get(), taskRange.put()), "task source from child");
    Require(ReadTextRangeText(*taskRange.get(), -1, "task raw range") == L"[ ]", "task child owns exact three-unit marker");
    Require(text->RangeFromChild(simple.get(), paddedRange.put()) == E_INVALIDARG && ! paddedRange, "foreign/nonchild element is rejected");
    wil::com_ptr_nothrow<IGridProvider> grid;
    RequireSucceeded(table.query_to(grid.put()), "semantic table provider");
    int rows = 0, columns = 0;
    RequireSucceeded(grid->get_RowCount(&rows), "semantic table rows");
    RequireSucceeded(grid->get_ColumnCount(&columns), "semantic table columns");
    Require(rows == 2 && columns == 3, "table includes header row and multiple generated padded cells");
    wil::com_ptr_nothrow<IRawElementProviderSimple> padded;
    RequireSucceeded(grid->GetItem(1, 1, padded.put()), "padded semantic table cell");
    wil::com_ptr_nothrow<IRawElementProviderSimple> invalidCell;
    Require(grid->GetItem(-1, 0, invalidCell.put()) == E_INVALIDARG && ! invalidCell, "invalid table coordinate fails");
    RequireSucceeded(text->RangeFromChild(padded.get(), paddedRange.put()), "padded cell source range");
    Require(ReadTextRangeText(*paddedRange.get(), -1, "padded raw source").empty(), "padding adds no invented source units");
    wil::com_ptr_nothrow<IRawElementProviderSimple> enclosing;
    RequireSucceeded(paddedRange->GetEnclosingElement(enclosing.put()), "padded affinity enclosing cell");
    Require(ReadProviderLongProperty(*enclosing.get(), UIA_ControlTypePropertyId, "padded enclosing type") == UIA_TextControlTypeId,
            "explicit RangeFromChild retains cell identity when multiple padded cells share an ACP");
    wil::com_ptr_nothrow<IGridItemProvider> firstEnclosingCell;
    RequireSucceeded(enclosing.query_to(firstEnclosingCell.put()), "padded enclosing grid item");
    int firstColumn = -1;
    RequireSucceeded(firstEnclosingCell->get_Column(&firstColumn), "first padded enclosing column");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondPadded;
    RequireSucceeded(grid->GetItem(1, 2, secondPadded.put()), "second cell at shared raw ACP");
    wil::com_ptr_nothrow<ITextRangeProvider> secondRange, secondClone;
    RequireSucceeded(text->RangeFromChild(secondPadded.get(), secondRange.put()), "second padded raw range");
    RequireSucceeded(secondRange->Clone(secondClone.put()), "clone distinct padded affinity");
    wil::com_ptr_nothrow<IRawElementProviderSimple> secondEnclosing;
    RequireSucceeded(secondClone->GetEnclosingElement(secondEnclosing.put()), "cloned second padded enclosing element");
    wil::com_ptr_nothrow<IGridItemProvider> secondEnclosingCell;
    RequireSucceeded(secondEnclosing.query_to(secondEnclosingCell.put()), "second padded enclosing grid item");
    int secondColumn = -1;
    RequireSucceeded(secondEnclosingCell->get_Column(&secondColumn), "second padded enclosing column");
    Require(firstColumn == 1 && secondColumn == 2 && ReadTextRangeText(*secondClone.get(), -1, "second padded source").empty(),
            "equal-ACP empty ranges and clones retain their distinct explicit child identity");
    wil::com_ptr_nothrow<IGridItemProvider> cell;
    RequireSucceeded(padded.query_to(cell.put()), "semantic table item");
    int row = -1, column = -1;
    RequireSucceeded(cell->get_Row(&row), "padded row coordinate");
    RequireSucceeded(cell->get_Column(&column), "padded column coordinate");
    Require(row == 1 && column == 1, "cell exposes semantic coordinates");
    SAFEARRAY* headers = nullptr;
    wil::com_ptr_nothrow<ITableItemProvider> tableItem;
    RequireSucceeded(padded.query_to(tableItem.put()), "semantic table header relation");
    RequireSucceeded(tableItem->GetColumnHeaderItems(&headers), "padded cell column header");
    Require(headers && headers->rgsabound[0].cElements == 1u, "each padded cell reaches its column header");
    SafeArrayDestroy(headers);
    wil::com_ptr_nothrow<IRawElementProviderFragment> taskFragment;
    RequireSucceeded(task.query_to(taskFragment.put()), "task bounds fragment");
    UiaRect bounds{};
    RequireSucceeded(taskFragment->get_BoundingRectangle(&bounds), "native task painted bounds");
    Require(bounds.width > 0.0 && bounds.height > 0.0, "task bounds follow acknowledged native geometry");
    UiaTest::Client client(fixture.Hwnd(), {.automationEvents = {UIA_Text_TextChangedEventId}}, [&] { fixture.Pump(); });
    const auto editor      = FindEditor(client, client.Root());
    const auto clientTask  = FindClientPart(client, editor, UIA_CheckBoxControlTypeId);
    const auto clientTable = FindClientPart(client, editor, UIA_TableControlTypeId);
    Require(clientTask && clientTable && client.TextRangeFromChild(editor, clientTask) == L"[ ]" &&
                client.TextRangeFromChild(editor, clientTable).find(L"| A | B | C |") != std::wstring::npos,
            "real UIA clients map reachable semantic children back to complete raw ranges");
    const auto all = client.InspectTextRange(editor, {});
    Require(client.Same(all.enclosing, editor) && all.children.size() >= 4u,
            "DocumentRange exposes immediate semantic children and the document enclosing element");
    const auto beforeSelection = fixture.editor->GetSelection();
    const auto mark            = client.Mark();
    RequireSucceeded(client.Toggle(clientTask), "actual native/embedded UIA task toggle");
    fixture.Publish();
    Require(fixture.editor->GetText().find(L"[x] task") != std::wstring::npos && fixture.editor->GetSelection() == beforeSelection && fixture.editor->CanUndo(),
            "task action changes one raw state character and preserves backward selection");
    Require(client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs,
                             [&]
    {
        const auto events = client.DistinctSince(mark);
        return std::ranges::any_of(events, [](const auto& event) { return event.id == UIA_Text_TextChangedEventId && event.automationId == EditorId; });
    }),
            "task action publishes source TextChanged to the reachable client");
    VARIANT retired{};
    Require(task->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "old semantic fragments retire at source publication");
    Require(fixture.editor->Undo() && fixture.editor->GetText() == source && fixture.editor->GetSelection() == beforeSelection,
            "one undo restores exact task spelling and selection");
    fixture.Publish();
    task = FindSemanticPart(*simple.get(), UIA_CheckBoxControlTypeId);
    wil::com_ptr_nothrow<IToggleProvider> toggle;
    RequireSucceeded(task.query_to(toggle.put()), "current task toggle pattern");
    fixture.editor->SetReadOnly(true);
    fixture.Publish();
    Require(toggle->Toggle() == UIA_E_INVALIDOPERATION, "read-only task action is rejected before mutation");
    fixture.editor->SetReadOnly(false);
    fixture.editor->SetEnabled(false);
    fixture.Publish();
    Require(toggle->Toggle() == UIA_E_ELEMENTNOTENABLED && fixture.editor->GetText() == source, "disabled task action preserves exact source");
    fixture.editor->SetEnabled(true);
    fixture.Publish();
    const auto epoch = fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls);
    fixture.editor->SetMarkdownRenderingEnabled(false);
    fixture.Publish();
    fixture.editor->SetMarkdownRenderingEnabled(true);
    fixture.Publish();
    Require(epoch && fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls)->Identity() != epoch->Identity() &&
                toggle->Toggle() == UIA_E_ELEMENTNOTAVAILABLE,
            "same-source rendering restart cannot resurrect old child identity");
    fixture.controls->SetRoot(std::make_unique<RichTextEditor>(L"replacement"));
    fixture.Publish();
    Require(table->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE, "root replacement disconnects structured parts");
}
void SemanticActionComposition(bool nativeAdapter, bool footnote = false)
{
    // A borrowed surface provides prepared semantic geometry without an HWND.
    // Its native backend exercises the real IMM adapter without desktop focus;
    // attached-window IMM deliberately requires OS focus and is a separate gate.
    RichAccessibilityFixture fixture(true, true, true);
    if (nativeAdapter)
    {
        fixture.controls->SetTextInputBackend(TextInputBackend::Native);
        fixture.controls->SetFocusControl(fixture.editor, false);
    }
    fixture.editor->SetMarkdownAnalysisProfile(footnote ? RichText::MarkdownProfile::RichMarkdown : RichText::MarkdownProfile::Gfm029);
    const std::wstring source = footnote ? L"main[^n]\r\n\r\n[^n]: body\r\n" : L"- [ ] task\r\n";
    const auto insertion      = footnote ? 4u : 10u;
    fixture.editor->SetText(source);
    fixture.editor->SetSelection({insertion, insertion});
    fixture.Publish();
    auto simple = fixture.Simple();
    if (! nativeAdapter)
    {
        EmbeddedTextInputSnapshot snapshot;
        RequireSucceeded(fixture.view.ReadTextInput(snapshot), "read semantic task composition base");
        auto preview = snapshot.state;
        preview.text.insert(insertion, L"\u4eee");
        preview.caretIndex = insertion + 1u;
        preview.selectionAnchorIndex.reset();
        preview.compositionStartIndex = insertion;
        preview.compositionEndIndex   = insertion + 1u;
        RequireSucceeded(fixture.view.ApplyTextInput(snapshot.revision, preview, EmbeddedTextInputAction::Preview), "preview task source composition");
    }
    else
    {
        bool handled = false;
        fixture.controls->HandleMessage(nullptr, WM_IME_STARTCOMPOSITION, 0, 0, handled);
        Require(handled, "synthetic native semantic composition starts without taking focus");
        NativeTextInputImePayload payload;
        payload.hasCompositionString = true;
        payload.compositionString    = L"\u4eee";
        fixture.controls->DebugSetNativeTextInputImePayloadForTest(payload);
        fixture.controls->HandleMessage(nullptr, WM_IME_COMPOSITION, 0, GCS_COMPSTR, handled);
        Require(handled, "production native adapter accepts the synthetic source preview");
    }
    fixture.Publish();
    auto action              = FindSemanticPart(*simple.get(), footnote ? UIA_HyperlinkControlTypeId : UIA_CheckBoxControlTypeId);
    const auto previewSource = fixture.editor->GetText();
    HRESULT result{};
    if (footnote)
    {
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        RequireSucceeded(action.query_to(invoke.put()), "composing footnote action");
        result = invoke->Invoke();
    }
    else
    {
        wil::com_ptr_nothrow<IToggleProvider> toggle;
        RequireSucceeded(action.query_to(toggle.put()), "composing task action");
        result = toggle->Toggle();
    }
    Require(result == UIA_E_INVALIDOPERATION && fixture.editor->GetText() == previewSource && ! fixture.editor->CanUndo(),
            "active native/embedded composition blocks semantic task edits and footnote selection changes without history");
    if (! nativeAdapter)
    {
        EmbeddedTextInputSnapshot snapshot;
        RequireSucceeded(fixture.view.ReadTextInput(snapshot), "read cancellation identity");
        RequireSucceeded(fixture.view.ApplyTextInput(snapshot.revision, snapshot.state, EmbeddedTextInputAction::Cancel), "cancel semantic source preview");
    }
    else
    {
        bool handled = false;
        fixture.controls->HandleMessage(nullptr, WM_IME_ENDCOMPOSITION, 0, 0, handled);
        Require(handled, "native source preview cancels through production adapter");
    }
    fixture.Publish();
    Require(fixture.editor->GetText() == source && ! fixture.editor->CanUndo(), "composition cancellation restores exact CRLF task source");
}
void SemanticSourceActionComposition(bool nativeAdapter)
{
    using namespace RichText;
    for (const bool alreadyEditing : {false, true})
    {
        // Native IMM on a borrowed surface uses the production text store
        // without taking OS focus. Physical HWND IME remains a separate gate.
        RichAccessibilityFixture fixture(true, true, true);
        if (nativeAdapter)
        {
            fixture.controls->SetTextInputBackend(TextInputBackend::Native);
            fixture.controls->SetFocusControl(fixture.editor, false);
        }
        fixture.editor->SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
        const std::wstring source = L"```mermaid\r\ngraph TD;A-->B\r\n```\r\n";
        const auto insertion      = source.find(L'A');
        fixture.editor->SetText(source);
        fixture.editor->SetSelection({insertion, insertion});
        fixture.Publish();
        if (alreadyEditing)
        {
            Require(fixture.editor->OnKeyDown(*fixture.controls, VK_F2, 0u), "enter source before composition");
            fixture.editor->SetSelection({insertion, insertion});
            fixture.Publish();
        }
        if (nativeAdapter)
        {
            bool handled = false;
            fixture.controls->HandleMessage(nullptr, WM_IME_STARTCOMPOSITION, 0, 0, handled);
            Require(handled, "native source action composition starts");
            NativeTextInputImePayload payload;
            payload.hasCompositionString = true;
            payload.compositionString    = L"\u4eee";
            fixture.controls->DebugSetNativeTextInputImePayloadForTest(payload);
            fixture.controls->HandleMessage(nullptr, WM_IME_COMPOSITION, 0, GCS_COMPSTR, handled);
            Require(handled, "native source preview uses the production adapter");
        }
        else
        {
            EmbeddedTextInputSnapshot snapshot;
            RequireSucceeded(fixture.view.ReadTextInput(snapshot), "read object source composition base");
            auto preview = snapshot.state;
            preview.text.insert(insertion, L"\u4eee");
            preview.caretIndex = insertion + 1u;
            preview.selectionAnchorIndex.reset();
            preview.compositionStartIndex = insertion;
            preview.compositionEndIndex   = insertion + 1u;
            RequireSucceeded(fixture.view.ApplyTextInput(snapshot.revision, preview, EmbeddedTextInputAction::Preview), "embedded raw object preview");
        }
        fixture.Publish();
        const auto semantic = fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls);
        Require(semantic != nullptr, "composing object retains acknowledged literal/native semantics");
        auto simple = fixture.Simple();
        wil::com_ptr_nothrow<IRawElementProviderSimple> object;
        for (std::size_t i = 0u; i < semantic->Elements().size(); ++i)
            if (semantic->SourceEditingNode(i))
            {
                object = FindNamedSemanticPart(*simple.get(), semantic->Elements()[i].name);
                break;
            }
        Require(object != nullptr, "composing source action remains a reachable fragment");
        wil::com_ptr_nothrow<IUnknown> pattern;
        RequireSucceeded(object->GetPatternProvider(UIA_InvokePatternId, pattern.put()), "composing source action pattern");
        wil::com_ptr_nothrow<IInvokeProvider> invoke;
        RequireSucceeded(pattern.query_to(invoke.put()), "composing source action");
        const auto previewSource = fixture.editor->GetText();
        Require(invoke->Invoke() == UIA_E_INVALIDOPERATION && ! fixture.editor->OnKeyDown(*fixture.controls, VK_F2, 0u) &&
                    fixture.editor->GetSemanticSourceRegion().has_value() == alreadyEditing && fixture.editor->GetText() == previewSource &&
                    ! fixture.editor->CanUndo(),
                "active native/embedded composition blocks both source entry and exit without source/history mutation");
        if (nativeAdapter)
        {
            bool handled = false;
            fixture.controls->HandleMessage(nullptr, WM_IME_ENDCOMPOSITION, 0, 0, handled);
            Require(handled, "native object source preview cancels");
        }
        else
        {
            EmbeddedTextInputSnapshot snapshot;
            RequireSucceeded(fixture.view.ReadTextInput(snapshot), "read object cancellation identity");
            RequireSucceeded(fixture.view.ApplyTextInput(snapshot.revision, snapshot.state, EmbeddedTextInputAction::Cancel),
                             "cancel embedded object source preview");
        }
        fixture.Publish();
        Require(fixture.editor->GetText() == source && ! fixture.editor->CanUndo() && fixture.editor->GetSemanticSourceRegion().has_value() == alreadyEditing,
                "composition cancellation restores exact source and its prior presentation session");
        Require(fixture.editor->OnKeyDown(*fixture.controls, VK_F2, 0u), "source presentation action works again after composition cancellation");
    }
}
void SemanticCompositionBindingAndRetirement(bool embedded)
{
    SemanticActionComposition(! embedded);
    SemanticActionComposition(! embedded, true);
    // This fixture separately qualifies the native HWND/application-bridged
    // semantic provider lifetime. Neither fixture takes desktop focus.
    RichAccessibilityFixture fixture(embedded, true, true);
    fixture.editor->SetMarkdownAnalysisProfile(RichText::MarkdownProfile::Gfm029);
    fixture.editor->SetText(L"- [ ] task\r\n");
    fixture.Publish();
    auto simple = fixture.Simple();
    auto task   = FindSemanticPart(*simple.get(), UIA_CheckBoxControlTypeId);
    wil::com_ptr_nothrow<IToggleProvider> toggle;
    RequireSucceeded(task.query_to(toggle.put()), "task before owner binding change");
    const auto publication = fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls);
    auto* editor           = static_cast<RichAccessibilityEditor*>(fixture.editor);
    editor->RebindForTest(nullptr);
    editor->RebindForTest(fixture.controls);
    fixture.Publish();
    Require(fixture.editor->GetAcknowledgedSemanticDocument(*fixture.controls) == publication && toggle->Toggle() == UIA_E_ELEMENTNOTAVAILABLE,
            "same-path same-source owner rebinding retires old fragments even when immutable CPU semantics are reused");
    task = FindSemanticPart(*simple.get(), UIA_CheckBoxControlTypeId);
    toggle.reset();
    RequireSucceeded(task.query_to(toggle.put()), "task from current owner binding");
    size_t notices = 0u;
    std::wstring observed;
    fixture.editor->SetOnTextChanged([&]
    {
        ++notices;
        observed = fixture.editor->GetText();
        fixture.controls->SetRoot(std::make_unique<Label>(L"retired by task callback"));
    });
    RequireSucceeded(toggle->Toggle(), "task callback may destroy its editor after commit");
    fixture.Publish();
    VARIANT retired{};
    Require(notices == 1u && observed == L"- [x] task\r\n" && task->GetPropertyValue(UIA_NamePropertyId, &retired) == UIA_E_ELEMENTNOTAVAILABLE,
            "reentrant destruction leaves one exact committed source edit and disconnects the old provider");
}
void TestRichTextAccessibilityWindowSemanticCompositionBindingAndRetirement()
{
    // The rootless native IMM probe also uses the application UIA bridge, which
    // requires an STA. The lifetime half still attaches an actual native HWND.
    EmbeddedCase([](bool) { SemanticCompositionBindingAndRetirement(false); });
}
void TestRichTextAccessibilityEmbeddedSemanticCompositionBindingAndRetirement()
{
    EmbeddedCase(SemanticCompositionBindingAndRetirement);
}
void TestRichTextAccessibilityWindowSemanticAttributesAndFormats()
{
    SemanticAttributesAndFormats(false);
}
void TestRichTextAccessibilityEmbeddedSemanticAttributesAndFormats()
{
    EmbeddedCase(SemanticAttributesAndFormats);
}
void TestRichTextAccessibilityWindowSemanticStructureAndTasks()
{
    SemanticStructureAndTasks(false);
}
void TestRichTextAccessibilityEmbeddedSemanticStructureAndTasks()
{
    EmbeddedCase(SemanticStructureAndTasks);
}
void TestRichTextAccessibilityWindowSemanticFrontMatterDiagnostics()
{
    SemanticFrontMatterDiagnostics(false);
}
void TestRichTextAccessibilityEmbeddedSemanticFrontMatterDiagnostics()
{
    EmbeddedCase(SemanticFrontMatterDiagnostics);
}
void TestRichTextAccessibilityWindowDiagramAlternatives()
{
    SemanticDiagramAlternatives(false);
}
void TestRichTextAccessibilityWindowDecodedImages()
{
    SemanticDecodedImages(false);
}
void TestRichTextAccessibilityEmbeddedDecodedImages()
{
    EmbeddedCase(SemanticDecodedImages);
}
void TestRichTextAccessibilityEmbeddedDiagramAlternatives()
{
    EmbeddedCase(SemanticDiagramAlternatives);
}
void TestRichTextAccessibilityWindowObjectSourceEditing()
{
    SemanticObjectSourceEditing(false);
}
void TestRichTextAccessibilityEmbeddedObjectSourceEditing()
{
    EmbeddedCase(SemanticObjectSourceEditing);
}
void TestRichTextAccessibilityWindowSourceEditingComposition()
{
    EmbeddedCase([](bool) { SemanticSourceActionComposition(true); });
}
void TestRichTextAccessibilityEmbeddedSourceEditingComposition()
{
    EmbeddedCase([](bool) { SemanticSourceActionComposition(false); });
}
void TestRichTextAccessibilityWindowFootnoteNavigation()
{
    SemanticFootnoteNavigation(false);
}
void TestRichTextAccessibilityEmbeddedFootnoteNavigation()
{
    EmbeddedCase(SemanticFootnoteNavigation);
}
} // namespace

void RunRichTextAccessibilityTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowPresentationAndLinkCues);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedPresentationAndLinkCues);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSelectionNotifications);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSelectionNotifications);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowAcknowledgementIsPerEditor);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedAcknowledgementIsPerEditor);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSemanticAttributesAndFormats);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSemanticCompositionBindingAndRetirement);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSemanticCompositionBindingAndRetirement);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSemanticAttributesAndFormats);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSemanticStructureAndTasks);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSemanticStructureAndTasks);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSemanticFrontMatterDiagnostics);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSemanticFrontMatterDiagnostics);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowFootnoteNavigation);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowDiagramAlternatives);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowDecodedImages);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedDecodedImages);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowObjectSourceEditing);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedObjectSourceEditing);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceEditingComposition);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceEditingComposition);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedDiagramAlternatives);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedFootnoteNavigation);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowClientNavigationAndEvents);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedClientNavigationAndEvents);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceTransactionClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceTransactionClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSemanticRenderingClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSemanticRenderingClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceCaretSitesClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowNativeHtmlAndMarkersClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedNativeHtmlAndMarkersClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceCaretSitesClient);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSelectionCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSelectionCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSearchCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSearchCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowScrollingCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedScrollingCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowRemainingRangeCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedRemainingRangeCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowThreadedRangeCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedThreadedRangeCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowUnicode17Commands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedUnicode17Commands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowInteriorAcpDeletion);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedInteriorAcpDeletion);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowBulkCharacterCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedBulkCharacterCommands);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowPendingPreparationGeometry);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedPendingPreparationGeometry);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowMarkdownSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedMarkdownSourceAndRanges);
}
