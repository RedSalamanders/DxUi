#pragma once

#include <DxUi/Failure.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <span>
#include <wil/com.h>

namespace DxUi::Detail
{
// Private creation seam for the Direct2D and DirectWrite factories a host creates (work counters). A test installs an
// interposer on its thread to hand each new factory to its counting wrappers and use what they return; with none installed,
// which is always the case outside test executables, the helpers below return the system factory unchanged.
struct FactoryInterposer final
{
    void* context                                                                                     = nullptr;
    HRESULT (*d2dFactory)(void* context, ID2D1Factory1* created, ID2D1Factory1** result) noexcept     = nullptr;
    HRESULT (*writeFactory)(void* context, IDWriteFactory* created, IDWriteFactory** result) noexcept = nullptr;
};
inline thread_local constinit const FactoryInterposer* g_factoryInterposer = nullptr;

// A single-threaded ID2D1Factory1, through the installed interposer if any.
[[nodiscard]] inline HRESULT CreateHostD2DFactory(const D2D1_FACTORY_OPTIONS& options, ID2D1Factory1** factory) noexcept
{
    *factory = nullptr;
    wil::com_ptr_nothrow<ID2D1Factory1> created;
    const HRESULT hr = ::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, created.put_void());
    if (FAILED(hr))
        return hr;
    if (const FactoryInterposer* const interposer = g_factoryInterposer; interposer && interposer->d2dFactory)
        return interposer->d2dFactory(interposer->context, created.get(), factory);
    *factory = created.detach();
    return S_OK;
}

// The shared IDWriteFactory, through the installed interposer if any.
[[nodiscard]] inline HRESULT CreateHostWriteFactory(IDWriteFactory** factory) noexcept
{
    *factory = nullptr;
    wil::com_ptr_nothrow<IDWriteFactory> created;
    const HRESULT hr = ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), created.put_unknown());
    if (FAILED(hr))
        return hr;
    if (const FactoryInterposer* const interposer = g_factoryInterposer; interposer && interposer->writeFactory)
        return interposer->writeFactory(interposer->context, created.get(), factory);
    *factory = created.detach();
    return S_OK;
}

// Private host helper. The factory seam permits deterministic native-ABI
// failure tests without a device, driver or process-wide replacement hook.
using D3D11DeviceCreator = decltype(&::D3D11CreateDevice);
// The system creator. Every device a creator returns is counted by CreateD3D11DeviceWithWarpFallback (work counters).
[[nodiscard]] inline D3D11DeviceCreator SystemD3D11DeviceCreator() noexcept
{
    return &::D3D11CreateDevice;
}
[[nodiscard]] HRESULT CreateD3D11DeviceWithWarpFallback(UINT creationFlags,
                                                        std::span<const D3D_FEATURE_LEVEL> featureLevels,
                                                        bool forceWarp,
                                                        ID3D11Device** device,
                                                        D3D_FEATURE_LEVEL* featureLevel,
                                                        ID3D11DeviceContext** context,
                                                        D3D_DRIVER_TYPE* driverType,
                                                        D3D11DeviceCreator creator) noexcept;
// EndDraw can execute deferred DirectWrite work. Keep the native allowance
// limited to the ABI call, while preserving fatal handling for escaping throws.
[[nodiscard]] inline HRESULT EndDrawNative(ID2D1RenderTarget* target, D2D1_TAG* tag1 = nullptr, D2D1_TAG* tag2 = nullptr) noexcept
{
    return Failure::InvokeNative(+[](ID2D1RenderTarget* nativeTarget, D2D1_TAG* first, D2D1_TAG* second) -> HRESULT {
        return nativeTarget->EndDraw(first, second);
    }, target, tag1, tag2);
}
} // namespace DxUi::Detail
