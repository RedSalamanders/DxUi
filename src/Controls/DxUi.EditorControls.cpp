#include "DxUi.Internal.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cwctype>
#include <format>
#include <string>
#include <utility>

#include "../Support/Diagnostics.h"

// Editor consumer controls: Splitter, NumericStepper, ColorPicker. See UI_ControlsAndLayout.md.

namespace DxUi
{
namespace
{
constexpr wchar_t kChevronUpGlyph   = L'\xE70E';
constexpr wchar_t kChevronDownGlyph = L'\xE70D';
constexpr float kGripDotRadiusDip   = 1.5f;
constexpr float kGripDotSpacingDip  = 5.0f;
constexpr float kFieldMarkerRadius  = 5.0f;
constexpr double kValueEpsilon      = 1e-9;

[[nodiscard]] float Width(const D2D1_RECT_F& rect) noexcept
{
    return (std::max)(0.0f, rect.right - rect.left);
}

[[nodiscard]] float Height(const D2D1_RECT_F& rect) noexcept
{
    return (std::max)(0.0f, rect.bottom - rect.top);
}

[[nodiscard]] D2D1_RECT_F MirrorRect(const D2D1_RECT_F& rect, const D2D1_RECT_F& within) noexcept
{
    const float left = within.left + (within.right - rect.right);
    return D2D1::RectF(left, rect.top, left + Width(rect), rect.bottom);
}

[[nodiscard]] bool SameRect(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

void FillRect(ControlHost& host, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color) noexcept
{
    if (auto* dc = host.GetDeviceContext())
    {
        dc->FillRectangle(rect, host.GetSolidBrush(color));
    }
}

void StrokeRect(ControlHost& host, const D2D1_RECT_F& rect, const D2D1_COLOR_F& color, float strokeWidth) noexcept
{
    if (auto* dc = host.GetDeviceContext())
    {
        dc->DrawRectangle(rect, host.GetSolidBrush(color), strokeWidth);
    }
}

void FillCircle(ControlHost& host, D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color) noexcept
{
    if (auto* dc = host.GetDeviceContext())
    {
        dc->FillEllipse(D2D1::Ellipse(center, radius, radius), host.GetSolidBrush(color));
    }
}

void StrokeCircle(ControlHost& host, D2D1_POINT_2F center, float radius, const D2D1_COLOR_F& color, float strokeWidth) noexcept
{
    if (auto* dc = host.GetDeviceContext())
    {
        dc->DrawEllipse(D2D1::Ellipse(center, radius, radius), host.GetSolidBrush(color), strokeWidth);
    }
}

void StrokeLine(ControlHost& host, D2D1_POINT_2F from, D2D1_POINT_2F to, const D2D1_COLOR_F& color, float strokeWidth) noexcept
{
    if (auto* dc = host.GetDeviceContext())
    {
        dc->DrawLine(from, to, host.GetSolidBrush(color), strokeWidth);
    }
}

[[nodiscard]] wil::com_ptr<ID2D1LinearGradientBrush> MakeGradientBrush(
    ID2D1DeviceContext* dc, D2D1_POINT_2F start, D2D1_POINT_2F end, const D2D1_GRADIENT_STOP* stops, UINT32 stopCount) noexcept
{
    wil::com_ptr<ID2D1GradientStopCollection> collection;
    if (! dc || FAILED(dc->CreateGradientStopCollection(stops, stopCount, collection.put())))
    {
        return {};
    }
    wil::com_ptr<ID2D1LinearGradientBrush> brush;
    if (FAILED(dc->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(start, end), collection.get(), brush.put())))
    {
        return {};
    }
    return brush;
}

[[nodiscard]] uint8_t Channel(double value) noexcept
{
    return static_cast<uint8_t>((std::clamp)(static_cast<int>(std::lround(value)), 0, 255));
}

[[nodiscard]] uint32_t PackRgb(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha) noexcept
{
    return (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | blue;
}

[[nodiscard]] int HexDigit(wchar_t ch) noexcept
{
    if (ch >= L'0' && ch <= L'9')
    {
        return ch - L'0';
    }
    if (ch >= L'a' && ch <= L'f')
    {
        return 10 + (ch - L'a');
    }
    if (ch >= L'A' && ch <= L'F')
    {
        return 10 + (ch - L'A');
    }
    return -1;
}

[[nodiscard]] size_t HexDigitCount(std::wstring_view text) noexcept
{
    size_t count = 0;
    for (const wchar_t ch : text)
    {
        if (HexDigit(ch) >= 0)
        {
            ++count;
        }
    }
    return count;
}
} // namespace

// ── Color helpers ────────────────────────────────────────────────────────

HsvColor HsvFromArgb(uint32_t argb, const HsvColor* previous) noexcept
{
    const float red   = static_cast<float>((argb >> 16) & 0xFFu) / 255.0f;
    const float green = static_cast<float>((argb >> 8) & 0xFFu) / 255.0f;
    const float blue  = static_cast<float>(argb & 0xFFu) / 255.0f;
    const float high  = (std::max)({red, green, blue});
    const float low   = (std::min)({red, green, blue});
    const float delta = high - low;
    HsvColor result{};
    result.value = high;
    if (high <= 0.0f)
    {
        // Black keeps the previous hue and saturation so the field marker does not jump to the corner.
        result.hue        = previous ? previous->hue : 0.0f;
        result.saturation = previous ? previous->saturation : 0.0f;
        return result;
    }
    result.saturation = delta / high;
    if (delta <= 0.0f)
    {
        result.hue = previous ? previous->hue : 0.0f;
        return result;
    }
    float hue = 0.0f;
    if (high == red)
    {
        hue = 60.0f * std::fmod((green - blue) / delta, 6.0f);
    }
    else if (high == green)
    {
        hue = 60.0f * (((blue - red) / delta) + 2.0f);
    }
    else
    {
        hue = 60.0f * (((red - green) / delta) + 4.0f);
    }
    if (hue < 0.0f)
    {
        hue += 360.0f;
    }
    result.hue = hue >= 360.0f ? 0.0f : hue;
    return result;
}

uint32_t ArgbFromHsv(const HsvColor& hsv, uint8_t alpha) noexcept
{
    const float hue        = std::isfinite(hsv.hue) ? std::fmod(std::fmod(hsv.hue, 360.0f) + 360.0f, 360.0f) : 0.0f;
    const float saturation = std::isfinite(hsv.saturation) ? (std::clamp)(hsv.saturation, 0.0f, 1.0f) : 0.0f;
    const float value      = std::isfinite(hsv.value) ? (std::clamp)(hsv.value, 0.0f, 1.0f) : 0.0f;
    const float chroma     = value * saturation;
    const float sector     = hue / 60.0f;
    const float second     = chroma * (1.0f - std::fabs(std::fmod(sector, 2.0f) - 1.0f));
    float red              = 0.0f;
    float green            = 0.0f;
    float blue             = 0.0f;
    if (sector < 1.0f)
    {
        red   = chroma;
        green = second;
    }
    else if (sector < 2.0f)
    {
        red   = second;
        green = chroma;
    }
    else if (sector < 3.0f)
    {
        green = chroma;
        blue  = second;
    }
    else if (sector < 4.0f)
    {
        green = second;
        blue  = chroma;
    }
    else if (sector < 5.0f)
    {
        red  = second;
        blue = chroma;
    }
    else
    {
        red  = chroma;
        blue = second;
    }
    const float lift = value - chroma;
    return PackRgb(Channel((red + lift) * 255.0), Channel((green + lift) * 255.0), Channel((blue + lift) * 255.0), alpha);
}

std::optional<uint32_t> ParseHexColor(std::wstring_view text) noexcept
{
    while (! text.empty() && std::iswspace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (! text.empty() && std::iswspace(text.back()))
    {
        text.remove_suffix(1);
    }
    if (! text.empty() && text.front() == L'#')
    {
        text.remove_prefix(1);
    }
    if (text.size() != 3 && text.size() != 6)
    {
        return std::nullopt;
    }
    int digits[6]{};
    for (size_t index = 0; index < text.size(); ++index)
    {
        const int digit = HexDigit(text[index]);
        if (digit < 0)
        {
            return std::nullopt;
        }
        digits[index] = digit;
    }
    if (text.size() == 3)
    {
        return PackRgb(static_cast<uint8_t>(digits[0] * 17), static_cast<uint8_t>(digits[1] * 17), static_cast<uint8_t>(digits[2] * 17), 255u);
    }
    return PackRgb(static_cast<uint8_t>(digits[0] * 16 + digits[1]),
                   static_cast<uint8_t>(digits[2] * 16 + digits[3]),
                   static_cast<uint8_t>(digits[4] * 16 + digits[5]),
                   255u);
}

std::wstring FormatHexColor(uint32_t argb)
{
    return std::format(L"#{:02X}{:02X}{:02X}", (argb >> 16) & 0xFFu, (argb >> 8) & 0xFFu, argb & 0xFFu);
}

// ── Splitter ─────────────────────────────────────────────────────────────

Splitter::Splitter()
{
    SetFocusable(true);
}

void Splitter::SetOrientation(SplitterOrientation orientation) noexcept
{
    if (_orientation == orientation)
    {
        return;
    }
    _orientation = orientation;
    _positionDip = ClampPosition(_positionDip);
    RequestInvalidate();
}

SplitterOrientation Splitter::GetOrientation() const noexcept
{
    return _orientation;
}

void Splitter::SetThickness(float thicknessDip) noexcept
{
    if (! std::isfinite(thicknessDip) || thicknessDip < 1.0f)
    {
        return;
    }
    _thicknessDip = thicknessDip;
    _positionDip  = ClampPosition(_positionDip);
    RequestInvalidate();
}

float Splitter::GetThickness() const noexcept
{
    return _thicknessDip;
}

void Splitter::SetMinimumFirstPane(float minimumDip) noexcept
{
    if (! std::isfinite(minimumDip) || minimumDip < 0.0f)
    {
        return;
    }
    _minimumFirstDip = minimumDip;
    _positionDip     = ClampPosition(_positionDip);
    RequestInvalidate();
}

float Splitter::GetMinimumFirstPane() const noexcept
{
    return _minimumFirstDip;
}

void Splitter::SetMinimumSecondPane(float minimumDip) noexcept
{
    if (! std::isfinite(minimumDip) || minimumDip < 0.0f)
    {
        return;
    }
    _minimumSecondDip = minimumDip;
    _positionDip      = ClampPosition(_positionDip);
    RequestInvalidate();
}

float Splitter::GetMinimumSecondPane() const noexcept
{
    return _minimumSecondDip;
}

float Splitter::Extent() const noexcept
{
    const D2D1_RECT_F bounds = GetBounds();
    return _orientation == SplitterOrientation::Vertical ? Width(bounds) : Height(bounds);
}

float Splitter::ClampPosition(float positionDip) const noexcept
{
    if (! std::isfinite(positionDip))
    {
        return _positionDip;
    }
    const float extent = Extent();
    if (extent <= 0.0f)
    {
        // No bounds yet: keep the requested position so a persisted value survives until the first layout.
        return (std::max)(_minimumFirstDip, positionDip);
    }
    const float maximum = (std::max)(_minimumFirstDip, extent - _thicknessDip - _minimumSecondDip);
    return (std::clamp)(positionDip, _minimumFirstDip, maximum);
}

void Splitter::SetPosition(float positionDip) noexcept
{
    const float clamped = ClampPosition(positionDip);
    if (clamped == _positionDip)
    {
        return;
    }
    _positionDip = clamped;
    RequestInvalidate();
}

float Splitter::GetPosition() const noexcept
{
    return _positionDip;
}

bool Splitter::RequestPosition(ControlHost& host, float positionDip) noexcept
{
    if (! IsEnabled() || _dragging || ! std::isfinite(positionDip))
    {
        return false;
    }
    const float clamped = ClampPosition(positionDip);
    if (clamped == _positionDip)
    {
        return true;
    }
    _positionDip = clamped;
    Invalidate(host);
    NotifyChange(SplitterChangePhase::Commit);
    return true;
}

void Splitter::SetOnChange(std::function<void(SplitterChange)> onChange)
{
    _onChange = std::move(onChange);
}

bool Splitter::IsDragging() const noexcept
{
    return _dragging;
}

D2D1_RECT_F Splitter::GetSeparatorBounds() const noexcept
{
    const D2D1_RECT_F bounds = GetBounds();
    if (_orientation == SplitterOrientation::Horizontal)
    {
        const float top = bounds.top + _positionDip;
        return D2D1::RectF(bounds.left, top, bounds.right, (std::min)(bounds.bottom, top + _thicknessDip));
    }
    if (IsRightToLeft())
    {
        const float right = bounds.right - _positionDip;
        return D2D1::RectF((std::max)(bounds.left, right - _thicknessDip), bounds.top, right, bounds.bottom);
    }
    const float left = bounds.left + _positionDip;
    return D2D1::RectF(left, bounds.top, (std::min)(bounds.right, left + _thicknessDip), bounds.bottom);
}

D2D1_RECT_F Splitter::GetFirstPaneBounds() const noexcept
{
    const D2D1_RECT_F bounds    = GetBounds();
    const D2D1_RECT_F separator = GetSeparatorBounds();
    if (_orientation == SplitterOrientation::Horizontal)
    {
        return D2D1::RectF(bounds.left, bounds.top, bounds.right, separator.top);
    }
    if (IsRightToLeft())
    {
        return D2D1::RectF(separator.right, bounds.top, bounds.right, bounds.bottom);
    }
    return D2D1::RectF(bounds.left, bounds.top, separator.left, bounds.bottom);
}

D2D1_RECT_F Splitter::GetSecondPaneBounds() const noexcept
{
    const D2D1_RECT_F bounds    = GetBounds();
    const D2D1_RECT_F separator = GetSeparatorBounds();
    if (_orientation == SplitterOrientation::Horizontal)
    {
        return D2D1::RectF(bounds.left, separator.bottom, bounds.right, bounds.bottom);
    }
    if (IsRightToLeft())
    {
        return D2D1::RectF(bounds.left, bounds.top, separator.left, bounds.bottom);
    }
    return D2D1::RectF(separator.right, bounds.top, bounds.right, bounds.bottom);
}

D2D1_RECT_F Splitter::GetHitBounds() const noexcept
{
    const D2D1_RECT_F bounds    = GetBounds();
    const D2D1_RECT_F separator = GetSeparatorBounds();
    if (Extent() <= 0.0f)
    {
        return D2D1::RectF();
    }
    const D2D1_RECT_F inflated =
        _orientation == SplitterOrientation::Horizontal ? InflateRect(separator, 0.0f, kHitSlopDip) : InflateRect(separator, kHitSlopDip, 0.0f);
    return D2D1::RectF((std::max)(bounds.left, inflated.left),
                       (std::max)(bounds.top, inflated.top),
                       (std::min)(bounds.right, inflated.right),
                       (std::min)(bounds.bottom, inflated.bottom));
}

float Splitter::PointerAxis(D2D1_POINT_2F point) const noexcept
{
    const D2D1_RECT_F bounds = GetBounds();
    if (_orientation == SplitterOrientation::Horizontal)
    {
        return point.y - bounds.top;
    }
    return IsRightToLeft() ? (bounds.right - point.x) : (point.x - bounds.left);
}

void Splitter::Paint(ControlHost& host) const
{
    const ThemePalette& theme   = host.GetTheme();
    const D2D1_RECT_F separator = GetSeparatorBounds();
    if (Width(separator) <= 0.0f || Height(separator) <= 0.0f)
    {
        return;
    }
    D2D1_COLOR_F fill = theme.border;
    if (! IsEnabled())
    {
        fill = BlendColor(theme.windowBackground, theme.border, theme.dark ? 0.6f : 0.5f);
    }
    else if (_dragging)
    {
        fill = theme.accent;
    }
    else if (IsHovered())
    {
        fill = BlendColor(theme.border, theme.accent, 0.5f);
    }
    FillRect(host, separator, fill);

    const D2D1_COLOR_F grip    = _dragging ? theme.selectionText : (IsEnabled() ? theme.subduedText : theme.disabledText);
    const D2D1_POINT_2F center = D2D1::Point2F((separator.left + separator.right) * 0.5f, (separator.top + separator.bottom) * 0.5f);
    const bool horizontalGrip  = _orientation == SplitterOrientation::Horizontal;
    for (int index = -1; index <= 1; ++index)
    {
        const float offset       = static_cast<float>(index) * kGripDotSpacingDip;
        const D2D1_POINT_2F spot = horizontalGrip ? D2D1::Point2F(center.x + offset, center.y) : D2D1::Point2F(center.x, center.y + offset);
        FillCircle(host, spot, kGripDotRadiusDip, grip);
    }
    if (HasFocus() && host.IsKeyboardFocusVisible())
    {
        PaintFocusRing(host, separator, 2.0f);
    }
}

bool Splitter::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT /*modifiers*/)
{
    if (! IsEnabled() || rightButton || ! PointInRect(GetHitBounds(), point))
    {
        return false;
    }
    host.SetFocusControl(this);
    _dragging             = true;
    _dragStartPositionDip = _positionDip;
    _dragPointerOffsetDip = PointerAxis(point) - _positionDip;
    host.CaptureMouse(this);
    Invalidate(host);
    return true;
}

bool Splitter::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    if (! _dragging)
    {
        return false;
    }
    const float next = ClampPosition(PointerAxis(point) - _dragPointerOffsetDip);
    if (next != _positionDip)
    {
        _positionDip = next;
        Invalidate(host);
        NotifyChange(SplitterChangePhase::Preview);
    }
    return true;
}

bool Splitter::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    if (rightButton || ! _dragging)
    {
        return false;
    }
    const auto life = GetLifetimeToken();
    OnMouseMove(host, point, modifiers);
    if (life.expired())
    {
        return true;
    }
    _dragging = false;
    host.ReleaseMouseCapture();
    Invalidate(host);
    NotifyChange(SplitterChangePhase::Commit);
    return true;
}

bool Splitter::OnKeyDown(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    if (! IsEnabled())
    {
        return false;
    }
    if (virtualKey == VK_ESCAPE && _dragging)
    {
        OnCaptureLost(host);
        host.ReleaseMouseCapture();
        return true;
    }
    if (_dragging)
    {
        return false;
    }
    const float step      = ModifiersContainShift(modifiers) ? kKeyboardLargeStepDip : kKeyboardStepDip;
    const bool horizontal = _orientation == SplitterOrientation::Vertical;
    const bool rtl        = IsRightToLeft();
    float target          = _positionDip;
    switch (virtualKey)
    {
        case VK_LEFT:
            if (! horizontal)
            {
                return false;
            }
            target += rtl ? step : -step;
            break;
        case VK_RIGHT:
            if (! horizontal)
            {
                return false;
            }
            target += rtl ? -step : step;
            break;
        case VK_UP:
            if (horizontal)
            {
                return false;
            }
            target -= step;
            break;
        case VK_DOWN:
            if (horizontal)
            {
                return false;
            }
            target += step;
            break;
        case VK_HOME: target = _minimumFirstDip; break;
        case VK_END: target = Extent(); break;
        default: return false;
    }
    const float clamped = ClampPosition(target);
    if (clamped != _positionDip)
    {
        _positionDip = clamped;
        Invalidate(host);
        NotifyChange(SplitterChangePhase::Commit);
    }
    return true;
}

void Splitter::OnCaptureLost(ControlHost& host)
{
    if (! _dragging)
    {
        return;
    }
    _dragging    = false;
    _positionDip = ClampPosition(_dragStartPositionDip);
    Invalidate(host);
    NotifyChange(SplitterChangePhase::Cancel);
}

void Splitter::OnBoundsChanged() noexcept
{
    Control::OnBoundsChanged();
    _positionDip = ClampPosition(_positionDip);
}

WindowHostCursorKind Splitter::ResolveCursorKind(ControlHost& /*host*/, D2D1_POINT_2F pointDip) const noexcept
{
    if (! IsEnabled() || (! _dragging && ! PointInRect(GetHitBounds(), pointDip)))
    {
        return WindowHostCursorKind::Default;
    }
    return _orientation == SplitterOrientation::Vertical ? WindowHostCursorKind::HorizontalResize : WindowHostCursorKind::VerticalResize;
}

void Splitter::NotifyChange(SplitterChangePhase phase) noexcept
{
    try
    {
        const auto changed = _onChange;
        const float value  = _positionDip;
        if (changed)
        {
            changed({phase, value});
        }
    }
    catch (const std::exception&)
    {
        Debug::Warning(L"Splitter change callback failed");
    }
}

// ── NumericStepper ───────────────────────────────────────────────────────

NumericStepper::NumericStepper()
{
    _field     = AddChild<TextField>();
    _increment = AddChild<Button>(std::wstring(1, kChevronUpGlyph));
    _decrement = AddChild<Button>(std::wstring(1, kChevronDownGlyph));
    _increment->SetVariant(ButtonVariant::IconOnly);
    _decrement->SetVariant(ButtonVariant::IconOnly);
    _increment->SetFocusable(false);
    _decrement->SetFocusable(false);
    _increment->SetOnClick([this]
    {
        if (ControlHost* host = GetHost())
        {
            Nudge(*host, +1, false);
        }
        else
        {
            ApplyValue(nullptr, _value + _step, NumericStepperChangePhase::Commit);
        }
    });
    _decrement->SetOnClick([this]
    {
        if (ControlHost* host = GetHost())
        {
            Nudge(*host, -1, false);
        }
        else
        {
            ApplyValue(nullptr, _value - _step, NumericStepperChangePhase::Commit);
        }
    });
    _field->SetOnTextChanged([this](std::wstring_view text)
    {
        if (_syncing)
        {
            return;
        }
        const std::optional<double> parsed = ParseValue(text);
        if (! parsed.has_value())
        {
            return;
        }
        if (! _editing)
        {
            _editing   = true;
            _editStart = _value;
        }
        const double clamped = ClampValue(parsed.value());
        if (std::fabs(clamped - _value) <= kValueEpsilon)
        {
            return;
        }
        _value = clamped;
        NotifyChange(NumericStepperChangePhase::Preview);
    });
    _field->SetOnPreviewKeyDown([this](ControlHost& host, UINT virtualKey, UINT modifiers) -> bool
    {
        switch (virtualKey)
        {
            case VK_UP: Nudge(host, +1, ModifiersContainShift(modifiers)); return true;
            case VK_DOWN: Nudge(host, -1, ModifiersContainShift(modifiers)); return true;
            case VK_ESCAPE:
                if (! _editing)
                {
                    return false;
                }
                CancelEdit(&host);
                return true;
            default: return false;
        }
    });
    _field->SetOnSubmitted([this] { static_cast<void>(CommitEdit(GetHost())); });
    _field->SetOnBlur([this] { static_cast<void>(CommitEdit(GetHost())); });
    SyncText();
}

void NumericStepper::SetLabel(std::wstring label, float widthDip)
{
    _label         = std::move(label);
    _labelWidthDip = std::isfinite(widthDip) ? (std::max)(0.0f, widthDip) : 0.0f;
    _field->SetAccessibleName(_label);
    Arrange();
    RequestInvalidate();
}

std::wstring_view NumericStepper::GetLabel() const noexcept
{
    return _label;
}

void NumericStepper::SetUnit(std::wstring unit, float widthDip)
{
    _unit         = std::move(unit);
    _unitWidthDip = std::isfinite(widthDip) ? (std::max)(0.0f, widthDip) : 0.0f;
    Arrange();
    RequestInvalidate();
}

std::wstring_view NumericStepper::GetUnit() const noexcept
{
    return _unit;
}

void NumericStepper::SetMinimum(double minimum) noexcept
{
    if (! std::isfinite(minimum))
    {
        return;
    }
    _minimum = minimum;
    _maximum = (std::max)(_maximum, _minimum);
    SetValue(_value);
}

double NumericStepper::GetMinimum() const noexcept
{
    return _minimum;
}

void NumericStepper::SetMaximum(double maximum) noexcept
{
    if (! std::isfinite(maximum))
    {
        return;
    }
    _maximum = maximum;
    _minimum = (std::min)(_minimum, _maximum);
    SetValue(_value);
}

double NumericStepper::GetMaximum() const noexcept
{
    return _maximum;
}

void NumericStepper::SetStep(double step) noexcept
{
    if (std::isfinite(step) && step > 0.0)
    {
        _step = step;
    }
}

double NumericStepper::GetStep() const noexcept
{
    return _step;
}

void NumericStepper::SetLargeStep(double step) noexcept
{
    if (std::isfinite(step) && step > 0.0)
    {
        _largeStep = step;
    }
}

double NumericStepper::GetLargeStep() const noexcept
{
    return _largeStep;
}

void NumericStepper::SetDecimals(uint8_t decimals) noexcept
{
    _decimals = (std::min)(decimals, static_cast<uint8_t>(6));
    SyncText();
}

uint8_t NumericStepper::GetDecimals() const noexcept
{
    return _decimals;
}

double NumericStepper::ClampValue(double value) const noexcept
{
    if (! std::isfinite(value))
    {
        return _value;
    }
    // The value is what the text shows: round to the displayed decimals before clamping.
    const double unit    = std::pow(10.0, static_cast<double>(_decimals));
    const double rounded = std::round(value * unit) / unit;
    return (std::clamp)(rounded, _minimum, _maximum);
}

void NumericStepper::SetValue(double value) noexcept
{
    _value   = ClampValue(value);
    _editing = false;
    SyncText();
    RequestInvalidate();
}

double NumericStepper::GetValue() const noexcept
{
    return _value;
}

bool NumericStepper::RequestValue(ControlHost& host, double value) noexcept
{
    if (! IsEnabled() || ! std::isfinite(value))
    {
        return false;
    }
    ApplyValue(&host, value, NumericStepperChangePhase::Commit);
    return true;
}

bool NumericStepper::Nudge(ControlHost& host, int direction, bool large) noexcept
{
    if (! IsEnabled() || direction == 0)
    {
        return false;
    }
    const double amount = (large ? _largeStep : _step) * (direction > 0 ? 1.0 : -1.0);
    const double target = ClampValue(_value + amount);
    if (std::fabs(target - _value) <= kValueEpsilon)
    {
        return false;
    }
    ApplyValue(&host, target, NumericStepperChangePhase::Commit);
    return true;
}

void NumericStepper::ApplyValue(ControlHost* host, double value, NumericStepperChangePhase phase) noexcept
{
    const double clamped = ClampValue(value);
    const bool changed   = std::fabs(clamped - _value) > kValueEpsilon;
    _value               = clamped;
    _editing             = false;
    SyncText();
    if (host)
    {
        Invalidate(*host);
    }
    else
    {
        RequestInvalidate();
    }
    if (changed || phase != NumericStepperChangePhase::Commit)
    {
        NotifyChange(phase);
    }
}

void NumericStepper::SetOnChange(std::function<void(NumericStepperChange)> onChange)
{
    _onChange = std::move(onChange);
}

bool NumericStepper::IsEditing() const noexcept
{
    return _editing;
}

TextField& NumericStepper::Field() noexcept
{
    return *_field;
}

const TextField& NumericStepper::Field() const noexcept
{
    return *_field;
}

Button& NumericStepper::IncrementButton() noexcept
{
    return *_increment;
}

Button& NumericStepper::DecrementButton() noexcept
{
    return *_decrement;
}

std::wstring NumericStepper::FormatValue(double value) const
{
    double rounded    = value;
    const double unit = std::pow(10.0, static_cast<double>(_decimals));
    rounded           = std::round(value * unit) / unit;
    if (std::fabs(rounded) < 0.5 / unit)
    {
        rounded = 0.0; // never "-0"
    }
    return std::format(L"{:.{}f}", rounded, static_cast<int>(_decimals));
}

std::optional<double> NumericStepper::ParseValue(std::wstring_view text) noexcept
{
    while (! text.empty() && std::iswspace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (! text.empty() && std::iswspace(text.back()))
    {
        text.remove_suffix(1);
    }
    if (text.empty() || text.size() > 32)
    {
        return std::nullopt;
    }
    char narrow[33]{};
    size_t length = 0;
    bool sawDigit = false;
    bool sawPoint = false;
    for (size_t index = 0; index < text.size(); ++index)
    {
        wchar_t ch = text[index];
        if (ch == L',')
        {
            ch = L'.';
        }
        if ((ch == L'+' || ch == L'-') && index == 0)
        {
            if (ch == L'-')
            {
                narrow[length++] = '-'; // from_chars accepts no leading '+'
            }
            continue;
        }
        if (ch == L'.')
        {
            if (sawPoint)
            {
                return std::nullopt;
            }
            sawPoint         = true;
            narrow[length++] = '.';
            continue;
        }
        if (ch < L'0' || ch > L'9')
        {
            return std::nullopt;
        }
        sawDigit         = true;
        narrow[length++] = static_cast<char>(ch);
    }
    if (! sawDigit)
    {
        return std::nullopt;
    }
    double value                        = 0.0;
    const std::from_chars_result result = std::from_chars(narrow, narrow + length, value);
    if (result.ec != std::errc{} || result.ptr != narrow + length || ! std::isfinite(value))
    {
        return std::nullopt;
    }
    return value;
}

void NumericStepper::SyncText()
{
    _syncing = true;
    _field->SetText(FormatValue(_value));
    _syncing = false;
}

bool NumericStepper::CommitEdit(ControlHost* host) noexcept
{
    const std::optional<double> parsed = ParseValue(_field->GetText());
    if (! _editing)
    {
        if (! parsed.has_value() || std::fabs(ClampValue(parsed.value()) - _value) > kValueEpsilon)
        {
            SyncText();
        }
        return false;
    }
    const double start = _editStart;
    const double value = parsed.has_value() ? ClampValue(parsed.value()) : _value;
    _value             = value;
    _editing           = false;
    SyncText();
    if (host)
    {
        Invalidate(*host);
    }
    else
    {
        RequestInvalidate();
    }
    if (std::fabs(value - start) > kValueEpsilon)
    {
        NotifyChange(NumericStepperChangePhase::Commit);
        return true;
    }
    return false;
}

void NumericStepper::CancelEdit(ControlHost* host) noexcept
{
    if (! _editing)
    {
        SyncText();
        return;
    }
    _value   = ClampValue(_editStart);
    _editing = false;
    SyncText();
    if (host)
    {
        Invalidate(*host);
    }
    else
    {
        RequestInvalidate();
    }
    NotifyChange(NumericStepperChangePhase::Cancel);
}

void NumericStepper::NotifyChange(NumericStepperChangePhase phase) noexcept
{
    try
    {
        const auto changed = _onChange;
        const double value = _value;
        if (changed)
        {
            changed({phase, value});
        }
    }
    catch (const std::exception&)
    {
        Debug::Warning(L"NumericStepper change callback failed");
    }
}

void NumericStepper::Arrange() noexcept
{
    const D2D1_RECT_F bounds = GetBounds();
    const float width        = Width(bounds);
    const float height       = Height(bounds);
    if (! _field || ! _increment || ! _decrement)
    {
        return;
    }
    float left      = bounds.left;
    float right     = bounds.right;
    const float mid = bounds.top + height * 0.5f;
    if (_labelWidthDip > 0.0f)
    {
        left += _labelWidthDip + kGapDip;
    }
    const float buttonWidth = (std::min)(kButtonWidthDip, width);
    D2D1_RECT_F increment   = D2D1::RectF(right - buttonWidth, bounds.top, right, mid);
    D2D1_RECT_F decrement   = D2D1::RectF(right - buttonWidth, mid, right, bounds.bottom);
    right                   = increment.left - kGapDip;
    if (_unitWidthDip > 0.0f)
    {
        right -= _unitWidthDip + kGapDip;
    }
    D2D1_RECT_F field = D2D1::RectF(left, bounds.top, (std::max)(left, right), bounds.bottom);
    if (IsRightToLeft())
    {
        field     = MirrorRect(field, bounds);
        increment = MirrorRect(increment, bounds);
        decrement = MirrorRect(decrement, bounds);
    }
    _field->SetBounds(field);
    _increment->SetBounds(increment);
    _decrement->SetBounds(decrement);
}

void NumericStepper::Paint(ControlHost& host) const
{
    const ThemePalette& theme = host.GetTheme();
    const D2D1_RECT_F bounds  = GetBounds();
    const D2D1_COLOR_F text   = IsEnabled() ? theme.text : theme.disabledText;
    const D2D1_COLOR_F subtle = IsEnabled() ? theme.subduedText : theme.disabledText;
    const bool rtl            = IsRightToLeft();
    if (_labelWidthDip > 0.0f && ! _label.empty())
    {
        D2D1_RECT_F label = D2D1::RectF(bounds.left, bounds.top, bounds.left + _labelWidthDip, bounds.bottom);
        if (rtl)
        {
            label = MirrorRect(label, bounds);
        }
        DrawCenteredText(host, _label, label, FontRole::Body, text, rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    if (_unitWidthDip > 0.0f && ! _unit.empty() && _field)
    {
        const D2D1_RECT_F field = _field->GetBounds();
        D2D1_RECT_F unit        = rtl ? D2D1::RectF(field.left - kGapDip - _unitWidthDip, bounds.top, field.left - kGapDip, bounds.bottom)
                                      : D2D1::RectF(field.right + kGapDip, bounds.top, field.right + kGapDip + _unitWidthDip, bounds.bottom);
        DrawCenteredText(host, _unit, unit, FontRole::Small, subtle, rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    Panel::Paint(host);
}

void NumericStepper::OnBoundsChanged() noexcept
{
    Panel::OnBoundsChanged();
    Arrange();
}

void NumericStepper::OnEnabledChanged(bool enabled) noexcept
{
    Panel::OnEnabledChanged(enabled);
    if (_field)
    {
        _field->SetEnabled(enabled);
    }
    if (_increment)
    {
        _increment->SetEnabled(enabled);
    }
    if (_decrement)
    {
        _decrement->SetEnabled(enabled);
    }
}

void NumericStepper::OnFlowDirectionChanged() noexcept
{
    Panel::OnFlowDirectionChanged();
    Arrange();
}

// ── ColorPicker ──────────────────────────────────────────────────────────

ColorPicker::ColorPicker()
{
    SetFocusable(true);
    _red    = AddChild<NumericStepper>();
    _green  = AddChild<NumericStepper>();
    _blue   = AddChild<NumericStepper>();
    _hex    = AddChild<TextField>();
    _ok     = AddChild<Button>(_labels.ok);
    _cancel = AddChild<Button>(_labels.cancel);
    _ok->SetPrimary(true);
    for (NumericStepper* stepper : {_red, _green, _blue})
    {
        stepper->SetMinimum(0.0);
        stepper->SetMaximum(255.0);
        stepper->SetStep(1.0);
        stepper->SetLargeStep(10.0);
        stepper->SetDecimals(0);
        stepper->SetOnChange([this](NumericStepperChange change)
        {
            if (_syncing || change.phase == NumericStepperChangePhase::Cancel)
            {
                return;
            }
            const uint32_t argb = PackRgb(Channel(_red->GetValue()), Channel(_green->GetValue()), Channel(_blue->GetValue()), 255u);
            ApplyArgb(GetHost(), argb, true);
        });
    }
    _red->SetLabel(_labels.red, 14.0f);
    _green->SetLabel(_labels.green, 14.0f);
    _blue->SetLabel(_labels.blue, 14.0f);
    _hex->SetOnTextChanged([this](std::wstring_view text)
    {
        if (_syncing)
        {
            return;
        }
        // Live preview waits for all six digits; "#abc" would otherwise flash a short-form color mid-typing.
        if (const std::optional<uint32_t> parsed = ParseHexColor(text); parsed.has_value() && HexDigitCount(text) == 6u)
        {
            ApplyArgb(GetHost(), parsed.value(), true);
        }
    });
    _hex->SetOnSubmitted([this]
    {
        ControlHost* host = GetHost();
        if (const std::optional<uint32_t> parsed = ParseHexColor(_hex->GetText()); parsed.has_value())
        {
            ApplyArgb(host, parsed.value(), true);
        }
        if (host)
        {
            Commit(*host);
        }
    });
    _hex->SetOnBlur([this]
    {
        if (const std::optional<uint32_t> parsed = ParseHexColor(_hex->GetText()); parsed.has_value())
        {
            ApplyArgb(GetHost(), parsed.value(), true);
        }
        SyncChildren();
    });
    _hex->SetAccessibleName(_labels.hex);
    _ok->SetOnClick([this]
    {
        if (ControlHost* host = GetHost())
        {
            Commit(*host);
        }
    });
    _cancel->SetOnClick([this]
    {
        if (ControlHost* host = GetHost())
        {
            Cancel(*host);
        }
    });
    _hsv = HsvFromArgb(_argb, nullptr);
    SyncChildren();
}

void ColorPicker::SetLabels(Labels labels)
{
    _labels = std::move(labels);
    _red->SetLabel(_labels.red, 14.0f);
    _green->SetLabel(_labels.green, 14.0f);
    _blue->SetLabel(_labels.blue, 14.0f);
    _hex->SetAccessibleName(_labels.hex);
    _ok->SetText(_labels.ok);
    _cancel->SetText(_labels.cancel);
    RequestInvalidate();
}

const ColorPicker::Labels& ColorPicker::GetLabels() const noexcept
{
    return _labels;
}

void ColorPicker::SetColor(uint32_t argb) noexcept
{
    const uint32_t opaque = argb | 0xFF000000u;
    _argb                 = opaque;
    _current              = opaque;
    _hsv                  = HsvFromArgb(opaque, &_hsv);
    SyncChildren();
    RequestInvalidate();
}

uint32_t ColorPicker::GetColor() const noexcept
{
    return _argb;
}

void ColorPicker::SetCurrentColor(uint32_t argb) noexcept
{
    _current = argb | 0xFF000000u;
    RequestInvalidate();
}

uint32_t ColorPicker::GetCurrentColor() const noexcept
{
    return _current;
}

HsvColor ColorPicker::GetHsv() const noexcept
{
    return _hsv;
}

void ColorPicker::SampleColor(ControlHost& host, uint32_t argb) noexcept
{
    ApplyArgb(&host, argb | 0xFF000000u, true);
}

void ColorPicker::Commit(ControlHost& host) noexcept
{
    _drag    = Drag::None;
    _current = _argb;
    SyncChildren();
    Invalidate(host);
    NotifyChange(ColorPickerChangePhase::Commit);
}

void ColorPicker::Cancel(ControlHost& host) noexcept
{
    _drag = Drag::None;
    _argb = _current;
    _hsv  = HsvFromArgb(_argb, &_hsv);
    SyncChildren();
    Invalidate(host);
    NotifyChange(ColorPickerChangePhase::Cancel);
}

void ColorPicker::SetOnChange(std::function<void(ColorPickerChange)> onChange)
{
    _onChange = std::move(onChange);
}

bool ColorPicker::IsDragging() const noexcept
{
    return _drag != Drag::None;
}

D2D1_RECT_F ColorPicker::GetFieldRect() const noexcept
{
    const D2D1_RECT_F bounds = GetBounds();
    const float side         = (std::min)(kFieldDip, (std::max)(0.0f, (std::min)(Width(bounds), Height(bounds)) - 2.0f * kGapDip));
    D2D1_RECT_F field        = D2D1::RectF(bounds.left + kGapDip, bounds.top + kGapDip, bounds.left + kGapDip + side, bounds.top + kGapDip + side);
    return IsRightToLeft() ? MirrorRect(field, bounds) : field;
}

D2D1_RECT_F ColorPicker::GetHueStripRect() const noexcept
{
    const D2D1_RECT_F field = GetFieldRect();
    return IsRightToLeft() ? D2D1::RectF(field.left - kGapDip - kHueStripWidthDip, field.top, field.left - kGapDip, field.bottom)
                           : D2D1::RectF(field.right + kGapDip, field.top, field.right + kGapDip + kHueStripWidthDip, field.bottom);
}

D2D1_RECT_F ColorPicker::GetNewSwatchRect() const noexcept
{
    const D2D1_RECT_F strip = GetHueStripRect();
    const float left        = IsRightToLeft() ? (strip.left - kGapDip - 2.0f * kSwatchWidthDip - kGapDip) : (strip.right + kGapDip);
    return D2D1::RectF(left, strip.top, left + kSwatchWidthDip, strip.top + kSwatchDip);
}

D2D1_RECT_F ColorPicker::GetCurrentSwatchRect() const noexcept
{
    const D2D1_RECT_F fresh = GetNewSwatchRect();
    return D2D1::RectF(fresh.right + kGapDip, fresh.top, fresh.right + kGapDip + kSwatchWidthDip, fresh.bottom);
}

NumericStepper& ColorPicker::RedField() noexcept
{
    return *_red;
}

NumericStepper& ColorPicker::GreenField() noexcept
{
    return *_green;
}

NumericStepper& ColorPicker::BlueField() noexcept
{
    return *_blue;
}

TextField& ColorPicker::HexField() noexcept
{
    return *_hex;
}

Button& ColorPicker::OkButton() noexcept
{
    return *_ok;
}

Button& ColorPicker::CancelButton() noexcept
{
    return *_cancel;
}

void ColorPicker::Arrange() noexcept
{
    if (! _red || ! _green || ! _blue || ! _hex || ! _ok || ! _cancel)
    {
        return;
    }
    const D2D1_RECT_F bounds = GetBounds();
    const D2D1_RECT_F strip  = GetHueStripRect();
    const bool rtl           = IsRightToLeft();
    const float columnLeft   = rtl ? bounds.left + kGapDip : strip.right + kGapDip;
    const float columnRight  = rtl ? strip.left - kGapDip : bounds.right - kGapDip;
    const float columnWidth  = (std::max)(0.0f, columnRight - columnLeft);
    float y                  = strip.top + kSwatchDip + 16.0f + kGapDip;
    const float rowStep      = kRowDip + kGapDip;
    const auto row           = [&](float top) { return D2D1::RectF(columnLeft, top, columnLeft + columnWidth, top + kRowDip); };
    _red->SetBounds(row(y));
    _green->SetBounds(row(y + rowStep));
    _blue->SetBounds(row(y + 2.0f * rowStep));
    const D2D1_RECT_F hexRow = row(y + 3.0f * rowStep);
    const float hexLabel     = 26.0f;
    _hex->SetBounds(rtl ? D2D1::RectF(hexRow.left, hexRow.top, (std::max)(hexRow.left, hexRow.right - hexLabel - kGapDip), hexRow.bottom)
                        : D2D1::RectF((std::min)(hexRow.right, hexRow.left + hexLabel + kGapDip), hexRow.top, hexRow.right, hexRow.bottom));
    const float buttonTop   = bounds.bottom - kGapDip - kRowDip;
    const float buttonWidth = (std::min)(kButtonWidthDip, (std::max)(0.0f, (Width(bounds) - 3.0f * kGapDip) * 0.5f));
    D2D1_RECT_F ok =
        D2D1::RectF(bounds.right - kGapDip - 2.0f * buttonWidth - kGapDip, buttonTop, bounds.right - 2.0f * kGapDip - buttonWidth, buttonTop + kRowDip);
    D2D1_RECT_F cancel = D2D1::RectF(bounds.right - kGapDip - buttonWidth, buttonTop, bounds.right - kGapDip, buttonTop + kRowDip);
    if (rtl)
    {
        ok     = MirrorRect(ok, bounds);
        cancel = MirrorRect(cancel, bounds);
    }
    _ok->SetBounds(ok);
    _cancel->SetBounds(cancel);
}

void ColorPicker::SyncChildren() noexcept
{
    _syncing = true;
    _red->SetValue(static_cast<double>((_argb >> 16) & 0xFFu));
    _green->SetValue(static_cast<double>((_argb >> 8) & 0xFFu));
    _blue->SetValue(static_cast<double>(_argb & 0xFFu));
    _hex->SetText(FormatHexColor(_argb));
    _syncing = false;
}

void ColorPicker::ApplyArgb(ControlHost* host, uint32_t argb, bool notify) noexcept
{
    const uint32_t opaque = argb | 0xFF000000u;
    if (opaque == _argb)
    {
        return;
    }
    _argb = opaque;
    _hsv  = HsvFromArgb(opaque, &_hsv);
    SyncChildren();
    if (host)
    {
        Invalidate(*host);
    }
    else
    {
        RequestInvalidate();
    }
    if (notify)
    {
        NotifyChange(ColorPickerChangePhase::Preview);
    }
}

void ColorPicker::ApplyHsv(ControlHost* host, HsvColor hsv, bool notify) noexcept
{
    hsv.hue             = std::isfinite(hsv.hue) ? std::fmod(std::fmod(hsv.hue, 360.0f) + 360.0f, 360.0f) : _hsv.hue;
    hsv.saturation      = std::isfinite(hsv.saturation) ? (std::clamp)(hsv.saturation, 0.0f, 1.0f) : _hsv.saturation;
    hsv.value           = std::isfinite(hsv.value) ? (std::clamp)(hsv.value, 0.0f, 1.0f) : _hsv.value;
    const uint32_t argb = ArgbFromHsv(hsv, 255u);
    const bool same     = argb == _argb;
    _hsv                = hsv; // keep the exact hue/saturation even where the 8-bit color cannot express it
    if (same)
    {
        if (host)
        {
            Invalidate(*host);
        }
        return;
    }
    _argb = argb;
    SyncChildren();
    if (host)
    {
        Invalidate(*host);
    }
    else
    {
        RequestInvalidate();
    }
    if (notify)
    {
        NotifyChange(ColorPickerChangePhase::Preview);
    }
}

void ColorPicker::UpdateFromPoint(ControlHost& host, D2D1_POINT_2F point) noexcept
{
    HsvColor hsv = _hsv;
    if (_drag == Drag::Field)
    {
        const D2D1_RECT_F field = GetFieldRect();
        const float width       = (std::max)(1.0f, Width(field));
        const float height      = (std::max)(1.0f, Height(field));
        hsv.saturation          = (std::clamp)((point.x - field.left) / width, 0.0f, 1.0f);
        hsv.value               = 1.0f - (std::clamp)((point.y - field.top) / height, 0.0f, 1.0f);
    }
    else if (_drag == Drag::Hue)
    {
        const D2D1_RECT_F strip = GetHueStripRect();
        const float height      = (std::max)(1.0f, Height(strip));
        hsv.hue                 = (std::clamp)((point.y - strip.top) / height, 0.0f, 0.99999f) * 360.0f;
    }
    else
    {
        return;
    }
    ApplyHsv(&host, hsv, true);
}

void ColorPicker::EnsureBrushes(ControlHost& host) const noexcept
{
    auto* dc                = host.GetDeviceContext();
    const D2D1_RECT_F field = GetFieldRect();
    const D2D1_RECT_F strip = GetHueStripRect();
    if (! dc)
    {
        return;
    }
    if (dc == _brushContext && _brushHue == _hsv.hue && SameRect(field, _brushField) && SameRect(strip, _brushStrip) && _saturationBrush && _valueBrush &&
        _hueBrush)
    {
        return;
    }
    const D2D1_COLOR_F hueColor                 = ColorFromArgb(ArgbFromHsv(HsvColor{_hsv.hue, 1.0f, 1.0f}, 255u));
    const D2D1_GRADIENT_STOP saturationStops[2] = {
        D2D1::GradientStop(0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f)),
        D2D1::GradientStop(1.0f, hueColor),
    };
    const D2D1_GRADIENT_STOP valueStops[2] = {
        D2D1::GradientStop(0.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f)),
        D2D1::GradientStop(1.0f, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f)),
    };
    const D2D1_GRADIENT_STOP hueStops[7] = {
        D2D1::GradientStop(0.0f, D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f)),
        D2D1::GradientStop(1.0f / 6.0f, D2D1::ColorF(1.0f, 1.0f, 0.0f, 1.0f)),
        D2D1::GradientStop(2.0f / 6.0f, D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f)),
        D2D1::GradientStop(3.0f / 6.0f, D2D1::ColorF(0.0f, 1.0f, 1.0f, 1.0f)),
        D2D1::GradientStop(4.0f / 6.0f, D2D1::ColorF(0.0f, 0.0f, 1.0f, 1.0f)),
        D2D1::GradientStop(5.0f / 6.0f, D2D1::ColorF(1.0f, 0.0f, 1.0f, 1.0f)),
        D2D1::GradientStop(1.0f, D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f)),
    };
    _saturationBrush = MakeGradientBrush(dc, D2D1::Point2F(field.left, field.top), D2D1::Point2F(field.right, field.top), saturationStops, 2u);
    _valueBrush      = MakeGradientBrush(dc, D2D1::Point2F(field.left, field.top), D2D1::Point2F(field.left, field.bottom), valueStops, 2u);
    _hueBrush        = MakeGradientBrush(dc, D2D1::Point2F(strip.left, strip.top), D2D1::Point2F(strip.left, strip.bottom), hueStops, 7u);
    _brushContext    = dc;
    _brushHue        = _hsv.hue;
    _brushField      = field;
    _brushStrip      = strip;
}

void ColorPicker::Paint(ControlHost& host) const
{
    const ThemePalette& theme = host.GetTheme();
    auto* dc                  = host.GetDeviceContext();
    const D2D1_RECT_F field   = GetFieldRect();
    const D2D1_RECT_F strip   = GetHueStripRect();
    EnsureBrushes(host);
    if (dc)
    {
        if (_saturationBrush && _valueBrush)
        {
            dc->FillRectangle(field, _saturationBrush.get());
            dc->FillRectangle(field, _valueBrush.get());
        }
        else
        {
            dc->FillRectangle(field, host.GetSolidBrush(ColorFromArgb(_argb)));
        }
        if (_hueBrush)
        {
            dc->FillRectangle(strip, _hueBrush.get());
        }
    }
    StrokeRect(host, field, theme.border, 1.0f);
    StrokeRect(host, strip, theme.border, 1.0f);

    // Markers: a two-ring circle in the field, a two-line bar on the strip, both visible on any color.
    const D2D1_POINT_2F marker = D2D1::Point2F(field.left + _hsv.saturation * Width(field), field.top + (1.0f - _hsv.value) * Height(field));
    StrokeCircle(host, marker, kFieldMarkerRadius, D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);
    StrokeCircle(host, marker, kFieldMarkerRadius - 1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);
    const float hueY = strip.top + (_hsv.hue / 360.0f) * Height(strip);
    StrokeLine(host, D2D1::Point2F(strip.left - 2.0f, hueY), D2D1::Point2F(strip.right + 2.0f, hueY), D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), 3.0f);
    StrokeLine(host, D2D1::Point2F(strip.left - 2.0f, hueY), D2D1::Point2F(strip.right + 2.0f, hueY), D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), 1.0f);

    const D2D1_RECT_F fresh   = GetNewSwatchRect();
    const D2D1_RECT_F current = GetCurrentSwatchRect();
    DrawRoundedRect(host, fresh, ColorFromArgb(_argb), theme.border, 4.0f);
    DrawRoundedRect(host, current, ColorFromArgb(_current), theme.border, 4.0f);
    const D2D1_COLOR_F caption = IsEnabled() ? theme.subduedText : theme.disabledText;
    DrawCenteredText(host, _labels.newColor, D2D1::RectF(fresh.left, fresh.bottom, fresh.right, fresh.bottom + 16.0f), FontRole::Small, caption);
    DrawCenteredText(host, _labels.currentColor, D2D1::RectF(current.left, current.bottom, current.right, current.bottom + 16.0f), FontRole::Small, caption);
    if (_hex)
    {
        const D2D1_RECT_F hex   = _hex->GetBounds();
        const bool rtl          = IsRightToLeft();
        const D2D1_RECT_F label = rtl ? D2D1::RectF(hex.right + kGapDip, hex.top, hex.right + kGapDip + 26.0f, hex.bottom)
                                      : D2D1::RectF(hex.left - kGapDip - 26.0f, hex.top, hex.left - kGapDip, hex.bottom);
        DrawCenteredText(host,
                         _labels.hex,
                         label,
                         FontRole::Body,
                         IsEnabled() ? theme.text : theme.disabledText,
                         rtl ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    }
    if (HasFocus() && host.IsKeyboardFocusVisible())
    {
        PaintFocusRing(host, _drag == Drag::Hue ? strip : field, 2.0f);
    }
    Panel::Paint(host);
}

bool ColorPicker::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT /*modifiers*/)
{
    if (! IsEnabled() || rightButton)
    {
        return false;
    }
    if (PointInRect(GetFieldRect(), point))
    {
        _drag = Drag::Field;
    }
    else if (PointInRect(GetHueStripRect(), point))
    {
        _drag = Drag::Hue;
    }
    else
    {
        return false;
    }
    host.SetFocusControl(this);
    _dragStartArgb = _argb;
    host.CaptureMouse(this);
    UpdateFromPoint(host, point);
    return true;
}

bool ColorPicker::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    if (_drag == Drag::None)
    {
        return false;
    }
    UpdateFromPoint(host, point);
    return true;
}

bool ColorPicker::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    if (rightButton || _drag == Drag::None)
    {
        return false;
    }
    const auto life = GetLifetimeToken();
    OnMouseMove(host, point, modifiers);
    if (life.expired())
    {
        return true;
    }
    _drag = Drag::None;
    host.ReleaseMouseCapture();
    Invalidate(host);
    return true;
}

bool ColorPicker::OnKeyDown(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    if (! IsEnabled())
    {
        return false;
    }
    const float fine = 1.0f / 255.0f;
    const float step = ModifiersContainShift(modifiers) ? 10.0f * fine : fine;
    HsvColor hsv     = _hsv;
    switch (virtualKey)
    {
        case VK_ESCAPE:
            if (_drag != Drag::None)
            {
                OnCaptureLost(host);
                host.ReleaseMouseCapture();
            }
            else
            {
                Cancel(host);
            }
            return true;
        case VK_RETURN: Commit(host); return true;
        case VK_LEFT: hsv.saturation -= IsRightToLeft() ? -step : step; break;
        case VK_RIGHT: hsv.saturation += IsRightToLeft() ? -step : step; break;
        case VK_UP: hsv.value += step; break;
        case VK_DOWN: hsv.value -= step; break;
        case VK_PRIOR: hsv.hue -= ModifiersContainShift(modifiers) ? 10.0f : 1.0f; break;
        case VK_NEXT: hsv.hue += ModifiersContainShift(modifiers) ? 10.0f : 1.0f; break;
        default: return false;
    }
    ApplyHsv(&host, hsv, true);
    return true;
}

void ColorPicker::OnCaptureLost(ControlHost& host)
{
    if (_drag == Drag::None)
    {
        return;
    }
    _drag = Drag::None;
    Cancel(host);
}

void ColorPicker::OnBoundsChanged() noexcept
{
    Panel::OnBoundsChanged();
    Arrange();
}

void ColorPicker::OnEnabledChanged(bool enabled) noexcept
{
    Panel::OnEnabledChanged(enabled);
    for (Control* child : {static_cast<Control*>(_red),
                           static_cast<Control*>(_green),
                           static_cast<Control*>(_blue),
                           static_cast<Control*>(_hex),
                           static_cast<Control*>(_ok),
                           static_cast<Control*>(_cancel)})
    {
        if (child)
        {
            child->SetEnabled(enabled);
        }
    }
}

void ColorPicker::NotifyChange(ColorPickerChangePhase phase) noexcept
{
    try
    {
        const auto changed  = _onChange;
        const uint32_t argb = _argb;
        if (changed)
        {
            changed({phase, argb});
        }
    }
    catch (const std::exception&)
    {
        Debug::Warning(L"ColorPicker change callback failed");
    }
}
} // namespace DxUi
