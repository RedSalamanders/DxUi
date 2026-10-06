#pragma once

#include "Controls.Tests.DxUiTestHelpers.h"

#include <cstring>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Moves a control from one window host to another, whose dpi, theme, density and Direct2D device may differ, and compares
// it with a fresh control made the same way in the second host: what a host shows a control must not stay behind from
// the previous host. Shared by the suites that test a control across hosts (editor controls, the multiline Grid).

namespace DxUiTestMoved
{
using namespace DxUi;

[[nodiscard]] inline bool RectNear(const D2D1_RECT_F& actual, const D2D1_RECT_F& expected) noexcept
{
    constexpr float epsilon = 0.01f;
    return std::fabs(actual.left - expected.left) < epsilon && std::fabs(actual.top - expected.top) < epsilon &&
           std::fabs(actual.right - expected.right) < epsilon && std::fabs(actual.bottom - expected.bottom) < epsilon;
}

using NamedRects = std::vector<std::pair<std::string, D2D1_RECT_F>>;

// Empty when both are laid out alike; otherwise names every rectangle that differs.
[[nodiscard]] inline std::string DescribeGeometryDifference(const NamedRects& actual, const NamedRects& expected)
{
    std::string difference;
    if (actual.size() != expected.size())
    {
        return std::format("{} rectangles instead of {}; ", actual.size(), expected.size());
    }
    for (size_t index = 0u; index < actual.size(); ++index)
    {
        const D2D1_RECT_F& a = actual[index].second;
        const D2D1_RECT_F& e = expected[index].second;
        if (! RectNear(a, e))
        {
            difference += std::format("{} ({:.1f},{:.1f},{:.1f},{:.1f}) instead of ({:.1f},{:.1f},{:.1f},{:.1f}); ",
                                      actual[index].first,
                                      a.left,
                                      a.top,
                                      a.right,
                                      a.bottom,
                                      e.left,
                                      e.top,
                                      e.right,
                                      e.bottom);
        }
    }
    return difference;
}

// Empty when the captures are pixel-identical; otherwise the count and the box of the differing pixels.
[[nodiscard]] inline std::string DescribeBitmapDifference(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected)
{
    if (actual.widthPx != expected.widthPx || actual.heightPx != expected.heightPx || actual.bgraPixels.size() != expected.bgraPixels.size())
    {
        return std::format("{}x{} px instead of {}x{}", actual.widthPx, actual.heightPx, expected.widthPx, expected.heightPx);
    }
    size_t differing = 0u;
    UINT left        = actual.widthPx;
    UINT top         = actual.heightPx;
    UINT right       = 0u;
    UINT bottom      = 0u;
    for (UINT y = 0u; y < actual.heightPx; ++y)
    {
        for (UINT x = 0u; x < actual.widthPx; ++x)
        {
            const size_t offset = (static_cast<size_t>(y) * actual.widthPx + x) * 4u;
            if (std::memcmp(&actual.bgraPixels[offset], &expected.bgraPixels[offset], 4u) != 0)
            {
                ++differing;
                left   = (std::min)(left, x);
                top    = (std::min)(top, y);
                right  = (std::max)(right, x);
                bottom = (std::max)(bottom, y);
            }
        }
    }
    return differing == 0u ? std::string{} : std::format("{} pixels differ inside ({},{})-({},{})", differing, left, top, right, bottom);
}

[[nodiscard]] inline size_t CountPixelsDifferingFromCorner(const WindowHostBitmapCapture& capture) noexcept
{
    size_t count = 0u;
    for (size_t offset = 4u; offset + 3u < capture.bgraPixels.size(); offset += 4u)
    {
        count += std::memcmp(&capture.bgraPixels[offset], capture.bgraPixels.data(), 4u) != 0 ? 1u : 0u;
    }
    return count;
}

inline WindowHostBitmapCapture CaptureWindow(AttachedHostWindow& window, const char* context)
{
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();
    WindowHostBitmapCapture capture;
    Require(window.Host().DebugCaptureBitmap(capture), context);
    return capture;
}

// A host with a theme, a density and a dpi the system did not choose, whose client area is exactly `sizeDip`.
inline void ConfigureHostPlace(AttachedHostWindow& window, UINT dpi, bool dark, Density density, D2D1_SIZE_F sizeDip)
{
    ThemePalette theme  = MakeDefaultThemePalette(dark);
    theme.reducedMotion = true;
    theme.density       = density;
    window.Host().SetTheme(theme);
    RECT outer{};
    GetWindowRect(window.Hwnd(), &outer);
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_DPICHANGED, MAKELONG(dpi, dpi), reinterpret_cast<LPARAM>(&outer), handled));
    Require(handled && window.Host().GetDpi() == static_cast<float>(dpi), "the host takes the requested dpi");
    RECT client{};
    GetWindowRect(window.Hwnd(), &outer);
    GetClientRect(window.Hwnd(), &client);
    const float scale = static_cast<float>(dpi) / 96.0f;
    SetWindowPos(window.Hwnd(),
                 nullptr,
                 0,
                 0,
                 (outer.right - outer.left) - (client.right - client.left) + static_cast<LONG>(std::lround(sizeDip.width * scale)),
                 (outer.bottom - outer.top) - (client.bottom - client.top) + static_cast<LONG>(std::lround(sizeDip.height * scale)),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    window.PumpMessages();
    const D2D1_RECT_F clientDip = window.Host().GetClientBoundsDip();
    Require(std::fabs(clientDip.right - sizeDip.width) < 0.01f && std::fabs(clientDip.bottom - sizeDip.height) < 0.01f,
            "the host's client area is the requested size");
}

[[nodiscard]] inline std::unique_ptr<Control> TakeChild(Panel& parent, size_t index)
{
    return std::move(parent.GetChildren()[index]);
}

[[nodiscard]] inline wil::com_ptr<ID2D1Device> DirectDeviceOf(ControlHost& host)
{
    wil::com_ptr<ID2D1Device> device;
    if (ID2D1DeviceContext* const context = host.GetDeviceContext())
    {
        context->GetDevice(device.put());
    }
    return device;
}

struct MovePlace
{
    UINT dpi;
    bool dark;
    Density density;
};

struct MoveScenario
{
    const char* name;
    bool oldParentRightToLeft;
    bool newDevice; // The new host draws on a Direct2D device made after the control painted for the old one.
    MovePlace from;
    MovePlace to;
};

// Moves a control made by `configure` from one window host to a second, whose client area is the control's size (so
// setting the root changes no bounds: only the control itself can notice the move), and compares its arrangement
// (`measure`) and the painted window with a fresh control made the same way in the second host.
template <typename TControl, typename Configure, typename Measure>
void ExpectMovedControlMatchesAFreshOne(const char* control, D2D1_SIZE_F size, std::span<const MoveScenario> scenarios, Configure configure, Measure measure)
{
    for (const MoveScenario& scenario : scenarios)
    {
        AttachedHostWindow oldWindow;
        AttachedHostWindow newWindow;
        ConfigureHostPlace(oldWindow, scenario.from.dpi, scenario.from.dark, scenario.from.density, size);
        ConfigureHostPlace(newWindow, scenario.to.dpi, scenario.to.dark, scenario.to.density, size);

        auto oldRoot = std::make_unique<Panel>();
        if (scenario.oldParentRightToLeft)
        {
            oldRoot->SetFlowDirection(FlowDirection::RightToLeft);
        }
        Panel* const oldPanel = oldRoot.get();
        auto* moved           = oldRoot->AddChild<TControl>();
        configure(*moved);
        moved->SetBounds(newWindow.Host().GetClientBoundsDip());
        oldWindow.Host().SetRoot(std::move(oldRoot));
        static_cast<void>(CaptureWindow(oldWindow, "the control paints in its first host"));
        const wil::com_ptr<ID2D1Device> oldDevice = DirectDeviceOf(oldWindow.Host());
        Require(oldDevice != nullptr, "the first host draws on a Direct2D device");
        if (scenario.newDevice)
        {
            // Hidden, the first host stays on its device while the loss makes the next host build another.
            ShowWindow(oldWindow.Hwnd(), SW_HIDE);
            newWindow.Host().DebugSimulateDeviceLoss();
        }

        newWindow.Host().SetRoot(TakeChild(*oldPanel, 0u));
        Require(newWindow.Host().GetRoot() == moved, "the second host holds the moved control");
        const NamedRects movedRects              = measure(*moved, newWindow.Host());
        const WindowHostBitmapCapture movedImage = CaptureWindow(newWindow, "the moved control paints in its new host");
        Require(CountPixelsDifferingFromCorner(movedImage) > 200u, "the moved control painted something");
        Require(! scenario.newDevice || DirectDeviceOf(newWindow.Host()) != oldDevice, "the second host draws on another Direct2D device");

        auto created = std::make_unique<TControl>();
        configure(*created);
        TControl* const fresh = created.get();
        newWindow.Host().SetRoot(std::move(created));
        const NamedRects freshRects              = measure(*fresh, newWindow.Host());
        const WindowHostBitmapCapture freshImage = CaptureWindow(newWindow, "a fresh control paints in the new host");

        const std::string geometryDifference = DescribeGeometryDifference(movedRects, freshRects);
        const std::string imageDifference    = DescribeBitmapDifference(movedImage, freshImage);
        if (! geometryDifference.empty() || ! imageDifference.empty())
        {
            std::cerr << "moved " << control << ", " << scenario.name << ": rectangles: " << (geometryDifference.empty() ? "same" : geometryDifference)
                      << " | pixels: " << (imageDifference.empty() ? "same" : imageDifference) << '\n';
        }
        Require(geometryDifference.empty(), "a moved control arranges itself like a fresh one");
        Require(imageDifference.empty(), "a moved control paints like a fresh one");
    }
}

inline constexpr MovePlace kBasePlace{96u, false, Density::Standard};
inline constexpr MoveScenario kEveryPlace[] = {
    {"same metrics", false, false, kBasePlace, kBasePlace},
    {"a larger dpi", false, false, kBasePlace, {192u, false, Density::Standard}},
    {"a smaller dpi", false, false, {144u, false, Density::Standard}, kBasePlace},
    {"another theme", false, false, kBasePlace, {96u, true, Density::Standard}},
    {"compact density", false, false, kBasePlace, {96u, false, Density::Compact}},
    {"standard density", false, false, {96u, false, Density::Compact}, kBasePlace},
    {"another Direct2D device", false, true, kBasePlace, kBasePlace},
    {"a right-to-left parent", true, false, kBasePlace, kBasePlace},
    {"a right-to-left parent, another device and every metric", true, true, {96u, false, Density::Compact}, {144u, true, Density::Standard}},
};
inline constexpr MoveScenario kRightToLeftPlaces[] = {
    {"same metrics", false, false, kBasePlace, kBasePlace},
    {"a right-to-left parent", true, false, kBasePlace, kBasePlace},
    {"a right-to-left parent, another device and every metric", true, true, {96u, false, Density::Compact}, {144u, true, Density::Standard}},
};
} // namespace DxUiTestMoved
