#pragma once
#include <cstdio>
#include <memory>
#include <windows.h>

#include "Support/PostedPayload.h"
#include <wil/resource.h>

namespace DxUiPostedPayloadIcfRegression
{
struct First final
{
    unsigned* destroyed = nullptr;
    ~First() noexcept
    {
        ++*destroyed;
    }
};
struct Second final
{
    unsigned* destroyed = nullptr;
    ~Second() noexcept
    {
        ++*destroyed;
    }
};
static_assert(sizeof(First) == sizeof(Second));
static_assert(alignof(First) == alignof(Second));

template <bool LegacyApi3, class T> std::unique_ptr<T> Take(HWND window, UINT message, LPARAM token) noexcept
{
    if constexpr (LegacyApi3)
    {
        static_cast<void>(window);
        static_cast<void>(message);
        return DxUi::TakeMessagePayload<T>(token);
    }
    else
    {
        return DxUi::TakeMessagePayload<T>(window, message, token);
    }
}

// Deliberate code-address observation, not synchronization. Keep the annotation
// at the function definition for MSVC ARM64 code generation.
// Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
template <bool LegacyApi3> int Run(bool requireFold)
{
    using Deleter           = void (*)(void*) noexcept;
    Deleter volatile first  = &DxUi::Detail::DestroyPayload<First>;
    Deleter volatile second = &DxUi::Detail::DestroyPayload<Second>;
    const bool folded       = first == second;
    if (requireFold && ! folded)
    {
        std::printf("FAIL: ICF did not fold the actual SDK deleter pair\n");
        return 6;
    }
    constexpr wchar_t ClassName[] = L"DxUi.PayloadIcf.Control";
    const auto instance           = GetModuleHandleW(nullptr);
    WNDCLASSW descriptor{};
    descriptor.lpfnWndProc   = DefWindowProcW;
    descriptor.hInstance     = instance;
    descriptor.lpszClassName = ClassName;
    if (! RegisterClassW(&descriptor))
        return 2;
    const auto unregister = wil::scope_exit([&]() noexcept { static_cast<void>(UnregisterClassW(ClassName, instance)); });
    wil::unique_hwnd window(CreateWindowExW(0, ClassName, L"", 0, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr));
    if (! window)
        return 3;
    static_cast<void>(DxUi::InitPostedPayloadWindow(window.get()));
    const auto drain       = wil::scope_exit([&]() noexcept { static_cast<void>(DxUi::DrainPostedPayloadsForWindow(window.get())); });
    constexpr UINT Message = WM_APP + 0x57u;
    unsigned destroyed     = 0u;
    auto payload           = std::make_unique<First>();
    payload->destroyed     = &destroyed;
    if (! DxUi::PostMessagePayload(window.get(), Message, 0u, std::move(payload)))
        return 4;
    MSG posted{};
    if (! PeekMessageW(&posted, window.get(), Message, Message, PM_REMOVE))
        return 5;
    auto wrong = Take<LegacyApi3, Second>(window.get(), Message, posted.lParam);
    if (wrong)
    {
        // Recover the original object without invoking a destructor for the wrong type.
        std::unique_ptr<First> rescue(reinterpret_cast<First*>(wrong.release()));
        rescue.reset();
        std::printf("FAIL: folded=%d accepted the wrong payload type; original safely destroyed=%u\n", folded, destroyed);
        return 8;
    }
    if constexpr (LegacyApi3)
    {
        // Preserve API 3's existing destructive wrong-type rejection contract.
        if (destroyed != 1u)
            return 9;
        auto retry       = std::make_unique<First>();
        retry->destroyed = &destroyed;
        if (! DxUi::PostMessagePayload(window.get(), Message, 0u, std::move(retry)))
            return 11;
        if (! PeekMessageW(&posted, window.get(), Message, Message, PM_REMOVE))
            return 12;
        auto correct = Take<LegacyApi3, First>(window.get(), Message, posted.lParam);
        if (! correct || destroyed != 1u)
            return 13;
        correct.reset();
        if (destroyed != 2u)
            return 14;
    }
    else
    {
        auto correct = Take<LegacyApi3, First>(window.get(), Message, posted.lParam);
        if (! correct || destroyed != 0u)
            return 9;
        correct.reset();
        if (destroyed != 1u)
            return 10;
    }
    std::printf("PASS: folded=%d rejected the wrong type and preserved the pinned API lifetime contract\n", folded);
    return 0;
}

#pragma warning(pop)
} // namespace DxUiPostedPayloadIcfRegression
