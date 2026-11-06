#pragma once

#include <DxUi/Failure.h>
#include <d2d1.h>
#include <d3d11.h>
#include <span>

namespace DxUi::Detail
{
// Private host helper. The factory seam permits deterministic native-ABI
// failure tests without a device, driver or process-wide replacement hook.
using D3D11DeviceCreator = decltype(&::D3D11CreateDevice);
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
