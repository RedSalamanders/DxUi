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
} // namespace

void RunRichTextAccessibilityTests()
{
    ScopedNonActivatingTestWindows nonActivating;
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedSourceAndRanges);
    DXUI_RUN_TEST(TestRichTextAccessibilityWindowClientNavigationAndEvents);
    DXUI_RUN_TEST(TestRichTextAccessibilityEmbeddedClientNavigationAndEvents);
}
