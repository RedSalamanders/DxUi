#include "Controls.Tests.TextRenderingParams.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support/NativeRendering.h"
#include "Support/WorkCounters.h"

#include "ComplexUiScene.h"
#include "GraphicsFixture.h"

#include <d2d1_3.h>
#include <d3d11.h>
#include <dwrite.h>

#include <array>
#include <cstring>
#include <format>
#include <string>
#include <thread>
#include <vector>

using namespace DxUi;

namespace
{
constexpr UINT kProbeWidthPx   = 320u;
constexpr UINT kProbeHeightPx  = 48u;
constexpr wchar_t kProbeText[] = L"Hamburgefonstiv Quartz 0123";

// A WARP device with a Direct2D device context and a DirectWrite factory: the same kind of context both hosts draw with.
struct ProbeGraphics final
{
    wil::com_ptr<ID3D11Device> device;
    wil::com_ptr<ID3D11DeviceContext> context;
    wil::com_ptr<ID2D1Factory1> factory;
    wil::com_ptr<ID2D1Device> d2dDevice;
    wil::com_ptr<IDWriteFactory> write;
    wil::com_ptr<IDWriteTextLayout> layout;
};

ProbeGraphics CreateProbeGraphics()
{
    ProbeGraphics graphics;
    const std::array levels{D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL{};
    RequireSucceeded(
        Detail::CreateD3D11DeviceWithWarpFallback(
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, true, graphics.device.put(), &level, graphics.context.put(), nullptr, Detail::SystemD3D11DeviceCreator()),
        "text rendering probe: create the WARP device");
    const D2D1_FACTORY_OPTIONS options{};
    RequireSucceeded(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, graphics.factory.put_void()),
                     "text rendering probe: create the Direct2D factory");
    wil::com_ptr<IDXGIDevice> dxgi;
    RequireSucceeded(graphics.device->QueryInterface(IID_PPV_ARGS(dxgi.put())), "text rendering probe: query the DXGI device");
    RequireSucceeded(graphics.factory->CreateDevice(dxgi.get(), graphics.d2dDevice.put()), "text rendering probe: create the Direct2D device");
    RequireSucceeded(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), graphics.write.put_unknown()),
                     "text rendering probe: create the DirectWrite factory");
    wil::com_ptr<IDWriteTextFormat> format;
    RequireSucceeded(graphics.write->CreateTextFormat(
                         L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", format.put()),
                     "text rendering probe: create the text format");
    RequireSucceeded(graphics.write->CreateTextLayout(kProbeText,
                                                      static_cast<UINT32>(std::size(kProbeText) - 1u),
                                                      format.get(),
                                                      static_cast<float>(kProbeWidthPx),
                                                      static_cast<float>(kProbeHeightPx),
                                                      graphics.layout.put()),
                     "text rendering probe: create the text layout");
    return graphics;
}

struct ProbeDraw final
{
    HRESULT endDraw = E_FAIL;
    std::vector<uint8_t> bgra;
};

// Draws the probe string, dark on light, into a fresh B8G8R8A8 target of the given alpha mode, as WindowHost
// (IGNORE or PREMULTIPLIED swap-chain buffer) and EmbeddedHost (PREMULTIPLIED texture) do, and reads it back.
// antialias == nullopt leaves the context at its initial mode; params == nullptr binds none.
ProbeDraw DrawProbe(ProbeGraphics& graphics, D2D1_ALPHA_MODE alphaMode, std::optional<D2D1_TEXT_ANTIALIAS_MODE> antialias, IDWriteRenderingParams* params)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = kProbeWidthPx;
    desc.Height           = kProbeHeightPx;
    desc.MipLevels        = 1u;
    desc.ArraySize        = 1u;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    wil::com_ptr<ID3D11Texture2D> texture;
    RequireSucceeded(graphics.device->CreateTexture2D(&desc, nullptr, texture.put()), "text rendering probe: create the target texture");
    wil::com_ptr<IDXGISurface> surface;
    RequireSucceeded(texture->QueryInterface(IID_PPV_ARGS(surface.put())), "text rendering probe: query the target surface");

    wil::com_ptr<ID2D1DeviceContext> dc;
    RequireSucceeded(graphics.d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, dc.put()), "text rendering probe: create the context");
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alphaMode), 96.0f, 96.0f);
    wil::com_ptr<ID2D1Bitmap1> target;
    RequireSucceeded(dc->CreateBitmapFromDxgiSurface(surface.get(), &properties, target.put()), "text rendering probe: wrap the target");
    dc->SetTarget(target.get());
    if (antialias)
        dc->SetTextAntialiasMode(*antialias);
    if (params)
        dc->SetTextRenderingParams(params);
    wil::com_ptr<ID2D1SolidColorBrush> ink;
    RequireSucceeded(dc->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.10f, 1.0f), ink.put()), "text rendering probe: create the brush");

    ProbeDraw result;
    dc->BeginDraw();
    dc->Clear(D2D1::ColorF(0.98f, 0.98f, 0.98f, 1.0f));
    dc->DrawTextLayout(D2D1::Point2F(4.0f, 8.0f), graphics.layout.get(), ink.get());
    result.endDraw = dc->EndDraw();
    dc->SetTarget(nullptr);

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage                = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags            = 0u;
    stagingDesc.CPUAccessFlags       = D3D11_CPU_ACCESS_READ;
    wil::com_ptr<ID3D11Texture2D> staging;
    RequireSucceeded(graphics.device->CreateTexture2D(&stagingDesc, nullptr, staging.put()), "text rendering probe: create the staging texture");
    graphics.context->CopyResource(staging.get(), texture.get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    RequireSucceeded(graphics.context->Map(staging.get(), 0u, D3D11_MAP_READ, 0u, &mapped), "text rendering probe: map the staging texture");
    result.bgra.resize(static_cast<size_t>(kProbeWidthPx) * kProbeHeightPx * 4u);
    for (UINT row = 0u; row < kProbeHeightPx; ++row)
        std::memcpy(result.bgra.data() + static_cast<size_t>(row) * kProbeWidthPx * 4u,
                    static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch,
                    static_cast<size_t>(kProbeWidthPx) * 4u);
    graphics.context->Unmap(staging.get(), 0u);
    return result;
}

// Pixels whose color channels differ: ClearType's subpixel fringes on neutral text. Grayscale and aliased text on a neutral
// ground have none.
size_t CountChromaticPixels(const std::vector<uint8_t>& bgra) noexcept
{
    size_t count = 0u;
    for (size_t index = 0u; index + 3u < bgra.size(); index += 4u)
        if (bgra[index] != bgra[index + 1u] || bgra[index + 1u] != bgra[index + 2u])
            ++count;
    return count;
}

// Distinct gray levels among neutral pixels: aliased text has two (ink and ground), grayscale text has many.
size_t CountGrayLevels(const std::vector<uint8_t>& bgra) noexcept
{
    std::array<bool, 256> seen{};
    size_t count = 0u;
    for (size_t index = 0u; index + 3u < bgra.size(); index += 4u)
        if (bgra[index] == bgra[index + 1u] && bgra[index + 1u] == bgra[index + 2u] && ! seen[bgra[index]])
        {
            seen[bgra[index]] = true;
            ++count;
        }
    return count;
}

const char* AntialiasName(D2D1_TEXT_ANTIALIAS_MODE mode) noexcept
{
    switch (mode)
    {
        case D2D1_TEXT_ANTIALIAS_MODE_DEFAULT: return "DEFAULT";
        case D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE: return "CLEARTYPE";
        case D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE: return "GRAYSCALE";
        case D2D1_TEXT_ANTIALIAS_MODE_ALIASED: return "ALIASED";
        case D2D1_TEXT_ANTIALIAS_MODE_FORCE_DWORD:
        default: return "?";
    }
}

const char* RenderingModeName(DWRITE_RENDERING_MODE mode) noexcept
{
    switch (mode)
    {
        case DWRITE_RENDERING_MODE_DEFAULT: return "DEFAULT";
        case DWRITE_RENDERING_MODE_ALIASED: return "ALIASED";
        case DWRITE_RENDERING_MODE_GDI_CLASSIC: return "GDI_CLASSIC";
        case DWRITE_RENDERING_MODE_GDI_NATURAL: return "GDI_NATURAL";
        case DWRITE_RENDERING_MODE_NATURAL: return "NATURAL";
        case DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC: return "NATURAL_SYMMETRIC";
        case DWRITE_RENDERING_MODE_OUTLINE: return "OUTLINE";
        default: return "?";
    }
}

// FX5 of the performance and motion plan: the ComplexUi scene of Samples/ComplexUi/ComplexUiScene.h, 83 controls at
// 1280x720 and 96 DPI, here also built under a WindowHost root with the same controls, bounds and model.
constexpr UINT kComplexUiWidthPx  = 1280u;
constexpr UINT kComplexUiHeightPx = 720u;

struct ComplexUiWindowScene final
{
    ComplexUiModel model;
    std::array<Slider*, 16> sliders{};
    std::array<ProgressBar*, 16> progress{};
    Grid* grid = nullptr;
};

void BuildComplexUiRoot(ControlHost& host, ComplexUiWindowScene& scene)
{
    auto root = std::make_unique<Panel>();
    for (size_t i = 0; i < scene.sliders.size(); ++i)
    {
        auto* card    = root->AddChild<CardPanel>();
        const float x = 8 + static_cast<float>(i % 4) * 316;
        const float y = 8 + static_cast<float>(i / 4) * 86;
        card->SetBounds(D2D1::RectF(x, y, x + 304, y + 78));
        card->AddChild<Label>(L"Metric " + std::to_wstring(i))->SetBounds(D2D1::RectF(x + 8, y + 2, x + 155, y + 26));
        auto* toggle = card->AddChild<Toggle>(L"Enabled");
        toggle->SetBounds(D2D1::RectF(x + 166, y + 2, x + 296, y + 30));
        toggle->SetChecked(i % 2 == 0);
        scene.sliders[i] = card->AddChild<Slider>();
        scene.sliders[i]->SetBounds(D2D1::RectF(x + 8, y + 30, x + 296, y + 58));
        scene.progress[i] = card->AddChild<ProgressBar>();
        scene.progress[i]->SetBounds(D2D1::RectF(x + 8, y + 62, x + 296, y + 74));
    }
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(8, 360, 380, 712));
    tree->SetModel(&scene.model);
    scene.grid = root->AddChild<Grid>();
    scene.grid->SetBounds(D2D1::RectF(392, 360, 1272, 712));
    scene.grid->SetModel(&scene.model);
    host.SetRoot(std::move(root));
}

// The scene's per-frame change (ComplexUiScene::Update) without the embedded invalidation.
void UpdateComplexUi(ComplexUiWindowScene& scene, size_t frame)
{
    for (size_t i = 0; i < scene.sliders.size(); ++i)
    {
        const double value = static_cast<double>((frame + i) % 100);
        scene.sliders[i]->SetValue(value);
        scene.progress[i]->SetValue(value);
    }
    scene.grid->EnsureRowVisible((frame * 7u) % scene.model.names.size());
}

void ResizeComplexUiWindow(AttachedHostWindow& window)
{
    const HWND hwnd = window.Hwnd();
    RECT rect{0, 0, static_cast<LONG>(kComplexUiWidthPx), static_cast<LONG>(kComplexUiHeightPx)};
    const DWORD style   = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    Require(AdjustWindowRectEx(&rect, style, FALSE, exStyle) != FALSE, "ComplexUi window rect adjusts for its client size");
    const SIZE outer{rect.right - rect.left, rect.bottom - rect.top};
    window.AllowOuterSizeBeyondDesktop(outer);
    Require(SetWindowPos(hwnd, nullptr, -32000, -32000, outer.cx, outer.cy, SWP_NOZORDER | SWP_NOACTIVATE) != FALSE, "ComplexUi window resizes");
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    window.PumpMessages();
}

uint64_t HashPixels(const std::vector<uint8_t>& bgra) noexcept
{
    uint64_t hash = 14695981039346656037ull;
    for (const uint8_t value : bgra)
    {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::filesystem::path TextRenderingEvidenceDirectory()
{
#if defined(__SANITIZE_ADDRESS__)
    constexpr const wchar_t* configuration = L"ASan-Debug";
#elif defined(_DEBUG)
    constexpr const wchar_t* configuration = L"Debug";
#else
    constexpr const wchar_t* configuration = L"Release";
#endif
    return FindRepoRootForDxUiTests() / L".build" / L"test-artifacts" / L"text-rendering" / configuration;
}

void SaveTextRenderingCapture(const std::string& name, UINT width, UINT height, const std::vector<uint8_t>& bgra)
{
    WindowHostBitmapCapture capture;
    capture.widthPx    = width;
    capture.heightPx   = height;
    capture.bgraPixels = bgra;
    std::filesystem::create_directories(TextRenderingEvidenceDirectory());
    static_cast<void>(SaveWindowHostBitmapCaptureAsPngForTest(TextRenderingEvidenceDirectory() / std::filesystem::path(name + ".png"), capture));
    std::cerr << std::format("    [TEXT] capture {} {}x{} fnv1a64=0x{:016X}\n", name, width, height, HashPixels(bgra));
}

void DescribeParams(const char* what, IDWriteRenderingParams* params)
{
    if (! params)
    {
        std::cerr << "    [TEXT] " << what << ": none\n";
        return;
    }
    std::cerr << std::format("    [TEXT] {}: gamma={} enhancedContrast={} clearTypeLevel={} pixelGeometry={} renderingMode={}\n",
                             what,
                             params->GetGamma(),
                             params->GetEnhancedContrast(),
                             params->GetClearTypeLevel(),
                             static_cast<int>(params->GetPixelGeometry()),
                             RenderingModeName(params->GetRenderingMode()));
}
} // namespace

void TestTextAntialiasDefaultsFollowTheTargetAlphaMode()
{
    BOOL smoothing     = FALSE;
    UINT smoothingType = 0u;
    static_cast<void>(SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0u, &smoothing, 0u));
    static_cast<void>(SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0u, &smoothingType, 0u));
    std::cerr << std::format("    [TEXT] system: SPI_GETFONTSMOOTHING={} SPI_GETFONTSMOOTHINGTYPE={} ({})\n",
                             smoothing != FALSE ? 1 : 0,
                             smoothingType,
                             smoothingType == FE_FONTSMOOTHINGCLEARTYPE ? "ClearType" : (smoothingType == FE_FONTSMOOTHINGSTANDARD ? "Standard" : "?"));

    ProbeGraphics graphics = CreateProbeGraphics();
    {
        wil::com_ptr<ID2D1DeviceContext> fresh;
        RequireSucceeded(graphics.d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, fresh.put()), "text rendering probe: a fresh context");
        wil::com_ptr<IDWriteRenderingParams> initialParams;
        fresh->GetTextRenderingParams(initialParams.put());
        std::cerr << "    [TEXT] fresh context: antialias=" << AntialiasName(fresh->GetTextAntialiasMode()) << '\n';
        DescribeParams("fresh context params", initialParams.get());
        Require(fresh->GetTextAntialiasMode() == D2D1_TEXT_ANTIALIAS_MODE_DEFAULT, "a new device context starts at D2D1_TEXT_ANTIALIAS_MODE_DEFAULT");
        Require(! initialParams, "a new device context binds no rendering parameters");
    }
    {
        wil::com_ptr<IDWriteRenderingParams> defaults;
        RequireSucceeded(graphics.write->CreateRenderingParams(defaults.put()), "text rendering probe: DirectWrite default parameters");
        DescribeParams("IDWriteFactory::CreateRenderingParams", defaults.get());
        wil::com_ptr<IDWriteRenderingParams> primary;
        RequireSucceeded(graphics.write->CreateMonitorRenderingParams(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), primary.put()),
                         "text rendering probe: primary-monitor parameters");
        DescribeParams("CreateMonitorRenderingParams(primary)", primary.get());
    }

    struct TargetCase
    {
        const char* name;
        D2D1_ALPHA_MODE alpha;
    };
    for (const TargetCase target :
         {TargetCase{"IGNORE (HwndSwapChain)", D2D1_ALPHA_MODE_IGNORE}, TargetCase{"PREMULTIPLIED (composition, embedded)", D2D1_ALPHA_MODE_PREMULTIPLIED}})
    {
        const ProbeDraw initial   = DrawProbe(graphics, target.alpha, std::nullopt, nullptr);
        const ProbeDraw cleartype = DrawProbe(graphics, target.alpha, D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE, nullptr);
        const ProbeDraw grayscale = DrawProbe(graphics, target.alpha, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE, nullptr);
        const ProbeDraw aliased   = DrawProbe(graphics, target.alpha, D2D1_TEXT_ANTIALIAS_MODE_ALIASED, nullptr);
        Require(SUCCEEDED(initial.endDraw) && SUCCEEDED(cleartype.endDraw) && SUCCEEDED(grayscale.endDraw) && SUCCEEDED(aliased.endDraw),
                "every explicit antialias mode draws without parameters");
        const bool equalsClearType = initial.bgra == cleartype.bgra;
        const bool equalsGrayscale = initial.bgra == grayscale.bgra;
        const bool equalsAliased   = initial.bgra == aliased.bgra;
        std::cerr << std::format("    [TEXT] target {}: default==CLEARTYPE {} default==GRAYSCALE {} default==ALIASED {}; chromatic pixels default {} "
                                 "cleartype {} grayscale {}; gray levels default {} aliased {}\n",
                                 target.name,
                                 equalsClearType,
                                 equalsGrayscale,
                                 equalsAliased,
                                 CountChromaticPixels(initial.bgra),
                                 CountChromaticPixels(cleartype.bgra),
                                 CountChromaticPixels(grayscale.bgra),
                                 CountGrayLevels(initial.bgra),
                                 CountGrayLevels(aliased.bgra));
        Require(CountChromaticPixels(grayscale.bgra) == 0u, "explicit grayscale text on a neutral ground has no chromatic pixel");
        if (target.alpha == D2D1_ALPHA_MODE_IGNORE)
            Require(equalsClearType && CountChromaticPixels(cleartype.bgra) != 0u,
                    "on an opaque (IGNORE) target the default antialias mode is ClearType, byte for byte");
        else
            Require(equalsGrayscale, "on a premultiplied target the default antialias mode is grayscale, byte for byte");
    }
}

namespace
{
// Runs one case on a fresh thread: its native graphics pool starts empty, so the explicit WARP request is never refused or
// inherited from an earlier test's state on the suite thread.
template <typename Body> void RunOnFreshThread(Body&& body)
{
    std::jthread fixture([&]
    {
        const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
        body();
    });
}

std::vector<uint8_t> CaptureComplexUiWindow(WindowHost::PresentationMode mode, bool dark, const char* name)
{
    std::vector<uint8_t> pixels;
    RunOnFreshThread([&]
    {
        ComplexUiWindowScene scene;
        AttachedHostWindow window(mode, true);
        WindowHost& host    = window.Host();
        auto theme          = MakeDefaultThemePalette(dark);
        theme.reducedMotion = true;
        host.SetTheme(theme);
        BuildComplexUiRoot(host, scene);
        UpdateComplexUi(scene, 0u);
        ResizeComplexUiWindow(window);
        WindowHostBitmapCapture capture;
        Require(host.DebugCaptureBitmap(capture), "the ComplexUi window renders and presents a captured frame");
        Require(capture.widthPx == kComplexUiWidthPx && capture.heightPx == kComplexUiHeightPx, "the ComplexUi capture has the fixture size");
        SaveTextRenderingCapture(name, capture.widthPx, capture.heightPx, capture.bgraPixels);
        pixels = std::move(capture.bgraPixels);
    });
    return pixels;
}

std::vector<uint8_t> CaptureComplexUiEmbedded(bool dark, const char* name)
{
    GraphicsFixture gpu;
    gpu.width  = kComplexUiWidthPx;
    gpu.height = kComplexUiHeightPx;
    RequireSucceeded(gpu.Create(), "ComplexUi embedded WARP device");
    ComplexUiScene scene;
    RequireSucceeded(scene.Initialize(gpu.device.get()), "ComplexUi embedded scene");
    auto theme          = MakeDefaultThemePalette(dark);
    theme.reducedMotion = true;
    scene.view.Controls().SetTheme(theme);
    scene.view.MarkDirty();
    RequireSucceeded(scene.view.Prepare(kComplexUiWidthPx, kComplexUiHeightPx), "ComplexUi embedded preparation");
    gpu.Bind();
    RequireSucceeded(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "ComplexUi embedded composition");
    std::vector<uint8_t> pixels;
    RequireSucceeded(gpu.Read(pixels), "ComplexUi embedded readback");
    SaveTextRenderingCapture(name, kComplexUiWidthPx, kComplexUiHeightPx, pixels);
    return pixels;
}
} // namespace

// Harness captures of FX5 with the user's own setting: light-on-dark (dark theme) and dark-on-light (light theme) text in
// both presentation modes and embedded, at 96 DPI, on WARP.
void TestTextRenderingComplexUiCapturesForEachHost()
{
    for (const bool dark : {true, false})
    {
        const char* const theme = dark ? "dark" : "light";
        static_cast<void>(CaptureComplexUiWindow(WindowHost::PresentationMode::HwndSwapChain, dark, std::format("hwnd-{}-96dpi", theme).c_str()));
        static_cast<void>(CaptureComplexUiWindow(WindowHost::PresentationMode::CompositionSwapChain, dark, std::format("composition-{}-96dpi", theme).c_str()));
        static_cast<void>(CaptureComplexUiEmbedded(dark, std::format("embedded-{}-96dpi", theme).c_str()));
    }
}

void TestTextRenderingParamsCompatibilityWithAntialiasModes()
{
    ProbeGraphics graphics = CreateProbeGraphics();
    constexpr std::array renderingModes{DWRITE_RENDERING_MODE_DEFAULT,
                                        DWRITE_RENDERING_MODE_ALIASED,
                                        DWRITE_RENDERING_MODE_GDI_CLASSIC,
                                        DWRITE_RENDERING_MODE_GDI_NATURAL,
                                        DWRITE_RENDERING_MODE_NATURAL,
                                        DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                                        DWRITE_RENDERING_MODE_OUTLINE};
    constexpr std::array antialiasModes{
        D2D1_TEXT_ANTIALIAS_MODE_DEFAULT, D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE, D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE, D2D1_TEXT_ANTIALIAS_MODE_ALIASED};
    for (const D2D1_ALPHA_MODE alpha : {D2D1_ALPHA_MODE_IGNORE, D2D1_ALPHA_MODE_PREMULTIPLIED})
        for (const float clearTypeLevel : {1.0f, 0.0f})
            for (const DWRITE_RENDERING_MODE renderingMode : renderingModes)
            {
                wil::com_ptr<IDWriteRenderingParams> params;
                RequireSucceeded(
                    graphics.write->CreateCustomRenderingParams(1.8f, 0.5f, clearTypeLevel, DWRITE_PIXEL_GEOMETRY_RGB, renderingMode, params.put()),
                    "text rendering probe: custom parameters");
                std::string row = std::format(
                    "    [TEXT] {} level {} {:<17}", alpha == D2D1_ALPHA_MODE_IGNORE ? "IGNORE" : "PREMUL", clearTypeLevel, RenderingModeName(renderingMode));
                bool predicted = true;
                for (const D2D1_TEXT_ANTIALIAS_MODE antialias : antialiasModes)
                {
                    const ProbeDraw draw = DrawProbe(graphics, alpha, antialias, params.get());
                    row += std::format(" {}=0x{:08X}", AntialiasName(antialias), static_cast<uint32_t>(draw.endDraw));
                    predicted = predicted && SUCCEEDED(draw.endDraw) == TextAntialiasModeAcceptsRenderingMode(antialias, renderingMode);
                    // The hosts' repair: parameters derived for the mode always draw, and keep the source's tuning.
                    wil::com_ptr<IDWriteRenderingParams> repaired;
                    const Detail::WorkCounterScope creation;
                    RequireSucceeded(MakeTextRenderingParamsFor(graphics.write.get(), params.get(), antialias, repaired.put()), "derive compatible parameters");
                    const bool kept = repaired.get() == params.get();
                    Require(kept == TextAntialiasModeAcceptsRenderingMode(antialias, renderingMode),
                            "an accepted parameter set is used as it is; only a rejected one is replaced");
                    Require(creation.Delta().resources.renderingParams.created == (kept ? 0u : 1u),
                            "a replacement is one counted rendering-parameter creation");
                    Require(repaired->GetGamma() == params->GetGamma() && repaired->GetEnhancedContrast() == params->GetEnhancedContrast() &&
                                repaired->GetClearTypeLevel() == params->GetClearTypeLevel() && repaired->GetPixelGeometry() == params->GetPixelGeometry(),
                            "the derived parameters keep the monitor's gamma, contrast, ClearType level and pixel geometry");
                    Require(SUCCEEDED(DrawProbe(graphics, alpha, antialias, repaired.get()).endDraw), "derived parameters draw under their antialias mode");
                }
                std::cerr << row << '\n';
                Require(predicted, "TextAntialiasModeAcceptsRenderingMode predicts exactly which pairs Direct2D draws");
            }
}

namespace
{
constexpr std::array kSmoothings{FontSmoothingForTest::ClearType, FontSmoothingForTest::Standard, FontSmoothingForTest::Off};
constexpr size_t kSteadyFrames = 12u;
constexpr RECT kComplexUiRect{0, 0, static_cast<LONG>(kComplexUiWidthPx), static_cast<LONG>(kComplexUiHeightPx)};

const char* SmoothingName(FontSmoothingForTest smoothing) noexcept
{
    switch (smoothing)
    {
        case FontSmoothingForTest::Off: return "off";
        case FontSmoothingForTest::Standard: return "standard";
        case FontSmoothingForTest::ClearType:
        default: return "cleartype";
    }
}

wil::com_ptr<IDWriteFactory> SharedWriteFactory()
{
    wil::com_ptr<IDWriteFactory> write;
    RequireSucceeded(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), write.put_unknown()), "create the DirectWrite factory");
    return write;
}

// The parameters bound to the context are the monitor's own, or a copy with only the rendering mode replaced.
void RequireMonitorParamsBound(ID2D1DeviceContext* dc, HMONITOR monitor, const char* what)
{
    wil::com_ptr<IDWriteRenderingParams> bound;
    dc->GetTextRenderingParams(bound.put());
    Require(bound != nullptr, what);
    Require(TextAntialiasModeAcceptsRenderingMode(dc->GetTextAntialiasMode(), bound->GetRenderingMode()),
            "the bound rendering parameters match the context's antialias mode");
    wil::com_ptr<IDWriteRenderingParams> expected;
    RequireSucceeded(SharedWriteFactory()->CreateMonitorRenderingParams(monitor, expected.put()), "read the monitor's rendering parameters");
    Require(bound->GetGamma() == expected->GetGamma() && bound->GetEnhancedContrast() == expected->GetEnhancedContrast() &&
                bound->GetClearTypeLevel() == expected->GetClearTypeLevel() && bound->GetPixelGeometry() == expected->GetPixelGeometry(),
            "the bound rendering parameters are the monitor's");
}

// One dirty FX5 frame drawn through the native renderer: it renders and presents (EndDraw succeeded), and returns its work.
Detail::WorkCounters RenderComplexUiFrame(WindowHost& host, ComplexUiWindowScene& scene, size_t frame)
{
    UpdateComplexUi(scene, frame);
    const Detail::WorkCounterScope scope;
    host.DebugRenderDirtyRectForTest(kComplexUiRect);
    const Detail::WorkCounters delta = scope.Delta();
    Require(delta.frame.renders == 1u && delta.frame.presents == 1u && host.DebugGetLastPresentResult() == S_OK,
            "every FX5 frame ends its draw without failure and presents");
    return delta;
}

std::vector<uint8_t> CaptureFrameZero(WindowHost& host, ComplexUiWindowScene& scene)
{
    UpdateComplexUi(scene, 0u);
    WindowHostBitmapCapture capture;
    const Detail::WorkCounterScope scope;
    Require(host.DebugCaptureBitmap(capture), "the FX5 window captures a frame");
    Require(scope.Delta().text.setTextRenderingParams == 0u, "a capture of an unchanged setting binds no parameters");
    Require(capture.widthPx == kComplexUiWidthPx && capture.heightPx == kComplexUiHeightPx, "the FX5 capture has the fixture size");
    return std::move(capture.bgraPixels);
}

void RunWindowHostSmoothingCase(WindowHost::PresentationMode mode)
{
    const bool composition = mode == WindowHost::PresentationMode::CompositionSwapChain;
    RunOnFreshThread([&]
    {
        const ScopedFontSmoothingForTest restoreSmoothing;
        ComplexUiWindowScene scene;
        AttachedHostWindow window(mode, true);
        WindowHost& host    = window.Host();
        auto theme          = MakeDefaultThemePalette(true);
        theme.reducedMotion = true;
        host.SetTheme(theme);
        BuildComplexUiRoot(host, scene);
        ResizeComplexUiWindow(window);
        static_cast<void>(RenderComplexUiFrame(host, scene, 0u));
        const HMONITOR monitor = MonitorFromWindow(window.Hwnd(), MONITOR_DEFAULTTONEAREST);

        std::array<std::vector<uint8_t>, kSmoothings.size()> pixels;
        for (size_t index = 0u; index < kSmoothings.size(); ++index)
        {
            const FontSmoothingForTest smoothing = kSmoothings[index];
            ApplyFontSmoothingForTest(host, smoothing);
            const Detail::WorkCounters change = RenderComplexUiFrame(host, scene, 1u);
            Require(change.text.setTextRenderingParams <= 1u, "a setting change binds at most one parameter set");
            for (size_t frame = 0u; frame < kSteadyFrames; ++frame)
            {
                const Detail::WorkCounters steady = RenderComplexUiFrame(host, scene, 2u + frame);
                Require(steady.text.setTextRenderingParams == 0u && steady.resources.renderingParams.created == 0u,
                        "a steady FX5 frame binds and creates no rendering parameters");
            }
            ID2D1DeviceContext* const dc = host.GetDeviceContext();
            const D2D1_TEXT_ANTIALIAS_MODE expected =
                (composition || smoothing == FontSmoothingForTest::ClearType) ? D2D1_TEXT_ANTIALIAS_MODE_DEFAULT : D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
            Require(dc && dc->GetTextAntialiasMode() == expected,
                    composition ? "the composition chain keeps Direct2D's default (grayscale on a premultiplied target)"
                                : "the opaque chain keeps the ClearType default and turns grayscale when ClearType is off");
            RequireMonitorParamsBound(dc, monitor, "the window host binds its monitor's rendering parameters");
            pixels[index] = CaptureFrameZero(host, scene);
            SaveTextRenderingCapture(std::format("{}-dark-96dpi-{}", composition ? "composition" : "hwnd", SmoothingName(smoothing)),
                                     kComplexUiWidthPx,
                                     kComplexUiHeightPx,
                                     pixels[index]);
        }
        if (composition)
            Require(pixels[0] == pixels[1] && pixels[1] == pixels[2], "a premultiplied chain draws grayscale text whatever the setting");
        else
        {
            Require(pixels[0] != pixels[1], "an opaque chain draws ClearType text only while ClearType is on");
            Require(pixels[1] == pixels[2], "Standard and off both draw grayscale text on an opaque chain");
        }
    });
}

void RunDefaultParityCase(WindowHost::PresentationMode mode, bool dark)
{
    RunOnFreshThread([&]
    {
        DebugSetFontSmoothingForTest(std::nullopt);
        ComplexUiWindowScene scene;
        AttachedHostWindow window(mode, true);
        WindowHost& host    = window.Host();
        auto theme          = MakeDefaultThemePalette(dark);
        theme.reducedMotion = true;
        host.SetTheme(theme);
        BuildComplexUiRoot(host, scene);
        ResizeComplexUiWindow(window);
        const std::vector<uint8_t> current = CaptureFrameZero(host, scene);
        // Today's state: no parameters and the default antialias mode. The host re-applies only on a change, so the next
        // frame draws in this state.
        ID2D1DeviceContext* const dc = host.GetDeviceContext();
        dc->SetTextRenderingParams(nullptr);
        dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);
        const std::vector<uint8_t> today = CaptureFrameZero(host, scene);
        if (ResolveFontSmoothing() == FontSmoothing::ClearType || mode == WindowHost::PresentationMode::CompositionSwapChain)
            Require(current == today, "with the user's unchanged ClearType setting the frame is byte-identical to one without bound parameters");
        else
            std::cerr << "    [TEXT] the user's setting is not ClearType: the opaque chain is grayscale by design\n";
    });
}

std::vector<uint8_t> PrepareEmbeddedFrame(GraphicsFixture& gpu, ComplexUiScene& scene, size_t frame, Detail::WorkCounters& work)
{
    scene.Update(frame);
    const Detail::WorkCounterScope scope;
    const HRESULT hr = scene.view.Prepare(kComplexUiWidthPx, kComplexUiHeightPx);
    work             = scope.Delta();
    Require(hr == S_OK && work.frame.preparations == 1u, "every embedded FX5 preparation ends its draw without failure");
    gpu.Bind();
    RequireSucceeded(scene.view.Composite(gpu.context.get(), gpu.Viewport()), "embedded FX5 composition");
    std::vector<uint8_t> pixels;
    RequireSucceeded(gpu.Read(pixels), "embedded FX5 readback");
    return pixels;
}
} // namespace

void TestTextRenderingWindowHostFollowsFontSmoothingOnComplexUi()
{
    RunWindowHostSmoothingCase(WindowHost::PresentationMode::HwndSwapChain);
    RunWindowHostSmoothingCase(WindowHost::PresentationMode::CompositionSwapChain);
}

void TestTextRenderingDefaultSettingKeepsTodaysPixels()
{
    for (const bool dark : {true, false})
    {
        RunDefaultParityCase(WindowHost::PresentationMode::HwndSwapChain, dark);
        RunDefaultParityCase(WindowHost::PresentationMode::CompositionSwapChain, dark);
    }
}

void TestTextRenderingEmbeddedComplexUiUnderEachSetting()
{
    const ScopedFontSmoothingForTest restoreSmoothing;
    GraphicsFixture gpu;
    gpu.width  = kComplexUiWidthPx;
    gpu.height = kComplexUiHeightPx;
    RequireSucceeded(gpu.Create(), "embedded FX5 WARP device");
    ComplexUiScene scene;
    const Detail::WorkCounterScope attach;
    RequireSucceeded(scene.Initialize(gpu.device.get()), "embedded FX5 scene");
    Detail::WorkCounters work{};
    const std::vector<uint8_t> withoutMonitor = PrepareEmbeddedFrame(gpu, scene, 0u, work);
    ID2D1DeviceContext* const dc              = scene.view.Controls().GetDeviceContext();
    wil::com_ptr<IDWriteRenderingParams> bound;
    dc->GetTextRenderingParams(bound.put());
    Require(attach.Delta().text.setTextRenderingParams == 0u && ! bound && dc->GetTextAntialiasMode() == D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE,
            "without a monitor an embedded view keeps grayscale and binds no parameters, exactly as before");

    const HMONITOR primary = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    for (const HMONITOR monitor : {HMONITOR{}, primary})
    {
        RequireSucceeded(scene.view.SetTextRenderingMonitor(monitor), "set the embedded view's monitor");
        Require(scene.view.NeedsPreparation(), "a new monitor asks for preparation");
        for (const FontSmoothingForTest smoothing : kSmoothings)
        {
            DebugSetFontSmoothingForTest(smoothing);
            static_cast<void>(PrepareEmbeddedFrame(gpu, scene, 1u, work));
            Require(work.text.setTextRenderingParams <= 1u, "a monitor or setting change binds at most one parameter set");
            for (size_t frame = 0u; frame < kSteadyFrames; ++frame)
            {
                static_cast<void>(PrepareEmbeddedFrame(gpu, scene, 2u + frame, work));
                Require(work.text.setTextRenderingParams == 0u && work.resources.renderingParams.created == 0u,
                        "a steady embedded FX5 frame binds and creates no rendering parameters");
            }
            Require(dc->GetTextAntialiasMode() == D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE, "embedded text stays grayscale whatever the setting");
            if (monitor)
                RequireMonitorParamsBound(dc, monitor, "an embedded view binds the parameters of the monitor the application supplied");
            else
            {
                bound.reset();
                dc->GetTextRenderingParams(bound.put());
                Require(! bound, "an embedded view without a monitor binds no parameters");
            }
        }
    }
    DebugSetFontSmoothingForTest(std::nullopt);
    const std::vector<uint8_t> withMonitor = PrepareEmbeddedFrame(gpu, scene, 0u, work);
    // With the primary monitor's parameters equal to DirectWrite's defaults, binding them changes no pixel.
    wil::com_ptr<IDWriteRenderingParams> defaults;
    RequireSucceeded(SharedWriteFactory()->CreateRenderingParams(defaults.put()), "read DirectWrite's default parameters");
    wil::com_ptr<IDWriteRenderingParams> primaryParams;
    RequireSucceeded(SharedWriteFactory()->CreateMonitorRenderingParams(primary, primaryParams.put()), "read the primary monitor's parameters");
    const bool sameTuning = defaults->GetGamma() == primaryParams->GetGamma() && defaults->GetEnhancedContrast() == primaryParams->GetEnhancedContrast() &&
                            defaults->GetClearTypeLevel() == primaryParams->GetClearTypeLevel() &&
                            defaults->GetPixelGeometry() == primaryParams->GetPixelGeometry() &&
                            defaults->GetRenderingMode() == primaryParams->GetRenderingMode();
    std::cerr << std::format(
        "    [TEXT] embedded primary-monitor parameters equal the defaults {}; frame equal {}\n", sameTuning, withMonitor == withoutMonitor);
    if (sameTuning)
        Require(withMonitor == withoutMonitor, "binding parameters equal to the defaults changes no embedded pixel");

    RequireSucceeded(scene.view.SetTextRenderingMonitor(nullptr), "clear the embedded view's monitor");
    const std::vector<uint8_t> cleared = PrepareEmbeddedFrame(gpu, scene, 0u, work);
    bound.reset();
    dc->GetTextRenderingParams(bound.put());
    Require(work.text.setTextRenderingParams == 1u && ! bound, "clearing the monitor unbinds the parameters once");
    Require(cleared == withoutMonitor, "after clearing the monitor the view draws exactly as before");
}

void TestTextRenderingWindowHostRebindsOnDpiMonitorAndSettingChanges()
{
    RunOnFreshThread([&]
    {
        DebugSetFontSmoothingForTest(std::nullopt);
        ComplexUiWindowScene scene;
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        WindowHost& host = window.Host();
        BuildComplexUiRoot(host, scene);
        ResizeComplexUiWindow(window);
        static_cast<void>(RenderComplexUiFrame(host, scene, 0u));
        const auto send = [&](UINT message, WPARAM wParam, LPARAM lParam)
        {
            bool handled = false;
            static_cast<void>(host.HandleMessage(window.Hwnd(), message, wParam, lParam, handled));
        };

        // A move that stays on the same monitor keeps the parameters.
        send(WM_MOVE, 0u, MAKELPARAM(-32000, -32000));
        Detail::WorkCounters frame = RenderComplexUiFrame(host, scene, 1u);
        Require(frame.resources.renderingParams.created == 0u && frame.text.setTextRenderingParams == 0u, "a same-monitor move keeps the parameters");
        // An unrelated setting keeps them too.
        send(WM_SETTINGCHANGE, SPI_SETWHEELSCROLLLINES, 0);
        frame = RenderComplexUiFrame(host, scene, 2u);
        Require(frame.resources.renderingParams.created == 0u && frame.text.setTextRenderingParams == 0u, "an unrelated setting keeps the parameters");
        // ClearType tuning reads the monitor's parameters again.
        send(WM_SETTINGCHANGE, SPI_SETFONTSMOOTHINGCONTRAST, 0);
        frame = RenderComplexUiFrame(host, scene, 3u);
        Require(frame.resources.renderingParams.created == 1u && frame.text.setTextRenderingParams == 1u,
                "ClearType tuning rebinds the monitor's parameters once");
        // A DPI change (the window may have moved to another monitor) reads them again, once.
        RECT windowRect{};
        Require(GetWindowRect(window.Hwnd(), &windowRect) != FALSE, "read the window rectangle");
        send(WM_DPICHANGED, MAKEWPARAM(USER_DEFAULT_SCREEN_DPI, USER_DEFAULT_SCREEN_DPI), reinterpret_cast<LPARAM>(&windowRect));
        frame = RenderComplexUiFrame(host, scene, 4u);
        Require(frame.resources.renderingParams.created == 1u && frame.text.setTextRenderingParams == 1u, "a DPI change rebinds the monitor's parameters once");
        for (size_t index = 0u; index < kSteadyFrames; ++index)
        {
            frame = RenderComplexUiFrame(host, scene, 5u + index);
            Require(frame.resources.renderingParams.created == 0u && frame.text.setTextRenderingParams == 0u, "steady frames after a rebind bind nothing");
        }
        RequireMonitorParamsBound(
            host.GetDeviceContext(), MonitorFromWindow(window.Hwnd(), MONITOR_DEFAULTTONEAREST), "the rebound parameters are the monitor's");
    });
}
