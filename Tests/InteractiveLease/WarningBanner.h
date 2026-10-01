#pragma once

#include "../Support/InteractiveLease.h"

#include <chrono>
#include <format>
#include <string>

namespace DxUi::InteractiveLease
{
// The warning that stays on screen while the tests run: a banner at the top of the primary monitor, above every other window, that
// the pointer and keys pass through (WS_EX_TRANSPARENT), so a test's click at those pixels reaches the window under it. The lease
// activates it once, before the first child starts, because it is the window that holds the foreground while the lease grants
// the child the right to take it, and again when the lease hands the foreground back to the person's window.
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
        _window.reset();
    }

private:
    static constexpr PCWSTR kWindowClassName = L"DxUi.InteractiveLease.Warning";
    static constexpr UINT_PTR kTimerId       = 1u;

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
            draw(_bodyFont.get(), L"Hands off the keyboard and mouse. Your window, focus and pointer come back when it ends.", rowLine);
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
    unsigned _estimateSeconds = 0u;
    UINT _dpi                 = 96u;
    std::chrono::steady_clock::time_point _started{};
};
} // namespace DxUi::InteractiveLease
