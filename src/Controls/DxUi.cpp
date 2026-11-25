#include <DxUi/Failure.h>

#include "DxUi.Internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <exception>
#include <limits>
#include <typeinfo>
#include <utility>

#include <UIAutomation.h>
#include <d2d1effects.h>

#include "Support/Diagnostics.h"
#include "Support/LayoutScopes.Tests.Hooks.h"
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
    ReleaseRetainedBlur();
}

void TransientSurfaceBackdrop::Reset() noexcept
{
    capture = {};
    cachedBitmap.reset();
    cachedDevice.reset();
    ReleaseRetainedBlur();
}

void TransientSurfaceBackdrop::ReleaseRetainedBlur() noexcept
{
    cachedClipGeometry.reset();
    cachedClipShape = {};
    cachedBlurredBitmap.reset();
    cachedBlurSource.reset();
    cachedBlurDip = 0.0f;
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

    if (Detail::ArePopupSurfacesUncachedForTest() || Detail::g_paintVerificationActive)
    {
        PopupBackdropState fresh{};
        PaintPopupBackdropLayer(host, *dc, fresh, bitmap, surfaceRect, cornerRadiusDip, opacity, blurDip, 0.0f, false, false);
        return;
    }
    // A capture bitmap made again (a new capture, a new device) takes what was made from the old one with it: the clip
    // belongs to that device's factory, the blurred bitmap to its device and capture.
    if (backdrop->cachedBlurSource.get() != bitmap)
        backdrop->ReleaseRetainedBlur();
    PopupBackdropState state{};
    state.clipGeometry  = std::move(backdrop->cachedClipGeometry);
    state.clipShape     = backdrop->cachedClipShape;
    state.blurredBitmap = std::move(backdrop->cachedBlurredBitmap);
    state.blurSource    = std::move(backdrop->cachedBlurSource);
    state.blurDip       = backdrop->cachedBlurDip;
    // As a popup's backdrop: the clip and the blurred capture are kept, made before the layer is drawn (a verifier's reference
    // paint makes nothing), and every paint draws from them, its first included, so whole and region frames draw the same.
    if (! Detail::g_paintVerificationActive)
    {
        static_cast<void>(EnsurePopupClipGeometry(*dc, state, D2D1::RoundedRect(surfaceRect, cornerRadiusDip, cornerRadiusDip)));
        EnsurePopupBlurredBackdrop(host, *dc, state, bitmap, blurDip);
    }
    const D2D1_ROUNDED_RECT shape = D2D1::RoundedRect(surfaceRect, cornerRadiusDip, cornerRadiusDip);
    if (state.clipGeometry && std::memcmp(&state.clipShape, &shape, sizeof(shape)) == 0)
        PaintPopupBackdropLayer(host, *dc, state, bitmap, surfaceRect, cornerRadiusDip, opacity, blurDip, 0.0f, true, false);
    else
    {
        PopupBackdropState fresh{};
        PaintPopupBackdropLayer(host, *dc, fresh, bitmap, surfaceRect, cornerRadiusDip, opacity, blurDip, 0.0f, false, false);
    }
    backdrop->cachedClipGeometry  = std::move(state.clipGeometry);
    backdrop->cachedClipShape     = state.clipShape;
    backdrop->cachedBlurredBitmap = std::move(state.blurredBitmap);
    backdrop->cachedBlurSource    = bitmap; // everything above was made from this capture's bitmap
    backdrop->cachedBlurDip       = state.blurDip;
}
} // namespace

namespace
{
[[nodiscard]] bool SameColor(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) noexcept
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

[[nodiscard]] bool SameRoundedRect(const D2D1_ROUNDED_RECT& a, const D2D1_ROUNDED_RECT& b) noexcept
{
    return a.rect.left == b.rect.left && a.rect.top == b.rect.top && a.rect.right == b.rect.right && a.rect.bottom == b.rect.bottom && a.radiusX == b.radiusX &&
           a.radiusY == b.radiusY;
}

[[nodiscard]] bool SameMatrix(const D2D1_MATRIX_3X2_F& a, const D2D1_MATRIX_3X2_F& b) noexcept
{
    return a._11 == b._11 && a._12 == b._12 && a._21 == b._21 && a._22 == b._22 && a._31 == b._31 && a._32 == b._32;
}

[[nodiscard]] bool IsTranslation(const D2D1_MATRIX_3X2_F& m) noexcept
{
    return m._11 == 1.0f && m._12 == 0.0f && m._21 == 0.0f && m._22 == 1.0f;
}

[[nodiscard]] uint64_t PixelBytes(ID2D1Bitmap1* bitmap) noexcept
{
    if (! bitmap)
        return 0u;
    const D2D1_SIZE_U size = bitmap->GetPixelSize();
    return uint64_t(size.width) * size.height * 4u;
}

// A 32-bit premultiplied bitmap the context can draw into, at the context's DPI.
[[nodiscard]] wil::com_ptr<ID2D1Bitmap1> CreatePopupTargetBitmap(ID2D1DeviceContext& dc, UINT32 widthPx, UINT32 heightPx) noexcept
{
    float dpiX = 96.0f;
    float dpiY = 96.0f;
    dc.GetDpi(&dpiX, &dpiY);
    const D2D1_BITMAP_PROPERTIES1 properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpiX, dpiY);
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    if (FAILED(CountedCreateBitmap(&dc, D2D1::SizeU(widthPx, heightPx), nullptr, 0u, &properties, bitmap.put())))
        return {};
    return bitmap;
}

} // namespace

namespace
{
// The capture's blur, made for one draw (the code before W2.4, and every popup not on the target's pixel grid).
[[nodiscard]] wil::com_ptr<ID2D1Effect> CreatePopupBlurEffect(ID2D1DeviceContext& dc, ID2D1Bitmap1* source, float blurDip) noexcept
{
    wil::com_ptr<ID2D1Effect> effect;
    if (FAILED(CountedCreateEffect(&dc, kTransientSurfaceGaussianBlurEffectId, effect.put())) || ! effect)
        return {};
    effect->SetInput(0u, source);
    effect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blurDip);
    effect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED);
    effect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    return effect;
}
} // namespace

uint64_t PopupResourceCreations() noexcept
{
    const auto& r = Detail::Work().resources;
    return r.effect.created + r.commandList.created + r.layer.created + r.geometry.created + r.gradientStops.created + r.brush.created + r.bitmap.created +
           r.deviceContext.created;
}

bool EnsurePopupClipGeometry(ID2D1DeviceContext& dc, PopupBackdropState& state, const D2D1_ROUNDED_RECT& shape) noexcept
{
    if (state.clipGeometry && SameRoundedRect(state.clipShape, shape))
        return true;
    state.clipGeometry.reset();
    wil::com_ptr<ID2D1Factory> factory;
    dc.GetFactory(factory.put());
    if (! factory || FAILED(CountedCreateRoundedRectangleGeometry(factory, shape, state.clipGeometry.put())) || ! state.clipGeometry)
    {
        state.clipGeometry.reset();
        return false;
    }
    state.clipShape = shape;
    return true;
}
// The capture blurred once into a bitmap of its size, made on the frame's context before the surface draws and drawn at the
// popup's position in every paint. Direct2D's blur result depends on the paint clip, so a blur drawn for each paint differs
// between a whole frame and a frame that repaints a region; the kept result is the same in both (developer's decision of 25
// November 2026, W2.4). Never made in a verifier's reference paint.
void EnsurePopupBlurredBackdrop(ControlHost& host, ID2D1DeviceContext& dc, PopupBackdropState& state, ID2D1Bitmap1* source, float blurDip) noexcept
{
    if (Detail::g_paintVerificationActive)
        return;
    float dpiX = 96.0f;
    float dpiY = 96.0f;
    dc.GetDpi(&dpiX, &dpiY);
    float blurredDpiX = 0.0f;
    float blurredDpiY = 0.0f;
    if (state.blurredBitmap)
        state.blurredBitmap->GetDpi(&blurredDpiX, &blurredDpiY);
    if (state.blurredBitmap && state.blurSource.get() == source && state.blurDip == blurDip && blurredDpiX == dpiX && blurredDpiY == dpiY)
        return;
    state.blurredBitmap.reset();
    state.blurSource.reset();
    const D2D1_SIZE_U sourcePx        = source->GetPixelSize();
    wil::com_ptr<ID2D1Effect> blur    = CreatePopupBlurEffect(dc, source, blurDip);
    wil::com_ptr<ID2D1Bitmap1> bitmap = blur ? CreatePopupTargetBitmap(dc, sourcePx.width, sourcePx.height) : nullptr;
    if (! bitmap)
        return;
    const D2D1_RECT_F sourceRect =
        D2D1::RectF(0.0f, 0.0f, host.PixelsToDip(static_cast<float>(sourcePx.width)), host.PixelsToDip(static_cast<float>(sourcePx.height)));
    wil::com_ptr<ID2D1Image> previousTarget;
    dc.GetTarget(previousTarget.put());
    D2D1_MATRIX_3X2_F transform{};
    dc.GetTransform(&transform);
    dc.SetTarget(bitmap.get());
    dc.SetTransform(D2D1::Matrix3x2F::Identity());
    dc.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    dc.DrawImage(blur.get(), nullptr, &sourceRect, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
    ++Detail::Work().popups.backdropBlurPasses;
    dc.SetTransform(transform);
    dc.SetTarget(previousTarget.get());
    state.blurredBitmap = std::move(bitmap);
    state.blurSource    = source;
    state.blurDip       = blurDip;
}
// The blurred backdrop inside its rounded layer (ComboBox list, menu popup, transient surface). `retain` false is the code
// before W2.4: geometry and effect made for this paint and (createLayer) a created layer. Retained: the clip geometry and
// the blur effect are kept. The blur itself runs every paint: no retained form of its result draws the per-paint pixels on
// every host (W2.4 receipt).
void PaintPopupBackdropLayer(ControlHost& host,
                             ID2D1DeviceContext& dc,
                             PopupBackdropState& state,
                             ID2D1Bitmap1* source,
                             const D2D1_RECT_F& rect,
                             float cornerRadiusDip,
                             float opacity,
                             float blurDip,
                             float detailOpacity,
                             bool retain,
                             bool createLayer) noexcept
{
    if (! EnsurePopupClipGeometry(dc, state, D2D1::RoundedRect(rect, cornerRadiusDip, cornerRadiusDip)))
        return;
    // Retained: the blurred bitmap (EnsurePopupBlurredBackdrop). The per-paint code blurs for this paint.
    ID2D1Bitmap1* const blurred = retain && state.blurSource.get() == source && state.blurDip == blurDip ? state.blurredBitmap.get() : nullptr;
    wil::com_ptr<ID2D1Effect> blur;
    if (! blurred)
    {
        blur = CreatePopupBlurEffect(dc, source, blurDip);
        if (! blur)
            return;
    }
    wil::com_ptr<ID2D1Layer> layer;
    if (createLayer && (FAILED(CountedCreateLayer(&dc, layer.put())) || ! layer))
        return;
    const D2D1_SIZE_U sourcePx = source->GetPixelSize();
    const D2D1_RECT_F sourceRect =
        D2D1::RectF(0.0f, 0.0f, host.PixelsToDip(static_cast<float>(sourcePx.width)), host.PixelsToDip(static_cast<float>(sourcePx.height)));
    const D2D1_POINT_2F targetOffset             = D2D1::Point2F(rect.left, rect.top);
    const D2D1_LAYER_PARAMETERS1 layerParameters = D2D1::LayerParameters1(rect,
                                                                          state.clipGeometry.get(),
                                                                          D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                                          D2D1::Matrix3x2F::Identity(),
                                                                          std::clamp(opacity, 0.0f, 1.0f),
                                                                          nullptr,
                                                                          D2D1_LAYER_OPTIONS1_NONE);
    dc.PushLayer(layerParameters, layer.get());
    if (blurred)
    {
        dc.DrawImage(blurred, &targetOffset, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
    }
    else
    {
        dc.DrawImage(blur.get(), &targetOffset, &sourceRect, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
        ++Detail::Work().popups.backdropBlurPasses;
    }
    if (detailOpacity > 0.0f)
    {
        dc.DrawBitmap(source, rect, std::clamp(detailOpacity, 0.0f, 1.0f), D2D1_INTERPOLATION_MODE_LINEAR);
    }
    dc.PopLayer();
}
namespace
{
constexpr GUID kPopupShadowEffectId = {0xC67EA361, 0x1863, 0x4e69, {0x89, 0xDB, 0x69, 0x5D, 0x3E, 0x9A, 0x5B, 0x6B}};

// Records the pass's mask and makes its effect, as DrawShadowEffectPass does for one paint; kept while the shape and the
// transform the mask was recorded under stay the same.
[[nodiscard]] bool EnsurePopupShadowPass(ControlHost& host,
                                         ID2D1DeviceContext& dc,
                                         PopupShadowPass& pass,
                                         const D2D1_RECT_F& targetRect,
                                         float cornerRadiusDip,
                                         float blurDip,
                                         float opacity) noexcept
{
    if (blurDip <= 0.0f || opacity <= 0.0f)
        return false;
    D2D1_MATRIX_3X2_F transform{};
    dc.GetTransform(&transform);
    const D2D1_ROUNDED_RECT shape = D2D1::RoundedRect(targetRect, cornerRadiusDip, cornerRadiusDip);
    if (! pass.mask || ! SameRoundedRect(pass.shape, shape) || ! SameMatrix(pass.transform, transform))
    {
        // The new mask is made before the old one is released, so the caller can tell them apart by address.
        wil::com_ptr<ID2D1Image> previousTarget;
        dc.GetTarget(previousTarget.put());
        wil::com_ptr<ID2D1CommandList> mask;
        if (FAILED(CountedCreateCommandList(&dc, mask.put())) || ! mask)
            return false;
        dc.SetTarget(mask.get());
        const auto restoreTarget = wil::scope_exit([&]() noexcept { dc.SetTarget(previousTarget.get()); });
        dc.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        auto* const maskBrush = host.GetSolidBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
        if (! maskBrush)
            return false;
        dc.FillRoundedRectangle(shape, maskBrush);
        dc.SetTarget(previousTarget.get());
        if (FAILED(mask->Close()))
            return false;
        pass.mask      = std::move(mask);
        pass.shape     = shape;
        pass.transform = transform;
        if (pass.effect)
            pass.effect->SetInput(0u, pass.mask.get());
    }
    if (! pass.effect)
    {
        if (FAILED(CountedCreateEffect(&dc, kPopupShadowEffectId, pass.effect.put())) || ! pass.effect)
        {
            pass.effect.reset();
            return false;
        }
        // In DrawShadowEffectPass's order.
        pass.effect->SetInput(0u, pass.mask.get());
        pass.effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, blurDip);
        pass.effect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(0.0f, 0.0f, 0.0f, opacity));
        pass.effect->SetValue(D2D1_SHADOW_PROP_OPTIMIZATION, D2D1_SHADOW_OPTIMIZATION_QUALITY);
        pass.blurDip = blurDip;
        pass.opacity = opacity;
    }
    if (pass.blurDip != blurDip)
    {
        pass.effect->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, blurDip);
        pass.blurDip = blurDip;
    }
    if (pass.opacity != opacity)
    {
        pass.effect->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(0.0f, 0.0f, 0.0f, opacity));
        pass.opacity = opacity;
    }
    return true;
}

// DrawShadowEffectPass switches the target to record each mask just before it draws, which flushes the drawing batched before
// the pass; a retained pass flushes there too, so its draw meets the target as the per-paint pass did.
void DrawPopupShadowPass(ID2D1DeviceContext& dc, const PopupShadowPass& pass, float yOffsetDip) noexcept
{
    static_cast<void>(dc.Flush());
    const D2D1_POINT_2F offset = D2D1::Point2F(0.0f, yOffsetDip);
    dc.DrawImage(pass.effect.get(), &offset, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
    ++Detail::Work().frame.shadowPasses;
}

// OverClearedSurface: both passes rendered once into a bitmap that starts at the target's device pixel (0,0), under the same
// transform, so its pixels are those the passes leave on cleared transparent pixels; drawn back at (0,0) unscaled.
[[nodiscard]] ID2D1Bitmap1* EnsurePopupShadowBitmap(ID2D1DeviceContext& dc, PopupSurfaceCache& cache, float yOffsetDip) noexcept
{
    if (cache.shadowBitmap && cache.shadowYOffsetDip == yOffsetDip)
        return cache.shadowBitmap.get();
    cache.shadowBitmap.reset();
    wil::com_ptr<ID2D1Image> previousTarget;
    dc.GetTarget(previousTarget.put());
    wil::com_ptr<ID2D1Bitmap> targetBitmap;
    if (! previousTarget || FAILED(previousTarget->QueryInterface(IID_PPV_ARGS(targetBitmap.put()))) || ! targetBitmap)
        return nullptr;
    const D2D1_SIZE_U targetPx = targetBitmap->GetPixelSize();
    float dpiX                 = 96.0f;
    float dpiY                 = 96.0f;
    dc.GetDpi(&dpiX, &dpiY);
    D2D1_RECT_F bounds{};
    for (const PopupShadowPass& pass : cache.passes)
    {
        D2D1_RECT_F passBounds{};
        wil::com_ptr<ID2D1Image> output;
        pass.effect->GetOutput(output.put());
        if (! output || FAILED(dc.GetImageWorldBounds(output.get(), &passBounds)))
            return nullptr;
        bounds.right  = (std::max)(bounds.right, passBounds.right);
        bounds.bottom = (std::max)(bounds.bottom, passBounds.bottom + yOffsetDip);
    }
    const auto extentPx = [](float dip, float dpi, UINT32 limit) noexcept
    { return static_cast<UINT32>(std::clamp(std::ceil(dip * dpi / 96.0f) + 1.0f, 1.0f, static_cast<float>((std::max)(limit, 1u)))); };
    wil::com_ptr<ID2D1Bitmap1> bitmap =
        CreatePopupTargetBitmap(dc, extentPx(bounds.right, dpiX, targetPx.width), extentPx(bounds.bottom, dpiY, targetPx.height));
    if (! bitmap)
        return nullptr;
    dc.SetTarget(bitmap.get());
    dc.Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    DrawPopupShadowPass(dc, cache.passes[0], yOffsetDip);
    DrawPopupShadowPass(dc, cache.passes[1], yOffsetDip);
    dc.SetTarget(previousTarget.get());
    cache.shadowBitmap     = std::move(bitmap);
    cache.shadowYOffsetDip = yOffsetDip;
    return cache.shadowBitmap.get();
}

// Makes (or keeps) both passes of DrawDropShadow and, over a cleared surface, the bitmap they are rendered into. False when
// a pass cannot be made or has nothing to draw: the caller then draws DrawDropShadow itself.
[[nodiscard]] bool PreparePopupShadow(ControlHost& host,
                                      ID2D1DeviceContext& dc,
                                      PopupSurfaceCache& cache,
                                      const D2D1_RECT_F& targetRect,
                                      float cornerRadiusDip,
                                      float yOffsetDip,
                                      float spreadDip,
                                      float outerOpacity,
                                      float innerOpacity,
                                      PopupShadowTarget shadowTarget) noexcept
{
    const D2D1_RECT_F innerTargetRect       = D2D1::RectF(targetRect.left - 0.5f, targetRect.top - 0.5f, targetRect.right + 0.5f, targetRect.bottom + 0.5f);
    const float innerBlurDip                = (std::max)(1.0f, spreadDip * 0.4f);
    const float innerCornerRadiusDip        = cornerRadiusDip + 0.5f;
    const ID2D1CommandList* const outerMask = cache.passes[0].mask.get();
    const ID2D1CommandList* const innerMask = cache.passes[1].mask.get();
    const ID2D1Effect* const outerEffect    = cache.passes[0].effect.get();
    const ID2D1Effect* const innerEffect    = cache.passes[1].effect.get();
    const float previousValues[4]           = {cache.passes[0].blurDip, cache.passes[0].opacity, cache.passes[1].blurDip, cache.passes[1].opacity};
    if (! EnsurePopupShadowPass(host, dc, cache.passes[0], targetRect, cornerRadiusDip, spreadDip, outerOpacity) ||
        ! EnsurePopupShadowPass(host, dc, cache.passes[1], innerTargetRect, innerCornerRadiusDip, innerBlurDip, innerOpacity))
    {
        cache.shadowBitmap.reset();
        return false;
    }
    // Any pass input changed: the bitmap rendered from the old passes goes.
    if (cache.passes[0].mask.get() != outerMask || cache.passes[1].mask.get() != innerMask || cache.passes[0].effect.get() != outerEffect ||
        cache.passes[1].effect.get() != innerEffect || previousValues[0] != cache.passes[0].blurDip || previousValues[1] != cache.passes[0].opacity ||
        previousValues[2] != cache.passes[1].blurDip || previousValues[3] != cache.passes[1].opacity)
        cache.shadowBitmap.reset();
    D2D1_MATRIX_3X2_F transform{};
    dc.GetTransform(&transform);
    if (shadowTarget == PopupShadowTarget::OverClearedSurface && IsTranslation(transform))
        static_cast<void>(EnsurePopupShadowBitmap(dc, cache, yOffsetDip));
    return true;
}

// Draws the prepared shadow: the bitmap over a cleared surface, else both passes.
void DrawPreparedPopupShadow(ID2D1DeviceContext& dc, const PopupSurfaceCache& cache, float yOffsetDip, PopupShadowTarget shadowTarget) noexcept
{
    D2D1_MATRIX_3X2_F transform{};
    dc.GetTransform(&transform);
    if (shadowTarget == PopupShadowTarget::OverClearedSurface && IsTranslation(transform) && cache.shadowBitmap && cache.shadowYOffsetDip == yOffsetDip)
    {
        dc.SetTransform(D2D1::Matrix3x2F::Identity());
        dc.DrawImage(cache.shadowBitmap.get(), nullptr, nullptr, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
        dc.SetTransform(transform);
        // The per-paint passes switch the target to record their masks, which flushes the batch; the bitmap's draw is
        // flushed too, so what follows blends as it did after them.
        static_cast<void>(dc.Flush());
        return;
    }
    DrawPopupShadowPass(dc, cache.passes[0], yOffsetDip);
    DrawPopupShadowPass(dc, cache.passes[1], yOffsetDip);
}

[[nodiscard]] ID2D1LinearGradientBrush* EnsurePopupGradient(ID2D1DeviceContext& dc,
                                                            PopupGradient& gradient,
                                                            const D2D1_RECT_F& rect,
                                                            const D2D1_COLOR_F (&colors)[3]) noexcept
{
    if (! gradient.stops || ! SameColor(gradient.colors[0], colors[0]) || ! SameColor(gradient.colors[1], colors[1]) ||
        ! SameColor(gradient.colors[2], colors[2]))
    {
        gradient.stops.reset();
        gradient.brush.reset();
        const D2D1_GRADIENT_STOP stops[3] = {
            D2D1::GradientStop(0.0f, colors[0]),
            D2D1::GradientStop(0.42f, colors[1]),
            D2D1::GradientStop(1.0f, colors[2]),
        };
        if (FAILED(CountedCreateGradientStopCollection(&dc, stops, static_cast<UINT32>(std::size(stops)), gradient.stops.put())) || ! gradient.stops)
        {
            gradient.stops.reset();
            return nullptr;
        }
        std::copy(std::begin(colors), std::end(colors), std::begin(gradient.colors));
    }
    if (! gradient.brush)
    {
        if (FAILED(
                CountedCreateLinearGradientBrush(&dc,
                                                 D2D1::LinearGradientBrushProperties(D2D1::Point2F(rect.left, rect.top), D2D1::Point2F(rect.left, rect.bottom)),
                                                 gradient.stops.get(),
                                                 gradient.brush.put())) ||
            ! gradient.brush)
        {
            gradient.brush.reset();
            return nullptr;
        }
    }
    else if (gradient.left != rect.left || gradient.top != rect.top || gradient.bottom != rect.bottom)
    {
        gradient.brush->SetStartPoint(D2D1::Point2F(rect.left, rect.top));
        gradient.brush->SetEndPoint(D2D1::Point2F(rect.left, rect.bottom));
    }
    gradient.left   = rect.left;
    gradient.top    = rect.top;
    gradient.bottom = rect.bottom;
    return gradient.brush.get();
}

// The code before W2.4, kept as the oracle's reference (SetPopupSurfacesUncachedForTest): every object made for this paint.
[[nodiscard]] wil::com_ptr<ID2D1LinearGradientBrush> CreatePopupGradientBrush(ID2D1DeviceContext& dc,
                                                                              const D2D1_RECT_F& rect,
                                                                              const D2D1_COLOR_F (&colors)[3]) noexcept
{
    PopupGradient fresh{};
    if (! EnsurePopupGradient(dc, fresh, rect, colors))
        return {};
    return std::move(fresh.brush);
}
} // namespace

void Detail::PopupSurfaceCacheDeleter::operator()(PopupSurfaceCache* cache) const noexcept
{
    delete cache;
}

void PopupSurfaceCache::Release() noexcept
{
    *this = PopupSurfaceCache{};
}

size_t PopupSurfaceCache::Entries() const noexcept
{
    const auto present = [](const auto& object) noexcept { return object ? size_t{1u} : size_t{0u}; };
    return present(passes[0].mask) + present(passes[0].effect) + present(passes[1].mask) + present(passes[1].effect) + present(shadowBitmap) +
           present(backdrop.clipGeometry) + present(backdrop.blurredBitmap) + present(glaze.stops) + present(glaze.brush) + present(shade.stops) +
           present(shade.brush);
}

uint64_t PopupSurfaceCache::ResidentBytes() const noexcept
{
    return PixelBytes(shadowBitmap.get()) + PixelBytes(backdrop.blurredBitmap.get());
}

void PaintPopupMaterialSurface(ControlHost& host,
                               PopupSurfaceCache& cache,
                               ID2D1Bitmap1* backdrop,
                               const D2D1_RECT_F& rect,
                               float cornerRadiusDip,
                               const PopupMaterialStyle& material,
                               PopupShadowTarget shadowTarget) noexcept
{
    ID2D1DeviceContext* const dc = host.GetDeviceContext();
    if (! dc)
        return;

    const D2D1_RECT_F shadowTargetRect   = InflateRect(rect, -material.shadowInsetDip, -material.shadowInsetDip);
    const D2D1_ROUNDED_RECT rounded      = D2D1::RoundedRect(rect, cornerRadiusDip, cornerRadiusDip);
    const D2D1_RECT_F innerRect          = InflateRect(rect, -1.0f, -1.0f);
    const D2D1_ROUNDED_RECT innerRounded = D2D1::RoundedRect(innerRect, (std::max)(0.0f, cornerRadiusDip - 1.0f), (std::max)(0.0f, cornerRadiusDip - 1.0f));
    const float shadowRadiusDip          = (std::max)(0.0f, cornerRadiusDip - material.shadowInsetDip);
    const bool hasBackdrop               = backdrop && material.backdropOpacity > 0.0f && material.backdropBlurDip > 0.0f;
    const D2D1_COLOR_F glazeColors[3]    = {
        material.glazeTop, material.glazeMid, D2D1::ColorF(material.glazeMid.r, material.glazeMid.g, material.glazeMid.b, 0.0f)};
    const D2D1_COLOR_F shadeColors[3] = {D2D1::ColorF(material.bottomShade.r, material.bottomShade.g, material.bottomShade.b, 0.0f),
                                         D2D1::ColorF(material.bottomShade.r, material.bottomShade.g, material.bottomShade.b, material.bottomShade.a * 0.35f),
                                         material.bottomShade};
    wil::com_ptr<ID2D1LinearGradientBrush> freshGlaze;
    wil::com_ptr<ID2D1LinearGradientBrush> freshShade;
    ID2D1LinearGradientBrush* glaze = nullptr;
    ID2D1LinearGradientBrush* shade = nullptr;

    // The code before W2.4: every object made for this paint.
    const auto paintPerPaint = [&]() noexcept
    {
        DrawDropShadow(host,
                       shadowTargetRect,
                       shadowRadiusDip,
                       material.shadowYOffsetDip,
                       material.shadowSpreadDip,
                       material.shadowOuterOpacity,
                       material.shadowInnerOpacity);
        if (hasBackdrop)
        {
            PopupBackdropState fresh{};
            PaintPopupBackdropLayer(host,
                                    *dc,
                                    fresh,
                                    backdrop,
                                    rect,
                                    cornerRadiusDip,
                                    material.backdropOpacity,
                                    material.backdropBlurDip,
                                    material.backdropDetailOpacity,
                                    false,
                                    true);
        }
        DrawRoundedRect(host, rect, material.baseFill, D2D1::ColorF(0, 0, 0, 0), cornerRadiusDip);
        freshGlaze = CreatePopupGradientBrush(*dc, rect, glazeColors);
        glaze      = freshGlaze.get();
        if (glaze)
            dc->FillRoundedRectangle(rounded, glaze);
        freshShade = CreatePopupGradientBrush(*dc, rect, shadeColors);
        shade      = freshShade.get();
    };

    // The oracle, and a reference paint before anything was kept, draw the per-paint way and touch nothing retained.
    // The paint-region verifier's reference paint (W4.10) draws from what the frame kept, read-only (its reference means regions
    // and culling off, not retention off): only when everything is there, so it makes, replaces or ages nothing.
    const bool referenceFromRetained =
        Detail::g_paintVerificationActive && cache.device && cache.passes[0].effect && cache.passes[1].effect && cache.glaze.brush && cache.shade.brush &&
        (! hasBackdrop || (cache.backdrop.blurredBitmap && cache.backdrop.blurSource.get() == backdrop && cache.backdrop.clipGeometry &&
                           SameRoundedRect(cache.backdrop.clipShape, D2D1::RoundedRect(rect, cornerRadiusDip, cornerRadiusDip))));
    if (referenceFromRetained)
    {
        DrawPreparedPopupShadow(*dc, cache, material.shadowYOffsetDip, hasBackdrop ? PopupShadowTarget::OverClearedSurface : shadowTarget);
        if (hasBackdrop)
        {
            PaintPopupBackdropLayer(host,
                                    *dc,
                                    cache.backdrop,
                                    backdrop,
                                    rect,
                                    cornerRadiusDip,
                                    material.backdropOpacity,
                                    material.backdropBlurDip,
                                    material.backdropDetailOpacity,
                                    true,
                                    false);
        }
        DrawRoundedRect(host, rect, material.baseFill, D2D1::ColorF(0, 0, 0, 0), cornerRadiusDip);
        glaze = cache.glaze.brush.get();
        shade = cache.shade.brush.get();
        dc->FillRoundedRectangle(rounded, glaze);
    }
    else if (Detail::ArePopupSurfacesUncachedForTest() || Detail::g_paintVerificationActive)
    {
        paintPerPaint();
    }
    else
    {
        // Everything retained was made on one device context: another one (device loss, replacement) starts again.
        wil::com_ptr<ID2D1Device> device;
        dc->GetDevice(device.put());
        if (cache.device.get() != device.get() || cache.context != dc)
        {
            cache.Release();
            cache.device  = std::move(device);
            cache.context = dc;
        }
        float dpiX = 96.0f;
        float dpiY = 96.0f;
        dc->GetDpi(&dpiX, &dpiY);
        if (cache.dpi != dpiX)
        {
            cache.shadowBitmap.reset();
            cache.dpi = dpiX;
        }
        // Everything is made (or kept) before the surface draws, then a frame that had to make anything draws the per-paint
        // way: making objects records masks and switches targets, which changes how Direct2D batches and blends the draws
        // around it (a level on a few pixels). A frame that made nothing draws only from what is retained, and draws exactly
        // the per-paint pixels.
        // Over a cleared surface the shadow is drawn from its bitmap, except under a blurred backdrop: the blur drawn after the
        // bitmap differs from the blur drawn after the passes by a level on a pixel, so there the passes run.
        const PopupShadowTarget shadowFromBitmap = hasBackdrop ? PopupShadowTarget::OverClearedSurface : shadowTarget;
        const uint64_t createdBefore             = PopupResourceCreations();
        const bool shadowReady                   = PreparePopupShadow(host,
                                                                      *dc,
                                                                      cache,
                                                                      shadowTargetRect,
                                                                      shadowRadiusDip,
                                                                      material.shadowYOffsetDip,
                                                                      material.shadowSpreadDip,
                                                                      material.shadowOuterOpacity,
                                                                      material.shadowInnerOpacity,
                                                                      shadowFromBitmap);
        if (hasBackdrop)
        {
            static_cast<void>(EnsurePopupClipGeometry(*dc, cache.backdrop, D2D1::RoundedRect(rect, cornerRadiusDip, cornerRadiusDip)));
            EnsurePopupBlurredBackdrop(host, *dc, cache.backdrop, backdrop, material.backdropBlurDip);
        }
        glaze = EnsurePopupGradient(*dc, cache.glaze, rect, glazeColors);
        shade = EnsurePopupGradient(*dc, cache.shade, rect, shadeColors);
        // A blurred popup draws from what it keeps in every paint, its first included, so that every frame (whole or a region)
        // draws the same blur and shadow.
        if (! shadowReady || (PopupResourceCreations() != createdBefore && ! hasBackdrop))
        {
            glaze = nullptr;
            shade = nullptr;
            paintPerPaint();
        }
        else
        {
            DrawPreparedPopupShadow(*dc, cache, material.shadowYOffsetDip, shadowFromBitmap);
            if (hasBackdrop)
            {
                PaintPopupBackdropLayer(host,
                                        *dc,
                                        cache.backdrop,
                                        backdrop,
                                        rect,
                                        cornerRadiusDip,
                                        material.backdropOpacity,
                                        material.backdropBlurDip,
                                        material.backdropDetailOpacity,
                                        true,
                                        false);
            }
            DrawRoundedRect(host, rect, material.baseFill, D2D1::ColorF(0, 0, 0, 0), cornerRadiusDip);
            if (glaze)
                dc->FillRoundedRectangle(rounded, glaze);
        }
    }
    if (shade)
        dc->FillRoundedRectangle(rounded, shade);

    if (auto* const topRimBrush = host.GetSolidBrush(material.topRim))
    {
        const float rimY = rect.top + 1.0f;
        dc->DrawLine(D2D1::Point2F(rect.left + cornerRadiusDip - 1.0f, rimY), D2D1::Point2F(rect.right - cornerRadiusDip + 1.0f, rimY), topRimBrush, 1.0f);
    }
    if (auto* const outerBorderBrush = host.GetSolidBrush(material.outerBorder))
        dc->DrawRoundedRectangle(rounded, outerBorderBrush, 1.0f);
    if (auto* const innerBorderBrush = host.GetSolidBrush(material.innerBorder))
        dc->DrawRoundedRectangle(innerRounded, innerBorderBrush, 1.0f);
}

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
        // W6.3: work left below it while hidden is reached again (its marks stayed); a shown control is entered once more.
        LayoutScopeAccess::MarkPending(*this);
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

void ControlModelQueryAccess::InvalidateHitTestForScroll(Control& control) noexcept
{
    if (ControlHost* const host = control._host)
    {
        const uint64_t hitBefore         = host->_hitRevision;
        const uint64_t interactionBefore = host->_interactionRevision;
        host->AdvanceHitRevision();
        NoteAccessibilityScrollMove(control, interactionBefore, hitBefore);
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
        // W6.3: its ancestors wait for the cutoff check before the next preparation or measurement reads them.
        LayoutScopeAccess::InvalidateMeasureOfAssignment(*this);
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
        {
            // A Selector button's ring is drawn at its 6 DIP corner radius, and DrawRawFocusRing scales the ring's offset and
            // stroke by radius / 4: the outer stroke's outside edge lies 3 x 1.5 = 4.5 DIP out (the review's E1).
            const auto* button = dynamic_cast<const Button*>(this);
            if (button && button->GetVariant() == ButtonVariant::Selector)
            {
                constexpr float reach = 3.0f * (6.0f / kButtonCornerRadiusDip);
                return {reach, reach, reach, reach};
            }
            break;
        }
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
    // TagPicker draws its field's frame in the hover colours of its inner ComboBox (the review's B1; the sweep found no other
    // library Paint that reads another control's hover).
    const bool paintsInnerHover = dynamic_cast<const TagPicker*>(&control) != nullptr;
    control._paintFlags         = static_cast<uint8_t>((control._paintFlags & ~(kDeclared | kClipsText | kPaintsInnerHover)) | (declared ? kDeclared : 0u) |
                                                       (clipsText ? kClipsText : 0u) | (paintsInnerHover ? kPaintsInnerHover : 0u));
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
    bool overlay     = kind == PaintKind::PopupLayer;
    // A ScrollPanel's content is drawn translated by its offset: its overlay content (below) is mapped by it.
    const float offset = kind == PaintKind::ScrollPanel ? static_cast<const ScrollPanel&>(control).GetScrollOffset() : 0.0f;
    for (const std::unique_ptr<Control>& child : panel._children)
    {
        if (unbounded)
            break;
        if (! child || ! child->_visible)
            continue;
        D2D1_RECT_F childExtent{};
        if (! Extent(host, *child, childExtent))
        {
            unbounded = true;
            break;
        }
        const PaintKind childKind = Kind(*child);
        const bool childOverlay   = (childKind == PaintKind::Panel || childKind == PaintKind::ScrollPanel || childKind == PaintKind::PopupLayer) &&
                                    static_cast<const Panel&>(*child)._paintExtent.overlay;
        overlay                   = overlay || childOverlay;
        if (IsEmptyExtent(childExtent))
            continue;
        // A ScrollPanel clips its content to its viewport, except overlay content (a PopupLayer's children, drawn by the overlay
        // pass through no viewport): a child that holds some extends it, at the content's offset.
        if (kind != PaintKind::ScrollPanel)
            rect = UnionRect(rect, childExtent);
        else if (childOverlay)
            rect = UnionRect(rect, D2D1::RectF(childExtent.left, childExtent.top - offset, childExtent.right, childExtent.bottom - offset));
    }
    cached.rect      = rect;
    cached.unbounded = unbounded;
    cached.overlay   = overlay;
    cached.topology  = host._paintTopologyRevision;
    cached.valid     = true;
    extent           = rect;
    return ! unbounded;
}

bool PaintExtentAccess::MapToView(const ControlHost& host, const Control* ancestor, bool overlay, D2D1_RECT_F& rect) noexcept
{
    for (; ancestor; ancestor = ancestor->_parent)
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
    rect =
        D2D1::RectF((std::max)(rect.left, view.left), (std::max)(rect.top, view.top), (std::min)(rect.right, view.right), (std::min)(rect.bottom, view.bottom));
    return true;
}

bool PaintExtentAccess::ViewExtent(const ControlHost& host, const Control& control, bool subtree, D2D1_RECT_F& extent) noexcept
{
    if (! Detail::PaintRegionsEnabled() || control._host != &host)
        return false;
    D2D1_RECT_F rect{};
    if (! (subtree ? Extent(host, control, rect) : OwnExtent(host, control, rect)))
        return false;
    // Overlay content (a PopupLayer's children) is not clipped by the viewports it is drawn through. A panel that holds some
    // has its overlay content in its extent already (Extent), and the viewports above it must not clip that part either: its
    // subtree extent is mapped as overlay (conservative: its other content is not clipped by them either).
    const PaintKind kind = Kind(control);
    const bool overlay   = kind == PaintKind::PopupLayer ||
                           (subtree && (kind == PaintKind::Panel || kind == PaintKind::ScrollPanel) && static_cast<const Panel&>(control)._paintExtent.overlay);
    if (! MapToView(host, control._parent, overlay, rect))
        return false;
    extent = rect;
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

void PaintExtentAccess::InvalidateInk(ControlHost& host, const Control& control, const D2D1_RECT_F& inkDip) noexcept
{
    D2D1_RECT_F own{};
    if (! Detail::PaintRegionsEnabled() || control._host != &host || (control._paintFlags & kClipsText) == 0u || inkDip.right < inkDip.left ||
        inkDip.bottom < inkDip.top || ! OwnExtent(host, control, own))
    {
        Invalidate(host, control);
        return;
    }
    // What the control draws lies inside its own extent; the ink is mapped through its ancestors as the extent is.
    D2D1_RECT_F rect = D2D1::RectF(
        (std::max)(inkDip.left, own.left), (std::max)(inkDip.top, own.top), (std::min)(inkDip.right, own.right), (std::min)(inkDip.bottom, own.bottom));
    if (! IsEmptyExtent(rect) && ! MapToView(host, control._parent, false, rect))
    {
        Invalidate(host, control);
        return;
    }
    if (! host._paintRegion.full && ! IsEmptyExtent(rect))
        AddDip(host, rect);
    host.RequestRegionFrame();
}

void PaintExtentAccess::AddHoverChange(ControlHost& host, const Control& control) noexcept
{
    AddToRegion(host, control, true);
    for (const Control* ancestor = control._parent; ancestor; ancestor = ancestor->_parent)
        if ((ancestor->_paintFlags & kPaintsInnerHover) != 0u)
            AddToRegion(host, *ancestor, false);
}

void PaintExtentAccess::ScrollMoved(const ScrollPanel& panel, float previousOffset) noexcept
{
    ControlHost* const host = panel._host;
    if (! host)
        return;
    // Its cached extent holds its overlay content at the old offset; every panel above it is stale with it.
    ExtentChanged(panel);
    if (host->_paintRegion.full || ! Detail::PaintRegionsEnabled())
        return;
    // The overlay content below it, in its content coordinates. Its viewport content is in its own extent, which the scroll's
    // own invalidation adds; overlay content is drawn wherever the offset puts it.
    D2D1_RECT_F content = D2D1::RectF();
    bool overlay        = false;
    for (const std::unique_ptr<Control>& child : panel._children)
    {
        if (! child || ! child->_visible)
            continue;
        const PaintKind childKind = Kind(*child);
        if (childKind != PaintKind::Panel && childKind != PaintKind::ScrollPanel && childKind != PaintKind::PopupLayer)
            continue;
        D2D1_RECT_F childExtent{};
        if (! Extent(*host, *child, childExtent))
        {
            MarkFull(*host);
            return;
        }
        if (static_cast<const Panel&>(*child)._paintExtent.overlay && ! IsEmptyExtent(childExtent))
        {
            content = overlay ? UnionRect(content, childExtent) : childExtent;
            overlay = true;
        }
    }
    if (! overlay)
        return;
    for (const float offset : {previousOffset, panel.GetScrollOffset()})
    {
        D2D1_RECT_F rect = D2D1::RectF(content.left, content.top - offset, content.right, content.bottom - offset);
        if (! MapToView(*host, panel._parent, true, rect))
        {
            MarkFull(*host);
            return;
        }
        if (! IsEmptyExtent(rect))
            AddDip(*host, rect);
    }
}

float PaintExtentAccess::PixelScale(const ControlHost& host) noexcept
{
    return (host._paintDpi > 0.0f ? host._paintDpi : host.GetDpi()) / 96.0f;
}

D2D1_RECT_F PaintExtentAccess::SnapOutward(const ControlHost& host, const D2D1_MATRIX_3X2_F& transform, const D2D1_RECT_F& rect) noexcept
{
    const float scale     = PixelScale(host);
    const D2D1_RECT_F box = TransformBox(transform, rect);
    const D2D1_RECT_F px  = D2D1::RectF(std::floor(box.left * scale), std::floor(box.top * scale), std::ceil(box.right * scale), std::ceil(box.bottom * scale));
    // Back through the translation and scale of the transform (it neither rotates nor skews).
    const float sx = transform._11 != 0.0f ? transform._11 : 1.0f;
    const float sy = transform._22 != 0.0f ? transform._22 : 1.0f;
    const float x0 = (px.left / scale - transform._31) / sx, x1 = (px.right / scale - transform._31) / sx;
    const float y0 = (px.top / scale - transform._32) / sy, y1 = (px.bottom / scale - transform._32) / sy;
    return D2D1::RectF((std::min)(x0, x1), (std::min)(y0, y1), (std::max)(x0, x1), (std::max)(y0, y1));
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
    // beyond the pixels a shape's geometry touches, and a text extent is its glyphs' ink (overhang metrics). The scale is the one
    // the frame paints with (an embedded view's float DPI), so the rectangle is the pixels the paint clip later covers.
    const float scale  = PixelScale(host);
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
    // The clip's pixels must each lie wholly inside the rectangle, where an antialiased clip to it covers them fully. That holds
    // only while the paint clip is whole pixels (a region rectangle, the surface): after an outer viewport narrowed it to a
    // fractional edge, its edge pixels are partly covered, and an inner antialiased clip covers them less again (the review's
    // D1), so the inner clip is pushed.
    const float scale      = PixelScale(host);
    const D2D1_RECT_F box  = TransformBox(transform, rect);
    const D2D1_RECT_F clip = host._paintClipDip;
    const auto pixel       = [scale](float dip, float& snapped) noexcept
    {
        const float px = dip * scale;
        snapped        = std::round(px);
        return std::abs(px - snapped) <= 1.0f / 1024.0f;
    };
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    if (! pixel(clip.left, left) || ! pixel(clip.top, top) || ! pixel(clip.right, right) || ! pixel(clip.bottom, bottom))
        return false;
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
    host._paintClipBounded = Detail::PaintRegionsEnabled() && ! Detail::g_paintVerificationActive;
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
    // The frame verifier's reference paint keeps no ink escape: a control the frame culled must not change state through it.
    if (Detail::g_paintVerificationActive) [[unlikely]]
    {
        host._paintingLimitBounded = false;
        return;
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

PaintExtentAccess::OverlayScope::OverlayScope(ControlHost& overlayHost, const Control& control) noexcept : host(overlayHost)
{
    if (! IsDeclared(control) && host._paintClipBounded)
    {
        clipSuspended          = true;
        host._paintClipBounded = false;
    }
}

PaintExtentAccess::OverlayScope::~OverlayScope()
{
    if (clipSuspended)
        host._paintClipBounded = true;
}

#if DXUI_ENABLE_DIAGNOSTICS
PaintExtentAccess::VerificationScope::VerificationScope(ControlHost& host) noexcept
    : _host(host),
      _work(Detail::Work()),
      _region(host._paintRegion),
      _inkEscapedNew(host._paintInkEscapedNew),
      _brushAges(),
      _formatAges(),
      _layouts(),
      _transitionBrushes(0u),
      _transitionBrushesUsed(host._transitionBrushesUsed)
{
    _observer = Detail::GetPaintObserverForTest(_observerContext);
    Detail::SetPaintObserverForTest(nullptr, nullptr);
    // Swapped, not copied: the reference paint finds empty caches and fills its own, which the end releases.
    _brushAges.reserve(host._brushCache.size());
    for (const auto& [key, entry] : host._brushCache)
        _brushAges.emplace_back(key, entry.lastUse);
    std::ranges::sort(_brushAges);
    _formatAges.reserve(host._configuredTextFormats.size());
    for (const auto& [key, entry] : host._configuredTextFormats)
        _formatAges.emplace_back(key, entry.lastUse);
    _layouts.swap(host._textLayoutCache);
    _transitionBrushes.swap(host._transitionBrushes);
    host._transitionBrushesUsed       = 0u;
    Detail::g_paintVerificationActive = true;
}

PaintExtentAccess::VerificationScope::~VerificationScope()
{
    Detail::g_paintVerificationActive = false;
    std::erase_if(_host._brushCache,
                  [this](auto& entry)
    {
        const auto found = std::ranges::lower_bound(_brushAges, entry.first, {}, &std::pair<uint32_t, uint64_t>::first);
        if (found == _brushAges.end() || found->first != entry.first)
            return true;
        entry.second.lastUse = found->second;
        return false;
    });
    for (const auto& [key, lastUse] : _formatAges)
        if (const auto found = _host._configuredTextFormats.find(key); found != _host._configuredTextFormats.end())
            found->second.lastUse = lastUse;
    _host._textLayoutCache.swap(_layouts);
    _host._transitionBrushes.swap(_transitionBrushes);
    _host._transitionBrushesUsed = _transitionBrushesUsed;
    _host._paintRegion           = _region;
    _host._paintInkEscapedNew    = _inkEscapedNew;
    Detail::SetPaintObserverForTest(_observer, _observerContext);
    Detail::Work() = _work;
}
#endif

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

void InvalidateCaretFlip(ControlHost& host, const Control& control, const D2D1_RECT_F* inkDip) noexcept
{
    const AttestedInvalidation reported(&host, ReportsOwnAccessibilityChanges(control));
    if (inkDip)
        PaintExtentAccess::InvalidateInk(host, control, *inkDip);
    else
        PaintExtentAccess::Invalidate(host, control);
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
            LayoutScopeAccess::MarkPending(*control);
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

void LayoutScopeAccess::InvalidateMeasureOfAssignment(Control& control) noexcept
{
    ControlHost* const host = control._host;
    // A detached control, a root, or one with no intrinsic owner above it has no layout to spare: today's eager path.
    bool ownerAbove = false;
    for (const Control* ancestor = control._parent; ancestor && ! ownerAbove; ancestor = ancestor->_parent)
        ownerAbove = ancestor->_intrinsicLayoutEnabled;
    if (! host || ! ownerAbove || ! Detail::LayoutScopePruningEnabled())
    {
        control.InvalidateMeasure();
        return;
    }
    if ((control._layoutFlags & kMeasureDeferred) == 0u)
    {
        try
        {
            if (host->_deferredMeasures.capacity() == 0u)
                host->_deferredMeasures.reserve(kDeferredMeasureCapacity);
            if (host->_deferredMeasures.size() >= kDeferredMeasureCapacity)
            {
                control.InvalidateMeasure();
                return;
            }
            host->_deferredMeasures.push_back({&control, GetControlLifetimeToken(control)});
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
        static_cast<void>(TakeMeasureSnapshot(control));
        control._layoutFlags = static_cast<uint8_t>(control._layoutFlags | kMeasureDeferred);
    }
    ++Detail::Work().layout.measureInvalidations;
    ++control._measureRevision;
    if (control._intrinsicLayoutEnabled)
    {
        control._intrinsicAllocationRetryUsed = false;
        control._intrinsicLayoutPending       = true;
        MarkPending(control);
    }
    // The resolution runs at the next preparation: ask for one, as the eager path does for a control below an owner.
    host->_pendingIntrinsicLayout = true;
    PaintExtentAccess::Invalidate(*host, control);
}

void LayoutScopeAccess::Resolve(ControlHost& host) noexcept
{
    if (host._resolvingDeferredMeasures || host._deferredMeasures.empty())
        return;
    host._resolvingDeferredMeasures = true;
    const auto done                 = wil::scope_exit([&host]() noexcept { host._resolvingDeferredMeasures = false; });
    // Each entry is taken out before it is resolved: a measurement may run code that queues again (it then resolves eagerly,
    // since the flag is set, through the next resolution of this loop).
    while (! host._deferredMeasures.empty())
    {
        ControlHost::DeferredMeasure entry = std::move(host._deferredMeasures.back());
        host._deferredMeasures.pop_back();
        if (entry.lifetime.expired() || ! entry.control)
            continue;
        Control* control = entry.control;
        if ((control->_layoutFlags & kMeasureDeferred) == 0u)
            continue;
        control->_layoutFlags = static_cast<uint8_t>(control->_layoutFlags & ~kMeasureDeferred);
        if (control->_host != &host)
            continue; // it moved: PropagateHost invalidated it eagerly
        // Level by level: a control whose parent never reads its measure, or whose results are unchanged under every
        // constraint it was asked, stops the propagation; otherwise its parent's revision moves and the parent is checked.
        while (control)
        {
            std::weak_ptr<int> lifetime = GetControlLifetimeToken(*control);
            if (Detail::LayoutCutoffUncheckedForTest() || IsArrangedAtDeclaredSize(*control) || MeasureUnchanged(*control, host))
            {
                ++Detail::Work().layout.measureCutoffs;
                break;
            }
            if (lifetime.expired() || control->_host != &host)
                break;
            Control* const parent = control->_parent;
            if (! parent)
                break;
            const bool snapshot = TakeMeasureSnapshot(*parent);
            ++parent->_measureRevision;
            if (parent->_intrinsicLayoutEnabled)
            {
                parent->_intrinsicAllocationRetryUsed = false;
                parent->_intrinsicLayoutPending       = true;
                MarkPending(*parent);
                host._pendingIntrinsicLayout = true;
            }
            if (! snapshot)
            {
                // No complete record of what the parent was asked: everything above it is invalidated as before.
                if (Control* const grandparent = parent->_parent)
                    grandparent->InvalidateMeasure();
                break;
            }
            control = parent;
        }
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
        LayoutScopeAccess::MarkPending(*this);
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
    LayoutScopeAccess::MarkPending(*this);
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
    LayoutScopeAccess::MarkPending(*this);
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
    // W6.3: a queued invalidation belonged to the previous host (the eager InvalidateMeasure below replaces it), and work
    // marked below this control is reached from its new ancestors.
    LayoutScopeAccess::Forget(*this);
    if (LayoutScopeAccess::IsPending(*this) && _parent)
    {
        LayoutScopeAccess::ClearPending(*this);
        LayoutScopeAccess::MarkPending(*this);
    }
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
