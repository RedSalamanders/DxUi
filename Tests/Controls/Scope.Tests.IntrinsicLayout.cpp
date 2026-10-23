#include "Scope.Tests.IntrinsicLayout.h"

#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "Controls.Tests.DxUiTestHelpers.h"

#include <limits>
#include <stdexcept>
#include <thread>

namespace
{
using namespace DxUi;

[[nodiscard]] bool Close(float a, float b) noexcept
{
    return std::abs(a - b) < 0.01f;
}

[[nodiscard]] bool SameBounds(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

class MeasuredProbe final : public Control
{
public:
    DesiredSize desired{.sizeDip = D2D1::SizeF(40.0f, 20.0f)};
    std::function<void()> measureCallback;
    std::function<void()> boundsCallback;
    HRESULT status = S_OK;
    void Paint(ControlHost&) const override
    {
    }

protected:
    HRESULT MeasureOverride(const MeasureContext&, const LayoutConstraints&, DesiredSize& result) const override
    {
        const auto callback = measureCallback;
        const auto hr       = status;
        result              = desired;
        if (callback)
            callback();
        return hr;
    }
    void OnBoundsChanged() noexcept override
    {
        const auto callback = boundsCallback;
        if (callback)
            callback();
    }
};

class FlexDetachProbe final : public Control
{
public:
    std::shared_ptr<const std::function<void()>> onDetach;
    void Paint(ControlHost&) const override
    {
    }

protected:
    void PropagateHost(ControlHost* host) noexcept override
    {
        const auto callback = onDetach;
        const auto lifetime = GetLifetimeToken();
        const bool detached = GetHost() && ! host;
        Control::PropagateHost(host);
        if (detached && ! lifetime.expired() && callback && *callback)
            (*callback)();
    }
};

[[nodiscard]] FlexChildOptions Fixed(float width, float height)
{
    return {.explicitSizeDip = D2D1::SizeF(width, height), .alignment = LayoutAlignment::Start};
}

void TestIntrinsicMeasurementContainsFailuresAndPreservesOutputs()
{
    ControlHost host;
    Panel unsupported;
    const DesiredSize initial{.sizeDip = D2D1::SizeF(123.0f, 456.0f), .baselineDip = 9.0f, .overflow = true};
    DesiredSize result = initial;
    Require(unsupported.Measure({host}, {}, result) == E_NOTIMPL && result.sizeDip.width == 123.0f && result.baselineDip == 9.0f,
            "legacy custom controls retain their explicit sizing contract and failed measure leaves output unchanged");
    for (const LayoutConstraints constraints : {LayoutConstraints{.minimumDip = D2D1::SizeF(-1.0f, 0.0f)},
                                                LayoutConstraints{.maximumWidthDip = std::numeric_limits<float>::infinity()},
                                                LayoutConstraints{.maximumHeightDip = std::numeric_limits<float>::quiet_NaN()},
                                                LayoutConstraints{.minimumDip = D2D1::SizeF(20.0f, 0.0f), .maximumWidthDip = 10.0f}})
    {
        Require(unsupported.Measure({host}, constraints, result) == E_INVALIDARG && result.sizeDip.height == 456.0f,
                "nonfinite, negative and contradictory constraints fail atomically");
    }
    MeasuredProbe probe;
    probe.desired.baselineDip = 21.0f;
    Require(probe.Measure({host}, {}, result) == E_INVALIDARG && result.sizeDip.width == 123.0f, "invalid custom baselines never publish");
    probe.desired.baselineDip.reset();
    probe.measureCallback = [] { throw std::bad_alloc(); };
    Require(probe.Measure({host}, {}, result) == E_OUTOFMEMORY && result.sizeDip.height == 456.0f, "measurement allocation failure is contained");
    probe.measureCallback = [] { throw std::runtime_error("measurement failure"); };
    Require(probe.Measure({host}, {}, result) == E_FAIL && result.overflow, "named callback exceptions are contained at the public boundary");
    Label tooLong(std::wstring(kMaxIntrinsicTextUnits + 1u, L'x'));
    Require(tooLong.Measure({host}, {}, result) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && result.sizeDip.width == 123.0f,
            "UTF16 measurement work is bounded before allocating shaping resources");
}

void TestIntrinsicLeavesShapeUnicodeAndKeepEditorsStable()
{
    ControlHost host;
    Label label(L"Annuler les modifications e\u0301 \U0001F642 العربية");
    DesiredSize unbounded;
    RequireSucceeded(label.Measure({host}, {}, unbounded), "measure combining, supplementary and Arabic text");
    Require(unbounded.sizeDip.width > 50.0f && unbounded.sizeDip.height > 0.0f && unbounded.baselineDip.has_value(),
            "shaped text supplies real metrics and baseline");
    label.SetBounds(D2D1::RectF(400.0f, 500.0f, 401.0f, 501.0f));
    DesiredSize moved;
    RequireSucceeded(label.Measure({host}, {}, moved), "measure with an unrelated previous rectangle");
    Require(Close(unbounded.sizeDip.width, moved.sizeDip.width) && Close(unbounded.sizeDip.height, moved.sizeDip.height),
            "the previous arranged rectangle cannot become intrinsic input");
    label.SetMultiline(true);
    DesiredSize wrapped;
    RequireSucceeded(label.Measure({host}, {.maximumWidthDip = 90.0f}, wrapped), "measure wrapped localized text");
    Require(wrapped.sizeDip.height > unbounded.sizeDip.height && (wrapped.sizeDip.width <= 90.01f || wrapped.overflow),
            "an explicit width wraps text and reports any natural text extent that exceeds it");
    Label indivisible(L"W");
    DesiredSize clippedGlyph;
    RequireSucceeded(indivisible.Measure({host}, {.maximumWidthDip = 1.0f}, clippedGlyph), "measure a glyph wider than available text width");
    Require(clippedGlyph.sizeDip.width > 1.0f && clippedGlyph.overflow,
            "measurement preserves an indivisible glyph and reports overflow instead of truncating");
    const auto revision = label.GetMeasureRevision();
    label.SetTextColor(D2D1::ColorF(D2D1::ColorF::Red));
    Require(label.GetMeasureRevision() == revision, "paint color does not dirty shape");
    label.SetFontRole(FontRole::Title);
    Require(label.GetMeasureRevision() != revision, "font role dirties intrinsic measurement");
    std::array<std::unique_ptr<Control>, 3> editors{std::make_unique<TextField>(L"1"), std::make_unique<ComboBox>(), std::make_unique<NumericStepper>()};
    for (const auto& owned : editors)
    {
        Control* const editor = owned.get();
        DesiredSize before;
        const LayoutConstraints constraints{.minimumDip = D2D1::SizeF(180.0f, 0.0f)};
        RequireSucceeded(editor->Measure({host}, constraints, before), "stable editor measurement");
        if (auto* text = dynamic_cast<TextField*>(editor))
            text->SetText(L"a very long draft that scrolls within the editor");
        else if (auto* combo = dynamic_cast<ComboBox*>(editor))
            combo->SetText(L"a very long selected value");
        else if (auto* stepper = dynamic_cast<NumericStepper*>(editor))
            stepper->SetValue(98765.0);
        DesiredSize after;
        RequireSucceeded(editor->Measure({host}, constraints, after), "changed editor value measurement");
        Require(Close(before.sizeDip.width, 180.0f) && Close(before.sizeDip.width, after.sizeDip.width) && Close(before.sizeDip.height, after.sizeDip.height),
                "editor values and drafts preserve caller-supplied preferred width");
    }
    Toggle toggle;
    toggle.SetStateLabels(L"Non", L"Activé pour tous les fichiers");
    DesiredSize off;
    DesiredSize on;
    RequireSucceeded(toggle.Measure({host}, {}, off), "measure both toggle labels");
    const auto toggleRevision = toggle.GetMeasureRevision();
    toggle.SetChecked(true);
    RequireSucceeded(toggle.Measure({host}, {}, on), "measure checked toggle");
    Require(Close(off.sizeDip.width, on.sizeDip.width) && toggle.GetMeasureRevision() == toggleRevision, "toggle state cannot resize the row");
    Checkbox checkbox(L"Une légende qui doit être mesurée et repliée sur plusieurs lignes");
    checkbox.SetMultiline(true);
    DesiredSize checkboxSize;
    RequireSucceeded(checkbox.Measure({host}, {.maximumWidthDip = 140.0f}, checkboxSize), "measure wrapping checkbox chrome");
    Require(checkboxSize.sizeDip.height > 32.0f && checkboxSize.sizeDip.width >= 36.0f && (checkboxSize.sizeDip.width <= 140.01f || checkboxSize.overflow),
            "checkbox measurement reserves its indicator and vertical padding and reports excess natural width");
}

void TestLabelTypographyCharacterSpacingUsesRequestedTotal()
{
    ControlHost host;
    Label baseline(L"MMMM");
    Label spaced(L"MMMM");
    const LabelTypography unspaced{.fontSizeDip = 16.0f, .fontWeight = DWRITE_FONT_WEIGHT_NORMAL, .characterSpacingDip = 0.0f};
    auto tracked                = unspaced;
    tracked.characterSpacingDip = 1.5f;
    Require(baseline.SetTypographyOverride(unspaced) == S_OK && spaced.SetTypographyOverride(tracked) == S_OK,
            "labels accept matching font metrics with explicit tracking");
    DesiredSize baselineSize;
    DesiredSize spacedSize;
    RequireSucceeded(baseline.Measure({host}, {}, baselineSize), "measure untracked typography baseline");
    RequireSucceeded(spaced.Measure({host}, {}, spacedSize), "measure tracked typography");
    const float increase = spacedSize.sizeDip.width - baselineSize.sizeDip.width;
    Require(increase > 5.8f && increase < 6.2f, "characterSpacingDip is the total 1.5 DIP per character across four glyphs, not 3 DIP per glyph");
    const auto normalized = spaced.GetTypographyOverride();
    Require(normalized && normalized->characterSpacingDip == 1.5f, "published character spacing uses the normalized public value");
    tracked.characterSpacingDip = 1.5001f;
    Require(spaced.SetTypographyOverride(tracked) == S_FALSE && spaced.GetTypographyOverride() == normalized,
            "spacing values within one 1/64-DIP bucket publish idempotently");
}

void TestButtonContentFontSizeDefaultsValidationAndNormalization()
{
    Button button(L"Apply");
    const auto initial = button.GetMeasureRevision();
    Require(! button.GetContentFontSizeDip() && button.SetContentFontSizeDip(std::nullopt) == S_FALSE && button.GetMeasureRevision() == initial,
            "button content inherits its existing role size by default without invalidating layout");
    Require(button.SetContentFontSizeDip(14.0001f) == S_OK && button.GetContentFontSizeDip() == 14.0f,
            "button content sizes normalize to one sixty-fourth DIP");
    const auto changed = button.GetMeasureRevision();
    Require(changed != initial && button.SetContentFontSizeDip(14.0002f) == S_FALSE && button.GetMeasureRevision() == changed,
            "an equivalent normalized size is idempotent for intrinsic layout");
    for (const float invalid :
         {-1.0f, 0.0f, 5.99f, 96.01f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        Require(button.SetContentFontSizeDip(invalid) == E_INVALIDARG && button.GetContentFontSizeDip() == 14.0f && button.GetMeasureRevision() == changed,
                "invalid content sizes leave the current size and measure epoch unchanged");
    Require(button.SetContentFontSizeDip(6.0f) == S_OK && button.GetContentFontSizeDip() == 6.0f && button.SetContentFontSizeDip(96.0f) == S_OK &&
                button.GetContentFontSizeDip() == 96.0f,
            "the closed supported size interval accepts both endpoints");
    Require(button.SetContentFontSizeDip(std::nullopt) == S_OK && ! button.GetContentFontSizeDip() && button.SetContentFontSizeDip(std::nullopt) == S_FALSE,
            "clearing the override restores the inherited role size idempotently");
}

void TestButtonContentFontSizeShapesIntrinsicVariants()
{
    ControlHost host;
    for (const auto variant : {ButtonVariant::Standard,
                               ButtonVariant::DropDown,
                               ButtonVariant::Selector,
                               ButtonVariant::Split,
                               ButtonVariant::Hyperlink,
                               ButtonVariant::IconOnly,
                               ButtonVariant::Disclosure,
                               ButtonVariant::Repeat})
    {
        Button button(L"MMMMMMMM");
        button.SetVariant(variant);
        DesiredSize original;
        RequireSucceeded(button.Measure({host}, {}, original), "measure inherited button content");
        const bool icon       = variant == ButtonVariant::IconOnly || variant == ButtonVariant::Disclosure;
        const float requested = icon ? 18.0f : 14.0f;
        Require(button.SetContentFontSizeDip(requested) == S_OK, "override button content size");
        const FontRole role =
            icon ? (host.HasFluentIconFont() ? FontRole::Icon : (variant == ButtonVariant::IconOnly ? FontRole::Body : FontRole::Small)) : FontRole::Body;
        auto* const base = host.GetTextFormat(role);
        Require(base != nullptr, "host retains the original role format");
        const auto baseSize = base->GetFontSize();
        // A native layout changes only the original role's glyph size; it independently verifies
        // Button's configured-format selection and baseline without relying on another control's optional baseline.
        const DWRITE_TEXT_RANGE range{0u, static_cast<UINT32>(button.GetText().size())};
        wil::com_ptr<IDWriteTextLayout> reference;
        RequireSucceeded(
            host.GetWriteFactory()->CreateTextLayout(
                button.GetText().data(), range.length, base, (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), reference.put()),
            "create an independent native layout with the unchanged role family and weight");
        RequireSucceeded(reference->SetFontSize(requested, range), "shape native glyphs at the requested size");
        DWRITE_TEXT_METRICS text{};
        DWRITE_LINE_METRICS line{};
        UINT32 lineCount = 0u;
        RequireSucceeded(reference->GetMetrics(&text), "measure independent requested glyph geometry");
        RequireSucceeded(reference->GetLineMetrics(&line, 1u, &lineCount), "measure the independent first-line baseline");
        Require(lineCount == 1u, "the single-line native reference has exactly one baseline");
        DesiredSize sized;
        RequireSucceeded(button.Measure({host}, {}, sized), "measure requested button content");
        float horizontal = icon ? 16.0f : 24.0f;
        if (variant == ButtonVariant::DropDown)
            horizontal += kButtonDropDownChevronWidthDip;
        else if (variant == ButtonVariant::Selector)
            horizontal += kButtonSelectorChevronWidthDip;
        else if (variant == ButtonVariant::Split)
            horizontal += kButtonSplitDropDownSegmentDip;
        Require(Close(sized.sizeDip.width, std::max(icon ? 32.0f : 0.0f, text.widthIncludingTrailingWhitespace + horizontal)) &&
                    Close(sized.sizeDip.height, std::max(32.0f, text.height + 16.0f)) && sized.baselineDip &&
                    Close(sized.baselineDip.value(), line.baseline + 8.0f),
                "every button variant measures the requested glyph size with its unchanged chrome and baseline padding");
        Require(base->GetFontSize() == baseSize, "a per-control size never mutates the host's role format");
        Require(button.SetContentFontSizeDip(std::nullopt) == S_OK, "restore role defaults");
        DesiredSize restored;
        RequireSucceeded(button.Measure({host}, {}, restored), "remeasure restored role defaults");
        Require(Close(original.sizeDip.width, restored.sizeDip.width) && Close(original.sizeDip.height, restored.sizeDip.height),
                "null preserves exact default intrinsic metrics for every variant");
    }
    Button multiline(L"A translated caption that needs several measured lines at this width");
    multiline.SetMultiline(true);
    DesiredSize compactCaption, expandedCaption;
    RequireSucceeded(multiline.SetContentFontSizeDip(14.0f), "size wrapped button text");
    RequireSucceeded(multiline.Measure({host}, {.maximumWidthDip = 180.0f}, compactCaption), "measure wrapped 14-DIP caption");
    RequireSucceeded(multiline.SetContentFontSizeDip(28.0f), "enlarge wrapped caption");
    RequireSucceeded(multiline.Measure({host}, {.maximumWidthDip = 180.0f}, expandedCaption), "measure wrapped enlarged caption");
    Require(expandedCaption.sizeDip.height > compactCaption.sizeDip.height && (expandedCaption.sizeDip.width <= 180.01f || expandedCaption.overflow),
            "wrapped measurement uses the same requested font size and preserves truthful overflow");
}

void TestButtonContentFontSizeNativeGlyphPaintCacheAndGeneration()
{
    // A prior headless measure can initialize a hardware bucket for the runner thread. WARP ownership
    // cannot replace a live graphics bucket, so keep this fixture on its own STA regardless of suite order.
    std::jthread fixture([]
    {
        const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
        DxUi::TestSupport::ScopedWindowActivationBlocker blocker;
        Require(blocker.Start(), "the WARP fixture thread cannot activate or focus desktop windows");
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        auto& host             = window.Host();
        auto theme             = host.GetTheme();
        theme.reducedMotion    = true;
        theme.text             = D2D1::ColorF(D2D1::ColorF::Black);
        theme.windowBackground = theme.buttonFill = theme.buttonHotFill = theme.buttonPressedFill = theme.buttonBorder = D2D1::ColorF(D2D1::ColorF::White);
        host.SetTheme(theme);
        auto root    = std::make_unique<Panel>();
        auto* button = root->AddChild<Button>(L"MMMMMMMM");
        button->SetBounds(D2D1::RectF(20.0f, 20.0f, 220.0f, 100.0f));
        host.SetRoot(std::move(root));
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        const auto capture = [&]
        {
            host.Invalidate();
            WindowHostBitmapCapture pixels;
            Require(host.DebugCaptureBitmap(pixels), "capture offscreen WARP button glyphs without desktop activation");
            return pixels;
        };
        // This fixture needs the glyph's ink bounds inside a border-free white button interior;
        // alpha/full-image comparison helpers deliberately measure a different property. Include antialiased
        // edge coverage: a thin Fluent icon stem centered between pixel columns can have no RGB<80 pixels.
        const auto ink = [&](const WindowHostBitmapCapture& pixels)
        {
            const auto bounds = button->GetBounds();
            const UINT left   = static_cast<UINT>(std::ceil(host.DipsToPixels(bounds.left + 4.0f)));
            const UINT right  = std::min(pixels.widthPx, static_cast<UINT>(std::floor(host.DipsToPixels(bounds.right - 4.0f))));
            const UINT top    = static_cast<UINT>(std::ceil(host.DipsToPixels(bounds.top + 4.0f)));
            const UINT bottom = std::min(pixels.heightPx, static_cast<UINT>(std::floor(host.DipsToPixels(bounds.bottom - 4.0f))));
            UINT minX = right, minY = bottom, maxX = left, maxY = top;
            bool found = false;
            for (UINT y = top; y < bottom; ++y)
                for (UINT x = left; x < right; ++x)
                {
                    const auto at = (static_cast<size_t>(y) * pixels.widthPx + x) * 4u;
                    if (pixels.bgraPixels[at] < 224u && pixels.bgraPixels[at + 1u] < 224u && pixels.bgraPixels[at + 2u] < 224u)
                    {
                        minX  = std::min(minX, x);
                        minY  = std::min(minY, y);
                        maxX  = std::max(maxX, x);
                        maxY  = std::max(maxY, y);
                        found = true;
                    }
                }
            Require(found, "button capture contains actual shaped glyph pixels");
            return D2D1::SizeF(static_cast<float>(maxX - minX + 1u), static_cast<float>(maxY - minY + 1u));
        };
        const auto inherited = ink(capture());
        RequireSucceeded(button->SetContentFontSizeDip(14.0f), "paint exact chrome label size");
        const auto normal = ink(capture());
        Require(normal.width > inherited.width, "14-DIP paint grows actual glyph geometry above the inherited 13-DIP role");
        RequireSucceeded(button->SetContentFontSizeDip(28.0f), "paint doubled chrome label size");
        const auto doubled = ink(capture());
        Require(doubled.width > normal.width * 1.8f && doubled.height > normal.height * 1.7f,
                "paint uses the requested content size rather than only enlarging intrinsic bounds");
        RequireSucceeded(button->SetContentFontSizeDip(std::nullopt), "restore native role default");
        const auto restored = ink(capture());
        Require(Close(restored.width, inherited.width) && Close(restored.height, inherited.height), "clearing the size restores native default glyph geometry");

        button->SetVariant(ButtonVariant::IconOnly);
        button->SetText(host.HasFluentIconFont() ? L"\uE710" : L"+");
        const auto originalIcon = ink(capture());
        RequireSucceeded(button->SetContentFontSizeDip(18.0f), "paint 18-DIP line icon");
        const auto sizedIcon = ink(capture());
        Require(sizedIcon.width > originalIcon.width + 1.0f && sizedIcon.height > originalIcon.height + 1.0f,
                "IconOnly scales actual Fluent or fallback glyphs using its retained host family");
        const auto cacheCount = host.DebugGetConfiguredTextFormatCount();
        for (unsigned repeat = 0u; repeat < 8u; ++repeat)
            static_cast<void>(capture());
        Require(host.DebugGetConfiguredTextFormatCount() == cacheCount, "repeated painting reuses configured formats without per-frame format growth");
        const auto generation = host.GetResourceGeneration();
        host.DebugSimulateDeviceLoss();
        const auto recreated = ink(capture());
        Require(host.GetResourceGeneration() != generation && Close(recreated.width, sizedIcon.width) && Close(recreated.height, sizedIcon.height) &&
                    button->GetContentFontSizeDip() == 18.0f && host.DebugGetConfiguredTextFormatCount() == cacheCount,
                "device-generation replacement preserves requested glyph geometry and device-independent format cache identity");

        button->SetVariant(ButtonVariant::DropDown);
        button->SetText(L"Apply");
        RequireSucceeded(button->SetContentFontSizeDip(std::nullopt), "restore native drop-down baseline");
        const auto before = capture();
        RequireSucceeded(button->SetContentFontSizeDip(28.0f), "enlarge only the drop-down's caller content");
        const auto after   = capture();
        const auto chevron = ComputeButtonChromeLayout(button->GetBounds(), ButtonVariant::DropDown).chevronRect;
        for (UINT y = static_cast<UINT>(host.DipsToPixels(chevron.top)); y < static_cast<UINT>(host.DipsToPixels(chevron.bottom)); ++y)
            for (UINT x = static_cast<UINT>(host.DipsToPixels(chevron.left)); x < static_cast<UINT>(host.DipsToPixels(chevron.right)); ++x)
            {
                const auto at = (static_cast<size_t>(y) * before.widthPx + x) * 4u;
                Require(std::equal(before.bgraPixels.begin() + at, before.bgraPixels.begin() + at + 4u, after.bgraPixels.begin() + at),
                        "internal drop-down chevron keeps its original pixel geometry when the caption size changes");
            }
    });
    Require(WaitForDxUiEventWithSentMessagesUntil(fixture.native_handle(), GetTickCount64() + 30000u) == WAIT_OBJECT_0,
            "the isolated WARP fixture finishes including owner-thread COM and HWND cleanup");
    fixture.join();
}

void TestButtonContentFontSizeSurvivesEmbeddedMeasureAndRetirementCallbacks()
{
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "button typography embedded WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "button typography graphics pool");
    EmbeddedHost view;
    struct CallbackState
    {
        ControlHost* host{};
        Button* button{};
        bool armed{}, retire{};
        HRESULT measured = E_PENDING;
        DesiredSize desired;
    } state{.host = &view.Controls()};
    const EmbeddedCallbacks callbacks{.context            = &state,
                                      .requestPreparation = [](void* context) noexcept
    {
        auto& current = *static_cast<CallbackState*>(context);
        if (! current.armed)
            return;
        current.armed = false;
        if (current.retire)
            current.host->SetRoot(nullptr);
        else
            current.measured = current.button->Measure({*current.host}, {}, current.desired);
    }};
    RequireSucceeded(view.Attach(device, callbacks), "attach button typography callback fixture");
    auto root    = std::make_unique<FlexPanel>();
    state.button = root->AddChild<Button>(L"A caption measured synchronously");
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(320u, 160u), "prepare callback fixture before changing typography");
    state.armed = true;
    Require(state.button->SetContentFontSizeDip(14.0f) == S_OK && state.measured == S_OK && state.desired.sizeDip.width > 50.0f,
            "embedded scheduling can synchronously measure the already-published new typography without stale output");
    RequireSucceeded(view.Prepare(320u, 160u), "prepare changed intrinsic typography");
    state.armed = state.retire = true;
    Require(state.button->SetContentFontSizeDip(18.0f) == S_OK && view.Controls().GetRoot() == nullptr,
            "the setter returns without dereferencing a button retired by its preparation callback");
}

void TestLabelAndSeparatorTypographySurviveEmbeddedMeasureAndRetirementCallbacks()
{
    const auto runRetiringSetter = []<typename AddChild, typename ChangeTypography>(AddChild&& addChild, ChangeTypography&& changeTypography)
    {
        GraphicsFixture graphics;
        RequireSucceeded(graphics.Create(), "embedded WARP fixture for a retiring typography setter");
        std::shared_ptr<GraphicsDevice> device;
        RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "graphics pool for a retiring typography setter");
        EmbeddedHost view;
        struct CallbackState
        {
            ControlHost* host{};
            bool armed{};
        } state{.host = &view.Controls()};
        const EmbeddedCallbacks callbacks{.context            = &state,
                                          .requestPreparation = [](void* context) noexcept
        {
            auto& current = *static_cast<CallbackState*>(context);
            if (current.armed)
            {
                current.armed = false;
                current.host->SetRoot(nullptr);
            }
        }};
        RequireSucceeded(view.Attach(device, callbacks), "attach retiring typography callback fixture");
        auto root              = std::make_unique<FlexPanel>();
        Control* const control = addChild(*root);
        Require(control != nullptr, "create intrinsic child for retiring typography setter");
        view.Controls().SetRoot(std::move(root));
        RequireSucceeded(view.Prepare(320u, 160u), "prepare callback fixture before changing typography");
        state.armed = true;
        changeTypography(control);
        Require(view.Controls().GetRoot() == nullptr, "setter returns after embedded preparation retires its root");
    };

    runRetiringSetter([](FlexPanel& root) -> Control* { return root.AddChild<Label>(L"Section heading"); },
                      [](Control* control) { static_cast<Label*>(control)->SetFontRole(FontRole::Title); });
    runRetiringSetter([](FlexPanel& root) -> Control* { return root.AddChild<Label>(L"Section heading"); },
                      [](Control* control)
    {
        Require(static_cast<Label*>(control)->SetTypographyOverride(
                    LabelTypography{.fontSizeDip = 16.0f, .fontWeight = DWRITE_FONT_WEIGHT_SEMI_BOLD, .characterSpacingDip = 0.5f}) == S_OK,
                "label typography override is accepted before embedded retirement");
    });
    runRetiringSetter([](FlexPanel& root) -> Control* { return root.AddChild<Separator>(); },
                      [](Control* control) { static_cast<Separator*>(control)->SetOrientation(SeparatorOrientation::Horizontal); });
}

void TestFlexFlowWrapGrowthRtlAndOverflow()
{
    ControlHost host;
    FlexPanel row;
    RequireSucceeded(row.SetOrientation(StackOrientation::Horizontal), "horizontal flex");
    RequireSucceeded(row.SetGap(8.0f), "flex gap");
    RequireSucceeded(row.SetWrap(true), "explicit flex wrapping");
    row.SetBounds(D2D1::RectF(10.0f, 20.0f, 180.0f, 120.0f));
    std::array<Panel*, 3> children{};
    for (auto& child : children)
    {
        child = row.AddChild<Panel>();
        RequireSucceeded(row.SetChildOptions(child, Fixed(70.0f, 20.0f)), "explicit fallback for a legacy panel");
    }
    RequireSucceeded(row.ApplyLayout(host), "wrapped flow arrangement");
    Require(Close(children[0]->GetBounds().left, 10.0f) && Close(children[1]->GetBounds().left, 88.0f) && Close(children[2]->GetBounds().top, 48.0f),
            "wrapping maintains logical row order and adds one gap between rows");
    Require(Close(row.GetContentSize().width, 148.0f) && Close(row.GetContentSize().height, 48.0f), "flow reports natural content extent");
    row.SetFlowDirection(FlowDirection::RightToLeft);
    RequireSucceeded(row.ApplyLayout(host), "mirrored flex arrangement");
    Require(Close(children[0]->GetBounds().left, 110.0f) && Close(children[1]->GetBounds().left, 32.0f) && row.GetLogicalChild(0u) == children[0],
            "RTL mirrors rectangles without reordering ownership or navigation");
    FlexPanel grow;
    RequireSucceeded(grow.SetOrientation(StackOrientation::Horizontal), "growing row");
    RequireSucceeded(grow.SetGap(10.0f), "growing row gap");
    grow.SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 50.0f));
    auto* first                        = grow.AddChild<Panel>();
    auto* second                       = grow.AddChild<Panel>();
    auto capped                        = Fixed(30.0f, 20.0f);
    capped.grow                        = 1.0f;
    capped.constraints.maximumWidthDip = 60.0f;
    auto flexible                      = Fixed(30.0f, 20.0f);
    flexible.grow                      = 1.0f;
    RequireSucceeded(grow.SetChildOptions(first, capped), "capped growth");
    RequireSucceeded(grow.SetChildOptions(second, flexible), "remaining growth");
    RequireSucceeded(grow.ApplyLayout(host), "redistribute growth after a maximum");
    Require(Close(first->GetBounds().right, 60.0f) && Close(second->GetBounds().right - second->GetBounds().left, 130.0f),
            "growth honors maximums and redistributes unused room");
    grow.SetBounds(D2D1::RectF(0.0f, 0.0f, 15.0f, 10.0f));
    RequireSucceeded(grow.ApplyLayout(host), "minimum-overflow arrangement");
    Require(grow.HasLayoutOverflow() && first->GetBounds().right - first->GetBounds().left >= 30.0f &&
                first->GetBounds().bottom - first->GetBounds().top >= 20.0f,
            "narrow parents report overflow while preserving child minima");
    const auto oldGap = grow.GetGap();
    Require(grow.SetGap(-1.0f) == E_INVALIDARG && grow.SetPadding(0.0f, 0.0f, std::numeric_limits<float>::infinity(), 0.0f) == E_INVALIDARG &&
                grow.GetGap() == oldGap,
            "invalid layout settings leave accepted settings alone");
}

void TestFlexCacheInvalidationAndCompositeBounds()
{
    ControlHost host;
    auto root    = std::make_unique<FlexPanel>();
    auto* panel  = root.get();
    auto* label  = panel->AddChild<Label>(L"Name");
    auto* field  = panel->AddChild<TextField>(L"1");
    auto* toggle = panel->AddChild<Toggle>();
    toggle->SetStateLabels(L"Off", L"On");
    auto* stepper = panel->AddChild<NumericStepper>();
    host.SetRoot(std::move(root));
    panel->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 250.0f));
    RequireSucceeded(panel->ApplyLayout(host), "arrange real leaf and composite controls");
    const auto measured = panel->DebugGetMeasureCount();
    Require(measured == 4u && stepper->Field().GetBounds().right > stepper->Field().GetBounds().left, "normal composite descendant arrangement is accepted");
    label->SetTextColor(D2D1::ColorF(D2D1::ColorF::Red));
    field->SetText(L"12345678901234567890");
    toggle->SetChecked(true);
    stepper->SetValue(123.0);
    panel->SetBounds(D2D1::RectF(10.0f, 15.0f, 310.0f, 265.0f));
    RequireSucceeded(panel->ApplyLayout(host), "paint, draft and position-only changes");
    Require(panel->DebugGetMeasureCount() == measured, "color, checked state, values and move-only changes reuse all numeric measures");
    label->SetText(L"Nom complet du document");
    RequireSucceeded(panel->ApplyLayout(host), "shape one changed label");
    Require(panel->DebugGetMeasureCount() == measured + 1u, "content mutation dirties only its own cached leaf");
    field->SetVisible(false);
    RequireSucceeded(panel->ApplyLayout(host), "omit hidden editor");
    Require(panel->DebugGetMeasureCount() == measured + 1u, "visibility changes reuse remaining siblings");
    field->SetVisible(true);
    RequireSucceeded(panel->ApplyLayout(host), "restore unchanged editor");
    Require(panel->DebugGetMeasureCount() == measured + 2u, "restored availability remeasures that leaf once");
    panel->SetDensity(Density::Compact);
    RequireSucceeded(panel->ApplyLayout(host), "density changes inherited typography and chrome");
    Require(panel->DebugGetMeasureCount() == measured + 6u, "all density-dependent leaves are remeasured once");
}

void TestFlexMeasureCallbacksCannotPublishStaleOwnershipOrGeometry()
{
    for (int scenario = 0; scenario < 5; ++scenario)
    {
        ControlHost host;
        auto root     = std::make_unique<FlexPanel>();
        auto* panel   = root.get();
        auto* probe   = panel->AddChild<MeasuredProbe>();
        auto* sibling = panel->AddChild<MeasuredProbe>();
        probe->SetFocusable(true);
        sibling->SetBounds(D2D1::RectF(70.0f, 80.0f, 90.0f, 100.0f));
        panel->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 200.0f));
        host.SetRoot(std::move(root));
        ControlHost destination;
        const auto siblingBounds = sibling->GetBounds();
        probe->measureCallback   = [&, scenario]
        {
            if (scenario == 0)
                host.SetRoot({});
            else if (scenario == 1)
                panel->ClearChildren();
            else if (scenario == 2)
                destination.SetRoot(panel->TakeChild(0u));
            else if (scenario == 3)
                sibling->SetBounds(D2D1::RectF(1.0f, 2.0f, 3.0f, 4.0f));
            else
                host.SetFocusControl(probe, false);
        };
        const auto lifetime = GetControlLifetimeToken(*panel);
        Require(panel->ApplyLayout(host) == E_ABORT, "measurement callback retirement, clearing, reparenting and foreign geometry abort stale layout");
        if (! lifetime.expired())
        {
            Require(panel->GetContentSize().width == 0.0f, "aborted measurement cannot publish content extent");
            if (scenario == 2)
                Require(SameBounds(sibling->GetBounds(), siblingBounds), "reparenting fails before any surviving sibling bounds are committed");
        }
    }
}

void TestFlexBoundsCallbacksCannotPublishRetiredOrSupersededPasses()
{
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        ControlHost host;
        auto root    = std::make_unique<FlexPanel>();
        auto* panel  = root.get();
        auto* first  = panel->AddChild<MeasuredProbe>();
        auto* second = panel->AddChild<MeasuredProbe>();
        first->SetFocusable(true);
        host.SetRoot(std::move(root));
        panel->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 200.0f));
        const auto secondBounds = second->GetBounds();
        bool nested             = false;
        first->boundsCallback   = [&, scenario]
        {
            if (scenario == 0)
                host.SetRoot({});
            else if (scenario == 1)
                panel->ClearChildren();
            else if (scenario == 2 && ! nested)
            {
                nested = true;
                RequireSucceeded(panel->ApplyLayout(host), "nested newer arrangement is authoritative");
            }
            else if (scenario == 3)
                second->SetBounds(D2D1::RectF(1.0f, 2.0f, 3.0f, 4.0f));
            else if (scenario == 4)
                host.SetFocusControl(first, false);
            else if (scenario == 5)
                second->SetVisible(false);
        };
        const auto lifetime = GetControlLifetimeToken(*panel);
        Require(panel->ApplyLayout(host) == E_ABORT, "extensible bounds callbacks cannot publish a retired or superseded pass");
        if (! lifetime.expired() && scenario == 3)
            Require(! SameBounds(second->GetBounds(), secondBounds) && panel->GetContentSize().width == 0.0f,
                    "callback-authored geometry survives while stale content extent is rejected");
        if (! lifetime.expired() && scenario == 2)
            Require(nested && panel->GetContentSize().width > 0.0f && second->GetBounds().top > first->GetBounds().top,
                    "the completed newer arrangement keeps its bounds and extent when its older caller aborts");
        if (! lifetime.expired() && (scenario == 4 || scenario == 5))
            Require(panel->GetContentSize().width == 0.0f, "focus and model callbacks cannot publish stale layout extent");
    }

    ControlHost host;
    auto root   = std::make_unique<Panel>();
    auto* outer = root->AddChild<FlexPanel>();
    auto* first = outer->AddChild<MeasuredProbe>();
    outer->AddChild<MeasuredProbe>();
    auto* foreign = root->AddChild<FlexPanel>();
    foreign->AddChild<MeasuredProbe>();
    host.SetRoot(std::move(root));
    outer->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 200.0f));
    foreign->SetBounds(D2D1::RectF(400.0f, 0.0f, 700.0f, 200.0f));
    first->boundsCallback = [&]() { RequireSucceeded(foreign->ApplyLayout(host), "newer layout outside the active branch completes"); };
    Require(outer->ApplyLayout(host) == E_ABORT && outer->GetContentSize().width == 0.0f && foreign->GetContentSize().width > 0.0f,
            "a nested foreign layout cannot disguise its geometry changes as accepted descendants of the older caller");
}

void TestFlexNestedMeasureCannotReplaceArrangementSnapshots()
{
    for (const bool attached : {false, true})
    {
        for (const auto orientation : {StackOrientation::Horizontal, StackOrientation::Vertical})
        {
            ControlHost host;
            auto root    = std::make_unique<FlexPanel>();
            auto* panel  = root.get();
            auto* first  = panel->AddChild<MeasuredProbe>();
            auto* second = panel->AddChild<MeasuredProbe>();
            RequireSucceeded(panel->SetOrientation(orientation), "nested measure fixture orientation");
            panel->SetBounds(D2D1::RectF(100.25f, 200.5f, 400.25f, 400.5f));
            second->SetBounds(D2D1::RectF(501.0f, 502.0f, 541.0f, 522.0f));
            if (attached)
                host.SetRoot(std::move(root));
            const auto secondBefore = second->GetBounds();
            bool measured           = false;
            first->boundsCallback   = [&]
            {
                if (measured)
                    return;
                measured = true;
                DesiredSize desired;
                RequireSucceeded(panel->Measure({host}, LayoutConstraints::AtMost(300.0f, 200.0f), desired),
                                 "a nested pure measure can complete during a bounds callback");
                Require(desired.sizeDip.width > 0.0f && desired.sizeDip.height > 0.0f, "nested measure returns usable desired geometry");
            };
            Require(panel->ApplyLayout(host) == E_ABORT && measured,
                    "nested measurement supersedes the shared arrangement snapshot before another child is moved");
            Require(SameBounds(second->GetBounds(), secondBefore) && panel->GetContentSize().width == 0.0f,
                    "the older pass cannot apply local scratch coordinates or publish its extent");
            first->boundsCallback = {};
            RequireSucceeded(panel->ApplyLayout(host), "a fresh arrangement succeeds after nested measurement");
            Require(second->GetBounds().left >= panel->GetBounds().left && second->GetBounds().top >= panel->GetBounds().top &&
                        panel->GetContentSize().width > 0.0f,
                    "the fresh pass translates all children to the nonzero panel origin");
        }
    }
}

void TestFlexClearChildrenPreservesCallbackAuthoredReplacementOptions()
{
    ControlHost host;
    auto root    = std::make_unique<FlexPanel>();
    auto* panel  = root.get();
    auto* sender = panel->AddChild<FlexDetachProbe>();
    RequireSucceeded(panel->SetChildOptions(sender, Fixed(40.0f, 20.0f)), "original unsupported child has an explicit extent");
    panel->SetBounds(D2D1::RectF(12.0f, 20.0f, 312.0f, 220.0f));
    host.SetRoot(std::move(root));
    const auto ownerLifetime = GetControlLifetimeToken(*panel);
    const auto oldLifetime   = GetControlLifetimeToken(*sender);
    Panel* replacement       = nullptr;
    HRESULT callbackResult   = E_UNEXPECTED;
    size_t detachCalls       = 0u;
    sender->onDetach         = std::make_shared<const std::function<void()>>([&]() noexcept
    {
        ++detachCalls;
        if (ownerLifetime.expired())
        {
            callbackResult = E_ABORT;
            return;
        }
        try
        {
            replacement    = panel->AddChild<Panel>();
            callbackResult = replacement ? panel->SetChildOptions(replacement, Fixed(73.0f, 29.0f)) : E_FAIL;
        }
        catch (const std::bad_alloc&)
        {
            callbackResult = E_OUTOFMEMORY;
        }
    });
    panel->ClearChildren();
    Require(detachCalls == 1u && callbackResult == S_OK && oldLifetime.expired() && replacement && panel->GetLogicalChildCount() == 1u,
            "the original detach callback creates and configures a live replacement");
    RequireSucceeded(panel->ApplyLayout(host), "ClearChildren preserves callback-authored explicit sizing for the unsupported replacement");
    Require(Close(replacement->GetBounds().right - replacement->GetBounds().left, 73.0f) &&
                Close(replacement->GetBounds().bottom - replacement->GetBounds().top, 29.0f),
            "the replacement receives its newer explicit extent after the older clear returns");
}

void TestFlexRepresentedEndpointsPreserveMinimaAndOrder()
{
    const HRESULT arithmetic = HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    for (const bool attached : {false, true})
    {
        for (const auto orientation : {StackOrientation::Horizontal, StackOrientation::Vertical})
        {
            for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
            {
                for (const auto origin :
                     {D2D1::Point2F(1.0e20f, 0.0f), D2D1::Point2F(-1.0e20f, 0.0f), D2D1::Point2F(0.0f, 1.0e20f), D2D1::Point2F(0.0f, -1.0e20f)})
                {
                    ControlHost host;
                    auto root    = std::make_unique<FlexPanel>();
                    auto* panel  = root.get();
                    auto* first  = panel->AddChild<MeasuredProbe>();
                    auto* second = panel->AddChild<MeasuredProbe>();
                    RequireSucceeded(panel->SetOrientation(orientation), "extreme-origin orientation");
                    panel->SetFlowDirection(flow);
                    panel->SetBounds(D2D1::RectF(10.25f, 20.5f, 410.25f, 220.5f));
                    if (attached)
                        host.SetRoot(std::move(root));
                    RequireSucceeded(panel->ApplyLayout(host), "extreme-origin fixture has an earlier valid layout");
                    const auto firstBefore  = first->GetBounds();
                    const auto secondBefore = second->GetBounds();
                    const auto content      = panel->GetContentSize();
                    const auto overflow     = panel->HasLayoutOverflow();
                    size_t callbacks        = 0u;
                    first->boundsCallback   = [&] { ++callbacks; };
                    second->boundsCallback  = [&] { ++callbacks; };
                    panel->SetBounds(D2D1::RectF(origin.x, origin.y, origin.x + 400.0f, origin.y + 200.0f));
                    Require(panel->ApplyLayout(host) == arithmetic && callbacks == 0u && SameBounds(first->GetBounds(), firstBefore) &&
                                SameBounds(second->GetBounds(), secondBefore) && panel->GetContentSize().width == content.width &&
                                panel->GetContentSize().height == content.height && panel->HasLayoutOverflow() == overflow,
                            "collapsed positive extents fail before any geometry callback and preserve the earlier published layout");
                }
            }
        }
    }
    for (const auto orientation : {StackOrientation::Horizontal, StackOrientation::Vertical})
    {
        for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
        {
            for (const float gap : {0.0f, 6.0f, 12.0f})
            {
                for (const float origin : {1.1f, 15.6f, 1023.2f, 1200.2f, -1.1f, -15.6f, -1023.2f, -1200.2f})
                {
                    ControlHost host;
                    FlexPanel panel;
                    RequireSucceeded(panel.SetOrientation(orientation), "fractional-origin orientation");
                    RequireSucceeded(panel.SetGap(gap), "fractional-origin gap");
                    panel.SetFlowDirection(flow);
                    panel.SetBounds(D2D1::RectF(origin, origin, origin + 400.0f, origin + 200.0f));
                    std::array<MeasuredProbe*, 3> children{};
                    for (auto*& child : children)
                    {
                        child                  = panel.AddChild<MeasuredProbe>();
                        child->desired.sizeDip = D2D1::SizeF(48.001f, 29.3f);
                        FlexChildOptions options;
                        options.alignment              = LayoutAlignment::Start;
                        options.constraints.minimumDip = child->desired.sizeDip;
                        RequireSucceeded(panel.SetChildOptions(child, options), "fractional-origin minima");
                    }
                    RequireSucceeded(panel.ApplyLayout(host), "ordinary fractional origins remain arrangeable");
                    std::array<D2D1_RECT_F, 3> before{};
                    for (size_t index = 0; index < children.size(); ++index)
                    {
                        before[index] = children[index]->GetBounds();
                        Require(before[index].right - before[index].left >= 48.001f && before[index].bottom - before[index].top >= 29.3f,
                                "represented endpoints preserve exact declared minima without an epsilon");
                        if (index == 0u)
                            continue;
                        const float distance = orientation == StackOrientation::Vertical ? before[index].top - before[index - 1u].bottom
                                               : flow == FlowDirection::RightToLeft      ? before[index - 1u].left - before[index].right
                                                                                         : before[index].left - before[index - 1u].right;
                        Require(distance >= gap, "corrected far edges preserve sibling order and the declared gap");
                    }
                    const auto count = panel.DebugGetMeasureCount();
                    RequireSucceeded(panel.ApplyLayout(host), "unchanged fractional arrangement repeats");
                    Require(panel.DebugGetMeasureCount() == count, "endpoint staging reuses unchanged measurement caches");
                    for (size_t index = 0; index < children.size(); ++index)
                        Require(SameBounds(children[index]->GetBounds(), before[index]), "unchanged represented geometry is stable");
                }
            }
        }
    }
    for (const auto orientation : {StackOrientation::Horizontal, StackOrientation::Vertical})
    {
        for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
        {
            for (const auto alignment : {LayoutAlignment::Start, LayoutAlignment::Center, LayoutAlignment::End, LayoutAlignment::Stretch})
            {
                for (const float origin : {0.7f, 1.1f, -0.7f, -15.6f})
                {
                    ControlHost host;
                    FlexPanel panel;
                    RequireSucceeded(panel.SetOrientation(orientation), "fractional padded orientation");
                    RequireSucceeded(panel.SetPadding(12.0f, 12.0f, 12.0f, 12.0f), "fractional padded insets");
                    panel.SetFlowDirection(flow);
                    panel.SetBounds(D2D1::RectF(origin, origin, origin + 400.0f, origin + 200.0f));
                    auto* child       = panel.AddChild<Panel>();
                    auto options      = Fixed(48.0f, 20.0f);
                    options.alignment = alignment;
                    RequireSucceeded(panel.SetChildOptions(child, options), "fractional padded child alignment");
                    RequireSucceeded(panel.ApplyLayout(host), "ordinary fractional padded layouts stay arrangeable");
                    const auto bounds = child->GetBounds();
                    const auto parent = panel.GetBounds();
                    Require(static_cast<double>(bounds.left) - parent.left >= 12.0 && static_cast<double>(bounds.top) - parent.top >= 12.0 &&
                                static_cast<double>(parent.right) - bounds.right >= 12.0 && static_cast<double>(parent.bottom) - bounds.bottom >= 12.0 &&
                                ! panel.HasLayoutOverflow(),
                            "exact represented insets remain present without reporting a false fractional-padding overflow");
                }
            }
            for (const float origin : {0.7f, 1.1f, -0.7f, -15.6f})
            {
                ControlHost host;
                FlexPanel panel;
                RequireSucceeded(panel.SetOrientation(orientation), "fractional capped growth orientation");
                RequireSucceeded(panel.SetPadding(12.0f, 12.0f, 12.0f, 12.0f), "fractional capped growth insets");
                panel.SetFlowDirection(flow);
                panel.SetBounds(D2D1::RectF(origin, origin, origin + 400.0f, origin + 200.0f));
                auto* child                          = panel.AddChild<Panel>();
                auto options                         = Fixed(30.0f, 20.0f);
                options.grow                         = 1.0f;
                options.alignment                    = LayoutAlignment::Stretch;
                options.constraints.maximumWidthDip  = 60.0f;
                options.constraints.maximumHeightDip = 60.0f;
                RequireSucceeded(panel.SetChildOptions(child, options), "fractional capped growth and Stretch");
                RequireSucceeded(panel.ApplyLayout(host), "optional capped expansion may round inward above the intrinsic minimum");
                const auto bounds   = child->GetBounds();
                const double width  = static_cast<double>(bounds.right) - bounds.left;
                const double height = static_cast<double>(bounds.bottom) - bounds.top;
                Require(width >= 59.0 && width <= 60.0 && height >= 59.0 && height <= 60.0 && ! panel.HasLayoutOverflow(),
                        "capped growth and Stretch use available represented space while preserving minima and exact caps");
            }
            ControlHost host;
            FlexPanel panel;
            RequireSucceeded(panel.SetOrientation(orientation), "coarse cross-axis orientation");
            panel.SetFlowDirection(flow);
            const auto parent = orientation == StackOrientation::Horizontal ? D2D1::RectF(0.0f, 1.0e8f, 400.0f, 1.0e8f + 128.0f)
                                                                            : D2D1::RectF(1.0e8f, 0.0f, 1.0e8f + 128.0f, 400.0f);
            panel.SetBounds(parent);
            auto* child       = panel.AddChild<Panel>();
            auto options      = Fixed(20.0f, 20.0f);
            options.alignment = LayoutAlignment::End;
            RequireSucceeded(panel.SetChildOptions(child, options), "coarse cross-axis end alignment");
            RequireSucceeded(panel.ApplyLayout(host), "end alignment uses the represented line endpoint");
            const auto before = child->GetBounds();
            Require(before.right - before.left >= 20.0f && before.bottom - before.top >= 20.0f && before.left >= parent.left && before.top >= parent.top &&
                        before.right <= parent.right && before.bottom <= parent.bottom && ! panel.HasLayoutOverflow(),
                    "corrected end alignment preserves minima inside its parent in both logical directions");
            RequireSucceeded(panel.SetPadding(4.0f, 4.0f, 4.0f, 4.0f), "positive padding at coarse coordinates");
            Require(panel.ApplyLayout(host) == arithmetic && SameBounds(child->GetBounds(), before),
                    "collapsed positive padding fails before committing child geometry");
            RequireSucceeded(orientation == StackOrientation::Horizontal ? panel.SetPadding(0.0f, 8.0f, 0.0f, 4.0f) : panel.SetPadding(8.0f, 0.0f, 4.0f, 0.0f),
                             "only the trailing positive inset collapses");
            Require(panel.ApplyLayout(host) == arithmetic && SameBounds(child->GetBounds(), before),
                    "the trailing inset is checked against the represented parent edge before child geometry");
            RequireSucceeded(orientation == StackOrientation::Horizontal ? panel.SetPadding(0.0f, 8.0f, 0.0f, 8.0f) : panel.SetPadding(8.0f, 0.0f, 8.0f, 0.0f),
                             "representable leading and trailing insets");
            RequireSucceeded(panel.ApplyLayout(host), "representable padding retains end alignment");
            const auto padded = child->GetBounds();
            Require(orientation == StackOrientation::Horizontal ? padded.top - parent.top >= 8.0f && parent.bottom - padded.bottom >= 8.0f
                                                                : padded.left - parent.left >= 8.0f && parent.right - padded.right >= 8.0f,
                    "both physical insets survive end alignment in either logical direction");
            Require(! panel.HasLayoutOverflow(), "representable end-aligned padding stays within its allocation");
        }
    }
    for (const bool grown : {false, true})
    {
        for (const bool capped : {false, true})
        {
            ControlHost host;
            FlexPanel panel;
            RequireSucceeded(panel.SetOrientation(StackOrientation::Horizontal), "coarse fractional-origin row");
            RequireSucceeded(panel.SetGap(6.0f), "coarse fractional-origin gap");
            panel.SetBounds(D2D1::RectF(1.0e8f, 0.0f, 1.0e8f + 48.0f, 100.0f));
            auto* first  = panel.AddChild<Panel>();
            auto* second = panel.AddChild<Panel>();
            auto options = Fixed(20.0f, 20.0f);
            options.grow = grown ? 1.0f : 0.0f;
            if (capped)
                options.constraints.maximumWidthDip = 20.0f;
            RequireSucceeded(panel.SetChildOptions(first, options), "coarse fractional-origin first extent");
            RequireSucceeded(panel.SetChildOptions(second, options), "coarse fractional-origin second extent");
            const auto firstBefore  = first->GetBounds();
            const auto secondBefore = second->GetBounds();
            const HRESULT result    = panel.ApplyLayout(host);
            if (capped)
                Require(result == arithmetic && SameBounds(first->GetBounds(), firstBefore) && SameBounds(second->GetBounds(), secondBefore),
                        "an incompatible hard maximum refuses partial inward rounding before geometry");
            else
            {
                RequireSucceeded(result, "uncapped partial inward rounding can be repaired in logical order");
                const auto a = first->GetBounds();
                const auto b = second->GetBounds();
                Require(a.right - a.left >= 20.0f && b.right - b.left >= 20.0f && b.left - a.right >= 6.0f &&
                            panel.GetContentSize().width >= b.right - a.left && panel.HasLayoutOverflow(),
                        "partial rounding preserves minima, spacing and truthful occupied extent/overflow");
            }
        }
    }
    ControlHost host;
    FlexPanel zero;
    auto* child = zero.AddChild<Panel>();
    RequireSucceeded(zero.SetChildOptions(child, Fixed(0.0f, 0.0f)), "zero extent remains a valid explicit size");
    zero.SetBounds(D2D1::RectF(1.1f, 2.2f, 101.1f, 102.2f));
    RequireSucceeded(zero.ApplyLayout(host), "legitimate zero-sized children remain accepted");
    Require(child->GetBounds().left == child->GetBounds().right && child->GetBounds().top == child->GetBounds().bottom,
            "zero-sized children are not expanded into visible targets");
}

void TestFlexBudgetsAndUnsupportedChildrenFailBeforeGeometryCommit()
{
    ControlHost host;
    FlexPanel unsupported;
    auto* child          = unsupported.AddChild<Panel>();
    const auto oldBounds = D2D1::RectF(4.0f, 5.0f, 6.0f, 7.0f);
    child->SetBounds(oldBounds);
    unsupported.SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 100.0f));
    Require(unsupported.ApplyLayout(host) == E_NOTIMPL && SameBounds(child->GetBounds(), oldBounds),
            "missing explicit fallback fails without geometry feedback");
    FlexPanel tooMany;
    for (size_t index = 0u; index <= FlexPanel::kMaxLayoutChildren; ++index)
        tooMany.AddChild<Label>(L"item");
    tooMany.SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 100.0f));
    Require(tooMany.ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && tooMany.DebugGetMeasureCount() == 0u,
            "oversized child sets are rejected before shaping or arranging");
    FlexPanel tooLarge;
    auto* a = tooLarge.AddChild<Panel>();
    auto* b = tooLarge.AddChild<Panel>();
    RequireSucceeded(tooLarge.SetChildOptions(a, Fixed(1.0f, (std::numeric_limits<float>::max)())), "large finite explicit extent");
    RequireSucceeded(tooLarge.SetChildOptions(b, Fixed(1.0f, (std::numeric_limits<float>::max)())), "second large finite extent");
    tooLarge.SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 100.0f));
    Require(tooLarge.ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) && SameBounds(a->GetBounds(), D2D1::RectF()),
            "aggregate arithmetic overflow fails before first child commit");
    FlexPanel tooMuchText;
    auto* nested            = tooMuchText.AddChild<FlexPanel>();
    auto* nestedLabel       = nested->AddChild<Label>(std::wstring(33'000u, L'x'));
    auto* otherLabel        = tooMuchText.AddChild<Label>(std::wstring(33'000u, L'y'));
    const auto nestedBounds = nested->GetBounds();
    tooMuchText.SetBounds(D2D1::RectF(0.0f, 0.0f, 500.0f, 300.0f));
    Require(tooMuchText.ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && SameBounds(nested->GetBounds(), nestedBounds),
            "nested containers share one shaping budget instead of resetting the limit for every leaf");
    nestedLabel->SetText(L"Short nested label");
    otherLabel->SetText(L"Short following label");
    RequireSucceeded(tooMuchText.ApplyLayout(host), "shorter current content recovers after a shaping budget failure");
    size_t remaining = 4u;
    DesiredSize untouched{.sizeDip = D2D1::SizeF(123.0f, 456.0f)};
    Require(otherLabel->Measure({host, &remaining}, {}, untouched) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && remaining == 4u &&
                untouched.sizeDip.width == 123.0f,
            "an inherited exhausted budget fails atomically before allocating text resources");
}

void TestIntrinsicPreparationIsSharedByNativeAndEmbeddedHosts()
{
    AttachedHostWindow native;
    ShowWindow(native.Hwnd(), SW_SHOWNOACTIVATE);
    auto root   = std::make_unique<FlexPanel>();
    auto* panel = root.get();
    auto* label = panel->AddChild<Label>(L"Première ligne");
    auto* next  = panel->AddChild<Button>(L"Continuer");
    native.Host().SetRoot(std::move(root));
    WindowHostBitmapCapture capture;
    Require(native.Host().DebugCaptureBitmap(capture), "native capture prepares pending intrinsic geometry");
    const auto oldTop = next->GetBounds().top;
    label->SetMultiline(true);
    label->SetText(L"Une explication beaucoup plus longue qui doit se replier et déplacer le bouton suivant lors de la préparation.");
    Require(native.Host().DebugCaptureBitmap(capture), "native preparation reflows changed content");
    Require(next->GetBounds().top > oldTop, "native preparation places following rows after measured wrapped content");
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "intrinsic embedded WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "intrinsic shared graphics pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "intrinsic embedded attach");
    auto embeddedRoot   = std::make_unique<FlexPanel>();
    auto* embeddedPanel = embeddedRoot.get();
    auto* embeddedLabel = embeddedPanel->AddChild<Label>(L"Description initiale");
    embeddedLabel->SetMultiline(true);
    auto* embeddedNext = embeddedPanel->AddChild<Button>(L"Terminer");
    view.Controls().SetRoot(std::move(embeddedRoot));
    RequireSucceeded(view.Prepare(240u, 200u, 96.0f), "initial intrinsic embedded preparation");
    const auto initialTop = embeddedNext->GetBounds().top;
    embeddedLabel->SetText(L"Une description traduite très longue qui remplit plusieurs lignes dans la largeur disponible.");
    Require(view.NeedsPreparation(), "intrinsic content invalidation requests embedded preparation");
    RequireSucceeded(view.Prepare(240u, 200u, 96.0f), "embedded retained-content reflow");
    Require(embeddedNext->GetBounds().top > initialTop, "embedded preparation uses the same measured row geometry");
    const auto measured   = embeddedPanel->DebugGetMeasureCount();
    const auto statistics = view.GetStatistics();
    graphics.Bind();
    for (int index = 0; index < 5; ++index)
        RequireSucceeded(view.Composite(graphics.context.get(), graphics.Viewport()), "unchanged intrinsic embedded composition");
    Require(embeddedPanel->DebugGetMeasureCount() == measured && view.GetStatistics().preparations == statistics.preparations,
            "cached composition performs no intrinsic measurement or preparation");
    RequireSucceeded(view.Prepare(360u, 300u, 144.0f), "DPI invalidation of intrinsic caches");
    Require(embeddedPanel->DebugGetMeasureCount() == measured + 2u, "DPI remeasures each visible leaf exactly once");
    view.SetVisible(false);
    Require(view.GetStatistics().surfaceBytes == 0u, "hidden intrinsic view releases its render surface");
    view.SetVisible(true);
    RequireSucceeded(view.Prepare(360u, 300u, 144.0f), "resume intrinsic view after hiding");
    Require(embeddedPanel->DebugGetMeasureCount() == measured + 4u, "hidden containers retire numeric shape entries and reconstruct once on resume");
}
void TestIntrinsicReflowPreservesTextDraftAndCancelsMovedPress()
{
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "reflow text/input WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "reflow text/input graphics pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "reflow text/input view");
    auto root   = std::make_unique<FlexPanel>();
    auto* panel = root.get();
    auto* label = panel->AddChild<Label>(L"Short description");
    label->SetMultiline(true);
    auto* field        = panel->AddChild<TextField>(L"draft");
    auto* button       = panel->AddChild<Button>(L"Apply");
    size_t invocations = 0u;
    button->SetOnClick([&] { ++invocations; });
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(260u, 260u), "prepare intrinsic text/input controls");
    view.Controls().SetFocusControl(field);
    EmbeddedTextInputSnapshot before;
    Require(view.ReadTextInput(before) == S_OK, "read focused application text service");
    auto composition                  = before.state;
    composition.text                  = L"候補";
    composition.caretIndex            = composition.text.size();
    composition.compositionStartIndex = 0u;
    composition.compositionEndIndex   = composition.text.size();
    RequireSucceeded(view.ApplyTextInput(before.revision, composition, EmbeddedTextInputAction::Preview), "preview an application composition draft");
    label->SetText(L"A much longer localized description that reflows several lines above the active editor and changes the following button position.");
    RequireSucceeded(view.Prepare(260u, 260u), "prepare layout while a composition draft is active");
    EmbeddedTextInputSnapshot after;
    Require(view.ReadTextInput(after) == S_OK && after.focusId == before.focusId && after.state.text == composition.text &&
                after.state.compositionStartIndex == composition.compositionStartIndex && after.state.compositionEndIndex == composition.compositionEndIndex &&
                view.Controls().GetFocusControl() == field,
            "intrinsic reflow preserves logical focus and application composition state");
    RequireSucceeded(view.ApplyTextInput(after.revision, {}, EmbeddedTextInputAction::Cancel), "cancel reflowed composition");
    Require(field->GetText() == L"draft", "composition cancellation restores the original draft after reflow");
    RequireSucceeded(view.Prepare(260u, 260u), "prepare canceled draft");
    const auto press = button->GetBounds();
    const auto event = [&](PointerAction action, const D2D1_RECT_F& bounds)
    { return PointerEvent{.action = action, .xPixels = (bounds.left + bounds.right) * 0.5f, .yPixels = (bounds.top + bounds.bottom) * 0.5f}; };
    Require(view.DispatchPointer(event(PointerAction::Down, press)), "start a press on current intrinsic geometry");
    Require(view.Controls().GetCapturedControl() == button, "button holds a logical press before reflow");
    label->SetText(L"Short again");
    RequireSucceeded(view.Prepare(260u, 260u), "reflow a currently pressed control");
    Require(view.Controls().GetCapturedControl() == nullptr, "layout preparation cancels a press whose bounds moved during the traversal");
    static_cast<void>(view.DispatchPointer(event(PointerAction::Up, button->GetBounds())));
    Require(invocations == 0u, "a stale press cannot commit against the new rectangle");
}

void TestTextFieldFontRolePreservesEmbeddedCompositionAndUpdatesGeometry()
{
    using namespace DxUi;
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "font-role embedded WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "font-role shared graphics pool");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "font-role embedded attach");

    auto root   = std::make_unique<FlexPanel>();
    auto* panel = root.get();
    auto* field = panel->AddChild<TextField>(L"iiiiiiiiiiiiiiiiiiiiiiii");
    field->SetMultiline(true);
    // Keep Body's narrow glyphs on one line while a Monospace run exceeds the text viewport,
    // so the same interior ACP proves both the role change and embedded hit-test geometry.
    RequireSucceeded(panel->SetChildOptions(field, Fixed(120.0f, 120.0f)), "font-role embedded editor extent");
    RequireSucceeded(panel->SetOrientation(StackOrientation::Vertical), "font-role embedded editor flow");
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(320u, 180u, 96.0f), "prepare embedded Body-role editor");
    view.Controls().SetFocusControl(field);
    RequireSucceeded(view.Prepare(320u, 180u, 96.0f), "prepare embedded Body-role caret geometry after focus changes");

    EmbeddedTextInputSnapshot before;
    Require(view.ReadTextInput(before) == S_OK && before.caretBoundsDip.has_value(), "embedded Body-role text geometry is published");
    NativeTextInputState composition  = before.state;
    composition.selectionAnchorIndex  = 2u;
    composition.caretIndex            = 6u;
    composition.compositionStartIndex = 2u;
    composition.compositionEndIndex   = 6u;
    RequireSucceeded(view.ApplyTextInput(before.revision, composition, EmbeddedTextInputAction::Preview), "preview an embedded composition before role change");
    EmbeddedTextInputSnapshot preview;
    Require(view.ReadTextInput(preview) == S_OK && preview.state.compositionStartIndex == composition.compositionStartIndex,
            "embedded composition is active before changing typography");

    D2D1_RECT_F bodyCaret{};
    Require(field->DebugGetCaretRect(view.Controls(), 18u, bodyCaret), "embedded Body role measures a wrapped caret");
    field->SetFontRole(FontRole::Monospace);
    Require(view.NeedsPreparation(), "font role invalidation requests event-driven embedded preparation");
    RequireSucceeded(view.Prepare(320u, 180u, 96.0f), "prepare embedded Monospace editor");
    EmbeddedTextInputSnapshot after;
    Require(view.ReadTextInput(after) == S_OK && after.state.text == preview.state.text &&
                after.state.selectionAnchorIndex == preview.state.selectionAnchorIndex && after.state.caretIndex == preview.state.caretIndex &&
                after.state.compositionStartIndex == preview.state.compositionStartIndex &&
                after.state.compositionEndIndex == preview.state.compositionEndIndex,
            "font role reflow preserves embedded draft, selection and active composition");
    D2D1_RECT_F monoCaret{};
    Require(field->DebugGetCaretRect(view.Controls(), 18u, monoCaret) && monoCaret.top > bodyCaret.top + 4.0f,
            "embedded caret geometry uses the selected Monospace role");
    size_t hitIndex                = 0u;
    const D2D1_POINT_2F caretPoint = D2D1::Point2F(monoCaret.left + 0.1f, (monoCaret.top + monoCaret.bottom) * 0.5f);
    Require(view.HitTestTextInput(after.revision, caretPoint, hitIndex) == S_OK && hitIndex == 18u,
            "embedded text-service hit testing resolves the refreshed Monospace caret at ACP 18");
    RequireSucceeded(view.ApplyTextInput(after.revision, {}, EmbeddedTextInputAction::Cancel), "cancel embedded composition after typography test");
    Require(field->GetText() == L"iiiiiiiiiiiiiiiiiiiiiiii", "font role and composition cancellation preserve the underlying draft");
}

void TestTextFieldFontRoleReturnsAfterEmbeddedPreparationOwnerRetires()
{
    using namespace DxUi;
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "font-role retirement WARP fixture");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "font-role retirement graphics pool");

    EmbeddedHost view;
    struct RetirementContext
    {
        ControlHost* controls = nullptr;
        bool armed            = false;
    } retirement{&view.Controls()};
    const EmbeddedCallbacks callbacks{
        .context = &retirement,
        .requestPreparation =
            [](void* context) noexcept
    {
        const auto* state = static_cast<RetirementContext*>(context);
        if (state->armed)
            state->controls->SetRoot(std::unique_ptr<Control>{});
    },
    };
    RequireSucceeded(view.Attach(device, callbacks), "font-role retirement embedded attach");
    auto root   = std::make_unique<FlexPanel>();
    auto* field = root->AddChild<TextField>(L"draft");
    field->SetMultiline(true);
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(240u, 120u, 96.0f), "prepare font-role retirement editor");
    retirement.armed = true;

    // The preparation callback retires the control through its host when the setter invalidates layout.
    // It does not re-enter EmbeddedHost; the setter must observe its expired control lifetime before continuing.
    field->SetFontRole(FontRole::Monospace);
    Require(view.Controls().GetRoot() == nullptr, "font-role setter returns safely after preparation retires its control");
}
} // namespace

void RunIntrinsicLayoutTests()
{
    DXUI_RUN_TEST(TestIntrinsicMeasurementContainsFailuresAndPreservesOutputs);
    DXUI_RUN_TEST(TestIntrinsicLeavesShapeUnicodeAndKeepEditorsStable);
    DXUI_RUN_TEST(TestLabelTypographyCharacterSpacingUsesRequestedTotal);
    DXUI_RUN_TEST(TestButtonContentFontSizeDefaultsValidationAndNormalization);
    DXUI_RUN_TEST(TestButtonContentFontSizeShapesIntrinsicVariants);
    DXUI_RUN_TEST(TestButtonContentFontSizeNativeGlyphPaintCacheAndGeneration);
    DXUI_RUN_TEST(TestButtonContentFontSizeSurvivesEmbeddedMeasureAndRetirementCallbacks);
    DXUI_RUN_TEST(TestLabelAndSeparatorTypographySurviveEmbeddedMeasureAndRetirementCallbacks);
    DXUI_RUN_TEST(TestFlexFlowWrapGrowthRtlAndOverflow);
    DXUI_RUN_TEST(TestFlexCacheInvalidationAndCompositeBounds);
    DXUI_RUN_TEST(TestFlexMeasureCallbacksCannotPublishStaleOwnershipOrGeometry);
    DXUI_RUN_TEST(TestFlexBoundsCallbacksCannotPublishRetiredOrSupersededPasses);
    DXUI_RUN_TEST(TestFlexNestedMeasureCannotReplaceArrangementSnapshots);
    DXUI_RUN_TEST(TestFlexClearChildrenPreservesCallbackAuthoredReplacementOptions);
    DXUI_RUN_TEST(TestFlexRepresentedEndpointsPreserveMinimaAndOrder);
    DXUI_RUN_TEST(TestFlexBudgetsAndUnsupportedChildrenFailBeforeGeometryCommit);
    DXUI_RUN_TEST(TestIntrinsicPreparationIsSharedByNativeAndEmbeddedHosts);
    DXUI_RUN_TEST(TestIntrinsicReflowPreservesTextDraftAndCancelsMovedPress);
    DXUI_RUN_TEST(TestTextFieldFontRolePreservesEmbeddedCompositionAndUpdatesGeometry);
    DXUI_RUN_TEST(TestTextFieldFontRoleReturnsAfterEmbeddedPreparationOwnerRetires);
}
