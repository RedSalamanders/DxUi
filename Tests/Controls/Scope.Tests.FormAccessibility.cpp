#include "Scope.Tests.FormAccessibility.h"

#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "../Embedded/Embedded.Tests.EmbeddedUiaBridge.h"
#include "../Support/Support.Tests.UiaTestClient.h"
#include "Controls.Tests.DxUiTestHelpers.h"

#include <thread>

namespace
{
constexpr std::wstring_view kFieldId     = L"canvas.width";
constexpr std::wstring_view kFieldLabel  = L"Largeur du dessin \u2014 \u5e45";
constexpr std::wstring_view kDescription = L"Largeur du dessin en pixels.";
constexpr std::wstring_view kError       = L"Saisissez une largeur comprise entre 1 et 4096 pixels.";
constexpr UINT kWidth                    = 480u;
constexpr UINT kHeight                   = 360u;

enum class Hosting
{
    Window,
    Embedded,
};

// The standalone control runner may already have an MTA for WIC. An application's embedded bridge owns a pumping STA,
// so give each embedded case its own apartment and activation guard rather than inheriting earlier tests' COM state.
void RunEmbeddedFormTest(void (*test)(Hosting))
{
    std::jthread owner([test]()
    {
        RequireSucceeded(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "initialize embedded form's application STA");
        const auto apartment = wil::scope_exit([]() noexcept { CoUninitialize(); });
        DxUi::TestSupport::ScopedWindowActivationBlocker activation;
        Require(activation.Start(), "embedded form's application STA refuses activation");
        test(Hosting::Embedded);
    });
}

[[nodiscard]] bool ReadBool(IRawElementProviderSimple& provider, PROPERTYID property)
{
    VARIANT value{};
    const auto clear = wil::scope_exit([&]() noexcept { VariantClear(&value); });
    RequireSucceeded(provider.GetPropertyValue(property, &value), "form exposes a boolean UIA property");
    Require(value.vt == VT_BOOL, "form boolean property has the UIA boolean type");
    return value.boolVal != VARIANT_FALSE;
}

[[nodiscard]] wil::com_ptr_nothrow<IRawElementProviderSimple> ReadLabel(IRawElementProviderSimple& provider)
{
    VARIANT value{};
    const auto clear = wil::scope_exit([&]() noexcept { VariantClear(&value); });
    RequireSucceeded(provider.GetPropertyValue(UIA_LabeledByPropertyId, &value), "read form LabeledBy relationship");
    wil::com_ptr_nothrow<IRawElementProviderSimple> label;
    if (value.vt != VT_EMPTY)
    {
        Require(value.vt == VT_UNKNOWN && value.punkVal, "LabeledBy is an element provider or an empty relationship");
        RequireSucceeded(value.punkVal->QueryInterface(IID_PPV_ARGS(label.put())), "LabeledBy supplies a simple provider");
    }
    return label;
}

[[nodiscard]] std::vector<LONG> RuntimeId(IRawElementProviderSimple& provider)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
    RequireSucceeded(provider.QueryInterface(IID_PPV_ARGS(fragment.put())), "form element supports Fragment");
    SAFEARRAY* array = nullptr;
    RequireSucceeded(fragment->GetRuntimeId(&array), "form element exposes a runtime identity");
    const auto destroy = wil::scope_exit([&]() noexcept
    {
        if (array)
            SafeArrayDestroy(array);
    });
    Require(array != nullptr, "form runtime identity has elements");
    LONG first = 0, last = -1;
    RequireSucceeded(SafeArrayGetLBound(array, 1, &first), "form runtime identity lower bound");
    RequireSucceeded(SafeArrayGetUBound(array, 1, &last), "form runtime identity upper bound");
    std::vector<LONG> result;
    for (LONG index = first; index <= last; ++index)
    {
        LONG part = 0;
        RequireSucceeded(SafeArrayGetElement(array, &index, &part), "form runtime identity part");
        result.push_back(part);
    }
    return result;
}

void RequireRetired(IRawElementProviderSimple& provider)
{
    VARIANT value{};
    const auto clear = wil::scope_exit([&]() noexcept { VariantClear(&value); });
    Require(provider.GetPropertyValue(UIA_NamePropertyId, &value) == UIA_E_ELEMENTNOTAVAILABLE,
            "a retained label/editor provider never reaches a detached or replacement control");
}

struct FormFixture final
{
    explicit FormFixture(Hosting hosting) : _hosting(hosting)
    {
        if (hosting == Hosting::Window)
        {
            window   = std::make_unique<AttachedHostWindow>();
            controls = &window->Host();
        }
        else
        {
            RequireSucceeded(graphics.Create(), "create WARP device for embedded form provider tests");
            std::shared_ptr<DxUi::GraphicsDevice> pool;
            RequireSucceeded(DxUi::GraphicsDevice::Create(graphics.device.get(), pool), "create embedded form graphics pool");
            RequireSucceeded(view.Attach(std::move(pool)), "attach embedded form to borrowed graphics");
            controls = &view.Controls();
        }
        auto root = std::make_unique<DxUi::Panel>();
        form      = root->AddChild<DxUi::FormPanel>();
        form->SetBounds(D2D1::RectF(12.0f, 12.0f, 460.0f, 300.0f));
        field = form->AddField(std::wstring(kFieldId), std::wstring(kFieldLabel));
        Require(field != nullptr, "form owns an identified width field");
        stepper = field->AddEditor<DxUi::NumericStepper>();
        Require(stepper != nullptr, "form owns its stable NumericStepper editor");
        stepper->SetValue(12.0);
        field->SetDescription(std::wstring(kDescription));
        auto* height = form->AddField(L"canvas.height", L"Hauteur du dessin");
        Require(height && height->AddEditor<DxUi::TextField>(), "form owns a second field for column reflow");
        externalLabels = root->AddChild<DxUi::Panel>();
        controls->SetRoot(std::move(root));
        RequireSucceeded(form->ApplyLayout(*controls), "arrange form before accessibility attachment");
        if (hosting == Hosting::Window)
        {
            provider.attach(DxUi::CreateWindowHostAccessibilityProvider(window->Hwnd()));
            Require(provider != nullptr, "window form has an accessibility root");
        }
        else
        {
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare embedded form before accessibility attachment");
            bridge = std::make_unique<EmbeddedUiaTest::Bridge>(view, kWidth, kHeight);
            RequireSucceeded(bridge->Attach(false, 0x464F524D0001ull), "attach embedded form's real application UIA bridge");
            RequireSucceeded(view.GetAccessibilityProvider(provider.put()), "embedded form has an accessibility root");
        }
        Publish();
    }

    ~FormFixture()
    {
        provider.reset();
        bridge.reset();
        view.Detach();
    }

    FormFixture(const FormFixture&)            = delete;
    FormFixture& operator=(const FormFixture&) = delete;

    void Publish()
    {
        RequireSucceeded(form->ApplyLayout(*controls), "prepare current form bounds");
        if (_hosting == Hosting::Window)
            controls->RefreshAccessibilitySnapshot();
        else
        {
            RequireSucceeded(view.Prepare(kWidth, kHeight, 96.0f), "prepare embedded form mutation");
            RequireSucceeded(bridge->Update(), "publish embedded form mutation");
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

    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return window ? window->Hwnd() : bridge->Hwnd();
    }

    [[nodiscard]] DxUi::TextField& Editor() const noexcept
    {
        return stepper->Field();
    }

    [[nodiscard]] wil::com_ptr_nothrow<IRawElementProviderSimple> EditorProvider()
    {
        const auto bounds = Editor().GetBounds();
        const float x     = (bounds.left + bounds.right) * 0.5f;
        const float y     = (bounds.top + bounds.bottom) * 0.5f;
        wil::com_ptr_nothrow<IRawElementProviderFragment> fragment;
        if (window)
            fragment = GetProviderAtDipPoint(window->Hwnd(), window->Host(), *provider.get(), x, y, "window form editor point provider");
        else
            RequireSucceeded(provider->ElementProviderFromPoint(-32000.0 + x, -32000.0 + y, fragment.put()), "embedded form editor point provider");
        Require(fragment != nullptr, "form editor has a hit-testable provider");
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(fragment.query_to(simple.put()), "form semantic editor simple provider");
        Require(ReadProviderLongProperty(*simple.get(), UIA_ControlTypePropertyId, "form semantic editor type") == UIA_EditControlTypeId,
                "NumericStepper semantic metadata belongs to Field, not its panel or step buttons");
        return simple;
    }

    [[nodiscard]] DxUi::Label* AddExternalLabel(std::wstring text)
    {
        auto* label = externalLabels->AddChild<DxUi::Label>(std::move(text));
        label->SetBounds(D2D1::RectF(12.0f, 310.0f, 360.0f, 342.0f));
        return label;
    }

    GraphicsFixture graphics;
    DxUi::EmbeddedHost view;
    std::unique_ptr<AttachedHostWindow> window;
    std::unique_ptr<EmbeddedUiaTest::Bridge> bridge;
    DxUi::ControlHost* controls   = nullptr;
    DxUi::FormPanel* form         = nullptr;
    DxUi::FormField* field        = nullptr;
    DxUi::NumericStepper* stepper = nullptr;
    DxUi::Panel* externalLabels   = nullptr;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    Hosting _hosting;
};

[[nodiscard]] UiaTest::Subscription FormSubscription()
{
    return {.properties = {
                UIA_HelpTextPropertyId, UIA_ItemStatusPropertyId, UIA_IsRequiredForFormPropertyId, UIA_IsDataValidForFormPropertyId, UIA_LabeledByPropertyId}};
}

[[nodiscard]] bool IsFieldEvent(const UiaTest::HeardEvent& event)
{
    return event.kind == UiaTest::EventKind::Property && event.automationId == kFieldId;
}

[[nodiscard]] std::vector<UiaTest::HeardEvent> FieldEvents(const UiaTest::Client& client, size_t mark)
{
    auto events = client.DistinctSince(mark);
    std::erase_if(events, [](const auto& event) noexcept { return ! IsFieldEvent(event); });
    return events;
}

void ExpectEvents(UiaTest::Client& client, size_t mark, const std::vector<std::pair<PROPERTYID, std::wstring>>& expected)
{
    Require(client.WaitUntil(UiaTest::Client::kNotificationDeadlineMs, [&]() { return FieldEvents(client, mark).size() >= expected.size(); }),
            "real UIA client hears each form presentation change");
    client.WaitOutTheStream();
    const auto events = FieldEvents(client, mark);
    if (events.size() != expected.size())
        client.PrintEvents();
    Require(events.size() == expected.size(), "form publication sends exactly the changed presentation properties");
    for (const auto& [property, value] : expected)
        Require(std::ranges::any_of(
                    events, [&](const auto& event) noexcept { return event.id == property && (property == UIA_LabeledByPropertyId || event.value == value); }),
                "form event carries its committed boolean/text value");
}

[[nodiscard]] UiaTest::ElementId FindClientField(UiaTest::Client& client, UiaTest::ElementId parent, size_t depth = 0u)
{
    Require(depth < 8u, "form UIA client walk is bounded");
    for (const auto child : client.Children(parent))
    {
        if (client.Describe(child).automationId == kFieldId)
            return child;
        if (depth + 1u < 8u)
            if (const auto found = FindClientField(client, child, depth + 1u))
                return found;
    }
    return 0u;
}

void PropertiesAndStableEditor(Hosting hosting)
{
    FormFixture fixture(hosting);
    auto simple = fixture.EditorProvider();
    auto label  = ReadLabel(*simple.get());
    Require(label != nullptr && ReadProviderStringProperty(*label.get(), UIA_NamePropertyId, "form label name") == kFieldLabel,
            "field label is a real label provider");
    Require(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "form editor name") == kFieldLabel, "field label names its semantic editor");
    Require(ReadProviderStringProperty(*simple.get(), UIA_AutomationIdPropertyId, "form editor id") == kFieldId, "field id reaches the stable semantic editor");
    Require(ReadProviderStringProperty(*simple.get(), UIA_HelpTextPropertyId, "form description") == kDescription, "description is editor HelpText");
    Require(! ReadBool(*simple.get(), UIA_IsRequiredForFormPropertyId) && ReadBool(*simple.get(), UIA_IsDataValidForFormPropertyId),
            "optional unvalidated field starts valid");
    RequireSucceeded(fixture.field->SetRequired(true), "present required form field");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::Error, std::wstring(kError)), "present form validation error");
    fixture.Publish();
    Require(ReadBool(*simple.get(), UIA_IsRequiredForFormPropertyId) && ! ReadBool(*simple.get(), UIA_IsDataValidForFormPropertyId),
            "required and invalid states describe the displayed editor");
    Require(ReadProviderStringProperty(*simple.get(), UIA_ItemStatusPropertyId, "form validation explanation") == kError,
            "form validation has a readable UIA explanation");
    Require(fixture.field->SetValidation(DxUi::ValidationState::Error, {}) == E_INVALIDARG, "an unexplained error is rejected");
    Require(fixture.field->SetValidation(static_cast<DxUi::ValidationState>(255), L"bad") == E_INVALIDARG, "unknown validation states are rejected");
    Require(fixture.field->SetValidation(DxUi::ValidationState::Error, std::wstring(65537u, L'x')) == E_INVALIDARG,
            "validation explanation has a finite length bound");
    fixture.Publish();
    Require(! ReadBool(*simple.get(), UIA_IsDataValidForFormPropertyId) &&
                ReadProviderStringProperty(*simple.get(), UIA_ItemStatusPropertyId, "unchanged form validation") == kError,
            "invalid presentation input leaves the previous complete UIA state");
    const auto identity  = RuntimeId(*simple.get());
    auto* const semantic = &fixture.Editor();
    fixture.Editor().SetText(L"0012");
    fixture.Editor().SetSelectionRange(1u, 3u);
    const auto selection = fixture.Editor().GetSelectionRange();
    RequireSucceeded(fixture.form->SetColumns(2u), "reflow form columns");
    RequireSucceeded(fixture.form->SetLabelPlacement(DxUi::LabelPlacement::Beside), "reflow form labels beside editors");
    fixture.Publish();
    Require(fixture.field->GetEditor() == fixture.stepper && fixture.field->GetSemanticEditor() == semantic && &fixture.stepper->Field() == semantic &&
                fixture.Editor().GetText() == L"0012" && fixture.Editor().GetSelectionRange() == selection,
            "column and label reflow preserve editor ownership, draft and selection");
    auto after = fixture.EditorProvider();
    Require(RuntimeId(*after.get()) == identity && RuntimeId(*simple.get()) == identity,
            "new and retained providers keep the same semantic editor identity through reflow");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::None, {}), "clear validation presentation");
    RequireSucceeded(fixture.field->SetRequired(false), "clear required presentation");
    fixture.field->SetDescription({});
    fixture.Publish();
    Require(ReadBool(*simple.get(), UIA_IsDataValidForFormPropertyId) && ! ReadBool(*simple.get(), UIA_IsRequiredForFormPropertyId) &&
                ReadProviderStringProperty(*simple.get(), UIA_HelpTextPropertyId, "cleared help").empty() &&
                ReadProviderStringProperty(*simple.get(), UIA_ItemStatusPropertyId, "cleared status").empty(),
            "cleared presentation removes help/status and restores accurate optional valid state");
}

void LabelLifetimeAndReplacement(Hosting hosting)
{
    FormFixture fixture(hosting);
    auto simple = fixture.EditorProvider();
    fixture.Editor().SetAccessibleName(L"Canvas width editor");
    auto* label = fixture.AddExternalLabel(L"External width label");
    RequireSucceeded(fixture.Editor().SetAccessibleFormProperties(label, false, true), "link external label without changing ownership");
    fixture.Publish();
    auto original = ReadLabel(*simple.get());
    Require(original != nullptr, "generic form metadata resolves a label in the same visible tree");
    const auto identity = RuntimeId(*original.get());
    UiaTest::Client client(fixture.Hwnd(), FormSubscription(), [&]() { fixture.Pump(); });
    const auto clientField = FindClientField(client, client.Root());
    Require(clientField != 0u, "real client reaches the field through its application window");
    client.WaitOutTheStream();
    auto mark = client.Mark();
    label->SetVisible(false);
    fixture.Publish();
    ExpectEvents(client, mark, {{UIA_LabeledByPropertyId, {}}});
    Require(! ReadLabel(*simple.get()), "hidden label has no effective LabeledBy relationship");
    RequireRetired(*original.get());
    mark = client.Mark();
    label->SetVisible(true);
    fixture.Publish();
    ExpectEvents(client, mark, {{UIA_LabeledByPropertyId, {}}});
    Require(ReadLabel(*simple.get()) != nullptr, "showing the same label restores the relationship");
    mark          = client.Mark();
    auto detached = fixture.externalLabels->TakeChild(0u);
    Require(detached.get() == label && fixture.Editor().GetAccessibleLabelledBy() == label,
            "weak presentation reference can outlive ownership without dereferencing a detached control");
    fixture.Publish();
    ExpectEvents(client, mark, {{UIA_LabeledByPropertyId, {}}});
    Require(! ReadLabel(*simple.get()), "live detached label is absent from the immutable visible tree");
    RequireRetired(*original.get());
    fixture.externalLabels->ClearChildren();
    auto* replacement = fixture.AddExternalLabel(L"Replacement width label");
    mark              = client.Mark();
    RequireSucceeded(fixture.Editor().SetAccessibleFormProperties(replacement, false, true), "link replacement label at the original path");
    fixture.Publish();
    ExpectEvents(client, mark, {{UIA_LabeledByPropertyId, {}}});
    auto current = ReadLabel(*simple.get());
    Require(current && RuntimeId(*current.get()) != identity &&
                ReadProviderStringProperty(*current.get(), UIA_NamePropertyId, "replacement label name") == L"Replacement width label",
            "same-path replacement label has its own runtime identity");
    RequireRetired(*original.get());
    mark         = client.Mark();
    auto removed = fixture.externalLabels->TakeChild(0u);
    Require(removed.get() == replacement, "detach replacement before destroying it");
    removed.reset();
    fixture.Publish();
    ExpectEvents(client, mark, {{UIA_LabeledByPropertyId, {}}});
    Require(! fixture.Editor().GetAccessibleLabelledBy() && ! ReadLabel(*simple.get()), "expired weak label has no pointer or provider relationship");
    RequireRetired(*current.get());
    detached.reset();
}

void PresentationEventsAreIdempotent(Hosting hosting)
{
    FormFixture fixture(hosting);
    UiaTest::Client client(fixture.Hwnd(), FormSubscription(), [&]() { fixture.Pump(); });
    Require(FindClientField(client, client.Root()) != 0u, "real UIA client walks to form semantic editor");
    client.WaitOutTheStream();
    auto mark = client.Mark();
    fixture.field->SetDescription(L"Largeur finale du dessin.");
    RequireSucceeded(fixture.field->SetRequired(true), "change form required state");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::Error, std::wstring(kError)), "change form validation state");
    fixture.Publish();
    ExpectEvents(client,
                 mark,
                 {{UIA_HelpTextPropertyId, L"Largeur finale du dessin."},
                  {UIA_IsRequiredForFormPropertyId, L"true"},
                  {UIA_IsDataValidForFormPropertyId, L"false"},
                  {UIA_ItemStatusPropertyId, std::wstring(kError)}});
    mark = client.Mark();
    fixture.field->SetDescription(L"Largeur finale du dessin.");
    RequireSucceeded(fixture.field->SetRequired(true), "repeat required state");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::Error, std::wstring(kError)), "repeat validation state");
    fixture.Publish();
    client.WaitOutTheStream();
    Require(FieldEvents(client, mark).empty(), "repeated unchanged presentation raises no UIA property events");
    mark = client.Mark();
    fixture.field->SetDescription({});
    RequireSucceeded(fixture.field->SetRequired(false), "restore optional state");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::None, {}), "restore valid state");
    fixture.Publish();
    ExpectEvents(client,
                 mark,
                 {{UIA_HelpTextPropertyId, {}},
                  {UIA_IsRequiredForFormPropertyId, L"false"},
                  {UIA_IsDataValidForFormPropertyId, L"true"},
                  {UIA_ItemStatusPropertyId, {}}});
}

struct PropertyInterruption final
{
    explicit PropertyInterruption(std::function<void()> callback) : action(std::move(callback))
    {
        DxUi::DebugSetAccessibilityPropertyEventHookForTest(&OnEvent, this);
    }
    ~PropertyInterruption()
    {
        DxUi::DebugSetAccessibilityPropertyEventHookForTest(nullptr, nullptr);
    }
    PropertyInterruption(const PropertyInterruption&)            = delete;
    PropertyInterruption& operator=(const PropertyInterruption&) = delete;

    static void OnEvent(long property, void* context) noexcept
    {
        auto& self = *static_cast<PropertyInterruption*>(context);
        // The description Label legitimately changes its Name before the semantic editor's HelpText. Retire from the
        // editor's owner-thread publication hook, then count every later property so none of the discarded
        // snapshot can slip through. Native notification is synchronous; embedded notification is queued here.
        if (self.calls == 0u && property != UIA_HelpTextPropertyId)
            return;
        if (++self.calls == 1u)
        {
            self.firstProperty = property;
            self.action();
        }
    }

    std::function<void()> action;
    size_t calls       = 0u;
    long firstProperty = 0;
};

enum class Retirement
{
    Root,
    Editor,
    RetainedEditor,
    HiddenField,
};

void PublicationStopsAfterDestructiveCallback(Hosting hosting, Retirement retirement)
{
    FormFixture fixture(hosting);
    auto simple = fixture.EditorProvider();
    wil::com_ptr_nothrow<IValueProvider> retainedValue;
    if (retirement == Retirement::RetainedEditor)
    {
        wil::com_ptr_nothrow<IUnknown> pattern;
        RequireSucceeded(simple->GetPatternProvider(UIA_ValuePatternId, pattern.put()), "retain editor value pattern before its notification");
        Require(pattern != nullptr, "retained text editor value provider exists");
        RequireSucceeded(pattern.query_to(retainedValue.put()), "retain editor value pattern interface");
    }
    auto subscription             = FormSubscription();
    subscription.automationEvents = {UIA_Text_TextChangedEventId, UIA_Text_TextSelectionChangedEventId};
    UiaTest::Client client(fixture.Hwnd(), std::move(subscription), [&]() { fixture.Pump(); });
    Require(FindClientField(client, client.Root()) != 0u, "client subscribes to form before destructive publication");
    client.WaitOutTheStream();
    const auto mark = client.Mark();
    std::unique_ptr<DxUi::Control> retainedEditor;
    fixture.Editor().SetText(L"retained draft");
    fixture.Editor().SetSelectionRange(2u, 6u);
    const std::wstring draft(fixture.Editor().GetText());
    fixture.field->SetDescription(L"Description that retires the form.");
    RequireSucceeded(fixture.field->SetRequired(true), "queue required property after help");
    RequireSucceeded(fixture.field->SetValidation(DxUi::ValidationState::Error, std::wstring(kError)), "queue error properties after help");
    PropertyInterruption interruption([&]()
    {
        if (retirement == Retirement::Editor)
            fixture.field->ClearChildren();
        else if (retirement == Retirement::RetainedEditor)
        {
            for (size_t index = 0u; index < fixture.field->GetLogicalChildCount(); ++index)
                if (fixture.field->GetLogicalChild(index) == fixture.stepper)
                {
                    retainedEditor = fixture.field->TakeChild(index);
                    break;
                }
            Require(retainedEditor.get() == fixture.stepper, "notification detaches and retains the semantic editor subtree");
        }
        else if (retirement == Retirement::HiddenField)
            fixture.field->SetVisible(false);
        else
            fixture.controls->SetRoot(std::make_unique<DxUi::Panel>());
    });
    fixture.Publish();
    Require(interruption.calls == 1u && interruption.firstProperty == UIA_HelpTextPropertyId,
            "owner-thread retirement after first property publication stops later notifications from the discarded snapshot");
    if (hosting == Hosting::Window)
        ExpectEvents(client, mark, {{UIA_HelpTextPropertyId, L"Description that retires the form."}});
    else
    {
        // The first HelpText was queued before this owner hook retired its source. A delivery already begun
        // can reach the client; a later delivery can discard the unavailable sender. Only this one attempted
        // event is permitted. Delivered-callback reentry is covered by the real embedded MTA client fixture.
        client.WaitOutTheStream();
        const auto events = FieldEvents(client, mark);
        Require(events.size() <= 1u && std::ranges::all_of(events,
                                                           [](const auto& event) noexcept
        { return event.id == UIA_HelpTextPropertyId && event.value == L"Description that retires the form."; }),
                "queue-time retirement delivers at most the already attempted HelpText and no later form property");
    }
    Require(std::ranges::none_of(client.DistinctSince(mark),
                                 [](const auto& event) noexcept
    {
        return event.kind == UiaTest::EventKind::Automation && event.automationId == kFieldId &&
               (event.id == UIA_Text_TextChangedEventId || event.id == UIA_Text_TextSelectionChangedEventId);
    }),
            "discarded form publication sends neither the queued text nor selection event after editor retirement");
    if (retirement == Retirement::HiddenField)
    {
        fixture.Publish();
        RequireRetired(*simple.get());
    }
    else if (retirement == Retirement::RetainedEditor)
    {
        // Read-only embedded properties use the last published snapshot until the application publishes again. An
        // action must already refuse this retained, unavailable editor, before any fresh preparation/publication.
        const HRESULT write = retainedValue->SetValue(L"99");
        Require(fixture.Editor().GetText() == draft, "retained detached editor action does not modify its draft");
        if (write != UIA_E_ELEMENTNOTAVAILABLE)
            std::cerr << "Retained editor SetValue returned 0x" << std::hex << static_cast<unsigned long>(write) << std::dec << '\n';
        Require(write == UIA_E_ELEMENTNOTAVAILABLE, "retained detached editor action reports the unavailable element");
        fixture.controls->SetRoot(std::make_unique<DxUi::Panel>());
        RequireRetired(*simple.get());
    }
    else
        RequireRetired(*simple.get());
}

void RootPublicationStopsAfterDestructiveCallback(Hosting hosting)
{
    PublicationStopsAfterDestructiveCallback(hosting, Retirement::Root);
}

void EditorPublicationStopsAfterDestructiveCallback(Hosting hosting)
{
    for (const auto retirement : {Retirement::Editor, Retirement::RetainedEditor, Retirement::HiddenField})
        PublicationStopsAfterDestructiveCallback(hosting, retirement);
}
} // namespace

void TestFormAccessibilityWindowPropertiesAndStableEditor()
{
    PropertiesAndStableEditor(Hosting::Window);
}
void TestFormAccessibilityEmbeddedPropertiesAndStableEditor()
{
    RunEmbeddedFormTest(PropertiesAndStableEditor);
}
void TestFormAccessibilityWindowLabelLifetimeAndReplacement()
{
    LabelLifetimeAndReplacement(Hosting::Window);
}
void TestFormAccessibilityEmbeddedLabelLifetimeAndReplacement()
{
    RunEmbeddedFormTest(LabelLifetimeAndReplacement);
}
void TestFormAccessibilityWindowPresentationEventsAreIdempotent()
{
    PresentationEventsAreIdempotent(Hosting::Window);
}
void TestFormAccessibilityEmbeddedPresentationEventsAreIdempotent()
{
    RunEmbeddedFormTest(PresentationEventsAreIdempotent);
}
void TestFormAccessibilityWindowPublicationStopsAfterDestructiveCallback()
{
    RootPublicationStopsAfterDestructiveCallback(Hosting::Window);
}
void TestFormAccessibilityEmbeddedPublicationStopsAfterDestructiveCallback()
{
    RunEmbeddedFormTest(RootPublicationStopsAfterDestructiveCallback);
}

void TestFormAccessibilityWindowPublicationStopsAfterEditorRetirement()
{
    EditorPublicationStopsAfterDestructiveCallback(Hosting::Window);
}

void TestFormAccessibilityEmbeddedPublicationStopsAfterEditorRetirement()
{
    RunEmbeddedFormTest(EditorPublicationStopsAfterDestructiveCallback);
}
