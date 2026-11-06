#pragma once

#include <DxUi/Failure.h>
#include <d2d1.h>

namespace DxUi::Detail
{
// EndDraw can execute deferred DirectWrite work. Keep the native allowance
// limited to the ABI call, while preserving fatal handling for escaping throws.
[[nodiscard]] inline HRESULT EndDrawNative(ID2D1RenderTarget* target, D2D1_TAG* tag1 = nullptr, D2D1_TAG* tag2 = nullptr) noexcept
{
    return Failure::InvokeNative(+[](ID2D1RenderTarget* nativeTarget, D2D1_TAG* first, D2D1_TAG* second) -> HRESULT {
        return nativeTarget->EndDraw(first, second);
    }, target, tag1, tag2);
}
} // namespace DxUi::Detail
