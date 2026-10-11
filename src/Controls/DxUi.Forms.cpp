#include "DxUi.Internal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace DxUi
{
namespace
{
constexpr float kFieldGapDip           = 6.0f;
constexpr float kLabelGapDip           = 12.0f;
constexpr float kMinimumEditorWidthDip = 48.0f;
constexpr size_t kMaxFormFields        = 256;

bool ValidExtent(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0f;
}
bool ValidRect(const D2D1_RECT_F& value) noexcept
{
    return std::isfinite(value.left) && std::isfinite(value.top) && std::isfinite(value.right) && std::isfinite(value.bottom) &&
           ValidExtent(value.right - value.left) && ValidExtent(value.bottom - value.top);
}
bool SameRect(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}
bool Owns(const Panel& panel, const Control* control, const std::weak_ptr<int>& lifetime) noexcept
{
    return control && ! lifetime.expired() && std::ranges::any_of(panel.GetChildren(), [&](const auto& child) noexcept { return child.get() == control; });
}
} // namespace

FormField::FormField() : FormField({}, {})
{
}
FormField::FormField(std::wstring id, std::wstring label) : _fieldId(std::move(id))
{
    EnableIntrinsicLayout();
    _label       = AddChild<Label>(std::move(label));
    _description = AddChild<Label>();
    _validation  = AddChild<Label>();
    _label->SetMultiline(true);
    _description->SetMultiline(true);
    _validation->SetMultiline(true);
    _description->SetFontRole(FontRole::Small);
    _validation->SetFontRole(FontRole::Small);
    _description->SetVisible(false);
    _validation->SetVisible(false);
}
std::wstring_view FormField::GetFieldId() const noexcept
{
    return _fieldId;
}
Control* FormField::GetEditor() const noexcept
{
    return Owns(*this, _editor, _editorLifetime) ? _editor : nullptr;
}
Control* FormField::GetSemanticEditor() const noexcept
{
    auto* editor = GetEditor();
    if (auto* stepper = dynamic_cast<NumericStepper*>(editor))
        return &stepper->Field();
    return editor;
}
Label* FormField::GetLabelControl() const noexcept
{
    return _label;
}
ValidationState FormField::GetValidationState() const noexcept
{
    return _validationState;
}
uint64_t FormField::DebugGetMeasureCount() const noexcept
{
    return _measureCount;
}

HRESULT FormField::BindEditor(Control& editor) noexcept
{
    if (GetEditor())
        return E_UNEXPECTED;
    _editor         = &editor;
    _editorLifetime = GetControlLifetimeToken(editor);
    InvalidateMeasure();
    return SyncAccessibility();
}
void FormField::RemoveFailedEditor(Control* editor) noexcept
{
    for (size_t index = 0; index < GetChildren().size(); ++index)
        if (GetChildren()[index].get() == editor)
        {
            static_cast<void>(TakeChild(index));
            return;
        }
}
HRESULT FormField::SyncAccessibility() noexcept
{
    auto* const editor = GetSemanticEditor();
    if (! editor)
        return S_FALSE;
    const auto ownerLifetime  = GetLifetimeToken();
    const auto editorLifetime = GetControlLifetimeToken(*editor);
    const auto current        = [&]() noexcept { return ! ownerLifetime.expired() && ! editorLifetime.expired() && GetSemanticEditor() == editor; };
    try
    {
        // Allocate strings before publication. The setters move them without further string allocation.
        std::wstring help(_description && _description->IsVisible() ? _description->GetText() : std::wstring_view{});
        std::wstring status(_validation && _validation->IsVisible() ? _validation->GetText() : std::wstring_view{});
        std::wstring id      = _fieldId;
        const HRESULT result = editor->SetAccessibleFormProperties(_label, _required, _validationState != ValidationState::Error, std::move(status));
        if (! current())
            return E_ABORT;
        if (FAILED(result))
            return result;
        if (_label)
            _label->SetMnemonicTarget(editor);
        if (! current())
            return E_ABORT;
        editor->SetAccessibleHelpText(std::move(help));
        if (! current())
            return E_ABORT;
        editor->SetAccessibleAutomationId(std::move(id));
        return current() ? S_OK : E_ABORT;
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
void FormField::SetLabel(std::wstring text)
{
    if (! _label)
        return;
    const auto lifetime = GetLifetimeToken();
    _label->SetText(std::move(text));
    if (lifetime.expired())
        return;
    InvalidateMeasure();
    static_cast<void>(SyncAccessibility());
}
void FormField::SetDescription(std::wstring text)
{
    if (! _description)
        return;
    const auto lifetime = GetLifetimeToken();
    const bool visible  = ! text.empty();
    _description->SetText(std::move(text));
    if (lifetime.expired())
        return;
    _description->SetVisible(visible);
    if (lifetime.expired())
        return;
    InvalidateMeasure();
    static_cast<void>(SyncAccessibility());
}
HRESULT FormField::SetValidation(ValidationState state, std::wstring explanation) noexcept
{
    if ((state != ValidationState::None && state != ValidationState::Error) || (state == ValidationState::Error && explanation.empty()) ||
        explanation.size() > 65536)
        return E_INVALIDARG;
    if (! _validation)
        return E_UNEXPECTED;
    const auto lifetime = GetLifetimeToken();
    try
    {
        _validation->SetText(state == ValidationState::None ? std::wstring{} : std::move(explanation));
        if (lifetime.expired())
            return E_ABORT;
        _validationState = state;
        _validation->SetVisible(state == ValidationState::Error);
        if (lifetime.expired())
            return E_ABORT;
        InvalidateMeasure();
        return SyncAccessibility();
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
HRESULT FormField::SetRequired(bool required) noexcept
{
    _required = required;
    return SyncAccessibility();
}
HRESULT FormField::SetEditorPreferredSize(D2D1_SIZE_F sizeDip) noexcept
{
    if (! ValidExtent(sizeDip.width) || ! ValidExtent(sizeDip.height) || sizeDip.width < kMinimumEditorWidthDip || sizeDip.height <= 0)
        return E_INVALIDARG;
    if (_editorPreferredSize.width == sizeDip.width && _editorPreferredSize.height == sizeDip.height)
        return S_FALSE;
    _editorPreferredSize = sizeDip;
    InvalidateMeasure();
    return S_OK;
}

HRESULT FormField::MeasurePart(const Control* control, const MeasureContext& context, float widthDip, PartMeasure& cache, DesiredSize& desired) const noexcept
{
    if (! control || ! control->IsVisible())
    {
        desired = {};
        return S_OK;
    }
    // A fresh native host initializes its shared formats on first use. Cache the resulting typography epoch,
    // rather than rejecting that legitimate initialization as a callback-induced replacement.
    static_cast<void>(context.host.GetWriteFactory());
    const auto ownerLifetime = GetLifetimeToken();
    const auto lifetime      = GetControlLifetimeToken(*control);
    const auto revision      = control->GetMeasureRevision();
    const auto ownerRevision = GetMeasureRevision();
    const auto typography    = context.host.GetTypographyRevision();
    if (cache.control == control && ! cache.lifetime.expired() && cache.revision == revision && cache.typography == typography && cache.host == &context.host &&
        cache.width == widthDip)
    {
        desired = cache.desired;
        return S_OK;
    }
    DesiredSize staged;
    const HRESULT result = MeasureControl(*control, context, LayoutConstraints::AtMost(widthDip, std::nullopt), staged);
    if (ownerLifetime.expired() || lifetime.expired())
        return E_ABORT;
    if (GetMeasureRevision() != ownerRevision || ! Owns(*this, control, lifetime) || context.host.GetTypographyRevision() != typography)
        return E_ABORT;
    if (result == E_NOTIMPL && control == GetEditor())
        staged.sizeDip = D2D1::SizeF((std::max)(kMinimumEditorWidthDip, (std::min)(_editorPreferredSize.width, widthDip)), _editorPreferredSize.height);
    else if (FAILED(result))
        return result;
    ++_measureCount;
    cache   = {control, lifetime, revision, typography, &context.host, widthDip, staged};
    desired = staged;
    return S_OK;
}
HRESULT FormField::MeasureSharedLabel(const MeasureContext& context, float widthDip, DesiredSize& desired) const noexcept
{
    if (_label && _label->GetText().empty())
    {
        desired = {};
        return S_OK;
    }
    return MeasurePart(_label, context, widthDip, _sharedLabelCache, desired);
}
HRESULT FormField::ResolvePartColumns(
    const D2D1_RECT_F& bounds, LabelPlacement placement, float labelWidthDip, bool rtl, float minimumEditorWidthDip, PartColumns& columns) noexcept
{
    if (! ValidRect(bounds) || ! ValidExtent(labelWidthDip) || ! ValidExtent(minimumEditorWidthDip))
        return E_INVALIDARG;
    const bool beside      = placement == LabelPlacement::Beside;
    const float labelWidth = beside ? labelWidthDip : bounds.right - bounds.left;
    const float gap        = beside && labelWidth > 0.0f ? kLabelGapDip : 0.0f;
    PartColumns staged;
    staged.labelLeft      = beside && rtl ? bounds.right - labelWidth : bounds.left;
    staged.labelRight     = beside && ! rtl ? bounds.left + labelWidth : bounds.right;
    const float laneLeft  = beside && ! rtl ? staged.labelRight + gap : bounds.left;
    const float laneRight = beside && rtl ? staged.labelLeft - gap : bounds.right;
    const float width     = (std::max)({kMinimumEditorWidthDip, minimumEditorWidthDip, laneRight - laneLeft});
    staged.editorLeft     = rtl ? laneRight - width : laneLeft;
    staged.editorRight    = rtl ? laneRight : laneLeft + width;
    // An absolute endpoint can round inward even when the requested extent is finite. Preserve the editor's
    // actual minimum in the represented rectangle, then measure at exactly that represented width.
    if (staged.editorRight - staged.editorLeft < width)
    {
        if (rtl)
            staged.editorLeft = std::nextafter(staged.editorLeft, -(std::numeric_limits<float>::infinity)());
        else
            staged.editorRight = std::nextafter(staged.editorRight, (std::numeric_limits<float>::infinity)());
    }
    if (! std::isfinite(staged.labelLeft) || ! std::isfinite(staged.labelRight) || ! ValidExtent(staged.labelRight - staged.labelLeft) ||
        ! std::isfinite(staged.editorLeft) || ! std::isfinite(staged.editorRight) || ! ValidExtent(staged.editorRight - staged.editorLeft) ||
        staged.editorRight - staged.editorLeft < width)
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    columns = staged;
    return S_OK;
}
HRESULT FormField::MeasureParts(const MeasureContext& context,
                                const D2D1_RECT_F& fieldBounds,
                                LabelPlacement placement,
                                float labelWidthDip,
                                std::array<DesiredSize, 4>& parts,
                                PartColumns* columns) const noexcept
{
    if (_measuring)
        return E_ABORT;
    if (! ValidRect(fieldBounds) || ! ValidExtent(labelWidthDip) || (GetHost() && GetHost() != &context.host))
        return E_INVALIDARG;
    const auto lifetime            = GetLifetimeToken();
    _measuring                     = true;
    const auto measuring           = wil::scope_exit([this, lifetime]() noexcept
    {
        if (! lifetime.expired())
            _measuring = false;
    });
    const auto pass                = _layoutRevision;
    const auto revision            = GetMeasureRevision();
    const auto focus               = context.host._focusRequestRevision;
    const auto bounds              = GetBounds();
    const auto* const attachedHost = GetHost();
    size_t remainingTextUnits      = kMaxIntrinsicTextUnits;
    auto boundedContext            = context;
    if (! boundedContext.remainingTextUnits)
        boundedContext.remainingTextUnits = &remainingTextUnits;
    const Control* const controls[] = {_label, GetEditor(), _description, _validation};
    const bool rtl                  = IsRightToLeft();
    PartColumns stagedColumns;
    HRESULT result = ResolvePartColumns(fieldBounds, placement, labelWidthDip, rtl, kMinimumEditorWidthDip, stagedColumns);
    if (FAILED(result))
        return result;
    for (size_t index = 0; index < parts.size(); ++index)
    {
        if (index == 0 && _label && _label->GetText().empty())
        {
            parts[index] = {};
            continue;
        }
        const float partWidth = index == 0 ? stagedColumns.labelRight - stagedColumns.labelLeft : stagedColumns.editorRight - stagedColumns.editorLeft;
        result                = MeasurePart(controls[index], boundedContext, partWidth, _measureCache[index], parts[index]);
        if (lifetime.expired())
            return E_ABORT;
        if (_layoutRevision != pass || GetMeasureRevision() != revision || context.host._focusRequestRevision != focus || ! SameRect(GetBounds(), bounds) ||
            GetHost() != attachedHost)
            return E_ABORT;
        if (FAILED(result))
            return result;
        if (index == 1)
        {
            if (controls[index])
            {
                parts[index].sizeDip.width =
                    (std::max)(parts[index].sizeDip.width, (std::max)(kMinimumEditorWidthDip, (std::min)(_editorPreferredSize.width, partWidth)));
                parts[index].sizeDip.height = (std::max)(parts[index].sizeDip.height, _editorPreferredSize.height);
            }
            result = ResolvePartColumns(fieldBounds, placement, labelWidthDip, rtl, parts[index].sizeDip.width, stagedColumns);
            if (FAILED(result))
                return result;
        }
    }
    if (columns)
        *columns = stagedColumns;
    return S_OK;
}
HRESULT FormField::MeasureForLayout(
    const MeasureContext& context, const D2D1_RECT_F& bounds, LabelPlacement placement, float labelWidthDip, DesiredSize& desired) const noexcept
{
    std::array<DesiredSize, 4> parts;
    PartColumns columns;
    const HRESULT result = MeasureParts(context, bounds, placement, labelWidthDip, parts, &columns);
    if (FAILED(result))
        return result;
    float height = placement == LabelPlacement::Beside
                       ? (std::max)(parts[0].sizeDip.height, parts[1].sizeDip.height)
                       : parts[0].sizeDip.height + (parts[0].sizeDip.height > 0 ? kFieldGapDip : 0.0f) + parts[1].sizeDip.height;
    for (size_t index = 2; index < parts.size(); ++index)
        if (parts[index].sizeDip.height > 0)
            height += kFieldGapDip + parts[index].sizeDip.height;
    if (! ValidExtent(height))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    const float widthDip      = bounds.right - bounds.left;
    const float editorMinimum = IsRightToLeft() ? bounds.right - columns.editorLeft : columns.editorRight - bounds.left;
    if (! ValidExtent(editorMinimum))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    desired.sizeDip = D2D1::SizeF((std::max)({kMinimumEditorWidthDip, widthDip, editorMinimum}), height);
    desired.overflow =
        widthDip < kMinimumEditorWidthDip || editorMinimum > widthDip || std::ranges::any_of(parts, [](const auto& part) noexcept { return part.overflow; });
    return S_OK;
}
HRESULT FormField::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& desired) const noexcept
{
    // Standalone fields measure above the editor. A form's shared-column measurement passes context without
    // changing field inputs or parent revisions inside Control::Measure.
    const float width = constraints.maximumWidthDip.value_or((std::max)(_editorPreferredSize.width, constraints.minimumDip.width));
    return MeasureForLayout(context, D2D1::RectF(0, 0, width, 0), LabelPlacement::Above, 0, desired);
}
HRESULT FormField::ApplyLayout(const ControlHost& host) noexcept
{
    return ApplyLayoutFor(host, _labelPlacement, _labelWidthDip);
}
HRESULT FormField::StageLayout(
    const MeasureContext& context, const D2D1_RECT_F& bounds, LabelPlacement placement, float labelWidthDip, std::array<PartLayout, 4>& staged) const noexcept
{
    if (! ValidRect(bounds))
        return E_INVALIDARG;
    std::array<DesiredSize, 4> parts;
    PartColumns columns;
    const HRESULT result = MeasureParts(context, bounds, placement, labelWidthDip, parts, &columns);
    if (FAILED(result))
        return result;
    const bool beside     = placement == LabelPlacement::Beside;
    const float editorTop = bounds.top + (beside || parts[0].sizeDip.height == 0 ? 0.0f : parts[0].sizeDip.height + kFieldGapDip);
    float nextTop         = beside ? bounds.top + (std::max)(parts[0].sizeDip.height, parts[1].sizeDip.height) : editorTop + parts[1].sizeDip.height;
    std::array<D2D1_RECT_F, 4> rectangles{D2D1::RectF(columns.labelLeft, bounds.top, columns.labelRight, bounds.top + parts[0].sizeDip.height),
                                          D2D1::RectF(columns.editorLeft, editorTop, columns.editorRight, editorTop + parts[1].sizeDip.height),
                                          {},
                                          {}};
    for (size_t index = 2; index < parts.size(); ++index)
    {
        if (parts[index].sizeDip.height > 0)
            nextTop += kFieldGapDip;
        rectangles[index] = D2D1::RectF(columns.editorLeft, nextTop, columns.editorRight, nextTop + parts[index].sizeDip.height);
        nextTop += parts[index].sizeDip.height;
    }
    if (! std::ranges::all_of(rectangles, ValidRect))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    Control* const controls[] = {_label, GetEditor(), _description, _validation};
    for (size_t index = 0; index < staged.size(); ++index)
    {
        auto* const control = controls[index];
        staged[index]       = {control,
                               control ? GetControlLifetimeToken(*control) : std::weak_ptr<int>{},
                               control ? control->GetMeasureRevision() : 0,
                               control && control->IsVisible(),
                               control ? control->GetBounds() : D2D1::RectF(),
                               rectangles[index]};
    }
    return S_OK;
}
bool FormField::ArePartsCurrent(const std::array<PartLayout, 4>& parts, size_t completed) const noexcept
{
    const Control* const controls[] = {_label, GetEditor(), _description, _validation};
    for (size_t index = 0; index < parts.size(); ++index)
    {
        const auto& part = parts[index];
        if (controls[index] != part.control)
            return false;
        if (! part.control)
            continue;
        if (! Owns(*this, part.control, part.lifetime) || part.control->GetMeasureRevision() != part.revision || part.control->IsVisible() != part.visible ||
            ! SameRect(part.control->GetBounds(), index < completed && part.visible ? part.bounds : part.before))
            return false;
    }
    return true;
}
HRESULT FormField::ApplyLayoutFor(const ControlHost& host, LabelPlacement placement, float labelWidthDip, const std::array<PartLayout, 4>* staged) noexcept
{
    if (IsRenderStageActiveForDebug())
        return E_UNEXPECTED;
    if (GetHost() && GetHost() != &host)
        return E_INVALIDARG;
    const auto lifetime            = GetLifetimeToken();
    const auto revision            = GetMeasureRevision();
    const auto focus               = host._focusRequestRevision;
    const auto bounds              = GetBounds();
    const auto* const attachedHost = GetHost();
    const auto pass                = ++_layoutRevision;
    if (! ValidRect(bounds))
        return E_INVALIDARG;
    std::array<PartLayout, 4> local;
    if (! staged)
    {
        const HRESULT result = StageLayout({host}, bounds, placement, labelWidthDip, local);
        if (FAILED(result) || lifetime.expired())
            return FAILED(result) ? result : E_ABORT;
        staged = &local;
    }
    if (_layoutRevision != pass || GetMeasureRevision() != revision || host._focusRequestRevision != focus || ! SameRect(GetBounds(), bounds) ||
        GetHost() != attachedHost || ! ArePartsCurrent(*staged, 0))
        return E_ABORT;
    const auto typography = host.GetTypographyRevision();
    auto& mutableHost     = const_cast<ControlHost&>(host);
    mutableHost.BeginIntrinsicArrange(*this);
    for (size_t index = 0; index < staged->size(); ++index)
    {
        const auto part     = (*staged)[index];
        auto* const control = part.control;
        if (! control || ! part.visible)
            continue;
        const auto* const previous          = mutableHost._intrinsicArrangeBranch;
        const auto foreign                  = mutableHost._intrinsicArrangeForeignRevision;
        const auto accepted                 = mutableHost._intrinsicArrangeBoundsRevision;
        const auto interaction              = host.GetInteractionRevision();
        mutableHost._intrinsicArrangeBranch = control;
        control->SetBounds(part.bounds);
        mutableHost._intrinsicArrangeBranch = previous;
        if (lifetime.expired() || part.lifetime.expired())
            return E_ABORT;
        if (_layoutRevision != pass || GetMeasureRevision() != revision || host._focusRequestRevision != focus || GetHost() != attachedHost ||
            host.GetTypographyRevision() != typography || ! SameRect(GetBounds(), bounds) || ! ArePartsCurrent(*staged, index + 1) ||
            mutableHost._intrinsicArrangeForeignRevision != foreign ||
            host.GetInteractionRevision() - interaction != mutableHost._intrinsicArrangeBoundsRevision - accepted)
            return E_ABORT;
    }
    _labelPlacement = placement;
    _labelWidthDip  = labelWidthDip;
    return S_OK;
}
HRESULT FormField::PrepareIntrinsicLayout(const MeasureContext& context) noexcept
{
    return ApplyLayout(context.host);
}
void FormField::OnBoundsChanged() noexcept
{
    ++_layoutRevision;
    RequestIntrinsicLayout();
}

std::unique_ptr<Control> FormField::TakeChild(size_t index) noexcept
{
    const auto lifetime = GetLifetimeToken();
    auto result         = Panel::TakeChild(index);
    if (lifetime.expired() || ! result)
        return result;
    if (result.get() == _label)
        _label = nullptr;
    if (result.get() == _description)
        _description = nullptr;
    if (result.get() == _validation)
    {
        _validation      = nullptr;
        _validationState = ValidationState::None;
    }
    if (result.get() == _editor)
    {
        _editor = nullptr;
        _editorLifetime.reset();
        auto* semantic = result.get();
        if (auto* stepper = dynamic_cast<NumericStepper*>(semantic))
            semantic = &stepper->Field();
        static_cast<void>(semantic->SetAccessibleFormProperties(nullptr, false, true));
        if (lifetime.expired())
            return result;
    }
    ++_layoutRevision;
    InvalidateMeasure();
    static_cast<void>(SyncAccessibility());
    return result;
}
void FormField::ClearChildren() noexcept
{
    const auto lifetime = GetLifetimeToken();
    // Retire the snapshot's semantic pointers before detach callbacks can bind a replacement in the live vector.
    _label = _description = _validation = nullptr;
    _editor                             = nullptr;
    _editorLifetime.reset();
    _validationState = ValidationState::None;
    ++_layoutRevision;
    Panel::ClearChildren();
    if (lifetime.expired())
        return;
    InvalidateMeasure();
}

FormPanel::FormPanel()
{
    EnableIntrinsicLayout();
}
FormField* FormPanel::AddField(std::wstring id, std::wstring label)
{
    if (id.empty() || id.size() > 65536 || label.size() > 65536 || GetLogicalChildCount() >= kMaxFormFields)
        return nullptr;
    for (const auto& child : GetChildren())
        if (const auto* field = dynamic_cast<FormField*>(child.get()); field && field->GetFieldId() == id)
            return nullptr;
    return AddChild<FormField>(std::move(id), std::move(label));
}
HRESULT FormPanel::SetColumns(size_t columns) noexcept
{
    if (columns != 1 && columns != 2)
        return E_INVALIDARG;
    if (_columns == columns)
        return S_FALSE;
    _columns = columns;
    ++_layoutRevision;
    InvalidateMeasure();
    return S_OK;
}
size_t FormPanel::GetColumns() const noexcept
{
    return _columns;
}
HRESULT FormPanel::SetLabelPlacement(LabelPlacement placement) noexcept
{
    if (placement != LabelPlacement::Above && placement != LabelPlacement::Beside)
        return E_INVALIDARG;
    if (_labelPlacement == placement)
        return S_FALSE;
    _labelPlacement = placement;
    ++_layoutRevision;
    InvalidateMeasure();
    return S_OK;
}
HRESULT FormPanel::SetGap(float horizontalDip, float verticalDip) noexcept
{
    if (! ValidExtent(horizontalDip) || ! ValidExtent(verticalDip))
        return E_INVALIDARG;
    if (_horizontalGapDip == horizontalDip && _verticalGapDip == verticalDip)
        return S_FALSE;
    _horizontalGapDip = horizontalDip;
    _verticalGapDip   = verticalDip;
    ++_layoutRevision;
    InvalidateMeasure();
    return S_OK;
}
HRESULT FormPanel::SetPadding(float paddingDip) noexcept
{
    if (! ValidExtent(paddingDip))
        return E_INVALIDARG;
    if (_paddingDip == paddingDip)
        return S_FALSE;
    _paddingDip = paddingDip;
    ++_layoutRevision;
    InvalidateMeasure();
    return S_OK;
}
D2D1_SIZE_F FormPanel::GetContentSize() const noexcept
{
    return _contentSize;
}
bool FormPanel::GetLayoutOverflow() const noexcept
{
    return _overflow;
}
uint64_t FormPanel::DebugGetMeasureCount() const noexcept
{
    uint64_t count = 0;
    for (const auto& child : GetChildren())
        if (const auto* field = dynamic_cast<const FormField*>(child.get()))
            count += field->DebugGetMeasureCount();
    return count;
}
HRESULT FormPanel::BuildLayout(
    const MeasureContext& context, float widthDip, D2D1_POINT_2F origin, D2D1_SIZE_F& content, float& labelWidthDip, bool& overflow) const noexcept
{
    if (_buildingLayout)
        return E_ABORT;
    if (! ValidExtent(widthDip) || ! std::isfinite(origin.x) || ! std::isfinite(origin.y) || GetLogicalChildCount() > kMaxFormFields ||
        (GetHost() && GetHost() != &context.host))
        return E_INVALIDARG;
    const auto lifetime = GetLifetimeToken();
    _buildingLayout     = true;
    const auto building = wil::scope_exit([this, lifetime]() noexcept
    {
        if (! lifetime.expired())
            _buildingLayout = false;
    });
    static_cast<void>(context.host.GetWriteFactory());
    const auto revision            = GetMeasureRevision();
    const auto pass                = _layoutRevision;
    const auto scratchRevision     = ++_scratchRevision;
    const auto bounds              = GetBounds();
    const auto* const attachedHost = GetHost();
    const auto geometry            = context.host.GetInteractionRevision();
    const auto focus               = context.host._focusRequestRevision;
    const auto typography          = context.host.GetTypographyRevision();
    const size_t count             = GetLogicalChildCount();
    const auto current             = [&]() noexcept
    {
        if (lifetime.expired() || GetMeasureRevision() != revision || _layoutRevision != pass || _scratchRevision != scratchRevision ||
            GetHost() != attachedHost || ! SameRect(GetBounds(), bounds) || GetLogicalChildCount() != count ||
            context.host.GetInteractionRevision() != geometry || context.host._focusRequestRevision != focus ||
            context.host.GetTypographyRevision() != typography)
            return false;
        for (const auto& record : _scratch)
            if (record.lifetime.expired() || GetLogicalChild(record.logicalIndex) != record.field || record.field->GetMeasureRevision() != record.revision ||
                ! record.field->IsVisible() || ! SameRect(record.field->GetBounds(), record.before) || ! record.field->ArePartsCurrent(record.parts, 0))
                return false;
        return true;
    };
    const float columns = static_cast<float>(_columns);
    const float gaps    = static_cast<float>(_columns - 1) * _horizontalGapDip;
    const float padding = 2.0f * _paddingDip;
    if (! ValidExtent(padding) || ! ValidExtent(gaps))
        return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
    const float available    = (std::max)(0.0f, widthDip - padding);
    const float minimumWidth = _labelPlacement == LabelPlacement::Beside ? 2.0f * kMinimumEditorWidthDip + kLabelGapDip : kMinimumEditorWidthDip;
    float columnWidth        = (std::max)(minimumWidth, (available - gaps) / columns);
    if (_columnMeasureRevision == revision && _columnTypography == context.host.GetTypographyRevision() && _columnHost == &context.host &&
        _columnContextWidth == widthDip && _columnOriginLeft == origin.x)
        columnWidth = (std::max)(columnWidth, _measuredColumnWidth);
    size_t remainingTextUnits = kMaxIntrinsicTextUnits;
    auto boundedContext       = context;
    if (! boundedContext.remainingTextUnits)
        boundedContext.remainingTextUnits = &remainingTextUnits;
    try
    {
        _scratch.reserve(count);
        _scratch.clear();
        // A detached form has no host geometry epoch. Snapshot all inputs before any extensible measurement,
        // so a later editor cannot move an already measured sibling without invalidating this pass.
        for (size_t index = 0; index < count; ++index)
        {
            auto* child = GetChildren()[index].get();
            if (! child || ! child->IsVisible())
                continue;
            auto* field = dynamic_cast<FormField*>(child);
            if (! field)
                return E_INVALIDARG;
            FieldLayout record;
            record.field                        = field;
            record.lifetime                     = GetControlLifetimeToken(*field);
            record.logicalIndex                 = index;
            record.revision                     = field->GetMeasureRevision();
            record.before                       = field->GetBounds();
            const std::array<Control*, 4> parts = {field->_label, field->GetEditor(), field->_description, field->_validation};
            for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex)
            {
                auto* part              = parts[partIndex];
                const auto partBounds   = part ? part->GetBounds() : D2D1::RectF();
                record.parts[partIndex] = {part,
                                           part ? GetControlLifetimeToken(*part) : std::weak_ptr<int>{},
                                           part ? part->GetMeasureRevision() : 0,
                                           part && part->IsVisible(),
                                           partBounds,
                                           partBounds};
            }
            _scratch.push_back(std::move(record));
        }
        // An editor can have a larger intrinsic minimum than its requested lane (for example unit chrome).
        // Settle uniform column widths in a bounded preparation pass, then reuse the settled numeric width.
        for (size_t widthPass = 0; widthPass < 4; ++widthPass)
        {
            float labelWidth    = 0.0f;
            bool stagedOverflow = false;
            if (_labelPlacement == LabelPlacement::Beside)
            {
                const float cap = (std::min)(columnWidth * 0.45f, columnWidth - kMinimumEditorWidthDip - kLabelGapDip);
                for (const auto& record : _scratch)
                {
                    auto* field = record.field;
                    DesiredSize label;
                    const HRESULT hr = field->MeasureSharedLabel(boundedContext, cap, label);
                    if (! current())
                        return E_ABORT;
                    if (FAILED(hr))
                        return hr;
                    labelWidth     = (std::max)(labelWidth, (std::min)(cap, label.sizeDip.width));
                    stagedOverflow = stagedOverflow || label.overflow;
                }
            }
            size_t fieldIndex = 0;
            float rowTop = _paddingDip, rowHeight = 0;
            float requiredColumnWidth = columnWidth;
            for (auto& record : _scratch)
            {
                auto* field             = record.field;
                const size_t column     = fieldIndex % _columns;
                const float offset      = static_cast<float>(column) * (columnWidth + _horizontalGapDip);
                const float left        = origin.x + (IsRightToLeft() ? widthDip - _paddingDip - columnWidth - offset : _paddingDip + offset);
                const float right       = left + columnWidth;
                const float actualWidth = right - left;
                if (! std::isfinite(left) || ! std::isfinite(right) || ! ValidExtent(actualWidth))
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
                DesiredSize desired;
                const HRESULT hr = field->MeasureForLayout(boundedContext, D2D1::RectF(left, 0, right, 0), _labelPlacement, labelWidth, desired);
                if (! current())
                    return E_ABORT;
                if (FAILED(hr))
                    return hr;
                if (desired.sizeDip.width > actualWidth)
                {
                    requiredColumnWidth = (std::max)(requiredColumnWidth, columnWidth + desired.sizeDip.width - actualWidth);
                    if (_labelPlacement == LabelPlacement::Beside && labelWidth > 0)
                    {
                        // Bound the shared-label cap after expansion so its new width cannot consume the editor minimum.
                        const double editorMinimum = static_cast<double>(field->_measureCache[1].desired.sizeDip.width);
                        requiredColumnWidth        = (std::max)(requiredColumnWidth, static_cast<float>((editorMinimum + kLabelGapDip) / 0.55));
                    }
                }
                if (column == 0 && fieldIndex)
                {
                    rowTop += rowHeight + _verticalGapDip;
                    rowHeight = 0;
                }
                const auto rectangle = D2D1::RectF(left, origin.y + rowTop, right, origin.y + rowTop + desired.sizeDip.height);
                if (! ValidRect(rectangle))
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
                record.bounds  = rectangle;
                rowHeight      = (std::max)(rowHeight, desired.sizeDip.height);
                stagedOverflow = stagedOverflow || desired.overflow;
                ++fieldIndex;
            }
            if (! ValidExtent(requiredColumnWidth))
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
            if (requiredColumnWidth > columnWidth)
            {
                columnWidth = requiredColumnWidth;
                continue;
            }
            const auto staged = D2D1::SizeF(padding + (fieldIndex ? columns * columnWidth + gaps : 0.0f), rowTop + rowHeight + _paddingDip);
            if (! ValidExtent(staged.width) || ! ValidExtent(staged.height))
                return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
            // Final translated widths are the cache keys. Stage every field's parts before the first extensible bounds
            // callback; the commit half never remeasures or allocates because of floating-point translation.
            for (auto& record : _scratch)
            {
                std::array<FormField::PartLayout, 4> parts;
                const HRESULT hr = record.field->StageLayout(boundedContext, record.bounds, _labelPlacement, labelWidth, parts);
                if (! current())
                    return E_ABORT;
                if (FAILED(hr))
                    return hr;
                record.parts = std::move(parts);
            }
            _columnMeasureRevision = revision;
            _columnTypography      = context.host.GetTypographyRevision();
            _columnHost            = &context.host;
            _columnContextWidth    = widthDip;
            _columnOriginLeft      = origin.x;
            _measuredColumnWidth   = columnWidth;
            content                = staged;
            labelWidthDip          = labelWidth;
            overflow               = stagedOverflow || content.width > widthDip;
            return S_OK;
        }
        // A custom editor whose minimum keeps growing with each available width cannot establish a stable layout.
        return HRESULT_FROM_WIN32(ERROR_RETRY);
    }
    catch (const std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
}
HRESULT FormPanel::MeasureOverride(const MeasureContext& context, const LayoutConstraints& constraints, DesiredSize& desired) const noexcept
{
    D2D1_SIZE_F content;
    float labelWidth           = 0;
    bool overflow              = false;
    const float preferredWidth = 2.0f * _paddingDip + 180.0f * static_cast<float>(_columns) + static_cast<float>(_columns - 1) * _horizontalGapDip;
    const float width          = constraints.maximumWidthDip.value_or((std::max)(preferredWidth, constraints.minimumDip.width));
    const HRESULT result       = BuildLayout(context, width, D2D1::Point2F(), content, labelWidth, overflow);
    if (FAILED(result))
        return result;
    desired.sizeDip  = content;
    desired.overflow = overflow || (constraints.maximumHeightDip && content.height > *constraints.maximumHeightDip);
    return S_OK;
}
HRESULT FormPanel::ApplyLayout(const ControlHost& host) noexcept
{
    if (IsRenderStageActiveForDebug())
        return E_UNEXPECTED;
    if (GetHost() && GetHost() != &host)
        return E_INVALIDARG;
    const auto lifetime            = GetLifetimeToken();
    const auto bounds              = GetBounds();
    const auto revision            = GetMeasureRevision();
    const auto focus               = host._focusRequestRevision;
    const auto* const attachedHost = GetHost();
    const auto pass                = ++_layoutRevision;
    if (! ValidRect(bounds))
        return E_INVALIDARG;
    D2D1_SIZE_F content;
    float labelWidth = 0;
    bool overflow    = false;
    const HRESULT hr = BuildLayout({host}, bounds.right - bounds.left, D2D1::Point2F(bounds.left, bounds.top), content, labelWidth, overflow);
    if (FAILED(hr) || lifetime.expired())
        return FAILED(hr) ? hr : E_ABORT;
    const auto typography      = host.GetTypographyRevision();
    const auto scratchRevision = _scratchRevision;
    const size_t count         = _scratch.size();
    auto& mutableHost          = const_cast<ControlHost&>(host);
    mutableHost.BeginIntrinsicArrange(*this);
    const auto current = [&](size_t completedFields, size_t arrangedFields) noexcept
    {
        if (lifetime.expired() || _layoutRevision != pass || _scratchRevision != scratchRevision || GetMeasureRevision() != revision ||
            host._focusRequestRevision != focus || GetHost() != attachedHost || host.GetTypographyRevision() != typography || ! SameRect(GetBounds(), bounds) ||
            _scratch.size() != count)
            return false;
        for (size_t recordIndex = 0; recordIndex < count; ++recordIndex)
        {
            const auto& record = _scratch[recordIndex];
            if (record.lifetime.expired() || GetLogicalChild(record.logicalIndex) != record.field || record.field->GetMeasureRevision() != record.revision ||
                ! record.field->IsVisible() || ! SameRect(record.field->GetBounds(), recordIndex < arrangedFields ? record.bounds : record.before) ||
                ! record.field->ArePartsCurrent(record.parts, recordIndex < completedFields ? record.parts.size() : 0))
                return false;
        }
        return true;
    };
    if (! current(0, 0))
        return E_ABORT;
    for (size_t index = 0; index < count; ++index)
    {
        const auto record = _scratch[index];
        if (record.lifetime.expired() || GetLogicalChild(record.logicalIndex) != record.field || record.field->GetMeasureRevision() != record.revision)
            return E_ABORT;
        const auto* const previous          = mutableHost._intrinsicArrangeBranch;
        const auto foreign                  = mutableHost._intrinsicArrangeForeignRevision;
        const auto accepted                 = mutableHost._intrinsicArrangeBoundsRevision;
        const auto interaction              = host.GetInteractionRevision();
        mutableHost._intrinsicArrangeBranch = record.field;
        record.field->SetBounds(record.bounds);
        mutableHost._intrinsicArrangeBranch = previous;
        if (! current(index, index + 1))
            return E_ABORT;
        if (mutableHost._intrinsicArrangeForeignRevision != foreign ||
            host.GetInteractionRevision() - interaction != mutableHost._intrinsicArrangeBoundsRevision - accepted ||
            ! SameRect(record.field->GetBounds(), record.bounds))
            return E_ABORT;
        const HRESULT result = record.field->ApplyLayoutFor(host, _labelPlacement, labelWidth, &record.parts);
        if (! current(index + 1, index + 1))
            return E_ABORT;
        if (FAILED(result))
            return result;
    }
    _contentSize = content;
    _overflow    = overflow || content.height > bounds.bottom - bounds.top;
    return S_OK;
}
HRESULT FormPanel::PrepareIntrinsicLayout(const MeasureContext& context) noexcept
{
    return ApplyLayout(context.host);
}
void FormPanel::OnBoundsChanged() noexcept
{
    ++_layoutRevision;
    RequestIntrinsicLayout();
}
} // namespace DxUi
