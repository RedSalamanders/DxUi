#include "../../src/Controls/DxUi.Typography.h"
#include "../../src/Support/WindowMessages.h"
#include "../Support/Support.Tests.PerformanceCapture.h"
#include "Controls.Tests.DxUiFocusEventClient.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/NativeMenuInterop.h>

#include <array>
#include <atomic>
#include <concepts>
#include <cstdint>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

void TestWindowHostCaptionMessagesReachApplicationAndDefaultHandling()
{
    using namespace DxUi;

    std::array<unsigned, 3> delivered{};
    AttachedHostWindow window;
    window.Host().SetTextInputBackend(TextInputBackend::Native);
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"editor text");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 44.0f));
    window.Host().SetRoot(std::move(root));

    for (const bool focusEditor : {false, true})
    {
        window.Host().SetFocusControl(focusEditor ? field : nullptr, false);
        delivered.fill(0u);
        window.SetApplicationMessageHandler([&](UINT message, WPARAM, LPARAM, LRESULT& result)
        {
            const auto index = message == WM_SETTEXT ? 0u : message == WM_GETTEXTLENGTH ? 1u : 2u;
            if (message != WM_SETTEXT && message != WM_GETTEXTLENGTH && message != WM_GETTEXT)
                return false;
            ++delivered[index];
            result = 100 + index;
            return true;
        });
        wchar_t caption[32]{};
        Require(SendMessageW(window.Hwnd(), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"host caption")) == 100 &&
                    SendMessageW(window.Hwnd(), WM_GETTEXTLENGTH, 0, 0) == 101 &&
                    SendMessageW(window.Hwnd(), WM_GETTEXT, std::size(caption), reinterpret_cast<LPARAM>(caption)) == 102,
                "application caption handlers receive all three messages with and without logical editor focus");
        Require(delivered == std::array<unsigned, 3>{1u, 1u, 1u}, "caption handling reaches the application exactly once per message");
        window.SetApplicationMessageHandler({});
        Require(SetWindowTextW(window.Hwnd(), L"host caption") != FALSE && SendMessageW(window.Hwnd(), WM_GETTEXTLENGTH, 0, 0) == 12,
                "default caption handling sets and measures the host title with and without logical editor focus");
        Require(SendMessageW(window.Hwnd(), WM_GETTEXT, std::size(caption), reinterpret_cast<LPARAM>(caption)) == 12 &&
                    std::wstring_view(caption) == L"host caption" && field->GetText() == L"editor text",
                "default caption reads preserve the editor's independent text");
    }
}

void TestDxUiTypographyMapsFontRolesToSegoeUiVariableFamilies()
{
    using namespace DxUi;
    using namespace DxUi::Typography;

    const Spec bodySpec       = GetSpec(FontRole::Body);
    const Spec bodyStrongSpec = GetSpec(FontRole::BodyStrong);
    const Spec listItemSpec   = GetSpec(FontRole::ListItem);
    const Spec smallSpec      = GetSpec(FontRole::Small);
    const Spec headerSpec     = GetSpec(FontRole::Header);
    const Spec titleLargeSpec = GetSpec(FontRole::TitleLarge);
    const Spec displaySpec    = GetSpec(FontRole::Display);
    const Spec iconSpec       = GetSpec(FontRole::Icon);
    const Spec iconLargeSpec  = GetSpec(FontRole::IconLarge);
    const Spec heroIconSpec   = GetSpec(FontRole::HeroIcon);
    const Spec monoSpec       = GetSpec(FontRole::Monospace);

    Require(bodySpec.familyName == kSegoeUiVariableTextFamily, "body role uses Segoe UI Variable Text");
    Require(bodyStrongSpec.familyName == kSegoeUiVariableTextFamily, "body-strong role uses Segoe UI Variable Text");
    Require(listItemSpec.familyName == kSegoeUiVariableSmallFamily && listItemSpec.sizeDip == 12.0f, "list-item role uses 12 DIP Segoe UI Variable Small");
    Require(smallSpec.familyName == kSegoeUiVariableSmallFamily, "small role uses Segoe UI Variable Small");
    Require(headerSpec.familyName == kSegoeUiVariableSmallFamily, "header role uses Segoe UI Variable Small");
    Require(titleLargeSpec.familyName == kSegoeUiVariableDisplayFamily, "title-large role uses Segoe UI Variable Display");
    Require(displaySpec.familyName == kSegoeUiVariableDisplayFamily, "display role uses Segoe UI Variable Display");
    Require(iconSpec.familyName == kSegoeFluentIconsFamily && iconSpec.sizeDip == 12.0f, "icon role uses 12 DIP Segoe Fluent Icons");
    Require(iconLargeSpec.familyName == kSegoeFluentIconsFamily && iconLargeSpec.sizeDip == 32.0f, "icon-large role uses 32 DIP Segoe Fluent Icons");
    Require(heroIconSpec.familyName == kSegoeFluentIconsFamily && heroIconSpec.sizeDip == 64.0f, "hero-icon role uses 64 DIP Segoe Fluent Icons");
    Require(monoSpec.familyName == kUiMonospaceFamily, "monospace role uses the shared monospace family");
}

DxUi::WindowHostBitmapCapture CaptureAttachedHostWindowBitmapForWindowHostSuite(AttachedHostWindow& window, const char* context)
{
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();

    DxUi::WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), context);
    return capture;
}

[[nodiscard]] uint32_t GetWindowHostCapturePixelBgra(const DxUi::WindowHostBitmapCapture& capture, UINT xPx, UINT yPx) noexcept
{
    if (xPx >= capture.widthPx || yPx >= capture.heightPx)
    {
        return 0u;
    }

    const size_t base = (static_cast<size_t>(yPx) * static_cast<size_t>(capture.widthPx) + static_cast<size_t>(xPx)) * 4u;
    if ((base + 3u) >= capture.bgraPixels.size())
    {
        return 0u;
    }

    return static_cast<uint32_t>(capture.bgraPixels[base + 0u]) | (static_cast<uint32_t>(capture.bgraPixels[base + 1u]) << 8u) |
           (static_cast<uint32_t>(capture.bgraPixels[base + 2u]) << 16u) | (static_cast<uint32_t>(capture.bgraPixels[base + 3u]) << 24u);
}

[[nodiscard]] UINT DipToPixelForWindowHost(float dip, float dpi, UINT maxPx) noexcept
{
    if (maxPx == 0u)
    {
        return 0u;
    }

    return (std::min)(maxPx - 1u, static_cast<UINT>((std::max)(0l, std::lround(static_cast<double>(dip) * static_cast<double>(dpi) / 96.0))));
}

[[nodiscard]] std::filesystem::path GetWindowHostPerfJsonlPathFromEnvironment()
{
    const DWORD required = GetEnvironmentVariableW(L"DXUI_PERF_JSONL_PATH", nullptr, 0u);
    if (required == 0u)
    {
        return {};
    }

    std::wstring value(required, L'\0');
    const DWORD copied = GetEnvironmentVariableW(L"DXUI_PERF_JSONL_PATH", value.data(), required);
    if (copied == 0u)
    {
        return {};
    }

    value.resize(copied);
    return std::filesystem::path(value);
}

[[nodiscard]] uintmax_t GetWindowHostFileSizeOrZero(const std::filesystem::path& path) noexcept
{
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    return ec ? 0u : size;
}

[[nodiscard]] std::string ReadWindowHostPerfJsonlFromOffset(const std::filesystem::path& path, uintmax_t offset)
{
    std::ifstream input(path, std::ios::binary);
    if (! input)
    {
        return {};
    }

    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

class ScopedWindowHostPerfJsonl final
{
public:
    ScopedWindowHostPerfJsonl()
    {
        _path = GetWindowHostPerfJsonlPathFromEnvironment();
        if (! _path.empty())
        {
            return;
        }

        _path = GetDxUiTestArtifactPath(L"dxui_windowhost_stage_metrics_testlocal.jsonl");
        std::error_code ec;
        std::filesystem::remove(_path, ec);
        TestPerformanceCapture::Start(_path, L"DxUiTests", L"Debug");
        _ownsConfiguration = true;
    }

    ~ScopedWindowHostPerfJsonl()
    {
        if (_ownsConfiguration)
        {
            TestPerformanceCapture::Stop();
        }
    }

    ScopedWindowHostPerfJsonl(const ScopedWindowHostPerfJsonl&)            = delete;
    ScopedWindowHostPerfJsonl& operator=(const ScopedWindowHostPerfJsonl&) = delete;
    ScopedWindowHostPerfJsonl(ScopedWindowHostPerfJsonl&&)                 = delete;
    ScopedWindowHostPerfJsonl& operator=(ScopedWindowHostPerfJsonl&&)      = delete;

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return _path;
    }

private:
    std::filesystem::path _path;
    bool _ownsConfiguration = false;
};

struct ReentrantKeyboardMutationState
{
    size_t keyDownCount = 0u;
    size_t charCount    = 0u;
};

class ReentrantKeyboardMutationControl final : public DxUi::Control
{
public:
    explicit ReentrantKeyboardMutationControl(ReentrantKeyboardMutationState& state) : _state(&state)
    {
        SetFocusable(true);
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnKeyDown(DxUi::WindowHost& /*host*/, UINT /*virtualKey*/, UINT /*modifiers*/) override
    {
        ++_state->keyDownCount;
        SetEnabled(false);
        return true;
    }

    bool OnChar(DxUi::WindowHost& /*host*/, wchar_t /*ch*/, UINT /*modifiers*/) override
    {
        ++_state->charCount;
        SetEnabled(false);
        return true;
    }

private:
    ReentrantKeyboardMutationState* _state = nullptr;
};

class FocusLossMutatesNextFocusControl final : public DxUi::Control
{
public:
    explicit FocusLossMutatesNextFocusControl(DxUi::Control*& controlToDisable) : _controlToDisable(&controlToDisable)
    {
        SetFocusable(true);
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

protected:
    void OnFocusChanged(DxUi::WindowHost& host, bool focused) override
    {
        if (! focused && _controlToDisable && *_controlToDisable)
        {
            (*_controlToDisable)->SetEnabled(false);
        }
        Control::OnFocusChanged(host, focused);
    }

private:
    DxUi::Control** _controlToDisable = nullptr;
};

struct PostedPayloadDrainStressWindowState final
{
    PostedPayloadDrainStressWindowState()                                                      = default;
    PostedPayloadDrainStressWindowState(const PostedPayloadDrainStressWindowState&)            = delete;
    PostedPayloadDrainStressWindowState(PostedPayloadDrainStressWindowState&&)                 = delete;
    PostedPayloadDrainStressWindowState& operator=(const PostedPayloadDrainStressWindowState&) = delete;
    PostedPayloadDrainStressWindowState& operator=(PostedPayloadDrainStressWindowState&&)      = delete;

    std::atomic<uint32_t> deliveredCount{0};
    std::atomic<uint32_t> drainedCount{0};
    std::atomic<uint32_t> staleTokenCount{0};
    std::atomic<uint32_t> staleTokenRejectionCount{0};
    std::atomic<uint64_t> drainDurationUs{0};
    std::atomic<bool> payloadQueuedBeforeDrain{false};
    std::atomic<bool> payloadQueuedAfterDrain{false};
};

struct PostedPayloadDrainStressPayload final
{
    std::atomic<uint32_t>* destroyedCount = nullptr;
    std::vector<uint32_t> values;

    ~PostedPayloadDrainStressPayload()
    {
        if (destroyedCount)
        {
            destroyedCount->fetch_add(1u, std::memory_order_acq_rel);
        }
    }
};

LRESULT CALLBACK PostedPayloadDrainStressWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    auto* state = reinterpret_cast<PostedPayloadDrainStressWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message)
    {
        case WM_NCCREATE:
        {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            state              = static_cast<PostedPayloadDrainStressWindowState*>(create ? create->lpCreateParams : nullptr);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            DxUi::InitPostedPayloadWindow(hwnd);
            return TRUE;
        }

        case (WM_APP + 0x70u):
        {
            auto payload = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(hwnd, message, lParam);
            if (state && payload)
            {
                state->deliveredCount.fetch_add(1u, std::memory_order_acq_rel);
            }
            return 0;
        }

        case WM_NCDESTROY:
        {
            MSG queuedMessage{};
            if (state)
            {
                state->payloadQueuedBeforeDrain.store(PeekMessageW(&queuedMessage, hwnd, (WM_APP + 0x70u), (WM_APP + 0x70u), PM_NOREMOVE) != 0,
                                                      std::memory_order_release);
            }
            const auto drainStarted = std::chrono::steady_clock::now();
            const size_t drained    = DxUi::DrainPostedPayloadsForWindow(hwnd);
            const auto drainDurationUs =
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - drainStarted).count());
            if (state)
            {
                state->drainedCount.store(static_cast<uint32_t>(drained), std::memory_order_release);
                state->drainDurationUs.store(drainDurationUs, std::memory_order_release);
                state->payloadQueuedAfterDrain.store(PeekMessageW(&queuedMessage, hwnd, (WM_APP + 0x70u), (WM_APP + 0x70u), PM_NOREMOVE) != 0,
                                                     std::memory_order_release);

                uint32_t staleTokenCount          = 0u;
                uint32_t staleTokenRejectionCount = 0u;
                while (PeekMessageW(&queuedMessage, hwnd, (WM_APP + 0x70u), (WM_APP + 0x70u), PM_REMOVE) != 0)
                {
                    ++staleTokenCount;
                    if (! DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(hwnd, queuedMessage.message, queuedMessage.lParam))
                    {
                        ++staleTokenRejectionCount;
                    }
                }
                state->staleTokenCount.store(staleTokenCount, std::memory_order_release);
                state->staleTokenRejectionCount.store(staleTokenRejectionCount, std::memory_order_release);
            }
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;
        }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

[[nodiscard]] wil::unique_hwnd CreatePostedPayloadDrainStressWindow(PostedPayloadDrainStressWindowState& state)
{
    constexpr wchar_t kClassName[] = L"DxUiTests.PostedPayloadDrainStressWindow";

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW existing{};
    if (GetClassInfoW(instance, kClassName, &existing) == FALSE)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc   = PostedPayloadDrainStressWndProc;
        wc.hInstance     = instance;
        wc.lpszClassName = kClassName;
        Require(RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "posted-payload drain stress window class registers");
    }

    return wil::unique_hwnd(CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, &state));
}

class OverlayZOrderControl final : public DxUi::Control
{
public:
    void Paint(DxUi::WindowHost& host) const override
    {
        auto* const dc = host.GetDeviceContext();
        if (! dc)
        {
            return;
        }

        if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.86f, 0.08f, 0.06f, 1.0f)))
        {
            dc->FillRectangle(host.GetClientBoundsDip(), brush);
        }
    }

    void PaintOverlay(DxUi::WindowHost& host) const override
    {
        auto* const dc = host.GetDeviceContext();
        if (! dc)
        {
            return;
        }

        if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.04f, 0.86f, 0.20f, 1.0f)))
        {
            dc->FillRectangle(D2D1::RectF(12.0f, 12.0f, 64.0f, 56.0f), brush);
        }
    }
};

class AnimationTickTraceControl final : public DxUi::Control
{
public:
    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool Tick(DxUi::WindowHost& /*host*/, uint64_t nowTickMs) override
    {
        lastTickMs = nowTickMs;
        ++tickCount;
        return true;
    }

    uint64_t tickCount  = 0u;
    uint64_t lastTickMs = 0u;
};

class OverlayHitRecordingControl final : public DxUi::Control
{
public:
    explicit OverlayHitRecordingControl(TrackingControlState& state) : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseDown(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        ++_state->mouseDownCount;
        return ! rightButton;
    }

protected:
    [[nodiscard]] DxUi::Control* HitTestOverlay(D2D1_POINT_2F point) override
    {
        return Control::HitTest(point);
    }

    [[nodiscard]] const DxUi::Control* HitTestOverlay(D2D1_POINT_2F point) const override
    {
        return Control::HitTest(point);
    }

private:
    TrackingControlState* _state = nullptr;
};

struct RootRetiringOverlayHitState
{
    size_t overlayHitCount = 0u;
    size_t normalHitCount  = 0u;
    size_t mouseMoveCount  = 0u;
    size_t mouseDownCount  = 0u;
};

class RootRetiringOverlayHitControl final : public DxUi::Control
{
public:
    RootRetiringOverlayHitControl(DxUi::WindowHost& host, RootRetiringOverlayHitState& state) noexcept : _host(&host), _state(&state)
    {
    }

    void Paint(DxUi::WindowHost&) const override
    {
    }

    [[nodiscard]] DxUi::Control* HitTest(D2D1_POINT_2F point) override
    {
        ++_state->normalHitCount;
        return Control::HitTest(point);
    }

    bool OnMouseMove(DxUi::WindowHost&, D2D1_POINT_2F, UINT) override
    {
        ++_state->mouseMoveCount;
        return true;
    }

    bool OnMouseDown(DxUi::WindowHost&, D2D1_POINT_2F, bool, UINT) override
    {
        ++_state->mouseDownCount;
        return true;
    }

protected:
    [[nodiscard]] DxUi::Control* HitTestOverlay(D2D1_POINT_2F) override
    {
        ++_state->overlayHitCount;
        _host->SetRoot({});
        return this;
    }

private:
    DxUi::WindowHost* _host             = nullptr;
    RootRetiringOverlayHitState* _state = nullptr;
};

struct ThrowOnceOnMouseDownState
{
    size_t mouseDownCount = 0u;
};

class ThrowOnceOnMouseDownControl final : public DxUi::Control
{
public:
    explicit ThrowOnceOnMouseDownControl(ThrowOnceOnMouseDownState& state) noexcept : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost&) const override
    {
    }

    bool OnMouseDown(DxUi::WindowHost&, D2D1_POINT_2F, bool, UINT) override
    {
        if (_state->mouseDownCount++ == 0u)
        {
            throw std::runtime_error("intentional one-shot WindowHost input failure");
        }
        return true;
    }

private:
    ThrowOnceOnMouseDownState* _state = nullptr;
};

struct WindowHostPaintFailureState
{
    size_t paintCount = 0u;
};

class ThrowOnceDuringWindowHostPaintControl final : public DxUi::Control
{
public:
    explicit ThrowOnceDuringWindowHostPaintControl(WindowHostPaintFailureState& state) noexcept : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& host) const override
    {
        if (_state->paintCount++ == 0u)
        {
            throw std::runtime_error("intentional one-shot WindowHost paint failure");
        }

        auto* const dc = host.GetDeviceContext();
        if (dc)
        {
            if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.05f, 0.80f, 0.18f, 1.0f)))
            {
                dc->FillRectangle(host.GetClientBoundsDip(), brush);
            }
        }
    }

private:
    WindowHostPaintFailureState* _state = nullptr;
};

struct WindowHostRootReplacementPaintState
{
    size_t paintCount = 0u;
    std::function<void(DxUi::WindowHost&)> firstPaintCallback;
};

class WindowHostSolidColorPaintControl final : public DxUi::Control
{
public:
    explicit WindowHostSolidColorPaintControl(D2D1_COLOR_F color) noexcept : _color(color)
    {
    }

    void Paint(DxUi::WindowHost& host) const override
    {
        auto* const dc = host.GetDeviceContext();
        if (dc)
        {
            if (auto* const brush = host.GetSolidBrush(_color))
            {
                dc->FillRectangle(host.GetClientBoundsDip(), brush);
            }
        }
    }

private:
    D2D1_COLOR_F _color{};
};

class ReplaceRootDuringWindowHostPaintControl final : public DxUi::Control
{
public:
    explicit ReplaceRootDuringWindowHostPaintControl(WindowHostRootReplacementPaintState& state) noexcept : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& host) const override
    {
        ++_state->paintCount;
        if (_state->firstPaintCallback)
        {
            auto callback = std::move(_state->firstPaintCallback);
            callback(host);
            return;
        }
    }

private:
    WindowHostRootReplacementPaintState* _state = nullptr;
};

struct SelfCapturingControlState
{
    size_t mouseDownCount          = 0u;
    size_t mouseMoveWhileDownCount = 0u;
    size_t captureLostCount        = 0u;
    bool dragging                  = false;
};

class SelfCapturingControl final : public DxUi::Control
{
public:
    explicit SelfCapturingControl(SelfCapturingControlState& state) : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseDown(DxUi::WindowHost& host, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        if (rightButton)
        {
            return false;
        }

        ++_state->mouseDownCount;
        _state->dragging = true;
        host.CaptureMouse(this);
        return true;
    }

    bool OnMouseMove(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, UINT /*modifiers*/) override
    {
        if (_state->dragging)
        {
            ++_state->mouseMoveWhileDownCount;
        }
        return true;
    }

    bool OnMouseUp(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, bool /*rightButton*/, UINT /*modifiers*/) override
    {
        _state->dragging = false;
        return true;
    }

    void OnCaptureLost(DxUi::WindowHost& /*host*/) override
    {
        ++_state->captureLostCount;
        _state->dragging = false;
    }

private:
    SelfCapturingControlState* _state = nullptr;
};

struct RootReplacingPointerControlState
{
    size_t mouseDownCount = 0u;
    size_t mouseUpCount   = 0u;
};

class RootReplacingPointerControl final : public DxUi::Control
{
public:
    explicit RootReplacingPointerControl(RootReplacingPointerControlState& state) : _state(&state)
    {
        SetFocusable(true);
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseDown(DxUi::WindowHost& host, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        if (rightButton)
        {
            return false;
        }

        ++_state->mouseDownCount;
        host.SetRoot(std::make_unique<DxUi::Panel>());
        return true;
    }

    bool OnMouseUp(DxUi::WindowHost& host, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        if (rightButton)
        {
            return false;
        }

        ++_state->mouseUpCount;
        host.SetRoot(std::make_unique<DxUi::Panel>());
        return true;
    }

private:
    RootReplacingPointerControlState* _state = nullptr;
};

struct RootReplacingHoverControlState
{
    size_t hoverEnterCount = 0u;
    size_t mouseMoveCount  = 0u;
};

class RootReplacingHoverControl final : public DxUi::Control
{
public:
    explicit RootReplacingHoverControl(RootReplacingHoverControlState& state) : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseMove(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, UINT /*modifiers*/) override
    {
        ++_state->mouseMoveCount;
        return true;
    }

protected:
    void OnHoverChanged(DxUi::WindowHost& host, bool hovered) override
    {
        Control::OnHoverChanged(host, hovered);

        if (hovered)
        {
            ++_state->hoverEnterCount;
            host.SetRoot(std::make_unique<DxUi::Panel>());
        }
    }

private:
    RootReplacingHoverControlState* _state = nullptr;
};

class DetachOrderProbeControl final : public DxUi::Control
{
public:
    DetachOrderProbeControl(DWORD ownerThreadId, uint32_t expectedAttachmentCount, bool& destroyed, bool& sawAttachmentAlive) noexcept
        : _ownerThreadId(ownerThreadId),
          _expectedAttachmentCount(expectedAttachmentCount),
          _destroyed(destroyed),
          _sawAttachmentAlive(sawAttachmentAlive)
    {
    }
    DetachOrderProbeControl(const DetachOrderProbeControl&)            = delete;
    DetachOrderProbeControl& operator=(const DetachOrderProbeControl&) = delete;
    DetachOrderProbeControl(DetachOrderProbeControl&&)                 = delete;
    DetachOrderProbeControl& operator=(DetachOrderProbeControl&&)      = delete;

    ~DetachOrderProbeControl() override
    {
        _destroyed          = true;
        _sawAttachmentAlive = DxUi::DebugGetSharedWindowHostAttachmentCountForThread(_ownerThreadId) == _expectedAttachmentCount;
    }

    void Paint(DxUi::WindowHost&) const override
    {
    }

private:
    DWORD _ownerThreadId              = 0u;
    uint32_t _expectedAttachmentCount = 0u;
    bool& _destroyed;
    bool& _sawAttachmentAlive;
};

class RenderLayoutMutationProbeControl final : public DxUi::Control
{
public:
    void Paint(DxUi::WindowHost& /*host*/) const override
    {
        if (_attemptedMutation)
        {
            return;
        }

        _attemptedMutation = true;
        const_cast<RenderLayoutMutationProbeControl*>(this)->SetBounds(D2D1::RectF(24.0f, 24.0f, 132.0f, 68.0f));
    }

private:
    mutable bool _attemptedMutation = false;
};

void TestWindowHostKeyboardInputMarksFocusVisible()
{
    using namespace DxUi;

    WindowHost host;
    Require(host.GetInputModality() == InputModality::Pointer, "window host starts in pointer modality");
    Require(! host.IsKeyboardFocusVisible(), "window host starts with pointer-style focus visuals");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, 'A', 0, handled));
    Require(handled, "keyboard input handled at host level");
    Require(host.GetInputModality() == InputModality::Keyboard, "keyboard input switches modality to keyboard");
    Require(host.IsKeyboardFocusVisible(), "keyboard input enables keyboard focus visuals");
}

void TestWindowHostPointerInputClearsKeyboardFocusVisible()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, 'A', 0, handled));
    Require(host.IsKeyboardFocusVisible(), "keyboard modality is active before pointer test");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, 0, handled));
    Require(handled, "pointer input handled at host level");
    Require(host.GetInputModality() == InputModality::Pointer, "pointer input switches modality back to pointer");
    Require(! host.IsKeyboardFocusVisible(), "pointer input clears keyboard-only focus visuals");
}

void TestWindowHostRejectsForeignThreadDetachUntilOwnerDetaches()
{
    using namespace DxUi;

    const DWORD ownerThreadId                   = GetCurrentThreadId();
    const size_t baselineAttachedHostCount      = DebugGetAttachedWindowHostCount();
    const uint32_t baselineOwnerAttachmentCount = DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId);

    AttachedHostWindow window;
    Require(DebugGetAttachedWindowHostCount() == (baselineAttachedHostCount + 1u), "attached host window registers one additional WindowHost");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == (baselineOwnerAttachmentCount + 1u),
            "attached host window increments the owner thread graphics attachment count");

    std::thread worker([&window] { window.Host().Detach(); });
    worker.join();

    Require(DebugGetAttachedWindowHostCount() == (baselineAttachedHostCount + 1u), "foreign-thread detach leaves the owner-thread WindowHost registered");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == (baselineOwnerAttachmentCount + 1u),
            "foreign-thread detach cannot release owner-thread graphics or TSF state");

    window.Host().Detach();
    Require(DebugGetAttachedWindowHostCount() == baselineAttachedHostCount, "owner-thread detach removes the host from the registry");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineOwnerAttachmentCount,
            "owner-thread detach releases its graphics attachment count");
}

void TestProcessExitShutdownMarshalsForeignWindowHostDetachToOwnerThread()
{
    using namespace DxUi;

    class ThreadRecordingLabel final : public Label
    {
    public:
        explicit ThreadRecordingLabel(std::atomic<DWORD>& destroyedThreadId) : Label(L"Worker host"), _destroyedThreadId(&destroyedThreadId)
        {
        }

        ~ThreadRecordingLabel() noexcept override
        {
            _destroyedThreadId->store(GetCurrentThreadId(), std::memory_order_release);
        }

    private:
        std::atomic<DWORD>* _destroyedThreadId = nullptr;
    };

    Require(DebugGetAttachedWindowHostCount() == 0u, "owner-thread shutdown marshal test starts at the host-registry quiet point");
    std::atomic<DWORD> workerThreadId{0u};
    std::atomic<DWORD> destroyedThreadId{0u};
    std::promise<void> readyPromise;
    std::future<void> readyFuture = readyPromise.get_future();
    std::promise<void> stopPromise;
    std::shared_future<void> stopFuture = stopPromise.get_future().share();

    std::thread worker([&workerThreadId, &destroyedThreadId, ready = std::move(readyPromise), stopFuture]() mutable
    {
        AttachedHostWindow workerWindow;
        workerThreadId.store(GetCurrentThreadId(), std::memory_order_release);
        auto root = std::make_unique<Panel>();
        root->AddChild<ThreadRecordingLabel>(destroyedThreadId)->SetBounds(D2D1::RectF(8.0f, 8.0f, 160.0f, 36.0f));
        workerWindow.Host().SetRoot(std::move(root));
        ready.set_value();
        while (stopFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        {
            workerWindow.PumpMessages();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    readyFuture.wait();
    Require(DebugGetAttachedWindowHostCount() == 1u, "worker UI thread registers one live WindowHost before process-exit shutdown");
    ShutdownAllWindowHostsForProcessExit();
    Require(DebugGetAttachedWindowHostCount() == 0u, "process-exit shutdown drains the WindowHost registry before global resource reset");
    Require(destroyedThreadId.load(std::memory_order_acquire) == workerThreadId.load(std::memory_order_acquire),
            "process-exit shutdown destroys the retained tree on its attachment thread");

    stopPromise.set_value();
    worker.join();
}

void TestWindowHostDetachKeepsSharedGraphicsAttachmentUntilControlTreeDestroyed()
{
    using namespace DxUi;

    const DWORD ownerThreadId                   = GetCurrentThreadId();
    const size_t baselineAttachedHostCount      = DebugGetAttachedWindowHostCount();
    const uint32_t baselineOwnerAttachmentCount = DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId);

    AttachedHostWindow window;
    const uint32_t attachedOwnerAttachmentCount = baselineOwnerAttachmentCount + 1u;
    bool probeDestroyed                         = false;
    bool probeSawAttachmentAlive                = false;
    window.Host().SetRoot(std::make_unique<DetachOrderProbeControl>(ownerThreadId, attachedOwnerAttachmentCount, probeDestroyed, probeSawAttachmentAlive));

    Require(DebugGetAttachedWindowHostCount() == (baselineAttachedHostCount + 1u), "attached host registers before detach-order validation");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == attachedOwnerAttachmentCount,
            "attached host increments graphics attachment count before detach-order validation");

    window.Host().Detach();

    Require(probeDestroyed, "detach destroys the retained control tree");
    Require(probeSawAttachmentAlive, "detach keeps the shared graphics attachment alive until the retained control tree is destroyed");
    Require(DebugGetAttachedWindowHostCount() == baselineAttachedHostCount, "detach removes host from the attached-host registry after order validation");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineOwnerAttachmentCount,
            "detach releases graphics attachment count after retained host resources are destroyed");
}

void TestWindowHostFocusLossCannotReattachDuringDetach()
{
    using namespace DxUi;
    const size_t baselineHostCount     = DebugGetAttachedWindowHostCount();
    const DWORD ownerThreadId          = GetCurrentThreadId();
    const uint32_t baselineThreadCount = DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId);
    wil::unique_hwnd replacement(CreateWindowExW(0, L"STATIC", L"", WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    Require(replacement != nullptr, "a hidden replacement HWND is available for the reentrant attachment");
    bool callbackRan         = false;
    bool attachmentSucceeded = true;
    DWORD attachmentError    = ERROR_SUCCESS;
    class ReattachingControl final : public Control
    {
    public:
        explicit ReattachingControl(std::function<void(WindowHost&)> callback) : _callback(std::move(callback))
        {
            SetFocusable(true);
        }
        void Paint(WindowHost&) const override
        {
        }
        void OnFocusChanged(WindowHost& host, bool focused) override
        {
            if (! focused)
                _callback(host);
            Control::OnFocusChanged(host, focused);
        }

    private:
        std::function<void(WindowHost&)> _callback;
    };
    AttachedHostWindow window;
    auto control           = std::make_unique<ReattachingControl>([&](WindowHost& host)
    {
        callbackRan = true;
        SetLastError(ERROR_SUCCESS);
        attachmentSucceeded = host.Attach(replacement.get());
        attachmentError     = GetLastError();
    });
    Control* const focused = control.get();
    window.Host().SetRoot(std::move(control));
    window.Host().SetFocusControl(focused);
    Require(window.Host().GetFocusControl() == focused, "logical focus is installed without taking desktop focus");
    window.Host().Detach();
    Require(callbackRan, "ordinary detach delivers the control's focus-loss callback");
    Require(! attachmentSucceeded && attachmentError == ERROR_BUSY, "a nested attachment is refused while teardown is running");
    Require(! window.Host().GetHwnd() && ! window.Host().GetRoot(), "the original detach leaves no attachment or retained root");
    Require(DebugGetAttachedWindowHostCount() == baselineHostCount && DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineThreadCount,
            "nested attachment refusal leaves both registries balanced");
    Require(window.Host().Attach(replacement.get()), "the same host can attach after teardown finishes");
    Require(DebugGetAttachedWindowHostCount() == baselineHostCount + 1u &&
                DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineThreadCount + 1u,
            "the later attachment reserves exactly one slot");
    window.Host().Detach();
    Require(DebugGetAttachedWindowHostCount() == baselineHostCount && DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineThreadCount,
            "the later attachment releases both registry slots");
}

void TestWindowHostEmitsFrameStageMetricsForCaptureRender()
{
    using namespace DxUi;

    ScopedWindowHostPerfJsonl perfJsonl;
    Require(! perfJsonl.Path().empty(), "window host stage metric test has a perf JSONL sink");

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"Frame telemetry");
    label->SetBounds(D2D1::RectF(8.0f, 8.0f, 160.0f, 32.0f));
    window.Host().SetRoot(std::move(root));
    window.Host().Invalidate();

    static_cast<void>(CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "frame-stage metric warm-up capture succeeds"));

    const uintmax_t metricOffset = GetWindowHostFileSizeOrZero(perfJsonl.Path());
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), "frame-stage metric direct debug capture render succeeds");
    Require(capture.widthPx > 0u && capture.heightPx > 0u && ! capture.bgraPixels.empty(), "frame-stage metric capture has pixels");

    const std::string appendedMetrics                          = ReadWindowHostPerfJsonlFromOffset(perfJsonl.Path(), metricOffset);
    constexpr std::array<std::string_view, 6> kExpectedMetrics = {{
        "\"metric\":\"dxui.frame.total_us\"",
        "\"metric\":\"dxui.frame.update_us\"",
        "\"metric\":\"dxui.frame.render_us\"",
        "\"metric\":\"dxui.frame.present_us\"",
        "\"metric\":\"dxui.frame.dirty_rect_count\"",
        "\"metric\":\"dxui.frame.dirty_rect_area_px\"",
    }};
    for (const std::string_view metric : kExpectedMetrics)
    {
        Require(appendedMetrics.find(metric) != std::string::npos, "window host capture render emits every frame-stage metric");
    }

    const auto findMetricLine = [&](std::string_view metric) noexcept -> std::string_view
    {
        const size_t metricPosition = appendedMetrics.find(metric);
        if (metricPosition == std::string::npos)
        {
            return {};
        }

        const size_t lineStart = appendedMetrics.rfind('\n', metricPosition);
        const size_t lineEnd   = appendedMetrics.find('\n', metricPosition);
        const size_t start     = lineStart == std::string::npos ? 0u : lineStart + 1u;
        const size_t end       = lineEnd == std::string::npos ? appendedMetrics.size() : lineEnd;
        return std::string_view(appendedMetrics).substr(start, end - start);
    };

    const std::string_view dirtyCountLine = findMetricLine("\"metric\":\"dxui.frame.dirty_rect_count\"");
    const std::string_view dirtyAreaLine  = findMetricLine("\"metric\":\"dxui.frame.dirty_rect_area_px\"");
    Require(dirtyCountLine.find("\"value\":0") != std::string_view::npos, "full-frame capture reports zero dirty rect count");
    Require(dirtyAreaLine.find("\"value\":0") != std::string_view::npos, "full-frame capture reports zero dirty rect area");
}

void TestWindowHostCaptureRecoversAfterControlPaintThrows()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    WindowHostPaintFailureState state;
    window.Host().SetRoot(std::make_unique<ThrowOnceDuringWindowHostPaintControl>(state));

    WindowHostBitmapCapture failedCapture;
    const bool firstCaptureSucceeded = window.Host().DebugCaptureBitmap(failedCapture);
    Require(! firstCaptureSucceeded, "a capture whose control paint throws is discarded instead of reporting a partial frame");
    Require(failedCapture.widthPx == 0u && failedCapture.heightPx == 0u && failedCapture.bgraPixels.empty(),
            "the failed paint leaves no mixed-frame bitmap in the capture output");
    Require(state.paintCount == 1u, "the first capture reaches the one-shot throwing paint exactly once");

    WindowHostBitmapCapture recoveredCapture;
    Require(window.Host().DebugCaptureBitmap(recoveredCapture), "a later capture succeeds after the one-shot paint exception");
    Require(recoveredCapture.widthPx > 0u && recoveredCapture.heightPx > 0u && ! recoveredCapture.bgraPixels.empty(),
            "the recovered capture contains a complete bitmap");
    Require(state.paintCount == 2u, "the later capture retries the control paint once without a retry loop");
}

void TestWindowHostCaptureDiscardsFrameWhenPaintReplacesItsRoot()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    WindowHostRootReplacementPaintState state;
    size_t replacementCallbackCount = 0u;
    state.firstPaintCallback        = [&replacementCallbackCount](WindowHost& host)
    {
        ++replacementCallbackCount;
        host.SetRoot(std::make_unique<WindowHostSolidColorPaintControl>(D2D1::ColorF(0.05f, 0.80f, 0.18f, 1.0f)));
    };
    window.Host().SetRoot(std::make_unique<ReplaceRootDuringWindowHostPaintControl>(state));

    WindowHostBitmapCapture retiredFrame;
    const bool retiredFrameCaptured = window.Host().DebugCaptureBitmap(retiredFrame);
    Require(! retiredFrameCaptured, "root replacement during paint rejects the frame being rendered from the retired tree");
    Require(retiredFrame.widthPx == 0u && retiredFrame.heightPx == 0u && retiredFrame.bgraPixels.empty(),
            "the retired-tree capture publishes no mixed-frame pixels");
    Require(replacementCallbackCount == 1u && state.paintCount == 1u, "the external one-shot callback survives retirement of the control that invoked it");
    Require(window.Host().GetRoot() != nullptr, "paint-time root replacement remains installed after the retired frame is discarded");

    WindowHostBitmapCapture freshFrame;
    Require(window.Host().DebugCaptureBitmap(freshFrame), "a later capture renders the replacement root successfully");
    const auto bounds = window.Host().GetRoot()->GetBounds();
    Require(bounds.right > bounds.left && bounds.bottom > bounds.top, "the replacement root receives native layout bounds before its first complete frame");
    Require(freshFrame.widthPx > 0u && freshFrame.heightPx > 0u && ! freshFrame.bgraPixels.empty(), "the replacement-root capture contains a complete bitmap");
    const UINT sampleX   = freshFrame.widthPx / 2u;
    const UINT sampleY   = freshFrame.heightPx / 2u;
    const uint32_t pixel = GetWindowHostCapturePixelBgra(freshFrame, sampleX, sampleY);
    Require(((pixel >> 8u) & 0xFFu) > 160u && ((pixel >> 16u) & 0xFFu) < 96u,
            "the fresh capture contains replacement-root green content rather than retired-frame pixels");
    Require(replacementCallbackCount == 1u, "the root-replacement callback runs only once across the recovery capture");
}

void TestWindowHostCaptureDiscardsFrameWhenPaintMutatesTheChildTree()
{
    using namespace DxUi;
    struct PaintState final
    {
        size_t firstPaints   = 0u;
        size_t secondPaints  = 0u;
        size_t callbackCount = 0u;
        std::function<void()> onNextPaint;
    };
    class ChildPainter final : public Control
    {
    public:
        ChildPainter(PaintState& state, bool first) noexcept : _state(&state), _first(first)
        {
        }
        void Paint(ControlHost& host) const override
        {
            ++(_first ? _state->firstPaints : _state->secondPaints);
            if (auto* dc = host.GetDeviceContext())
            {
                if (auto* brush = host.GetSolidBrush(_first ? D2D1::ColorF(0.86f, 0.08f, 0.06f, 1.0f) : D2D1::ColorF(0.04f, 0.86f, 0.20f, 1.0f)))
                    dc->FillRectangle(host.GetClientBoundsDip(), brush);
            }
            if (_first)
            {
                auto callback = std::move(_state->onNextPaint);
                if (callback)
                    callback();
            }
        }

    private:
        PaintState* _state;
        bool _first;
    };
    for (const unsigned mutation : {0u, 1u, 2u, 3u, 4u})
    {
        PaintState state;
        AttachedHostWindow window;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        auto root   = std::make_unique<Panel>();
        auto* panel = root.get();
        root->AddChild<ChildPainter>(state, true)->SetBounds(D2D1::RectF(0, 0, 160, 80));
        auto* second = root->AddChild<ChildPainter>(state, false);
        second->SetBounds(D2D1::RectF(0, 0, 160, 80));
        auto* destination = root->AddChild<PageHost>();
        destination->SetBounds(D2D1::RectF(0, 0, 160, 80));
        window.Host().SetRoot(std::move(root));
        WindowHostBitmapCapture initialFrame;
        Require(window.Host().DebugCaptureBitmap(initialFrame), "initial child-tree capture completes");
        ValidateRect(window.Hwnd(), nullptr);
        Require(GetUpdateRect(window.Hwnd(), nullptr, FALSE) == FALSE, "initial capture has no pending native corrective paint");
        state.firstPaints  = 0u;
        state.secondPaints = 0u;
        state.onNextPaint  = [&]
        {
            ++state.callbackCount;
            switch (mutation)
            {
                case 0u: panel->ClearChildren(); break;
                case 1u: static_cast<void>(panel->TakeChild(0u)); break;
                case 2u: destination->SetPage(panel->TakeChild(0u)); break;
                case 3u: second->SetVisible(false); break;
                case 4u: second->SetEnabled(false); break;
            }
        };
        WindowHostBitmapCapture partialFrame;
        Require(! window.Host().DebugCaptureBitmap(partialFrame), "supported child-tree mutation during paint rejects the incomplete frame");
        Require(partialFrame.widthPx == 0u && partialFrame.heightPx == 0u && partialFrame.bgraPixels.empty(),
                "child-tree mutation cannot publish mixed-frame pixels");
        Require(state.firstPaints >= 1u && state.callbackCount == 1u, "the one-shot mutation runs from the first child paint");
        Require(window.Host().GetRoot() == panel, "child mutation keeps the native root identity unchanged");
        if (mutation == 0u)
            Require(state.secondPaints == 0u && panel->GetChildren().empty(), "clearing children skips the retired sibling paint");
        Require(GetUpdateRect(window.Hwnd(), nullptr, FALSE) != FALSE, "child-tree mutation schedules corrective native painting");
        WindowHostBitmapCapture freshFrame;
        Require(window.Host().DebugCaptureBitmap(freshFrame), "a later native capture completes for the current child tree");
        Require(! freshFrame.bgraPixels.empty() && ! state.onNextPaint, "recovery yields a complete frame without repeating the callback");
        if (mutation == 0u || mutation == 3u)
        {
            const auto pixel = GetWindowHostCapturePixelBgra(freshFrame, freshFrame.widthPx / 2u, freshFrame.heightPx / 2u);
            Require(! (((pixel >> 8u) & 0xFFu) > 180u && ((pixel >> 16u) & 0xFFu) < 40u), "recovery excludes the removed or hidden green sibling content");
        }
    }
}

void TestWindowHostGridFocusMutationCannotRetargetTheCurrentClick()
{
    using namespace DxUi;
    struct State final
    {
        size_t actions = 0u;
        bool checked   = false;
        bool mirrored  = false;
    } state;
    class Model final : public IGridModel
    {
    public:
        explicit Model(State& state) noexcept : _state(&state)
        {
        }
        size_t GetRowCount() const noexcept override
        {
            return 1u;
        }
        size_t GetColumnCount() const noexcept override
        {
            return 2u;
        }
        GridColumnDesc GetColumn(size_t column) const override
        {
            return {.id       = column == 0u ? L"check" : L"text",
                    .title    = column == 0u ? L"Check" : L"Text",
                    .widthDip = 160.0f,
                    .kind     = column == 0u ? GridColumnKind::Checkbox : GridColumnKind::Text};
        }
        std::optional<size_t> FindRowByStableId(uint64_t id) const noexcept override
        {
            return id == 0u ? std::optional<size_t>(0u) : std::nullopt;
        }
        void GetCellData(size_t, size_t column, GridCellData& cell) const override
        {
            cell.kind    = column == 0u ? GridCellKind::Checkbox : GridCellKind::Text;
            cell.checked = _state->checked;
        }

    private:
        State* _state;
    } model(state);
    class Delegate final : public IGridDelegate
    {
    public:
        explicit Delegate(State& state) noexcept : _state(&state)
        {
        }
        void OnGridCheckboxToggled(size_t row, size_t column, bool checked) override
        {
            Require(row == 0u && column == 0u, "native retry toggles the stable checkbox cell");
            ++_state->actions;
            _state->checked = checked;
        }

    private:
        State* _state;
    } delegate(state);
    WindowHost host;
    auto root  = std::make_unique<Grid>();
    auto* grid = root.get();
    grid->SetHeaderHeightDip(24.0f);
    grid->SetRowHeightDip(24.0f);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);
    host.SetRoot(std::move(root));
    grid->SetBounds(D2D1::RectF(0, 0, 320, 160));
    host.SetOnFocusChanged([&](Control* focused)
    {
        if (focused == grid && ! state.mirrored)
        {
            state.mirrored = true;
            grid->SetFlowDirection(FlowDirection::RightToLeft);
        }
    });
    bool handled = false;
    host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(240, 36), handled);
    Require(handled && state.mirrored && state.actions == 0u && ! state.checked,
            "focus-time RTL change consumes the old text-cell click without retargeting it to a checkbox");
    host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(240, 36), handled);
    Require(host.GetCapturedControl() == nullptr, "the consumed native gesture releases capture");
    host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(240, 36), handled);
    Require(handled && state.actions == 1u && state.checked, "an independent later click uses the current mirrored checkbox geometry");
    host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(240, 36), handled);
}

void TestWindowHostBlocksLayoutMutationDuringRender()
{
    using namespace DxUi;

    ScopedWindowHostPerfJsonl perfJsonl;
    Require(! perfJsonl.Path().empty(), "window host render-mutation test has a perf JSONL sink");

    AttachedHostWindow window;
    auto root                       = std::make_unique<Panel>();
    auto* mutating                  = root->AddChild<RenderLayoutMutationProbeControl>();
    const D2D1_RECT_F initialBounds = D2D1::RectF(8.0f, 8.0f, 96.0f, 40.0f);
    mutating->SetBounds(initialBounds);
    window.Host().SetRoot(std::move(root));
    window.Host().Invalidate();

    const uintmax_t metricOffset = GetWindowHostFileSizeOrZero(perfJsonl.Path());
    static_cast<void>(CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "render layout mutation capture succeeds"));

    const D2D1_RECT_F finalBounds = mutating->GetBounds();
    Require(finalBounds.left == initialBounds.left && finalBounds.top == initialBounds.top && finalBounds.right == initialBounds.right &&
                finalBounds.bottom == initialBounds.bottom,
            "render layout mutation keeps control bounds unchanged");

    const std::string appendedMetrics = ReadWindowHostPerfJsonlFromOffset(perfJsonl.Path(), metricOffset);
    Require(appendedMetrics.find("\"metric\":\"dxui.frame.render_layout_mutation_blocked\"") != std::string::npos,
            "render layout mutation emits blocked counter");
}

void TestPostMessagePayloadTeardownDrainDeletesUndeliveredPayloads()
{
    constexpr UINT kPayloadMessage = (WM_APP + 0x70u);
    PostedPayloadDrainStressWindowState state;
    std::atomic<uint32_t> destroyedCount{0};

    wil::unique_hwnd hwnd = CreatePostedPayloadDrainStressWindow(state);
    Require(hwnd != nullptr, "payload drain stress window is created");

    constexpr uint32_t kPayloadCount = 128u;
    const auto postStarted           = std::chrono::steady_clock::now();
    for (uint32_t i = 0u; i < kPayloadCount; ++i)
    {
        auto payload            = std::make_unique<PostedPayloadDrainStressPayload>();
        payload->destroyedCount = &destroyedCount;
        Require(DxUi::PostMessagePayload(hwnd.get(), kPayloadMessage, 0, std::move(payload)),
                "PostMessagePayload accepts payloads while the target window is alive");
    }
    const auto postDurationUs =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - postStarted).count());
    DxUi::Debug::Perf::Emit(L"dxui.posted_payload.post_batch_us", L"128 queued payloads", postDurationUs, kPayloadCount, kPayloadCount, S_OK);

    MSG capturedStaleMessage{};
    Require(PeekMessageW(&capturedStaleMessage, nullptr, kPayloadMessage, kPayloadMessage, PM_NOREMOVE) != 0,
            "payload drain stress captures one queued message identity before teardown");
    const HWND retiredHwnd = hwnd.get();

    hwnd.reset();
    DxUi::Debug::Perf::Emit(L"dxui.posted_payload.teardown_drain_us",
                            L"128 queued payloads",
                            state.drainDurationUs.load(std::memory_order_acquire),
                            kPayloadCount,
                            kPayloadCount,
                            S_OK);

    Require(state.deliveredCount.load(std::memory_order_acquire) == 0u, "stress test destroys the window before delivery");
    Require(state.drainedCount.load(std::memory_order_acquire) == kPayloadCount, "WM_NCDESTROY drains all queued payloads");
    Require(destroyedCount.load(std::memory_order_acquire) == kPayloadCount, "drained payloads are deleted exactly once");
    Require(state.payloadQueuedBeforeDrain.load(std::memory_order_acquire),
            "the teardown test observes a payload message queued while the retiring HWND is still valid");
    Require(state.payloadQueuedAfterDrain.load(std::memory_order_acquire),
            "the opaque-token design deliberately leaves stale messages queued without retaining their storage");
    Require(state.staleTokenCount.load(std::memory_order_acquire) == kPayloadCount,
            "the teardown stress test pumps every deliberately retained stale token before Windows can discard it");
    Require(state.staleTokenRejectionCount.load(std::memory_order_acquire) == kPayloadCount,
            "every stale queued token is rejected after teardown invalidates the registry entries");

    DxUi::InitPostedPayloadWindow(retiredHwnd);
    Require(destroyedCount.load(std::memory_order_acquire) == kPayloadCount, "pumping stale tokens after teardown cannot delete payload storage a second time");

    auto stalePayload = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(retiredHwnd, kPayloadMessage, capturedStaleMessage.lParam);
    Require(! stalePayload, "a stale queued lParam is rejected after its registered payload was drained");

    auto staleAfterSimulatedHwndReuse = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(retiredHwnd, kPayloadMessage, capturedStaleMessage.lParam);
    Require(! staleAfterSimulatedHwndReuse, "clearing the retired-HWND fence never makes a stale lParam ownable again");

    auto unregisteredPayload = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(retiredHwnd, kPayloadMessage, static_cast<LPARAM>(0x1234u));
    Require(! unregisteredPayload, "TakeMessagePayload never adopts an unregistered lParam");
}

void TestPostedPayloadTakeRequiresTheMatchingWindowMessageAndType()
{
    constexpr UINT kPayloadMessage = WM_APP + 0x70u;
    PostedPayloadDrainStressWindowState firstState;
    PostedPayloadDrainStressWindowState secondState;
    std::atomic<uint32_t> destroyedCount{0};
    auto first  = CreatePostedPayloadDrainStressWindow(firstState);
    auto second = CreatePostedPayloadDrainStressWindow(secondState);
    Require(first && second, "both payload-protocol windows are created");

    auto payload            = std::make_unique<PostedPayloadDrainStressPayload>();
    payload->destroyedCount = &destroyedCount;
    Require(DxUi::PostMessagePayload(first.get(), kPayloadMessage, 0, std::move(payload)), "the matching payload is queued");

    MSG queued{};
    Require(PeekMessageW(&queued, first.get(), kPayloadMessage, kPayloadMessage, PM_REMOVE) != 0,
            "the test removes the message before exercising direct Take calls");
    const LPARAM token = queued.lParam;
    Require(! DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(first.get(), kPayloadMessage + 1u, token),
            "the same token with a different protocol message is rejected");
    Require(! DxUi::TakeMessagePayload<int>(first.get(), kPayloadMessage, token), "the same token with a different payload type is rejected");
    Require(! DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(second.get(), kPayloadMessage, token),
            "the same token with a different HWND is rejected");
    Require(destroyedCount.load(std::memory_order_acquire) == 0u, "mismatched Take attempts preserve the registered payload");

    auto genuine = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(first.get(), kPayloadMessage, token);
    Require(genuine != nullptr, "the exact HWND, message, and type can still take the preserved payload");
    genuine.reset();
    Require(destroyedCount.load(std::memory_order_acquire) == 1u, "the correctly consumed payload is destroyed exactly once");
}

void TestPostedPayloadCapacityFailureDestroysAndAllowsRetry()
{
    constexpr UINT kPayloadMessage   = WM_APP + 0x70u;
    constexpr uint32_t kPayloadCount = 128u;
    PostedPayloadDrainStressWindowState state;
    std::atomic<uint32_t> destroyedCount{0};
    auto hwnd = CreatePostedPayloadDrainStressWindow(state);
    Require(hwnd != nullptr, "payload capacity window is created");

    for (uint32_t i = 0u; i < kPayloadCount; ++i)
    {
        auto payload            = std::make_unique<PostedPayloadDrainStressPayload>();
        payload->destroyedCount = &destroyedCount;
        Require(DxUi::PostMessagePayload(hwnd.get(), kPayloadMessage, 0, std::move(payload)), "each available registry slot accepts one payload");
    }

    auto rejected            = std::make_unique<PostedPayloadDrainStressPayload>();
    rejected->destroyedCount = &destroyedCount;
    SetLastError(ERROR_SUCCESS);
    Require(! DxUi::PostMessagePayload(hwnd.get(), kPayloadMessage, 0, std::move(rejected)), "the 129th outstanding payload is rejected");
    Require(GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "capacity failure reports a recoverable registry-full error");
    Require(destroyedCount.load(std::memory_order_acquire) == 1u, "the rejected payload is destroyed without entering the registry");

    MSG firstQueued{};
    Require(PeekMessageW(&firstQueued, hwnd.get(), kPayloadMessage, kPayloadMessage, PM_REMOVE) != 0, "one valid message is removed to free its registry slot");
    auto taken = DxUi::TakeMessagePayload<PostedPayloadDrainStressPayload>(hwnd.get(), kPayloadMessage, firstQueued.lParam);
    Require(taken != nullptr, "the freed slot is associated with a valid consumed payload");
    taken.reset();

    auto retry            = std::make_unique<PostedPayloadDrainStressPayload>();
    retry->destroyedCount = &destroyedCount;
    Require(DxUi::PostMessagePayload(hwnd.get(), kPayloadMessage, 0, std::move(retry)), "posting succeeds after a Take releases one bounded registry slot");
    hwnd.reset();
    Require(state.drainedCount.load(std::memory_order_acquire) == kPayloadCount, "teardown drains all 127 original entries and the successful retry");
    Require(destroyedCount.load(std::memory_order_acquire) == 130u, "all accepted, rejected, and retried payloads are destroyed exactly once");
}

void TestWindowHostAttachmentReservationIsExclusiveAndIdempotent()
{
    using namespace DxUi;
    const size_t baselineCount = DebugGetAttachedWindowHostCount();
    AttachedHostWindow first;
    AttachedHostWindow second;
    Require(DebugGetAttachedWindowHostCount() == baselineCount + 2u, "two independent HWNDs register two host attachments");

    Require(first.Host().Attach(first.Hwnd()), "repeating an attachment to the same HWND and options is idempotent");
    Require(DebugGetAttachedWindowHostCount() == baselineCount + 2u, "idempotent Attach does not double-count the host");

    {
        WindowHost competingHost;
        SetLastError(ERROR_SUCCESS);
        Require(! competingHost.Attach(first.Hwnd()), "a second host cannot reserve an HWND already owned by the first");
        Require(GetLastError() == ERROR_ALREADY_EXISTS, "duplicate HWND reservation is reported explicitly");
    }
    Require(DebugGetAttachedWindowHostCount() == baselineCount + 2u, "destroying the rejected host leaves both original registrations intact");
    Require(first.Host().GetHwnd() == first.Hwnd() && second.Host().GetHwnd() == second.Hwnd(), "both original hosts remain attached to their own HWNDs");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> firstProvider;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> secondProvider;
    firstProvider.attach(first.Host().DebugCreateAccessibilityProvider());
    secondProvider.attach(second.Host().DebugCreateAccessibilityProvider());
    Require(firstProvider != nullptr && secondProvider != nullptr && firstProvider.get() != secondProvider.get(),
            "both original hosts still create independent accessibility providers after duplicate rejection");
}

void TestWindowHostRejectedReattachmentPreservesBothHostsAndQueuedPayloads()
{
    using namespace DxUi;
    struct Payload final
    {
        uint32_t value = 0u;
    };
    constexpr UINT kPayloadMessage = WM_APP + 0x72u;
    AttachedHostWindow first;
    AttachedHostWindow second;
    auto post = [](HWND hwnd, uint32_t value)
    {
        auto payload   = std::make_unique<Payload>();
        payload->value = value;
        return PostMessagePayload(hwnd, kPayloadMessage, 0u, std::move(payload));
    };
    Require(post(first.Hwnd(), 11u) && post(second.Hwnd(), 22u), "both independent hosts accept an application payload");

    MSG firstMessage{};
    MSG secondMessage{};
    Require(PeekMessageW(&firstMessage, first.Hwnd(), kPayloadMessage, kPayloadMessage, PM_REMOVE) != 0 &&
                PeekMessageW(&secondMessage, second.Hwnd(), kPayloadMessage, kPayloadMessage, PM_REMOVE) != 0,
            "the test holds each token while checking failed reattachment");
    Require(! first.Host().Attach(second.Hwnd()), "a host cannot switch to an HWND owned by another attached host");
    Require(first.Host().GetHwnd() == first.Hwnd() && second.Host().GetHwnd() == second.Hwnd(), "rejected switching leaves both HWND associations intact");
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> firstProvider;
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> secondProvider;
    firstProvider.attach(first.Host().DebugCreateAccessibilityProvider());
    secondProvider.attach(second.Host().DebugCreateAccessibilityProvider());
    Require(firstProvider != nullptr && secondProvider != nullptr && firstProvider.get() != secondProvider.get(),
            "both hosts still resolve independent providers after a rejected HWND switch");

    auto firstPayload  = TakeMessagePayload<Payload>(first.Hwnd(), kPayloadMessage, firstMessage.lParam);
    auto secondPayload = TakeMessagePayload<Payload>(second.Hwnd(), kPayloadMessage, secondMessage.lParam);
    Require(firstPayload && firstPayload->value == 11u, "the original host's queued token remains valid after rejection");
    Require(secondPayload && secondPayload->value == 22u, "the destination host remains independently usable after rejection");
}

void TestWindowHostAttachmentCapacityRejectsThe129thAndRecovers()
{
    using namespace DxUi;
    constexpr size_t kHostCapacity = 128u;
    Require(DebugGetAttachedWindowHostCount() == 0u, "host-capacity test starts at the existing registry quiet point");

    std::vector<wil::unique_hwnd> windows;
    std::vector<std::unique_ptr<WindowHost>> hosts;
    windows.reserve(kHostCapacity + 1u);
    hosts.reserve(kHostCapacity + 1u);
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    for (size_t i = 0u; i < kHostCapacity + 1u; ++i)
    {
        HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, -32000, -32000, 1, 1, nullptr, nullptr, instance, nullptr);
        Require(hwnd != nullptr, "a hidden test HWND is created for each host-capacity attempt");
        windows.emplace_back(hwnd);
        hosts.push_back(std::make_unique<WindowHost>());
    }

    for (size_t i = 0u; i < kHostCapacity; ++i)
    {
        Require(hosts[i]->Attach(windows[i].get()), "the bounded registry accepts each of its 128 host slots");
    }
    Require(DebugGetAttachedWindowHostCount() == kHostCapacity, "the host registry reports all 128 live reservations");
    SetLastError(ERROR_SUCCESS);
    Require(! hosts[kHostCapacity]->Attach(windows[kHostCapacity].get()), "the 129th live host is rejected");
    Require(GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "host capacity exhaustion reports a retryable registry-full error");
    Require(DebugGetAttachedWindowHostCount() == kHostCapacity, "rejection does not alter the 128 existing reservations");

    hosts.front()->Detach();
    Require(hosts[kHostCapacity]->Attach(windows[kHostCapacity].get()), "attachment succeeds after one existing host releases its reservation");
    Require(DebugGetAttachedWindowHostCount() == kHostCapacity, "retry recovers the released slot without exceeding capacity");
    for (auto& host : hosts)
    {
        host->Detach();
    }
    Require(DebugGetAttachedWindowHostCount() == 0u, "the test releases every host registration before its HWNDs are destroyed");
}

void TestWindowHostAccessibilityRegistrationFailureRollsBackAndAllowsRetry()
{
    using namespace DxUi;
    AttachedHostWindow window;
    window.Host().Detach();
    const size_t hostCount     = DebugGetAttachedWindowHostCount();
    const uint32_t threadCount = DebugGetSharedWindowHostAttachmentCountForThread(GetCurrentThreadId());
    DebugFailNextWindowHostAccessibilityRegistrationForTest();
    Require(! window.Host().Attach(window.Hwnd()), "a missing accessibility registration rejects the incomplete attachment");
    Require(GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "registration failure preserves its error through rollback");
    Require(! window.Host().GetHwnd() && DebugGetAttachedWindowHostCount() == hostCount &&
                DebugGetSharedWindowHostAttachmentCountForThread(GetCurrentThreadId()) == threadCount,
            "failed registration rolls back the HWND reservation and shared graphics count");
    Require(! GetPropW(window.Hwnd(), kNativeAccessibilityTargetProperty), "failed registration leaves no stale accessibility target");
    Require(window.Host().Attach(window.Hwnd()), "a subsequent attachment retries after registration failure");
    window.Host().SetRoot(std::make_unique<Button>(L"Recovered attachment"));
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
    provider.attach(window.Host().DebugCreateAccessibilityProvider());
    Require(provider != nullptr, "the recovered attachment exposes its accessibility provider");
}

void TestWindowHostMouseMoveUpdatesHoverTarget()
{
    using namespace DxUi;

    WindowHost host;
    auto root = std::make_unique<Panel>();
    TrackingControlState firstState;
    TrackingControlState secondState;
    auto* first  = root->AddChild<TrackingControl>(firstState);
    auto* second = root->AddChild<TrackingControl>(secondState);
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 80.0f));
    first->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 28.0f));
    second->SetBounds(D2D1::RectF(110.0f, 0.0f, 210.0f, 28.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 80.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(12, 12), handled));
    Require(handled, "first hover move handled");
    Require(firstState.hoverEnterCount == 1u, "first control receives hover enter");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(128, 12), handled));
    Require(handled, "second hover move handled");
    Require(firstState.hoverLeaveCount == 1u, "first control receives hover leave when hover target changes");
    Require(secondState.hoverEnterCount == 1u, "second control receives hover enter when hover target changes");
}

[[nodiscard]] HWND CreateForeignPopupForWindowHostMouseLeaveTest(POINT screenPoint)
{
    static constexpr PCWSTR kClassName = L"DxUiTests.ForeignPopup";
    static const ATOM atom             = []() noexcept
    {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc   = DefWindowProcW;
        windowClass.hInstance     = GetModuleHandleW(nullptr);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = kClassName;
        return RegisterClassW(&windowClass);
    }();
    Require(atom != 0, "window host mouse-leave foreign popup class registers");

    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
                           kClassName,
                           L"DxUiTestsForeignPopup",
                           WS_POPUP | WS_VISIBLE,
                           screenPoint.x - 8,
                           screenPoint.y - 8,
                           48,
                           48,
                           nullptr,
                           nullptr,
                           GetModuleHandleW(nullptr),
                           nullptr);
}

void TestWindowHostMouseLeaveOverForeignPopupClearsHover()
{
    using namespace DxUi;

    AttachedHostWindow window;
    SetWindowPos(window.Hwnd(), nullptr, 180, 180, 320, 220, SWP_NOZORDER);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);

    auto root = std::make_unique<Panel>();
    TrackingControlState controlState;
    auto* control = root->AddChild<TrackingControl>(controlState);
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 220.0f));
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 80.0f));
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();

    constexpr LONG kHoverX = 32;
    constexpr LONG kHoverY = 32;
    static_cast<void>(SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(kHoverX, kHoverY)));
    Require(controlState.hoverEnterCount == 1u, "window host mouse-leave popup test starts with a hovered control");

    POINT popupPoint{kHoverX, kHoverY};
    Require(ClientToScreen(window.Hwnd(), &popupPoint) != FALSE, "window host mouse-leave popup point converts to screen coordinates");
    wil::unique_hwnd popup(CreateForeignPopupForWindowHostMouseLeaveTest(popupPoint));
    Require(popup != nullptr, "window host mouse-leave popup creates a foreign popup over the host");
    SetWindowPos(popup.get(), HWND_TOPMOST, popupPoint.x - 8, popupPoint.y - 8, 48, 48, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    UpdateWindow(popup.get());

    // The foreign popup is created over the host purely to document the scenario; the
    // production WM_MOUSELEAVE handler clears hover unconditionally (it reads neither the
    // live cursor nor WindowFromPoint), so the delivered WM_MOUSELEAVE alone exercises the
    // contract without warping the interactive user's cursor.
    static_cast<void>(SendMessageW(window.Hwnd(), WM_MOUSELEAVE, 0, 0));

    Require(controlState.hoverLeaveCount == 1u, "window host mouse leave over a foreign popup clears owner hover instead of rearming tracking");
}

void TestWindowHostMouseLeaveWithForeignCaptureClearsHover()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();
    TrackingControlState controlState;
    auto* control = root->AddChild<TrackingControl>(controlState);
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 220.0f));
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 80.0f));
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();

    constexpr LONG kHoverX = 32;
    constexpr LONG kHoverY = 32;
    static_cast<void>(SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(kHoverX, kHoverY)));
    Require(controlState.hoverEnterCount == 1u, "window host foreign-capture test starts with a hovered control");

    POINT popupPoint{260, 170};
    Require(ClientToScreen(window.Hwnd(), &popupPoint) != FALSE, "window host foreign-capture popup point converts to screen coordinates");
    wil::unique_hwnd popup(CreateForeignPopupForWindowHostMouseLeaveTest(popupPoint));
    Require(popup != nullptr, "window host foreign-capture test creates a foreign popup");
    SetWindowPos(popup.get(), HWND_TOPMOST, popupPoint.x - 8, popupPoint.y - 8, 48, 48, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    UpdateWindow(popup.get());

    SetCapture(popup.get());
    const auto releaseCapture = wil::scope_exit([&]() noexcept
    {
        if (GetCapture() == popup.get())
        {
            ReleaseCapture();
        }
    });
    Require(GetCapture() == popup.get(), "window host foreign-capture test gives capture to a non-owner hwnd");

    static_cast<void>(SendMessageW(window.Hwnd(), WM_MOUSELEAVE, 0, 0));

    Require(controlState.hoverLeaveCount == 1u, "window host mouse leave with foreign capture clears owner hover instead of rearming tracking");
}

void TestWindowHostTabTraversal()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* first  = root->AddChild<Button>(L"First");
    auto* second = root->AddChild<Button>(L"Second");
    auto* third  = root->AddChild<Button>(L"Third");
    first->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 24.0f));
    second->SetBounds(D2D1::RectF(0.0f, 28.0f, 80.0f, 52.0f));
    third->SetBounds(D2D1::RectF(0.0f, 56.0f, 80.0f, 80.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(first);
    Require(host.GetFocusControl() == first, "initial focus control");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab key handled");
    Require(host.GetFocusControl() == second, "tab moves focus to second control");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "second tab key handled");
    Require(host.GetFocusControl() == third, "tab moves focus to third control");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "third tab key handled");
    Require(host.GetFocusControl() == first, "tab traversal wraps to the first control");
}

void TestWindowHostShiftTabTraversal()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* first  = root->AddChild<Button>(L"First");
    auto* second = root->AddChild<Button>(L"Second");
    auto* third  = root->AddChild<Button>(L"Third");
    first->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 24.0f));
    second->SetBounds(D2D1::RectF(0.0f, 28.0f, 80.0f, 52.0f));
    third->SetBounds(D2D1::RectF(0.0f, 56.0f, 80.0f, 80.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(first);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab key handled");
    Require(host.GetFocusControl() == third, "shift+tab traversal wraps to the last control");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
}

void TestWindowHostTabBoundaryCallbackCanReplaceItself()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Only focus target");
    host.SetRoot(std::move(root));
    host.SetFocusControl(button);

    auto callbackText = std::make_shared<std::wstring>(L"tab callback survives replacement");
    std::wstring captureAfterReplacement;
    size_t originalCount    = 0u;
    size_t replacementCount = 0u;
    host.SetOnTabBoundary([&, callbackText = std::move(callbackText)](bool)
    {
        ++originalCount;
        host.SetOnTabBoundary([&](bool)
        {
            ++replacementCount;
            return true;
        });
        captureAfterReplacement = *callbackText;
        return true;
    });

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && originalCount == 1u, "tab-boundary callback handles the wrapped traversal once");
    Require(captureAfterReplacement == L"tab callback survives replacement", "the tab-boundary callback capture remains readable after self-replacement");
    Require(host.GetFocusControl() == button, "the handled boundary keeps logical focus on the original control");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && replacementCount == 1u, "the replacement tab-boundary callback remains installed");
}

void TestWindowHostTabBoundaryMutableCallbackRetainsState()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Only focus target");
    host.SetRoot(std::move(root));
    host.SetFocusControl(button);

    std::vector<size_t> observed;
    host.SetOnTabBoundary([count = size_t{0u}, &observed](bool) mutable
    {
        observed.push_back(++count);
        return true;
    });

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "the first wrapped Tab reaches the boundary callback");
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "the second wrapped Tab reaches the boundary callback");
    Require(observed == std::vector<size_t>{1u, 2u}, "the registered mutable tab-boundary callback retains its counter across notifications");
}

void TestWindowHostTabBoundaryCanRetireTheNextTarget()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Only focus target");
    host.SetRoot(std::move(root));
    host.SetFocusControl(button);

    auto callbackText = std::make_shared<std::wstring>(L"tab callback survives root retirement");
    std::wstring captureAfterRetirement;
    size_t callbackCount = 0u;
    host.SetOnTabBoundary([&, callbackText = std::move(callbackText)](bool)
    {
        ++callbackCount;
        host.SetRoot(std::make_unique<Panel>());
        captureAfterRetirement = *callbackText;
        return false;
    });

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && callbackCount == 1u, "a one-control tree reaches the tab boundary and retires its root");
    Require(captureAfterRetirement == L"tab callback survives root retirement", "the tab-boundary capture remains readable after root retirement");
    Require(host.GetRoot() != nullptr && host.GetFocusControl() == nullptr, "returning false after root retirement does not focus the destroyed next target");
}

void TestWindowHostTabBoundarySnapshotCleanupRetiresTheValidatedTarget()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Only focus target");
    host.SetRoot(std::move(root));
    host.SetFocusControl(button);

    size_t callbackCount = 0u;
    std::shared_ptr<int> payload(new int(0),
                                 [&host](int* value) noexcept
    {
        delete value;
        host.SetRoot({});
    });
    host.SetOnTabBoundary([&host, &callbackCount, payload = std::move(payload)](bool) mutable
    {
        static_cast<void>(payload);
        ++callbackCount;
        host.SetOnTabBoundary({});
        return false;
    });
    Require(! payload, "the retained callback is the only owner of its root-retirement payload");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled && callbackCount == 1u, "the callback declines the wrapped Tab after clearing its registration");
    Require(host.GetRoot() == nullptr && host.GetFocusControl() == nullptr,
            "releasing the last callback snapshot retires the previously validated target before refocus");
}

void TestWindowHostNativeFocusLossRetainsLogicalFocusForTraversal()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* first  = root->AddChild<Button>(L"First");
    auto* second = root->AddChild<Button>(L"Second");
    auto* third  = root->AddChild<Button>(L"Third");
    first->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 24.0f));
    second->SetBounds(D2D1::RectF(0.0f, 28.0f, 80.0f, 52.0f));
    third->SetBounds(D2D1::RectF(0.0f, 56.0f, 80.0f, 80.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(second);
    Require(host.GetFocusControl() == second, "native focus loss test starts from the second control");
    Require(second->HasFocus(), "native focus loss test starts with active focus visuals");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KILLFOCUS, 0, 0, handled));
    Require(handled, "native focus loss is handled");
    Require(host.GetFocusControl() == second, "native focus loss keeps the retained logical focus target");
    Require(! second->HasFocus(), "native focus loss clears active control focus visuals");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SETFOCUS, 0, 0, handled));
    Require(handled, "native focus regain is handled");
    Require(host.GetFocusControl() == second, "native focus regain keeps the retained logical focus target");
    Require(second->HasFocus(), "native focus regain restores active control focus visuals");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));
    Require(handled, "shift key down is tracked before native focus loss");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KILLFOCUS, 0, 0, handled));
    Require(handled, "second native focus loss is handled");
    Require(host.GetFocusControl() == second, "second native focus loss still keeps the retained logical focus target");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab traversal after native focus loss is handled");
    Require(host.GetFocusControl() == third, "tab traversal after native focus loss continues from the retained control");
}

void TestWindowHostReturnInvokesDefaultButtonWhenFocusedControlDoesNotOwnEnter()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* field  = root->AddChild<TextField>();
    auto* button = root->AddChild<Button>(L"Search");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    button->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));

    size_t clickCount = 0u;
    button->SetOnClick([&clickCount] { ++clickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(button);
    host.SetFocusControl(field);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled through host default-button routing");
    Require(clickCount == 1u, "default button invoked when focused field does not own enter");
}

void TestWindowHostReturnInvokesDefaultButtonWhenNoControlIsFocused()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"OK");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 28.0f));

    size_t clickCount = 0u;
    button->SetOnClick([&clickCount] { ++clickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(button);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled through host default-button routing with no focused control");
    Require(clickCount == 1u, "default button invoked when no focused control owns enter");
}

void TestWindowHostReturnDoesNotInvokeDefaultButtonWhenFocusedControlOwnsEnter()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* field  = root->AddChild<TextField>();
    auto* button = root->AddChild<Button>(L"Search");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    button->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));

    bool submitted    = false;
    size_t clickCount = 0u;
    field->SetOnSubmitted([&submitted] { submitted = true; });
    button->SetOnClick([&clickCount] { ++clickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(button);
    host.SetFocusControl(field);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled when focused field owns enter");
    Require(submitted, "focused field submit callback runs");
    Require(clickCount == 0u, "default button is not invoked when focused field owns enter");
}

void TestButtonKeyboardActivationCanReplaceRootSafely()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Apply");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 28.0f));

    bool clicked = false;
    button->SetOnClick([&]
    {
        clicked = true;
        host.SetRoot(std::make_unique<Panel>());
    });

    host.SetRoot(std::move(root));

    Require(button->OnKeyDown(host, VK_RETURN, 0), "button handles keyboard activation before replacing the root");
    Require(clicked, "button click callback runs before replacing the root");
    Require(host.GetRoot() != nullptr, "button keyboard activation can replace the root safely");
}

void TestWindowHostSpaceAndReturnInvokeFocusedButtonWithoutDefaultButtonFallback()
{
    using namespace DxUi;

    WindowHost host;
    auto root           = std::make_unique<Panel>();
    auto* focusedButton = root->AddChild<Button>(L"Apply");
    auto* defaultButton = root->AddChild<Button>(L"Search");
    focusedButton->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 28.0f));
    defaultButton->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));

    size_t focusedClickCount = 0u;
    size_t defaultClickCount = 0u;
    focusedButton->SetOnClick([&] { ++focusedClickCount; });
    defaultButton->SetOnClick([&] { ++defaultClickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(defaultButton);
    host.SetFocusControl(focusedButton);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "space handled by focused button");
    Require(focusedClickCount == 1u, "space invokes the focused button");
    Require(defaultClickCount == 0u, "space does not fall through to the default button");
    Require(host.GetInputModality() == InputModality::Keyboard, "space keeps keyboard input modality on the focused button");
    Require(host.IsKeyboardFocusVisible(), "space keeps keyboard focus visuals visible on the focused button");
    Require(host.GetFocusControl() == focusedButton, "space keeps focus on the focused button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused button");
    Require(focusedClickCount == 2u, "return invokes the focused button");
    Require(defaultClickCount == 0u, "return does not fall through to the default button");
    Require(host.GetFocusControl() == focusedButton, "return keeps focus on the focused button");
}

void TestWindowHostDpiChangedIsHandled()
{
    using namespace DxUi;

    WindowHost host;
    RECT suggestedRect{10, 12, 210, 112};
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_DPICHANGED, MAKELONG(144u, 144u), reinterpret_cast<LPARAM>(&suggestedRect), handled));
    Require(handled, "window host handles WM_DPICHANGED explicitly");
}

void TestWindowHostDpiChangedInvalidatesMultilineCachesAndResizesAttachedWindow()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>();
    field->SetMultiline(true);
    field->SetText(L"alpha line\nbeta line\ncharlie line\ndelta line\necho line");
    field->SetBounds(D2D1::RectF(16.0f, 16.0f, 280.0f, 150.0f));
    window.Host().SetRoot(std::move(root));

    const UINT widthPxBefore  = window.Host().DebugGetWidthPx();
    const UINT heightPxBefore = window.Host().DebugGetHeightPx();
    static_cast<void>(CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "attached host initial capture succeeds before dpi-change cache invalidation"));

    TextFieldDebugMultilineState beforeState{};
    Require(field->DebugGetMultilineState(window.Host(), beforeState), "multiline debug state available before dpi change");
    Require(beforeState.cachedLayoutPresent, "multiline text layout cache exists before dpi change");
    Require(! beforeState.layoutDirty, "multiline text layout cache is clean before dpi change");

    RECT windowRect{};
    Require(GetWindowRect(window.Hwnd(), &windowRect) != FALSE, "attached host window rect readable before dpi change");
    const LONG windowWidthPx  = std::max<LONG>(1, windowRect.right - windowRect.left);
    const LONG windowHeightPx = std::max<LONG>(1, windowRect.bottom - windowRect.top);
    RECT suggestedRect{windowRect.left,
                       windowRect.top,
                       windowRect.left + MulDiv(windowWidthPx, 144, USER_DEFAULT_SCREEN_DPI),
                       windowRect.top + MulDiv(windowHeightPx, 144, USER_DEFAULT_SCREEN_DPI)};

    const uint64_t invalidateCountBefore = window.Host().DebugGetInvalidateCount();
    bool handled                         = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKELONG(144u, 144u), reinterpret_cast<LPARAM>(&suggestedRect), handled));
    Require(handled, "attached host handles WM_DPICHANGED");
    Require(std::abs(window.Host().GetDpi() - 144.0f) <= 0.001f, "attached host dpi updates to the requested value");
    Require(window.Host().DebugGetInvalidateCount() > invalidateCountBefore, "dpi change invalidates the attached host");

    TextFieldDebugMultilineState invalidatedState{};
    Require(field->DebugGetMultilineState(window.Host(), invalidatedState), "multiline debug state available immediately after dpi change");
    Require(! invalidatedState.cachedLayoutPresent, "dpi change clears the retained multiline text layout cache");
    Require(invalidatedState.layoutDirty, "dpi change marks multiline layout dirty before the next paint");

    window.PumpMessages();
    static_cast<void>(CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "attached host capture succeeds after dpi-change cache invalidation"));

    TextFieldDebugMultilineState rebuiltState{};
    Require(field->DebugGetMultilineState(window.Host(), rebuiltState), "multiline debug state available after dpi-change repaint");
    Require(rebuiltState.cachedLayoutPresent, "multiline text layout cache is rebuilt on the first repaint after dpi change");
    Require(! rebuiltState.layoutDirty, "multiline text layout cache is clean again after repaint");
    Require(window.Host().DebugGetWidthPx() > widthPxBefore, "attached host client width grows after dpi-driven resize");
    Require(window.Host().DebugGetHeightPx() > heightPxBefore, "attached host client height grows after dpi-driven resize");
}

void TestWindowHostAttachedWindowsRenderAcrossUiThreads()
{
    using namespace DxUi;

    const auto installScene = [](AttachedHostWindow& window, std::wstring text)
    {
        auto root   = std::make_unique<Panel>();
        auto* label = root->AddChild<Label>(std::move(text));
        label->SetBounds(D2D1::RectF(16.0f, 16.0f, 220.0f, 48.0f));
        window.Host().SetRoot(std::move(root));
    };

    AttachedHostWindow primaryWindow;
    installScene(primaryWindow, L"Primary thread");
    const auto primaryCaptureBefore =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(primaryWindow, "primary thread attached host capture succeeds before worker-thread render");
    Require(primaryCaptureBefore.widthPx > 0u && primaryCaptureBefore.heightPx > 0u,
            "primary thread capture has non-zero dimensions before worker-thread render");
    Require(primaryWindow.Host().DebugGetRenderCount() > 0u, "primary thread host renders before worker-thread render");

    struct ThreadRenderResult
    {
        bool captureSucceeded = false;
        UINT widthPx          = 0u;
        UINT heightPx         = 0u;
        uint64_t renderCount  = 0u;
    };

    std::promise<ThreadRenderResult> resultPromise;
    std::future<ThreadRenderResult> resultFuture = resultPromise.get_future();
    std::thread worker([promise = std::move(resultPromise), installScene]() mutable
    {
        ThreadRenderResult result{};
        AttachedHostWindow workerWindow;
        installScene(workerWindow, L"Worker thread");
        const auto workerCapture = CaptureAttachedHostWindowBitmapForWindowHostSuite(workerWindow, "worker thread attached host capture succeeds");
        result.captureSucceeded  = true;
        result.widthPx           = workerCapture.widthPx;
        result.heightPx          = workerCapture.heightPx;
        result.renderCount       = workerWindow.Host().DebugGetRenderCount();
        promise.set_value(result);
    });

    const ThreadRenderResult workerResult = resultFuture.get();
    worker.join();

    Require(workerResult.captureSucceeded, "worker thread attached host capture completed");
    Require(workerResult.widthPx > 0u && workerResult.heightPx > 0u, "worker thread capture has non-zero dimensions");
    Require(workerResult.renderCount > 0u, "worker thread host renders on its own UI thread");

    const auto primaryCaptureAfter =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(primaryWindow, "primary thread attached host capture still succeeds after worker-thread render");
    Require(primaryCaptureAfter.widthPx > 0u && primaryCaptureAfter.heightPx > 0u, "primary thread capture has non-zero dimensions after worker-thread render");
    Require(primaryWindow.Host().DebugGetRenderCount() > 1u, "primary thread host renders again after worker-thread render");
}

void TestWindowHostEscapeInvokesCancelButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root          = std::make_unique<Panel>();
    auto* focusButton  = root->AddChild<Button>(L"Other");
    auto* cancelButton = root->AddChild<Button>(L"Cancel");
    focusButton->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 28.0f));
    cancelButton->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));

    size_t cancelCount = 0u;
    cancelButton->SetOnClick([&cancelCount] { ++cancelCount; });

    host.SetRoot(std::move(root));
    host.SetCancelButton(cancelButton);
    host.SetFocusControl(focusButton);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "escape handled through host cancel-button routing");
    Require(cancelCount == 1u, "cancel button invoked from escape");
}

void TestWindowHostEscapeCallbackCanReplaceItself()
{
    using namespace DxUi;

    WindowHost host;
    auto callbackText = std::make_shared<std::wstring>(L"escape callback survives replacement");
    std::wstring captureAfterReplacement;
    size_t originalCount    = 0u;
    size_t replacementCount = 0u;
    host.SetOnEscape([&, callbackText = std::move(callbackText)]
    {
        ++originalCount;
        host.SetOnEscape([&]
        {
            ++replacementCount;
            return true;
        });
        captureAfterReplacement = *callbackText;
        return true;
    });

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled && originalCount == 1u, "the original Escape callback handles its key once");
    Require(captureAfterReplacement == L"escape callback survives replacement", "the Escape callback capture remains readable after self-replacement");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled && replacementCount == 1u, "the replacement Escape callback remains installed");
}

void TestWindowHostEscapeMutableCallbackRetainsState()
{
    using namespace DxUi;

    WindowHost host;
    std::vector<size_t> observed;
    host.SetOnEscape([count = size_t{0u}, &observed]() mutable
    {
        observed.push_back(++count);
        return true;
    });

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "the first Escape reaches the callback");
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "the second Escape reaches the callback");
    Require(observed == std::vector<size_t>{1u, 2u}, "the registered mutable Escape callback retains its counter across notifications");
}

void TestNativeMenuBarRefreshMutableCallbackRetainsState()
{
    using namespace DxUi;

    AttachedHostWindow owner;
    wil::unique_hmenu nativeMenu{CreateMenu()};
    Require(nativeMenu != nullptr, "the native menu refresh fixture creates a menu");
    Require(AppendMenuW(nativeMenu.get(), MF_STRING, 7301u, L"&File") != FALSE, "the native menu refresh fixture populates its menu");

    NativeMenuBarHost menuBar;
    Require(menuBar.Attach(GetModuleHandleW(nullptr), owner.Hwnd(), nativeMenu.get()), "the native menu refresh fixture attaches its menu-bar host");

    std::vector<size_t> observed;
    menuBar.SetRefreshMenuStateCallback([count = size_t{0u}, &observed]() mutable { observed.push_back(++count); });
    menuBar.SyncMenuModel();
    menuBar.SyncMenuModel();
    Require(observed == std::vector<size_t>{1u, 2u}, "the registered mutable native-menu refresh callback retains state across synchronization");
}

void TestWindowHostFocusChangedMutableCallbackRetainsState()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* first  = root->AddChild<Button>(L"First");
    auto* second = root->AddChild<Button>(L"Second");
    host.SetRoot(std::move(root));

    std::vector<size_t> observed;
    host.SetOnFocusChanged([count = size_t{0u}, &observed](Control*) mutable { observed.push_back(++count); });
    host.SetFocusControl(first, false);
    host.SetFocusControl(second, false);
    Require(observed == std::vector<size_t>{1u, 2u}, "the registered mutable focus callback retains its counter across notifications");
}

void TestWindowHostLogicalEditorFocusCanAvoidNativeActivation()
{
    using namespace DxUi;

    AttachedHostWindow window;
    static_cast<void>(ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE));
    const HWND foregroundBefore = GetForegroundWindow();
    const HWND focusBefore      = GetFocus();

    window.Host().SetTextInputBackend(TextInputBackend::Native);
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"logical focus");
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(field, false);

    NativeTextInputState state{};
    Require(window.Host().GetFocusControl() == field && field->HasFocus(), "the editor receives logical focus without native activation");
    Require(window.Host().HasActiveTextInput() && window.Host().TryReadNativeTextInputState(field, state) && state.text == L"logical focus",
            "the logically focused editor publishes its native text-input cache while remaining nonactivating");
    Require(GetForegroundWindow() == foregroundBefore && GetFocus() == focusBefore,
            "suppressing native focus transfer preserves the existing foreground and thread-focus windows");

    window.Host().SetFocusControl(field, false);
    window.Host().SyncTextInput(field);
    Require(window.Host().GetFocusControl() == field && field->HasFocus() && window.Host().TryReadNativeTextInputState(field, state) &&
                state.text == L"logical focus",
            "idempotent focus and explicit text synchronization retain the active editor cache");
    Require(GetForegroundWindow() == foregroundBefore && GetFocus() == focusBefore,
            "idempotent focus and text synchronization still do not activate or focus the HWND");
}

void TestWindowHostEscapeClosesComboPopupBeforeCancelButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root          = std::make_unique<Panel>();
    auto* combo        = root->AddChild<ComboBox>();
    auto* cancelButton = root->AddChild<Button>(L"Cancel");
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    cancelButton->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    size_t cancelCount = 0u;
    cancelButton->SetOnClick([&cancelCount] { ++cancelCount; });

    host.SetRoot(std::move(root));
    host.SetCancelButton(cancelButton);
    host.SetFocusControl(combo);

    Require(combo->OnKeyDown(host, VK_RETURN, 0), "combo enter opens popup before escape test");
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "combo popup is open before escape");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "escape handled while combo popup is open");
    Require(cancelCount == 0u, "cancel button not invoked while popup-owned escape is handled");
    Require(combo->GetHitBounds().bottom == combo->GetBounds().bottom, "escape closes combo popup first");
}

void TestWindowHostMenuKeyInvokesFocusedButtonContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Run");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));

    RecordingContextMenuInvocation contextMenu;
    button->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 48.0f));
    host.SetFocusControl(button);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused button");
    Require(contextMenu.count == 1u, "menu key invokes button context menu once");
    Require(contextMenu.lastKeyboardInvocation, "button menu key reports keyboard invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{16, 16}, "button menu key uses the button keyboard anchor");
    Require(host.GetFocusControl() == button, "button menu key keeps focus on the button");
}

void TestWindowHostShiftF10InvokesFocusedToggleContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* toggle = root->AddChild<Toggle>(L"Menu bar");
    toggle->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));
    toggle->SetChecked(true);

    RecordingContextMenuInvocation contextMenu;
    toggle->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 56.0f));
    host.SetFocusControl(toggle);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_F10, 0, handled));
    Require(handled, "shift+f10 handled for focused toggle");
    Require(contextMenu.count == 1u, "shift+f10 invokes toggle context menu once");
    Require(contextMenu.lastKeyboardInvocation, "toggle shift+f10 reports keyboard invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{16, 20}, "toggle shift+f10 uses the toggle keyboard anchor");
    Require(toggle->IsChecked(), "toggle shift+f10 does not change the checked state");
    Require(host.GetFocusControl() == toggle, "toggle shift+f10 keeps focus on the toggle");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
}

void TestWindowHostMenuKeyInvokesFocusedCheckboxContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* checkbox = root->AddChild<Checkbox>(L"Selected");
    checkbox->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    checkbox->SetChecked(true);

    RecordingContextMenuInvocation contextMenu;
    checkbox->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 48.0f));
    host.SetFocusControl(checkbox);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused checkbox");
    Require(contextMenu.count == 1u, "menu key invokes checkbox context menu once");
    Require(contextMenu.lastKeyboardInvocation, "checkbox menu key reports keyboard invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{16, 16}, "checkbox menu key uses the checkbox keyboard anchor");
    Require(checkbox->IsChecked(), "checkbox menu key does not change the checked state");
    Require(host.GetFocusControl() == checkbox, "checkbox menu key keeps focus on the checkbox");
}

void TestWindowHostSpaceAndReturnToggleFocusedToggleWithoutDefaultButtonFallback()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* toggle = root->AddChild<Toggle>(L"Ascending");
    auto* button = root->AddChild<Button>(L"Apply");
    toggle->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));
    button->SetBounds(D2D1::RectF(0.0f, 48.0f, 100.0f, 76.0f));

    size_t toggleCount       = 0u;
    size_t defaultClickCount = 0u;
    toggle->SetOnToggled([&](bool) { ++toggleCount; });
    button->SetOnClick([&] { ++defaultClickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(button);
    host.SetFocusControl(toggle);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "space handled by focused toggle");
    Require(toggle->IsChecked(), "space toggles the focused toggle on");
    Require(toggleCount == 1u, "space fires the toggle callback once");
    Require(defaultClickCount == 0u, "space does not fall through to the default button");
    Require(host.GetInputModality() == InputModality::Keyboard, "space keeps keyboard input modality");
    Require(host.IsKeyboardFocusVisible(), "space keeps keyboard focus visuals visible");
    Require(host.GetFocusControl() == toggle, "space keeps focus on the toggle");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused toggle");
    Require(! toggle->IsChecked(), "return toggles the focused toggle off");
    Require(toggleCount == 2u, "return fires the toggle callback once");
    Require(defaultClickCount == 0u, "return does not fall through to the default button");
    Require(host.GetFocusControl() == toggle, "return keeps focus on the toggle");
}

void TestWindowHostSpaceTogglesFocusedCheckboxAndReturnInvokesDefaultButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* checkbox = root->AddChild<Checkbox>(L"Selected");
    auto* button   = root->AddChild<Button>(L"Apply");
    checkbox->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    button->SetBounds(D2D1::RectF(0.0f, 40.0f, 100.0f, 68.0f));

    size_t toggleCount       = 0u;
    size_t defaultClickCount = 0u;
    checkbox->SetOnToggled([&](bool) { ++toggleCount; });
    button->SetOnClick([&] { ++defaultClickCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(button);
    host.SetFocusControl(checkbox);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "space handled by focused checkbox");
    Require(checkbox->IsChecked(), "space toggles the focused checkbox on");
    Require(toggleCount == 1u, "space fires the checkbox callback once");
    Require(defaultClickCount == 0u, "space does not fall through to the default button");
    Require(host.GetInputModality() == InputModality::Keyboard, "space keeps keyboard input modality for the checkbox");
    Require(host.IsKeyboardFocusVisible(), "space keeps keyboard focus visuals visible for the checkbox");
    Require(host.GetFocusControl() == checkbox, "space keeps focus on the checkbox");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused checkbox through the host default button");
    Require(checkbox->IsChecked(), "return does not toggle the focused checkbox off");
    Require(toggleCount == 1u, "return does not fire the checkbox callback");
    Require(defaultClickCount == 1u, "return falls through to the default button for the checkbox");
    Require(host.GetFocusControl() == checkbox, "return keeps focus on the checkbox");
}

void TestWindowHostMixedDialogKeyboardFlowKeepsCommandsOnFocusedControls()
{
    using namespace DxUi;

    WindowHost host;
    auto root           = std::make_unique<Panel>();
    auto* field         = root->AddChild<TextField>();
    auto* combo         = root->AddChild<ComboBox>();
    auto* checkbox      = root->AddChild<Checkbox>(L"Selected");
    auto* toggle        = root->AddChild<Toggle>(L"Ascending");
    auto* applyButton   = root->AddChild<Button>(L"Apply");
    auto* defaultButton = root->AddChild<Button>(L"Search");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    combo->SetBounds(D2D1::RectF(0.0f, 36.0f, 180.0f, 64.0f));
    checkbox->SetBounds(D2D1::RectF(0.0f, 72.0f, 220.0f, 104.0f));
    toggle->SetBounds(D2D1::RectF(0.0f, 112.0f, 220.0f, 152.0f));
    applyButton->SetBounds(D2D1::RectF(0.0f, 160.0f, 100.0f, 188.0f));
    defaultButton->SetBounds(D2D1::RectF(108.0f, 160.0f, 208.0f, 188.0f));
    combo->SetItems({ComboBox::Item{L"alpha", L"Alpha"}, ComboBox::Item{L"beta", L"Beta"}});

    size_t defaultClickCount   = 0u;
    size_t applyClickCount     = 0u;
    size_t checkboxToggleCount = 0u;
    size_t toggleCount         = 0u;
    defaultButton->SetOnClick([&] { ++defaultClickCount; });
    applyButton->SetOnClick([&] { ++applyClickCount; });
    checkbox->SetOnToggled([&](bool) { ++checkboxToggleCount; });
    toggle->SetOnToggled([&](bool) { ++toggleCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(defaultButton);
    host.SetFocusControl(field);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled from focused text field in mixed dialog flow");
    Require(defaultClickCount == 1u, "focused text field falls back to the default button in mixed dialog flow");
    Require(applyClickCount == 0u, "text field return does not invoke the non-default command button");
    Require(host.GetFocusControl() == field, "text field return keeps focus on the field");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from text field handled in mixed dialog flow");
    Require(host.GetFocusControl() == combo, "tab advances focus from text field to combo");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "space handled by focused combo in mixed dialog flow");
    Require(combo->DebugIsPopupOpen(), "focused combo opens its popup in mixed dialog flow");
    Require(defaultClickCount == 1u, "combo space does not leak to the default button in mixed dialog flow");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused combo in mixed dialog flow");
    Require(! combo->DebugIsPopupOpen(), "focused combo closes its popup in mixed dialog flow");
    Require(defaultClickCount == 1u, "combo return does not leak to the default button in mixed dialog flow");
    Require(host.GetFocusControl() == combo, "combo command routing keeps focus on the combo");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from combo handled in mixed dialog flow");
    Require(host.GetFocusControl() == checkbox, "tab advances focus from combo to checkbox");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SPACE, 0, handled));
    Require(handled, "space handled by focused checkbox in mixed dialog flow");
    Require(checkbox->IsChecked(), "focused checkbox toggles in mixed dialog flow");
    Require(checkboxToggleCount == 1u, "checkbox callback fires once in mixed dialog flow");
    Require(defaultClickCount == 1u, "checkbox space does not leak to the default button in mixed dialog flow");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled from focused checkbox in mixed dialog flow");
    Require(checkbox->IsChecked(), "focused checkbox return does not toggle in mixed dialog flow");
    Require(checkboxToggleCount == 1u, "checkbox return does not fire the checkbox callback in mixed dialog flow");
    Require(defaultClickCount == 2u, "focused checkbox return invokes the default button in mixed dialog flow");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from checkbox handled in mixed dialog flow");
    Require(host.GetFocusControl() == toggle, "tab advances focus from checkbox to toggle");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused toggle in mixed dialog flow");
    Require(toggle->IsChecked(), "focused toggle switches on in mixed dialog flow");
    Require(toggleCount == 1u, "toggle callback fires once in mixed dialog flow");
    Require(defaultClickCount == 2u, "toggle return does not leak to the default button in mixed dialog flow");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from toggle handled in mixed dialog flow");
    Require(host.GetFocusControl() == applyButton, "tab advances focus from toggle to command button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return handled by focused command button in mixed dialog flow");
    Require(applyClickCount == 1u, "focused command button invokes itself in mixed dialog flow");
    Require(defaultClickCount == 2u, "focused command button return does not leak to the default button in mixed dialog flow");
    Require(host.GetInputModality() == InputModality::Keyboard, "mixed dialog flow stays in keyboard modality");
    Require(host.IsKeyboardFocusVisible(), "mixed dialog flow preserves keyboard focus visuals");
    Require(host.GetFocusControl() == applyButton, "focused command button keeps focus after invocation in mixed dialog flow");
}

void TestWindowHostMixedDialogMouseFlowKeepsCommandsOnHitControls()
{
    using namespace DxUi;

    WindowHost host;
    auto root           = std::make_unique<Panel>();
    auto* field         = root->AddChild<TextField>();
    auto* combo         = root->AddChild<ComboBox>();
    auto* checkbox      = root->AddChild<Checkbox>(L"Selected");
    auto* toggle        = root->AddChild<Toggle>(L"Ascending");
    auto* applyButton   = root->AddChild<Button>(L"Apply");
    auto* defaultButton = root->AddChild<Button>(L"Search");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    combo->SetBounds(D2D1::RectF(0.0f, 36.0f, 180.0f, 64.0f));
    checkbox->SetBounds(D2D1::RectF(0.0f, 72.0f, 220.0f, 104.0f));
    toggle->SetBounds(D2D1::RectF(0.0f, 112.0f, 220.0f, 152.0f));
    applyButton->SetBounds(D2D1::RectF(0.0f, 160.0f, 100.0f, 188.0f));
    defaultButton->SetBounds(D2D1::RectF(108.0f, 160.0f, 208.0f, 188.0f));
    combo->SetItems({ComboBox::Item{L"alpha", L"Alpha"}, ComboBox::Item{L"beta", L"Beta"}});

    size_t defaultClickCount   = 0u;
    size_t applyClickCount     = 0u;
    size_t checkboxToggleCount = 0u;
    size_t toggleCount         = 0u;
    defaultButton->SetOnClick([&] { ++defaultClickCount; });
    applyButton->SetOnClick([&] { ++applyClickCount; });
    checkbox->SetOnToggled([&](bool) { ++checkboxToggleCount; });
    toggle->SetOnToggled([&](bool) { ++toggleCount; });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 204.0f));
    host.SetDefaultButton(defaultButton);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, 'A', 0, handled));
    Require(host.IsKeyboardFocusVisible(), "keyboard modality is active before mixed mouse-flow coverage");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(16, 14), handled));
    Require(handled, "text field click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(16, 14), handled));
    Require(host.GetFocusControl() == field, "text field click moves focus to the text field");
    Require(host.GetInputModality() == InputModality::Pointer, "text field click switches modality back to pointer");
    Require(! host.IsKeyboardFocusVisible(), "text field click clears keyboard focus visuals in mixed mouse flow");
    Require(defaultClickCount == 0u, "text field click does not invoke the default button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(172, 50), handled));
    Require(handled, "combo click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(172, 50), handled));
    Require(combo->DebugIsPopupOpen(), "combo click opens the popup in mixed mouse flow");
    Require(host.GetFocusControl() == combo, "combo click moves focus to the combo");
    Require(defaultClickCount == 0u, "combo click does not invoke the default button");

    const D2D1_RECT_F popupItemRect = combo->DebugGetPopupItemRect(1u, &host);
    const LONG popupItemX           = static_cast<LONG>((popupItemRect.left + popupItemRect.right) * 0.5f);
    const LONG popupItemY           = static_cast<LONG>((popupItemRect.top + popupItemRect.bottom) * 0.5f);
    handled                         = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(popupItemX, popupItemY), handled));
    Require(handled, "combo popup row click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(popupItemX, popupItemY), handled));
    Require(! combo->DebugIsPopupOpen(), "combo popup row click closes the popup in mixed mouse flow");
    Require(combo->GetSelectedIndex().has_value() && combo->GetSelectedIndex().value() == 1u, "combo popup row click commits the hit item in mixed mouse flow");
    Require(defaultClickCount == 0u, "combo popup row click does not invoke the default button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(28, 88), handled));
    Require(handled, "checkbox click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(28, 88), handled));
    Require(checkbox->IsChecked(), "checkbox click toggles the checkbox on in mixed mouse flow");
    Require(checkboxToggleCount == 1u, "checkbox click fires the checkbox callback once in mixed mouse flow");
    Require(host.GetFocusControl() == checkbox, "checkbox click moves focus to the checkbox");
    Require(defaultClickCount == 0u, "checkbox click does not invoke the default button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(32, 132), handled));
    Require(handled, "toggle click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(32, 132), handled));
    Require(toggle->IsChecked(), "toggle click toggles the switch on in mixed mouse flow");
    Require(toggleCount == 1u, "toggle click fires the toggle callback once in mixed mouse flow");
    Require(host.GetFocusControl() == toggle, "toggle click moves focus to the toggle");
    Require(defaultClickCount == 0u, "toggle click does not invoke the default button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(48, 174), handled));
    Require(handled, "command button click handles mouse-down in mixed mouse flow");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(48, 174), handled));
    Require(applyClickCount == 1u, "command button click invokes only the hit command button in mixed mouse flow");
    Require(defaultClickCount == 0u, "command button click does not fall through to the default button in mixed mouse flow");
    Require(host.GetFocusControl() == applyButton, "command button click moves focus to the hit button");
}

void TestWindowHostMenuKeyInvokesFocusedTreeContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 10u, .text = L"General"},
        TreeItemData{.id = 20u, .text = L"Viewers"},
        TreeItemData{.id = 30u, .text = L"Themes"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    tree->SetSelectedItemId(20u);
    host.SetFocusControl(tree);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused tree");
    Require(delegate.contextMenuCount == 1u, "menu key invokes tree context menu once");
    Require(delegate.lastContextMenuItemId == 20u, "menu key targets the selected tree item");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{16, 44}, "menu key uses a stable selected-item anchor");
}

void TestWindowHostShiftF10InvokesFocusedTreeContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    MutableTreeModel model;
    model.SetVisibleItems({
        TreeItemData{.id = 10u, .text = L"General"},
        TreeItemData{.id = 20u, .text = L"Viewers"},
        TreeItemData{.id = 30u, .text = L"Themes"},
    });

    RecordingTreeDelegate delegate;
    tree->SetModel(&model);
    tree->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    tree->SetSelectedItemId(30u);
    host.SetFocusControl(tree);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_F10, 0, handled));
    Require(handled, "shift+f10 handled for focused tree");
    Require(delegate.contextMenuCount == 1u, "shift+f10 invokes tree context menu once");
    Require(delegate.lastContextMenuItemId == 30u, "shift+f10 targets the selected tree item");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{16, 72}, "shift+f10 uses the selected-item keyboard anchor");
}

void TestWindowHostMenuKeyInvokesFocusedGridContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    Require(grid->RequestSelectRow(1u, 0u), "the context-menu fixture initializes row selection and independent focus");
    host.SetFocusControl(grid);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused grid");
    Require(delegate.contextMenuCount == 1u, "menu key invokes grid context menu once");
    Require(delegate.lastContextMenuRow == 1u, "menu key targets the focused grid row");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{16, 74}, "menu key uses a stable selected-row anchor");
}

void TestWindowHostShiftF10InvokesFocusedGridContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    MultiRowGridModel model(3u);
    RecordingGridDelegate delegate;
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    host.SetRoot(std::move(root));
    Require(grid->RequestSelectRow(2u, 0u), "the Shift+F10 fixture initializes row selection and independent focus");
    host.SetFocusControl(grid);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_F10, 0, handled));
    Require(handled, "shift+f10 handled for focused grid");
    Require(delegate.contextMenuCount == 1u, "shift+f10 invokes grid context menu once");
    Require(delegate.lastContextMenuRow == 2u, "shift+f10 targets the focused grid row");
    RequirePointNear(delegate.lastContextMenuPoint, POINT{16, 102}, "shift+f10 uses the selected-row keyboard anchor");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
}

void TestWindowHostMenuKeyInvokesFocusedTextFieldContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<TextField>(L"alpha");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));

    RecordingContextMenuInvocation contextMenu;
    field->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 40.0f));
    host.SetFocusControl(field);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused text field");
    Require(contextMenu.count == 1u, "menu key invokes text field context menu once");
    Require(contextMenu.lastKeyboardInvocation, "text field menu key reports keyboard invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{16, 14}, "text field menu key uses the text field keyboard anchor");
    Require(host.GetFocusControl() == field, "text field menu key keeps focus on the text field");
}

void TestWindowHostMenuKeyInvokesFocusedComboContextMenu()
{
    using namespace DxUi;

    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    combo->SetItems({ComboBox::Item{L"alpha", L"Alpha"}, ComboBox::Item{L"beta", L"Beta"}});
    combo->SetSelectedIndex(0u);

    RecordingContextMenuInvocation contextMenu;
    combo->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 40.0f));
    host.SetFocusControl(combo);

    Require(! combo->DebugIsPopupOpen(), "non-editable combo popup starts closed for menu-key context menu");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_APPS, 0, handled));
    Require(handled, "menu key handled for focused non-editable combo");
    Require(contextMenu.count == 1u, "menu key invokes non-editable combo context menu once");
    Require(contextMenu.lastKeyboardInvocation, "non-editable combo menu key reports keyboard invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{16, 14}, "non-editable combo menu key uses the combo keyboard anchor");
    Require(host.GetFocusControl() == combo, "non-editable combo menu key keeps focus on the combo");
    Require(! combo->DebugIsPopupOpen(), "non-editable combo menu key does not open the popup");
}

void TestWindowHostSetRootClearsDestroyedTreeInteractionState()
{
    using namespace DxUi;

    WindowHost host;
    TrackingControlState oldState;
    auto oldRoot     = std::make_unique<Panel>();
    auto* oldControl = oldRoot->AddChild<TrackingControl>(oldState);
    oldControl->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    host.SetRoot(std::move(oldRoot));
    host.SetFocusControl(oldControl);
    host.CaptureMouse(oldControl);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(8, 8), handled));
    Require(handled, "initial hover update handled before root swap");
    Require(oldState.focusGainCount == 1u, "old control receives focus before root swap");
    Require(oldState.hoverEnterCount == 1u, "old control receives hover before root swap");

    auto replacementRoot = std::make_unique<Panel>();
    host.SetRoot(std::move(replacementRoot));
    Require(host.GetFocusControl() == nullptr, "set root clears focused control from destroyed tree");
    Require(oldState.focusLossCount == 1u, "set root notifies old focused control about focus loss");
    Require(oldState.hoverLeaveCount == 1u, "set root clears hovered control from destroyed tree");
    Require(oldState.mouseLeaveCount == 1u, "set root issues a mouse-leave to the old hovered control");

    const size_t mouseMoveCountBefore = oldState.mouseMoveCount;
    const size_t mouseUpCountBefore   = oldState.mouseUpCount;

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(12, 12), handled));
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(12, 12), handled));
    Require(oldState.mouseMoveCount == mouseMoveCountBefore, "stale hovered control is not reused after root swap");
    Require(oldState.mouseUpCount == mouseUpCountBefore, "stale captured control is not reused after root swap");
}

void TestWindowHostPromotesAChildBeforeDestroyingItsOldParent()
{
    using namespace DxUi;
    class Probe final : public Control
    {
    public:
        using Control::GetHost;
        using Control::GetParent;
        void Paint(ControlHost&) const override
        {
        }
    };
    WindowHost host;
    auto parent = std::make_unique<Panel>();
    parent->SetFlowDirection(FlowDirection::RightToLeft);
    parent->SetDensity(Density::Compact);
    Probe* child = parent->AddChild<Probe>();
    host.SetRoot(std::move(parent));
    Require(child->GetFlowDirection() == FlowDirection::RightToLeft && child->GetDensity() == Density::Compact,
            "the child initially inherits its parent's layout policies");
    auto promoted = std::move(static_cast<Panel*>(host.GetRoot())->GetChildren().front());
    host.SetRoot(std::move(promoted));
    Require(host.GetRoot() == child && child->GetParent() == nullptr && child->GetHost() == &host, "the promoted child is the host root with no stale parent");
    Require(child->GetFlowDirection() == FlowDirection::LeftToRight && child->GetDensity() == host.GetTheme().density,
            "the promoted child now inherits the root policies");
}

void TestWindowHostPromotionSurvivesResetCallbacksRetiringTheParent()
{
    using namespace DxUi;
    class Probe final : public Control
    {
    public:
        using Control::GetHost;
        using Control::GetParent;
        void Paint(ControlHost&) const override
        {
        }
    };
    class FocusProbe final : public Control
    {
    public:
        FocusProbe(bool& armed, size_t& losses, bool detach) noexcept : _armed(armed), _losses(losses), _detach(detach)
        {
            SetFocusable(true);
        }
        void Paint(ControlHost&) const override
        {
        }

    protected:
        void OnFocusChanged(ControlHost& host, bool focused) override
        {
            Control::OnFocusChanged(host, focused);
            if (! focused && std::exchange(_armed, false))
            {
                ++_losses;
                const bool detach = _detach;
                if (detach)
                    host.Detach();
                else
                    host.SetRoot({});
            }
        }

    private:
        bool& _armed;
        size_t& _losses;
        bool _detach;
    };
    for (const bool detach : std::array{false, true})
    {
        WindowHost host;
        bool armed    = false;
        size_t losses = 0u;
        auto parent   = std::make_unique<Panel>();
        parent->SetFlowDirection(FlowDirection::RightToLeft);
        parent->SetDensity(Density::Compact);
        auto* child = parent->AddChild<Probe>();
        auto* focus = parent->AddChild<FocusProbe>(armed, losses, detach);
        host.SetRoot(std::move(parent));
        host.SetFocusControl(focus, false);
        auto promoted = std::move(static_cast<Panel*>(host.GetRoot())->GetChildren().front());
        armed         = true;
        host.SetRoot(std::move(promoted));
        Require(losses == 1u && ! armed, "the reset callback retires the original parent exactly once");
        Require(host.GetRoot() == child && child->GetParent() == nullptr && child->GetHost() == &host,
                "promotion survives reset callbacks that replace or detach the host tree");
        Require(child->GetFlowDirection() == FlowDirection::LeftToRight && child->GetDensity() == host.GetTheme().density,
                "the promoted child resolves inheritance through its new host");
    }
}

void TestWindowHostPromotionStopsWhenAnInheritanceCallbackReplacesTheRoot()
{
    using namespace DxUi;
    class Probe final : public Control
    {
    public:
        Probe(WindowHost& host, bool& armed, size_t& densityChanges) noexcept : _host(host), _armed(armed), _densityChanges(densityChanges)
        {
        }
        void Paint(ControlHost&) const override
        {
        }

    protected:
        void OnFlowDirectionChanged() noexcept override
        {
            if (_armed)
            {
                _armed = false;
                _host.SetRoot({});
            }
        }
        void OnDensityChanged() noexcept override
        {
            ++_densityChanges;
        }

    private:
        WindowHost& _host;
        bool& _armed;
        size_t& _densityChanges;
    };
    WindowHost host;
    bool armed            = false;
    size_t densityChanges = 0u;
    auto parent           = std::make_unique<Panel>();
    parent->SetFlowDirection(FlowDirection::RightToLeft);
    parent->SetDensity(Density::Compact);
    parent->AddChild<Probe>(host, armed, densityChanges);
    host.SetRoot(std::move(parent));
    const size_t before = densityChanges;
    auto promoted       = std::move(static_cast<Panel*>(host.GetRoot())->GetChildren().front());
    armed               = true;
    host.SetRoot(std::move(promoted));
    Require(! armed && ! host.GetRoot(), "the inheritance callback's replacement root survives promotion");
    Require(densityChanges == before, "promotion sends no later inheritance notification to the destroyed control");
}

void TestWindowHostPointerFocusCallbackCanDestroyTheClickedControl()
{
    using namespace DxUi;
    class ClickControl final : public Control
    {
    public:
        ClickControl()
        {
            SetFocusable(true);
        }
        void Paint(ControlHost&) const override
        {
        }
        bool OnMouseDown(ControlHost&, D2D1_POINT_2F, bool, UINT) override
        {
            return true;
        }
    };
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* clicked = root->AddChild<ClickControl>();
    clicked->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f));
    host.SetRoot(std::move(root));
    host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 120.0f));
    bool replaced = false;
    host.SetOnFocusChanged([&](Control* control)
    {
        if (control == clicked)
        {
            replaced = true;
            host.SetRoot(std::make_unique<Panel>());
        }
    });
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 16), handled));
    Require(handled && replaced, "pointer focus invokes the rebuilding callback");
    Require(host.GetFocusControl() == nullptr && host.GetCapturedControl() == nullptr, "the destroyed click target is neither focused nor captured");
}

void TestWindowHostTakesTheRequestedLifetimeBeforePruningFocus()
{
    using namespace DxUi;
    class RebuildingBlurControl final : public Control
    {
    public:
        explicit RebuildingBlurControl(bool& armed) : _armed(armed)
        {
            SetFocusable(true);
        }
        void Paint(ControlHost&) const override
        {
        }

    protected:
        void OnFocusChanged(ControlHost& host, bool focused) override
        {
            Control::OnFocusChanged(host, focused);
            if (! focused && _armed)
            {
                _armed = false;
                host.SetRoot(std::make_unique<Panel>());
            }
        }

    private:
        bool& _armed;
    };
    WindowHost host;
    bool armed      = false;
    auto root       = std::make_unique<Panel>();
    auto* oldFocus  = root->AddChild<RebuildingBlurControl>(armed);
    auto* requested = root->AddChild<Button>(L"Next");
    host.SetRoot(std::move(root));
    host.SetFocusControl(oldFocus, false);
    oldFocus->SetEnabled(false);
    armed = true;
    host.SetFocusControl(requested, false);
    Require(! armed && host.GetFocusControl() == nullptr, "pruning may destroy the requested target before focus transfers");
}

void TestWindowHostNewerCallbackFocusRequestWins()
{
    using namespace DxUi;
    class FocusCallbackControl final : public Control
    {
    public:
        FocusCallbackControl()
        {
            SetFocusable(true);
        }
        std::function<void(ControlHost&, bool)> callback;
        void Paint(ControlHost&) const override
        {
        }
        void OnFocusChanged(ControlHost& host, bool focused) override
        {
            Control::OnFocusChanged(host, focused);
            if (callback)
                callback(host, focused);
        }
    };
    for (unsigned phase = 0u; phase < 3u; ++phase)
    {
        WindowHost host;
        auto root       = std::make_unique<Panel>();
        auto* previous  = root->AddChild<FocusCallbackControl>();
        auto* requested = root->AddChild<FocusCallbackControl>();
        auto* winner    = root->AddChild<Button>(L"Callback choice");
        host.SetRoot(std::move(root));
        host.SetFocusControl(previous, false);
        size_t callbackCount = 0u;
        const auto redirect  = [&](ControlHost& callbackHost)
        {
            ++callbackCount;
            callbackHost.SetFocusControl(winner, false);
            // The exception fallback must also leave the newer focus alone.
            throw std::runtime_error("failed outer callback after choosing focus");
        };
        if (phase == 0u)
            previous->callback = [&](ControlHost& callbackHost, bool focused)
            {
                if (! focused)
                    redirect(callbackHost);
            };
        else if (phase == 1u)
            requested->callback = [&](ControlHost& callbackHost, bool focused)
            {
                if (focused)
                    redirect(callbackHost);
            };
        else
            host.SetOnFocusChanged([&](Control* notified)
            {
                if (notified == requested)
                    redirect(host);
            });
        host.SetFocusControl(requested, false);
        Require(callbackCount == 1u && host.GetFocusControl() == winner, "newer focus from loss, gain or host notification survives the outer request");
        Require(winner->HasFocus() && ! previous->HasFocus() && ! requested->HasFocus(), "only the callback-selected control acknowledges focus");
    }
}

void TestWindowHostFailedBlurBeforeBasePreservesOnlyTheNewFocus()
{
    using namespace DxUi;
    class BlurBeforeBaseControl final : public Control
    {
    public:
        BlurBeforeBaseControl()
        {
            SetFocusable(true);
        }
        std::function<void(ControlHost&)> onBlur;
        void Paint(ControlHost&) const override
        {
        }
        void OnFocusChanged(ControlHost& host, bool focused) override
        {
            if (! focused && onBlur)
                onBlur(host);
            Control::OnFocusChanged(host, focused);
        }
    };
    for (bool refocusSame : {false, true})
    {
        WindowHost host;
        auto root             = std::make_unique<Panel>();
        auto* previous        = root->AddChild<BlurBeforeBaseControl>();
        auto* requested       = root->AddChild<Button>(L"Outer choice");
        auto* other           = root->AddChild<Button>(L"Callback choice");
        Control* const winner = refocusSame ? static_cast<Control*>(previous) : other;
        host.SetRoot(std::move(root));
        host.SetFocusControl(previous, false);
        previous->onBlur = [&](ControlHost& callbackHost)
        {
            callbackHost.SetFocusControl(winner, false);
            throw std::runtime_error("blur failed before base acknowledgement");
        };
        host.SetFocusControl(requested, false);
        Require(host.GetFocusControl() == winner && winner->HasFocus(), "a before-base throwing blur preserves the newer callback choice");
        Require(previous->HasFocus() == refocusSame && ! requested->HasFocus() && other->HasFocus() == ! refocusSame,
                "the failed old blur is acknowledged unless that same old target was explicitly refocused");
        previous->onBlur = {};
    }
}

void TestWindowHostResetPreservesExplicitFocusAndRootReplacementRetiresOldChoice()
{
    using namespace DxUi;
    class CallbackControl final : public Control
    {
    public:
        CallbackControl()
        {
            SetFocusable(true);
        }
        std::function<void(ControlHost&)> onBlur;
        std::function<void(bool)> onDestroy;
        ~CallbackControl() noexcept override
        {
            if (onDestroy)
                onDestroy(HasFocus());
        }
        void Paint(ControlHost&) const override
        {
        }
        void OnFocusChanged(ControlHost& host, bool focused) override
        {
            Control::OnFocusChanged(host, focused);
            if (! focused && onBlur)
                onBlur(host);
        }
    };
    WindowHost host;
    auto root       = std::make_unique<Panel>();
    auto* previous  = root->AddChild<CallbackControl>();
    auto* oldChoice = root->AddChild<CallbackControl>();
    host.SetRoot(std::move(root));
    host.SetFocusControl(previous, false);
    previous->onBlur = [&](ControlHost& callbackHost) { callbackHost.SetFocusControl(previous, false); };
    host.ResetInteractionState();
    Require(host.GetFocusControl() == previous && previous->HasFocus(), "an explicit same-control refocus during reset survives the snapshotted blur");
    previous->onBlur              = [&](ControlHost& callbackHost) { callbackHost.SetFocusControl(oldChoice, false); };
    bool retiredOldChoiceHadFocus = true;
    size_t retiredBlurCount       = 0u;
    auto replacement              = std::make_unique<Panel>();
    auto* successor               = replacement->AddChild<Button>(L"New root successor");
    oldChoice->onBlur             = [&](ControlHost& callbackHost)
    {
        ++retiredBlurCount;
        callbackHost.SetFocusControl(successor, false);
    };
    oldChoice->onDestroy = [&](bool focused) { retiredOldChoiceHadFocus = focused; };
    host.SetRoot(std::move(replacement));
    Require(retiredBlurCount == 1u && ! retiredOldChoiceHadFocus, "the reset-selected old-tree control loses focus before its owner is released");
    Require(host.GetFocusControl() == successor && successor->HasFocus(), "old-tree retirement preserves a successor chosen in the installed new root");
}

void TestWindowHostThrowingFocusCallbacksAcknowledgeTransitionsAndRetirement()
{
    using namespace DxUi;
    class ThrowingFocusControl final : public Control
    {
    public:
        ThrowingFocusControl()
        {
            SetFocusable(true);
        }
        size_t notificationCount = 0u;
        void Paint(ControlHost&) const override
        {
        }
        void OnFocusChanged(ControlHost&, bool) override
        {
            ++notificationCount;
            throw std::runtime_error("focus override failed before base acknowledgement");
        }
    };
    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* control = root->AddChild<ThrowingFocusControl>();
    host.SetRoot(std::move(root));
    host.SetFocusControl(control, false);
    Require(host.GetFocusControl() == control && control->HasFocus() && control->notificationCount == 1u,
            "a failing gain callback falls back to base focus acknowledgement");
    host.ResetInteractionState();
    Require(! host.GetFocusControl() && ! control->HasFocus() && control->notificationCount == 2u,
            "a failing reset callback leaves logical and acknowledged focus cleared");
    host.SetOnFocusChanged([&](Control*)
    {
        host.SetRoot(std::make_unique<Panel>());
        throw std::runtime_error("focus notification retired the target before failing");
    });
    host.SetFocusControl(control, false);
    Require(host.GetFocusControl() == nullptr, "a failing host callback can retire its notified control safely");
}

void TestWindowHostDetachDeactivatesSecureTextInputBeforeDestroyingRoot()
{
    using namespace DxUi;

    class DestructionTrackedTextField final : public TextField
    {
    public:
        DestructionTrackedTextField(std::wstring text, size_t& destroyedCount) : TextField(std::move(text)), _destroyedCount(&destroyedCount)
        {
        }

        ~DestructionTrackedTextField() noexcept override
        {
            ++*_destroyedCount;
        }

    private:
        size_t* _destroyedCount = nullptr;
    };

    WindowHost host;
    size_t destroyedCount = 0u;
    auto root             = std::make_unique<Panel>();
    auto* secretField     = root->AddChild<DestructionTrackedTextField>(std::wstring(L"credential-secret"), destroyedCount);
    secretField->SetMasked(true);
    secretField->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 28.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(secretField);

    NativeTextInputState activeState{};
    Require(host.GetFocusControl() == secretField, "secure text field starts focused before detach");
    Require(host.HasActiveTextInput(), "secure text field has an active text-input session before detach");
    Require(host.DebugHasActiveNativeTextInputSession(), "secure text field has an active native text-input session before detach");
    Require(host.DebugGetNativeTextInputState(activeState), "secure text field exports native text-input state before detach");
    Require(activeState.text == L"credential-secret", "secure text field native cache contains the secret before detach");

    host.Detach();

    NativeTextInputState detachedState{};
    Require(destroyedCount == 1u, "detach destroys the secure text field through retained root ownership");
    Require(host.GetFocusControl() == nullptr, "detach clears focus before retained secure text field storage is destroyed");
    Require(! host.HasActiveTextInput(), "detach clears text-input session before retained secure text field storage is destroyed");
    Require(! host.DebugHasActiveNativeTextInputSession(), "detach clears native text-input session before retained secure text field storage is destroyed");
    Require(! host.DebugGetNativeTextInputState(detachedState), "detach clears the native text-input cache for destroyed secure fields");
}

void TestWindowHostProcessExitDetachAbandonsRetainedControlObserversBeforeNativeTeardown()
{
    using namespace DxUi;

    struct ProcessExitTextFieldState final
    {
        size_t destroyedCount = 0u;
        size_t focusLossCount = 0u;
    };

    class ProcessExitTextField final : public TextField
    {
    public:
        explicit ProcessExitTextField(ProcessExitTextFieldState& state) : TextField(L"credential-secret"), _state(&state)
        {
            SetMasked(true);
        }

        ~ProcessExitTextField() noexcept override
        {
            ++_state->destroyedCount;
        }

    protected:
        void OnFocusChanged(WindowHost& host, bool focused) override
        {
            if (! focused)
            {
                ++_state->focusLossCount;
            }
            TextField::OnFocusChanged(host, focused);
        }

    private:
        ProcessExitTextFieldState* _state = nullptr;
    };

    const DWORD ownerThreadId                   = GetCurrentThreadId();
    const size_t baselineAttachedHostCount      = DebugGetAttachedWindowHostCount();
    const uint32_t baselineOwnerAttachmentCount = DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId);

    ProcessExitTextFieldState fieldState;
    TrackingControlState hoverState;
    SelfCapturingControlState captureState;
    AttachedHostWindow window;
    static_cast<void>(ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE));
    auto root            = std::make_unique<Panel>();
    auto* secretField    = root->AddChild<ProcessExitTextField>(fieldState);
    auto* hoverControl   = root->AddChild<TrackingControl>(hoverState);
    auto* captureControl = root->AddChild<SelfCapturingControl>(captureState);
    secretField->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 28.0f));
    hoverControl->SetBounds(D2D1::RectF(0.0f, 40.0f, 120.0f, 72.0f));
    captureControl->SetBounds(D2D1::RectF(0.0f, 80.0f, 120.0f, 112.0f));

    size_t focusCallbackCount = 0u;
    window.Host().SetOnFocusChanged([&](Control*) { ++focusCallbackCount; });
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(secretField, false);
    const size_t focusCallbackCountBeforeDetach = focusCallbackCount;
    bool handled                                = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(8, 48), handled));
    window.Host().CaptureMouse(captureControl);

    NativeTextInputState activeState{};
    Require(window.Host().HasActiveTextInput(), "process-exit detach starts with an active text-input session");
    Require(window.Host().DebugGetNativeTextInputState(activeState), "process-exit detach starts with a native text-input cache");
    Require(activeState.text == L"credential-secret", "process-exit detach test cache contains the secret before teardown");
    Require(hoverState.hoverEnterCount == 1u, "process-exit detach starts with a retained hover observer");
    Require(window.Host().GetCapturedControl() == captureControl, "process-exit detach starts with a retained capture observer");

    window.Host().DebugDetachForProcessExit();

    NativeTextInputState detachedState{};
    Require(fieldState.destroyedCount == 1u, "process-exit detach destroys the retained secure text field");
    Require(fieldState.focusLossCount == 0u, "process-exit detach does not dispatch focus loss through an abandoned control observer");
    Require(hoverState.hoverLeaveCount == 0u && hoverState.mouseLeaveCount == 0u,
            "process-exit detach does not dispatch hover callbacks through an abandoned control observer");
    Require(captureState.captureLostCount == 0u, "process-exit detach clears capture before synchronous capture-change dispatch");
    Require(focusCallbackCount == focusCallbackCountBeforeDetach, "process-exit detach clears host focus callbacks before retained-tree teardown");
    Require(window.Host().GetRoot() == nullptr, "process-exit detach releases retained root ownership");
    Require(window.Host().GetFocusControl() == nullptr && window.Host().GetCapturedControl() == nullptr,
            "process-exit detach clears retained focus and capture observers");
    Require(! window.Host().HasActiveTextInput(), "process-exit detach clears the native text-input session");
    Require(! window.Host().DebugHasActiveNativeTextInputSession(), "process-exit detach clears the native control observer");
    Require(! window.Host().DebugGetNativeTextInputState(detachedState), "process-exit detach securely clears the native text-input cache");
    Require(DebugGetAttachedWindowHostCount() == baselineAttachedHostCount, "process-exit detach removes the host from the attached-host registry");
    Require(DebugGetSharedWindowHostAttachmentCountForThread(ownerThreadId) == baselineOwnerAttachmentCount,
            "process-exit detach releases the owner-thread graphics attachment");
}

void TestWindowHostClearChildrenPrunesDestroyedTreeInteractionState()
{
    using namespace DxUi;

    WindowHost host;
    TrackingControlState oldState;
    auto root        = std::make_unique<Panel>();
    auto* rootPanel  = root.get();
    auto* oldControl = root->AddChild<TrackingControl>(oldState);
    oldControl->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(oldControl);
    host.CaptureMouse(oldControl);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(8, 8), handled));
    Require(handled, "initial hover update handled before child clear");
    Require(host.GetFocusControl() == oldControl, "focus starts on the installed child control");

    rootPanel->ClearChildren();

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(12, 12), handled));
    Require(host.GetFocusControl() == nullptr, "clearing children prunes focus from destroyed controls on the next message");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(12, 12), handled));
    Require(oldState.mouseUpCount == 0u, "clearing children does not reuse stale captured controls after prune");
}

void TestWindowHostKeyDownCallbackDisablingFocusPrunesBeforePostDispatchSync()
{
    using namespace DxUi;

    WindowHost host;
    auto root = std::make_unique<Panel>();
    ReentrantKeyboardMutationState state;
    auto* control = root->AddChild<ReentrantKeyboardMutationControl>(state);
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(control);
    Require(host.GetFocusControl() == control, "reentrant key-down test starts with the mutating control focused");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, 'A', 0, handled));
    Require(handled, "focused mutating control handles key down");
    Require(state.keyDownCount == 1u, "focused mutating control receives one key-down callback");
    Require(host.GetFocusControl() == nullptr, "key-down callback disabling the focus target prunes retained focus before post-dispatch sync");
}

void TestWindowHostCharCallbackDisablingFocusPrunesBeforePostDispatchSync()
{
    using namespace DxUi;

    WindowHost host;
    auto root = std::make_unique<Panel>();
    ReentrantKeyboardMutationState state;
    auto* control = root->AddChild<ReentrantKeyboardMutationControl>(state);
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(control);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_CHAR, L'x', 0, handled));
    Require(handled, "focused mutating control handles char input");
    Require(state.charCount == 1u, "focused mutating control receives one char callback");
    Require(host.GetFocusControl() == nullptr, "char callback disabling the focus target prunes retained focus before post-dispatch sync");
}

void TestWindowHostFocusLossCallbackRevalidatesRequestedFocusTarget()
{
    using namespace DxUi;

    WindowHost host;
    auto root                 = std::make_unique<Panel>();
    Control* nextFocusControl = nullptr;
    auto* oldFocus            = root->AddChild<FocusLossMutatesNextFocusControl>(nextFocusControl);
    auto* nextFocus           = root->AddChild<Button>(L"Next");
    nextFocusControl          = nextFocus;
    oldFocus->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    nextFocus->SetBounds(D2D1::RectF(0.0f, 40.0f, 120.0f, 72.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(oldFocus);
    Require(host.GetFocusControl() == oldFocus, "focus-loss mutation test starts with old focus");

    host.SetFocusControl(nextFocus);
    Require(! nextFocus->IsEnabled(), "old focus loss callback disables the requested next focus control");
    Require(host.GetFocusControl() == nullptr, "focus change revalidates the requested focus target after focus-loss callbacks");
}

void TestWindowHostIgnoresObserverButtonsOutsideInstalledRoot()
{
    using namespace DxUi;

    WindowHost host;
    auto root           = std::make_unique<Panel>();
    auto* field         = root->AddChild<TextField>();
    auto* defaultButton = root->AddChild<Button>(L"Search");
    auto* cancelButton  = root->AddChild<Button>(L"Cancel");
    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    defaultButton->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));
    cancelButton->SetBounds(D2D1::RectF(108.0f, 36.0f, 208.0f, 64.0f));

    Button outsideDefault(L"Outside Default");
    Button outsideCancel(L"Outside Cancel");

    size_t defaultCount        = 0u;
    size_t cancelCount         = 0u;
    size_t outsideDefaultCount = 0u;
    size_t outsideCancelCount  = 0u;
    defaultButton->SetOnClick([&defaultCount] { ++defaultCount; });
    cancelButton->SetOnClick([&cancelCount] { ++cancelCount; });
    outsideDefault.SetOnClick([&outsideDefaultCount] { ++outsideDefaultCount; });
    outsideCancel.SetOnClick([&outsideCancelCount] { ++outsideCancelCount; });

    host.SetRoot(std::move(root));
    host.SetDefaultButton(defaultButton);
    host.SetCancelButton(cancelButton);
    host.SetDefaultButton(&outsideDefault);
    host.SetCancelButton(&outsideCancel);
    host.SetFocusControl(field);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RETURN, 0, handled));
    Require(handled, "return remains routed to the installed-root default button");
    Require(defaultCount == 1u, "outside default button does not replace the valid default button");
    Require(outsideDefaultCount == 0u, "outside default button is ignored");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "escape remains routed to the installed-root cancel button");
    Require(cancelCount == 1u, "outside cancel button does not replace the valid cancel button");
    Require(outsideCancelCount == 0u, "outside cancel button is ignored");
}

void TestWindowHostIgnoresFocusAndCaptureOutsideInstalledRoot()
{
    using namespace DxUi;

    WindowHost host;
    TrackingControlState insideState;
    TrackingControlState outsideState;

    auto root           = std::make_unique<Panel>();
    auto* insideControl = root->AddChild<TrackingControl>(insideState);
    insideControl->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    TrackingControl outsideControl(outsideState);
    outsideControl.SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));

    host.SetRoot(std::move(root));
    host.SetFocusControl(insideControl);
    Require(host.GetFocusControl() == insideControl, "focus starts on an installed-root control");
    Require(insideState.focusGainCount == 1u, "installed-root control receives focus");

    host.SetFocusControl(&outsideControl);
    Require(host.GetFocusControl() == insideControl, "outside control does not replace focused installed-root control");
    Require(outsideState.focusGainCount == 0u, "outside control does not receive focus");
    Require(insideState.focusLossCount == 0u, "installed-root focus is preserved when outside control is ignored");

    host.CaptureMouse(insideControl);
    host.CaptureMouse(&outsideControl);
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(8, 8), handled));
    Require(handled, "mouse move remains handled by the installed-root captured control");
    Require(insideState.mouseMoveCount == 1u, "installed-root captured control handles mouse move");
    Require(outsideState.mouseMoveCount == 0u, "outside captured control is ignored");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(8, 8), handled));
    Require(outsideState.mouseUpCount == 0u, "outside captured control does not receive mouse-up");
}

void TestWindowHostCaptureLossClearsPressedButtonState()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<ExposedButton>(L"Apply");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 16), handled));
    Require(handled, "capture-loss button test handles mouse-down");
    Require(button->IsPressed(), "capture-loss button test enters pressed state after mouse-down");

    handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_CAPTURECHANGED, 0, 0, handled));
    Require(handled, "capture-loss button test handles capture change");
    Require(! button->IsPressed(), "capture-loss button test clears pressed state when capture is lost");
}

void TestWindowHostResetInteractionStateNotifiesCapturedControl()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<ExposedButton>(L"Apply");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 32.0f));
    window.Host().SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 16), handled));
    Require(handled, "interaction reset test handles mouse-down");
    Require(button->IsPressed() && window.Host().GetCapturedControl() == button && GetCapture() == window.Hwnd(),
            "interaction reset test starts with a pressed captured button");

    window.Host().ResetInteractionState();

    Require(! button->IsPressed(), "interaction reset notifies the captured control so it clears pressed state");
    Require(window.Host().GetCapturedControl() == nullptr && GetCapture() != window.Hwnd(), "interaction reset clears retained and Win32 mouse capture");
}

void TestWindowHostRedundantCaptureDoesNotCancelMouseDownCapture()
{
    using namespace DxUi;

    AttachedHostWindow window;
    SelfCapturingControlState state;
    auto root     = std::make_unique<Panel>();
    auto* control = root->AddChild<SelfCapturingControl>(state);
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f));
    window.Host().SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 16), handled));
    Require(handled, "self-capturing control mouse-down is handled");
    Require(state.mouseDownCount == 1u, "self-capturing control receives one mouse-down");
    Require(state.captureLostCount == 0u, "self-capturing control keeps capture after WindowHost post-handler capture");
    Require(state.dragging, "self-capturing control remains in dragging state after mouse-down");

    handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(24, 56), handled));
    Require(handled, "self-capturing control captured mouse-move is handled");
    Require(state.mouseMoveWhileDownCount == 1u, "self-capturing control receives captured mouse-move while dragging");

    handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(24, 56), handled));
    Require(handled, "self-capturing control mouse-up is handled");
    Require(! state.dragging, "self-capturing control clears dragging state on mouse-up");
}

void TestWindowHostRenderSurvivesForcedNullSolidBrushes()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();

    auto* menuBar = root->AddChild<MenuBar>();
    menuBar->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 32.0f));
    menuBar->SetItems({MenuBarItem{.text = L"File", .mnemonic = L'F'}, MenuBarItem{.text = L"View", .mnemonic = L'V'}});

    auto* toolbar = root->AddChild<Toolbar>();
    toolbar->SetBounds(D2D1::RectF(0.0f, 32.0f, 320.0f, 64.0f));
    toolbar->AddButton(L"Copy", L"\xE8C8");
    toolbar->AddSeparator();
    toolbar->AddToggleButton(L"Bold", L"\xE8DD");

    auto* button = root->AddChild<Button>(L"Apply");
    button->SetBounds(D2D1::RectF(12.0f, 76.0f, 120.0f, 108.0f));

    auto* toggle = root->AddChild<Toggle>(L"Feature");
    toggle->SetBounds(D2D1::RectF(132.0f, 76.0f, 308.0f, 108.0f));
    toggle->SetChecked(true);

    auto* checkbox = root->AddChild<Checkbox>(L"Remember");
    checkbox->SetBounds(D2D1::RectF(12.0f, 116.0f, 160.0f, 144.0f));
    checkbox->SetChecked(true);

    auto* radio = root->AddChild<RadioButton>(L"Daily");
    radio->SetBounds(D2D1::RectF(176.0f, 116.0f, 308.0f, 144.0f));
    radio->SetChecked(true);

    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(12.0f, 156.0f, 220.0f, 184.0f));
    slider->SetTickMarks({0.0, 50.0, 100.0});
    slider->SetValue(50.0);

    auto* status = root->AddChild<StatusStrip>();
    status->SetBounds(D2D1::RectF(0.0f, 186.0f, 320.0f, 208.0f));
    status->SetSections({StatusStrip::Section{.text = L"Ready", .widthDip = 0.0f}, StatusStrip::Section{.text = L"UTF-8", .widthDip = 80.0f}});

    auto* tabs = root->AddChild<TabControl>();
    tabs->AddTab<Label>(L"General", L"Pane");
    tabs->SetBounds(D2D1::RectF(0.0f, 208.0f, 320.0f, 320.0f));

    window.Host().SetSmokeOverlayVisible(true);
    window.Host().SetRoot(std::move(root));
    window.Host().DebugSetForceNullSolidBrushes(true);

    const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
    const DxUi::WindowHostBitmapCapture capture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "forced-null solid brush render completes without crashing");
    Require(capture.widthPx > 0u && capture.heightPx > 0u, "forced-null solid brush render still produces a capture");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "forced-null solid brush render does not introduce present failures");

    window.Host().DebugSetForceNullSolidBrushes(false);
}

void TestWindowHostEditorControlsSurviveForcedNullSolidBrushes()
{
    using namespace DxUi;

    // The editor controls draw separators, grips, markers and a focused field through their own brush helpers.
    // A null solid brush (brush failure or device loss) must skip those draws instead of reaching Direct2D.
    AttachedHostWindow window;
    auto root     = std::make_unique<Panel>();
    auto* picker  = root->AddChild<ColorPicker>();
    auto* stepper = root->AddChild<NumericStepper>();
    auto* split   = root->AddChild<Splitter>();
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, ColorPicker::kDefaultWidthDip, ColorPicker::kDefaultHeightDip));
    picker->SetColor(0xFF3A7BD5u);
    stepper->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 8.0f + NumericStepper::kDefaultHeightDip));
    stepper->SetLabel(L"X", 14.0f);
    stepper->SetUnit(L"px", 18.0f);
    split->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 160.0f));
    split->SetPosition(120.0f);
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(&stepper->Field());
    window.Host().DebugSetForceNullSolidBrushes(true);

    const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
    const WindowHostBitmapCapture capture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "forced-null solid brush editor render completes without crashing");
    Require(capture.widthPx > 0u && capture.heightPx > 0u, "forced-null solid brush editor render still produces a capture");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "forced-null solid brush editor render does not introduce present failures");

    window.Host().DebugSetForceNullSolidBrushes(false);
}

void TestWindowHostGridPaintSurvivesForcedNullSolidBrushes()
{
    using namespace DxUi;
    class BrushFailureGridModel final : public IGridModel
    {
    public:
        [[nodiscard]] size_t GetRowCount() const noexcept override
        {
            return 4u;
        }
        [[nodiscard]] size_t GetColumnCount() const noexcept override
        {
            return 2u;
        }
        [[nodiscard]] GridColumnDesc GetColumn(size_t columnIndex) const override
        {
            return GridColumnDesc{
                .id       = columnIndex == 0u ? L"name" : L"status",
                .title    = columnIndex == 0u ? L"Name" : L"Status",
                .widthDip = 132.0f,
            };
        }
        void GetCellData(size_t rowIndex, size_t columnIndex, GridCellData& outCell) const override
        {
            if (rowIndex == 2u && columnIndex == 1u)
            {
                outCell.kind     = GridCellKind::Marquee;
                outCell.progress = 0.42f;
                outCell.text     = L"42%";
                ++marqueeCellReads;
                return;
            }
            outCell.kind = GridCellKind::Text;
            outCell.text = std::format(L"Row {}", rowIndex + 1u);
        }
        [[nodiscard]] size_t GetGroupCount() const noexcept override
        {
            return 1u;
        }
        [[nodiscard]] GridGroupDesc GetGroup(size_t /*groupIndex*/) const override
        {
            return GridGroupDesc{.stableId = 10u, .title = L"Operations", .startRowIndex = 0u, .rowCount = 4u};
        }
        [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
        {
            return static_cast<uint64_t>(rowIndex + 1u);
        }
        [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
        {
            return rowId >= 1u && rowId <= GetRowCount() ? std::optional<size_t>(static_cast<size_t>(rowId - 1u)) : std::nullopt;
        }

        mutable size_t marqueeCellReads = 0u;
    };

    const auto runScene = [](GridVisualMode mode)
    {
        BrushFailureGridModel model;
        AttachedHostWindow window;
        auto root  = std::make_unique<Panel>();
        auto* grid = root->AddChild<Grid>();
        grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 292.0f, 188.0f));
        grid->SetHeaderHeightDip(28.0f);
        grid->SetRowHeightDip(32.0f);
        grid->SetVisualMode(mode);
        grid->SetSelectionMode(GridSelectionMode::Extended);
        grid->SetModel(&model);
        grid->GetSelectionModel().SetSingle(model.GetStableRowId(2u));
        window.Host().SetRoot(std::move(root));
        static_cast<Panel*>(window.Host().GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));

        const GridVisibleWorkMetrics metrics = grid->GetVisibleWorkMetrics();
        Require(metrics.visibleGroupHeaderCount == 1u, "forced-null Grid scene has its grouped header in the viewport");
        Require(metrics.visibleRowCount >= 3u && metrics.visibleColumnCount == 2u, "forced-null Grid scene exposes selected rows and column separators");
        GridDebugRowVisualState selectedState{};
        Require(grid->DebugGetRowVisualState(window.Host().GetTheme(), 2u, selectedState) && selectedState.selected,
                "forced-null Grid scene has a selected row to paint");

        const size_t readsBeforeCapture = model.marqueeCellReads;
        const WindowHostBitmapCapture normalCapture =
            CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Grid baseline render succeeds before null-brush injection");
        Require(normalCapture.widthPx > 0u && normalCapture.heightPx > 0u, "Grid baseline capture is nonempty");
        Require(model.marqueeCellReads > readsBeforeCapture, "Grid baseline render paints the marquee progress cell");

        const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
        window.Host().DebugSetForceNullSolidBrushes(true);
        const WindowHostBitmapCapture nullBrushCapture =
            CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Grid paint completes with every solid brush forced null");
        window.Host().DebugSetForceNullSolidBrushes(false);
        Require(nullBrushCapture.widthPx > 0u && nullBrushCapture.heightPx > 0u, "Grid null-brush capture is nonempty");
        Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "Grid null-brush paint introduces no presentation failures");

        const WindowHostBitmapCapture recoveredCapture =
            CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Grid rendering recovers after null-brush injection");
        Require(recoveredCapture.widthPx > 0u && recoveredCapture.heightPx > 0u, "Grid recovery capture is nonempty");
        Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "Grid recovery introduces no presentation failures");
    };

    runScene(GridVisualMode::Standard);
    runScene(GridVisualMode::FolderView);
}

void TestWindowHostTreeFocusOutlineSurvivesForcedNullSolidBrushes()
{
    using namespace DxUi;
    MutableTreeModel model;
    model.SetVisibleItems(
        {TreeItemData{.id = 10u, .text = L"General"}, TreeItemData{.id = 20u, .text = L"Display"}, TreeItemData{.id = 30u, .text = L"Accessibility"}});
    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 292.0f, 188.0f));
    tree->SetModel(&model);
    tree->SetMultiSelectEnabled(true);
    tree->SetSelectedItemIds(std::vector<uint64_t>{10u, 30u});
    tree->SetFocusedItemId(20u);
    window.Host().SetRoot(std::move(root));
    static_cast<Panel*>(window.Host().GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    window.Host().SetFocusControl(tree, false);

    // Set keyboard modality through the host's message dispatcher; this does not activate or focus the HWND.
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_KEYDOWN, VK_SHIFT, 0, handled));
    handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_KEYUP, VK_SHIFT, 0, handled));
    Require(window.Host().GetFocusControl() == tree && window.Host().IsKeyboardFocusVisible(),
            "Tree scene has logical keyboard focus without native activation");
    Require(tree->MultiSelectEnabled() && tree->GetSelectedItemIds().size() == 2u, "Tree scene has an active multiselection");
    TreeDebugRowVisualState focusedState{};
    Require(tree->DebugGetRowVisualState(window.Host().GetTheme(), 1u, window.Host().IsKeyboardFocusVisible(), focusedState) && focusedState.current &&
                focusedState.showFocus,
            "Tree scene resolves a focus outline on the focused row outside the selection");

    const WindowHostBitmapCapture normalCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Tree baseline render succeeds before null-brush injection");
    Require(normalCapture.widthPx > 0u && normalCapture.heightPx > 0u, "Tree baseline capture is nonempty");
    Require(tree->HasFocus() && window.Host().IsKeyboardFocusVisible(), "the baseline capture preserves the logical focus-outline paint condition");
    const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
    window.Host().DebugSetForceNullSolidBrushes(true);
    const WindowHostBitmapCapture nullBrushCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Tree focus-outline paint completes with every solid brush forced null");
    window.Host().DebugSetForceNullSolidBrushes(false);
    Require(nullBrushCapture.widthPx > 0u && nullBrushCapture.heightPx > 0u, "Tree null-brush capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "Tree null-brush paint introduces no presentation failures");

    const WindowHostBitmapCapture recoveredCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "Tree rendering recovers after null-brush injection");
    Require(recoveredCapture.widthPx > 0u && recoveredCapture.heightPx > 0u, "Tree recovery capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "Tree recovery introduces no presentation failures");
}

void TestWindowHostScrollPanelScrollbarSurvivesForcedNullSolidBrushes()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root     = std::make_unique<Panel>();
    auto* scroll  = root->AddChild<ScrollPanel>();
    auto* content = scroll->AddChild<Panel>();
    auto* label   = content->AddChild<Label>(L"Scrollable content");
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 292.0f, 188.0f));
    scroll->SetContentHeight(520.0f);
    scroll->SetScrollOffset(96.0f);
    content->SetBounds(D2D1::RectF(0.0f, 0.0f, 276.0f, 520.0f));
    label->SetBounds(D2D1::RectF(12.0f, 100.0f, 240.0f, 132.0f));
    window.Host().SetRoot(std::move(root));
    static_cast<Panel*>(window.Host().GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));

    D2D1_RECT_F thumbRect{};
    Require(scroll->NeedsScrollbar() && scroll->DebugGetScrollbarThumbHitRect(thumbRect), "ScrollPanel scene has a visible scrollbar thumb");
    Require(thumbRect.right > thumbRect.left && thumbRect.bottom > thumbRect.top, "ScrollPanel scrollbar thumb has paintable bounds");

    const WindowHostBitmapCapture normalCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ScrollPanel baseline render succeeds before null-brush injection");
    Require(normalCapture.widthPx > 0u && normalCapture.heightPx > 0u, "ScrollPanel baseline capture is nonempty");
    const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
    window.Host().DebugSetForceNullSolidBrushes(true);
    const WindowHostBitmapCapture nullBrushCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ScrollPanel scrollbar paint completes with every solid brush forced null");
    window.Host().DebugSetForceNullSolidBrushes(false);
    Require(nullBrushCapture.widthPx > 0u && nullBrushCapture.heightPx > 0u, "ScrollPanel null-brush capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "ScrollPanel null-brush paint introduces no presentation failures");

    const WindowHostBitmapCapture recoveredCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ScrollPanel rendering recovers after null-brush injection");
    Require(recoveredCapture.widthPx > 0u && recoveredCapture.heightPx > 0u, "ScrollPanel recovery capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "ScrollPanel recovery introduces no presentation failures");
}

void TestWindowHostComboBoxPopupSurvivesForcedNullSolidBrushes()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(8.0f, 8.0f, 292.0f, 40.0f));
    combo->SetVariant(ComboBoxVariant::Window);
    combo->SetChromeVisible(true);
    combo->SetMaxVisibleItems(4u);
    std::vector<ComboBox::Item> items;
    for (size_t index = 0u; index < 20u; ++index)
    {
        items.push_back(ComboBox::Item{std::format(L"value{:02}", index), std::format(L"Item {:02}", index)});
    }
    combo->SetItems(std::move(items));
    combo->SetSelectedIndex(1u);
    window.Host().SetRoot(std::move(root));
    static_cast<Panel*>(window.Host().GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
    window.Host().SetFocusControl(combo, false);
    Require(combo->OnKeyDown(window.Host(), VK_RETURN, 0u), "ComboBox scene opens its popup through the control's nonnative key handler");
    Require(combo->DebugIsPopupOpen() && combo->GetSelectedIndex() == std::optional<size_t>(1u), "ComboBox popup is open with a selected item");
    const ComboBoxVisualStyle comboStyle = ResolveComboBoxVisualStyle(
        window.Host().GetTheme(), combo->GetVariant(), combo->IsEnabled(), combo->IsHovered(), combo->DebugIsPopupOpen(), combo->HasFocus(), false);
    Require(comboStyle.showButtonSplit, "ComboBox scene enables the raw split-stroke draw");
    Require(combo->DebugGetPopupBounds().bottom > combo->DebugGetPopupBounds().top, "ComboBox popup has paintable bounds");
    Require(combo->DebugGetPopupItemRect(1u, &window.Host()).bottom > combo->DebugGetPopupItemRect(1u, &window.Host()).top,
            "ComboBox selected popup row has paintable bounds");
    size_t visiblePopupRows = 0u;
    for (size_t index = 0u; index < combo->GetItems().size(); ++index)
    {
        const D2D1_RECT_F itemRect = combo->DebugGetPopupItemRect(index, &window.Host());
        if (itemRect.right > itemRect.left && itemRect.bottom > itemRect.top)
        {
            ++visiblePopupRows;
        }
    }
    Require(visiblePopupRows > 0u && visiblePopupRows < combo->GetItems().size(), "ComboBox popup viewport is scrollable and exposes its scrollbar draw path");

    const WindowHostBitmapCapture normalCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ComboBox baseline render succeeds before null-brush injection");
    Require(normalCapture.widthPx > 0u && normalCapture.heightPx > 0u, "ComboBox baseline capture is nonempty");
    const uint64_t presentFailuresBefore = window.Host().DebugGetPresentFailureCount();
    window.Host().DebugSetForceNullSolidBrushes(true);
    const WindowHostBitmapCapture nullBrushCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ComboBox popup and scrollbar paint complete with every solid brush forced null");
    window.Host().DebugSetForceNullSolidBrushes(false);
    Require(nullBrushCapture.widthPx > 0u && nullBrushCapture.heightPx > 0u, "ComboBox null-brush capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "ComboBox null-brush paint introduces no presentation failures");

    const WindowHostBitmapCapture recoveredCapture =
        CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "ComboBox rendering recovers after null-brush injection");
    Require(recoveredCapture.widthPx > 0u && recoveredCapture.heightPx > 0u, "ComboBox recovery capture is nonempty");
    Require(window.Host().DebugGetPresentFailureCount() == presentFailuresBefore, "ComboBox recovery introduces no presentation failures");
}

void TestWindowHostDisabledOrHiddenCaptureCancelsTheDrag()
{
    using namespace DxUi;

    // A drag whose control or ancestor is disabled or hidden ends as a cancel, as in EmbeddedHost, instead of the
    // host dropping the capture silently and leaving the control mid-drag for the next press.
    AttachedHostWindow window;
    auto root      = std::make_unique<Panel>();
    auto* pane     = root->AddChild<Panel>();
    auto* splitter = pane->AddChild<Splitter>();
    pane->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 160.0f));
    splitter->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 160.0f));
    splitter->SetPosition(120.0f);
    std::vector<SplitterChangePhase> phases;
    splitter->SetOnChange([&](SplitterChange change) { phases.push_back(change.phase); });
    window.Host().SetRoot(std::move(root));
    const auto cancels = [&phases]
    {
        size_t count = 0u;
        for (const SplitterChangePhase phase : phases)
        {
            count += phase == SplitterChangePhase::Cancel ? 1u : 0u;
        }
        return count;
    };
    const D2D1_POINT_2F press = D2D1::Point2F(122.0f, 40.0f);
    const D2D1_POINT_2F moved = D2D1::Point2F(180.0f, 40.0f);
    bool handled              = false;

    Require(splitter->OnMouseDown(window.Host(), press, false, 0u), "the splitter drag starts");
    Require(splitter->OnMouseMove(window.Host(), moved, 0u) && splitter->GetPosition() > 150.0f, "the splitter drag previews");
    splitter->SetEnabled(false);
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_NULL, 0, 0, handled));
    Require(! splitter->IsDragging() && splitter->GetPosition() == 120.0f, "disabling the captured splitter cancels its drag and restores the start");
    Require(window.Host().GetCapturedControl() == nullptr && cancels() == 1u, "the host released the capture and the drag notified one cancel");

    splitter->SetEnabled(true);
    Require(splitter->OnMouseDown(window.Host(), press, false, 0u), "a second splitter drag starts");
    Require(splitter->OnMouseMove(window.Host(), moved, 0u), "the second drag previews");
    pane->SetVisible(false);
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_NULL, 0, 0, handled));
    Require(! splitter->IsDragging() && splitter->GetPosition() == 120.0f,
            "hiding an ancestor cancels the drag although the splitter's own flags are unchanged");
    Require(cancels() == 2u, "each abandoned drag notifies one cancel");
}

void TestWindowHostSmokeOverlayRendersBelowRootOverlay()
{
    using namespace DxUi;

    AttachedHostWindow window;
    window.Host().SetSmokeOverlayVisible(true);
    window.Host().SetRoot(std::make_unique<OverlayZOrderControl>());

    const WindowHostBitmapCapture capture = CaptureAttachedHostWindowBitmapForWindowHostSuite(window, "smoke-overlay z-order capture succeeds");
    Require(capture.widthPx > 0u && capture.heightPx > 0u && ! capture.bgraPixels.empty(), "smoke-overlay z-order capture has pixels");

    const float dpi        = window.Host().GetDpi();
    const UINT overlayX    = DipToPixelForWindowHost(24.0f, dpi, capture.widthPx);
    const UINT overlayY    = DipToPixelForWindowHost(24.0f, dpi, capture.heightPx);
    const UINT contentX    = DipToPixelForWindowHost(96.0f, dpi, capture.widthPx);
    const UINT contentY    = DipToPixelForWindowHost(24.0f, dpi, capture.heightPx);
    const uint32_t overlay = GetWindowHostCapturePixelBgra(capture, overlayX, overlayY);
    const uint32_t content = GetWindowHostCapturePixelBgra(capture, contentX, contentY);

    const auto red   = [](uint32_t bgra) noexcept { return static_cast<uint8_t>((bgra >> 16u) & 0xFFu); };
    const auto green = [](uint32_t bgra) noexcept { return static_cast<uint8_t>((bgra >> 8u) & 0xFFu); };
    const auto blue  = [](uint32_t bgra) noexcept { return static_cast<uint8_t>(bgra & 0xFFu); };

    Require(green(overlay) > 180u && red(overlay) < 96u && blue(overlay) < 96u, "root overlay paints above the smoke overlay instead of being dimmed by it");
    Require(red(content) > (green(content) + 40u) && red(content) < 190u, "smoke overlay dims normal root content below root overlay paint");
}

void TestWindowHostOverlayHitTestingPrecedesContentHitTesting()
{
    using namespace DxUi;

    WindowHost host;
    TrackingControlState overlayState;
    TrackingControlState contentState;

    auto root     = std::make_unique<Panel>();
    auto* overlay = root->AddChild<OverlayHitRecordingControl>(overlayState);
    auto* content = root->AddChild<TrackingControl>(contentState);

    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 80.0f));
    overlay->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));
    content->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));
    host.SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(16, 16), handled));
    Require(handled, "overlay hit-test mouse-down is handled");
    Require(overlayState.mouseDownCount == 1u, "overlay hit target receives mouse-down before overlapping content");
    Require(contentState.mouseDownCount == 0u, "overlapping content is not invoked when an overlay hit target exists");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(16, 16), handled));
}

void TestWindowHostOverlayHitTestRetiringRootStopsPointerResolutionAndDispatch()
{
    using namespace DxUi;

    const auto verifyRetiredOverlayStopsMessage = [](UINT message, const char* messageName)
    {
        WindowHost host;
        RootRetiringOverlayHitState state;
        auto root = std::make_unique<RootRetiringOverlayHitControl>(host, state);
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 40.0f));
        host.SetRoot(std::move(root));

        bool handled = false;
        static_cast<void>(host.HandleMessage(nullptr, message, message == WM_LBUTTONDOWN ? MK_LBUTTON : 0, MAKELPARAM(12, 12), handled));

        Require(host.GetRoot() == nullptr, messageName);
        Require(state.overlayHitCount == 1u, "the retiring overlay receives one hit-test query");
        Require(state.normalHitCount == 0u, "root retirement in overlay hit testing skips the normal content-hit query");
        Require(state.mouseMoveCount == 0u && state.mouseDownCount == 0u, "pointer input is not dispatched through a retired overlay hit result");
    };

    verifyRetiredOverlayStopsMessage(WM_MOUSEMOVE, "mouse-move overlay hit testing can clear the host root");
    verifyRetiredOverlayStopsMessage(WM_LBUTTONDOWN, "mouse-down overlay hit testing can clear the host root");
}

void TestWindowHostContainsMouseDownCallbackExceptionAndContinues()
{
    using namespace DxUi;

    WindowHost host;
    ThrowOnceOnMouseDownState state;
    auto root = std::make_unique<ThrowOnceOnMouseDownControl>(state);
    host.SetRoot(std::move(root));
    host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 40.0f));

    bool handled                       = false;
    const LRESULT failedCallbackResult = host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 12), handled);
    Require(handled && failedCallbackResult == 0, "the host consumes the mouse-down whose control callback throws");
    Require(state.mouseDownCount == 1u, "the failing input callback is contained after one invocation");

    handled                               = false;
    const LRESULT recoveredCallbackResult = host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 12), handled);
    Require(handled && recoveredCallbackResult == 0, "the next mouse-down is handled normally after the callback failure");
    Require(state.mouseDownCount == 2u, "the next input reaches the callback once and does not retry the failed callback");
}

void TestWindowHostEscapeClosesMouseOpenedComboPopupBeforeCancelButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root          = std::make_unique<Panel>();
    auto* combo        = root->AddChild<ComboBox>();
    auto* cancelButton = root->AddChild<Button>(L"Cancel");
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    cancelButton->SetBounds(D2D1::RectF(0.0f, 36.0f, 100.0f, 64.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    size_t cancelCount = 0u;
    cancelButton->SetOnClick([&cancelCount] { ++cancelCount; });

    host.SetRoot(std::move(root));
    host.SetCancelButton(cancelButton);
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 120.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0, MAKELPARAM(172, 12), handled));
    Require(handled, "mouse-open combo click handled");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(172, 12), handled));
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "mouse-opened combo popup is open before escape");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_ESCAPE, 0, handled));
    Require(handled, "escape handled while mouse-opened combo popup is open");
    Require(cancelCount == 0u, "cancel button not invoked while mouse-opened popup owns escape");
    Require(combo->GetHitBounds().bottom == combo->GetBounds().bottom, "escape closes mouse-opened combo popup first");
}

void TestWindowHostTabTraversalIncludesComboBox()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"First");
    auto* combo  = root->AddChild<ComboBox>();
    auto* field  = root->AddChild<TextField>();
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 24.0f));
    combo->SetBounds(D2D1::RectF(0.0f, 28.0f, 120.0f, 56.0f));
    field->SetBounds(D2D1::RectF(0.0f, 60.0f, 140.0f, 88.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    host.SetRoot(std::move(root));
    host.SetFocusControl(button);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab to combo handled");
    Require(host.GetFocusControl() == combo, "combo participates in tab traversal");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from combo handled");
    Require(host.GetFocusControl() == field, "tab advances from combo to next focusable control");
}

void TestWindowHostTabTraversalStaysConsistentAcrossFieldComboTreeGridAndButtons()
{
    using namespace DxUi;

    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* field    = root->AddChild<TextField>();
    auto* combo    = root->AddChild<ComboBox>();
    auto* tree     = root->AddChild<Tree>();
    auto* grid     = root->AddChild<Grid>();
    auto* okButton = root->AddChild<Button>(L"OK");

    field->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 28.0f));
    combo->SetBounds(D2D1::RectF(0.0f, 36.0f, 160.0f, 64.0f));
    tree->SetBounds(D2D1::RectF(0.0f, 72.0f, 220.0f, 156.0f));
    grid->SetBounds(D2D1::RectF(0.0f, 164.0f, 260.0f, 252.0f));
    okButton->SetBounds(D2D1::RectF(0.0f, 260.0f, 96.0f, 288.0f));

    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    MutableTreeModel treeModel;
    treeModel.SetVisibleItems({
        TreeItemData{.id = 1u, .text = L"General"},
        TreeItemData{.id = 2u, .text = L"Viewers"},
    });
    tree->SetModel(&treeModel);

    MultiRowGridModel gridModel(3u);
    RecordingGridDelegate gridDelegate;
    grid->SetModel(&gridModel);
    grid->SetDelegate(&gridDelegate);

    host.SetRoot(std::move(root));
    host.SetFocusControl(field);
    Require(host.GetFocusControl() == field, "mixed retained-tree traversal starts on the text field");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from text field handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == combo, "tab advances from text field to combo");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from combo handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == tree, "tab advances from combo to tree");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from tree handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == grid, "tab advances from tree to grid");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from grid handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == okButton, "tab advances from grid to command button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "tab from command button handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == field, "tab wraps from command button back to the text field");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab from text field handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == okButton, "shift+tab wraps from text field to command button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab from command button handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == grid, "shift+tab moves from command button to grid");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab from grid handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == tree, "shift+tab moves from grid to tree");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab from tree handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == combo, "shift+tab moves from tree to combo");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "shift+tab from combo handled in mixed retained-tree traversal");
    Require(host.GetFocusControl() == field, "shift+tab moves from combo back to text field");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
}

void TestWindowHostGroupedListNavigationKeepsTreeTypeaheadAndGridSelectionVisible()
{
    using namespace DxUi;

    WindowHost host;
    auto root         = std::make_unique<Panel>();
    auto* tree        = root->AddChild<Tree>();
    auto* grid        = root->AddChild<Grid>();
    auto* closeButton = root->AddChild<Button>(L"Close");

    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 108.0f));
    grid->SetBounds(D2D1::RectF(0.0f, 116.0f, 280.0f, 292.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);
    closeButton->SetBounds(D2D1::RectF(0.0f, 300.0f, 96.0f, 328.0f));

    MutableTreeModel treeModel;
    const auto setTreeItems = [&treeModel](bool expanded)
    {
        if (expanded)
        {
            treeModel.SetVisibleItems({
                TreeItemData{.id = 1u, .text = L"Plugins", .hasChildren = true, .expanded = true},
                TreeItemData{.id = 2u, .parentId = 1u, .text = L"ViewerSqlite", .depth = 1u},
                TreeItemData{.id = 3u, .text = L"Search"},
                TreeItemData{.id = 4u, .text = L"Themes"},
            });
            return;
        }

        treeModel.SetVisibleItems({
            TreeItemData{.id = 1u, .text = L"Plugins", .hasChildren = true, .expanded = false},
            TreeItemData{.id = 3u, .text = L"Search"},
            TreeItemData{.id = 4u, .text = L"Themes"},
        });
    };
    setTreeItems(false);

    RecordingTreeDelegate treeDelegate;
    tree->SetModel(&treeModel);
    tree->SetDelegate(&treeDelegate);

    GroupedGridModel gridModel(6u);
    gridModel.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    CollapsibleGroupedGridDelegate gridDelegate(gridModel);
    grid->SetModel(&gridModel);
    grid->SetDelegate(&gridDelegate);

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 360.0f));

    tree->SetSelectedItemId(1u);
    grid->GetSelectionModel().SetSingle(gridModel.GetStableRowId(1u));
    host.SetFocusControl(tree);
    Require(host.GetFocusControl() == tree, "grouped/list host navigation starts with tree focus");

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RIGHT, 0, handled));
    Require(handled, "grouped/list host handles tree right key on a collapsed parent");
    Require(treeDelegate.toggleCount == 1u, "grouped/list host requests one tree expansion");
    Require(treeDelegate.lastToggledItemId == 1u && treeDelegate.lastExpandedState, "grouped/list host tree right key expands the parent item");

    setTreeItems(true);
    tree->NotifyDataChanged();

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RIGHT, 0, handled));
    Require(handled, "grouped/list host handles tree right key on an expanded parent");
    Require(tree->GetSelectedItemId().has_value() && tree->GetSelectedItemId().value() == 2u,
            "grouped/list host tree right key selects the first child when the parent is expanded");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_LEFT, 0, handled));
    Require(handled, "grouped/list host handles tree left key on a child item");
    Require(tree->GetSelectedItemId().has_value() && tree->GetSelectedItemId().value() == 1u,
            "grouped/list host tree left key selects the parent from the child row");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_LEFT, 0, handled));
    Require(handled, "grouped/list host handles tree left key on an expanded parent");
    Require(treeDelegate.toggleCount == 2u, "grouped/list host requests one tree collapse after expansion");
    Require(treeDelegate.lastToggledItemId == 1u && ! treeDelegate.lastExpandedState, "grouped/list host tree left key collapses the parent item");

    setTreeItems(false);
    tree->NotifyDataChanged();

    const size_t selectionChangedBeforeTypeahead = treeDelegate.selectionChangedCount;
    handled                                      = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_CHAR, static_cast<WPARAM>(L's'), 0, handled));
    Require(handled, "grouped/list host handles tree typeahead");
    Require(tree->GetSelectedItemId().has_value() && tree->GetSelectedItemId().value() == 3u,
            "grouped/list host tree typeahead selects the matching visible item after collapse");
    Require(treeDelegate.selectionChangedCount == selectionChangedBeforeTypeahead + 1u,
            "grouped/list host tree typeahead notifies one visible selection change");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "grouped/list host handles tab from tree to grouped grid");
    Require(host.GetFocusControl() == grid, "grouped/list host tab advances focus from tree to grouped grid");

    Require(grid->GetSelectionModel().GetCount() == 1u, "grouped/list host grouped grid starts with one selected row");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == gridModel.GetStableRowId(1u),
            "grouped/list host grouped grid starts on a row that will become hidden when the group collapses");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_LEFT, 0, handled));
    Require(handled, "grouped/list host grouped grid handles left key to collapse the selected group");
    Require(gridDelegate.groupToggleCount == 1u, "grouped/list host grouped grid reports one collapse toggle");
    Require(gridDelegate.lastGroupStableId == 10u && gridDelegate.lastGroupCollapsed,
            "grouped/list host grouped grid reports the collapsed group id and state from the keyboard path");
    Require(grid->GetSelectionModel().GetCount() == 1u, "grouped/list host grouped grid keeps one visible row selected after collapse");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == gridModel.GetStableRowId(2u),
            "grouped/list host grouped grid rehomes selection to the nearest visible row after collapse");

    const GridVisibleWorkMetrics collapsedMetrics = grid->GetVisibleWorkMetrics();
    Require(collapsedMetrics.visibleRowCount == 4u, "grouped/list host grouped grid collapse updates visible row work");
    Require(collapsedMetrics.visibleGroupHeaderCount == 2u, "grouped/list host grouped grid keeps visible headers after collapse");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_RIGHT, 0, handled));
    Require(handled, "grouped/list host grouped grid handles right key to re-expand the collapsed group from the fallback row");
    Require(gridDelegate.groupToggleCount == 2u, "grouped/list host grouped grid reports one expand toggle after keyboard collapse");
    Require(gridDelegate.lastGroupStableId == 10u && ! gridDelegate.lastGroupCollapsed,
            "grouped/list host grouped grid reports the expanded group id and state from the keyboard path");
    Require(grid->GetSelectionModel().GetCount() == 1u, "grouped/list host grouped grid keeps one selected row after re-expansion");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == gridModel.GetStableRowId(2u),
            "grouped/list host grouped grid keeps the fallback row selected after re-expansion");

    const GridVisibleWorkMetrics expandedMetrics = grid->GetVisibleWorkMetrics();
    Require(expandedMetrics.visibleRowCount == 4u, "grouped/list host grouped grid re-expansion restores visible row work");
    Require(expandedMetrics.visibleGroupHeaderCount == 2u, "grouped/list host grouped grid keeps visible headers after re-expansion");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_END, 0, handled));
    Require(handled, "grouped/list host handles grouped grid end key after keyboard collapse and re-expansion");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == gridModel.GetStableRowId(5u),
            "grouped/list host grouped grid end key keeps keyboard navigation on the last visible row after re-expansion");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "grouped/list host handles tab from grouped grid to the dialog button");
    Require(host.GetFocusControl() == closeButton, "grouped/list host tab advances from the grouped grid to the dialog button");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_SHIFT, 0, handled));
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));
    Require(handled, "grouped/list host handles shift+tab from the dialog button back to the grouped grid");
    Require(host.GetFocusControl() == grid, "grouped/list host shift+tab returns focus to the grouped grid");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYUP, VK_SHIFT, 0, handled));
}

void TestWindowHostAltDownOpensComboPopup()
{
    using namespace DxUi;

    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    host.SetRoot(std::move(root));
    host.SetFocusControl(combo);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_DOWN, 0, handled));
    Require(handled, "alt+down is handled for combo");
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "alt+down opens combo popup");
}

void TestWindowHostAltUpClosesComboPopup()
{
    using namespace DxUi;

    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* combo = root->AddChild<ComboBox>();
    combo->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 28.0f));
    combo->SetItems({ComboBox::Item{L"one", L"One"}, ComboBox::Item{L"two", L"Two"}});

    host.SetRoot(std::move(root));
    host.SetFocusControl(combo);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_DOWN, 0, handled));
    Require(handled, "alt+down handled before alt+up close");
    Require(combo->GetHitBounds().bottom > combo->GetBounds().bottom, "combo popup is open before alt+up");

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSKEYDOWN, VK_UP, 0, handled));
    Require(handled, "alt+up is handled for open combo");
    Require(combo->GetHitBounds().bottom == combo->GetBounds().bottom, "alt+up closes combo popup");
}

void TestWindowHostMnemonicActivatesButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Find");
    button->SetMnemonic(L'F');
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 28.0f));

    bool clicked = false;
    button->SetOnClick([&clicked] { clicked = true; });
    host.SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSCHAR, L'f', 0, handled));
    Require(handled, "button mnemonic handled");
    Require(clicked, "button mnemonic invokes click");
    Require(host.GetFocusControl() == button, "button mnemonic focuses button");
}

void TestWindowHostLabelMnemonicTargetsField()
{
    using namespace DxUi;

    WindowHost host;
    auto root   = std::make_unique<Panel>();
    auto* label = root->AddChild<Label>(L"Root");
    auto* combo = root->AddChild<ComboBox>();
    label->SetMnemonic(L'R');
    label->SetMnemonicTarget(combo);
    combo->SetEditable(true);
    label->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 24.0f));
    combo->SetBounds(D2D1::RectF(0.0f, 28.0f, 180.0f, 56.0f));

    host.SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSCHAR, L'r', 0, handled));
    Require(handled, "label mnemonic handled");
    Require(host.GetFocusControl() == combo, "label mnemonic focuses target control");
}

void TestWindowHostUnknownMnemonicRemainsUnhandled()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Open");
    button->SetMnemonic(L'O');
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 28.0f));
    host.SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SYSCHAR, L'x', 0, handled));
    Require(! handled, "unknown mnemonic remains unhandled");
    Require(host.GetFocusControl() == nullptr, "unknown mnemonic does not move focus");
}

void TestWindowHostHiddenAnimationTickDropsSubscriptionUntilShown()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root     = std::make_unique<Panel>();
    auto* ticking = root->AddChild<AnimationTickTraceControl>();
    window.Host().SetRoot(std::move(root));
    window.Host().RequestAnimation();

    Require(IsWindowVisible(window.Hwnd()) == FALSE, "attached host window starts hidden for hidden-animation test");
    Require(window.Host().DebugHasActiveAnimationSubscription(), "hidden-animation test starts with an active subscription");
    const uint64_t invalidatesBefore = window.Host().DebugGetInvalidateCount();

    Require(! window.Host().DebugAnimationTickForTest(123u), "hidden host animation tick releases the dispatcher subscription");
    Require(! window.Host().DebugHasActiveAnimationSubscription(),
            "hidden host animation tick clears the subscription while remembering the suspended animation");
    Require(ticking->tickCount == 0u, "hidden host animation tick skips root ticking while hidden");
    Require(window.Host().DebugGetInvalidateCount() == invalidatesBefore, "hidden host animation tick skips invalidation while hidden");

    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    Require(window.Host().DebugHasActiveAnimationSubscription(), "showing the host restores a suspended animation subscription");
    Require(window.Host().DebugAnimationTickForTest(140u), "visible host animation tick resumes root ticking after show");
    Require(ticking->tickCount == 1u, "visible host animation tick reaches the root after show");
}

void TestWindowHostRestoreFromMinimizeRearmsSuspendedAnimation()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();
    static_cast<void>(root->AddChild<AnimationTickTraceControl>());
    window.Host().SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.Host().RequestAnimation();

    ShowWindow(window.Hwnd(), SW_MINIMIZE);
    Require(! window.Host().DebugAnimationTickForTest(200u), "iconic host latches its animation as suspended");
    Require(! window.Host().DebugHasActiveAnimationSubscription(), "iconic host releases its animation subscription");

    ShowWindow(window.Hwnd(), SW_RESTORE);
    Require(window.Host().DebugHasActiveAnimationSubscription(), "SIZE_RESTORED re-arms a suspended animation for an effectively visible host");
}

void TestNoninteractiveWindowActivationBlockerRejectsFocusStealing()
{
    using DxUi::TestSupport::ScopedWindowActivationBlocker;

    ScopedWindowActivationBlocker localBlocker;
    const bool ownsBlocker = ! ScopedWindowActivationBlocker::IsActiveForCurrentThread();
    if (ownsBlocker)
    {
        Require(localBlocker.Start(), "noninteractive activation blocker installs on the test UI thread");
    }

    wil::unique_hwnd window(CreateWindowExW(
        0, L"STATIC", L"DxUi no-activation guard probe", WS_OVERLAPPEDWINDOW, 0, 0, 160, 90, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr));
    Require(window != nullptr, "no-activation guard probe window is created");
    Require((GetWindowLongPtrW(window.get(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0, "no-activation guard applies WS_EX_NOACTIVATE to top-level test windows");

    ShowWindow(window.get(), SW_SHOW);
    static_cast<void>(SetActiveWindow(window.get()));
    static_cast<void>(SetForegroundWindow(window.get()));
    static_cast<void>(SetFocus(window.get()));

    Require(GetForegroundWindow() != window.get(), "no-activation guard preserves the user's foreground window");
    Require(GetActiveWindow() != window.get(), "no-activation guard rejects top-level activation");
    Require(GetFocus() != window.get(), "no-activation guard rejects keyboard focus");
    if (ownsBlocker)
    {
        Require(localBlocker.ProtectedTopLevelWindowCount() >= 1u, "no-activation guard records protected top-level windows");
        Require(localBlocker.BlockedActivationCount() >= 1u, "no-activation guard records rejected activation attempts");
    }
}

void TestWindowHostPointerDispatchDoesNotReuseTargetAfterRootReplacement()
{
    using namespace DxUi;

    WindowHost host;
    RootReplacingPointerControlState state;
    auto root     = std::make_unique<Panel>();
    auto* control = root->AddChild<RootReplacingPointerControl>(state);
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 120.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(24, 16), handled));
    Require(handled, "root-replacing pointer-down is handled");
    Require(state.mouseDownCount == 1u, "root-replacing control receives one mouse-down");
    Require(host.GetFocusControl() == nullptr, "root-replacing pointer-down leaves no stale focus target");

    auto secondRoot     = std::make_unique<Panel>();
    auto* secondControl = secondRoot->AddChild<RootReplacingPointerControl>(state);
    secondControl->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f));
    host.SetRoot(std::move(secondRoot));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 120.0f));

    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(24, 16), handled));
    Require(handled, "root-replacing pointer-up is handled");
    Require(state.mouseUpCount == 1u, "root-replacing control receives one mouse-up");
}

void TestWindowHostHoverEnterDoesNotReuseTargetAfterRootReplacement()
{
    using namespace DxUi;

    WindowHost host;
    RootReplacingHoverControlState state;
    auto root     = std::make_unique<Panel>();
    auto* control = root->AddChild<RootReplacingHoverControl>(state);
    control->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 80.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 160.0f, 120.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, 0, MAKELPARAM(24, 16), handled));
    Require(handled, "root-replacing hover-enter mouse move is handled");
    Require(state.hoverEnterCount == 1u, "root-replacing hover control receives hover enter");
    Require(state.mouseMoveCount == 0u, "root-replacing hover control is not reused for mouse move after replacing the root");
}

// A window's focus gain is reported by one system event, and a click that activates the window sets its control after
// WM_SETFOCUS, in the same turn of the message loop: in a window UI Automation has never asked for its focus the host
// treats the focus moves of that turn as the event's to report (see the tests that follow). The turn ends when the
// message the host posted at the gain is dispatched, when the window loses focus, or after 500 ms should a window
// procedure never hand the host that message.
void TestWindowHostFocusGainTurnEndsWithItsMessageOrTheLossOfFocusOrItsLimit()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root   = std::make_unique<Panel>();
    auto* first = root->AddChild<Button>(L"Premier bouton");
    first->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    window.Host().SetRoot(std::move(root));
    window.PumpMessages();
    Require(! window.Host().DebugIsInFocusGainTurn(), "a window that has not gained focus is in no turn");

    SendMessageW(window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(window.Host().DebugIsInFocusGainTurn(), "gaining focus starts a turn");
    window.PumpMessages();
    Require(! window.Host().DebugIsInFocusGainTurn(), "the turn ends when the loop dispatches the message posted at the gain");

    SendMessageW(window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(window.Host().DebugIsInFocusGainTurn(), "gaining focus again starts another turn");
    SendMessageW(window.Hwnd(), WM_KILLFOCUS, 0, 0);
    Require(! window.Host().DebugIsInFocusGainTurn(), "losing focus ends the turn");
    window.PumpMessages();
    Require(! window.Host().DebugIsInFocusGainTurn(), "the message posted at the earlier gain ends nothing more");

    SendMessageW(window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(window.Host().DebugIsInFocusGainTurn(), "a third gain starts a turn whose message is held back");
    Sleep(600);
    Require(! window.Host().DebugIsInFocusGainTurn(), "a turn whose message never arrives ends after 500 ms");
    window.PumpMessages();
}

// A window with two buttons that never holds the foreground (a gain is played by sending it WM_SETFOCUS), the fragment
// root UI Automation calls GetFocus on, and a UI Automation client that listens, since the host compares its snapshots
// only while one does.
struct FocusGainTestWindow final
{
    FocusGainTestWindow()
    {
        auto tree = std::make_unique<DxUi::Panel>();
        first     = tree->AddChild<DxUi::Button>(L"Premier bouton");
        second    = tree->AddChild<DxUi::Button>(L"Second bouton");
        first->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
        second->SetBounds(D2D1::RectF(8.0f, 48.0f, 200.0f, 80.0f));
        window.Host().SetRoot(std::move(tree));
        root.attach(DxUi::CreateWindowHostAccessibilityProvider(window.Hwnd()));
        Require(root != nullptr, "the window exposes its fragment root");
        Require(client.WaitUntil(FocusEventClient::kNotificationDeadlineMs, [] { return UiaClientsAreListening() != FALSE; }),
                "a UI Automation client listens");
        window.PumpMessages();
    }

    AttachedHostWindow window;
    FocusEventClient client{window, window.Hwnd()}; // After the window it pumps, so it is destroyed first.
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> root;
    DxUi::Button* first  = nullptr;
    DxUi::Button* second = nullptr;
};

// UI Automation answers the system's focus event for a window it has never asked about with a call of the fragment root's
// GetFocus, on whatever thread, which reads the published snapshot when it runs. A click that activates such a window
// moves focus after its WM_SETFOCUS, in the same turn, before UI Automation has acted: the host leaves that move to the
// call, which reports the clicked control, for as long as no call has begun, because every call that begins later reads the
// snapshot after the move. The window never holds the foreground here, so the decision is read from a count; the Menu suite
// plays the click with a real UI Automation client.
void TestWindowHostFocusMoveInTheGainTurnOfAWindowNoGetFocusHasBegunOnIsLeftToTheSystem()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host         = test.window.Host();
    const uint64_t leftBefore = host.DebugGetFocusMovesLeftToSystemCount();
    Require(DebugGetAccessibilityFocusResolutionCountForTest(test.window.Hwnd()) == 0u, "nothing has asked the new window for its focus");

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "the gain focuses the first button on the way");
    Require(host.DebugIsInFocusGainTurn(), "the gain starts a turn");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "what the gain itself focuses is the system's event to report, not a move left to it");

    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "the click focuses the second button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore + 1u,
            "a move in the gain's turn is left to the system's event while no GetFocus call has begun");
    host.SetFocusControl(test.first);
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore + 2u, "so is a further move while still no call has begun");
    Require(ReadFocusedElementName(*test.root) == L"Premier bouton", "the first call reports the control the host last moved focus to");

    host.SetFocusControl(test.second);
    Require(host.DebugIsInFocusGainTurn(), "the turn still runs");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore + 2u,
            "once a GetFocus call has begun, a further move in the turn is no longer left to the system's event");
    Require(ReadFocusedElementName(*test.root) == L"Second bouton", "and the call that follows reports it");
}

// A GetFocus call that has begun since the gain may have answered the system's event before the click's move, so the
// host announces that move itself, at worst as a duplicate.
void TestWindowHostFocusMoveAfterAGetFocusCallHasBegunIsAnnouncedByTheHost()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host         = test.window.Host();
    const uint64_t leftBefore = host.DebugGetFocusMovesLeftToSystemCount();

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "the gain focuses the first button on the way");
    Require(ReadFocusedElementName(*test.root) == L"Premier bouton", "the call that answers the system's event reports the control the gain focused");
    Require(DebugGetAccessibilityFocusResolutionCountForTest(test.window.Hwnd()) == 1u, "a GetFocus call counts itself once");

    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "the click focuses the second button");
    Require(host.DebugIsInFocusGainTurn(), "the move was made in the gain's turn");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "the host does not leave the move to an event a GetFocus call may have answered");
    Require(ReadFocusedElementName(*test.root) == L"Second bouton", "the next call reports the moved-to control");
}

// UI Automation answers a window it has asked about before from its root element's keyboard-focus property, which reports
// nothing while the focus is inside a control: the click that activates such a window is the host's to announce,
// whichever way the earlier call fell. The count the decision reads is the window's whole history, not what happened
// since the gain.
void TestWindowHostFocusMoveInTheGainTurnOfAWindowGetFocusHasBegunOnBeforeIsAnnouncedByTheHost()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host         = test.window.Host();
    const uint64_t leftBefore = host.DebugGetFocusMovesLeftToSystemCount();
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(ReadFocusedElementName(*test.root) == L"Premier bouton", "UI Automation asked the fragment root for the focus of the window's first activation");
    test.window.PumpMessages();
    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "the first button keeps its logical focus while the window has none");

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.DebugIsInFocusGainTurn(), "the second activation starts a turn");
    Require(DebugGetAccessibilityFocusResolutionCountForTest(test.window.Hwnd()) == 1u,
            "nothing has asked the window for its focus since the first activation");
    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "the click focuses the second button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "the host announces the click in a window UI Automation has asked before");
}

// Only the turn of the gain is the system's: a move after the loop has turned, after the window lost focus again, or after
// the turn outlived its limit is the host's to announce, whether or not a GetFocus call has begun.
void TestWindowHostFocusMovesOutsideTheGainTurnAreTheHosts()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host         = test.window.Host();
    const uint64_t leftBefore = host.DebugGetFocusMovesLeftToSystemCount();

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    test.window.PumpMessages();
    Require(! host.DebugIsInFocusGainTurn(), "the loop turned");
    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "a click in the active window focuses the second button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "a move after the loop has turned is the host's");

    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.DebugIsInFocusGainTurn(), "gaining focus again starts a turn");
    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    host.SetFocusControl(test.first);
    Require(host.GetFocusControl() == test.first, "a move after the window lost focus focuses the first button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "a move after the window lost focus is the host's");
    test.window.PumpMessages();

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.DebugIsInFocusGainTurn(), "a third gain starts a turn whose message is held back");
    Sleep(600);
    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "a move after the turn outlived its limit focuses the second button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore, "a turn that outlived its limit leaves nothing to the system's event");
    test.window.PumpMessages();
}

// UI Automation's call can be slow: it comes on a thread of its own at any time. One that has not yet counted itself when
// the click moves focus reads the snapshot after the move and reports the clicked control, which is why the host leaves
// that move to it. The test gate holds the call before it counts itself, as a client thread that has not got as far would
// be.
void TestWindowHostGetFocusHeldBeforeItCountsReportsTheMoveTheHostLeftToTheSystem()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host         = test.window.Host();
    const uint64_t leftBefore = host.DebugGetFocusMovesLeftToSystemCount();
    wil::unique_event entered(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    wil::unique_event release(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    Require(entered && release, "create the gate events");
    DebugSetAccessibilityFocusResolutionGateForTest(test.window.Hwnd(), entered.get(), release.get());
    const auto clearGate = wil::scope_exit([&]() noexcept
    {
        static_cast<void>(SetEvent(release.get()));
        DebugSetAccessibilityFocusResolutionGateForTest(nullptr, nullptr, nullptr);
    });

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "the gain focuses the first button on the way");

    // The call UI Automation makes to answer the system's event, on a thread of its own, as a provider call arrives.
    std::wstring answer;
    std::atomic<bool> answered{false};
    std::jthread resolver([&]
    {
        wil::com_ptr_nothrow<IRawElementProviderFragment> focused;
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        VARIANT name{};
        VariantInit(&name);
        if (SUCCEEDED(test.root->GetFocus(focused.put())) && focused && SUCCEEDED(focused.query_to(simple.put())) &&
            SUCCEEDED(simple->GetPropertyValue(UIA_NamePropertyId, &name)) && name.vt == VT_BSTR && name.bstrVal)
        {
            answer.assign(name.bstrVal, SysStringLen(name.bstrVal));
        }
        VariantClear(&name);
        answered.store(true);
    });
    Require(WaitForSingleObject(entered.get(), 5000) == WAIT_OBJECT_0, "the call reaches the gate");
    Require(! answered.load() && DebugGetAccessibilityFocusResolutionCountForTest(test.window.Hwnd()) == 0u, "the gate holds the call before it counts itself");

    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second, "the click focuses the second button");
    Require(host.DebugGetFocusMovesLeftToSystemCount() == leftBefore + 1u, "a move made while the call is held is left to the system's event");

    Require(SetEvent(release.get()) != FALSE, "release the gate");
    resolver.join();
    Require(answer == L"Second bouton", "the call the gate held reports the control the host left to it");
    Require(DebugGetAccessibilityFocusResolutionCountForTest(test.window.Hwnd()) == 1u, "the released call counts itself once");
}

// UI Automation answers the focus event of a window it has asked for its focus before from the root element's
// keyboard-focus property, which reports nothing while the focus is inside a control, so a window activated again (Alt+Tab
// back to it) is reported by nothing: the host announces what the gain restored or focused once the gain's turn ends. For
// a window's first gain UI Automation asks the fragment root, and the host adds nothing. The window never holds the
// foreground here, so the decision is read from a count; the Menu suite plays it with a real UI Automation client.
void TestWindowHostGainOfAWindowAskedBeforeIsAnnouncedWhenItsTurnEnds()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host              = test.window.Host();
    const uint64_t announcedBefore = host.DebugGetReactivationAnnouncementCount();

    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "the window's first gain focuses the first button");
    Require(ReadFocusedElementName(*test.root) == L"Premier bouton", "UI Automation asks the fragment root for the focus of the window's first gain");
    test.window.PumpMessages();
    Require(! host.DebugIsInFocusGainTurn(), "the first gain's turn ended");
    Require(host.DebugGetReactivationAnnouncementCount() == announcedBefore, "the host adds nothing to the answer to a window's first gain");

    host.SetFocusControl(test.second);
    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.second, "the second button keeps its logical focus while the window has none");
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.second, "the gain restores the second button");
    Require(host.DebugGetReactivationAnnouncementCount() == announcedBefore, "nothing is announced before the gain's turn ends");
    test.window.PumpMessages();
    Require(host.DebugGetReactivationAnnouncementCount() == announcedBefore + 1u, "the host announces the restored control when the turn ends");

    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    host.SetFocusControl(nullptr);
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.GetFocusControl() == test.first, "a gain with no control to restore focuses the first button on the way");
    test.window.PumpMessages();
    Require(host.DebugGetReactivationAnnouncementCount() == announcedBefore + 2u, "the host announces the control the gain focused when the turn ends");
}

// The end of a gain's turn reports only what nothing else does: after the host announced a move of the turn itself (the
// click that activates a window UI Automation asked before), and for a root that stands for its focused control, which
// UI Automation reports itself because the root says it has the keyboard focus, it announces nothing more.
void TestWindowHostEndOfAGainTurnAnnouncesOnlyWhatNothingElseReports()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host = test.window.Host();
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(ReadFocusedElementName(*test.root) == L"Premier bouton", "UI Automation asked the fragment root for the focus of the window's first gain");
    test.window.PumpMessages();
    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);

    const uint64_t announcedBefore = host.DebugGetReactivationAnnouncementCount();
    const uint64_t leftBefore      = host.DebugGetFocusMovesLeftToSystemCount();
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    host.SetFocusControl(test.second);
    Require(host.GetFocusControl() == test.second && host.DebugGetFocusMovesLeftToSystemCount() == leftBefore,
            "the host announces the click that activates a window UI Automation asked before");
    test.window.PumpMessages();
    Require(host.DebugGetReactivationAnnouncementCount() == announcedBefore, "the end of the turn adds nothing to the click the host announced");

    AttachedHostWindow single;
    single.Host().SetRoot(std::make_unique<Button>(L"Seul bouton"));
    single.PumpMessages();
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> singleRoot;
    singleRoot.attach(CreateWindowHostAccessibilityProvider(single.Hwnd()));
    Require(singleRoot != nullptr, "the single-control window exposes its fragment root");
    SendMessageW(single.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(single.Host().GetFocusControl() != nullptr, "the gain focuses the window's only control");
    Require(ReadFocusedElementName(*singleRoot) == L"Seul bouton", "UI Automation asked the fragment root for the focus of that window's first gain");
    single.PumpMessages();
    SendMessageW(single.Hwnd(), WM_KILLFOCUS, 0, 0);
    const uint64_t singleBefore = single.Host().DebugGetReactivationAnnouncementCount();
    SendMessageW(single.Hwnd(), WM_SETFOCUS, 0, 0);
    single.PumpMessages();
    Require(single.Host().DebugGetReactivationAnnouncementCount() == singleBefore, "a root that stands for its focused control is UI Automation's to report");
}

// Only the message a gain posted ends its turn. The message of an earlier gain, which the window lost before the loop
// turned, arrives while the later gain's turn runs and ends nothing: that turn still leaves its moves to the system and
// reports its gain when its own message comes.
void TestWindowHostEarlierGainsTurnEndMessageDoesNotEndALaterTurn()
{
    using namespace DxUi;
    FocusGainTestWindow test;
    ControlHost& host = test.window.Host();
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    SendMessageW(test.window.Hwnd(), WM_KILLFOCUS, 0, 0);
    SendMessageW(test.window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(host.DebugIsInFocusGainTurn(), "the later gain starts a turn");

    MSG earlier{};
    const WndMsg::RegisteredMessage turnEnd = WndMsg::WindowHostFocusGainTurnEnd();
    Require(turnEnd && PeekMessageW(&earlier, test.window.Hwnd(), turnEnd.value, turnEnd.value, PM_REMOVE) != FALSE,
            "the earlier gain's message is queued first");
    static_cast<void>(DispatchQueuedMessageForTest(earlier));
    Require(host.DebugIsInFocusGainTurn(), "the earlier gain's message does not end the later turn");
    test.window.PumpMessages();
    Require(! host.DebugIsInFocusGainTurn(), "the later gain's own message ends it");
}

// DxUi registers each of its private messages by name, so each has a value of its own from 0xC000-0xFFFF: never WM_NULL,
// and never one an application gives its own messages (WM_USER and WM_APP values lie below 0xC000). Each accessor returns
// the value its documented name registers, the same on every call. A failed registration cannot be caused here without
// hooking RegisterWindowMessageW (filling the session's atom table, the only other way, would break every application of
// the session), so its contract is checked on the type instead: a failed message is falsy, so no sender posts or sends
// it; Matches recognizes no message for it, WM_NULL included; and a message does not compare with a message value, so a
// receiver cannot test one without Matches.
void TestDxUiPrivateMessagesAreRegisteredDistinctAndNeverWmNull()
{
    using DxUi::WndMsg::RegisteredMessage;
    static_assert(! std::is_convertible_v<RegisteredMessage, UINT>, "a registered message never stands in for a message value");
    static_assert(! std::equality_comparable_with<RegisteredMessage, UINT>, "a receiver recognizes a registered message only through Matches");

    const RegisteredMessage failed{};
    Require(! failed, "a failed registration is falsy, so no sender posts or sends it");
    Require(! failed.Matches(WM_NULL), "a failed registration does not match WM_NULL");
    Require(! failed.Matches(WM_APP) && ! failed.Matches(0xC000u), "a failed registration matches no message at all");
    const RegisteredMessage registered{0xC123u};
    Require(registered && registered.Matches(0xC123u), "a registered message matches its own value");
    Require(! registered.Matches(WM_NULL) && ! registered.Matches(0xC124u), "a registered message matches no other value");

    struct Accessor final
    {
        std::string_view name;
        RegisteredMessage (*get)() noexcept = nullptr;
    };
    const std::array<Accessor, 17> accessors = {{
        {"RedSalamanders.DxUi.WindowHost.FocusGainTurnEnd.v1", &DxUi::WndMsg::WindowHostFocusGainTurnEnd},
        {"RedSalamanders.DxUi.WindowHost.ProcessExitDetach.v1", &DxUi::WndMsg::WindowHostProcessExitDetach},
        {"RedSalamanders.DxUi.Accessibility.UiThreadAction.v1", &DxUi::WndMsg::AccessibilityUiThreadAction},
        {"RedSalamanders.DxUi.Accessibility.CreateProvider.v2", &DxUi::WndMsg::AccessibilityCreateProvider},
        {"RedSalamanders.DxUi.ContextMenu.RootHoverChanged.v1", &DxUi::WndMsg::ContextMenuRootHoverChanged},
        {"RedSalamanders.DxUi.MenuPopup.AccessibleInvoke.v1", &DxUi::WndMsg::MenuPopupAccessibleInvoke},
        {"RedSalamanders.DxUi.MenuPopup.AccessibleFocus.v1", &DxUi::WndMsg::MenuPopupAccessibleFocus},
        {"RedSalamanders.DxUi.MenuPopup.DeferredFinalize.v1", &DxUi::WndMsg::MenuPopupDeferredFinalize},
        {"RedSalamanders.DxUi.MenuPopup.ForwardCursor.v1", &DxUi::WndMsg::MenuPopupForwardCursor},
        {"RedSalamanders.DxUi.MenuPopup.DebugCaptureBitmap.v1", &DxUi::WndMsg::MenuPopupDebugCaptureBitmap},
        {"RedSalamanders.DxUi.MenuPopup.DebugGetItemText.v1", &DxUi::WndMsg::MenuPopupDebugGetItemText},
        {"RedSalamanders.DxUi.MenuPopup.DebugSetBackdrop.v1", &DxUi::WndMsg::MenuPopupDebugSetBackdrop},
        {"RedSalamanders.DxUi.MenuPopup.DebugGetState.v1", &DxUi::WndMsg::MenuPopupDebugGetState},
        {"RedSalamanders.DxUi.MenuPopup.DebugGetItemRect.v1", &DxUi::WndMsg::MenuPopupDebugGetItemRect},
        {"RedSalamanders.DxUi.MenuPopup.DebugGetItemPaint.v1", &DxUi::WndMsg::MenuPopupDebugGetItemPaint},
        {"RedSalamanders.DxUi.MenuPopup.DebugGetItemLayout.v1", &DxUi::WndMsg::MenuPopupDebugGetItemLayout},
        {"RedSalamanders.DxUi.TextInputServices.DeferredLock.v1", &DxUi::WndMsg::TextInputServicesDeferredLock},
    }};
    std::vector<UINT> values;
    for (const Accessor& accessor : accessors)
    {
        const RegisteredMessage message = accessor.get();
        const std::wstring name(accessor.name.begin(), accessor.name.end());
        Require(message.value >= 0xC000u && message.value <= 0xFFFFu, std::format("{} is a registered message (0xC000-0xFFFF)", accessor.name).c_str());
        Require(message.value == RegisterWindowMessageW(name.c_str()), std::format("{} is the message its name registers", accessor.name).c_str());
        Require(accessor.get().value == message.value, std::format("{} returns its cached value on every call", accessor.name).c_str());
        values.push_back(message.value);
    }
    std::ranges::sort(values);
    Require(std::ranges::adjacent_find(values) == values.end(), "no two DxUi private messages share a value");
}

// Before DxUi registered its messages they were WM_APP + 0x6A through 0x6D and WM_APP + 0x539, values an application also
// gives its own messages on the window it shares with DxUi (RedSalamander's test message at WM_APP + 0x6A was DxUi's
// accessibility action). Now an application message at each of them, sent or posted, reaches the window procedure with its
// parameters after HandleMessage, which neither consumes it nor acts on it: the host stays attached, no root provider is
// stored through lParam, none of the application's posted payloads is taken, and the turn of a focus gain runs on until
// its own registered message ends it.
void TestWindowHostLeavesApplicationMessagesAtDxUisFormerValuesToTheApplication()
{
    using namespace DxUi;
    struct Delivery final
    {
        UINT message  = 0u;
        WPARAM wParam = 0u;
        LPARAM lParam = 0;
    };
    struct PayloadProbe final
    {
        bool* destroyed = nullptr;
        ~PayloadProbe()
        {
            *destroyed = true;
        }
    };
    // Static, so the handler below sees one array: MSVC used separate copies of a non-static constexpr local there.
    static constexpr std::array<UINT, 5> kFormerValues = {WM_APP + 0x06Au, WM_APP + 0x06Bu, WM_APP + 0x06Cu, WM_APP + 0x06Du, WM_APP + 0x539u};
    constexpr UINT kApplicationPayloadMessage          = WM_APP + 0x0A0u;
    constexpr LRESULT kApplicationAnswer               = 0x5EED;
    std::vector<Delivery> delivered;
    bool payloadDestroyed = false;

    AttachedHostWindow window;
    auto root = std::make_unique<Panel>();
    root->AddChild<Button>(L"Premier bouton")->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    window.Host().SetRoot(std::move(root));
    window.SetApplicationMessageHandler([&](UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
    {
        if (! std::ranges::contains(kFormerValues, message))
            return false;
        delivered.push_back(Delivery{message, wParam, lParam});
        result = kApplicationAnswer;
        return true;
    });
    window.PumpMessages();

    // A focus gain posts its registered turn-end message, which names the turn in wParam.
    SendMessageW(window.Hwnd(), WM_SETFOCUS, 0, 0);
    Require(window.Host().DebugIsInFocusGainTurn(), "the focus gain starts its turn");
    const WndMsg::RegisteredMessage turnEnd = WndMsg::WindowHostFocusGainTurnEnd();
    MSG gain{};
    Require(turnEnd && PeekMessageW(&gain, window.Hwnd(), turnEnd.value, turnEnd.value, PM_NOREMOVE) != FALSE,
            "the focus gain posts its registered turn-end message");

    // A payload of the application's own, which the former accessibility action took when its lParam named it.
    auto probe       = std::make_unique<PayloadProbe>();
    probe->destroyed = &payloadDestroyed;
    Require(PostMessagePayload(window.Hwnd(), kApplicationPayloadMessage, 0, std::move(probe)), "the application posts a payload to its window");
    MSG payload{};
    Require(PeekMessageW(&payload, window.Hwnd(), kApplicationPayloadMessage, kApplicationPayloadMessage, PM_REMOVE) != FALSE,
            "the application's payload message is queued");

    IRawElementProviderFragmentRoot* storedProvider = nullptr;
    const std::array<Delivery, 5> messages          = {{
        {WM_APP + 0x06Au, 0u, payload.lParam},                            // The former accessibility action took the payload lParam named.
        {WM_APP + 0x06Bu, 0u, reinterpret_cast<LPARAM>(&storedProvider)}, // The former provider creation stored a root through lParam.
        {WM_APP + 0x06Cu, 0u, 0},                                         // The former process-exit detach.
        {WM_APP + 0x06Du, gain.wParam, 0},                                // The former turn end, naming the turn that runs.
        {WM_APP + 0x539u, 1u, 2},                                         // The former menu-bar hover: item 1, sequence 2.
    }};
    const auto requireDelivered                     = [&](const char* how)
    {
        Require(delivered.size() == messages.size(),
                std::format("every application message {} at a former DxUi value reaches the window procedure", how).c_str());
        for (size_t index = 0u; index < messages.size(); ++index)
        {
            const Delivery& expected = messages[index];
            const Delivery& actual   = delivered[index];
            Require(actual.message == expected.message && actual.wParam == expected.wParam && actual.lParam == expected.lParam,
                    std::format("the application message {} at WM_APP + {:#x} reaches the window procedure with its parameters", how, expected.message - WM_APP)
                        .c_str());
        }
    };

    for (const Delivery& message : messages)
    {
        Require(SendMessageW(window.Hwnd(), message.message, message.wParam, message.lParam) == kApplicationAnswer,
                std::format("the application's procedure answers its message sent at WM_APP + {:#x}", message.message - WM_APP).c_str());
    }
    wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> stored;
    stored.attach(std::exchange(storedProvider, nullptr)); // Releases a root that a receiver of the former value stored.
    requireDelivered("sent");
    // The detach check comes first: a detach also drains the window's posted payloads.
    Require(window.Host().GetHwnd() == window.Hwnd(), "the former process-exit value detaches nothing");
    Require(stored == nullptr, "no root provider is stored through the lParam of the former provider-creation value");
    Require(! payloadDestroyed, "the former accessibility-action value takes none of the application's payloads");
    Require(window.Host().DebugIsInFocusGainTurn(), "the former turn-end value ends no turn");
    Require(TakeMessagePayload<PayloadProbe>(window.Hwnd(), kApplicationPayloadMessage, payload.lParam) != nullptr,
            "the application takes its own payload back");
    Require(payloadDestroyed, "the application's payload is destroyed once taken back");

    // Posted, they reach the application through the message loop too, after the gain's own message has ended its turn.
    delivered.clear();
    for (const Delivery& message : messages)
    {
        Require(PostMessageW(window.Hwnd(), message.message, message.wParam, message.lParam) != FALSE,
                "the application posts its message at a former DxUi value");
    }
    window.PumpMessages();
    stored.attach(std::exchange(storedProvider, nullptr));
    requireDelivered("posted");
    Require(stored == nullptr, "no root provider is stored through the lParam of a posted former provider-creation value");
    Require(! window.Host().DebugIsInFocusGainTurn(), "the gain's own registered message ends its turn");
    Require(window.Host().GetHwnd() == window.Hwnd(), "the host stays attached");
}

} // namespace

void TestWindowHostWorksWithoutOptionalSdkDebugLayer()
{
    wil::com_ptr_nothrow<ID3D11Device> device;
    wil::com_ptr_nothrow<ID3D11DeviceContext> context;
    const HRESULT debugResult = D3D11CreateDevice(nullptr,
                                                  D3D_DRIVER_TYPE_WARP,
                                                  nullptr,
                                                  D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
                                                  nullptr,
                                                  0,
                                                  D3D11_SDK_VERSION,
                                                  device.put(),
                                                  nullptr,
                                                  context.put());
    std::cout << "D3D11 WARP optional debug-layer probe: 0x" << std::hex << static_cast<unsigned long>(debugResult) << std::dec << '\n';
    Require(SUCCEEDED(debugResult) || debugResult == DXGI_ERROR_SDK_COMPONENT_MISSING,
            "WARP probe either creates debug graphics or reports the optional SDK component missing");
    DxUi::WindowHost host;
    Require(host.GetTextFormat(DxUi::FontRole::Body) != nullptr, "native graphics and text initialize with or without the optional D3D SDK debug layer");
}

// Windows marks a mouse message it promoted from a touch or pen contact in the thread's extra message information, and the
// host reads it for each mouse message it handles: a slider that such a message drags shows its touch halo, and the next
// message from the mouse says the mouse again. SetMessageExtraInfo stands in for the input system, so no input is injected.
void TestWindowHostReadsThePointerDeviceOfEachMouseMessage()
{
    using namespace DxUi;

    Require(PointerDeviceFromMessageExtraInfo(0) == PointerDevice::Mouse, "no extra information is the mouse");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0xFF515780u)) == PointerDevice::Touch, "the signature with the touch bit is touch");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0xFF515700u)) == PointerDevice::Pen, "the signature without it is a pen");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0xFF5157FFu)) == PointerDevice::Touch, "the bits below the touch bit do not matter");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0xFF51577Fu)) == PointerDevice::Pen, "nor do they with it clear");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0x00515780u)) == PointerDevice::Mouse, "a near signature is the mouse");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(0x12345678u)) == PointerDevice::Mouse, "and so is an application's own value");
    Require(PointerDeviceFromMessageExtraInfo(static_cast<LPARAM>(static_cast<LONG>(0xFF515780u))) == PointerDevice::Touch,
            "a sign-extended value is read in its low 32 bits");

    WindowHost host;
    ThemePalette theme  = MakeDefaultThemePalette(false);
    theme.reducedMotion = true;
    host.SetTheme(theme);
    auto root    = std::make_unique<Panel>();
    auto* slider = root->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 48.0f));
    slider->SetValue(50.0);
    host.SetRoot(std::move(root));
    // A host without a window has no client area to lay its root out to.
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 64.0f));
    const D2D1_RECT_F thumb = slider->DebugGetThumbRect();
    const LPARAM point      = MAKELPARAM(static_cast<int>((thumb.left + thumb.right) * 0.5f), static_cast<int>((thumb.top + thumb.bottom) * 0.5f));

    struct ExtraInfoRestore final
    {
        LPARAM previous = 0;
        ~ExtraInfoRestore()
        {
            static_cast<void>(SetMessageExtraInfo(previous));
        }
    } restore{SetMessageExtraInfo(static_cast<LPARAM>(0xFF515780u))};
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, point, handled));
    Require(handled && host.GetPointerDevice() == PointerDevice::Touch, "the host reads a touch contact from the message it handles");
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 1.0f, 0.0001f, "and the slider that contact drags shows its touch halo");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, point, handled));
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "until the contact lifts");

    static_cast<void>(SetMessageExtraInfo(0));
    handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, point, handled));
    Require(handled && host.GetPointerDevice() == PointerDevice::Mouse, "the next message, from the mouse, says so");
    RequireFloatNear(slider->DebugGetTouchHaloProgress(), 0.0f, 0.0001f, "and its drag shows no touch halo");
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, point, handled));
}

void RunWindowHostTests()
{
    DXUI_RUN_TEST(TestWindowHostTakesTheRequestedLifetimeBeforePruningFocus);
    DXUI_RUN_TEST(TestWindowHostNewerCallbackFocusRequestWins);
    DXUI_RUN_TEST(TestWindowHostFailedBlurBeforeBasePreservesOnlyTheNewFocus);
    DXUI_RUN_TEST(TestWindowHostResetPreservesExplicitFocusAndRootReplacementRetiresOldChoice);
    DXUI_RUN_TEST(TestWindowHostThrowingFocusCallbacksAcknowledgeTransitionsAndRetirement);
    DXUI_RUN_TEST(TestWindowHostPromotesAChildBeforeDestroyingItsOldParent);
    DXUI_RUN_TEST(TestWindowHostPromotionSurvivesResetCallbacksRetiringTheParent);
    DXUI_RUN_TEST(TestWindowHostPromotionStopsWhenAnInheritanceCallbackReplacesTheRoot);
    DXUI_RUN_TEST(TestWindowHostPointerFocusCallbackCanDestroyTheClickedControl);
    DXUI_RUN_TEST(TestWindowHostWorksWithoutOptionalSdkDebugLayer);
    DXUI_RUN_TEST(TestDxUiTypographyMapsFontRolesToSegoeUiVariableFamilies);
    DXUI_RUN_TEST(TestWindowHostKeyboardInputMarksFocusVisible);
    DXUI_RUN_TEST(TestWindowHostPointerInputClearsKeyboardFocusVisible);
    DXUI_RUN_TEST(TestWindowHostRejectsForeignThreadDetachUntilOwnerDetaches);
    DXUI_RUN_TEST(TestWindowHostDetachKeepsSharedGraphicsAttachmentUntilControlTreeDestroyed);
    DXUI_RUN_TEST(TestWindowHostFocusLossCannotReattachDuringDetach);
    DXUI_RUN_TEST(TestWindowHostEmitsFrameStageMetricsForCaptureRender);
    DXUI_RUN_TEST(TestWindowHostCaptureRecoversAfterControlPaintThrows);
    DXUI_RUN_TEST(TestWindowHostCaptureDiscardsFrameWhenPaintReplacesItsRoot);
    DXUI_RUN_TEST(TestWindowHostCaptureDiscardsFrameWhenPaintMutatesTheChildTree);
    DXUI_RUN_TEST(TestWindowHostGridFocusMutationCannotRetargetTheCurrentClick);
    DXUI_RUN_TEST(TestWindowHostBlocksLayoutMutationDuringRender);
    DXUI_RUN_TEST(TestPostMessagePayloadTeardownDrainDeletesUndeliveredPayloads);
    DXUI_RUN_TEST(TestPostedPayloadTakeRequiresTheMatchingWindowMessageAndType);
    DXUI_RUN_TEST(TestPostedPayloadCapacityFailureDestroysAndAllowsRetry);
    DXUI_RUN_TEST(TestWindowHostAttachmentReservationIsExclusiveAndIdempotent);
    DXUI_RUN_TEST(TestWindowHostRejectedReattachmentPreservesBothHostsAndQueuedPayloads);
    DXUI_RUN_TEST(TestWindowHostAttachmentCapacityRejectsThe129thAndRecovers);
    DXUI_RUN_TEST(TestWindowHostAccessibilityRegistrationFailureRollsBackAndAllowsRetry);
    DXUI_RUN_TEST(TestWindowHostMouseMoveUpdatesHoverTarget);
    DXUI_RUN_TEST(TestWindowHostMouseLeaveOverForeignPopupClearsHover);
    DXUI_RUN_TEST(TestWindowHostMouseLeaveWithForeignCaptureClearsHover);
    DXUI_RUN_TEST(TestWindowHostTabTraversal);
    DXUI_RUN_TEST(TestWindowHostShiftTabTraversal);
    DXUI_RUN_TEST(TestWindowHostTabBoundaryCallbackCanReplaceItself);
    DXUI_RUN_TEST(TestWindowHostTabBoundaryMutableCallbackRetainsState);
    DXUI_RUN_TEST(TestWindowHostTabBoundaryCanRetireTheNextTarget);
    DXUI_RUN_TEST(TestWindowHostTabBoundarySnapshotCleanupRetiresTheValidatedTarget);
    DXUI_RUN_TEST(TestWindowHostNativeFocusLossRetainsLogicalFocusForTraversal);
    DXUI_RUN_TEST(TestWindowHostFocusGainTurnEndsWithItsMessageOrTheLossOfFocusOrItsLimit);
    DXUI_RUN_TEST(TestWindowHostFocusMoveInTheGainTurnOfAWindowNoGetFocusHasBegunOnIsLeftToTheSystem);
    DXUI_RUN_TEST(TestWindowHostFocusMoveAfterAGetFocusCallHasBegunIsAnnouncedByTheHost);
    DXUI_RUN_TEST(TestWindowHostFocusMoveInTheGainTurnOfAWindowGetFocusHasBegunOnBeforeIsAnnouncedByTheHost);
    DXUI_RUN_TEST(TestWindowHostFocusMovesOutsideTheGainTurnAreTheHosts);
    DXUI_RUN_TEST(TestWindowHostGetFocusHeldBeforeItCountsReportsTheMoveTheHostLeftToTheSystem);
    DXUI_RUN_TEST(TestWindowHostGainOfAWindowAskedBeforeIsAnnouncedWhenItsTurnEnds);
    DXUI_RUN_TEST(TestWindowHostEndOfAGainTurnAnnouncesOnlyWhatNothingElseReports);
    DXUI_RUN_TEST(TestWindowHostEarlierGainsTurnEndMessageDoesNotEndALaterTurn);
    DXUI_RUN_TEST(TestDxUiPrivateMessagesAreRegisteredDistinctAndNeverWmNull);
    DXUI_RUN_TEST(TestWindowHostLeavesApplicationMessagesAtDxUisFormerValuesToTheApplication);
    DXUI_RUN_TEST(TestWindowHostReturnInvokesDefaultButtonWhenFocusedControlDoesNotOwnEnter);
    DXUI_RUN_TEST(TestWindowHostReturnInvokesDefaultButtonWhenNoControlIsFocused);
    DXUI_RUN_TEST(TestWindowHostReturnDoesNotInvokeDefaultButtonWhenFocusedControlOwnsEnter);
    DXUI_RUN_TEST(TestButtonKeyboardActivationCanReplaceRootSafely);
    DXUI_RUN_TEST(TestWindowHostSpaceAndReturnInvokeFocusedButtonWithoutDefaultButtonFallback);
    DXUI_RUN_TEST(TestWindowHostPointerDispatchDoesNotReuseTargetAfterRootReplacement);
    DXUI_RUN_TEST(TestWindowHostHoverEnterDoesNotReuseTargetAfterRootReplacement);
    DXUI_RUN_TEST(TestWindowHostDpiChangedIsHandled);
    DXUI_RUN_TEST(TestWindowHostDpiChangedInvalidatesMultilineCachesAndResizesAttachedWindow);
    DXUI_RUN_TEST(TestWindowHostAttachedWindowsRenderAcrossUiThreads);
    DXUI_RUN_TEST(TestWindowHostEscapeInvokesCancelButton);
    DXUI_RUN_TEST(TestWindowHostEscapeCallbackCanReplaceItself);
    DXUI_RUN_TEST(TestWindowHostEscapeMutableCallbackRetainsState);
    DXUI_RUN_TEST(TestNativeMenuBarRefreshMutableCallbackRetainsState);
    DXUI_RUN_TEST(TestWindowHostFocusChangedMutableCallbackRetainsState);
    DXUI_RUN_TEST(TestWindowHostLogicalEditorFocusCanAvoidNativeActivation);
    DXUI_RUN_TEST(TestWindowHostCaptionMessagesReachApplicationAndDefaultHandling);
    DXUI_RUN_TEST(TestWindowHostEscapeClosesComboPopupBeforeCancelButton);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedButtonContextMenu);
    DXUI_RUN_TEST(TestWindowHostShiftF10InvokesFocusedToggleContextMenu);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedCheckboxContextMenu);
    DXUI_RUN_TEST(TestWindowHostSpaceAndReturnToggleFocusedToggleWithoutDefaultButtonFallback);
    DXUI_RUN_TEST(TestWindowHostSpaceTogglesFocusedCheckboxAndReturnInvokesDefaultButton);
    DXUI_RUN_TEST(TestWindowHostMixedDialogKeyboardFlowKeepsCommandsOnFocusedControls);
    DXUI_RUN_TEST(TestWindowHostMixedDialogMouseFlowKeepsCommandsOnHitControls);
    DXUI_RUN_TEST(TestWindowHostReadsThePointerDeviceOfEachMouseMessage);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedTreeContextMenu);
    DXUI_RUN_TEST(TestWindowHostShiftF10InvokesFocusedTreeContextMenu);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedGridContextMenu);
    DXUI_RUN_TEST(TestWindowHostShiftF10InvokesFocusedGridContextMenu);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedTextFieldContextMenu);
    DXUI_RUN_TEST(TestWindowHostMenuKeyInvokesFocusedComboContextMenu);
    DXUI_RUN_TEST(TestWindowHostSetRootClearsDestroyedTreeInteractionState);
    DXUI_RUN_TEST(TestWindowHostDetachDeactivatesSecureTextInputBeforeDestroyingRoot);
    DXUI_RUN_TEST(TestWindowHostProcessExitDetachAbandonsRetainedControlObserversBeforeNativeTeardown);
    DXUI_RUN_TEST(TestProcessExitShutdownMarshalsForeignWindowHostDetachToOwnerThread);
    DXUI_RUN_TEST(TestWindowHostClearChildrenPrunesDestroyedTreeInteractionState);
    DXUI_RUN_TEST(TestWindowHostKeyDownCallbackDisablingFocusPrunesBeforePostDispatchSync);
    DXUI_RUN_TEST(TestWindowHostCharCallbackDisablingFocusPrunesBeforePostDispatchSync);
    DXUI_RUN_TEST(TestWindowHostFocusLossCallbackRevalidatesRequestedFocusTarget);
    DXUI_RUN_TEST(TestWindowHostIgnoresObserverButtonsOutsideInstalledRoot);
    DXUI_RUN_TEST(TestWindowHostIgnoresFocusAndCaptureOutsideInstalledRoot);
    DXUI_RUN_TEST(TestWindowHostCaptureLossClearsPressedButtonState);
    DXUI_RUN_TEST(TestWindowHostResetInteractionStateNotifiesCapturedControl);
    DXUI_RUN_TEST(TestWindowHostRedundantCaptureDoesNotCancelMouseDownCapture);
    DXUI_RUN_TEST(TestWindowHostRenderSurvivesForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostEditorControlsSurviveForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostGridPaintSurvivesForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostTreeFocusOutlineSurvivesForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostScrollPanelScrollbarSurvivesForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostComboBoxPopupSurvivesForcedNullSolidBrushes);
    DXUI_RUN_TEST(TestWindowHostDisabledOrHiddenCaptureCancelsTheDrag);
    DXUI_RUN_TEST(TestWindowHostSmokeOverlayRendersBelowRootOverlay);
    DXUI_RUN_TEST(TestWindowHostOverlayHitTestingPrecedesContentHitTesting);
    DXUI_RUN_TEST(TestWindowHostOverlayHitTestRetiringRootStopsPointerResolutionAndDispatch);
    DXUI_RUN_TEST(TestWindowHostContainsMouseDownCallbackExceptionAndContinues);
    DXUI_RUN_TEST(TestWindowHostEscapeClosesMouseOpenedComboPopupBeforeCancelButton);
    DXUI_RUN_TEST(TestWindowHostTabTraversalIncludesComboBox);
    DXUI_RUN_TEST(TestWindowHostTabTraversalStaysConsistentAcrossFieldComboTreeGridAndButtons);
    DXUI_RUN_TEST(TestWindowHostGroupedListNavigationKeepsTreeTypeaheadAndGridSelectionVisible);
    DXUI_RUN_TEST(TestWindowHostAltDownOpensComboPopup);
    DXUI_RUN_TEST(TestWindowHostAltUpClosesComboPopup);
    DXUI_RUN_TEST(TestWindowHostMnemonicActivatesButton);
    DXUI_RUN_TEST(TestWindowHostLabelMnemonicTargetsField);
    DXUI_RUN_TEST(TestWindowHostUnknownMnemonicRemainsUnhandled);
    DXUI_RUN_TEST(TestWindowHostHiddenAnimationTickDropsSubscriptionUntilShown);
    DXUI_RUN_TEST(TestWindowHostRestoreFromMinimizeRearmsSuspendedAnimation);
    DXUI_RUN_TEST(TestNoninteractiveWindowActivationBlockerRejectsFocusStealing);
}
