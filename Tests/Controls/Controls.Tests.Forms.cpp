#include "Controls.Tests.Forms.h"

#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/ControlCatalog.h>

#include <limits>
#include <new>

namespace
{
using namespace DxUi;

bool Near(float a, float b) noexcept
{
    return std::abs(a - b) < 0.02f;
}
bool Same(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
bool FiniteRect(const D2D1_RECT_F& rect) noexcept
{
    return std::isfinite(rect.left) && std::isfinite(rect.top) && std::isfinite(rect.right) && std::isfinite(rect.bottom) && rect.right >= rect.left &&
           rect.bottom >= rect.top;
}

// Extensible editor whose callbacks exercise the same public boundary as an application custom control.
class FormProbe final : public Control
{
public:
    std::function<void()> onMeasure;
    std::function<void()> onBounds;
    HRESULT measureResult = S_OK;
    void Paint(ControlHost&) const override
    {
    }

protected:
    HRESULT MeasureOverride(const MeasureContext&, const LayoutConstraints&, DesiredSize& desired) const override
    {
        const auto callback = onMeasure;
        const auto result   = measureResult;
        desired.sizeDip     = D2D1::SizeF(120.0f, 38.0f);
        if (callback)
            callback();
        return result;
    }
    void OnBoundsChanged() noexcept override
    {
        const auto callback = onBounds;
        if (callback)
            callback();
    }
};

// A custom editor declares a real intrinsic width, and can fail a later measurement without mutating its owner.
class FormMinimumProbe final : public Control
{
public:
    explicit FormMinimumProbe(float widthDip) : _widthDip(widthDip)
    {
    }
    mutable size_t measureCalls = 0;
    size_t failFromMeasure      = 0;
    bool growWithAvailableWidth = false;
    void RequestRemeasure() noexcept
    {
        InvalidateMeasure();
    }
    void Paint(ControlHost&) const override
    {
    }

protected:
    HRESULT MeasureOverride(const MeasureContext&, const LayoutConstraints& constraints, DesiredSize& desired) const override
    {
        ++measureCalls;
        if (failFromMeasure && measureCalls >= failFromMeasure)
            return E_FAIL;
        desired.sizeDip = D2D1::SizeF(growWithAvailableWidth ? constraints.maximumWidthDip.value_or(_widthDip) + 1.0f : _widthDip, 38.0f);
        return S_OK;
    }

private:
    float _widthDip;
};

class FormWrappedLaneProbe final : public Panel
{
public:
    FormWrappedLaneProbe()
    {
        text = AddChild<Label>(
            L"alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu");
        text->SetMultiline(true);
    }
    Label* text                 = nullptr;
    mutable float measuredWidth = 0.0f;
    mutable size_t measureCalls = 0;
    size_t measuresAtBounds     = 0;

protected:
    HRESULT MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& desired) const override
    {
        measuredWidth = constraints.maximumWidthDip.value_or(0.0f);
        ++measureCalls;
        return text->Measure(context, constraints, desired);
    }
    void OnBoundsChanged() noexcept override
    {
        measuresAtBounds = measureCalls;
        text->SetBounds(GetBounds());
    }
};

class FormDetachProbe final : public Control
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
        // No sender access follows the callback or the retained callable's final release.
    }
};

class NestedFormEditor final : public Panel
{
public:
    NestedFormEditor()
    {
        flex  = AddChild<FlexPanel>();
        label = flex->AddChild<Label>(L"Nested label");
        value = flex->AddChild<TextField>(L"draft");
    }
    FlexPanel* flex      = nullptr;
    Label* label         = nullptr;
    TextField* value     = nullptr;
    HRESULT nestedResult = E_UNEXPECTED;

protected:
    void OnBoundsChanged() noexcept override
    {
        flex->SetBounds(GetBounds());
        if (GetHost())
            nestedResult = flex->ApplyLayout(*GetHost());
    }
};

void TestFormsMeasureIsPureAndSharedLabelCacheIsStable()
{
    ControlHost host;
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* first  = form->AddField(L"width", L"Largeur du dessin");
    auto* second = form->AddField(L"height", L"Hauteur");
    Require(first && second && first->AddEditor<NumericStepper>() && second->AddEditor<TextField>(), "form owns measured editors");
    first->SetDescription(L"La taille du document e\u0301 \U0001F642 العربية.");
    RequireSucceeded(first->SetValidation(ValidationState::Error, L"Saisissez une valeur positive."), "textual validation");
    RequireSucceeded(form->SetLabelPlacement(LabelPlacement::Beside), "shared beside labels");
    host.SetRoot(std::move(root));
    form->SetBounds(D2D1::RectF(10, 20, 470, 400));
    Require(host.GetTypographyRevision() == 0, "fresh form host has not initialized device-independent typography");
    const auto revision    = form->GetMeasureRevision();
    const auto firstBounds = first->GetBounds();
    DesiredSize desired;
    RequireSucceeded(form->Measure({host}, LayoutConstraints::AtMost(460.0f, std::nullopt), desired), "first contextual beside measure succeeds");
    Require(host.GetTypographyRevision() != 0, "first form measure initializes typography before caching its epoch");
    Require(form->GetMeasureRevision() == revision && Same(first->GetBounds(), firstBounds), "measure changes no input revision or child rectangle");
    Require(desired.sizeDip.height > 80.0f, "descriptions and validation contribute real wrapped height");
    const auto measured = form->DebugGetMeasureCount();
    RequireSucceeded(form->ApplyLayout(host), "arrange the same measured width");
    Require(form->DebugGetMeasureCount() == measured, "the first arrangement reuses contextual label and editor measures");
    Require(Near(first->GetEditor()->GetBounds().left, second->GetEditor()->GetBounds().left), "visible fields share the same measured label column");
    for (int repeat = 0; repeat < 8; ++repeat)
        RequireSucceeded(form->ApplyLayout(host), "unchanged form preparation");
    first->GetLabelControl()->SetTextColor(D2D1::ColorF(D2D1::ColorF::Red));
    RequireSucceeded(form->ApplyLayout(host), "paint-only label update");
    Require(form->DebugGetMeasureCount() == measured, "unchanged and color-only shared labels perform no new shaping");
    first->SetLabel(L"L");
    RequireSucceeded(form->ApplyLayout(host), "changed shared label");
    Require(form->DebugGetMeasureCount() > measured && Same(first->GetBounds(), firstBounds) == false, "changed label recomputes relevant intrinsic records");
    second->SetVisible(false);
    RequireSucceeded(form->ApplyLayout(host), "shared labels omit hidden fields");
    Require(Near(form->GetContentSize().height, first->GetBounds().bottom - form->GetBounds().top), "hidden fields reserve no row");
}

void TestFormsLocalizedGeometryMatrix()
{
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "form matrix WARP device");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "form matrix shared graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "form matrix embedded attachment");
    auto root    = std::make_unique<Panel>();
    auto* form   = root->AddChild<FormPanel>();
    auto* first  = form->AddField(L"name", L"Nom du document e\u0301 \U0001F642 العربية");
    auto* second = form->AddField(L"mode", L"Une légende française volontairement longue");
    auto* third  = form->AddField(L"count", L"Nombre de pages");
    auto* text   = first->AddEditor<TextField>(L"brouillon conservé");
    auto* combo  = second->AddEditor<ComboBox>();
    auto* number = third->AddEditor<NumericStepper>();
    Require(text && combo && number, "matrix has distinct stable editor kinds");
    first->SetDescription(L"Cette description explique la valeur et doit se replier dans la largeur réellement disponible.");
    RequireSucceeded(second->SetValidation(ValidationState::Error, L"Choisissez une valeur valide avant de confirmer les modifications."), "matrix long error");
    RequireSucceeded(form->SetPadding(8), "matrix card inset");
    view.Controls().SetRoot(std::move(root));
    for (const float dpi : {96.0f, 144.0f, 192.0f})
        for (const auto density : {Density::Standard, Density::Compact})
            for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
                for (const size_t columns : {1u, 2u})
                    for (const auto placement : {LabelPlacement::Above, LabelPlacement::Beside})
                        for (const float width : {160.0f, 360.0f, 680.0f})
                        {
                            form->SetDensity(density);
                            form->SetFlowDirection(flow);
                            RequireSucceeded(form->SetColumns(columns), "matrix column policy");
                            RequireSucceeded(form->SetLabelPlacement(placement), "matrix label policy");
                            form->SetBounds(D2D1::RectF(12, 12, 12 + width, 812));
                            RequireSucceeded(view.Prepare(static_cast<UINT>((width + 24) * dpi / 96), static_cast<UINT>(840 * dpi / 96), dpi),
                                             "matrix preparation");
                            for (auto* field : {first, second, third})
                            {
                                Require(FiniteRect(field->GetBounds()) && FiniteRect(field->GetEditor()->GetBounds()),
                                        "localized fields publish finite ordered geometry");
                                const auto editor = field->GetEditor()->GetBounds();
                                Require(editor.right - editor.left >= 47.99f && editor.bottom - editor.top >= 31.99f,
                                        "reflow preserves explicit editor minimums");
                                const auto label = field->GetLabelControl()->GetBounds();
                                if (placement == LabelPlacement::Above)
                                    Require(label.bottom <= editor.top, "above labels never overlap editors");
                                else if (flow == FlowDirection::LeftToRight)
                                    Require(label.right <= editor.left, "LTR label and editor lanes do not overlap");
                                else
                                    Require(editor.right <= label.left, "RTL editor and label lanes do not overlap");
                            }
                            if (columns == 2u)
                            {
                                Require(Near(first->GetBounds().top, second->GetBounds().top) && third->GetBounds().top >= first->GetBounds().bottom,
                                        "two-column rows reserve the tallest field");
                                Require(flow == FlowDirection::LeftToRight ? first->GetBounds().left < second->GetBounds().left
                                                                           : first->GetBounds().left > second->GetBounds().left,
                                        "RTL mirrors columns without changing logical order");
                            }
                            else
                                Require(second->GetBounds().top >= first->GetBounds().bottom, "one-column rows do not overlap");
                            if (columns == 2u && placement == LabelPlacement::Beside && width == 160.0f)
                                Require(form->GetLayoutOverflow() && form->GetContentSize().width > width, "narrow beside forms report minimum-width overflow");
                            Require(form->GetLogicalChild(0) == first && form->GetLogicalChild(1) == second && first->GetEditor() == text &&
                                        second->GetEditor() == combo && third->GetEditor() == number && text->GetText() == L"brouillon conservé",
                                    "DPI, density, flow and column policies preserve tree identity and drafts");
                            const auto count = form->DebugGetMeasureCount();
                            RequireSucceeded(view.Prepare(static_cast<UINT>((width + 24) * dpi / 96), static_cast<UINT>(840 * dpi / 96), dpi),
                                             "unchanged matrix preparation");
                            Require(form->DebugGetMeasureCount() == count, "unchanged matrix performs no new measurement");
                        }
}

void TestFormsConstraintsBudgetsAndCustomFallbackFailBeforeGeometry()
{
    ControlHost host;
    constexpr auto longLabel = L"Une légende très longue pour cet éditeur personnalisé qui se replie dans la largeur préférée et conserve une hauteur plus "
                               L"petite dans une largeur supérieure.";
    FormField standalone(L"custom", longLabel);
    auto* custom = standalone.AddEditor<Panel>();
    Require(custom != nullptr && standalone.AddEditor<TextField>() == nullptr, "a field owns one editor");
    RequireSucceeded(standalone.SetEditorPreferredSize(D2D1::SizeF(210, 43)), "explicit unsupported-editor fallback");
    DesiredSize narrow, wide, minimum;
    RequireSucceeded(standalone.Measure({host}, LayoutConstraints::AtMost(180.0f, std::nullopt), narrow), "narrow field measurement");
    RequireSucceeded(standalone.Measure({host}, LayoutConstraints::AtMost(420.0f, std::nullopt), wide), "wide field measurement");
    RequireSucceeded(standalone.Measure({host}, {.minimumDip = D2D1::SizeF(420, 0)}, minimum), "unbounded field honors minimum width in shaping");
    Require(narrow.sizeDip.height > wide.sizeDip.height && Near(minimum.sizeDip.height, wide.sizeDip.height),
            "a minimum-width field shapes at its requested width rather than stale preferred width");
    FormPanel minimumForm;
    auto* minimumField = minimumForm.AddField(L"minimum", longLabel);
    minimumField->AddEditor<TextField>();
    DesiredSize minimumFormSize, explicitFormSize;
    RequireSucceeded(minimumForm.Measure({host}, {.minimumDip = D2D1::SizeF(420, 0)}, minimumFormSize), "form minimum-width measurement");
    RequireSucceeded(minimumForm.Measure({host}, LayoutConstraints::AtMost(420.0f, std::nullopt), explicitFormSize),
                     "equivalent explicit-width form measurement");
    Require(Near(minimumFormSize.sizeDip.height, explicitFormSize.sizeDip.height), "minimum width also participates in the form's contextual child measure");
    standalone.SetBounds(D2D1::RectF(5, 6, 305, 206));
    RequireSucceeded(standalone.ApplyLayout(host), "legacy custom editor arranges from explicit fallback");
    Require(Near(custom->GetBounds().bottom - custom->GetBounds().top, 43), "custom preferred height remains authoritative");
    const auto before = custom->GetBounds();
    const auto nan    = std::numeric_limits<float>::quiet_NaN();
    Require(standalone.SetEditorPreferredSize(D2D1::SizeF(nan, 32)) == E_INVALIDARG && standalone.SetEditorPreferredSize(D2D1::SizeF(47, 32)) == E_INVALIDARG &&
                standalone.SetEditorPreferredSize(D2D1::SizeF(180, 0)) == E_INVALIDARG,
            "invalid preferred minima are rejected before presentation changes");
    Require(standalone.SetValidation(ValidationState::Error, {}) == E_INVALIDARG &&
                standalone.SetValidation(static_cast<ValidationState>(255), L"invalid") == E_INVALIDARG,
            "invalid validation state and unexplained error are rejected");
    standalone.SetBounds(D2D1::RectF(nan, 0, 200, 100));
    Require(standalone.ApplyLayout(host) == E_INVALIDARG && Same(custom->GetBounds(), before), "nonfinite origin fails before child geometry");
    FormPanel form;
    auto* first  = form.AddField(L"one", std::wstring(35000, L'x'));
    auto* second = form.AddField(L"two", std::wstring(35000, L'y'));
    Require(first && second && first->AddEditor<TextField>() && second->AddEditor<TextField>(), "budget fixture fields");
    form.SetBounds(D2D1::RectF(0, 0, 350, 200));
    const auto oldFirst = first->GetBounds();
    Require(form.ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW) && Same(first->GetBounds(), oldFirst),
            "aggregate label shaping budget fails before the first field commit");
    second->SetLabel(L"Small");
    RequireSucceeded(form.ApplyLayout(host), "failed budget pass can recover after reducing content");
    const auto recovered = first->GetBounds();
    Require(form.SetColumns(3) == E_INVALIDARG && form.SetLabelPlacement(static_cast<LabelPlacement>(255)) == E_INVALIDARG &&
                form.SetGap(-1, 2) == E_INVALIDARG && form.SetPadding(nan) == E_INVALIDARG,
            "invalid policies leave accepted layout inputs unchanged");
    RequireSucceeded(form.SetPadding((std::numeric_limits<float>::max)()), "finite padding accepted as input");
    Require(form.ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) && Same(first->GetBounds(), recovered),
            "finite arithmetic overflow is rejected before geometry");
    FormPanel inventory;
    Require(inventory.AddField({}, L"missing") == nullptr, "field identity is mandatory");
    for (size_t index = 0; index < 256; ++index)
        Require(inventory.AddField(std::to_wstring(index), L"Field") != nullptr, "bounded field inventory accepts its documented capacity");
    Require(inventory.AddField(L"257", L"extra") == nullptr && inventory.AddField(L"0", L"duplicate") == nullptr,
            "capacity and duplicate field identities refuse additions");
    FormPanel invalid;
    invalid.AddChild<Button>(L"place actions outside the body");
    invalid.SetBounds(D2D1::RectF(0, 0, 300, 200));
    Require(invalid.ApplyLayout(host) == E_INVALIDARG, "visible non-field children cannot silently disappear in form layout");
}

// A smaller preferred size shrinks an editor without intrinsic measurement; an editor or a text part that leaves the field
// takes none of the field's semantics with it and leaves no pointer behind; removed fields free their place under the cap.
void TestFormsPartsLeaveCleanlyAndPreferencesShrink()
{
    ControlHost host;
    FormField field(L"custom", L"&Name");
    auto* custom = field.AddEditor<Panel>();
    Require(custom != nullptr, "the custom editor is added");
    field.SetBounds(D2D1::RectF(0, 0, 400, 300));
    RequireSucceeded(field.SetEditorPreferredSize(D2D1::SizeF(300, 60)), "a tall preferred size");
    RequireSucceeded(field.ApplyLayout(host), "the tall editor arranges");
    Require(Near(custom->GetBounds().bottom - custom->GetBounds().top, 60), "the editor takes the tall preference");
    RequireSucceeded(field.SetEditorPreferredSize(D2D1::SizeF(120, 40)), "a smaller preferred size");
    RequireSucceeded(field.ApplyLayout(host), "the smaller editor arranges");
    Require(Near(custom->GetBounds().bottom - custom->GetBounds().top, 40), "the editor shrinks to the smaller preference");

    Label* const label = field.GetLabelControl();
    Require(label && label->GetMnemonicTarget() == custom && custom->GetAccessibleAutomationId() == L"custom",
            "the label's access key and the field's identity point at the editor");
    std::unique_ptr<Control> taken;
    for (size_t index = 0; index < field.GetChildren().size() && ! taken; ++index)
        if (field.GetChildren()[index].get() == custom)
            taken = field.TakeChild(index);
    Require(taken.get() == custom && field.GetEditor() == nullptr, "the editor leaves the field");
    Require(label->GetMnemonicTarget() == nullptr, "the label's access key no longer names the departed editor");
    Require(taken->GetAccessibleAutomationId().empty() && taken->GetAccessibleHelpText().empty(), "the departed editor keeps none of the field's semantics");
    taken.reset();

    // A text part moved out through GetChildren() is forgotten, never used again.
    for (auto& child : field.GetChildren())
        if (child.get() == label)
            child.reset();
    Require(field.GetLabelControl() == nullptr, "a label moved out of the field is no longer its label");
    field.SetLabel(L"Renamed");
    field.SetDescription(L"Help");
    RequireSucceeded(field.SetValidation(ValidationState::Error, L"Required"), "validation after the label left");
    RequireSucceeded(field.ApplyLayout(host), "the field lays out without its label");

    FormPanel inventory;
    for (size_t round = 0; round < 3; ++round)
    {
        for (size_t index = 0; index < 256; ++index)
            Require(inventory.AddField(std::to_wstring(round * 1000u + index), L"Field") != nullptr, "the cap admits live fields only");
        while (! inventory.GetChildren().empty() && std::ranges::any_of(inventory.GetChildren(), [](const auto& child) { return child != nullptr; }))
        {
            for (size_t index = 0; index < inventory.GetChildren().size(); ++index)
                if (inventory.GetChildren()[index])
                    static_cast<void>(inventory.TakeChild(index));
        }
    }
    Require(inventory.AddField(L"after", L"Field") != nullptr, "fields removed earlier free their place under the cap");
}

// Review of the later merges (N-18): an empty label is hidden, so the editor's LabeledBy never names an element with no name.
void TestFormsHideAnEmptyLabel()
{
    FormField field(L"unnamed", L"");
    Label* const label = field.GetLabelControl();
    Require(label != nullptr && ! label->IsVisible(), "a field created without a label hides its label part");
    field.SetLabel(L"Name");
    Require(label->IsVisible() && label->GetText() == L"Name", "giving the field a label shows it");
    field.SetLabel(L"");
    Require(! label->IsVisible(), "clearing the label hides it again");
}

void TestFormsFractionalTranslationFailsBeforeAnyChildGeometry()
{
    ControlHost host;
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* first  = form->AddField(L"first", L"First");
    auto* second = form->AddField(L"second", L"Second");
    Require(first && second, "fractional form owns two identified fields");
    auto* firstEditor = first->AddEditor<TextField>(L"unchanged draft");
    auto* failing     = second->AddEditor<FormMinimumProbe>(120.0f);
    Require(firstEditor && failing, "fractional form has a custom failable editor");
    failing->failFromMeasure = 2;
    RequireSucceeded(form->SetColumns(2), "fractional form columns");
    host.SetRoot(std::move(root));
    form->SetBounds(D2D1::RectF(1.1f, 5.3f, 681.9f, 305.3f));

    std::array<Control*, 10> controls{first,
                                      first->GetLogicalChild(0),
                                      first->GetLogicalChild(1),
                                      first->GetLogicalChild(2),
                                      firstEditor,
                                      second,
                                      second->GetLogicalChild(0),
                                      second->GetLogicalChild(1),
                                      second->GetLogicalChild(2),
                                      failing};
    std::array<D2D1_RECT_F, 10> before;
    for (size_t index = 0; index < controls.size(); ++index)
    {
        const float offset = static_cast<float>(index) * 8.0f;
        before[index]      = D2D1::RectF(30.0f + offset, 40.0f, 90.0f + offset, 70.0f);
        controls[index]->SetBounds(before[index]);
    }
    const auto unchanged = [&]() noexcept
    {
        for (size_t index = 0; index < controls.size(); ++index)
            if (! Same(controls[index]->GetBounds(), before[index]))
                return false;
        return true;
    };

    // Translation can change the second field's width by one float ULP. Any second measurement must still
    // precede all geometry. Measuring directly at the final width may avoid that second measurement entirely.
    const HRESULT result = form->ApplyLayout(host);
    if (FAILED(result))
    {
        Require(result == E_FAIL && failing->measureCalls == 2, "the injected second-measure failure reaches the form boundary");
        Require(unchanged() && form->GetContentSize().width == 0.0f,
                "fractional remeasurement fails before either field or any label/editor/help/error rectangle changes");
    }
    else
    {
        Require(failing->measureCalls == 1 && form->GetContentSize().width > 0.0f,
                "a form measured directly at translated widths needs no failable commit-time remeasurement");
    }

    for (size_t index = 0; index < controls.size(); ++index)
        before[index] = controls[index]->GetBounds();
    const auto contentBefore = form->GetContentSize();
    failing->failFromMeasure = failing->measureCalls + 1;
    failing->RequestRemeasure();
    Require(form->ApplyLayout(host) == E_FAIL && unchanged() && Near(form->GetContentSize().width, contentBefore.width) &&
                Near(form->GetContentSize().height, contentBefore.height),
            "an explicitly invalidated custom measure failure also preserves every pre-existing rectangle and published extent");
}

void TestFormsFractionalBesidePartsMeasureTheirCommittedWidths()
{
    ControlHost host;
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* field  = form->AddField(L"fractional", L"Un libellé précis — é 東京");
    auto* editor = field->AddEditor<FormWrappedLaneProbe>();
    Require(editor != nullptr, "fractional beside fixture owns a wrapped custom editor");
    field->SetDescription(L"Cette description se replie dans la largeur exacte de l’éditeur et reste lisible après une translation fractionnaire.");
    RequireSucceeded(field->SetValidation(ValidationState::Error, L"Indiquez une valeur complète et valide avant de poursuivre dans cette présentation."),
                     "fractional beside fixture has a wrapped explanation");
    RequireSucceeded(form->SetLabelPlacement(LabelPlacement::Beside), "fractional beside policy");
    host.SetRoot(std::move(root));
    bool exercisedRounding = false;
    for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
        for (const float origin : {1.1f, 15.6f, 351.5f, 400.1f, 1200.2f})
        {
            form->SetFlowDirection(flow);
            form->SetBounds(D2D1::RectF(origin, 5.3f, origin + 340.4f, 505.3f));
            const auto bounds = form->GetBounds();
            const float width = bounds.right - bounds.left;
            const float cap   = (std::min)(width * 0.45f, width - 48.0f - 12.0f);
            DesiredSize sharedLabel;
            RequireSucceeded(field->GetLabelControl()->Measure({host}, LayoutConstraints::AtMost(cap), sharedLabel), "fractional shared-label measurement");
            const float logicalLabelWidth  = (std::min)(cap, sharedLabel.sizeDip.width);
            const float formerOfferedWidth = width - (logicalLabelWidth + 12.0f);
            RequireSucceeded(form->ApplyLayout(host), "fractional beside preparation stages represented part widths");
            const auto editorBounds      = editor->GetBounds();
            const float representedWidth = editorBounds.right - editorBounds.left;
            exercisedRounding            = exercisedRounding || formerOfferedWidth != representedWidth;
            Require(editor->measuredWidth == representedWidth && editor->measureCalls == editor->measuresAtBounds,
                    "wrapped editor receives the exact offered width and is not remeasured during commit");
            DesiredSize expectedEditor;
            RequireSucceeded(editor->text->Measure({host}, LayoutConstraints::AtMost(representedWidth), expectedEditor),
                             "measure editor text at committed width");
            Require(expectedEditor.sizeDip.height > 32.0f && Near(editorBounds.bottom - editorBounds.top, expectedEditor.sizeDip.height),
                    "fractional editor preserves its complete wrapped height");
            for (const size_t part : {0u, 1u, 2u})
            {
                const auto* text     = field->GetLogicalChild(part);
                const auto rectangle = text->GetBounds();
                DesiredSize expected;
                RequireSucceeded(text->Measure({host}, LayoutConstraints::AtMost(rectangle.right - rectangle.left), expected),
                                 "measure form text at its committed width");
                Require(Near(rectangle.bottom - rectangle.top, expected.sizeDip.height),
                        "shared label, description and error heights match their represented wrap widths");
            }
        }
    Require(exercisedRounding, "fractional cases exercise distinct algebraic and represented lane widths");
}

void TestFormsIntrinsicEditorMinimaDriveNarrowContentAndCache()
{
    for (const bool custom : {false, true})
        for (const auto flow : {FlowDirection::LeftToRight, FlowDirection::RightToLeft})
            for (const size_t columns : {1u, 2u})
                for (const auto placement : {LabelPlacement::Above, LabelPlacement::Beside})
                {
                    ControlHost host;
                    auto root  = std::make_unique<FormPanel>();
                    auto* form = root.get();
                    auto* wide = form->AddField(L"wide", L"Value");
                    Require(wide != nullptr, "narrow form owns its intrinsically wide field");
                    FormMinimumProbe* probe = nullptr;
                    NumericStepper* stepper = nullptr;
                    if (custom)
                        probe = wide->AddEditor<FormMinimumProbe>(300.0f);
                    else
                    {
                        stepper = wide->AddEditor<NumericStepper>();
                        Require(stepper != nullptr, "narrow form owns its numeric editor");
                        stepper->SetUnit(L"pixels", 64.0f);
                    }
                    Require(wide->GetEditor() != nullptr, "narrow form binds one stable editor");
                    FormField* sibling = nullptr;
                    if (columns == 2u)
                    {
                        sibling = form->AddField(L"sibling", L"Other");
                        Require(sibling && sibling->AddEditor<TextField>(), "a narrower sibling shares the two-column form");
                    }
                    RequireSucceeded(form->SetColumns(columns), "narrow column policy");
                    RequireSucceeded(form->SetLabelPlacement(placement), "narrow label policy");
                    form->SetFlowDirection(flow);
                    host.SetRoot(std::move(root));
                    form->SetBounds(D2D1::RectF(12.0f, 20.0f, 72.0f, 420.0f));

                    DesiredSize editorMinimum;
                    RequireSucceeded(wide->GetEditor()->Measure({host}, LayoutConstraints::AtMost(48.0f, std::nullopt), editorMinimum),
                                     "editor declares its intrinsic size even under a narrow constraint");
                    Require(editorMinimum.overflow && editorMinimum.sizeDip.width > 60.0f, "fixture requires real overflow beyond the form's available width");
                    DesiredSize formMinimum;
                    RequireSucceeded(form->Measure({host}, LayoutConstraints::AtMost(60.0f, std::nullopt), formMinimum), "measure narrow intrinsic form");
                    Require(formMinimum.overflow && formMinimum.sizeDip.width >= editorMinimum.sizeDip.width,
                            "form desired extent retains successful editor intrinsic minima");
                    RequireSucceeded(form->ApplyLayout(host), "arrange overflowing intrinsic form");
                    const auto editorBounds = wide->GetEditor()->GetBounds();
                    Require(editorBounds.right - editorBounds.left + 0.02f >= editorMinimum.sizeDip.width && form->GetLayoutOverflow() &&
                                Near(formMinimum.sizeDip.width, form->GetContentSize().width),
                            "overflow is reported with matching measured/arranged extent and a usable intrinsic editor rectangle");
                    if (stepper)
                    {
                        DesiredSize textMinimum;
                        RequireSucceeded(stepper->Field().Measure({host}, LayoutConstraints::AtMost(0.0f, std::nullopt), textMinimum),
                                         "numeric semantic editor declares its text/chrome minimum");
                        const auto textBounds = stepper->Field().GetBounds();
                        Require(textBounds.right - textBounds.left + 0.02f >= textMinimum.sizeDip.width,
                                "numeric unit chrome cannot collapse the semantic text editor to zero width");
                    }
                    const auto formBounds = form->GetBounds();
                    const auto outermost  = sibling ? sibling->GetBounds() : wide->GetBounds();
                    const float usedWidth = flow == FlowDirection::LeftToRight ? outermost.right - formBounds.left : formBounds.right - outermost.left;
                    Require(Near(form->GetContentSize().width, usedWidth), "published content width covers the complete overflowing columns");
                    if (sibling)
                    {
                        const auto wideBounds = wide->GetBounds();
                        Require(Near(wideBounds.top, outermost.top) &&
                                    (flow == FlowDirection::LeftToRight ? wideBounds.right <= outermost.left : outermost.right <= wideBounds.left),
                                "growing for an intrinsic minimum preserves separate columns and their RTL order");
                    }
                    const auto measured    = form->DebugGetMeasureCount();
                    const auto customCalls = probe ? probe->measureCalls : 0u;
                    for (int repeat = 0; repeat < 4; ++repeat)
                        RequireSucceeded(form->ApplyLayout(host), "unchanged overflowing form preparation");
                    Require(form->DebugGetMeasureCount() == measured && (! probe || probe->measureCalls == customCalls),
                            "unchanged overflowing forms reuse all measurements rather than rediscovering and reshaping their minima");
                }
}

// Review of the later merges (N-18): a field keeps its label, description and validation, as a composite control keeps its
// parts. TakeChild refuses them and ClearChildren removes the editor alone, so the field takes a new editor with its text
// as it was; taking a part out had left its setters without a part for good. Swapping editors does not grow the field.
void TestFormFieldKeepsItsPartsThroughTakeChildAndClearChildren()
{
    FormField field(L"name", L"&Name");
    field.SetDescription(L"Your full name");
    RequireSucceeded(field.SetValidation(ValidationState::Error, L"Required"), "the field shows an error");
    auto* editor = field.AddEditor<TextField>(L"draft");
    Require(editor != nullptr, "the field takes an editor");
    Label* const label = field.GetLabelControl();
    for (size_t index = 0; index < field.GetLogicalChildCount(); ++index)
        if (field.GetLogicalChild(index) != editor)
            Require(field.TakeChild(index) == nullptr, "TakeChild refuses the field's own parts");
    Require(field.GetLabelControl() == label && field.GetLogicalChildCount() == 4u, "the parts stay in the field");
    field.ClearChildren();
    Require(field.GetEditor() == nullptr && field.GetLogicalChildCount() == 3u && field.GetLabelControl() == label, "ClearChildren removes the editor alone");
    Require(field.GetValidationState() == ValidationState::Error && label->GetText() == L"&Name", "the field's text and state stay");
    auto* replacement = field.AddEditor<TextField>(L"");
    Require(replacement != nullptr && label->GetMnemonicTarget() == replacement, "a new editor takes the field's label");
    for (int cycle = 0; cycle < 8; ++cycle)
    {
        field.ClearChildren();
        Require(field.AddEditor<TextField>(L"") != nullptr, "each cleared field takes a new editor");
    }
    Require(field.GetLogicalChildCount() == 4u, "swapping editors leaves no empty slots behind");
}

void TestFormsClearChildrenPreservesCallbackBoundReplacementEditor()
{
    ControlHost host;
    auto root       = std::make_unique<FormField>(L"replace", L"Field");
    auto* field     = root.get();
    auto* oldEditor = field->AddEditor<FormDetachProbe>();
    Require(oldEditor != nullptr, "detach fixture owns its original custom editor");
    host.SetRoot(std::move(root));
    field->SetBounds(D2D1::RectF(12.0f, 20.0f, 412.0f, 220.0f));
    const auto fieldLifetime = GetControlLifetimeToken(*field);
    const auto oldLifetime   = GetControlLifetimeToken(*oldEditor);
    TextField* replacement   = nullptr;
    std::weak_ptr<int> replacementLifetime;
    size_t detachCalls     = 0;
    HRESULT callbackResult = E_UNEXPECTED;
    oldEditor->onDetach    = std::make_shared<const std::function<void()>>([&]() noexcept
    {
        ++detachCalls;
        if (fieldLifetime.expired())
        {
            callbackResult = E_ABORT;
            return;
        }
        try
        {
            replacement    = field->AddEditor<TextField>(L"replacement draft");
            callbackResult = replacement ? S_OK : E_FAIL;
            if (replacement)
                replacementLifetime = GetControlLifetimeToken(*replacement);
        }
        catch (const std::bad_alloc&)
        {
            callbackResult = E_OUTOFMEMORY;
        }
    });
    Require(ControlModelQueryAccess::Host(*oldEditor) == &host, "detach callback begins from an attached editor");
    field->ClearChildren();
    Require(detachCalls == 1u && callbackResult == S_OK && oldLifetime.expired() && replacement && ! replacementLifetime.expired(),
            "the attached original editor's detach callback binds a live replacement while the original retires");
    const bool recognized = field->GetEditor() == replacement && field->GetSemanticEditor() == replacement;
    auto* const duplicate = field->AddEditor<TextField>(L"must be refused");
    Require(recognized && ! duplicate && field->GetLogicalChildCount() == 4u && field->GetLabelControl() != nullptr,
            "ClearChildren keeps the field's parts and the newer bound editor, and goes on refusing a second editor");
    Require(ControlModelQueryAccess::Host(*replacement) == &host && replacement->GetText() == L"replacement draft" &&
                replacement->GetAccessibleAutomationId() == field->GetFieldId(),
            "replacement keeps host, draft and form identity after the older clear returns");
    RequireSucceeded(field->ApplyLayout(host), "replacement-only field remains arrangeable after callback reconstruction");
    Require(replacement->GetBounds().right > replacement->GetBounds().left, "replacement receives usable geometry");
}

void TestFormsNonconvergentCustomMinimaFailBeforeGeometry()
{
    ControlHost host;
    auto root                      = std::make_unique<FormPanel>();
    auto* form                     = root.get();
    auto* field                    = form->AddField(L"growing", L"Value");
    auto* editor                   = field->AddEditor<FormMinimumProbe>(56.0f);
    editor->growWithAvailableWidth = true;
    host.SetRoot(std::move(root));
    form->SetBounds(D2D1::RectF(12, 20, 112, 320));
    const auto oldField  = field->GetBounds();
    const auto oldEditor = editor->GetBounds();
    Require(form->ApplyLayout(host) == HRESULT_FROM_WIN32(ERROR_RETRY) && editor->measureCalls == 4,
            "a minimum that always exceeds its available width is refused after four bounded passes");
    Require(Same(field->GetBounds(), oldField) && Same(editor->GetBounds(), oldEditor) && form->GetContentSize().width == 0,
            "nonconvergence publishes no field geometry or content extent");
    editor->growWithAvailableWidth = false;
    editor->RequestRemeasure();
    RequireSucceeded(form->ApplyLayout(host), "a stable custom minimum recovers after the refused pass");
    Require(editor->GetBounds().right > editor->GetBounds().left && form->GetContentSize().width > 0, "recovery publishes complete usable geometry");
}

void TestFormsMeasurementCallbacksRetireOrSupersedeSafely()
{
    for (int action = 0; action < 6; ++action)
    {
        ControlHost host;
        auto root    = std::make_unique<FormPanel>();
        auto* form   = root.get();
        auto* first  = form->AddField(L"one", L"First");
        auto* editor = first->AddEditor<FormProbe>();
        auto* second = form->AddField(L"two", L"Second");
        second->AddEditor<TextField>();
        host.SetRoot(std::move(root));
        form->SetBounds(D2D1::RectF(0, 0, 420, 300));
        editor->SetFocusable(true);
        const auto lifetime  = GetControlLifetimeToken(*form);
        const auto oldSecond = second->GetBounds();
        HRESULT nested       = S_OK;
        editor->onMeasure    = [&, action]
        {
            if (action == 0)
                host.SetRoot({});
            else if (action == 1)
                first->ClearChildren();
            else if (action == 2)
                form->SetColumns(2);
            else if (action == 3)
                second->SetBounds(D2D1::RectF(1, 2, 3, 4));
            else if (action == 4)
                host.SetFocusControl(editor, false);
            else
                nested = form->ApplyLayout(host);
        };
        const HRESULT result = form->ApplyLayout(host);
        Require(result == E_ABORT || (action == 5 && FAILED(result)), "custom measure mutation or nested pass cannot publish stale form geometry");
        if (! lifetime.expired())
        {
            Require(form->GetContentSize().width == 0, "aborted first measurement publishes no form extent");
            if (action != 3)
                Require(Same(second->GetBounds(), oldSecond), "measurement failure precedes all sibling geometry commits");
            if (action == 5)
                Require(nested == E_ABORT, "reentrant measurement refuses shared scratch traversal");
        }
    }
}

void TestFormsBoundsCallbacksKeepNewerLayoutsAndRejectForeignMutations()
{
    for (int action = 0; action < 6; ++action)
    {
        ControlHost host;
        auto root    = std::make_unique<FormPanel>();
        auto* form   = root.get();
        auto* first  = form->AddField(L"one", L"First");
        auto* editor = first->AddEditor<FormProbe>();
        auto* second = form->AddField(L"two", L"Second");
        second->AddEditor<TextField>();
        host.SetRoot(std::move(root));
        form->SetBounds(D2D1::RectF(0, 0, 420, 300));
        editor->SetFocusable(true);
        const auto lifetime = GetControlLifetimeToken(*form);
        bool entered        = false;
        editor->onBounds    = [&, action]
        {
            if (entered)
                return;
            entered = true;
            if (action == 0)
                host.SetRoot({});
            else if (action == 1)
                first->ClearChildren();
            else if (action == 2)
            {
                RequireSucceeded(form->SetColumns(2), "nested newer form policy");
                RequireSucceeded(form->ApplyLayout(host), "nested newer form arrangement completes");
            }
            else if (action == 3)
                second->SetBounds(D2D1::RectF(1, 2, 3, 4));
            else if (action == 4)
                host.SetFocusControl(editor, false);
            else
                second->SetVisible(false);
        };
        Require(form->ApplyLayout(host) == E_ABORT && entered, "bounds callback invalidates the older form pass");
        if (! lifetime.expired())
        {
            if (action == 2)
                Require(form->GetContentSize().width > 0 && Near(first->GetBounds().top, second->GetBounds().top),
                        "nested complete form keeps its new extent and geometry");
            else
                Require(form->GetContentSize().width == 0, "stale form extent is never published after a bounds callback");
        }
    }
}

void TestFormsDetachedCallbacksCannotMoveEarlierInputsOrCompletedGeometry()
{
    for (int action = 0; action < 3; ++action)
    {
        ControlHost host;
        FormPanel form;
        auto* first       = form.AddField(L"one", L"First");
        auto* firstEditor = first->AddEditor<TextField>(L"Retained draft");
        auto* second      = form.AddField(L"two", L"Second");
        auto* editor      = second->AddEditor<FormProbe>();
        form.SetBounds(D2D1::RectF(1.1f, 5.3f, 421.1f, 305.3f));
        const auto firstBefore  = first->GetBounds();
        const auto secondBefore = second->GetBounds();
        const auto editorBefore = editor->GetBounds();
        const auto authored     = D2D1::RectF(17, 19, 217, 59);
        bool entered            = false;
        const auto mutate       = [&]()
        {
            entered = true;
            (action == 2 ? static_cast<Control*>(first) : static_cast<Control*>(firstEditor))->SetBounds(authored);
        };
        if (action == 0)
            editor->onMeasure = mutate;
        else
            editor->onBounds = mutate;
        Require(ControlModelQueryAccess::Host(form) == nullptr && ControlModelQueryAccess::Host(*firstEditor) == nullptr,
                "explicit-host form remains detached");
        Require(form.ApplyLayout(host) == E_ABORT && entered, "detached earlier-input or completed-geometry mutation invalidates the pass");
        Require(form.GetContentSize().width == 0, "a detached stale pass cannot publish content extent");
        Require(Same((action == 2 ? static_cast<Control*>(first) : static_cast<Control*>(firstEditor))->GetBounds(), authored),
                "callback-authored geometry survives the aborted older pass");
        if (action == 0)
            Require(Same(first->GetBounds(), firstBefore) && Same(second->GetBounds(), secondBefore) && Same(editor->GetBounds(), editorBefore),
                    "a later measurement mutation is rejected before any form geometry is committed");
    }
}

void TestFormsNestedDetachedMeasureCannotReplaceOlderArrangementSnapshots()
{
    ControlHost host;
    FormPanel form;
    auto* first        = form.AddField(L"one", L"First");
    auto* editor       = first->AddEditor<FormProbe>();
    auto* second       = form.AddField(L"two", L"Second");
    auto* futureEditor = second->AddEditor<TextField>(L"Retained draft");
    form.SetBounds(D2D1::RectF(0, 0, 420, 300));
    const auto authored  = D2D1::RectF(17, 119, 217, 159);
    bool entered         = false;
    HRESULT nestedResult = E_UNEXPECTED;
    DesiredSize nestedSize;
    editor->onBounds = [&]
    {
        if (entered)
            return;
        entered = true;
        futureEditor->SetBounds(authored);
        nestedResult = form.Measure({host}, LayoutConstraints::AtMost(420.0f), nestedSize);
    };
    Require(ControlModelQueryAccess::Host(form) == nullptr && ControlModelQueryAccess::Host(*futureEditor) == nullptr, "nested explicit-host form is detached");
    const HRESULT result = form.ApplyLayout(host);
    Require(entered && nestedResult == S_OK && nestedSize.sizeDip.width > 0.0f,
            "bounds callback completes a fresh pure measurement using the same available width");
    Require(result == E_ABORT && Same(futureEditor->GetBounds(), authored) && form.GetContentSize().width == 0.0f,
            "a fresh measurement cannot refresh the older pass's snapshots and conceal callback-authored future geometry");
}

void TestFormsMaximumInventoryRetainsCacheAndFiniteGeometry()
{
    ControlHost host;
    FormPanel form;
    RequireSucceeded(form.SetColumns(2), "maximum form has two columns");
    std::array<FormField*, 256> fields;
    for (size_t index = 0; index < fields.size(); ++index)
    {
        fields[index] = form.AddField(L"field." + std::to_wstring(index), L"Localized value é");
        Require(fields[index] && fields[index]->AddEditor<FormMinimumProbe>(120.0f), "maximum form has a complete identified editor");
    }
    Require(form.AddField(L"too-many", L"Too many") == nullptr, "logical field ceiling refuses a 257th slot");
    form.SetBounds(D2D1::RectF(0, 0, 720, 240));
    RequireSucceeded(form.ApplyLayout(host), "maximum inventory prepares with finite extent");
    const auto extent = form.GetContentSize();
    Require(form.GetLayoutOverflow() && extent.height > 240 && std::isfinite(extent.height), "maximum inventory reports its scrollable vertical overflow");
    for (const auto* field : fields)
        Require(FiniteRect(field->GetBounds()) && FiniteRect(field->GetEditor()->GetBounds()), "maximum inventory commits finite field/editor rectangles");
    const auto measurements = form.DebugGetMeasureCount();
    RequireSucceeded(form.ApplyLayout(host), "unchanged maximum inventory reuses its numeric records");
    Require(form.DebugGetMeasureCount() == measurements, "maximum unchanged inventory performs no new leaf measurements");
    const auto hiddenBounds = fields[0]->GetBounds();
    fields[0]->SetVisible(false);
    RequireSucceeded(form.ApplyLayout(host), "logical slot snapshots retain identity when a leading field is hidden");
    Require(Same(fields[0]->GetBounds(), hiddenBounds) && fields[1]->GetBounds().left == 0,
            "hidden logical slots retain their bounds and the next visible field starts the first column");
}

void TestFormsNestedIntrinsicEditorIsAnAcceptedDescendant()
{
    ControlHost host;
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* field  = form->AddField(L"group", L"A composite editor");
    auto* editor = field->AddEditor<NestedFormEditor>();
    host.SetRoot(std::move(root));
    form->SetBounds(D2D1::RectF(0, 0, 420, 300));
    RequireSucceeded(form->ApplyLayout(host), "form accepts composite desired size");
    RequireSucceeded(editor->nestedResult, "bounds callback arranges a nested owner within the active editor branch");
    Require(form->GetContentSize().width > 0 && editor->label->GetBounds().right > editor->label->GetBounds().left &&
                editor->value->GetBounds().top >= editor->label->GetBounds().bottom,
            "nested editor geometry is arranged without treating its own descendants as foreign");
}

void TestFormsEmbeddedReflowPreservesCompositionSelectionAndFocus()
{
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "embedded form input WARP");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "embedded form input graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "embedded form input attach");
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* first  = form->AddField(L"name", L"Nom");
    auto* editor = first->AddEditor<TextField>(L"brouillon e\u0301");
    auto* second = form->AddField(L"height", L"Hauteur");
    second->AddEditor<NumericStepper>();
    view.Controls().SetRoot(std::move(root));
    const HRESULT initialPrepare = view.Prepare(420, 400);
    RequireSucceeded(initialPrepare, "initial embedded form input layout");
    if (initialPrepare == S_FALSE && view.Controls().HasPendingContentPreparation())
        Require(view.NeedsPreparation(), "pending initial content preparation keeps the view dirty");
    if (view.NeedsPreparation() || view.Controls().HasPendingContentPreparation())
        RequireSucceeded(PrepareEmbeddedContentForTests(view, 420, 400), "complete initial embedded form input layout");
    Require(! view.NeedsPreparation() && ! view.Controls().HasPendingContentPreparation(), "initial form input geometry is fully prepared");
    view.Controls().SetFocusControl(editor, false);
    editor->SetSelectionRange(1, 4);
    EmbeddedTextInputSnapshot before;
    Require(view.ReadTextInput(before) == S_OK, "read focused form draft");
    auto composition                  = before.state;
    composition.text                  = L"候補 e\u0301";
    composition.caretIndex            = composition.text.size();
    composition.compositionStartIndex = 0;
    composition.compositionEndIndex   = 2;
    RequireSucceeded(view.ApplyTextInput(before.revision, composition, EmbeddedTextInputAction::Preview), "preview form composition");
    RequireSucceeded(form->SetColumns(2), "responsive form columns");
    RequireSucceeded(form->SetLabelPlacement(LabelPlacement::Beside), "responsive form labels");
    form->SetFlowDirection(FlowDirection::RightToLeft);
    first->SetDescription(L"Une description plus longue qui augmente la hauteur de la ligne.");
    RequireSucceeded(first->SetValidation(ValidationState::Error, L"Conservez le brouillon pendant cette présentation."),
                     "validation presentation while composing");
    const HRESULT reflowPrepare = view.Prepare(520, 450, 144);
    RequireSucceeded(reflowPrepare, "DPI/flow/column/error reflow while composing");
    if (view.NeedsPreparation() || view.Controls().HasPendingContentPreparation())
        RequireSucceeded(PrepareEmbeddedContentForTests(view, 520, 450, 144), "complete DPI/flow/column/error reflow");
    Require(! view.NeedsPreparation() && ! view.Controls().HasPendingContentPreparation(), "reflowed form input geometry is fully prepared");
    EmbeddedTextInputSnapshot after;
    Require(view.ReadTextInput(after) == S_OK && after.focusId == before.focusId && after.state.text == composition.text &&
                after.state.compositionStartIndex == 0 && after.state.compositionEndIndex == 2 && view.Controls().GetFocusControl() == editor,
            "form reflow preserves stable semantic editor and application composition");
    RequireSucceeded(view.ApplyTextInput(after.revision, {}, EmbeddedTextInputAction::Cancel), "cancel form composition after reflow");
    Require(editor->GetText() == L"brouillon e\u0301", "composition cancellation restores the same editor draft");
    editor->SetSelectionRange(1, 4);
    const auto selection = editor->GetSelectionRange();
    RequireSucceeded(form->SetColumns(1), "return to one column");
    RequireSucceeded(view.Prepare(420, 400, 96), "selection-preserving form preparation");
    Require(editor->GetSelectionRange() == selection && view.Controls().GetFocusControl() == editor,
            "ordinary responsive reflow preserves selection and focus");
    first->SetVisible(false);
    EmbeddedTextInputSnapshot hidden;
    Require(view.ReadTextInput(hidden) == S_FALSE, "the next input boundary refuses the hidden field's editor");
    Require(view.Controls().GetFocusControl() != editor, "hiding a field retires its unavailable editor focus");
    Require(! view.DispatchCharacter(L'x') && editor->GetText() == L"brouillon e\u0301", "a hidden form editor receives no character input");
}

// An application that validates on every edit invalidates the form's measure between frames. The characters of the same
// frame still reach the editor, a surrogate pair whole, since geometry moves only at the next preparation.
void TestFormsEmbeddedValidationOnEachEditKeepsInput()
{
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "validating form WARP");
    std::shared_ptr<GraphicsDevice> device;
    RequireSucceeded(GraphicsDevice::Create(graphics.device.get(), device), "validating form graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(device), "validating form attach");
    auto root    = std::make_unique<FormPanel>();
    auto* form   = root.get();
    auto* field  = form->AddField(L"name", L"Nom");
    auto* editor = field->AddEditor<TextField>(L"");
    size_t edits = 0u;
    editor->SetOnTextChanged([&](std::wstring_view text)
    {
        ++edits;
        static_cast<void>(field->SetValidation(text.size() > 2u ? ValidationState::Error : ValidationState::None, L"Trop long"));
    });
    view.Controls().SetRoot(std::move(root));
    RequireSucceeded(view.Prepare(420, 300), "initial validating form layout");
    view.Controls().SetFocusControl(editor, false);
    Require(view.DispatchCharacter(L'a') && view.DispatchCharacter(L'b'), "two characters typed in one frame both reach the editor");
    Require(view.DispatchCharacter(static_cast<wchar_t>(0xD83D)) && view.DispatchCharacter(static_cast<wchar_t>(0xDE00)),
            "both halves of an astral character reach the editor");
    Require(editor->GetText() == L"ab\U0001F600" && edits >= 3u, "the editor holds every character, the surrogate pair whole");
    RequireSucceeded(view.Prepare(420, 300), "the next frame lays the validated form out");
}

void TestFormsNativePreparationAndFactoryCoverage()
{
    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    auto root  = std::make_unique<Panel>();
    auto* form = root->AddChild<FormPanel>();
    form->SetBounds(D2D1::RectF(8, 8, 428, 240));
    auto* first  = form->AddField(L"size", L"Taille");
    auto* editor = first->AddEditor<TextField>(L"draft");
    auto* second = form->AddField(L"unit", L"Unité");
    second->AddEditor<ComboBox>();
    window.Host().SetRoot(std::move(root));
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), "native preparation captures a populated form without activation");
    window.Host().SetFocusControl(editor, false);
    editor->SetSelectionRange(1, 3);
    const auto selection = editor->GetSelectionRange();
    RequireSucceeded(form->SetColumns(2), "native responsive columns");
    RequireSucceeded(form->SetLabelPlacement(LabelPlacement::Beside), "native beside labels");
    Require(window.Host().DebugCaptureBitmap(capture), "native capture processes pending form layout");
    Require(Near(first->GetBounds().top, second->GetBounds().top) && window.Host().GetFocusControl() == editor && editor->GetSelectionRange() == selection,
            "native preparation shares form layout without blurring or reconstructing its editor");
    for (const auto kind : {ControlKind::FlexPanel, ControlKind::FormField, ControlKind::FormPanel})
    {
        std::unique_ptr<Control> control;
        RequireSucceeded(CreateControl(kind, control), "factory constructs each intrinsic container");
        Require(control != nullptr, "catalogued intrinsic control owns a concrete instance");
    }
    Require(GetControlCatalog().size() == 35, "the catalog includes the opt-in controls and retained rich-text preview");
}
} // namespace

void RunFormLayoutTests()
{
    DXUI_RUN_TEST(TestFormsMeasureIsPureAndSharedLabelCacheIsStable);
    DXUI_RUN_TEST(TestFormsLocalizedGeometryMatrix);
    DXUI_RUN_TEST(TestFormsConstraintsBudgetsAndCustomFallbackFailBeforeGeometry);
    DXUI_RUN_TEST(TestFormsPartsLeaveCleanlyAndPreferencesShrink);
    DXUI_RUN_TEST(TestFormsHideAnEmptyLabel);
    DXUI_RUN_TEST(TestFormsFractionalTranslationFailsBeforeAnyChildGeometry);
    DXUI_RUN_TEST(TestFormsFractionalBesidePartsMeasureTheirCommittedWidths);
    DXUI_RUN_TEST(TestFormsIntrinsicEditorMinimaDriveNarrowContentAndCache);
    DXUI_RUN_TEST(TestFormFieldKeepsItsPartsThroughTakeChildAndClearChildren);
    DXUI_RUN_TEST(TestFormsClearChildrenPreservesCallbackBoundReplacementEditor);
    DXUI_RUN_TEST(TestFormsNonconvergentCustomMinimaFailBeforeGeometry);
    DXUI_RUN_TEST(TestFormsMeasurementCallbacksRetireOrSupersedeSafely);
    DXUI_RUN_TEST(TestFormsBoundsCallbacksKeepNewerLayoutsAndRejectForeignMutations);
    DXUI_RUN_TEST(TestFormsDetachedCallbacksCannotMoveEarlierInputsOrCompletedGeometry);
    DXUI_RUN_TEST(TestFormsNestedDetachedMeasureCannotReplaceOlderArrangementSnapshots);
    DXUI_RUN_TEST(TestFormsMaximumInventoryRetainsCacheAndFiniteGeometry);
    DXUI_RUN_TEST(TestFormsNestedIntrinsicEditorIsAnAcceptedDescendant);
    DXUI_RUN_TEST(TestFormsEmbeddedReflowPreservesCompositionSelectionAndFocus);
    DXUI_RUN_TEST(TestFormsEmbeddedValidationOnEachEditKeepsInput);
    DXUI_RUN_TEST(TestFormsNativePreparationAndFactoryCoverage);
}
