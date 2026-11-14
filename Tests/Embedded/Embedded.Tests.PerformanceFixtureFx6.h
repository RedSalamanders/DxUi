#pragma once
// FX6 Accessibility (plan W0.4): 100 mutations with 0 and with 1 UI Automation client; 100 Slider steps; a Splitter drag
// of 200 px; a word walk over a 64 Ki-unit document. The mutations are those of the W8.1 scene
// (Tests/Controls/Controls.Tests.AccessibilityFx6.h): a status label, a named text field and a named slider, mutation i
// setting the label to "Status i" (i % 3 == 0), the field to "Value i" (i % 3 == 1) or the slider to i (i % 3 == 2). That
// header builds on the control-suite helpers, which a fixture header cannot include, so the same three controls and the
// same mutation are written here.
//
// What "a client" means here: the client gate's test override (DebugSetAccessibilitySystemListenersForTest) answers what
// UiaClientsAreListening would answer while a screen reader runs, true in the one-client phases and false in the
// no-client phase whatever runs on the desktop. No UI Automation client thread runs, so every counter (snapshot builds,
// records, bytes, word units visited) depends only on the steps, never on when events arrive; the events themselves, and
// a real client's own subscription, are the W8.1 client-gate tests' subject. The word walk calls the text range provider
// in-process, as UI Automation calls it for a client.
//
// Notification delivery is manual (DebugSetAccessibilityNotificationDeliveryManualForTest): the owner queues each property
// change and structure invalidation exactly as it does for a real client, but no thread-pool work item delivers them;
// each publication first completes the previous step's queue (DebugCompletePendingAccessibilityNotificationsForTest).
// With the work item, whether a change coalesced with a still-queued one, and whether the queue had to grow, depended on
// how far the work item had got: one step made 118 or 120 C++ allocations. Not exercised here: the delivery work item
// (unmarshalling the senders, resolving values, UiaRaiseAutomationPropertyChangedEvent and the structure event), so
// nothing reaches UI Automation from this fixture whoever listens on the desktop.
//
// The scene owns its EmbeddedHost to attach its accessibility and to publish each frame as an application does
// (UpdateAccessibility after each prepared frame): every step first publishes the frame the previous step's change
// prepared, then makes its own change. Embedded host only: a window host would take the real pointer capture for the
// Splitter drag (SetCapture), which no fixture may do; the native client gate is covered by the W8.1 tests.
#include "Controls/DxUi.Internal.h"
#include "Embedded.Tests.PerformanceFixture.h"

#include <DxUi/EmbeddedAccessibility.h>
#include <DxUi/Failure.h>
#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>

namespace PerformanceFixtures::Fx6
{
inline constexpr size_t kMutations     = 100u;
inline constexpr size_t kSliderSteps   = 100u;
inline constexpr size_t kDragPixels    = 200u;
inline constexpr size_t kWordSteps     = 64u;
inline constexpr size_t kDocumentUnits = 65'536u;
// Each word step first moves the range this many characters on, so the 64 steps sample the whole document.
inline constexpr int kWordStride  = 1'000;
inline constexpr UINT kWidth      = 640u;
inline constexpr UINT kHeight     = 480u;
inline constexpr float kSplitterY = 176.0f;
// The separator's centre: the splitter's left edge (8) plus its position (100) plus half its 6 DIP thickness.
inline constexpr float kSplitterX = 111.0f;

inline constexpr std::array kBudgets{std::string_view("G9")};
inline constexpr std::array kVariants{Variant{"default", kWidth, kHeight, 96.0f, false}};
inline constexpr size_t kMutationsNoClient  = 1u;
inline constexpr size_t kMutationsOneClient = 2u;
inline constexpr size_t kSlider             = 3u;
inline constexpr size_t kSplitter           = 4u;
inline constexpr size_t kWordWalk           = 5u;
inline constexpr std::array kPhases{
    Phase{
        "warm", 3u, PhaseKind::Warmup, "the first frame; accessibility attaches with no client and the root provider is taken; the slider takes logical focus"},
    Phase{"mutations-no-client", kMutations, PhaseKind::Measured, "no client: publish the previous frame, then mutation i of the W8.1 scene"},
    Phase{"mutations-one-client", kMutations, PhaseKind::Measured, "one client: publish the previous frame, then mutation i of the W8.1 scene"},
    Phase{"slider-one-client", kSliderSteps, PhaseKind::Measured, "one client: publish, then one Right key on the focused slider (one step)"},
    Phase{"splitter-one-client",
          kDragPixels + 2u,
          PhaseKind::Measured,
          "one client: publish, then the Splitter drag: pointer down on the separator, 200 moves of 1 px, pointer up"},
    Phase{"wordwalk-one-client",
          kWordSteps + 1u,
          PhaseKind::Measured,
          "one client: step 0 shows the 64 Ki-unit document; each later step moves the text range 1,000 characters on, then one "
          "word step: Move by one word, ExpandToEnclosingUnit(Word) and GetText, through the text range provider in-process"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx6.h"),
                                     std::string_view("Tests/Controls/Controls.Tests.AccessibilityFx6.h")};

// The document: ASCII words of seven letters and a space, with a line break in place of every sixteenth space.
[[nodiscard]] inline std::wstring MakeDocument()
{
    std::wstring text(kDocumentUnits, L'a');
    for (size_t index = 0u; index < text.size(); ++index)
        text[index] = (index % 128u) == 127u ? L'\n' : (index % 8u) == 7u ? L' ' : static_cast<wchar_t>(L'a' + (index % 26u));
    return text;
}

inline void Succeed(HRESULT hr) noexcept
{
    if (FAILED(hr))
        DxUi::Failure::FailContract(hr);
}

// The application's side of the embedded attachment: the view is its own fragment root.
class Site final : public DxUi::EmbeddedAccessibilitySite
{
public:
    IRawElementProviderFragmentRoot* root = nullptr; // Borrowed; cleared before the view detaches.
    HRESULT Navigate(NavigateDirection, IRawElementProviderFragment** result) noexcept override
    {
        *result = nullptr;
        return S_OK;
    }
    HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept override
    {
        *result = root;
        if (root)
            root->AddRef();
        return root ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }
    HRESULT RequestFocus() noexcept override
    {
        return S_OK;
    }
};

class Scene final : public Scenario
{
public:
    ~Scene() override
    {
        range_.reset();
        if (site_)
            site_->root = nullptr;
        provider_.reset();
        DxUi::DebugCompletePendingAccessibilityNotificationsForTest(view_.Controls());
        view_.DisconnectAccessibility();
        DxUi::DebugSetAccessibilityNotificationDeliveryManualForTest(false);
        DxUi::DebugSetAccessibilitySystemListenersForTest(outerListeners_);
    }
    DxUi::EmbeddedHost* AttachOwnEmbeddedHost(ID3D11Device* device, HRESULT& result) override
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        result = DxUi::GraphicsDevice::Create(device, graphics);
        if (SUCCEEDED(result))
            result = view_.Attach(std::move(graphics));
        if (FAILED(result))
            return nullptr;
        auto theme          = DxUi::MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        view_.Controls().SetTheme(theme);
        return &view_;
    }
    HRESULT Build(Context& context) override
    {
        auto root = std::make_unique<DxUi::Panel>();
        label_    = root->AddChild<DxUi::Label>(L"Status");
        label_->SetBounds(D2D1::RectF(8.0f, 8.0f, 240.0f, 32.0f));
        field_ = root->AddChild<DxUi::TextField>(L"Value");
        field_->SetBounds(D2D1::RectF(8.0f, 40.0f, 240.0f, 68.0f));
        field_->SetAccessibleName(L"Entry");
        slider_ = root->AddChild<DxUi::Slider>();
        slider_->SetBounds(D2D1::RectF(8.0f, 76.0f, 240.0f, 112.0f));
        slider_->SetAccessibleName(L"Level");
        slider_->SetMinimum(0.0);
        slider_->SetMaximum(200.0);
        auto* splitter = root->AddChild<DxUi::Splitter>();
        splitter->SetBounds(D2D1::RectF(8.0f, 120.0f, 632.0f, 232.0f));
        splitter->SetAccessibleName(L"Panes");
        splitter->SetPosition(100.0f);
        document_ = root->AddChild<DxUi::TextField>(MakeDocument());
        document_->SetMultiline(true);
        document_->SetBounds(D2D1::RectF(8.0f, 240.0f, 632.0f, 472.0f));
        document_->SetAccessibleName(L"Document");
        document_->SetVisible(false); // shown for the word walk only, so the other phases neither paint nor copy it
        context.controls.SetRoot(std::move(root));
        return S_OK;
    }
    void Step(Context& context, size_t phase, size_t step) override
    {
        // The phase's client state first, so the publication below is made as that phase's client sees it.
        if (phase == kMutationsNoClient)
            DxUi::DebugSetAccessibilitySystemListenersForTest(false);
        else if (phase >= kMutationsOneClient)
            DxUi::DebugSetAccessibilitySystemListenersForTest(true);
        Publish();
        switch (phase)
        {
            case 0u:
                if (step == 1u)
                {
                    DxUi::DebugSetAccessibilitySystemListenersForTest(false);
                    DxUi::DebugSetAccessibilityNotificationDeliveryManualForTest(true);
                    site_ = std::make_shared<Site>();
                    Succeed(view_.AttachAccessibility(site_, 0x4658360000000001ull, Placement()));
                    Succeed(view_.GetAccessibilityProvider(provider_.put()));
                    site_->root = provider_.get();
                    context.controls.SetFocusControl(slider_, false);
                }
                break;
            case kMutationsNoClient:
            case kMutationsOneClient: Mutate(step); break;
            case kSlider:
                static_cast<void>(context.port.Key(VK_RIGHT, true));
                static_cast<void>(context.port.Key(VK_RIGHT, false));
                break;
            case kSplitter:
            {
                DxUi::PointerEvent event{};
                event.yPixels = kSplitterY;
                if (step == 0u)
                {
                    event.action  = DxUi::PointerAction::Down;
                    event.xPixels = kSplitterX;
                }
                else if (step <= kDragPixels)
                {
                    event.action  = DxUi::PointerAction::Move;
                    event.xPixels = kSplitterX + static_cast<float>(step);
                }
                else if (step == kDragPixels + 1u)
                {
                    event.action  = DxUi::PointerAction::Up;
                    event.xPixels = kSplitterX + static_cast<float>(kDragPixels);
                }
                else
                    break;
                static_cast<void>(context.port.Pointer(event));
                break;
            }
            case kWordWalk:
                if (step == 0u)
                    document_->SetVisible(true);
                else if (step <= kWordSteps)
                    WordStep();
                break;
            default: break;
        }
    }

private:
    [[nodiscard]] static DxUi::EmbeddedAccessibilityPlacement Placement() noexcept
    {
        return DxUi::EmbeddedAccessibilityPlacement{{0.0, 0.0, static_cast<double>(kWidth), static_cast<double>(kHeight)}, false};
    }
    void Publish()
    {
        if (! provider_)
            return;
        // The previous step's notifications are complete before this publication, as if their delivery always finished
        // first; nothing reaches UI Automation, so counters do not depend on that work item or on the desktop's clients.
        DxUi::DebugCompletePendingAccessibilityNotificationsForTest(view_.Controls());
        Succeed(view_.UpdateAccessibility(Placement()));
    }
    void Mutate(size_t index) const
    {
        switch (index % 3u)
        {
            case 0u: label_->SetText(L"Status " + std::to_wstring(index)); break;
            case 1u: field_->SetText(L"Value " + std::to_wstring(index)); break;
            default: slider_->SetValue(static_cast<double>(index)); break;
        }
    }
    // The document's element, found as a client finds it: by name among the root's children.
    [[nodiscard]] wil::com_ptr_nothrow<ITextRangeProvider> DocumentRange() const
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> root;
        Succeed(provider_.query_to(root.put()));
        wil::com_ptr_nothrow<IRawElementProviderFragment> child;
        Succeed(root->Navigate(NavigateDirection_FirstChild, child.put()));
        while (child)
        {
            wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
            Succeed(child.query_to(simple.put()));
            wil::unique_variant name;
            Succeed(simple->GetPropertyValue(UIA_NamePropertyId, &name));
            if (name.vt == VT_BSTR && name.bstrVal && std::wstring_view(name.bstrVal, SysStringLen(name.bstrVal)) == L"Document")
            {
                wil::com_ptr_nothrow<IUnknown> pattern;
                Succeed(simple->GetPatternProvider(UIA_TextPatternId, pattern.put()));
                wil::com_ptr_nothrow<ITextProvider> text;
                Succeed(pattern ? pattern.query_to(text.put()) : E_NOINTERFACE);
                wil::com_ptr_nothrow<ITextRangeProvider> range;
                Succeed(text->get_DocumentRange(range.put()));
                // Collapsed to the document's start.
                Succeed(range->MoveEndpointByRange(TextPatternRangeEndpoint_End, range.get(), TextPatternRangeEndpoint_Start));
                return range;
            }
            wil::com_ptr_nothrow<IRawElementProviderFragment> next;
            Succeed(child->Navigate(NavigateDirection_NextSibling, next.put()));
            child = std::move(next);
        }
        DxUi::Failure::FailContract(HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
    }
    void WordStep()
    {
        if (! range_)
            range_ = DocumentRange();
        int moved = 0;
        Succeed(range_->Move(TextUnit_Character, kWordStride, &moved));
        Succeed(range_->Move(TextUnit_Word, 1, &moved));
        Succeed(range_->ExpandToEnclosingUnit(TextUnit_Word));
        wil::unique_bstr word;
        Succeed(range_->GetText(-1, word.put()));
    }

    // Declared first, so the controls and the attachment outlive everything that borrows them.
    DxUi::EmbeddedHost view_;
    std::shared_ptr<Site> site_;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider_;
    wil::com_ptr_nothrow<ITextRangeProvider> range_;
    std::optional<bool> outerListeners_ = DxUi::DebugGetAccessibilitySystemListenersForTest();
    DxUi::Label* label_                 = nullptr;
    DxUi::TextField* field_             = nullptr;
    DxUi::Slider* slider_               = nullptr;
    DxUi::TextField* document_          = nullptr;
};

inline constexpr Definition kDefinition{
    .id         = "FX6",
    .title      = "Accessibility",
    .scenario   = "The W8.1 scene (status label, named text field, named slider) with a Splitter and a hidden 64 Ki-unit multiline document, "
                  "640x480 offscreen EmbeddedHost with UI Automation attached, reduced motion. 100 mutations with no client and 100 with "
                  "one; 100 Slider steps, a 200 px Splitter drag and a 64-step word walk with one. A client is the client gate's test "
                  "override, so counters never depend on event arrival. Proves G9 and the W8 gates.",
    .budgets    = kBudgets,
    .hosts      = kEmbedded,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "mutations-one-client",
    .sources    = kSources,
    .create     = []() -> std::unique_ptr<Scenario> { return std::make_unique<Scene>(); },
};
} // namespace PerformanceFixtures::Fx6
