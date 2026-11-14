#pragma once
// Presented-frame truth (Win32 host contract, "Presented frame after partial presents"): a WindowHost presents a full frame,
// then a partial frame over rectangle A, then one over rectangle B, through Present1 with that rectangle. The image the
// compositor shows afterwards must equal a full render of the same state everywhere, outside B included, while the back
// buffer read before the last Present still holds an older frame outside B. Read through the non-activating presented-image
// capture of Support.Tests.PresentedWindowCapture.h on WARP and on the default hardware adapter, in both presentation modes.
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support.Tests.PresentedWindowCapture.h"

#include <d2d1_3.h>
#include <dxgi1_6.h>

#include <exception>
#include <sstream>
#include <thread>

namespace PresentedFrameTests
{
using DxUi::TestSupport::PresentedCaptureStatus;
using DxUi::TestSupport::PresentedWindowImage;

constexpr LONG kWidthPx  = 192;
constexpr LONG kHeightPx = 128;
// A and B overlap, so the image has A-only, B (B is painted over A), neither and an unpainted strip.
constexpr RECT kRectA{16, 16, 112, 80};
constexpr RECT kRectB{80, 48, 176, 112};
// Columns from here to the right edge are never painted: the window background in HwndSwapChain, transparent (the
// backdrop shows) in CompositionSwapChain.
constexpr LONG kPaintedRight = 184;

struct Rgb
{
    uint8_t r = 0u;
    uint8_t g = 0u;
    uint8_t b = 0u;
};

// Every region and step has its own color, so any stale pixel names the frame it came from.
constexpr Rgb kNeither{0x20, 0x40, 0x60};
constexpr Rgb kA0{0xC0, 0x10, 0x10};
constexpr Rgb kA1{0x10, 0xC0, 0x10};
constexpr Rgb kB0{0x10, 0x10, 0xC0};
constexpr Rgb kB2{0xE0, 0xE0, 0x10};
constexpr Rgb kB3{0x10, 0xE0, 0xE0};
constexpr Rgb kWindowBackground{0x30, 0x30, 0x30};
constexpr Rgb kBackdrop{0xF0, 0x80, 0x00};

struct Model
{
    bool aChanged = false; // Step 1 repaints A.
    bool bChanged = false; // Step 2 repaints B.
    bool bThird   = false; // The negative control repaints B once more.
};

[[nodiscard]] inline D2D1_COLOR_F ToColor(Rgb c) noexcept
{
    return D2D1::ColorF(static_cast<float>(c.r) / 255.0f, static_cast<float>(c.g) / 255.0f, static_cast<float>(c.b) / 255.0f, 1.0f);
}

[[nodiscard]] inline bool Contains(const RECT& rect, LONG x, LONG y) noexcept
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

// Paints the model with aliased, pixel-aligned rectangles at any DPI, so the expected image is exact.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class SolidRegionPainter final : public DxUi::Control
{
public:
    explicit SolidRegionPainter(const Model& model) noexcept : _model(&model)
    {
    }

    void Paint(DxUi::ControlHost& host) const override
    {
        ID2D1DeviceContext* const dc = host.GetDeviceContext();
        if (! dc)
            return;
        const float dipPerPx = 96.0f / host.GetDpi();
        const auto toDip     = [dipPerPx](const RECT& r) noexcept
        {
            return D2D1::RectF(static_cast<float>(r.left) * dipPerPx,
                               static_cast<float>(r.top) * dipPerPx,
                               static_cast<float>(r.right) * dipPerPx,
                               static_cast<float>(r.bottom) * dipPerPx);
        };
        const D2D1_ANTIALIAS_MODE previous = dc->GetAntialiasMode();
        dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        const auto fill = [&](const RECT& r, Rgb color)
        {
            if (auto* brush = host.GetSolidBrush(ToColor(color)))
                dc->FillRectangle(toDip(r), brush);
        };
        fill(RECT{0, 0, kPaintedRight, kHeightPx}, kNeither);
        fill(kRectA, _model->aChanged ? kA1 : kA0);
        fill(kRectB, _model->bThird ? kB3 : (_model->bChanged ? kB2 : kB0));
        dc->SetAntialiasMode(previous);
    }

private:
    const Model* _model;
};
#pragma warning(pop)

[[nodiscard]] inline Rgb ExpectedAt(const Model& model, LONG x, LONG y, bool composition) noexcept
{
    if (Contains(kRectB, x, y))
        return model.bThird ? kB3 : (model.bChanged ? kB2 : kB0);
    if (Contains(kRectA, x, y))
        return model.aChanged ? kA1 : kA0;
    if (x < kPaintedRight)
        return kNeither;
    return composition ? kBackdrop : kWindowBackground;
}

[[nodiscard]] inline PresentedWindowImage ExpectedImage(const Model& model, bool composition)
{
    PresentedWindowImage image;
    image.widthPx  = kWidthPx;
    image.heightPx = kHeightPx;
    image.bgraPixels.resize(static_cast<size_t>(kWidthPx) * kHeightPx * 4u);
    for (LONG y = 0; y < kHeightPx; ++y)
        for (LONG x = 0; x < kWidthPx; ++x)
        {
            const Rgb c                  = ExpectedAt(model, x, y, composition);
            const size_t index           = (static_cast<size_t>(y) * kWidthPx + static_cast<size_t>(x)) * 4u;
            image.bgraPixels[index + 0u] = c.b;
            image.bgraPixels[index + 1u] = c.g;
            image.bgraPixels[index + 2u] = c.r;
            image.bgraPixels[index + 3u] = 255u;
        }
    return image;
}

[[nodiscard]] inline bool PixelIs(const PresentedWindowImage& image, LONG x, LONG y, Rgb c) noexcept
{
    if (image.widthPx != static_cast<UINT>(kWidthPx) || image.heightPx != static_cast<UINT>(kHeightPx))
        return false;
    const size_t index = (static_cast<size_t>(y) * kWidthPx + static_cast<size_t>(x)) * 4u;
    return image.bgraPixels[index + 0u] == c.b && image.bgraPixels[index + 1u] == c.g && image.bgraPixels[index + 2u] == c.r;
}

struct Difference
{
    size_t total        = 0u;
    size_t outsideB     = 0u;
    size_t aOnly        = 0u;
    size_t neither      = 0u;
    size_t unpainted    = 0u;
    LONG firstX         = -1;
    LONG firstY         = -1;
    uint32_t firstGot   = 0u;
    uint32_t firstWas   = 0u;
    bool sizeMismatch   = false;
    UINT actualWidth    = 0u;
    UINT actualHeight   = 0u;
    UINT expectedWidth  = 0u;
    UINT expectedHeight = 0u;
};

// Exact BGRA comparison of two opaque images, counted per region.
[[nodiscard]] inline Difference Compare(const PresentedWindowImage& actual, const PresentedWindowImage& expected) noexcept
{
    Difference d{};
    d.actualWidth    = actual.widthPx;
    d.actualHeight   = actual.heightPx;
    d.expectedWidth  = expected.widthPx;
    d.expectedHeight = expected.heightPx;
    if (actual.widthPx != expected.widthPx || actual.heightPx != expected.heightPx || actual.bgraPixels.size() != expected.bgraPixels.size())
    {
        d.sizeMismatch = true;
        return d;
    }
    for (LONG y = 0; y < static_cast<LONG>(actual.heightPx); ++y)
        for (LONG x = 0; x < static_cast<LONG>(actual.widthPx); ++x)
        {
            const size_t index = (static_cast<size_t>(y) * actual.widthPx + static_cast<size_t>(x)) * 4u;
            if (std::memcmp(&actual.bgraPixels[index], &expected.bgraPixels[index], 4u) == 0)
                continue;
            ++d.total;
            if (! Contains(kRectB, x, y))
            {
                ++d.outsideB;
                if (Contains(kRectA, x, y))
                    ++d.aOnly;
                else if (x < kPaintedRight)
                    ++d.neither;
                else
                    ++d.unpainted;
            }
            if (d.firstX < 0)
            {
                d.firstX = x;
                d.firstY = y;
                std::memcpy(&d.firstGot, &actual.bgraPixels[index], 4u);
                std::memcpy(&d.firstWas, &expected.bgraPixels[index], 4u);
            }
        }
    return d;
}

[[nodiscard]] inline std::string Describe(const Difference& d)
{
    if (d.sizeMismatch)
        return std::format("size mismatch: {}x{} against {}x{}", d.actualWidth, d.actualHeight, d.expectedWidth, d.expectedHeight);
    return std::format("{} differing pixels ({} outside B: A-only {}, neither {}, unpainted {}); first at ({}, {}) BGRA 0x{:08X} expected 0x{:08X}",
                       d.total,
                       d.outsideB,
                       d.aOnly,
                       d.neither,
                       d.unpainted,
                       d.firstX,
                       d.firstY,
                       d.firstGot,
                       d.firstWas);
}

struct AdapterIdentity
{
    std::string description;
    UINT vendorId = 0u;
    UINT deviceId = 0u;
    bool software = false;
    std::string driverVersion;
};

[[nodiscard]] inline AdapterIdentity ReadAdapterIdentity(DxUi::ControlHost& host)
{
    AdapterIdentity identity;
    ID2D1DeviceContext* const context = host.GetDeviceContext();
    Require(context != nullptr, "presented-frame host has a device context");
    wil::com_ptr_nothrow<ID2D1Device> device;
    context->GetDevice(device.put());
    wil::com_ptr_nothrow<ID2D1Device2> device2;
    RequireSucceeded(device.query_to(device2.put()), "query the presented-frame Direct2D device");
    wil::com_ptr_nothrow<IDXGIDevice> dxgi;
    RequireSucceeded(device2->GetDxgiDevice(dxgi.put()), "read the presented-frame DXGI device");
    wil::com_ptr_nothrow<IDXGIAdapter> adapter;
    RequireSucceeded(dxgi->GetAdapter(adapter.put()), "read the presented-frame adapter");
    wil::com_ptr_nothrow<IDXGIAdapter1> adapter1;
    RequireSucceeded(adapter.query_to(adapter1.put()), "read the presented-frame adapter description");
    DXGI_ADAPTER_DESC1 description{};
    RequireSucceeded(adapter1->GetDesc1(&description), "read the presented-frame adapter identity");
    for (const wchar_t ch : std::wstring_view(description.Description))
        identity.description.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
    identity.vendorId = description.VendorId;
    identity.deviceId = description.DeviceId;
    // The Microsoft Basic Render Driver (vendor 0x1414) is software whether or not it reports the flag.
    identity.software = (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0u || description.VendorId == 0x1414u;
    LARGE_INTEGER umd{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd)))
    {
        const auto part        = [&](int shift) { return static_cast<unsigned>((static_cast<uint64_t>(umd.QuadPart) >> shift) & 0xFFFFu); };
        identity.driverVersion = std::format("{}.{}.{}.{}", part(48), part(32), part(16), part(0));
    }
    return identity;
}

[[nodiscard]] inline const char* ConfigurationName() noexcept
{
#if defined(__SANITIZE_ADDRESS__)
    return "ASan-Debug";
#elif defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

[[nodiscard]] inline const char* PlatformName() noexcept
{
#if defined(_M_ARM64)
    return "ARM64";
#else
    return "x64";
#endif
}

inline void SaveImage(const std::filesystem::path& directory, const std::string& name, UINT width, UINT height, const std::vector<uint8_t>& bgra)
{
    DxUi::WindowHostBitmapCapture capture;
    capture.widthPx    = width;
    capture.heightPx   = height;
    capture.bgraPixels = bgra;
    std::wstring wideName(name.begin(), name.end());
    static_cast<void>(SaveWindowHostBitmapCaptureAsPngForTest(directory / (wideName + L".png"), capture));
}

enum class Outcome : uint8_t
{
    Passed,
    NoHardwareAdapter,
    NoInteractiveDesktop,
};

// One adapter, one presentation mode: full frame, partial A, partial B, then the presented image after B against a full
// render of the same state. Returns the capability that was missing, or fails through Require.
inline Outcome RunPresentedFrameCase(bool forceWarp, DxUi::WindowHost::PresentationMode mode)
{
    using DxUi::TestSupport::ComposeOverBackdrop;
    using DxUi::TestSupport::WaitForPresentedFrame;
    const bool composition        = mode == DxUi::WindowHost::PresentationMode::CompositionSwapChain;
    const char* const modeName    = composition ? "CompositionSwapChain" : "HwndSwapChain";
    const char* const adapterName = forceWarp ? "warp" : "hardware";

    if (! DxUi::TestSupport::HasInteractiveWindowStation())
        return Outcome::NoInteractiveDesktop;

    // Device pixels: the compositor shows (and the capture reads) the swap chain unscaled. The fixture thread keeps the
    // window's awareness throughout, so no coordinate it exchanges with the window is virtualized.
    const DxUi::TestSupport::ScopedPerMonitorV2Awareness awareness;
    Require(awareness.Applied(), "presented-frame window is created per-monitor DPI aware");
    Model model;
    auto window            = std::make_unique<AttachedHostWindow>(mode, forceWarp);
    DxUi::WindowHost& host = window->Host();
    auto theme             = host.GetTheme();
    theme.reducedMotion    = true;
    theme.windowBackground = ToColor(kWindowBackground);
    host.SetTheme(theme);
    auto root = std::make_unique<SolidRegionPainter>(model);
    host.SetRoot(std::move(root));
    Require(DxUi::TestSupport::PlacePresentedCaptureWindow(window->Hwnd(), SIZE{kWidthPx, kHeightPx}),
            "presented-frame window shows without activation, off-screen, with an exact client size");
    window->PumpMessages();

    // Step 0: a full frame, read before Present and as presented.
    DxUi::WindowHostBitmapCapture reference0;
    Require(host.DebugCaptureBitmap(reference0), "full frame renders and presents");
    const AdapterIdentity adapter = ReadAdapterIdentity(host);
    std::cerr << std::format("    [PRESENTED] adapter={} mode={} description=\"{}\" vendor=0x{:04X} device=0x{:04X} software={} driver={} dpi={} "
                             "configuration={}-{}\n",
                             adapterName,
                             modeName,
                             adapter.description,
                             adapter.vendorId,
                             adapter.deviceId,
                             adapter.software ? 1 : 0,
                             adapter.driverVersion,
                             host.GetDpi(),
                             PlatformName(),
                             ConfigurationName())
              << std::flush;
    if (forceWarp)
        Require(adapter.software, "the WARP case renders on a software adapter");
    else if (adapter.software)
        return Outcome::NoHardwareAdapter;

    const auto expected0 = ExpectedImage(model, composition);
    const auto composed  = [&](const DxUi::WindowHostBitmapCapture& buffer)
    { return ComposeOverBackdrop(buffer.widthPx, buffer.heightPx, buffer.bgraPixels, composition, RGB(kBackdrop.r, kBackdrop.g, kBackdrop.b)); };
    const auto requireEqual = [&](const PresentedWindowImage& actual, const PresentedWindowImage& expected, const char* what)
    {
        const Difference d = Compare(actual, expected);
        if (d.total != 0u || d.sizeMismatch)
        {
            std::cerr << "    [PRESENTED] " << adapterName << ' ' << modeName << ": " << what << ": " << Describe(d) << '\n';
            Require(false, what);
        }
    };
    requireEqual(composed(reference0), expected0, "the full render paints the model exactly");

    const std::filesystem::path evidence = FindRepoRootForDxUiTests() / L".build" / L"test-artifacts" / L"presented-frame" /
                                           std::filesystem::path(std::format("{}-{}", PlatformName(), ConfigurationName()));
    const std::string prefix             = std::format("{}-{}", adapterName, composition ? "composition" : "hwnd");
    const auto capture                   = [&](const char* what, const std::function<bool(const PresentedWindowImage&)>& shown)
    {
        PresentedWindowImage image;
        const uint64_t rendersBeforeCapture = host.DebugGetRenderCount();
        const PresentedCaptureStatus status = WaitForPresentedFrame(window->Hwnd(), shown, image, RGB(kBackdrop.r, kBackdrop.g, kBackdrop.b));
        // PrintWindow's WM_PAINT never reaches the host, so reading never renders or presents a frame of its own.
        Require(host.DebugGetRenderCount() == rendersBeforeCapture, "capturing the presented image never renders the window");
        if (status != PresentedCaptureStatus::Captured)
            std::cerr << "    [PRESENTED] " << adapterName << ' ' << modeName << ": " << what << ": "
                      << DxUi::TestSupport::DescribePresentedCaptureStatus(status) << '\n';
        Require(status == PresentedCaptureStatus::Captured, what);
        return image;
    };
    const LONG aOnlyX = kRectA.left + 4;
    const LONG aOnlyY = kRectA.top + 4;
    const LONG bX     = kRectB.right - 4;
    const LONG bY     = kRectB.bottom - 4;

    // Let every queued paint (show, theme, root) render now, so nothing but the frames below renders from here on.
    window->PumpMessages();
    Require(GetUpdateRect(window->Hwnd(), nullptr, FALSE) == FALSE, "no paint is pending before the presented-frame sequence");
    const uint64_t rendersBefore = host.DebugGetRenderCount();
    const PresentedWindowImage presented0 =
        capture("the full frame reaches the compositor", [&](const PresentedWindowImage& image) { return PixelIs(image, aOnlyX, aOnlyY, kA0); });
    // Calibration: the capture of a full present reproduces the full render exactly.
    requireEqual(presented0, expected0, "the presented full frame equals the full render (capture calibration)");

    // Step 1: A changes; render and present only A.
    model.aChanged           = true;
    const auto partialBefore = host.DebugGetPartialPresentAttemptCount();
    host.DebugRenderDirtyRectForTest(kRectA);
    Require(host.DebugGetPartialPresentAttemptCount() == partialBefore + 1u && host.DebugGetLastPresentResult() == S_OK,
            "frame A is a successful partial Present1");
    const PresentedWindowImage presented1 =
        capture("frame A reaches the compositor", [&](const PresentedWindowImage& image) { return PixelIs(image, aOnlyX, aOnlyY, kA1); });
    requireEqual(presented1, ExpectedImage(model, composition), "the presented image after A is current everywhere");

    // Step 2: B changes; render and present only B, reading the back buffer before Present.
    model.bChanged = true;
    DxUi::WindowHostBitmapCapture prePresentB;
    Require(host.DebugRenderDirtyRectForTest(kRectB, prePresentB), "frame B renders with a pre-present back-buffer read");
    Require(host.DebugGetPartialPresentAttemptCount() == partialBefore + 2u && host.DebugGetLastPresentResult() == S_OK,
            "frame B is a successful partial Present1");
    const PresentedWindowImage presented2 =
        capture("frame B reaches the compositor", [&](const PresentedWindowImage& image) { return PixelIs(image, bX, bY, kB2); });
    if (host.DebugGetRenderCount() != rendersBefore + 2u)
        std::cerr << "    [PRESENTED] renders since the full frame: " << (host.DebugGetRenderCount() - rendersBefore) << '\n';
    Require(host.DebugGetRenderCount() == rendersBefore + 2u, "exactly the two partial frames rendered after the full frame");

    // The full render of the same state, then the gate: 0 differences, outside B included.
    const auto expected2 = ExpectedImage(model, composition);
    DxUi::WindowHostBitmapCapture reference2;
    Require(host.DebugCaptureBitmap(reference2), "full render of the final state");
    const PresentedWindowImage full2 = composed(reference2);
    requireEqual(full2, expected2, "the full render of the final state paints the model exactly");

    SaveImage(evidence, prefix + "-presented-0-full", presented0.widthPx, presented0.heightPx, presented0.bgraPixels);
    SaveImage(evidence, prefix + "-presented-1-after-a", presented1.widthPx, presented1.heightPx, presented1.bgraPixels);
    SaveImage(evidence, prefix + "-presented-2-after-b", presented2.widthPx, presented2.heightPx, presented2.bgraPixels);
    SaveImage(evidence, prefix + "-backbuffer-before-present-b", prePresentB.widthPx, prePresentB.heightPx, composed(prePresentB).bgraPixels);
    SaveImage(evidence, prefix + "-full-render-final", full2.widthPx, full2.heightPx, full2.bgraPixels);

    const Difference presentedDifference  = Compare(presented2, full2);
    const Difference backBufferDifference = Compare(composed(prePresentB), full2);
    std::cerr << std::format("    [PRESENTED] adapter={} mode={} presentedVsFullRender={} backBufferBeforePresentVsFullRender={}\n",
                             adapterName,
                             modeName,
                             Describe(presentedDifference),
                             Describe(backBufferDifference))
              << std::flush;
    requireEqual(presented2, full2, "the presented image after B equals a full render of the same state (0 differences)");

    // A property of CaptureCurrentBackBuffer: before Present the back buffer is current inside B only. Its A-only pixels
    // hold frame 0 (A0), two frames old: Present1 copies the previous frame's updated area into the buffer it presents.
    Require(backBufferDifference.total == backBufferDifference.outsideB && backBufferDifference.aOnly != 0u,
            "the back buffer read before Present is current inside B and older outside it");
    Require(PixelIs(composed(prePresentB), aOnlyX, aOnlyY, kA0), "the pre-present back buffer shows A from the full frame, not frame A");

    // Negative control: the capture shows what was presented, not the model. A frame whose dirty rectangle (B) misses a
    // change (A returns to A0) leaves the presented A-only pixels at A1, and the comparison reports exactly those pixels.
    model.aChanged = false;
    model.bThird   = true;
    host.DebugRenderDirtyRectForTest(kRectB);
    Require(host.DebugGetLastPresentResult() == S_OK, "the negative-control frame presents");
    const PresentedWindowImage presented3 =
        capture("the negative-control frame reaches the compositor", [&](const PresentedWindowImage& image) { return PixelIs(image, bX, bY, kB3); });
    const Difference missed = Compare(presented3, ExpectedImage(model, composition));
    std::cerr << std::format("    [PRESENTED] adapter={} mode={} negativeControl={}\n", adapterName, modeName, Describe(missed)) << std::flush;
    constexpr size_t kAOnlyArea =
        static_cast<size_t>((kRectA.right - kRectA.left) * (kRectA.bottom - kRectA.top) - (kRectA.right - kRectB.left) * (kRectA.bottom - kRectB.top));
    Require(missed.total == kAOnlyArea && missed.aOnly == kAOnlyArea && PixelIs(presented3, aOnlyX, aOnlyY, kA1),
            "a change outside the presented dirty rectangle stays visible as a difference (the capture can see staleness)");
    SaveImage(evidence, prefix + "-presented-3-negative-control", presented3.widthPx, presented3.heightPx, presented3.bgraPixels);
    return Outcome::Passed;
}

inline void RunPresentedFrameCases(bool forceWarp)
{
    for (const auto mode : {DxUi::WindowHost::PresentationMode::HwndSwapChain, DxUi::WindowHost::PresentationMode::CompositionSwapChain})
    {
        // Each case attaches on a fresh thread: the owner thread's native graphics pool then starts empty, so the explicit
        // WARP request (or the default hardware policy) is never refused or inherited from an earlier test's state.
        Outcome outcome             = Outcome::Passed;
        uint64_t blockedActivations = 0u;
        uint64_t blockedFocus       = 0u;
        std::exception_ptr failure;
        std::jthread fixture([&]
        {
            // Runner assertions cross this thread-entry boundary; relay named failures after joining.
            try
            {
                const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
                DxUi::TestSupport::ScopedWindowActivationBlocker blocker;
                Require(blocker.Start(), "the presented-frame fixture cannot activate or focus desktop windows");
                outcome            = RunPresentedFrameCase(forceWarp, mode);
                blockedActivations = blocker.BlockedActivationCount();
                blockedFocus       = blocker.BlockedFocusCount();
            }
            catch (const std::exception&)
            {
                failure = std::current_exception(); // Test-only cross-thread result.
            }
        });
        fixture.join();
        if (failure)
            std::rethrow_exception(failure);
        if (outcome == Outcome::NoInteractiveDesktop)
        {
            SkipDxUiTest("presented-frame capture needs an interactive desktop compositor");
            return;
        }
        if (outcome == Outcome::NoHardwareAdapter)
        {
            SkipDxUiTest("presented-frame hardware case needs a hardware Direct3D adapter");
            return;
        }
        Require(blockedActivations == 0u && blockedFocus == 0u, "the presented-frame fixture never tried to activate or focus");
    }
}
} // namespace PresentedFrameTests

inline void TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnWarp()
{
    PresentedFrameTests::RunPresentedFrameCases(true);
}

inline void TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnHardware()
{
    PresentedFrameTests::RunPresentedFrameCases(false);
}
