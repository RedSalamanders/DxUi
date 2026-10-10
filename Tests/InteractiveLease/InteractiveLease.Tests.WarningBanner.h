#pragma once

#include "../Support/Support.Tests.InteractiveLease.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <windowsx.h>

namespace DxUi::InteractiveLease
{
enum class HostedWarningInputFailure
{
    None,
    HostedAuthorizationLost,
    MouseButtonHeld,
    PatchUnavailable,
    WarningDoesNotOwnPoint,
    PointerMoveFailed,
    ButtonDownFailed,
    ForegroundWaitFailed,
    ButtonUpFailed,
    MessagePumpFailed,
    ForegroundNotTaken,
    PatchRestoreFailed
};

// A failed ordinary activation gets this input path only in the separately authorized hosted mode. Kept as a pure decision so the
// local confirmation path cannot accidentally acquire synthetic input as a fallback.
[[nodiscard]] constexpr bool ShouldUseHostedWarningInputFallback(bool hosted, bool ordinaryActivationSucceeded) noexcept
{
    return hosted && ! ordinaryActivationSucceeded;
}

// The small operation surface makes the failure ordering testable without touching a real desktop. Implementations are noexcept and
// own the platform-specific checks, including exact warning-window hit-testing before/after moving the cursor.
template <typename Operations> [[nodiscard]] HostedWarningInputFailure ActivateHostedWarningWithInput(Operations& operations) noexcept
{
    if (! operations.IsAuthorized())
        return HostedWarningInputFailure::HostedAuthorizationLost;
    if (! operations.MouseButtonsReleased())
        return HostedWarningInputFailure::MouseButtonHeld;
    if (! operations.BeginPatch())
        return HostedWarningInputFailure::PatchUnavailable;

    HostedWarningInputFailure failure = HostedWarningInputFailure::None;
    if (! operations.WarningOwnsPoint())
        failure = HostedWarningInputFailure::WarningDoesNotOwnPoint;
    else if (! operations.PlacePointerAndVerify())
        failure = HostedWarningInputFailure::PointerMoveFailed;
    else if (! operations.WarningOwnsPoint())
        failure = HostedWarningInputFailure::WarningDoesNotOwnPoint;
    else if (! operations.IsAuthorized())
        failure = HostedWarningInputFailure::HostedAuthorizationLost;
    else if (! operations.MouseButtonsReleased())
        failure = HostedWarningInputFailure::MouseButtonHeld;
    else if (! operations.SendLeftButtonDown())
        failure = HostedWarningInputFailure::ButtonDownFailed;
    else
    {
        if (! operations.PumpUntilForeground())
            failure = HostedWarningInputFailure::ForegroundWaitFailed;
        // Always attempt the matching up event after an inserted down, including when the bounded pump failed.
        bool upInserted = operations.SendLeftButtonUp();
        if (! upInserted)
        {
            failure    = HostedWarningInputFailure::ButtonUpFailed;
            upInserted = operations.SendLeftButtonUp(); // One cleanup retry; a failed insertion never becomes a pass.
        }
        // An inserted up is only queued. Keep the owned patch until its matching down/up messages were dispatched and Windows
        // reports released buttons, including cleanup after a failed foreground wait. Never pass on SendInput's count alone.
        const bool released = operations.PumpUntilButtonReleased(); // Also read back after both up insertions fail; cleanup is bounded.
        if (upInserted && ! released)
            failure = HostedWarningInputFailure::MessagePumpFailed;
        if (failure == HostedWarningInputFailure::None && ! operations.WarningIsForeground())
            failure = HostedWarningInputFailure::ForegroundNotTaken;
    }

    if (! operations.EndPatch())
        failure = HostedWarningInputFailure::PatchRestoreFailed;
    return failure;
}

[[nodiscard]] constexpr std::string_view HostedWarningInputFailureText(HostedWarningInputFailure failure) noexcept
{
    switch (failure)
    {
        case HostedWarningInputFailure::None: return "none";
        case HostedWarningInputFailure::HostedAuthorizationLost: return "the exact hosted runner authorization is missing";
        case HostedWarningInputFailure::MouseButtonHeld: return "a mouse button was already held";
        case HostedWarningInputFailure::PatchUnavailable: return "the warning input patch could not be exposed";
        case HostedWarningInputFailure::WarningDoesNotOwnPoint: return "the warning does not own the safe input point";
        case HostedWarningInputFailure::PointerMoveFailed: return "the pointer could not be placed on the warning";
        case HostedWarningInputFailure::ButtonDownFailed: return "the warning click could not begin";
        case HostedWarningInputFailure::ForegroundWaitFailed: return "the warning did not become foreground after the click";
        case HostedWarningInputFailure::ButtonUpFailed: return "the warning click could not be released";
        case HostedWarningInputFailure::MessagePumpFailed: return "the warning click could not be fully dispatched";
        case HostedWarningInputFailure::ForegroundNotTaken: return "the warning did not retain the foreground after the click";
        case HostedWarningInputFailure::PatchRestoreFailed: return "the warning input patch could not be reverted";
    }
    return "unknown warning input failure";
}

// The warning that stays on screen while the tests run: a banner at the top of the primary monitor, above every other window, that
// ordinary pointer input passes through. Only a verified hosted-runner recovery briefly exposes its marked patch as an input target.
// The lease activates the banner before the first child starts, because it holds the foreground while the lease grants the child the
// right to take it, and again when the lease hands the foreground back to the person's window.
class WarningBanner final
{
public:
    WarningBanner()                                = default;
    WarningBanner(const WarningBanner&)            = delete;
    WarningBanner& operator=(const WarningBanner&) = delete;

    ~WarningBanner()
    {
        Hide();
    }

    [[nodiscard]] bool Show(const std::wstring& label, unsigned estimateSeconds)
    {
        Hide();
        _label           = label;
        _estimateSeconds = estimateSeconds;
        _started         = std::chrono::steady_clock::now();
        if (EnsureWindowClass() == 0)
            return false;
        _window.reset(CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
                                      kWindowClassName,
                                      L"DxUi interactive tests",
                                      WS_POPUP | WS_BORDER,
                                      0,
                                      0,
                                      100,
                                      40,
                                      nullptr,
                                      nullptr,
                                      GetModuleHandleW(nullptr),
                                      this));
        if (! _window)
            return false;
        SetLayeredWindowAttributes(_window.get(), 0, 235, LWA_ALPHA);
        Place();
        SetTimer(_window.get(), kTimerId, 1000u, nullptr); // The elapsed time moves once a second; nothing runs between.
        ShowWindow(_window.get(), SW_SHOWNOACTIVATE);
        UpdateWindow(_window.get());
        return true;
    }

    [[nodiscard]] HWND Window() const noexcept
    {
        return _window.get();
    }

    void Hide() noexcept
    {
        _patchActive = false;
        _window.reset();
    }

    // Temporarily clips the window to one visibly marked patch during verified hosted-runner recovery. HTTRANSPARENT only forwards
    // within one thread, so clipping also keeps every other point outside this HWND's input region when the underlying app differs.
    [[nodiscard]] bool BeginHostedActivationPatch() noexcept
    {
        if (_window == nullptr || IsWindow(_window.get()) == FALSE || _patchActive)
            return false;
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR current = GetWindowLongPtrW(_window.get(), GWL_EXSTYLE);
        if (current == 0 && GetLastError() != ERROR_SUCCESS)
            return false;
        _originalExtendedStyle = current;
        RECT patch{};
        RECT windowRect{};
        POINT clientOrigin{};
        if (! GetHostedActivationPatchRect(patch) || GetWindowRect(_window.get(), &windowRect) == FALSE ||
            ClientToScreen(_window.get(), &clientOrigin) == FALSE)
            return false;
        OffsetRect(&patch, clientOrigin.x - windowRect.left, clientOrigin.y - windowRect.top);
        wil::unique_hrgn region(CreateRectRgn(patch.left, patch.top, patch.right, patch.bottom));
        if (! region || SetWindowRgn(_window.get(), region.get(), TRUE) == 0)
            return false;
        static_cast<void>(region.release()); // Windows owns the region after a successful SetWindowRgn.
        _patchActive         = true;
        _patchButtonDown     = false;
        _patchButtonReleased = false;
        const LONG_PTR next  = current & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR previous = SetWindowLongPtrW(_window.get(), GWL_EXSTYLE, next);
        if (previous == 0 && GetLastError() != ERROR_SUCCESS)
        {
            static_cast<void>(EndHostedActivationPatch());
            return false;
        }
        if (SetWindowPos(_window.get(), nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) == FALSE)
        {
            static_cast<void>(EndHostedActivationPatch());
            return false;
        }
        if (GetWindowLongPtrW(_window.get(), GWL_EXSTYLE) != next || ! GetHostedActivationPatchRect(patch))
        {
            static_cast<void>(EndHostedActivationPatch());
            return false;
        }
        InvalidateRect(_window.get(), nullptr, FALSE);
        UpdateWindow(_window.get());
        return true;
    }

    [[nodiscard]] bool EndHostedActivationPatch() noexcept
    {
        if (! _patchActive || _window == nullptr || IsWindow(_window.get()) == FALSE)
        {
            _patchActive = false;
            return false;
        }
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR previous   = SetWindowLongPtrW(_window.get(), GWL_EXSTYLE, _originalExtendedStyle);
        const bool styleRestored  = previous != 0 || GetLastError() == ERROR_SUCCESS;
        _patchActive              = false; // The WndProc is fail-closed even if Windows refuses the style restoration.
        const bool regionRestored = SetWindowRgn(_window.get(), nullptr, TRUE) != 0; // The ordinary warning has no explicit region.
        const bool frameRestored =
            SetWindowPos(_window.get(), nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE;
        InvalidateRect(_window.get(), nullptr, FALSE);
        UpdateWindow(_window.get());
        wil::unique_hrgn actualRegion(CreateRectRgn(0, 0, 0, 0));
        return styleRestored && regionRestored && frameRestored && GetWindowLongPtrW(_window.get(), GWL_EXSTYLE) == _originalExtendedStyle && actualRegion &&
               GetWindowRgn(_window.get(), actualRegion.get()) == ERROR;
    }

    [[nodiscard]] bool HostedActivationPoint(POINT& screenPoint) const noexcept
    {
        const HWND window = _window.get();
        if (! _patchActive || window == nullptr || IsWindow(window) == FALSE)
            return false;
        RECT patch{};
        if (! GetHostedActivationPatchRect(patch))
            return false;
        screenPoint = POINT{(patch.left + patch.right) / 2, (patch.top + patch.bottom) / 2};
        return ClientToScreen(window, &screenPoint) != FALSE;
    }

    [[nodiscard]] bool HostedActivationOwnsPoint() const noexcept
    {
        POINT point{};
        return HostedActivationPoint(point) && WindowFromPoint(point) == _window.get();
    }

    [[nodiscard]] bool HostedActivationClickReleased() const noexcept
    {
        return _patchActive && _patchButtonDown && _patchButtonReleased;
    }

private:
    static constexpr PCWSTR kWindowClassName = L"DxUi.InteractiveLease.Warning";
    static constexpr UINT_PTR kTimerId       = 1u;

    [[nodiscard]] bool GetHostedActivationPatchRect(RECT& patch) const noexcept
    {
        const HWND window = _window.get();
        RECT client{};
        if (window == nullptr || GetClientRect(window, &client) == FALSE)
            return false;
        const auto scale = [this](int dip) noexcept { return MulDiv(dip, static_cast<int>(_dpi), 96); };
        const int width  = scale(48);
        const int height = scale(26);
        const int margin = scale(8);
        patch            = RECT{client.right - margin - width, client.bottom - margin - height, client.right - margin, client.bottom - margin};
        return patch.left > client.left && patch.top > client.top && patch.right < client.right && patch.bottom < client.bottom;
    }

    static ATOM EnsureWindowClass() noexcept
    {
        static const ATOM atom = []() noexcept
        {
            WNDCLASSEXW windowClass{};
            windowClass.cbSize        = sizeof(windowClass);
            windowClass.lpfnWndProc   = &WarningBanner::WindowProc;
            windowClass.hInstance     = GetModuleHandleW(nullptr);
            windowClass.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.lpszClassName = kWindowClassName;
            return RegisterClassExW(&windowClass);
        }();
        return atom;
    }

    // Top centre of the primary monitor's work area, sized and lettered for that monitor's DPI.
    void Place() noexcept
    {
        const HWND window = _window.get();
        const UINT dpi    = (std::max)(GetDpiForWindow(window), 96u);
        MONITORINFO monitor{sizeof(MONITORINFO)};
        RECT work{0, 0, 1280, 720};
        if (GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor) != FALSE)
            work = monitor.rcWork;
        const auto scale = [dpi](int dip) noexcept { return MulDiv(dip, static_cast<int>(dpi), 96); };
        const int width  = (std::min)(scale(760), static_cast<int>(work.right - work.left) - 2 * scale(8));
        const int height = scale(96);
        SetWindowPos(
            window, HWND_TOPMOST, work.left + (static_cast<int>(work.right - work.left) - width) / 2, work.top + scale(8), width, height, SWP_NOACTIVATE);

        NONCLIENTMETRICSW metrics{sizeof(NONCLIENTMETRICSW)};
        LOGFONTW body{};
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi) != FALSE)
            body = metrics.lfMessageFont;
        else
            body.lfHeight = -scale(13);
        LOGFONTW title = body;
        title.lfWeight = FW_SEMIBOLD;
        title.lfHeight = title.lfHeight * 5 / 4;
        _bodyFont.reset(CreateFontIndirectW(&body));
        _titleFont.reset(CreateFontIndirectW(&title));
        _dpi = dpi;
    }

    void Paint() noexcept
    {
        PAINTSTRUCT paint{};
        const HDC dc = BeginPaint(_window.get(), &paint);
        if (dc == nullptr)
            return;
        RECT client{};
        GetClientRect(_window.get(), &client);
        const wil::unique_hbrush background(CreateSolidBrush(RGB(255, 191, 0)));
        FillRect(dc, &client, background.get());
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(0, 0, 0));

        const auto scale   = [this](int dip) noexcept { return MulDiv(dip, static_cast<int>(_dpi), 96); };
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _started).count();
        const int margin   = scale(12);
        const int rowTitle = scale(34);
        const int rowLine  = scale(26);
        RECT row{client.left + margin, client.top + scale(6), client.right - margin, client.top + scale(6) + rowTitle};
        const UINT flags = DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
        try
        {
            const auto draw = [&](HFONT font, const std::wstring& text, int height)
            {
                row.bottom             = row.top + height;
                const HGDIOBJ previous = SelectObject(dc, font);
                DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &row, flags);
                SelectObject(dc, previous);
                row.top = row.bottom;
            };
            draw(_titleFont.get(), L"DxUi interactive tests are running", rowTitle);
            draw(_bodyFont.get(),
                 std::format(L"{}  -  {}:{:02} elapsed, {} expected",
                             _label,
                             elapsed / 60,
                             elapsed % 60,
                             TestSupport::FromUtf8(TestSupport::FormatApproximateDuration(_estimateSeconds))),
                 rowLine);
            if (_patchActive)
            {
                RECT patch{};
                if (GetHostedActivationPatchRect(patch))
                    row.right = patch.left - margin / 2;
            }
            draw(_bodyFont.get(), L"Hands off the keyboard and mouse. Your window, focus and pointer come back when it ends.", rowLine);
            if (_patchActive)
            {
                RECT patch{};
                if (GetHostedActivationPatchRect(patch))
                {
                    const wil::unique_hpen outline(CreatePen(PS_SOLID, (std::max)(1, scale(1)), RGB(0, 0, 0)));
                    const HGDIOBJ previousPen   = SelectObject(dc, outline.get());
                    const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    Rectangle(dc, patch.left, patch.top, patch.right, patch.bottom);
                    SelectObject(dc, previousBrush);
                    SelectObject(dc, previousPen);
                    RECT marker = patch;
                    DrawTextW(dc, L"CI", 2, &marker, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
                }
            }
        }
        catch (const std::bad_alloc&)
        {
            // The banner keeps what it drew; the next second's paint tries again.
        }
        EndPaint(_window.get(), &paint);
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept
    {
        if (message == WM_NCCREATE)
        {
            const auto* const create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        }
        auto* const self = reinterpret_cast<WarningBanner*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (self != nullptr && self->_window.get() == window)
        {
            switch (message)
            {
                case WM_ERASEBKGND: return 1;
                case WM_PAINT: self->Paint(); return 0;
                case WM_TIMER: InvalidateRect(window, nullptr, FALSE); return 0;
                case WM_DPICHANGED: self->Place(); return 0;
                case WM_NCHITTEST:
                    if (self->_patchActive)
                    {
                        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                        if (ScreenToClient(window, &point) != FALSE)
                        {
                            RECT patch{};
                            if (self->GetHostedActivationPatchRect(patch) && PtInRect(&patch, point) != FALSE)
                                return HTCLIENT;
                        }
                    }
                    return HTTRANSPARENT;
                case WM_MOUSEACTIVATE:
                    return self->_patchActive && LOWORD(lParam) == HTCLIENT && HIWORD(lParam) == WM_LBUTTONDOWN ? MA_ACTIVATE : MA_NOACTIVATE;
                case WM_LBUTTONDOWN:
                    if (self->_patchActive)
                        self->_patchButtonDown = true;
                    return 0;
                case WM_LBUTTONUP:
                    if (self->_patchActive && self->_patchButtonDown)
                        self->_patchButtonReleased = true;
                    return 0;
                default: break;
            }
        }
        switch (message)
        {
            case WM_NCHITTEST: return HTTRANSPARENT;
            case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
            case WM_NCDESTROY: SetWindowLongPtrW(window, GWLP_USERDATA, 0); break;
            default: break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    wil::unique_hwnd _window;
    wil::unique_hfont _bodyFont;
    wil::unique_hfont _titleFont;
    std::wstring _label;
    LONG_PTR _originalExtendedStyle = 0;
    bool _patchButtonDown           = false;
    bool _patchButtonReleased       = false;
    unsigned _estimateSeconds       = 0u;
    UINT _dpi                       = 96u;
    bool _patchActive               = false;
    std::chrono::steady_clock::time_point _started{};
};
} // namespace DxUi::InteractiveLease
