#include <DxUi/Failure.h>

#include "DxUi.Internal.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <exception>
#include <limits>
#include <typeinfo>
#include <utility>

#include <UIAutomation.h>
#include <d2d1effects.h>

#include "Support/Diagnostics.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include "Support/Trace.h"

namespace DxUi
{
namespace
{
constexpr GUID kTransientSurfaceGaussianBlurEffectId = {0x1feb6d69, 0x2fe6, 0x4ac9, {0x8c, 0x58, 0x1d, 0x7f, 0x93, 0xe7, 0xa6, 0xa5}};

[[nodiscard]] wchar_t NormalizeMnemonicChar(wchar_t ch) noexcept
{
    return static_cast<wchar_t>(std::towupper(static_cast<wint_t>(ch)));
}

[[nodiscard]] bool ContinueModalLoopByDefault(void*) noexcept
{
    return true;
}
} // namespace

ModalLoopResult RunModalLoop(HWND hwnd, const ModalLoopOptions& options) noexcept
{
    const ModalLoopContinueCallback shouldContinue = options.shouldContinue ? options.shouldContinue : ContinueModalLoopByDefault;
    const std::wstring_view diagnosticName         = options.diagnosticName.empty() ? std::wstring_view(L"modal") : options.diagnosticName;

    MSG msg{};
    while (shouldContinue(options.context))
    {
        const BOOL getMessageResult = GetMessageW(&msg, nullptr, 0, 0);
        if (getMessageResult == -1)
        {
            const DWORD lastError = GetLastError();
            Debug::Warning(L"DxUi::RunModalLoop: GetMessageW failed for '{0}' (hwnd=0x{1:X}, lastError={2})",
                           diagnosticName,
                           reinterpret_cast<uintptr_t>(hwnd),
                           lastError);
            SetLastError(lastError);
            return ModalLoopResult::GetMessageFailed;
        }

        if (getMessageResult == 0)
        {
            if (options.onQuit)
            {
                options.onQuit(msg.wParam, options.context);
            }
            PostQuitMessage(static_cast<int>(msg.wParam));
            return ModalLoopResult::Quit;
        }

        if (! DispatchAttachedWindowHostKeyboardMessage(msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    return ModalLoopResult::Completed;
}

void TransientSurfaceBackdrop::SetCapture(WindowHostBitmapCapture value) noexcept
{
    capture = std::move(value);
    cachedBitmap.reset();
    cachedDevice.reset();
}

void TransientSurfaceBackdrop::Reset() noexcept
{
    capture = {};
    cachedBitmap.reset();
    cachedDevice.reset();
}

bool TransientSurfaceBackdrop::HasCapture() const noexcept
{
    const Failure::Scope outerFailureScope;
    const uint64_t expectedBytes = static_cast<uint64_t>(capture.widthPx) * static_cast<uint64_t>(capture.heightPx) * 4u;
    return capture.widthPx != 0u && capture.heightPx != 0u && expectedBytes == capture.bgraPixels.size();
}

bool CaptureTransientSurfaceBackdrop(const RECT& surfaceScreenRect, TransientSurfaceBackdrop& outBackdrop, std::wstring_view componentName) noexcept
{
    WindowHostBitmapCapture capture;
    if (! CaptureBackdropScreenRegion(surfaceScreenRect, capture, componentName))
    {
        outBackdrop.Reset();
        return false;
    }

    outBackdrop.SetCapture(std::move(capture));
    return true;
}

namespace
{
[[nodiscard]] ID2D1Bitmap1* EnsureTransientSurfaceBackdropBitmap(ControlHost& host, TransientSurfaceBackdrop& backdrop) noexcept
{
    if (! backdrop.HasCapture())
    {
        return nullptr;
    }

    ID2D1DeviceContext* const dc = host.GetDeviceContext();
    if (! dc)
    {
        return nullptr;
    }

    wil::com_ptr<ID2D1Device> device;
    dc->GetDevice(device.put());
    if (! device)
    {
        return nullptr;
    }

    if (backdrop.cachedBitmap && backdrop.cachedDevice && backdrop.cachedDevice.get() == device.get())
    {
        return backdrop.cachedBitmap.get();
    }

    const D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), host.GetDpi(), host.GetDpi());
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    const UINT32 pitch = backdrop.capture.widthPx * 4u;
    const HRESULT hr   = CountedCreateBitmap(
        dc, D2D1::SizeU(backdrop.capture.widthPx, backdrop.capture.heightPx), backdrop.capture.bgraPixels.data(), pitch, &bitmapProperties, bitmap.put());
    if (FAILED(hr) || ! bitmap)
    {
        Debug::Warning(L"DxUi::PaintTransientSurface: failed to create backdrop bitmap: 0x{:08X}", hr);
        return nullptr;
    }

    backdrop.cachedDevice = std::move(device);
    backdrop.cachedBitmap = std::move(bitmap);
    return backdrop.cachedBitmap.get();
}

void PaintTransientSurfaceBackdrop(ControlHost& host, const D2D1_RECT_F& surfaceRect, float cornerRadiusDip, TransientSurfaceBackdrop* backdrop) noexcept
{
    const ThemePalette& theme = host.GetTheme();
    const float opacity       = ResolveOverlayBackdropOpacity(theme);
    const float blurDip       = ResolveOverlayBackdropBlurDip(theme);
    if (theme.highContrast || ! backdrop || opacity <= 0.0f || blurDip <= 0.0f)
    {
        return;
    }

    ID2D1DeviceContext* const dc = host.GetDeviceContext();
    ID2D1Bitmap1* const bitmap   = EnsureTransientSurfaceBackdropBitmap(host, *backdrop);
    if (! dc || ! bitmap)
    {
        return;
    }

    wil::com_ptr<ID2D1Factory> factory;
    dc->GetFactory(factory.put());
    wil::com_ptr<ID2D1RoundedRectangleGeometry> roundedGeometry;
    if (! factory ||
        FAILED(CountedCreateRoundedRectangleGeometry(factory, D2D1::RoundedRect(surfaceRect, cornerRadiusDip, cornerRadiusDip), roundedGeometry.put())) ||
        ! roundedGeometry)
    {
        return;
    }

    wil::com_ptr<ID2D1Effect> blurEffect;
    if (FAILED(CountedCreateEffect(dc, kTransientSurfaceGaussianBlurEffectId, blurEffect.put())) || ! blurEffect)
    {
        return;
    }
    blurEffect->SetInput(0u, bitmap);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blurDip);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);

    const D2D1_RECT_F sourceRect = D2D1::RectF(
        0.0f, 0.0f, host.PixelsToDip(static_cast<float>(backdrop->capture.widthPx)), host.PixelsToDip(static_cast<float>(backdrop->capture.heightPx)));
    const D2D1_LAYER_PARAMETERS1 layerParameters = D2D1::LayerParameters1(surfaceRect,
                                                                          roundedGeometry.get(),
                                                                          D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                                          D2D1::Matrix3x2F::Identity(),
                                                                          std::clamp(opacity, 0.0f, 1.0f),
                                                                          nullptr,
                                                                          D2D1_LAYER_OPTIONS1_NONE);
    const D2D1_POINT_2F targetOffset             = D2D1::Point2F(surfaceRect.left, surfaceRect.top);
    dc->PushLayer(layerParameters, nullptr);
    dc->DrawImage(blurEffect.get(), &targetOffset, &sourceRect, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
    ++Detail::Work().popups.backdropBlurPasses;
    dc->PopLayer();
}
} // namespace

void PaintTransientSurface(ControlHost& host, const D2D1_RECT_F& surfaceRect, const TransientSurfaceOptions& options) noexcept
{
    ID2D1DeviceContext* const dc = host.GetDeviceContext();
    if (! dc)
    {
        return;
    }

    const ThemePalette& theme = host.GetTheme();
    const float radius        = theme.highContrast ? 0.0f : std::max(0.0f, options.cornerRadiusDip);
    if (options.drawShadow && ! theme.highContrast)
    {
        for (int ring = 3; ring >= 1; --ring)
        {
            const float spread = static_cast<float>(ring) * 2.0f;
            const float alpha  = 0.025f * static_cast<float>(4 - ring);
            const D2D1_RECT_F shadowRect =
                D2D1::RectF(surfaceRect.left - spread, surfaceRect.top - spread + 2.0f, surfaceRect.right + spread, surfaceRect.bottom + spread + 2.0f);
            if (ID2D1SolidColorBrush* const shadow = host.GetSolidBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha)))
            {
                dc->FillRoundedRectangle(D2D1::RoundedRect(shadowRect, radius + spread, radius + spread), shadow);
            }
        }
    }

    PaintTransientSurfaceBackdrop(host, surfaceRect, radius, options.backdrop);

    D2D1_COLOR_F fillColor = theme.overlayBackground;
    if (theme.highContrast)
    {
        fillColor.a = 1.0f;
    }
    else
    {
        switch (theme.overlayMaterial)
        {
            case OverlayMaterial::Solid: break;
            case OverlayMaterial::Mica: fillColor.a = std::min(fillColor.a, 0.86f); break;
            case OverlayMaterial::MicaAlt: fillColor.a = std::min(fillColor.a, 0.80f); break;
            case OverlayMaterial::Acrylic: fillColor.a = std::min(fillColor.a, 0.72f); break;
        }
    }
    if (ID2D1SolidColorBrush* const fill = host.GetSolidBrush(fillColor))
    {
        dc->FillRoundedRectangle(D2D1::RoundedRect(surfaceRect, radius, radius), fill);
    }
    D2D1_COLOR_F borderColor = theme.overlayBorder;
    if (theme.highContrast)
    {
        borderColor.a = 1.0f;
    }
    if (ID2D1SolidColorBrush* const border = host.GetSolidBrush(borderColor))
    {
        dc->DrawRoundedRectangle(D2D1::RoundedRect(surfaceRect, radius, radius), border, theme.highContrast ? 2.0f : 1.0f);
    }
    if (! theme.highContrast && surfaceRect.right - surfaceRect.left > 2.0f && surfaceRect.bottom - surfaceRect.top > 2.0f)
    {
        D2D1_COLOR_F innerRimColor = borderColor;
        innerRimColor.a *= 0.45f;
        if (ID2D1SolidColorBrush* const innerRim = host.GetSolidBrush(innerRimColor))
        {
            const D2D1_RECT_F innerRect = D2D1::RectF(surfaceRect.left + 1.0f, surfaceRect.top + 1.0f, surfaceRect.right - 1.0f, surfaceRect.bottom - 1.0f);
            dc->DrawRoundedRectangle(D2D1::RoundedRect(innerRect, std::max(0.0f, radius - 1.0f), std::max(0.0f, radius - 1.0f)), innerRim, 1.0f);
        }
    }
    if (options.pressed)
    {
        if (ID2D1SolidColorBrush* const pressed = host.GetSolidBrush(theme.pressedFill))
        {
            dc->FillRoundedRectangle(D2D1::RoundedRect(surfaceRect, radius, radius), pressed);
        }
    }
}

bool CaptureBackdropScreenRegion(const RECT& screenRect, WindowHostBitmapCapture& outCapture, std::wstring_view componentName) noexcept
{
    outCapture = {};

    const LONG widthPx  = screenRect.right - screenRect.left;
    const LONG heightPx = screenRect.bottom - screenRect.top;
    if (widthPx <= 0 || heightPx <= 0)
    {
        return false;
    }

    const uint64_t pixelCount = static_cast<uint64_t>(widthPx) * static_cast<uint64_t>(heightPx);
    if (pixelCount > static_cast<uint64_t>((std::numeric_limits<size_t>::max)() / 4u))
    {
        Debug::Warning(L"DxUi::{}: popup backdrop capture is too large (widthPx={} heightPx={})", componentName, widthPx, heightPx);
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = widthPx;
    bmi.bmiHeader.biHeight      = -heightPx;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    wil::unique_hdc_window screenDc{GetDC(nullptr)};
    if (! screenDc)
    {
        Debug::Warning(L"DxUi::{}: unable to acquire screen DC for popup backdrop capture", componentName);
        return false;
    }

    wil::unique_hdc memoryDc{CreateCompatibleDC(screenDc.get())};
    if (! memoryDc)
    {
        Debug::Warning(L"DxUi::{}: unable to create memory DC for popup backdrop capture", componentName);
        return false;
    }

    wil::unique_hbitmap bitmap{CreateDIBSection(screenDc.get(), &bmi, DIB_RGB_COLORS, &bits, nullptr, 0)};
    if (! bitmap || ! bits)
    {
        Debug::Warning(L"DxUi::{}: unable to create DIB section for popup backdrop capture", componentName);
        return false;
    }

    [[maybe_unused]] const auto oldBitmap = wil::SelectObject(memoryDc.get(), bitmap.get());
    if (CountedBitBlt(memoryDc.get(), 0, 0, widthPx, heightPx, screenDc.get(), screenRect.left, screenRect.top, SRCCOPY | CAPTUREBLT) == FALSE)
    {
        Debug::Warning(L"DxUi::{}: BitBlt failed for popup backdrop capture (lastError={})", componentName, GetLastError());
        return false;
    }

    outCapture.widthPx  = static_cast<UINT>(widthPx);
    outCapture.heightPx = static_cast<UINT>(heightPx);
    outCapture.bgraPixels.resize(static_cast<size_t>(pixelCount) * 4u);

    const auto* const sourceBytes = static_cast<const uint8_t*>(bits);
    std::copy_n(sourceBytes, outCapture.bgraPixels.size(), outCapture.bgraPixels.data());
    for (size_t offset = 3u; offset < outCapture.bgraPixels.size(); offset += 4u)
    {
        outCapture.bgraPixels[offset] = 0xFFu;
    }

    return true;
}

void ControlHost::BeginIntrinsicArrange(const Control& owner) noexcept
{
    if (! _intrinsicArrangeBranch)
        return;
    const Control* ancestor = &owner;
    while (ancestor && ancestor != _intrinsicArrangeBranch)
        ancestor = ancestor->_parent;
    // A newer arrangement may supersede the caller, but cannot make geometry outside its active subtree look accepted.
    if (! ancestor)
        ++_intrinsicArrangeForeignRevision;
}

void Control::SetBounds(const D2D1_RECT_F& bounds) noexcept
{
    if (_bounds.left != bounds.left || _bounds.top != bounds.top || _bounds.right != bounds.right || _bounds.bottom != bounds.bottom)
    {
        if (IsRenderStageActiveForDebug())
        {
            EmitRenderMutationBlockedForDebug();
            return;
        }

        uint64_t interactionBefore = 0u;
        if (_host)
        {
            interactionBefore = _host->_interactionRevision;
            ++_host->_interactionRevision;
            if (_host->_intrinsicArrangeBranch)
            {
                const Control* ancestor = this;
                while (ancestor && ancestor != _host->_intrinsicArrangeBranch)
                    ancestor = ancestor->_parent;
                if (! ancestor)
                    ++_host->_intrinsicArrangeForeignRevision;
                else
                    ++_host->_intrinsicArrangeBoundsRevision;
            }
        }
        const bool hadArea = _bounds.right > _bounds.left && _bounds.bottom > _bounds.top;
        // W4.10: what the control painted at its old bounds leaves the view. Its own extent only: a panel's children keep
        // theirs. The new extent joins through RequestInvalidate below, and the panels above it extend again.
        if (_host)
            PaintExtentAccess::AddToRegion(*_host, *this, false);
        _bounds = bounds;
        PaintExtentAccess::ExtentChanged(*this);
        // UI Automation publishes this control's bounding rectangles again, and its subtree's (W8.2): marked once the bounds
        // are stored, before any override runs.
        if (_host)
            NoteAccessibilityGeometryMove(*this, interactionBefore, _host->_hitRevision);
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        OnBoundsChanged();
        if (lifetime.expired())
            return;
        // A caret owner that gains or loses its area starts or stops blinking (W3.2): with no area it draws nothing.
        if (ControlHost* const host = GetHost(); host && hadArea != (bounds.right > bounds.left && bounds.bottom > bounds.top))
        {
            host->OnCaretOwnerAreaChanged(*this);
            if (lifetime.expired() || GetHost() != host)
                return;
        }
        if (ControlHost* const host = GetHost())
        {
            host->NotifyTextInputLayoutChanged(this);
            if (lifetime.expired() || GetHost() != host)
                return;
        }
        const AttestedInvalidation geometry(GetHost()); // the move is marked above
        RequestInvalidate();
    }
}

D2D1_RECT_F Control::GetBounds() const noexcept
{
    return _bounds;
}

D2D1_RECT_F Control::GetHitBounds() const noexcept
{
    return _bounds;
}

std::optional<D2D1_RECT_F> Control::TryGetTextInputViewportRect() const noexcept
{
    return GetTextInputViewportRect();
}

std::optional<D2D1_RECT_F> Control::TryGetTextInputCaretRect(const ControlHost& host, size_t controlTextIndex) const noexcept
{
    return GetTextInputCaretRect(host, controlTextIndex);
}

std::optional<std::vector<D2D1_RECT_F>> Control::TryGetTextInputRangeRects(const ControlHost& host,
                                                                           size_t controlTextStartIndex,
                                                                           size_t controlTextEndIndex) const
{
    return GetTextInputRangeRects(host, controlTextStartIndex, controlTextEndIndex);
}

std::optional<size_t> Control::TryHitTestTextInputPoint(const ControlHost& host, D2D1_POINT_2F point) const noexcept
{
    return HitTestTextInputPoint(host, point);
}

void Control::SetVisible(bool visible) noexcept
{
    const Failure::Scope outerFailureScope;
    if (_visible != visible)
    {
        if (_host)
        {
            ++_host->_interactionRevision;
            // A pending or shown supplemental tooltip re-resolves its target at the next wake (this one, or an ancestor of it,
            // may be it); the OnFrameScheduleChanged calls below tell the dispatcher.
            static_cast<void>(_host->MarkTooltipTargetStale());
        }
        _visible = visible;
        // W4.10: the panels above it extend over it, or no longer do.
        PaintExtentAccess::ExtentChanged(*this);
        if (_host)
        {
            _host->_pendingContentPreparation = true;
            _host->_pendingVisualPreparation  = true;
            // A shown plain container can contain layout owners outside its own ancestor chain.
            if (visible)
                _host->_pendingIntrinsicLayout = true;
        }
        if (_visualPreparationEnabled)
            _visualPreparationPending = true;
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        InvalidateMeasure();
        if (lifetime.expired())
            return;
        if (! visible)
        {
            OnHidden();
            if (lifetime.expired())
            {
                return;
            }
            if (ControlHost* const host = GetHost())
            {
                // A hidden control (and everything below it) keeps its frame-schedule request without being ticked or
                // waking anything; the request leaves the wake computation here.
                host->OnFrameScheduleChanged();
                if (lifetime.expired())
                    return;
                host->CancelPreparedVisualContent(this);
                if (lifetime.expired())
                    return;
            }
        }
        RequestInvalidate();
        if (lifetime.expired())
        {
            return;
        }
        if (ControlHost* const host = GetHost())
        {
            // Shown: the requests this control and its descendants held while hidden are ticked again from the next wake,
            // so a transition left part-way lands or resumes with no further input.
            if (visible)
            {
                host->OnFrameScheduleChanged();
                if (lifetime.expired() || GetHost() != host)
                    return;
            }
            RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
        }
    }
}

bool Control::IsVisible() const noexcept
{
    return _visible;
}

void Control::SetEnabled(bool enabled) noexcept
{
    const Failure::Scope outerFailureScope;
    if (_enabled != enabled)
    {
        bool tooltipTargetStale = false;
        if (_host)
        {
            ++_host->_interactionRevision;
            // A disabled control (or one below it) is no supplemental-tooltip target.
            tooltipTargetStale = _host->MarkTooltipTargetStale();
        }
        _enabled                          = enabled;
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        OnEnabledChanged(enabled);
        if (lifetime.expired())
        {
            return;
        }
        RequestInvalidate();
        if (lifetime.expired())
        {
            return;
        }
        if (ControlHost* const host = GetHost())
        {
            if (tooltipTargetStale)
            {
                host->OnTooltipScheduleChanged();
                if (lifetime.expired() || GetHost() != host)
                    return;
            }
            RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
        }
    }
}

bool Control::IsEnabled() const noexcept
{
    return _enabled;
}

void Control::SetFocusable(bool focusable) noexcept
{
    const Failure::Scope outerFailureScope;
    Assign(_focusable, focusable, Affects::SemanticState);
}

bool Control::IsFocusable() const noexcept
{
    return _focusable && _enabled && _visible;
}

bool Control::HasFocus() const noexcept
{
    return _hasFocus;
}

bool Control::IsHovered() const noexcept
{
    return _hovered;
}

void Control::SetTooltipText(std::wstring tooltipText)
{
    // Read when the pointer next rests on the control (OnMouseMove): nothing paints, measures, hit-tests or publishes it. A
    // pending or shown supplemental tooltip shows the new text, or none, from the next wake.
    Assign(_tooltipText,
           std::move(tooltipText),
           Affects::None,
           [this]() noexcept
    {
        if (_host && _host->MarkTooltipTargetStale())
            _host->OnTooltipScheduleChanged();
    });
}

std::wstring_view Control::GetTooltipText() const noexcept
{
    return _tooltipText;
}

void Control::PaintOverlay(ControlHost& /*host*/) const
{
}

bool Control::Tick(ControlHost& /*host*/, uint64_t /*nowTickMs*/)
{
    return false;
}

bool Control::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    const Failure::Scope entryFailureScope;
    if (! _tooltipText.empty() && PointInRect(GetHitBounds(), point))
    {
        static_cast<void>(host.SetTooltipDelayed(_tooltipText, point));
    }
    return false;
}

bool Control::OnMouseLeave(ControlHost& host)
{
    static_cast<void>(host.ClearTooltip());
    return false;
}

bool Control::OnMouseDown(ControlHost& /*host*/, D2D1_POINT_2F /*point*/, bool /*rightButton*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const Failure::Scope entryFailureScope;
    return OnMouseDown(host, point, rightButton, modifiers);
}

bool Control::OnMouseTripleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const Failure::Scope entryFailureScope;
    return OnMouseDown(host, point, rightButton, modifiers);
}

bool Control::OnMouseMiddleDown(ControlHost&, D2D1_POINT_2F, UINT)
{
    return false;
}

bool Control::OnMouseMiddleUp(ControlHost&, D2D1_POINT_2F, UINT)
{
    return false;
}

bool Control::OnMouseUp(ControlHost& /*host*/, D2D1_POINT_2F /*point*/, bool /*rightButton*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnMouseWheel(ControlHost& /*host*/, D2D1_POINT_2F /*point*/, float /*wheelDelta*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnKeyDown(ControlHost& /*host*/, UINT /*virtualKey*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnKeyUp(ControlHost& /*host*/, UINT /*virtualKey*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnChar(ControlHost& /*host*/, wchar_t /*ch*/, UINT /*modifiers*/)
{
    return false;
}

bool Control::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip)
{
    const Failure::Scope entryFailureScope;
    if (! IsEnabled() || ! IsVisible() || ! _onContextMenu)
    {
        return false;
    }

    const auto onContextMenu = _onContextMenu;
    (*onContextMenu)(ResolveContextMenuAnchor(host, keyboardInvocation, pointDip), keyboardInvocation);
    return true;
}

bool Control::OnCopy(ControlHost& /*host*/)
{
    return false;
}

bool Control::OnSelectAll(ControlHost& /*host*/)
{
    return false;
}

bool Control::OnMnemonic(ControlHost& host)
{
    if (! IsEnabled() || ! IsVisible())
    {
        return false;
    }

    if (IsFocusable())
    {
        static_cast<void>(FocusControlAndSurvive(host, *this, true));
        return true;
    }

    return false;
}

bool FocusControlAndSurvive(ControlHost& host, Control& control, bool takeNativeFocus) noexcept
{
    const std::weak_ptr<int> lifetime = GetControlLifetimeToken(control);
    if (takeNativeFocus)
    {
        if (const HWND hwnd = host.GetHwnd())
        {
            SetFocus(hwnd);
            if (lifetime.expired())
                return false;
        }
    }
    host.SetFocusControl(&control);
    return ! lifetime.expired();
}

size_t Control::GetLogicalChildCount() const noexcept
{
    return 0u;
}

Control* Control::GetLogicalChild(size_t /*index*/) noexcept
{
    return nullptr;
}

const Control* Control::GetLogicalChild(size_t /*index*/) const noexcept
{
    return nullptr;
}

ControlHost* Control::GetHost() const noexcept
{
    return _host;
}

void Control::SetMnemonic(wchar_t mnemonic) noexcept
{
    // A plain Label is not prepared: a mnemonic makes it one that is, and clearing it makes it plain again (Label::Label).
    Assign(_mnemonic,
           NormalizeMnemonicChar(mnemonic),
           Affects::Paint,
           [this]() noexcept
    {
        RefreshLabelVisualPreparation();
        RequestVisualContentPreparation(false);
    });
}

wchar_t Control::GetMnemonic() const noexcept
{
    return _mnemonic;
}

void Control::SetFlowDirection(FlowDirection direction) noexcept
{
    if (_explicitFlowDirection.has_value() && _explicitFlowDirection.value() == direction)
    {
        return;
    }

    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _explicitFlowDirection        = direction;
    OnFlowDirectionChanged();
}

void Control::ClearFlowDirection() noexcept
{
    if (! _explicitFlowDirection.has_value())
    {
        return;
    }

    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _explicitFlowDirection.reset();
    OnFlowDirectionChanged();
}

bool Control::HasExplicitFlowDirection() const noexcept
{
    return _explicitFlowDirection.has_value();
}

FlowDirection Control::GetFlowDirection() const noexcept
{
    const Failure::Scope outerFailureScope;
    if (_explicitFlowDirection.has_value())
    {
        return _explicitFlowDirection.value();
    }

    return _parent ? _parent->GetFlowDirection() : _detachedFlowDirection.value_or(FlowDirection::LeftToRight);
}

bool Control::IsRightToLeft() const noexcept
{
    return GetFlowDirection() == FlowDirection::RightToLeft;
}

void Control::SetDensity(Density density) noexcept
{
    if (_explicitDensity.has_value() && _explicitDensity.value() == density)
    {
        return;
    }

    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _explicitDensity              = density;
    OnDensityChanged();
}

void Control::ClearDensity() noexcept
{
    if (! _explicitDensity.has_value())
    {
        return;
    }

    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _explicitDensity.reset();
    OnDensityChanged();
}

bool Control::HasExplicitDensity() const noexcept
{
    return _explicitDensity.has_value();
}

Density Control::GetDensity() const noexcept
{
    const Failure::Scope outerFailureScope;
    if (_explicitDensity.has_value())
    {
        return _explicitDensity.value();
    }

    if (_parent)
    {
        return _parent->GetDensity();
    }

    return _host ? _host->GetTheme().density : _detachedDensity.value_or(Density::Standard);
}

bool Control::IsCompactDensity() const noexcept
{
    return GetDensity() == Density::Compact;
}

void Control::SetConnectedAnimationKey(std::wstring key)
{
    // Read when a PageHost starts a transition, never painted.
    Assign(_connectedAnimationKey, std::move(key), Affects::None);
}

std::wstring_view Control::GetConnectedAnimationKey() const noexcept
{
    return _connectedAnimationKey;
}

void Control::SetAccessibleName(std::wstring name)
{
    const Failure::Scope outerFailureScope;
    Assign(_accessibleName, std::move(name), Affects::SemanticName);
}

std::wstring_view Control::GetAccessibleName() const noexcept
{
    return _accessibleName;
}

void Control::SetAccessibleHelpText(std::wstring helpText)
{
    const Failure::Scope outerFailureScope;
    Assign(_accessibleHelpText, std::move(helpText), Affects::SemanticName);
}

std::wstring_view Control::GetAccessibleHelpText() const noexcept
{
    return _accessibleHelpText;
}

HRESULT Control::SetAccessibleFormProperties(Control* label, bool required, bool dataValid, std::wstring itemStatus) noexcept
{
    const Failure::Scope outerFailureScope;
    if (label == this)
        return E_INVALIDARG;
    if (GetAccessibleLabelledBy() == label && IsAccessibleRequiredForForm() == required && IsAccessibleDataValidForForm() == dataValid &&
        GetAccessibleItemStatus() == itemStatus)
        return S_FALSE;
    try
    {
        std::unique_ptr<AccessibleFormProperties> properties;
        if (label || required || ! dataValid || ! itemStatus.empty())
        {
            properties        = std::make_unique<AccessibleFormProperties>();
            properties->label = label;
            if (label)
                properties->labelLifetime = GetControlLifetimeToken(*label);
            properties->required   = required;
            properties->dataValid  = dataValid;
            properties->itemStatus = std::move(itemStatus);
        }
        _accessibleFormProperties.swap(properties);
        // Presentation metadata UI Automation reads (labelled-by, required, valid, item status); nothing paints it. Routed as
        // Control::Assign routes a semantic change, so an embedded view still prepares and republishes.
        RouteAffects(Affects::SemanticState);
        return S_OK;
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
}

Control* Control::GetAccessibleLabelledBy() const noexcept
{
    return _accessibleFormProperties && ! _accessibleFormProperties->labelLifetime.expired() ? _accessibleFormProperties->label : nullptr;
}

bool Control::IsAccessibleRequiredForForm() const noexcept
{
    return _accessibleFormProperties && _accessibleFormProperties->required;
}

bool Control::IsAccessibleDataValidForForm() const noexcept
{
    return ! _accessibleFormProperties || _accessibleFormProperties->dataValid;
}

std::wstring_view Control::GetAccessibleItemStatus() const noexcept
{
    return _accessibleFormProperties ? std::wstring_view(_accessibleFormProperties->itemStatus) : std::wstring_view{};
}

void Control::SetAccessibleAutomationId(std::wstring automationId)
{
    const Failure::Scope outerFailureScope;
    Assign(_accessibleAutomationId, std::move(automationId), Affects::SemanticName);
}

std::wstring_view Control::GetAccessibleAutomationId() const noexcept
{
    return _accessibleAutomationId;
}

void Control::SetAccessibilityRole(AccessibilityRole role) noexcept
{
    const Failure::Scope outerFailureScope;
    // The control type and patterns UI Automation reports: a state, never painted.
    Assign(_accessibilityRole, role, Affects::SemanticState);
}

AccessibilityRole Control::GetAccessibilityRole() const noexcept
{
    return _accessibilityRole;
}

void Control::SetAccessibleInvoke(std::function<void(ControlHost&)> onInvoke)
{
    if (! onInvoke)
    {
        SetAccessibleInvokeResult({});
        return;
    }
    SetAccessibleInvokeResult([onInvoke = std::move(onInvoke)](ControlHost& host)
    {
        onInvoke(host);
        return S_OK;
    });
}

void Control::SetAccessibleInvokeResult(std::function<HRESULT(ControlHost&)> onInvoke)
{
    const Failure::Scope outerFailureScope;
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    ReplaceControlCallback(_onAccessibleInvoke, std::move(onInvoke));
    if (lifetime.expired())
    {
        return;
    }
    if (ControlHost* const host = GetHost())
    {
        RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
    }
}

bool Control::SupportsAccessibleInvoke() const noexcept
{
    return static_cast<bool>(_onAccessibleInvoke);
}

bool Control::InvokeAccessible(ControlHost& host)
{
    return SUCCEEDED(InvokeAccessibleResult(host));
}

HRESULT Control::InvokeAccessibleResult(ControlHost& host) noexcept
try
{
    const Failure::Scope outerFailureScope;
    if (! _onAccessibleInvoke || ! IsVisible())
        return UIA_E_NOTSUPPORTED;
    if (! IsEnabled())
        return UIA_E_ELEMENTNOTENABLED;
    const auto onAccessibleInvoke = _onAccessibleInvoke;
    return (*onAccessibleInvoke)(host);
}
catch (const std::bad_alloc&)
{
    Failure::FailAllocation();
}
catch (const std::exception&)
{
    Failure::FailUnexpected();
}

void Control::SetOnContextMenu(std::function<void(POINT screenPoint, bool keyboardInvocation)> onContextMenu)
{
    ReplaceControlCallback(_onContextMenu, std::move(onContextMenu));
}

void Control::InvalidateHitTest() noexcept
{
    if (_host)
    {
        // What a hit test reads of this control moved (a popup, a scroll offset, a separator, a declared Affects::Hit), and
        // the bounding rectangles UI Automation publishes read the same geometry: the host's geometry path marks it (W8.2).
        const uint64_t hitBefore         = _host->_hitRevision;
        const uint64_t interactionBefore = _host->_interactionRevision;
        _host->AdvanceHitRevision();
        NoteAccessibilityGeometryMove(*this, interactionBefore, hitBefore);
    }
}

void Control::RouteAffects(Affects affects) noexcept
{
    RouteAffectsAt(affects, static_cast<uint8_t>(HitRevisionSite::Assign));
}

// The routing of a changed assignment (UI_ControlsAndLayout.md, "Assigning a property"). Each step can run the application's
// code (an embedded invalidation callback, an accessibility event) that destroys this control, so each later step first
// checks that it is still alive.
void Control::RouteAffectsAt(Affects affects, uint8_t hitRevisionSite) noexcept
{
    if (affects == Affects::None)
        return;
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    // What UI Automation publishes of this assignment (W8.2): a name or a value marks this control's record; a hit change, and a
    // painted change that declared no semantic aspect (a color, a scroll extent: drawn geometry a bounding rectangle may follow),
    // mark its geometry; a state is a whole-window mutation (patterns, control type, focus). The invalidations below declare
    // what they repaint, so they do not by themselves make the next publication whole.
    const bool semantic = HasAffects(affects, Affects::SemanticName | Affects::SemanticValue | Affects::SemanticState);
    if (HasAffects(affects, Affects::Hit))
        NoteHitTestChange(*this, static_cast<HitRevisionSite>(hitRevisionSite)); // marks its geometry (InvalidateHitTest)
    else if (HasAffects(affects, Affects::Paint) && ! semantic)
        NoteAccessibilityRecordChange(*this, AccessibilityRecordAspect::Geometry);
    if (HasAffects(affects, Affects::Layout))
    {
        const AttestedInvalidation declared(GetHost());
        InvalidateMeasure();
        if (lifetime.expired())
            return;
    }
    if (HasAffects(affects, Affects::Paint))
    {
        const AttestedInvalidation declared(GetHost());
        RequestInvalidate();
        if (lifetime.expired())
            return;
    }
    // A state can change patterns and focus (General); a name or value cannot, and marks its record. An embedded view publishes
    // at the application's UpdateAccessibility after a preparation, so a change it has not repainted asks for one.
    if (semantic)
    {
        if (ControlHost* const host = GetHost())
        {
            if (host->IsEmbedded() && ! HasAffects(affects, Affects::Paint))
            {
                const AttestedInvalidation declared(host);
                RequestInvalidate();
                if (lifetime.expired() || GetHost() != host)
                    return;
            }
            if (HasAffects(affects, Affects::SemanticState))
                RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host, AccessibilityMutationKind::General);
            else
            {
                if (HasAffects(affects, Affects::SemanticName))
                    NoteAccessibilityRecordChange(*this, AccessibilityRecordAspect::Name);
                if (HasAffects(affects, Affects::SemanticValue))
                    NoteAccessibilityRecordChange(*this, AccessibilityRecordAspect::Value);
            }
        }
    }
}

namespace
{
// Classes outside the library that declared their hit test reported, each by the exact type whose constructor declared it.
// Bounded and allocation-free: a declaration that does not fit leaves its class unreported, which only turns reuse off.
constexpr size_t kMaxDeclaredHitTypes = 256u;
std::array<const std::type_info*, kMaxDeclaredHitTypes> g_declaredHitTypes{};
size_t g_declaredHitTypeCount  = 0u;
SRWLOCK g_declaredHitTypesLock = SRWLOCK_INIT;

[[nodiscard]] bool IsDeclaredHitType(const std::type_info& type) noexcept
{
    AcquireSRWLockShared(&g_declaredHitTypesLock);
    const auto release = wil::scope_exit([]() noexcept { ReleaseSRWLockShared(&g_declaredHitTypesLock); });
    for (size_t index = 0u; index < g_declaredHitTypeCount; ++index)
        if (*g_declaredHitTypes[index] == type)
            return true;
    return false;
}
} // namespace

void Control::DeclareHitTestReported() noexcept
{
    // In a constructor typeid(*this) is the class being constructed, so the declaration covers exactly that class.
    const std::type_info& type = typeid(*this);
    if (! IsDeclaredHitType(type))
    {
        AcquireSRWLockExclusive(&g_declaredHitTypesLock);
        const auto release = wil::scope_exit([]() noexcept { ReleaseSRWLockExclusive(&g_declaredHitTypesLock); });
        bool present       = false;
        for (size_t index = 0u; index < g_declaredHitTypeCount && ! present; ++index)
            present = *g_declaredHitTypes[index] == type;
        if (! present && g_declaredHitTypeCount < g_declaredHitTypes.size())
            g_declaredHitTypes[g_declaredHitTypeCount++] = &type;
    }
    // Declared while attached (not from a constructor): the host stops counting it once its exact type is declared.
    if (_host && (_hitTestReporting & kHitTestCounted) != 0u && IsHitTestReported())
    {
        --_host->_unreportedHitControls;
        _hitTestReporting = static_cast<uint8_t>(_hitTestReporting & ~kHitTestCounted);
    }
}

// The library's own control classes report every change their hit testing reads: bounds, visibility, enabled state and
// tree membership move the interaction revision, anything else calls InvalidateHitTest (UI_InputAndAccessibility.md,
// "Pointer hit result"). Only the exact class counts: a subclass defined elsewhere may override hit testing with state the
// host cannot see, so it is reported only when its own constructor declares so. A new library control class is added here.
bool Control::IsHitTestReported() const noexcept
{
    static const std::array<const std::type_info*, 35u> kLibraryTypes{
        &typeid(Panel),       &typeid(PageHost),      &typeid(CardPanel),       &typeid(Label),          &typeid(Separator),
        &typeid(Button),      &typeid(Toggle),        &typeid(Checkbox),        &typeid(RadioButton),    &typeid(RadioButtons),
        &typeid(ProgressBar), &typeid(PageIndicator), &typeid(ThroughputGraph), &typeid(Slider),         &typeid(Toolbar),
        &typeid(MenuBar),     &typeid(TabControl),    &typeid(ColorSwatch),     &typeid(RichTextEditor), &typeid(TextField),
        &typeid(ComboBox),    &typeid(TagPicker),     &typeid(StatusStrip),     &typeid(PopupLayer),     &typeid(StackPanel),
        &typeid(FormField),   &typeid(FormPanel),     &typeid(FlexPanel),       &typeid(ScrollPanel),    &typeid(TooltipLayer),
        &typeid(Tree),        &typeid(Grid),          &typeid(Splitter),        &typeid(NumericStepper), &typeid(ColorPicker),
    };
    const std::type_info& type = typeid(*this);
    return std::ranges::any_of(kLibraryTypes, [&type](const std::type_info* library) noexcept { return *library == type; }) || IsDeclaredHitType(type);
}

namespace
{
// Classes outside the library that declared their painted extent (DeclarePaintOverhang), each by the exact type whose
// constructor declared it; bounded as the hit-test declarations: one that does not fit leaves its class undeclared.
std::array<const std::type_info*, kMaxDeclaredHitTypes> g_declaredPaintTypes{};
size_t g_declaredPaintTypeCount  = 0u;
SRWLOCK g_declaredPaintTypesLock = SRWLOCK_INIT;
bool g_paintRegionsEnabled       = true;

[[nodiscard]] bool IsDeclaredPaintType(const std::type_info& type) noexcept
{
    AcquireSRWLockShared(&g_declaredPaintTypesLock);
    const auto release = wil::scope_exit([]() noexcept { ReleaseSRWLockShared(&g_declaredPaintTypesLock); });
    for (size_t index = 0u; index < g_declaredPaintTypeCount; ++index)
        if (*g_declaredPaintTypes[index] == type)
            return true;
    return false;
}

// The library's own classes, whose painted extent the library declares (the hit-test list: the same classes).
[[nodiscard]] bool IsLibraryControlType(const std::type_info& type) noexcept
{
    static const std::array<const std::type_info*, 35u> kLibraryTypes{
        &typeid(Panel),       &typeid(PageHost),      &typeid(CardPanel),       &typeid(Label),          &typeid(Separator),
        &typeid(Button),      &typeid(Toggle),        &typeid(Checkbox),        &typeid(RadioButton),    &typeid(RadioButtons),
        &typeid(ProgressBar), &typeid(PageIndicator), &typeid(ThroughputGraph), &typeid(Slider),         &typeid(Toolbar),
        &typeid(MenuBar),     &typeid(TabControl),    &typeid(ColorSwatch),     &typeid(RichTextEditor), &typeid(TextField),
        &typeid(ComboBox),    &typeid(TagPicker),     &typeid(StatusStrip),     &typeid(PopupLayer),     &typeid(StackPanel),
        &typeid(FormField),   &typeid(FormPanel),     &typeid(FlexPanel),       &typeid(ScrollPanel),    &typeid(TooltipLayer),
        &typeid(Tree),        &typeid(Grid),          &typeid(Splitter),        &typeid(NumericStepper), &typeid(ColorPicker),
    };
    return std::ranges::any_of(kLibraryTypes, [&type](const std::type_info* library) noexcept { return *library == type; });
}

[[nodiscard]] bool IsUnboundedOverhang(const PaintOverhang& overhang) noexcept
{
    return ! std::isfinite(overhang.left) || ! std::isfinite(overhang.top) || ! std::isfinite(overhang.right) || ! std::isfinite(overhang.bottom);
}

[[nodiscard]] D2D1_RECT_F UnionRect(const D2D1_RECT_F& a, const D2D1_RECT_F& b) noexcept
{
    return D2D1::RectF((std::min)(a.left, b.left), (std::min)(a.top, b.top), (std::max)(a.right, b.right), (std::max)(a.bottom, b.bottom));
}

[[nodiscard]] bool IsEmptyExtent(const D2D1_RECT_F& rect) noexcept
{
    return ! (rect.right > rect.left) || ! (rect.bottom > rect.top);
}

// The axis-aligned box of `rect` under `transform`.
[[nodiscard]] D2D1_RECT_F TransformBox(const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept
{
    if (transform._12 == 0.0f && transform._21 == 0.0f)
    {
        const float x0 = rect.left * transform._11 + transform._31;
        const float x1 = rect.right * transform._11 + transform._31;
        const float y0 = rect.top * transform._22 + transform._32;
        const float y1 = rect.bottom * transform._22 + transform._32;
        return D2D1::RectF((std::min)(x0, x1), (std::min)(y0, y1), (std::max)(x0, x1), (std::max)(y0, y1));
    }
    const std::array<D2D1_POINT_2F, 4u> corners{
        D2D1::Point2F(rect.left, rect.top), D2D1::Point2F(rect.right, rect.top), D2D1::Point2F(rect.left, rect.bottom), D2D1::Point2F(rect.right, rect.bottom)};
    D2D1_RECT_F box = D2D1::RectF(std::numeric_limits<float>::infinity(),
                                  std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity());
    for (const D2D1_POINT_2F& corner : corners)
    {
        const float x = corner.x * transform._11 + corner.y * transform._21 + transform._31;
        const float y = corner.x * transform._12 + corner.y * transform._22 + transform._32;
        box           = D2D1::RectF((std::min)(box.left, x), (std::min)(box.top, y), (std::max)(box.right, x), (std::max)(box.bottom, y));
    }
    return box;
}
} // namespace

PaintOverhang Control::GetPaintOverhang(const ControlHost& /*host*/) const noexcept
{
    switch (PaintExtentAccess::Kind(*this))
    {
        case PaintKind::Unresolved:
        case PaintKind::Plain: return {};
        case PaintKind::PageHost:
        {
            // A page transition draws its pages translated and faded: the whole view repaints while it runs.
            const auto* page = dynamic_cast<const PageHost*>(this);
            if (page && page->HasActiveTransition())
                return {std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f};
            return {kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip};
        }
        case PaintKind::ComboBox:
        {
            // The open popup paints in the overlay pass, with its shadow and backdrop, outside the box: unbounded while open.
            const auto* combo = dynamic_cast<const ComboBox*>(this);
            if (combo && combo->IsPopupOpen())
                return {std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f};
            return {kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip};
        }
        case PaintKind::Library:
        case PaintKind::Panel:
        case PaintKind::ScrollPanel:
        case PaintKind::PopupLayer: break;
    }
    return {kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip, kLibraryPaintReachDip};
}

void Control::DeclarePaintOverhang() noexcept
{
    // In a constructor typeid(*this) is the class being constructed, so the declaration covers exactly that class.
    const std::type_info& type = typeid(*this);
    if (! IsDeclaredPaintType(type))
    {
        AcquireSRWLockExclusive(&g_declaredPaintTypesLock);
        const auto release = wil::scope_exit([]() noexcept { ReleaseSRWLockExclusive(&g_declaredPaintTypesLock); });
        bool present       = false;
        for (size_t index = 0u; index < g_declaredPaintTypeCount && ! present; ++index)
            present = *g_declaredPaintTypes[index] == type;
        if (! present && g_declaredPaintTypeCount < g_declaredPaintTypes.size())
            g_declaredPaintTypes[g_declaredPaintTypeCount++] = &type;
    }
    // Declared while attached (not from a constructor): resolved again, and every cached extent of the host is stale.
    if (_host)
    {
        PaintExtentAccess::Resolve(*this);
        ++_host->_paintTopologyRevision;
    }
}

namespace Detail
{
bool PaintRegionsEnabled() noexcept
{
    return g_paintRegionsEnabled;
}
void SetPaintRegionsEnabledForTest(bool enabled) noexcept
{
    g_paintRegionsEnabled = enabled;
}
namespace
{
PaintObserverForTest g_paintObserver = nullptr;
void* g_paintObserverContext         = nullptr;
} // namespace
void SetPaintObserverForTest(PaintObserverForTest observer, void* context) noexcept
{
    g_paintObserver        = observer;
    g_paintObserverContext = context;
}
PaintObserverForTest GetPaintObserverForTest(void*& context) noexcept
{
    context = g_paintObserverContext;
    return g_paintObserver;
}
namespace
{
void (*g_fullRegionObserver)() noexcept = nullptr;
} // namespace
void SetFullRegionObserverForTest(void (*observer)() noexcept) noexcept
{
    g_fullRegionObserver = observer;
}
void NoteFullRegionForTest() noexcept
{
    if (g_fullRegionObserver) [[unlikely]]
        g_fullRegionObserver();
}
} // namespace Detail

void PaintExtentAccess::Resolve(Control& control) noexcept
{
    PaintKind kind = PaintKind::Plain;
    if (dynamic_cast<const PageHost*>(&control))
        kind = PaintKind::PageHost;
    else if (dynamic_cast<const ScrollPanel*>(&control))
        kind = PaintKind::ScrollPanel;
    else if (dynamic_cast<const PopupLayer*>(&control))
        kind = PaintKind::PopupLayer;
    else if (dynamic_cast<const Panel*>(&control))
        kind = PaintKind::Panel;
    else if (dynamic_cast<const ComboBox*>(&control))
        kind = PaintKind::ComboBox;
    else if (dynamic_cast<const Label*>(&control) || dynamic_cast<const Separator*>(&control) || dynamic_cast<const Button*>(&control) ||
             dynamic_cast<const ProgressBar*>(&control) || dynamic_cast<const PageIndicator*>(&control) || dynamic_cast<const ThroughputGraph*>(&control) ||
             dynamic_cast<const Slider*>(&control) || dynamic_cast<const MenuBar*>(&control) || dynamic_cast<const ColorSwatch*>(&control) ||
             dynamic_cast<const RichTextEditor*>(&control) || dynamic_cast<const TextField*>(&control) || dynamic_cast<const StatusStrip*>(&control) ||
             dynamic_cast<const TooltipLayer*>(&control) || dynamic_cast<const Tree*>(&control) || dynamic_cast<const Grid*>(&control) ||
             dynamic_cast<const Splitter*>(&control))
        kind = PaintKind::Library;
    control._paintKind         = static_cast<uint8_t>(kind);
    const std::type_info& type = typeid(control);
    const bool declared        = IsLibraryControlType(type) || IsDeclaredPaintType(type);
    // Only the library classes themselves: a subclass may draw text of its own outside the clip.
    const bool clipsText = type == typeid(Tree) || type == typeid(Grid) || type == typeid(TextField) || type == typeid(ComboBox) ||
                           type == typeid(RichTextEditor) || type == typeid(TabControl);
    control._paintFlags = static_cast<uint8_t>((control._paintFlags & ~(kDeclared | kClipsText)) | (declared ? kDeclared : 0u) | (clipsText ? kClipsText : 0u));
}

bool PaintExtentAccess::OwnExtent(const ControlHost& host, const Control& control, D2D1_RECT_F& extent) noexcept
{
    if (! IsDeclared(control) || (control._paintFlags & kInkEscaped) != 0u || Kind(control) == PaintKind::Unresolved)
        return false;
    const PaintOverhang overhang = control.GetPaintOverhang(host);
    if (IsUnboundedOverhang(overhang))
        return false;
    const D2D1_RECT_F bounds = control._bounds;
    extent                   = D2D1::RectF(bounds.left - (std::max)(0.0f, overhang.left),
                                           bounds.top - (std::max)(0.0f, overhang.top),
                                           bounds.right + (std::max)(0.0f, overhang.right),
                                           bounds.bottom + (std::max)(0.0f, overhang.bottom));
    return true;
}

bool PaintExtentAccess::Extent(const ControlHost& host, const Control& control, D2D1_RECT_F& extent) noexcept
{
    const PaintKind kind = Kind(control);
    if (kind == PaintKind::PageHost)
        return false; // its pages are not panel children: never culled, its invalidation repaints the view
    if (kind != PaintKind::Panel && kind != PaintKind::ScrollPanel && kind != PaintKind::PopupLayer)
        return OwnExtent(host, control, extent);
    const auto& panel = static_cast<const Panel&>(control);
    auto& cached      = panel._paintExtent;
    if (cached.valid && cached.topology == host._paintTopologyRevision)
    {
        extent = cached.rect;
        return ! cached.unbounded;
    }
    D2D1_RECT_F rect = D2D1::RectF();
    bool unbounded   = ! OwnExtent(host, control, rect);
    for (const std::unique_ptr<Control>& child : panel._children)
    {
        if (unbounded)
            break;
        if (! child || ! child->_visible)
            continue;
        D2D1_RECT_F childExtent{};
        if (! Extent(host, *child, childExtent))
            unbounded = true;
        // A ScrollPanel clips its content to its own bounds; its children only decide whether it is bounded.
        else if (kind != PaintKind::ScrollPanel && ! IsEmptyExtent(childExtent))
            rect = UnionRect(rect, childExtent);
    }
    cached.rect      = rect;
    cached.unbounded = unbounded;
    cached.topology  = host._paintTopologyRevision;
    cached.valid     = true;
    extent           = rect;
    return ! unbounded;
}

bool PaintExtentAccess::ViewExtent(const ControlHost& host, const Control& control, bool subtree, D2D1_RECT_F& extent) noexcept
{
    if (! Detail::PaintRegionsEnabled() || control._host != &host)
        return false;
    D2D1_RECT_F rect{};
    if (! (subtree ? Extent(host, control, rect) : OwnExtent(host, control, rect)))
        return false;
    // Overlay content (a PopupLayer's children) is not clipped by the viewports it is drawn through.
    bool overlay = Kind(control) == PaintKind::PopupLayer;
    for (const Control* ancestor = control._parent; ancestor; ancestor = ancestor->_parent)
    {
        if (! IsDeclared(*ancestor))
            return false;
        switch (Kind(*ancestor))
        {
            case PaintKind::PageHost:
            {
                const auto* page = dynamic_cast<const PageHost*>(ancestor);
                if (! page || page->HasActiveTransition())
                    return false;
                break;
            }
            case PaintKind::ScrollPanel:
            {
                const auto* scroll = static_cast<const ScrollPanel*>(ancestor);
                const float offset = scroll->GetScrollOffset();
                rect.top -= offset;
                rect.bottom -= offset;
                if (! overlay)
                {
                    const D2D1_RECT_F viewport = scroll->GetViewportRect();
                    rect                       = D2D1::RectF((std::max)(rect.left, viewport.left),
                                                             (std::max)(rect.top, viewport.top),
                                                             (std::min)(rect.right, viewport.right),
                                                             (std::min)(rect.bottom, viewport.bottom));
                }
                break;
            }
            case PaintKind::PopupLayer: overlay = true; break;
            case PaintKind::Unresolved: return false;
            case PaintKind::Plain:
            case PaintKind::Library:
            case PaintKind::Panel:
            case PaintKind::ComboBox: break;
        }
    }
    // A control in another tree than the host's root (a detached branch) is never drawn by it.
    const D2D1_RECT_F view = host.GetClientBoundsDip();
    extent =
        D2D1::RectF((std::max)(rect.left, view.left), (std::max)(rect.top, view.top), (std::min)(rect.right, view.right), (std::min)(rect.bottom, view.bottom));
    return true;
}

void PaintExtentAccess::ExtentChanged(const Control& control) noexcept
{
    // The control's own cached extent (a panel's) and every one above it, up to the first already stale: a stale extent's
    // ancestors are stale too, because computing an extent computes the stale extents below it first.
    const ControlHost* const host = control._host;
    if (! host)
        return;
    for (const Control* node = &control; node; node = node->_parent)
    {
        const PaintKind kind = Kind(*node);
        if (kind != PaintKind::Panel && kind != PaintKind::ScrollPanel && kind != PaintKind::PopupLayer)
            continue;
        auto& cached = static_cast<const Panel*>(node)->_paintExtent;
        if (! cached.valid || cached.topology != host->_paintTopologyRevision)
        {
            if (node == &control)
                continue; // the control's own may be stale while its ancestors are not
            return;
        }
        cached.valid = false;
    }
}

void PaintExtentAccess::AddToRegion(ControlHost& host, const Control& control, bool subtree) noexcept
{
    if (host._paintRegion.full)
        return;
    D2D1_RECT_F extent{};
    if (! ViewExtent(host, control, subtree, extent))
    {
        MarkFull(host);
        return;
    }
    if (! IsEmptyExtent(extent))
        AddDip(host, extent);
}

void PaintExtentAccess::Invalidate(ControlHost& host, const Control& control) noexcept
{
    // A control outside the library that declared its extent may change its reach with any invalidation.
    if (IsDeclared(control) && ! IsLibraryControlType(typeid(control)))
        ExtentChanged(control);
    AddToRegion(host, control, true);
    host.RequestRegionFrame();
}

namespace Detail
{
void NoteFullRegionForTest() noexcept;
}

void PaintExtentAccess::MarkFull(const ControlHost& host) noexcept
{
    if (! host._paintRegion.full)
        Detail::NoteFullRegionForTest();
    host._paintRegion.full  = true;
    host._paintRegion.count = 0u;
}

void PaintExtentAccess::AddDip(ControlHost& host, const D2D1_RECT_F& rectDip) noexcept
{
    ControlHost::PaintRegion& region = host._paintRegion;
    if (region.full)
        return;
    const UINT width  = host._widthPx;
    const UINT height = host._heightPx;
    if (width == 0u || height == 0u)
    {
        MarkFull(host);
        return;
    }
    // Snapped outward to device pixels: every pixel the extent touches, partly or wholly. Antialiased coverage never reaches
    // beyond the pixels a shape's geometry touches, and a text extent is its glyphs' ink (overhang metrics).
    const float scale  = static_cast<float>(host.GetDpi()) / 96.0f;
    const auto clampPx = [](float value, UINT limit) noexcept { return static_cast<LONG>(std::clamp(value, 0.0f, static_cast<float>(limit))); };
    RECT rect{clampPx(std::floor(rectDip.left * scale), width),
              clampPx(std::floor(rectDip.top * scale), height),
              clampPx(std::ceil(rectDip.right * scale), width),
              clampPx(std::ceil(rectDip.bottom * scale), height)};
    if (rect.right <= rect.left || rect.bottom <= rect.top)
        return;
    const auto area  = [](const RECT& r) noexcept { return int64_t(r.right - r.left) * int64_t(r.bottom - r.top); };
    const auto unite = [](const RECT& a, const RECT& b) noexcept
    { return RECT{(std::min)(a.left, b.left), (std::min)(a.top, b.top), (std::max)(a.right, b.right), (std::max)(a.bottom, b.bottom)}; };
    const auto touches = [](const RECT& a, const RECT& b) noexcept { return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom; };
    const auto removeAt = [&region](size_t index) noexcept
    {
        region.rects[index] = region.rects[region.count - 1u];
        --region.count;
    };
    // Rectangles that touch or overlap the new one merge into it, until it touches none; the stored ones never overlap.
    for (bool merged = true; merged;)
    {
        merged = false;
        for (size_t index = 0u; index < region.count; ++index)
        {
            if (touches(region.rects[index], rect))
            {
                rect = unite(rect, region.rects[index]);
                removeAt(index);
                merged = true;
                break;
            }
        }
    }
    if (region.count == region.rects.size())
    {
        // Full: the new rectangle merges with the stored one whose union grows least (then may touch others: merge again).
        size_t best        = 0u;
        int64_t bestGrowth = (std::numeric_limits<int64_t>::max)();
        for (size_t index = 0u; index < region.count; ++index)
        {
            const int64_t growth = area(unite(rect, region.rects[index])) - area(rect) - area(region.rects[index]);
            if (growth < bestGrowth)
            {
                bestGrowth = growth;
                best       = index;
            }
        }
        rect = unite(rect, region.rects[best]);
        removeAt(best);
        for (bool merged = true; merged;)
        {
            merged = false;
            for (size_t index = 0u; index < region.count; ++index)
            {
                if (touches(region.rects[index], rect))
                {
                    rect = unite(rect, region.rects[index]);
                    removeAt(index);
                    merged = true;
                    break;
                }
            }
        }
    }
    region.rects[region.count++] = rect;
    int64_t covered              = 0;
    for (size_t index = 0u; index < region.count; ++index)
        covered += area(region.rects[index]);
    // Above half the view a full repaint costs about as much and needs no clip per rectangle.
    if (covered * 2 > int64_t(width) * int64_t(height))
        MarkFull(host);
}

bool PaintExtentAccess::MissesClip(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& extent) noexcept
{
    if (! host._paintClipBounded)
        return false;
    const D2D1_RECT_F box  = TransformBox(transform, extent);
    const D2D1_RECT_F clip = host._paintClipDip;
    return ! (box.left < clip.right && clip.left < box.right && box.top < clip.bottom && clip.top < box.bottom);
}

bool PaintExtentAccess::ClipEnclosedBy(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept
{
    if (! host._paintClipBounded || transform._12 != 0.0f || transform._21 != 0.0f)
        return false;
    // The clip's pixels must each lie wholly inside the rectangle, where an antialiased clip to it covers them fully.
    const float scale      = static_cast<float>(host.GetDpi()) / 96.0f;
    const D2D1_RECT_F box  = TransformBox(transform, rect);
    const D2D1_RECT_F clip = host._paintClipDip;
    const float left       = std::floor(clip.left * scale + 0.5f);
    const float top        = std::floor(clip.top * scale + 0.5f);
    const float right      = std::floor(clip.right * scale + 0.5f);
    const float bottom     = std::floor(clip.bottom * scale + 0.5f);
    return left >= std::ceil(box.left * scale) && top >= std::ceil(box.top * scale) && right <= std::floor(box.right * scale) &&
           bottom <= std::floor(box.bottom * scale);
}

void PaintExtentAccess::IntersectClip(ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept
{
    if (! host._paintClipBounded)
        return;
    const D2D1_RECT_F box  = TransformBox(transform, rect);
    const D2D1_RECT_F clip = host._paintClipDip;
    host._paintClipDip =
        D2D1::RectF((std::max)(clip.left, box.left), (std::max)(clip.top, box.top), (std::min)(clip.right, box.right), (std::min)(clip.bottom, box.bottom));
    if (host._paintClipDip.right < host._paintClipDip.left)
        host._paintClipDip.right = host._paintClipDip.left;
    if (host._paintClipDip.bottom < host._paintClipDip.top)
        host._paintClipDip.bottom = host._paintClipDip.top;
}

void PaintExtentAccess::NoteTextInk(ControlHost& host, const D2D1_RECT_F& inkDip) noexcept
{
    if (! host._paintingControl || ! host._paintingLimitBounded || host._paintingInkEscaped)
        return;
    const D2D1_RECT_F limit = host._paintingLimitDip;
    if (inkDip.left >= limit.left && inkDip.top >= limit.top && inkDip.right <= limit.right && inkDip.bottom <= limit.bottom)
        return;
    host._paintingInkEscaped = true;
}

void PaintExtentAccess::NoteTextInk(ControlHost& host, IDWriteTextLayout* layout, D2D1_POINT_2F origin) noexcept
{
    if (! layout || ! host._paintingControl || ! host._paintingLimitBounded || host._paintingInkEscaped)
        return;
    DWRITE_OVERHANG_METRICS overhang{};
    if (FAILED(layout->GetOverhangMetrics(&overhang)))
    {
        NoteUnknownTextInk(host);
        return;
    }
    NoteTextInk(host,
                D2D1::RectF(origin.x - overhang.left,
                            origin.y - overhang.top,
                            origin.x + layout->GetMaxWidth() + overhang.right,
                            origin.y + layout->GetMaxHeight() + overhang.bottom));
}

void PaintExtentAccess::NoteUnknownTextInk(ControlHost& host) noexcept
{
    if (host._paintingControl && host._paintingLimitBounded)
        host._paintingInkEscaped = true;
}

size_t PaintExtentAccess::CachedExtents(const Control& control) noexcept
{
    const auto* panel = dynamic_cast<const Panel*>(&control);
    return panel && panel->_paintExtent.valid ? 1u : 0u;
}

void PaintExtentAccess::DropCachedExtent(Control& control) noexcept
{
    if (const auto* panel = dynamic_cast<const Panel*>(&control))
        panel->_paintExtent = Panel::PaintExtent{};
}

PaintExtentAccess::ClipScope::ClipScope(ControlHost& clipHost) noexcept : host(clipHost), clip(clipHost._paintClipDip), bounded(clipHost._paintClipBounded)
{
}

PaintExtentAccess::ClipScope::~ClipScope()
{
    host._paintClipDip     = clip;
    host._paintClipBounded = bounded;
}

void PaintExtentAccess::SetClip(ControlHost& host, const D2D1_RECT_F& clipDip) noexcept
{
    // With regions off nothing is culled, as before W4.10.
    host._paintClipDip     = clipDip;
    host._paintClipBounded = Detail::PaintRegionsEnabled();
}

PaintExtentAccess::PaintingScope::PaintingScope(ControlHost& paintingHost, const Control& control) noexcept
    : host(paintingHost),
      lifetime(control._lifetimeToken),
      outerControl(paintingHost._paintingControl),
      outerLimit(paintingHost._paintingLimitDip),
      outerBounded(paintingHost._paintingLimitBounded),
      outerEscaped(paintingHost._paintingInkEscaped)
{
    host._paintingControl    = &control;
    host._paintingInkEscaped = false;
    // Nothing below an undeclared control is culled: its Paint may draw its children into another target, through an effect
    // or at a place the world transform does not say.
    if (! IsDeclared(control) && host._paintClipBounded)
    {
        clipSuspended          = true;
        outerClipBounded       = true;
        host._paintClipBounded = false;
    }
    // The limit is the control's bounds plus its declared reach; an undeclared or unbounded control escapes nothing.
    const bool wasEscaped      = (control._paintFlags & kInkEscaped) != 0u;
    control._paintFlags        = static_cast<uint8_t>(control._paintFlags & ~kInkEscaped);
    host._paintingLimitBounded = (control._paintFlags & kClipsText) == 0u && OwnExtent(host, control, host._paintingLimitDip);
    control._paintFlags        = static_cast<uint8_t>(control._paintFlags | (wasEscaped ? kInkEscaped : 0u));
}

PaintExtentAccess::PaintingScope::~PaintingScope()
{
    const Control* const control = host._paintingControl;
    if (control && host._paintingLimitBounded && ! lifetime.expired())
    {
        const bool escaped    = host._paintingInkEscaped;
        const bool wasEscaped = (control->_paintFlags & kInkEscaped) != 0u;
        if (escaped != wasEscaped)
        {
            control->_paintFlags = static_cast<uint8_t>(escaped ? (control->_paintFlags | kInkEscaped) : (control->_paintFlags & ~kInkEscaped));
            ExtentChanged(*control);
            if (escaped)
            {
                // Newly escaping: a region computed with its bounded extent may miss its ink; the frame repaints the view.
                host._paintInkEscapedNew = true;
                ++Detail::Work().frame.inkEscapes;
            }
        }
    }
    if (clipSuspended)
        host._paintClipBounded = outerClipBounded;
    host._paintingControl      = outerControl;
    host._paintingLimitDip     = outerLimit;
    host._paintingLimitBounded = outerBounded;
    host._paintingInkEscaped   = outerEscaped;
}

Control::~Control()
{
    // Normally a control leaves its host (PropagateHost(nullptr)) before it is destroyed; one destroyed while attached must
    // not leave the host counting it, which would turn hit-result reuse off for the host's life.
    if (_host && (_hitTestReporting & kHitTestCounted) != 0u)
        --_host->_unreportedHitControls;
}

void Control::RefreshLabelVisualPreparation() noexcept
{
    const auto* const label = dynamic_cast<const Label*>(this);
    if (! label)
        return;
    const bool plain = ! label->GetTypographyOverride() && GetMnemonic() == L'\0' && typeid(*this) == typeid(Label);
    if (plain == ! _visualPreparationEnabled)
        return;
    if (! plain)
    {
        EnableVisualContentPreparation();
        return;
    }
    // Plain again: nothing to prepare, and a stale prepared layout is released.
    _visualPreparationEnabled = false;
    _visualPreparationPending = false;
    CancelVisualContentPreparation();
}
#if DXUI_ENABLE_DIAGNOSTICS
bool Control::DebugIsHitTestReported() const noexcept
{
    return IsHitTestReported();
}

bool Control::DebugVisualPreparationEnabled() const noexcept
{
    return _visualPreparationEnabled;
}
#endif

Control* Control::HitTest(D2D1_POINT_2F point)
{
    return (_visible && _enabled && PointInRect(GetHitBounds(), point)) ? this : nullptr;
}

const Control* Control::HitTest(D2D1_POINT_2F point) const
{
    return (_visible && _enabled && PointInRect(GetHitBounds(), point)) ? this : nullptr;
}

Control* Control::HitTestOverlay(D2D1_POINT_2F /*point*/)
{
    return nullptr;
}

const Control* Control::HitTestOverlay(D2D1_POINT_2F /*point*/) const
{
    return nullptr;
}

bool Control::DismissOverlayOnPointerDown(ControlHost& host, D2D1_POINT_2F point)
{
    if (! IsVisible() || ! IsEnabled())
    {
        return false;
    }

    const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
    for (size_t childIndex = GetLogicalChildCount(); childIndex > 0u; --childIndex)
    {
        Control* const child = GetLogicalChild(childIndex - 1u);
        if (child && child->DismissOverlayOnPointerDown(host, point))
        {
            return true;
        }
        if (selfLifetime.expired())
        {
            return false;
        }
    }

    return false;
}

POINT Control::ResolveContextMenuAnchor(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip) const noexcept
{
    const Failure::Scope outerFailureScope;
    // Pointer dispatch uses content coordinates. Walk the logical parents, including non-Panel owners such
    // as PageHost, before the virtual hit-bounds query so no control state is read after that query. The walk also
    // gathers, in this control's coordinates, the part of the window the scroll viewports show.
    float ancestorScrollDip = 0.0f;
    bool clipped            = false;
    D2D1_RECT_F shown{};
    for (const Control* ancestor = _parent; ancestor; ancestor = ancestor->_parent)
    {
        const auto* scroll = dynamic_cast<const ScrollPanel*>(ancestor);
        if (! scroll)
            continue;
        ancestorScrollDip += scroll->GetScrollOffset();
        // A viewport is in the content space of the panels above it: in this control's space it moves down by the
        // offsets of the panels from here to it. An outer viewport that shows nothing of the inner ones prevails.
        D2D1_RECT_F viewport = scroll->GetViewportRect();
        viewport.right       = (std::max)(viewport.left, viewport.right);
        viewport.bottom      = (std::max)(viewport.top, viewport.bottom);
        viewport.top += ancestorScrollDip;
        viewport.bottom += ancestorScrollDip;
        const D2D1_RECT_F common = D2D1::RectF((std::max)(shown.left, viewport.left),
                                               (std::max)(shown.top, viewport.top),
                                               (std::min)(shown.right, viewport.right),
                                               (std::min)(shown.bottom, viewport.bottom));
        shown                    = clipped && common.left <= common.right && common.top <= common.bottom ? common : viewport;
        clipped                  = true;
    }
    if (keyboardInvocation)
    {
        // A keyboard menu opens at the reading-order start of the control, inside what the viewports show: a control
        // scrolled partly or wholly out of view would otherwise open its menu away from the window.
        const bool rightToLeft   = IsRightToLeft();
        const D2D1_RECT_F bounds = GetHitBounds();
        const float startX       = rightToLeft ? (std::max)(bounds.left, bounds.right - 16.0f) : (std::min)(bounds.right, bounds.left + 16.0f);
        pointDip                 = D2D1::Point2F(startX, bounds.top + ((bounds.bottom - bounds.top) * 0.5f));
        if (clipped)
            pointDip = D2D1::Point2F(std::clamp(pointDip.x, shown.left, shown.right), std::clamp(pointDip.y, shown.top, shown.bottom));
    }
    pointDip.y -= ancestorScrollDip;
    return host.DipPointToScreenPoint(pointDip);
}

WindowHostCursorKind Control::ResolveCursorKind(ControlHost& /*host*/, D2D1_POINT_2F /*pointDip*/) const noexcept
{
    return WindowHostCursorKind::Default;
}

// Attributed invalidations (W4.10): the control's painted extent, with what it paints below it, joins its host's paint region.
// An undeclared or unbounded extent, or a host the control is not attached to, repaints the whole view.
void Control::Invalidate(ControlHost& host) const
{
    // A control that reports every change of its record repaints with nothing undeclared for UI Automation (W8.2).
    const AttestedInvalidation reported(&host, ReportsOwnAccessibilityChanges(*this));
    PaintExtentAccess::Invalidate(host, *this);
}

void Control::RequestInvalidate() const noexcept
{
    if (_host)
    {
        const AttestedInvalidation reported(_host, ReportsOwnAccessibilityChanges(*this));
        PaintExtentAccess::Invalidate(*_host, *this);
    }
}

void Control::InvalidateMeasure() noexcept
{
    ++Detail::Work().layout.measureInvalidations;
    bool hasLayoutOwner = false;
    for (Control* control = this; control; control = control->_parent)
    {
        ++control->_measureRevision;
        if (control->_intrinsicLayoutEnabled)
        {
            control->_intrinsicAllocationRetryUsed = false;
            control->_intrinsicLayoutPending       = true;
            hasLayoutOwner                         = true;
        }
    }
    // Geometry moves only when the next preparation arranges it, and SetBounds then advances the interaction revision.
    // Until then input keeps meeting the geometry on screen, so an edit (a validation message, a multiline field's text)
    // neither drops the next key nor cancels a press.
    if (hasLayoutOwner && _host)
    {
        _host->_pendingIntrinsicLayout = true;
        // The layout it asks for moves controls through SetBounds, which adds what moves; until then this control's paint.
        PaintExtentAccess::Invalidate(*_host, *this);
    }
}

void Control::EnableIntrinsicLayout() noexcept
{
    _intrinsicLayoutEnabled = true;
    RequestIntrinsicLayout();
}

void Control::RequestIntrinsicLayout() noexcept
{
    if (_intrinsicLayoutEnabled)
    {
        _intrinsicAllocationRetryUsed = false;
        _intrinsicLayoutPending       = true;
        if (_host)
        {
            _host->_pendingIntrinsicLayout = true;
            // A layout request: what the layout then changes reaches UI Automation through each child's SetBounds, which
            // marks it (W8.2), so the repaint that asks for the layout is a declared one.
            const AttestedInvalidation layout(_host);
            _host->Invalidate();
        }
    }
}

HRESULT Control::PrepareIntrinsicLayout(const MeasureContext& /*context*/) noexcept
{
    return S_OK;
}
void Control::EnableContentPreparation() noexcept
{
    _contentPreparationEnabled = true;
    RequestContentPreparation(false);
}
void Control::RequestContentPreparation(bool requestWake) const noexcept
{
    if (! _contentPreparationEnabled)
        return;
    _contentPreparationState = ContentPreparationState::Pending;
    if (_host)
    {
        _host->_pendingContentPreparation = true;
        // Application scheduling may retire this control. All state is set
        // before crossing that boundary; nothing is read after it returns.
        if (requestWake)
            _host->Invalidate();
    }
}
HRESULT Control::PrepareContent(ControlHost&) noexcept
{
    return S_OK;
}
void Control::CancelContentPreparation() noexcept
{
}
void Control::EnableVisualContentPreparation() noexcept
{
    _visualPreparationEnabled = true;
    RequestVisualContentPreparation(false);
}
void Control::RequestVisualContentPreparation(bool requestWake) const noexcept
{
    if (! _visualPreparationEnabled)
        return;
    _visualPreparationPending = true;
    if (_host)
    {
        _host->_pendingVisualPreparation = true;
        if (requestWake)
            _host->Invalidate();
    }
}
void Control::PrepareVisualContent(ControlHost& /*host*/) noexcept
{
}
void Control::CancelVisualContentPreparation() noexcept
{
}
bool Control::AcceptsTextInputDuringPreparation() const noexcept
{
    return false;
}

void Control::PropagateHost(ControlHost* host) noexcept
{
    if (_host == host)
        return;

    // Measurements belong to the host that made them: none survive a move, even to a later host at the same address.
    _measureCache.reset();
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    ControlHost* const previousHost   = _host;
    if (previousHost && _visualPreparationEnabled)
    {
        CancelVisualContentPreparation();
        if (lifetime.expired() || _host != previousHost)
            return;
        _visualPreparationPending = true;
    }
    if (previousHost)
    {
        // The departing host forgets this control's frame-schedule request before anything can call back; the host it
        // joins next ticks it once instead, so a transition in flight lands or resumes there.
        if (previousHost->ForgetControl(*this))
            _tickRequestPending = true;
        previousHost->InvalidateRichTitleTooltip(this);
        if (lifetime.expired() || _host != previousHost)
            return;
        ++previousHost->_interactionRevision;
        // A control leaving the host may be (or contain) the supplemental tooltip's target.
        if (previousHost->MarkTooltipTargetStale())
        {
            previousHost->OnTooltipScheduleChanged();
            if (lifetime.expired() || _host != previousHost)
                return;
        }
        previousHost->Invalidate();
        if (lifetime.expired() || _host != previousHost)
            return;
    }
    if (host)
    {
        ++host->_interactionRevision;
        // A control joining the host may cover the supplemental tooltip's target.
        if (host->MarkTooltipTargetStale())
        {
            host->OnTooltipScheduleChanged();
            if (lifetime.expired() || _host != previousHost)
                return;
        }
        host->Invalidate();
        if (lifetime.expired() || _host != previousHost)
            return;
    }
    // A host reuses no hit result while it holds a control whose hit testing it cannot see.
    if (previousHost && (_hitTestReporting & kHitTestCounted) != 0u)
    {
        --previousHost->_unreportedHitControls;
        _hitTestReporting = static_cast<uint8_t>(_hitTestReporting & ~kHitTestCounted);
    }
    // W4.10: a tree change makes every cached painted extent of both hosts stale (both repaint whole frames above).
    if (previousHost)
        ++previousHost->_paintTopologyRevision;
    if (host)
    {
        ++host->_paintTopologyRevision;
        PaintExtentAccess::Resolve(*this);
    }
    else
        _paintKind = static_cast<uint8_t>(PaintKind::Unresolved);
    _host = host;
    if (host && std::exchange(_tickRequestPending, false))
        host->RequestAnimationOnce(*this);
    if (host && ! IsHitTestReported())
    {
        if (host->_unreportedHitControls++ == 0u && Detail::Trace::Listening()) [[unlikely]]
            Detail::Trace::HitReuseOff(host->_trace);
        _hitTestReporting = static_cast<uint8_t>(_hitTestReporting | kHitTestCounted);
    }
    // A Label subclass keeps the visual preparation a plain Label does without (its dynamic type is known only now).
    if (host)
        RefreshLabelVisualPreparation();
    RequestContentPreparation(false);
    RequestVisualContentPreparation(false);
    InvalidateMeasure();
    if (! lifetime.expired() && _host == host)
        RequestIntrinsicLayout();
}

void Control::SetParent(Panel* parent) noexcept
{
    Reparent(parent, parent ? parent->_host : nullptr);
}

void Control::Reparent(Panel* parent, ControlHost* host) noexcept
{
    ReparentWithPreviousInheritance(parent, host, GetFlowDirection(), GetDensity());
}

void Control::ReparentWithPreviousInheritance(Control* parent, ControlHost* host, FlowDirection flowDirection, Density density) noexcept
{
    const Failure::Scope outerFailureScope;
    const std::weak_ptr<int> lifetime       = GetLifetimeToken();
    const std::weak_ptr<int> parentLifetime = parent ? parent->GetLifetimeToken() : std::weak_ptr<int>{};
    // The flow direction and density a control inherits resolve through its parent chain (a root takes its host's
    // density). A parent's own change is announced to its children; a move changes them just the same, so the moved
    // control hears it the same way: once, after it stands in its final place, and only when a value differs. A child
    // that inherits what its parent already has, and a host being torn down, announce nothing. Snapshot the previous
    // values before callbacks can retire the old parent; this operation reads only the new parent chain.
    // W4.10: a move within one host leaves its host unchanged but the panels' painted extents stale.
    // Topology repaints the whole view.
    if (_parent != parent && _host)
    {
        ++_host->_paintTopologyRevision;
        PaintExtentAccess::MarkFull(*_host);
    }
    _parent = parent;
    if (parent || host)
    {
        _detachedFlowDirection.reset();
        _detachedDensity.reset();
    }
    PropagateHost(parent ? parent->_host : nullptr);
    if (lifetime.expired())
    {
        return;
    }
    if (parent && parentLifetime.expired())
    {
        _parent = nullptr;
        return;
    }
    if (host != _host)
    {
        PropagateHost(host);
        if (lifetime.expired())
        {
            return;
        }
        if (parent && parentLifetime.expired())
        {
            _parent = nullptr;
            return;
        }
    }
    if (GetFlowDirection() != flowDirection)
    {
        OnFlowDirectionChanged();
        if (lifetime.expired())
        {
            return;
        }
        if (parent && parentLifetime.expired())
        {
            _parent = nullptr;
            return;
        }
    }
    if (GetDensity() != density)
    {
        OnDensityChanged();
        if (lifetime.expired())
        {
            return;
        }
        if (parent && parentLifetime.expired())
        {
            _parent = nullptr;
        }
    }
}

Panel* Control::GetParent() const noexcept
{
    return dynamic_cast<Panel*>(_parent);
}

void Control::OnBoundsChanged() noexcept
{
}

void Control::OnFlowDirectionChanged() noexcept
{
    const Failure::Scope outerFailureScope;
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    InvalidateMeasure();
    if (lifetime.expired())
        return;
    if (_host && _host->GetFocusControl() == this && SupportsTextInput())
    {
        _host->SyncTextInput(this);
        if (lifetime.expired())
        {
            return;
        }
    }
    RequestInvalidate();
}

void Control::OnDensityChanged() noexcept
{
    const Failure::Scope outerFailureScope;
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    InvalidateMeasure();
    if (lifetime.expired())
        return;
    if (_host && _host->GetFocusControl() == this && SupportsTextInput())
    {
        _host->SyncTextInput(this);
        if (lifetime.expired())
        {
            return;
        }
    }
    RequestInvalidate();
}

void Control::OnEnabledChanged(bool /*enabled*/) noexcept
{
}

void Control::OnHidden() noexcept
{
}

void Control::OnHostDpiChanged(ControlHost& /*host*/) noexcept
{
    InvalidateMeasure();
}

void Control::OnFocusChanged(ControlHost& /*host*/, bool focused)
{
    _hasFocus = focused;
}

void Control::OnHoverChanged(ControlHost& /*host*/, bool hovered)
{
    _hovered = hovered;
}

void Control::OnCaptureLost(ControlHost& /*host*/)
{
}

bool Control::SupportsTextInput() const noexcept
{
    return false;
}

bool Control::AcceptsTypedText() const noexcept
{
    return SupportsTextInput();
}

std::optional<D2D1_RECT_F> Control::GetTextInputViewportRect() const noexcept
{
    return std::nullopt;
}

std::optional<D2D1_RECT_F> Control::GetTextInputCaretRect(const ControlHost& /*host*/, size_t /*controlTextIndex*/) const noexcept
{
    return std::nullopt;
}

std::optional<std::vector<D2D1_RECT_F>> Control::GetTextInputRangeRects(const ControlHost& /*host*/,
                                                                        size_t /*controlTextStartIndex*/,
                                                                        size_t /*controlTextEndIndex*/) const
{
    return std::nullopt;
}

std::optional<size_t> Control::HitTestTextInputPoint(const ControlHost& /*host*/, D2D1_POINT_2F /*point*/) const noexcept
{
    return std::nullopt;
}

bool Control::ExportTextInputState(TextInputState& /*outState*/) const
{
    return false;
}

bool Control::ImportTextInputState(ControlHost& /*host*/, const TextInputState& /*state*/, bool /*notifyChange*/)
{
    return false;
}

std::vector<std::unique_ptr<Control>>& Panel::AccessChildren() noexcept
{
    return _children;
}

const std::vector<std::unique_ptr<Control>>& Panel::AccessChildren() const noexcept
{
    return _children;
}

} // namespace DxUi
