#include "Scope.Tests.RichTextAccessibility.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../Embedded/Embedded.Tests.EmbeddedUiaBridge.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <thread>

namespace
{
using namespace DxUi;
constexpr std::wstring_view EditorId = L"rich.source";

struct RichAccessibilityFixture
{
    explicit RichAccessibilityFixture(bool embedded) : embedded(embedded)
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
        auto root = std::make_unique<RichTextEditor>(L"**alpha**\r\nbeta");
        editor    = root.get();
        editor->SetAccessibleName(L"Markdown editor");
        editor->SetAccessibleAutomationId(std::wstring(EditorId));
        controls->SetRoot(std::move(root));
        if (embedded)
        {
            RequireSucceeded(view.Prepare(480, 240), "prepare rich UIA view");
            bridge = std::make_unique<EmbeddedUiaTest::Bridge>(view, 480, 240);
            RequireSucceeded(bridge->Attach(false, 0x524943480001ull), "attach rich application accessibility bridge");
            RequireSucceeded(view.GetAccessibilityProvider(provider.put()), "embedded rich root provider");
        }
        else
        {
            Require(controls->RenderInitialFrameForShow(), "prepare native rich UIA pixels");
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
            RequireSucceeded(view.Prepare(480, 240), "prepare rich source publication");
            RequireSucceeded(bridge->Update(), "publish rich embedded semantics");
        }
        else
        {
            Require(controls->RenderInitialFrameForShow(), "prepare native rich source publication");
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

void SourceAndRanges(bool embedded)
{
    RichAccessibilityFixture fixture(embedded);
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
    auto text = TextProvider(fixture);
    wil::com_ptr_nothrow<ITextRangeProvider> document;
    RequireSucceeded(text->get_DocumentRange(document.put()), "scroll document");
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
    EmbeddedCase(SourceAndRanges);
}
void TestRichTextAccessibilityWindowClientNavigationAndEvents()
{
    ClientNavigationAndEvents(false);
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
} // namespace

void RunRichTextAccessibilityTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowClientNavigationAndEvents);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedClientNavigationAndEvents);
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
}
