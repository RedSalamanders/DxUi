#pragma once
// Plan item W2.4, popup surfaces (Specs/Core/Core_PerformanceAndResources.md, "Popup surfaces"), on the embedded host. Included
// once from Embedded.Tests.Embedded.cpp after the performance fixtures; uses Check and Hr from there. No window, no focus.
//
// References: the code before W2.4, kept callable (Detail::SetPopupSurfacesUncachedForTest: every object made for each paint),
// and, for a blurred popup, a frame drawn from a freshly made retained surface. A Solid popup draws exactly the old pixels; a
// blurred popup keeps its blur as a bitmap (developer's decision of 25 November 2026) and its shadow from a bitmap, so it may
// differ from the old pixels by at most one level (the counts are printed for the receipt), and its whole and region frames
// are identical. Every other comparison is bit for bit (WARP).
// - TestPopupSurfaceMatchesUncached: a probe paints the popup surface with the menu's material, every overlay material, dark
//   and light, 96/144/192 DPI, over busy content (the ComboBox case) and over a cleared surface (a menu popup window).
// - TestPopupSurfaceReleases: every input a retained object was made from, changed, draws what a fresh surface draws; a new
//   capture remakes the blurred bitmap (the rule a menu popup's move or resize follows: RefreshMenuPopupBackdrop captures again).
// - TestComboBoxPopupSurface: a real ComboBox popup, every material and DPI; its registry row counts and drops the surface.
// - TestPopupBackdropDoesNotFollowContent: content changed behind an open ComboBox popup captures nothing, blurs nothing and
//   remakes nothing: the backdrop is the content captured when the popup opened, until W5.12.
// - TestTransientSurfaceBackdropRetained: PaintTransientSurface keeps its clip and blurred capture.
// - TestFx2PopupSurfaceGate: FX2's steady popup frames under Solid and Acrylic.
// - TestPopupBlurRegionFramesEqualWholeFrames: Mica and Acrylic at 100 % and 150 %, a region frame over a blurred popup equals
//   the whole frame, with the paint-region verifier on (its reference paint draws from what was kept).
// - TestPerPaintBlurRegionEvidence: what the per-paint blur does under a paint region, recorded (the reason for the rule), and
//   what a Solid popup's shadow passes over content do under one.
#include "Controls/DxUi.Internal.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "Embedded.Tests.PerformanceFixtures.h"
#include "Support.Tests.FrameVerifier.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

#include <array>
#include <cstring>
#include <format>
#include <string>

namespace PopupSurfaceTests
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;

inline constexpr std::array<DxUi::OverlayMaterial, 4u> kMaterials{
    DxUi::OverlayMaterial::Solid, DxUi::OverlayMaterial::Mica, DxUi::OverlayMaterial::MicaAlt, DxUi::OverlayMaterial::Acrylic};
inline constexpr std::array<UINT, 3u> kDpis{96u, 144u, 192u};
inline constexpr UINT kWidthDip  = 320u;
inline constexpr UINT kHeightDip = 260u;

[[nodiscard]] inline const char* MaterialName(DxUi::OverlayMaterial material) noexcept
{
    switch (material)
    {
        case DxUi::OverlayMaterial::Mica: return "Mica";
        case DxUi::OverlayMaterial::MicaAlt: return "MicaAlt";
        case DxUi::OverlayMaterial::Acrylic: return "Acrylic";
        case DxUi::OverlayMaterial::Solid:
        default: return "Solid";
    }
}

[[nodiscard]] inline DxUi::ThemePalette MakeTheme(DxUi::OverlayMaterial material, bool dark)
{
    DxUi::ThemePalette theme = DxUi::MakeDefaultThemePalette(dark);
    theme.reducedMotion      = true;
    theme.overlayMaterial    = material;
    return theme;
}

// While alive, popup surfaces paint as before W2.4.
class UncachedScope final
{
public:
    explicit UncachedScope(bool uncached) noexcept
    {
        DxUi::Detail::SetPopupSurfacesUncachedForTest(uncached);
    }
    ~UncachedScope()
    {
        DxUi::Detail::SetPopupSurfacesUncachedForTest(false);
    }
    UncachedScope(const UncachedScope&)            = delete;
    UncachedScope& operator=(const UncachedScope&) = delete;
    UncachedScope(UncachedScope&&)                 = delete;
    UncachedScope& operator=(UncachedScope&&)      = delete;
};

// Paints the popup surface as a ComboBox list or a menu popup does, over stripes or over the cleared surface, with a synthetic
// capture behind it. A new capture releases the blur made from the old one first, as both owners do.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // never copied
class PopupSurfaceProbe final : public DxUi::Control
{
public:
    DxUi::PopupMaterialStyle material{};
    D2D1_RECT_F rect                     = D2D1::RectF(40.0f, 32.0f, 280.0f, 232.0f);
    DxUi::PopupShadowTarget shadowTarget = DxUi::PopupShadowTarget::OverContent;
    uint32_t captureSeed                 = 1u;
    mutable DxUi::PopupSurfaceCache cache;

    void Paint(DxUi::ControlHost& host) const override
    {
        ID2D1DeviceContext* const dc = host.GetDeviceContext();
        if (! dc)
            return;
        if (shadowTarget == DxUi::PopupShadowTarget::OverContent)
        {
            for (int stripe = 0; stripe < 20; ++stripe)
            {
                const float s = static_cast<float>(stripe);
                const float t = s / 20.0f;
                if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.1f + 0.8f * t, 0.9f - 0.7f * t, static_cast<float>(stripe % 3) * 0.4f, 1.0f)))
                    dc->FillRectangle(D2D1::RectF(16.0f * s, 0.0f, 16.0f * s + 16.0f, static_cast<float>(kHeightDip)), brush);
            }
        }
        ID2D1Bitmap1* const source = material.backdropOpacity > 0.0f && material.backdropBlurDip > 0.0f ? EnsureCapture(host, *dc) : nullptr;
        DxUi::PaintPopupMaterialSurface(host, cache, source, rect, DxUi::kPopupRoundSmallCornerRadiusDip, material, shadowTarget);
    }

private:
    [[nodiscard]] ID2D1Bitmap1* EnsureCapture(DxUi::ControlHost& host, ID2D1DeviceContext& dc) const
    {
        wil::com_ptr<ID2D1Device> device;
        dc.GetDevice(device.put());
        const float dpi = host.GetDpi();
        if (capture_ && captureDevice_.get() == device.get() && capturedSeed_ == captureSeed && capturedDpi_ == dpi)
            return capture_.get();
        capture_.reset();
        const UINT widthPx  = static_cast<UINT>(std::lround(host.DipsToPixels(rect.right - rect.left)));
        const UINT heightPx = static_cast<UINT>(std::lround(host.DipsToPixels(rect.bottom - rect.top)));
        std::vector<uint8_t> pixels(static_cast<size_t>(widthPx) * heightPx * 4u);
        for (UINT y = 0u; y < heightPx; ++y)
            for (UINT x = 0u; x < widthPx; ++x)
            {
                const size_t offset  = (static_cast<size_t>(y) * widthPx + x) * 4u;
                const bool alternate = ((x / (12u + captureSeed)) + (y / 9u)) % 2u != 0u;
                pixels[offset]       = static_cast<uint8_t>(alternate ? 0x20u + captureSeed * 40u : 0xD0u);
                pixels[offset + 1u]  = static_cast<uint8_t>(alternate ? 0xB0u : (x * 3u + captureSeed * 17u));
                pixels[offset + 2u]  = static_cast<uint8_t>(alternate ? 0xF0u : (y * 5u));
                pixels[offset + 3u]  = 0xFFu;
            }
        const D2D1_BITMAP_PROPERTIES1 properties =
            D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi);
        if (FAILED(dc.CreateBitmap(D2D1::SizeU(widthPx, heightPx), pixels.data(), widthPx * 4u, &properties, capture_.put())))
            return nullptr;
        captureDevice_ = std::move(device);
        capturedSeed_  = captureSeed;
        capturedDpi_   = dpi;
        return capture_.get();
    }

    mutable wil::com_ptr<ID2D1Bitmap1> capture_;
    mutable wil::com_ptr<ID2D1Device> captureDevice_;
    mutable uint32_t capturedSeed_ = 0u;
    mutable float capturedDpi_     = 0.0f;
};
#pragma warning(pop)

struct ProbeView final
{
    explicit ProbeView(ID3D11Device* device)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(device, graphics), "popup surface probe graphics");
        Hr(view.Attach(std::move(graphics), StoppedTestClock()), "popup surface probe view");
        auto root = std::make_unique<DxUi::Panel>();
        probe     = root->AddChild<PopupSurfaceProbe>();
        probe->SetBounds(D2D1::RectF(0.0f, 0.0f, static_cast<float>(kWidthDip), static_cast<float>(kHeightDip)));
        view.Controls().SetRoot(std::move(root));
    }
    ~ProbeView()
    {
        view.Detach();
    }
    ProbeView(const ProbeView&)            = delete;
    ProbeView& operator=(const ProbeView&) = delete;
    ProbeView(ProbeView&&)                 = delete;
    ProbeView& operator=(ProbeView&&)      = delete;

    // One whole frame at `dpi`, uncached (the oracle) or retained; its pixels, and its work counters in `work`.
    [[nodiscard]] std::vector<uint8_t> Frame(UINT dpi, bool uncached, WorkCounters* work = nullptr)
    {
        view.Controls().Invalidate();
        WorkCounterScope scope;
        {
            const UncachedScope seam(uncached);
            Hr(view.Prepare(static_cast<UINT>(MulDiv(static_cast<int>(kWidthDip), static_cast<int>(dpi), 96)),
                            static_cast<UINT>(MulDiv(static_cast<int>(kHeightDip), static_cast<int>(dpi), 96)),
                            static_cast<float>(dpi)),
               "popup surface probe frame");
        }
        if (work)
            *work = scope.Delta();
        DxUi::WindowHostBitmapCapture surface;
        Hr(view.DebugReadSurfaceForTest(surface), "popup surface probe surface");
        return std::move(surface.bgraPixels);
    }

    DxUi::EmbeddedHost view;
    PopupSurfaceProbe* probe = nullptr;
};

inline void RequireSame(const std::vector<uint8_t>& expected, const std::vector<uint8_t>& actual, const std::string& context)
{
    size_t differing = 0u;
    if (expected.size() == actual.size())
        for (size_t index = 0u; index < expected.size(); ++index)
            differing += expected[index] != actual[index] ? 1u : 0u;
    if (expected.size() != actual.size() || differing != 0u)
    {
        std::cerr << "  popup surface pixels differ (" << context << "): " << differing << " bytes\n";
        for (size_t index = 0u, shown = 0u; index + 3u < expected.size() && index + 3u < actual.size() && shown < 8u; index += 4u)
            if (std::memcmp(&expected[index], &actual[index], 4u) != 0)
            {
                ++shown;
                std::cerr << std::format("    pixel {}: expected {} {} {} {}, drawn {} {} {} {}\n",
                                         index / 4u,
                                         expected[index],
                                         expected[index + 1u],
                                         expected[index + 2u],
                                         expected[index + 3u],
                                         actual[index],
                                         actual[index + 1u],
                                         actual[index + 2u],
                                         actual[index + 3u]);
            }
    }
    Check(expected.size() == actual.size() && differing == 0u, "the popup surface draws exactly the reference pixels");
}

struct PixelDifference
{
    size_t pixels = 0u;
    int maximum   = 0;
    UINT left     = UINT_MAX;
    UINT top      = UINT_MAX;
    UINT right    = 0u;
    UINT bottom   = 0u;
};
[[nodiscard]] inline PixelDifference Difference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, UINT widthPx)
{
    PixelDifference difference;
    for (size_t index = 0u; index + 3u < a.size() && index + 3u < b.size(); index += 4u)
    {
        int largest = 0;
        for (size_t channel = 0u; channel < 4u; ++channel)
            largest = (std::max)(largest, std::abs(int(a[index + channel]) - int(b[index + channel])));
        if (largest == 0)
            continue;
        ++difference.pixels;
        difference.maximum = (std::max)(difference.maximum, largest);
        const UINT x       = static_cast<UINT>((index / 4u) % widthPx);
        const UINT y       = static_cast<UINT>((index / 4u) / widthPx);
        difference.left    = (std::min)(difference.left, x);
        difference.top     = (std::min)(difference.top, y);
        difference.right   = (std::max)(difference.right, x + 1u);
        difference.bottom  = (std::max)(difference.bottom, y + 1u);
    }
    return difference;
}
[[nodiscard]] inline std::string Describe(const PixelDifference& difference)
{
    if (difference.pixels == 0u)
        return "0 pixels";
    return std::format("{} pixels, up to {} level(s), in x [{},{}) y [{},{})",
                       difference.pixels,
                       difference.maximum,
                       difference.left,
                       difference.right,
                       difference.top,
                       difference.bottom);
}
// The accepted change of a blurred popup against the pixels before W2.4: at most one level, anywhere.
inline PixelDifference RequireWithinOneLevel(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after, UINT widthPx, const std::string& context)
{
    const PixelDifference difference = Difference(before, after, widthPx);
    if (difference.maximum > 1)
        std::cerr << "  a blurred popup moved more than one level from the pixels before W2.4 (" << context << "): " << Describe(difference) << '\n';
    Check(before.size() == after.size() && difference.maximum <= 1, "a blurred popup differs from the pixels before W2.4 by at most one level");
    return difference;
}

[[nodiscard]] inline bool IsBlurred(const DxUi::PopupMaterialStyle& material) noexcept
{
    return material.backdropOpacity > 0.0f && material.backdropBlurDip > 0.0f;
}

// A steady retained frame: nothing made, no blur; shadow passes only for an unblurred popup over content.
inline void RequireSteady(const WorkCounters& work, uint64_t shadowPasses, const std::string& context)
{
    const auto& r          = work.resources;
    const bool nothingMade = r.effect.created == 0u && r.commandList.created == 0u && r.layer.created == 0u && r.geometry.created == 0u &&
                             r.gradientStops.created == 0u && r.brush.created == 0u && r.bitmap.created == 0u && r.deviceContext.created == 0u;
    if (! nothingMade || work.popups.backdropBlurPasses != 0u || work.frame.shadowPasses != shadowPasses)
        std::cerr << std::format("  steady popup frame ({}): effects {}, command lists {}, layers {}, geometries {}, stops {}, brushes {}, bitmaps {}, "
                                 "blur passes {}, shadow passes {} (expected {})\n",
                                 context,
                                 r.effect.created,
                                 r.commandList.created,
                                 r.layer.created,
                                 r.geometry.created,
                                 r.gradientStops.created,
                                 r.brush.created,
                                 r.bitmap.created,
                                 work.popups.backdropBlurPasses,
                                 work.frame.shadowPasses,
                                 shadowPasses);
    Check(nothingMade && work.popups.backdropBlurPasses == 0u && work.frame.shadowPasses == shadowPasses,
          "a steady popup frame makes nothing and runs no blur (shadow passes only for an unblurred popup over content)");
}
[[nodiscard]] inline uint64_t SteadyShadowPasses(const DxUi::PopupMaterialStyle& material, DxUi::PopupShadowTarget target) noexcept
{
    return ! IsBlurred(material) && target == DxUi::PopupShadowTarget::OverContent ? 2u : 0u;
}

__declspec(noinline) inline void TestPopupSurfaceMatchesUncached(GraphicsFixture& gpu)
{
    size_t compared = 0u;
    for (const DxUi::PopupShadowTarget target : {DxUi::PopupShadowTarget::OverContent, DxUi::PopupShadowTarget::OverClearedSurface})
        for (const DxUi::OverlayMaterial material : kMaterials)
            for (const bool dark : {true, false})
                for (const UINT dpi : kDpis)
                {
                    ProbeView probe(gpu.device.get());
                    const DxUi::ThemePalette theme = MakeTheme(material, dark);
                    probe.view.Controls().SetTheme(theme);
                    probe.probe->material             = DxUi::ResolveMenuPopupMaterialForTest(theme);
                    probe.probe->shadowTarget         = target;
                    const bool blurred                = IsBlurred(probe.probe->material);
                    const UINT widthPx                = static_cast<UINT>(MulDiv(static_cast<int>(kWidthDip), static_cast<int>(dpi), 96));
                    const std::string context         = std::format("{} {} {} at {} DPI",
                                                                    target == DxUi::PopupShadowTarget::OverContent ? "over content" : "over a cleared surface",
                                                                    MaterialName(material),
                                                                    dark ? "dark" : "light",
                                                                    dpi);
                    const std::vector<uint8_t> before = probe.Frame(dpi, true);
                    const std::vector<uint8_t> first  = probe.Frame(dpi, false);
                    WorkCounters work{};
                    const std::vector<uint8_t> steady = probe.Frame(dpi, false, &work);
                    RequireSteady(work, SteadyShadowPasses(probe.probe->material, target), context);
                    if (blurred)
                    {
                        // The first frame draws from the surface it makes, as every later frame does.
                        RequireSame(first, steady, context + ", first against steady retained frame");
                        const PixelDifference change = RequireWithinOneLevel(before, steady, widthPx, context);
                        std::cout << std::format("  pixel change, {}: {}\n", context, Describe(change));
                    }
                    else
                    {
                        RequireSame(before, first, context + ", first retained frame");
                        RequireSame(before, steady, context + ", steady frame");
                    }
                    // The oracle neither used nor changed what is retained.
                    static_cast<void>(probe.Frame(dpi, true));
                    RequireSame(steady, probe.Frame(dpi, false, &work), context + ", after an uncached frame");
                    RequireSteady(work, SteadyShadowPasses(probe.probe->material, target), context + ", after an uncached frame");
                    compared += 3u;
                }
    std::cout << "PASS popup surface: " << compared
              << " comparisons (4 materials, dark and light, 96/144/192 DPI, over content and over a cleared surface): Solid exact, blurred within "
                 "one level\n";
}

__declspec(noinline) inline void TestPopupSurfaceReleases(GraphicsFixture& gpu)
{
    for (const DxUi::PopupShadowTarget target : {DxUi::PopupShadowTarget::OverContent, DxUi::PopupShadowTarget::OverClearedSurface})
    {
        const char* const where = target == DxUi::PopupShadowTarget::OverContent ? "over content" : "over a cleared surface";
        ProbeView probe(gpu.device.get());
        DxUi::ThemePalette theme = MakeTheme(DxUi::OverlayMaterial::Acrylic, true);
        probe.view.Controls().SetTheme(theme);
        probe.probe->material     = DxUi::ResolveMenuPopupMaterialForTest(theme);
        probe.probe->shadowTarget = target;
        UINT dpi                  = 144u;
        static_cast<void>(probe.Frame(dpi, false));
        // After a change: the next frame and a steady one draw what a freshly made surface draws (nothing stale is kept), and a
        // Solid popup draws the pixels before W2.4. Returns the counters of the frame after the change.
        const auto check = [&](const char* change) -> WorkCounters
        {
            WorkCounters changed{};
            const std::vector<uint8_t> next = probe.Frame(dpi, false, &changed);
            WorkCounters work{};
            const std::vector<uint8_t> steady = probe.Frame(dpi, false, &work);
            RequireSteady(work, SteadyShadowPasses(probe.probe->material, target), std::format("{}, after {}", where, change));
            RequireSame(next, steady, std::format("{}, steady after {}", where, change));
            probe.probe->cache.Release();
            RequireSame(probe.Frame(dpi, false), steady, std::format("{}, a fresh surface after {}", where, change));
            if (! IsBlurred(probe.probe->material))
                RequireSame(probe.Frame(dpi, true), steady, std::format("{}, the pixels before W2.4 after {}", where, change));
            static_cast<void>(probe.Frame(dpi, false));
            return changed;
        };
        probe.probe->rect = D2D1::RectF(40.0f, 32.0f, 300.0f, 250.0f);
        static_cast<void>(check("a size change"));
        probe.probe->rect = D2D1::RectF(20.0f, 8.0f, 280.0f, 226.0f);
        static_cast<void>(check("a move"));
        // A new capture (a popup opened, or a menu popup moved or resized: RefreshMenuPopupBackdrop captures again) remakes the
        // blurred bitmap from it.
        probe.probe->captureSeed      = 3u;
        const WorkCounters newCapture = check("a new capture");
        Check(newCapture.popups.backdropBlurPasses == 1u && newCapture.resources.bitmap.created == 1u,
              "a new capture remakes the blurred bitmap (one blur pass into a new bitmap)");
        theme = MakeTheme(DxUi::OverlayMaterial::Mica, false);
        probe.view.Controls().SetTheme(theme);
        probe.probe->material = DxUi::ResolveMenuPopupMaterialForTest(theme);
        static_cast<void>(check("a theme change"));
        theme = MakeTheme(DxUi::OverlayMaterial::Solid, false);
        probe.view.Controls().SetTheme(theme);
        probe.probe->material = DxUi::ResolveMenuPopupMaterialForTest(theme);
        static_cast<void>(check("a change to Solid"));
        theme = MakeTheme(DxUi::OverlayMaterial::MicaAlt, true);
        probe.view.Controls().SetTheme(theme);
        probe.probe->material = DxUi::ResolveMenuPopupMaterialForTest(theme);
        static_cast<void>(check("a change back to a blurred material"));
        dpi = 192u;
        static_cast<void>(check("a DPI change"));
        dpi = 96u;
        static_cast<void>(check("a second DPI change"));
        const ID2D1Device* const before = probe.probe->cache.device.get();
        std::shared_ptr<DxUi::GraphicsDevice> replacement;
        Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), replacement), "popup surface replacement graphics");
        Hr(probe.view.ReplaceDevice(replacement), "popup surface device replacement");
        static_cast<void>(check("a device replacement"));
        Check(probe.probe->cache.device.get() != before, "the popup surface keeps nothing of the replaced device");
        probe.probe->cache.Release();
        Check(probe.probe->cache.Entries() == 0u && probe.probe->cache.ResidentBytes() == 0u, "a released popup surface holds nothing");
        static_cast<void>(check("a release"));
    }
    std::cout << "PASS popup surface releases: size, move, new capture, theme, material, DPI, device replacement and release draw what a fresh "
                 "surface draws\n";
}

[[nodiscard]] inline size_t ComboRowEntries(DxUi::ControlHost& host, uint64_t* bytes = nullptr)
{
    std::vector<DxUi::RetainedCacheCount> counts;
    DxUi::RetainedCacheAccess::Count(host, counts);
    for (const DxUi::RetainedCacheCount& count : counts)
        if (count.name == "ComboBox._popupSurfaceCache")
        {
            if (bytes)
                *bytes = count.residentBytes;
            return count.entries;
        }
    Check(false, "the registry has the ComboBox popup surface row");
    return 0u;
}

// A view with a label under an editable ComboBox of 20 items.
struct ComboView final
{
    ComboView(ID3D11Device* device, DxUi::OverlayMaterial material, UINT dpi_) : dpi(dpi_)
    {
        std::shared_ptr<DxUi::GraphicsDevice> graphics;
        Hr(DxUi::GraphicsDevice::Create(device, graphics), "ComboBox popup surface graphics");
        Hr(view.Attach(std::move(graphics), StoppedTestClock()), "ComboBox popup surface view");
        view.Controls().SetTheme(MakeTheme(material, true));
        auto root = std::make_unique<DxUi::Panel>();
        label     = root->AddChild<DxUi::Label>(L"Content under the popup: the list is drawn over it");
        label->SetBounds(D2D1::RectF(16.0f, 60.0f, 336.0f, 200.0f));
        combo = root->AddChild<DxUi::ComboBox>();
        std::vector<DxUi::ComboBox::Item> items;
        for (int index = 0; index < 20; ++index)
            items.push_back({std::format(L"Item {}", index), std::format(L"Item {}", index)});
        combo->SetItems(std::move(items));
        combo->SetBounds(D2D1::RectF(16.0f, 16.0f, 336.0f, 48.0f));
        view.Controls().SetRoot(std::move(root));
    }
    ~ComboView()
    {
        view.Detach();
    }
    ComboView(const ComboView&)            = delete;
    ComboView& operator=(const ComboView&) = delete;
    ComboView(ComboView&&)                 = delete;
    ComboView& operator=(ComboView&&)      = delete;
    [[nodiscard]] UINT WidthPx() const noexcept
    {
        return static_cast<UINT>(MulDiv(400, static_cast<int>(dpi), 96));
    }
    std::vector<uint8_t> Frame(bool uncached, WorkCounters* work = nullptr)
    {
        view.Controls().Invalidate();
        WorkCounterScope scope;
        {
            const UncachedScope seam(uncached);
            Hr(view.Prepare(WidthPx(), static_cast<UINT>(MulDiv(360, static_cast<int>(dpi), 96)), static_cast<float>(dpi)), "ComboBox popup surface frame");
        }
        if (work)
            *work = scope.Delta();
        DxUi::WindowHostBitmapCapture surface;
        Hr(view.DebugReadSurfaceForTest(surface), "ComboBox popup surface read");
        return std::move(surface.bgraPixels);
    }

    UINT dpi = 96u;
    DxUi::EmbeddedHost view;
    DxUi::Label* label    = nullptr;
    DxUi::ComboBox* combo = nullptr;
};

__declspec(noinline) inline void TestComboBoxPopupSurface(GraphicsFixture& gpu)
{
    size_t compared = 0u;
    for (const DxUi::OverlayMaterial material : kMaterials)
        for (const UINT dpi : kDpis)
        {
            ComboView combo(gpu.device.get(), material, dpi);
            static_cast<void>(combo.Frame(false));
            static_cast<void>(combo.combo->OnKeyDown(combo.view.Controls(), VK_F4, 0u));
            Check(combo.combo->IsPopupOpen(), "F4 opens the ComboBox popup");
            const std::string context         = std::format("ComboBox {} at {} DPI", MaterialName(material), dpi);
            const std::vector<uint8_t> before = combo.Frame(true);
            const std::vector<uint8_t> first  = combo.Frame(false);
            WorkCounters work{};
            const std::vector<uint8_t> steady = combo.Frame(false, &work);
            // The embedded ComboBox captures the screen at open; a blurred list draws its shadow from its bitmap.
            RequireSteady(work, material == DxUi::OverlayMaterial::Solid ? 2u : 0u, context);
            if (material == DxUi::OverlayMaterial::Solid)
            {
                RequireSame(before, first, context + ", first retained frame");
                RequireSame(before, steady, context + ", steady frame");
            }
            else
            {
                RequireSame(first, steady, context + ", first against steady retained frame");
                std::cout << std::format("  pixel change, {}: {}\n", context, Describe(RequireWithinOneLevel(before, steady, combo.WidthPx(), context)));
            }
            Check(ComboRowEntries(combo.view.Controls()) != 0u, "the registry counts the open popup's retained surface");
            combo.view.Controls().DebugDropRetainedStateForTest();
            Check(ComboRowEntries(combo.view.Controls()) == 0u, "the registry drops the open popup's retained surface");
            RequireSame(steady, combo.Frame(false), context + ", after the registry drop");
            static_cast<void>(combo.combo->OnKeyDown(combo.view.Controls(), VK_ESCAPE, 0u));
            Check(! combo.combo->IsPopupOpen() && ComboRowEntries(combo.view.Controls()) == 0u, "closing the popup releases its retained surface");
            static_cast<void>(combo.Frame(false));
            compared += 3u;
        }
    std::cout << "PASS ComboBox popup surface: " << compared
              << " comparisons (4 materials, 96/144/192 DPI): Solid exact, blurred within one level; the registry counts and drops it; closing "
                 "releases it\n";
}

// The backdrop is the content captured when the popup opened: content that changes behind an open popup is not captured
// again, the blur is not made again and nothing is remade, until W5.12 replaces the capture.
__declspec(noinline) inline void TestPopupBackdropDoesNotFollowContent(GraphicsFixture& gpu)
{
    for (const DxUi::OverlayMaterial material : {DxUi::OverlayMaterial::Mica, DxUi::OverlayMaterial::Acrylic})
    {
        ComboView combo(gpu.device.get(), material, 96u);
        static_cast<void>(combo.Frame(false));
        static_cast<void>(combo.combo->OnKeyDown(combo.view.Controls(), VK_F4, 0u));
        static_cast<void>(combo.Frame(false));
        uint64_t bytesBefore       = 0u;
        const size_t entriesBefore = ComboRowEntries(combo.view.Controls(), &bytesBefore);
        combo.label->SetText(L"Different content behind the open popup, drawn after it opened");
        WorkCounters work{};
        static_cast<void>(combo.Frame(false, &work));
        uint64_t bytesAfter       = 0u;
        const size_t entriesAfter = ComboRowEntries(combo.view.Controls(), &bytesAfter);
        Check(work.popups.backdropCaptures == 0u && work.popups.backdropBlurPasses == 0u && work.resources.bitmap.created == 0u &&
                  work.resources.effect.created == 0u && entriesAfter == entriesBefore && bytesAfter == bytesBefore,
              "content changed behind an open popup is not captured or blurred again: the backdrop stays the content at open");
    }
    std::cout << "PASS popup backdrop: content changed behind an open popup leaves its backdrop as captured at open (no capture, blur or "
                 "bitmap made)\n";
}

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // never copied
class TransientProbe final : public DxUi::Control
{
public:
    void Paint(DxUi::ControlHost& host) const override
    {
        if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.2f, 0.5f, 0.3f, 1.0f)))
            host.GetDeviceContext()->FillRectangle(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f), brush);
        DxUi::PaintTransientSurface(host, D2D1::RectF(40.0f, 30.0f, 280.0f, 150.0f), DxUi::TransientSurfaceOptions{.backdrop = &backdrop});
    }
    mutable DxUi::TransientSurfaceBackdrop backdrop;
};
#pragma warning(pop)

[[nodiscard]] inline DxUi::WindowHostBitmapCapture MakeCapture(UINT widthPx, UINT heightPx, uint32_t seed)
{
    DxUi::WindowHostBitmapCapture capture;
    capture.widthPx  = widthPx;
    capture.heightPx = heightPx;
    capture.bgraPixels.resize(static_cast<size_t>(widthPx) * heightPx * 4u);
    for (UINT y = 0u; y < heightPx; ++y)
        for (UINT x = 0u; x < widthPx; ++x)
        {
            const size_t offset             = (static_cast<size_t>(y) * widthPx + x) * 4u;
            capture.bgraPixels[offset]      = static_cast<uint8_t>(x * seed);
            capture.bgraPixels[offset + 1u] = static_cast<uint8_t>(y * 3u + seed);
            capture.bgraPixels[offset + 2u] = static_cast<uint8_t>(((x / 10u + y / 7u) % 2u) * 200u);
            capture.bgraPixels[offset + 3u] = 0xFFu;
        }
    return capture;
}

__declspec(noinline) inline void TestTransientSurfaceBackdropRetained(GraphicsFixture& gpu)
{
    size_t compared = 0u;
    for (const DxUi::OverlayMaterial material : {DxUi::OverlayMaterial::Mica, DxUi::OverlayMaterial::Acrylic})
        for (const UINT dpi : kDpis)
        {
            DxUi::EmbeddedHost view;
            std::shared_ptr<DxUi::GraphicsDevice> graphics;
            Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "transient surface graphics");
            Hr(view.Attach(std::move(graphics), StoppedTestClock()), "transient surface view");
            view.Controls().SetTheme(MakeTheme(material, true));
            auto root                   = std::make_unique<DxUi::Panel>();
            TransientProbe* const probe = root->AddChild<TransientProbe>();
            probe->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
            view.Controls().SetRoot(std::move(root));
            const UINT widthPx = static_cast<UINT>(MulDiv(320, static_cast<int>(dpi), 96));
            const auto frame   = [&](bool uncached, WorkCounters* work = nullptr)
            {
                view.Controls().Invalidate();
                WorkCounterScope scope;
                {
                    const UncachedScope seam(uncached);
                    Hr(view.Prepare(widthPx, static_cast<UINT>(MulDiv(200, static_cast<int>(dpi), 96)), static_cast<float>(dpi)), "transient surface frame");
                }
                if (work)
                    *work = scope.Delta();
                DxUi::WindowHostBitmapCapture surface;
                Hr(view.DebugReadSurfaceForTest(surface), "transient surface read");
                return std::move(surface.bgraPixels);
            };
            static_cast<void>(frame(false)); // the host's DPI first: the capture's bitmap takes it
            for (const uint32_t seed : {5u, 11u})
            {
                probe->backdrop.SetCapture(
                    MakeCapture(static_cast<UINT>(MulDiv(240, static_cast<int>(dpi), 96)), static_cast<UINT>(MulDiv(120, static_cast<int>(dpi), 96)), seed));
                const std::string context         = std::format("transient {} at {} DPI, capture {}", MaterialName(material), dpi, seed);
                const std::vector<uint8_t> before = frame(true);
                const std::vector<uint8_t> first  = frame(false);
                WorkCounters work{};
                const std::vector<uint8_t> steady = frame(false, &work);
                RequireSame(first, steady, context + ", first against steady retained frame");
                std::cout << std::format("  pixel change, {}: {}\n", context, Describe(RequireWithinOneLevel(before, steady, widthPx, context)));
                Check(work.resources.effect.created == 0u && work.resources.geometry.created == 0u && work.resources.layer.created == 0u &&
                          work.resources.bitmap.created == 0u && work.popups.backdropBlurPasses == 0u,
                      "a steady transient-surface frame makes nothing and runs no blur");
                Check(probe->backdrop.cachedBlurredBitmap != nullptr && probe->backdrop.cachedClipGeometry != nullptr,
                      "the transient surface keeps its blurred capture and clip");
                probe->backdrop.ReleaseRetainedBlur();
                RequireSame(steady, frame(false), context + ", after ReleaseRetainedBlur");
                compared += 2u;
            }
            probe->backdrop.ReleaseRetainedBlur();
            Check(! probe->backdrop.cachedBlurredBitmap && ! probe->backdrop.cachedClipGeometry,
                  "ReleaseRetainedBlur drops the transient surface's retained blur");
            view.Detach();
        }
    std::cout << "PASS transient surface backdrop: " << compared << " comparisons, blurred within one level of the pixels before W2.4\n";
}

// FX2's steady popup frames (hover and fit-hover steps) under `material`.
inline void CheckFx2SteadyFrames(GraphicsFixture& gpu, DxUi::OverlayMaterial material)
{
    using namespace PerformanceFixtures;
    const Definition& definition = Fx2::kDefinition;
    const Variant& variant       = Fx2::kVariants[0];
    gpu.width                    = variant.width;
    gpu.height                   = variant.height;
    Hr(gpu.Resize(variant.width, variant.height), "FX2 popup surface gate size");
    EmbeddedInstance instance;
    instance.Instantiate(definition);
    Hr(instance.Create(gpu.device.get(), variant), "FX2 popup surface gate build");
    instance.View().Controls().SetTheme(MakeTheme(material, true));
    uint64_t steadyFrames = 0u, shadowPasses = 0u, maximumShadowPasses = 0u, blurPasses = 0u, effects = 0u;
    for (size_t phase = 0u; phase < definition.phases.size(); ++phase)
        for (size_t step = 0u; step < definition.phases[phase].steps; ++step)
        {
            WorkCounterScope scope;
            instance.Step(phase, step);
            Hr(instance.Frame(gpu), "FX2 popup surface gate frame");
            const WorkCounters work = scope.Delta();
            if (phase != Fx2::kHover && phase != Fx2::kFitHover)
                continue;
            const auto& r = work.resources;
            Check(r.commandList.created == 0u && r.layer.created == 0u && r.geometry.created == 0u && r.gradientStops.created == 0u,
                  "an FX2 steady popup frame makes no command list, layer, geometry or stop collection");
            ++steadyFrames;
            effects += r.effect.created;
            blurPasses += work.popups.backdropBlurPasses;
            shadowPasses += work.frame.shadowPasses;
            maximumShadowPasses = (std::max)(maximumShadowPasses, work.frame.shadowPasses);
        }
    Check(steadyFrames == Fx2::kHoverMoves + Fx2::kFitHoverMoves, "every FX2 hover step is a steady popup frame");
    Check(effects == 0u && blurPasses == 0u, "an FX2 steady popup frame makes no effect and runs no blur");
    Check(maximumShadowPasses <= 2u, "an FX2 steady popup frame runs at most the list's two shadow passes");
    std::cout << std::format("  FX2 {}: {} steady popup frames: 0 command-list/layer/geometry/stop creations, {} effects made, {} blur passes, {} shadow "
                             "passes (max {} per frame)\n",
                             MaterialName(material),
                             steadyFrames,
                             effects,
                             blurPasses,
                             shadowPasses,
                             maximumShadowPasses);
}

__declspec(noinline) inline void TestFx2PopupSurfaceGate(GraphicsFixture& gpu)
{
    CheckFx2SteadyFrames(gpu, DxUi::OverlayMaterial::Solid);
    CheckFx2SteadyFrames(gpu, DxUi::OverlayMaterial::Acrylic);
    std::cout << "PASS FX2 popup surface gate\n";
}

// A declared control inside the popup, invalidated alone: the next frame paints the region of its extent (W4.10).
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // never copied
class RegionMarker final : public DxUi::Control
{
public:
    RegionMarker() noexcept
    {
        DeclarePaintOverhang();
    }
    void Paint(DxUi::ControlHost&) const override
    {
    }
    void Repaint(DxUi::ControlHost& host) const
    {
        Invalidate(host);
    }
};
class DeclaredPopupSurfaceProbe final : public DxUi::Control
{
public:
    DeclaredPopupSurfaceProbe() noexcept
    {
        DeclarePaintOverhang();
    }
    void Paint(DxUi::ControlHost& host) const override
    {
        inner.Paint(host);
    }
    PopupSurfaceProbe inner;
};
#pragma warning(pop)

// Draws the popup surface (menu material, over striped content, popup at (40,32)-(280,232)) with a 40 by 40 DIP declared marker
// at (230,20)-(270,60), and compares a frame that repaints only the marker's region with the whole frame before it.
[[nodiscard]] inline PixelDifference RegionAgainstWhole(
    GraphicsFixture& gpu, DxUi::OverlayMaterial material, UINT dpi, bool perPaint, D2D1_RECT_F markerDip = D2D1::RectF(230.0f, 20.0f, 270.0f, 60.0f))
{
    DxUi::EmbeddedHost view;
    std::shared_ptr<DxUi::GraphicsDevice> graphics;
    Hr(DxUi::GraphicsDevice::Create(gpu.device.get(), graphics), "region frame graphics");
    Hr(view.Attach(std::move(graphics), StoppedTestClock()), "region frame view");
    const DxUi::ThemePalette theme = MakeTheme(material, true);
    view.Controls().SetTheme(theme);
    auto root   = std::make_unique<DxUi::Panel>();
    auto* probe = root->AddChild<DeclaredPopupSurfaceProbe>();
    probe->SetBounds(D2D1::RectF(0.0f, 0.0f, static_cast<float>(kWidthDip), static_cast<float>(kHeightDip)));
    probe->inner.material     = DxUi::ResolveMenuPopupMaterialForTest(theme);
    probe->inner.shadowTarget = DxUi::PopupShadowTarget::OverContent;
    auto* marker              = root->AddChild<RegionMarker>();
    marker->SetBounds(markerDip);
    view.Controls().SetRoot(std::move(root));
    const UINT widthPx  = static_cast<UINT>(MulDiv(static_cast<int>(kWidthDip), static_cast<int>(dpi), 96));
    const UINT heightPx = static_cast<UINT>(MulDiv(static_cast<int>(kHeightDip), static_cast<int>(dpi), 96));
    const UncachedScope seam(perPaint);
    const auto frame = [&]()
    {
        Hr(view.Prepare(widthPx, heightPx, static_cast<float>(dpi)), "region frame");
        DxUi::WindowHostBitmapCapture surface;
        Hr(view.DebugReadSurfaceForTest(surface), "region frame read");
        return std::move(surface.bgraPixels);
    };
    view.Controls().Invalidate();
    static_cast<void>(frame());
    view.Controls().Invalidate();
    const std::vector<uint8_t> whole = frame();
    marker->Repaint(view.Controls());
    const std::vector<uint8_t> region = frame();
    view.Detach();
    return Difference(whole, region, widthPx);
}

__declspec(noinline) inline void TestPopupBlurRegionFramesEqualWholeFrames(GraphicsFixture& gpu)
{
    // The paint-region verifier stays as the program set it (on in the Debug test programs): its reference paint draws from
    // the kept surface and must agree too.
    const FrameVerifier::Totals before = FrameVerifier::Read();
    for (const DxUi::OverlayMaterial material : {DxUi::OverlayMaterial::Mica, DxUi::OverlayMaterial::Acrylic})
        for (const UINT dpi : {96u, 144u})
        {
            const PixelDifference difference = RegionAgainstWhole(gpu, material, dpi, false);
            if (difference.pixels != 0u)
                std::cerr << std::format("  region frame against whole frame, {} at {} DPI: {}\n", MaterialName(material), dpi, Describe(difference));
            Check(difference.pixels == 0u, "a region frame over a blurred popup equals the whole frame");
        }
    const FrameVerifier::Totals after = FrameVerifier::Read();
    std::cout << std::format("PASS blurred popup region frames equal whole frames (Mica and Acrylic, 96 and 144 DPI); verifier {} frames, {} mismatches\n",
                             after.verified - before.verified,
                             after.mismatches - before.mismatches);
}

// Evidence, not a gate: the per-paint blur (the code before W2.4) under a paint region. Its pixels depend on the paint clip,
// which is why the blur is kept as a bitmap. The verifier is set aside here (it fails such frames).
__declspec(noinline) inline void TestPerPaintBlurRegionEvidence(GraphicsFixture& gpu)
{
    const bool verifying = DxUi::Detail::GetPaintRegionVerificationForTest();
    DxUi::Detail::SetPaintRegionVerificationForTest(false);
    struct Case
    {
        DxUi::OverlayMaterial material;
        UINT dpi;
    };
    for (const Case config : {Case{DxUi::OverlayMaterial::Mica, 96u}, Case{DxUi::OverlayMaterial::Acrylic, 144u}})
        std::cout << std::format("  evidence: the per-paint blur, {} dark at {} DPI, a region frame against the whole frame: {}\n",
                                 MaterialName(config.material),
                                 config.dpi,
                                 Describe(RegionAgainstWhole(gpu, config.material, config.dpi, true)));
    // A Solid popup over content (the ComboBox case) has no blur; its two shadow passes run every paint, as they did before
    // W2.4 (per paint) and from the retained objects (what ships). Regions on the top-right corner and on the bottom-right
    // corner, where the shadow spreads below and right of the popup.
    for (const D2D1_RECT_F marker : {D2D1::RectF(230.0f, 20.0f, 270.0f, 60.0f), D2D1::RectF(260.0f, 210.0f, 300.0f, 250.0f)})
        for (const UINT dpi : {96u, 144u})
            for (const bool perPaint : {true, false})
                std::cout << std::format(
                    "  evidence: the Solid shadow passes over content ({}), dark at {} DPI, region ({},{})-({},{}) DIP against the whole frame: {}\n",
                    perPaint ? "per paint" : "retained",
                    dpi,
                    marker.left,
                    marker.top,
                    marker.right,
                    marker.bottom,
                    Describe(RegionAgainstWhole(gpu, DxUi::OverlayMaterial::Solid, dpi, perPaint, marker)));
    DxUi::Detail::SetPaintRegionVerificationForTest(verifying);
    std::cout << "PASS per-paint blur region evidence recorded\n";
}

inline void Run(GraphicsFixture& gpu)
{
    std::cout << std::unitbuf; // a fail-fast loses buffered output
    const FrameVerifier::Scope verifier;
    TestPopupSurfaceMatchesUncached(gpu);
    TestPopupSurfaceReleases(gpu);
    TestComboBoxPopupSurface(gpu);
    TestPopupBackdropDoesNotFollowContent(gpu);
    TestTransientSurfaceBackdropRetained(gpu);
    TestFx2PopupSurfaceGate(gpu);
    TestPopupBlurRegionFramesEqualWholeFrames(gpu);
    TestPerPaintBlurRegionEvidence(gpu);
}
} // namespace PopupSurfaceTests
