#pragma once

// Counted creation helpers (Specs/Core/Core_PerformanceAndResources.md, "Work counters"). Library code creates the
// counted native resources only through these: each forwards to the named method of its receiver unchanged and, when the
// method succeeds with an object, counts it in Detail::Work().resources. Tools/tests/Test-CountedCreation.ps1 fails on any
// other use of a counted method name under src, whatever the receiver (`->`, `.`, `::` or a function pointer).
//
// A call site replaces `receiver->CreateTextLayout(` with `CountedCreateTextLayout(receiver, `; a receiver held by
// reference passes its address. The created object is the last argument, as in every counted method.

#include "WorkCounters.h"

#include <DxUi/Typography.h>
#include <d2d1_1.h>
#include <tuple>
#include <type_traits>
#include <utility>

namespace DxUi
{
namespace Detail
{
template <typename... Args> void NoteCreatedOutput(ResourceKind kind, const Args&... args) noexcept
{
    static_assert(sizeof...(Args) != 0u, "a counted creation method returns its object through its last argument");
    const auto& output = std::get<sizeof...(Args) - 1u>(std::forward_as_tuple(args...));
    using Output       = std::remove_cvref_t<decltype(output)>;
    static_assert(std::is_pointer_v<Output> && std::is_pointer_v<std::remove_pointer_t<Output>>, "the last argument receives a COM interface pointer");
    using Interface = std::remove_cv_t<std::remove_pointer_t<std::remove_pointer_t<Output>>>;
    static_assert(std::is_base_of_v<IUnknown, Interface>, "a counted resource is a COM object");
    if (output)
        NoteResourceCreated(kind, *output);
}
} // namespace Detail

#define DXUI_DEFINE_COUNTED_CREATE(Method, Kind)                                                                                                               \
    template <typename Receiver, typename... Args> [[nodiscard]] HRESULT Counted##Method(Receiver&& receiver, Args&&... args) noexcept                         \
    {                                                                                                                                                          \
        const HRESULT hr = receiver->Method(args...);                                                                                                          \
        if (SUCCEEDED(hr))                                                                                                                                     \
            ::DxUi::Detail::NoteCreatedOutput(::DxUi::Detail::ResourceKind::Kind, args...);                                                                    \
        return hr;                                                                                                                                             \
    }

// DirectWrite.
DXUI_DEFINE_COUNTED_CREATE(CreateTextFormat, textFormat)
DXUI_DEFINE_COUNTED_CREATE(CreateTextLayout, textLayout)
DXUI_DEFINE_COUNTED_CREATE(CreateGdiCompatibleTextLayout, textLayout)
DXUI_DEFINE_COUNTED_CREATE(CreateMonitorRenderingParams, renderingParams)
DXUI_DEFINE_COUNTED_CREATE(CreateCustomRenderingParams, renderingParams)
// Direct2D device-independent resources.
DXUI_DEFINE_COUNTED_CREATE(CreatePathGeometry, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateRectangleGeometry, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateRoundedRectangleGeometry, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateEllipseGeometry, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateGeometryGroup, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateTransformedGeometry, geometry)
DXUI_DEFINE_COUNTED_CREATE(CreateStrokeStyle, strokeStyle)
// Direct2D device resources.
DXUI_DEFINE_COUNTED_CREATE(CreateDeviceContext, deviceContext)
DXUI_DEFINE_COUNTED_CREATE(CreateSolidColorBrush, brush)
DXUI_DEFINE_COUNTED_CREATE(CreateLinearGradientBrush, brush)
DXUI_DEFINE_COUNTED_CREATE(CreateRadialGradientBrush, brush)
DXUI_DEFINE_COUNTED_CREATE(CreateBitmapBrush, brush)
DXUI_DEFINE_COUNTED_CREATE(CreateImageBrush, brush)
DXUI_DEFINE_COUNTED_CREATE(CreateGradientStopCollection, gradientStops)
DXUI_DEFINE_COUNTED_CREATE(CreateBitmap, bitmap)
DXUI_DEFINE_COUNTED_CREATE(CreateBitmapFromDxgiSurface, bitmap)
DXUI_DEFINE_COUNTED_CREATE(CreateBitmapFromWicBitmap, bitmap)
DXUI_DEFINE_COUNTED_CREATE(CreateSharedBitmap, bitmap)
DXUI_DEFINE_COUNTED_CREATE(CreateEffect, effect)
DXUI_DEFINE_COUNTED_CREATE(CreateCommandList, commandList)
DXUI_DEFINE_COUNTED_CREATE(CreateLayer, layer)
#undef DXUI_DEFINE_COUNTED_CREATE

// Typography::CreateTextFormat (public, include/DxUi/Typography.h) creates one text format, possibly after a refused
// first family; count what it returns.
[[nodiscard]] inline HRESULT CountedCreateTypographyTextFormat(IDWriteFactory* factory,
                                                               const Typography::Spec& spec,
                                                               IDWriteTextFormat** format,
                                                               PCWSTR localeName = L"") noexcept
{
    const HRESULT hr = Typography::CreateTextFormat(factory, spec, format, localeName);
    if (SUCCEEDED(hr) && format)
        Detail::NoteResourceCreated(Detail::ResourceKind::textFormat, *format);
    return hr;
}

// DrawText lays out its whole string on every call: one implicit text layout, created and released inside it.
template <typename Receiver, typename... Args> void CountedDrawText(Receiver&& receiver, Args&&... args) noexcept
{
    Detail::NoteImplicitTextLayout();
    receiver->DrawTextW(args...);
}

// A synchronous GDI capture of the screen behind a popup.
[[nodiscard]] inline BOOL CountedBitBlt(HDC destination, int x, int y, int width, int height, HDC source, int sourceX, int sourceY, DWORD operation) noexcept
{
    auto& popups = Detail::Work().popups;
    ++popups.backdropCaptures;
    popups.backdropCapturePixels += static_cast<uint64_t>(width > 0 ? width : 0) * static_cast<uint64_t>(height > 0 ? height : 0);
    return ::BitBlt(destination, x, y, width, height, source, sourceX, sourceY, operation);
}
} // namespace DxUi
