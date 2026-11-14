#pragma once

// Non-activating capture of a window's presented image: what the desktop compositor holds for the window's client area
// after Present, as opposed to a swap chain's back buffer read before Present.
//
// Method: DwmFlush waits for the compositor to take every outstanding DirectX surface update, then PrintWindow with
// PW_CLIENTONLY | PW_RENDERFULLCONTENT asks the compositor to render the window's own visual tree (the flip-model buffer
// it latched, DirectComposition visuals and the window's redirection surface below them) into a caller-owned bitmap. The
// call is synchronous, works for an occluded or off-screen window and never activates, focuses, raises or moves anything.
// PrintWindow also sends the window WM_PAINT (and WM_ERASEBKGND) to draw its GDI content into the print DC. A DxUi host
// would answer that by rendering and presenting a new full frame, replacing the very frame being read, so for the
// duration of the call the capture answers those two messages itself (validating, drawing nothing) and forwards every
// other message to the window procedure. For the same reason a window with a pending update region is refused
// (PaintPending): the caller lets it paint first. A caller waits for a frame with WaitForPresentedFrame: one compositor
// frame per attempt, bounded by a frame count, never by a sleep.
//
// Exactness: the window must be per-monitor DPI aware so the compositor neither stretches its content nor the capture; the
// image is then the client area at the window's device scale, one pixel per swap-chain pixel. The presented image is
// opaque: alpha is 255. A premultiplied composition swap chain is composed over the window's GDI layer; the capture's
// `backdrop` argument paints that layer (in the print only) with a defined color, and ComposeOverBackdrop computes the
// image a premultiplied back buffer should present over it.

#include <Windows.h>
#include <dwmapi.h>

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027 28182)
#include <wil/resource.h>
#pragma warning(pop)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace DxUi::TestSupport
{
struct PresentedWindowImage
{
    UINT widthPx                    = 0u;
    UINT heightPx                   = 0u;
    std::vector<uint8_t> bgraPixels = std::vector<uint8_t>(0u); // Top-down BGRA rows, alpha 255.
};

enum class PresentedCaptureStatus : uint8_t
{
    Captured,
    NoInteractiveDesktop, // The process has no visible window station: there is no compositor to read (a capability skip).
    NotPerMonitorAware,   // The compositor would scale the window: create it under ScopedPerMonitorV2Awareness.
    WindowNotPresentable, // Hidden, minimized or empty client area.
    CaptureFailed,        // PrintWindow or the bitmap failed.
    FrameNotShown,        // The compositor did not show a frame the predicate accepts within the frame budget.
    PaintPending,         // The window has an update region: PrintWindow would paint it first, so the read would not be
                          // the frame the caller presented. Dispatch the window's paint (or validate it) and read again.
};

[[nodiscard]] inline const char* DescribePresentedCaptureStatus(PresentedCaptureStatus status) noexcept
{
    switch (status)
    {
        case PresentedCaptureStatus::Captured: return "captured";
        case PresentedCaptureStatus::NoInteractiveDesktop: return "no interactive desktop";
        case PresentedCaptureStatus::NotPerMonitorAware: return "window is not per-monitor DPI aware";
        case PresentedCaptureStatus::WindowNotPresentable: return "window is hidden, minimized or empty";
        case PresentedCaptureStatus::CaptureFailed: return "PrintWindow capture failed";
        case PresentedCaptureStatus::FrameNotShown: return "the compositor did not show the expected frame within the frame budget";
        case PresentedCaptureStatus::PaintPending: return "the window has a pending paint that the capture would render";
    }
    return "unknown";
}

// Windows created on this thread while the scope lives are per-monitor (v2) DPI aware: their client pixels are device
// pixels and the compositor shows them unscaled.
class ScopedPerMonitorV2Awareness final
{
public:
    ScopedPerMonitorV2Awareness() noexcept : _previous(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
    }
    ~ScopedPerMonitorV2Awareness() noexcept
    {
        if (_previous)
            static_cast<void>(SetThreadDpiAwarenessContext(_previous));
    }
    ScopedPerMonitorV2Awareness(const ScopedPerMonitorV2Awareness&)            = delete;
    ScopedPerMonitorV2Awareness& operator=(const ScopedPerMonitorV2Awareness&) = delete;
    ScopedPerMonitorV2Awareness(ScopedPerMonitorV2Awareness&&)                 = delete;
    ScopedPerMonitorV2Awareness& operator=(ScopedPerMonitorV2Awareness&&)      = delete;

    [[nodiscard]] bool Applied() const noexcept
    {
        return _previous != nullptr;
    }

private:
    DPI_AWARENESS_CONTEXT _previous = nullptr;
};

// Coordinates exchanged with a window are virtualized unless the calling thread shares the window's DPI awareness; each
// helper below therefore runs in the window's own context.
class ScopedWindowDpiContext final
{
public:
    explicit ScopedWindowDpiContext(HWND hwnd) noexcept : _previous(SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hwnd)))
    {
    }
    ~ScopedWindowDpiContext() noexcept
    {
        if (_previous)
            static_cast<void>(SetThreadDpiAwarenessContext(_previous));
    }
    ScopedWindowDpiContext(const ScopedWindowDpiContext&)            = delete;
    ScopedWindowDpiContext& operator=(const ScopedWindowDpiContext&) = delete;
    ScopedWindowDpiContext(ScopedWindowDpiContext&&)                 = delete;
    ScopedWindowDpiContext& operator=(ScopedWindowDpiContext&&)      = delete;

private:
    DPI_AWARENESS_CONTEXT _previous = nullptr;
};

[[nodiscard]] inline bool HasInteractiveWindowStation() noexcept
{
    USEROBJECTFLAGS flags{};
    const HWINSTA station = GetProcessWindowStation();
    return station && GetUserObjectInformationW(station, UOI_FLAGS, &flags, sizeof(flags), nullptr) != FALSE && (flags.dwFlags & WSF_VISIBLE) != 0u;
}

[[nodiscard]] inline bool IsPerMonitorAwareWindow(HWND hwnd) noexcept
{
    const DPI_AWARENESS awareness = GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(hwnd));
    return awareness == DPI_AWARENESS_PER_MONITOR_AWARE;
}

// Shows a top-level window without activating it, at the bottom of the z-order and off every monitor, with a client area
// of exactly clientSizePx device pixels. Nothing is raised above the person's windows and the pointer is untouched.
[[nodiscard]] inline bool PlacePresentedCaptureWindow(HWND hwnd, SIZE clientSizePx) noexcept
{
    if (! hwnd || clientSizePx.cx <= 0 || clientSizePx.cy <= 0)
        return false;
    const ScopedWindowDpiContext context(hwnd);
    const auto style   = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const auto exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    RECT outer{0, 0, clientSizePx.cx, clientSizePx.cy};
    if (AdjustWindowRectExForDpi(&outer, style, FALSE, exStyle, GetDpiForWindow(hwnd)) == FALSE)
        return false;
    if (SetWindowPos(hwnd,
                     HWND_BOTTOM,
                     -32000,
                     -32000,
                     outer.right - outer.left,
                     outer.bottom - outer.top,
                     // Without WM_WINDOWPOSCHANGING the overlapped window's minimum tracking width (its caption buttons
                     // at the monitor's DPI) cannot widen a small capture window.
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_SHOWWINDOW) == FALSE)
        return false;
    // The frame the system actually applies can differ from AdjustWindowRectExForDpi's estimate; correct by the measured
    // difference (bounded).
    RECT client{};
    for (int attempt = 0; attempt < 3 && GetClientRect(hwnd, &client) != FALSE && (client.right != clientSizePx.cx || client.bottom != clientSizePx.cy);
         ++attempt)
    {
        RECT window{};
        if (GetWindowRect(hwnd, &window) == FALSE)
            return false;
        if (SetWindowPos(hwnd,
                         nullptr,
                         0,
                         0,
                         (window.right - window.left) + (clientSizePx.cx - client.right),
                         (window.bottom - window.top) + (clientSizePx.cy - client.bottom),
                         SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING | SWP_NOMOVE | SWP_NOZORDER) == FALSE)
            return false;
    }
    if (GetClientRect(hwnd, &client) == FALSE || client.right != clientSizePx.cx || client.bottom != clientSizePx.cy)
    {
        std::fprintf(stderr,
                     "    [PRESENTED] placement: client %ldx%ld, wanted %ldx%ld at dpi %u\n",
                     client.right,
                     client.bottom,
                     clientSizePx.cx,
                     clientSizePx.cy,
                     GetDpiForWindow(hwnd));
        return false;
    }
    return true;
}

namespace PresentedWindowCaptureDetail
{
inline thread_local LONG_PTR originalProcedure = 0;
inline thread_local COLORREF backdropColor     = CLR_INVALID;

inline LRESULT CALLBACK PaintAbsorbingProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) noexcept
{
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint{};
        if (const HDC dc = BeginPaint(hwnd, &paint))
        {
            // The window's GDI layer in the print: what a premultiplied composition swap chain is composed over.
            if (backdropColor != CLR_INVALID)
            {
                RECT client{};
                if (GetClientRect(hwnd, &client) != FALSE)
                    if (const wil::unique_hbrush brush(CreateSolidBrush(backdropColor)); brush)
                        static_cast<void>(FillRect(dc, &client, brush.get()));
            }
            EndPaint(hwnd, &paint);
        }
        return 0;
    }
    if (message == WM_ERASEBKGND)
        return 1;
    // The window's own procedure keeps the host's exception policy; this forwarding adds no exception of its own.
#pragma warning(suppress : 5039)
    return CallWindowProcW(reinterpret_cast<WNDPROC>(originalProcedure), hwnd, message, wParam, lParam);
}

// While alive, the window's own procedure never sees WM_PAINT or WM_ERASEBKGND.
class ScopedPaintAbsorber final
{
public:
    ScopedPaintAbsorber(HWND hwnd, COLORREF backdrop) noexcept : _hwnd(hwnd)
    {
        if (originalProcedure)
            return; // Not reentrant; the outer scope already absorbs.
        backdropColor     = backdrop;
        originalProcedure = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&PaintAbsorbingProcedure));
        _installed        = originalProcedure != 0;
    }
    ~ScopedPaintAbsorber() noexcept
    {
        if (! _installed)
            return;
        static_cast<void>(SetWindowLongPtrW(_hwnd, GWLP_WNDPROC, originalProcedure));
        originalProcedure = 0;
        backdropColor     = CLR_INVALID;
    }
    ScopedPaintAbsorber(const ScopedPaintAbsorber&)            = delete;
    ScopedPaintAbsorber& operator=(const ScopedPaintAbsorber&) = delete;
    ScopedPaintAbsorber(ScopedPaintAbsorber&&)                 = delete;
    ScopedPaintAbsorber& operator=(ScopedPaintAbsorber&&)      = delete;

    [[nodiscard]] bool Installed() const noexcept
    {
        return _installed;
    }

private:
    HWND _hwnd      = nullptr;
    bool _installed = false;
};
} // namespace PresentedWindowCaptureDetail

// One read of what the compositor currently shows for the window's client area, after one compositor frame. `backdrop`
// is the opaque color of the window's GDI layer in the read, the layer a premultiplied composition swap chain is composed
// over (CLR_INVALID leaves it undefined; an opaque HwndSwapChain covers it).
[[nodiscard]] inline PresentedCaptureStatus CapturePresentedClient(HWND hwnd, PresentedWindowImage& out, COLORREF backdrop = CLR_INVALID)
{
    out = {};
    if (! HasInteractiveWindowStation())
        return PresentedCaptureStatus::NoInteractiveDesktop;
    if (! IsPerMonitorAwareWindow(hwnd))
        return PresentedCaptureStatus::NotPerMonitorAware;
    const ScopedWindowDpiContext context(hwnd);
    RECT client{};
    if (IsWindowVisible(hwnd) == FALSE || IsIconic(hwnd) != FALSE || GetClientRect(hwnd, &client) == FALSE || client.right <= 0 || client.bottom <= 0)
        return PresentedCaptureStatus::WindowNotPresentable;
    // PrintWindow updates a window with a pending update region (a WM_PAINT on this thread) before it reads it.
    if (GetUpdateRect(hwnd, nullptr, FALSE) != FALSE)
        return PresentedCaptureStatus::PaintPending;

    // Outstanding Present calls reach the compositor before it renders the window for us.
    static_cast<void>(DwmFlush());

    const LONG width  = client.right;
    const LONG height = client.bottom;
    BITMAPINFO info{};
    info.bmiHeader.biSize        = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth       = width;
    info.bmiHeader.biHeight      = -height; // Top-down.
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits                   = nullptr;
    const wil::unique_hdc memoryDc(CreateCompatibleDC(nullptr));
    if (! memoryDc)
        return PresentedCaptureStatus::CaptureFailed;
    const wil::unique_hbitmap bitmap(CreateDIBSection(memoryDc.get(), &info, DIB_RGB_COLORS, &bits, nullptr, 0u));
    if (! bitmap || ! bits)
        return PresentedCaptureStatus::CaptureFailed;
    const auto selected = wil::SelectObject(memoryDc.get(), bitmap.get());

    BOOL printed = FALSE;
    {
        const PresentedWindowCaptureDetail::ScopedPaintAbsorber absorber(hwnd, backdrop);
        if (! absorber.Installed())
            return PresentedCaptureStatus::CaptureFailed;
        printed = PrintWindow(hwnd, memoryDc.get(), PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    }
    if (printed == FALSE || GdiFlush() == FALSE)
        return PresentedCaptureStatus::CaptureFailed;

    out.widthPx  = static_cast<UINT>(width);
    out.heightPx = static_cast<UINT>(height);
    out.bgraPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    std::memcpy(out.bgraPixels.data(), bits, out.bgraPixels.size());
    for (size_t index = 3u; index < out.bgraPixels.size(); index += 4u)
        out.bgraPixels[index] = 255u; // The compositor's output is opaque; GDI leaves the byte undefined.
    return PresentedCaptureStatus::Captured;
}

// Reads the presented image until `shown` accepts it, one compositor frame per attempt and at most maxCompositorFrames
// attempts. A caller passes a predicate that identifies the frame it presented (for example a pixel only that frame
// paints), so a frame the compositor has not taken yet is never compared. The last image read is returned either way.
[[nodiscard]] inline PresentedCaptureStatus WaitForPresentedFrame(HWND hwnd,
                                                                  const std::function<bool(const PresentedWindowImage&)>& shown,
                                                                  PresentedWindowImage& out,
                                                                  COLORREF backdrop            = CLR_INVALID,
                                                                  uint32_t maxCompositorFrames = 30u)
{
    for (uint32_t attempt = 0u; attempt < maxCompositorFrames; ++attempt)
    {
        const PresentedCaptureStatus status = CapturePresentedClient(hwnd, out, backdrop);
        if (status != PresentedCaptureStatus::Captured)
            return status;
        if (! shown || shown(out))
            return PresentedCaptureStatus::Captured;
    }
    return PresentedCaptureStatus::FrameNotShown;
}

// The opaque image a premultiplied BGRA buffer presents over an opaque backdrop: source + (1 - alpha) * backdrop, rounded.
// Pass composeOverBackdrop = false for a buffer whose alpha the presentation ignores (an opaque HWND swap chain).
[[nodiscard]] inline PresentedWindowImage ComposeOverBackdrop(
    UINT widthPx, UINT heightPx, const std::vector<uint8_t>& premultipliedBgra, bool composeOverBackdrop, COLORREF backdrop)
{
    PresentedWindowImage image;
    image.widthPx  = widthPx;
    image.heightPx = heightPx;
    image.bgraPixels.resize(premultipliedBgra.size());
    const uint32_t backdropBgr[3]{GetBValue(backdrop), GetGValue(backdrop), GetRValue(backdrop)};
    for (size_t pixel = 0u; pixel + 3u < premultipliedBgra.size(); pixel += 4u)
    {
        const uint32_t alpha = composeOverBackdrop ? premultipliedBgra[pixel + 3u] : 255u;
        for (size_t channel = 0u; channel < 3u; ++channel)
        {
            const uint32_t source             = premultipliedBgra[pixel + channel];
            const uint32_t composed           = source + (((255u - alpha) * backdropBgr[channel]) + 127u) / 255u;
            image.bgraPixels[pixel + channel] = static_cast<uint8_t>(composed > 255u ? 255u : composed);
        }
        image.bgraPixels[pixel + 3u] = 255u;
    }
    return image;
}
} // namespace DxUi::TestSupport
