#include "../../src/Controls/TextClipboard.h"
#pragma once

#include "../../src/Controls/DxUi.Internal.h"
#include "../../src/Controls/DxUi.h"
#include "../../src/Support/Diagnostics.h"
#include "../../src/Support/WindowMessages.h"
#include "../Support/Support.Tests.TestWatchdog.h"
#include "../Support/Support.Tests.TestWindowActivationGuard.h"

#include <UIAutomation.h>
#include <imm.h>
#include <richedit.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wincodec.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#pragma comment(lib, "imm32.lib")

// Animation fixtures explicitly choose motion instead of depending on the runner's accessibility preference.
// Tests for reduced motion still set theme.reducedMotion = true. No system setting is changed.
[[nodiscard]] inline DxUi::ThemePalette MakeAnimatedTestThemePalette(bool dark)
{
    auto theme          = DxUi::MakeDefaultThemePalette(dark);
    theme.reducedMotion = false;
    return theme;
}

inline void EnableMotionForTest(DxUi::WindowHost& host)
{
    auto theme          = host.GetTheme();
    theme.reducedMotion = false;
    host.SetTheme(theme);
}

inline void Require(bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline void SkipDxUiTest(const char* reason)
{
    std::cerr << "SKIPPED: " << reason << '\n';
}

// --test=<Name>[,<Name>...] limits a run to the named test functions of the selected suites. Suite runners start every test
// through DXUI_RUN_TEST, so a run without the option executes all of them in order, and a name that no selected suite
// registers is reported when the suites end instead of passing with nothing run. Names are exact and case-sensitive.
struct DxUiTestFilter
{
    bool active = false;
    std::vector<std::string> names;
    std::vector<bool> matched;
};

inline DxUiTestFilter& GetDxUiTestFilter() noexcept
{
    static DxUiTestFilter filter;
    return filter;
}

inline void SetDxUiTestFilter(std::vector<std::string> names)
{
    DxUiTestFilter& filter = GetDxUiTestFilter();
    filter.active          = true;
    filter.matched.assign(names.size(), false);
    filter.names = std::move(names);
}

// The requested test names that no suite registered a test for.
[[nodiscard]] inline std::vector<std::string> UnmatchedDxUiTestNames()
{
    const DxUiTestFilter& filter = GetDxUiTestFilter();
    std::vector<std::string> unmatched;
    for (size_t index = 0u; index < filter.names.size(); ++index)
    {
        if (! filter.matched[index])
            unmatched.push_back(filter.names[index]);
    }
    return unmatched;
}

// The unit of work under way (a test, or a fixture suite without named tests) has made progress: its watchdog deadline starts
// over. A fixture that repeats one cycle many times reports each cycle, so the deadline bounds a cycle instead of the whole
// fixture.
inline void NoteDxUiTestProgress()
{
    DxUi::TestSupport::TestWatchdog::Instance().Renew();
}

// Runs one test with its [START]/[DONE] markers, and its duration on [DONE], unless --test= leaves it out. Returns whether it ran.
// Every test runs under the watchdog's deadline (--test-timeout=<seconds>, 0 turns it off): a test that outlives it ends the run
// with "TIMEOUT: <name> after <N> s" and exit code 124 instead of holding a CI job until the job's time limit. A fixture suite
// without named tests is one unit and is armed by the runner (see DxUi.Tests.Runner.cpp); see Tests/Support/Support.Tests.TestWatchdog.h.
inline bool RunDxUiTest(const char* name, void (*test)())
{
    DxUiTestFilter& filter = GetDxUiTestFilter();
    if (filter.active)
    {
        bool selected = false;
        for (size_t index = 0u; index < filter.names.size(); ++index)
        {
            if (filter.names[index] == name)
            {
                filter.matched[index] = true;
                selected              = true;
            }
        }
        if (! selected)
            return false;
    }
    std::cerr << "  [START] " << name << '\n' << std::flush;
    const auto started = std::chrono::steady_clock::now();
    {
        const DxUi::TestSupport::ScopedTestDeadline deadline(name);
        test();
    }
    std::cerr << std::format("  [DONE] {} ({:.3f} s)\n", name, std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()) << std::flush;
    return true;
}

#define DXUI_RUN_TEST(test) RunDxUiTest(#test, test)

[[nodiscard]] inline bool WaitForDxUiThreadFocus(HWND hwnd, DWORD timeoutMs = 800u) noexcept
{
    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        if (GetFocus() == hwnd)
        {
            return true;
        }

        Sleep(10);
    } while (GetTickCount64() < deadline);

    return GetFocus() == hwnd;
}

[[nodiscard]] inline bool TryFocusDxUiTestWindow(HWND hwnd, DWORD timeoutMs = 800u) noexcept
{
    if (hwnd == nullptr || IsWindow(hwnd) == FALSE)
    {
        return false;
    }
    if ((GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) != 0)
    {
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    static_cast<void>(UpdateWindow(hwnd));
    static_cast<void>(SetActiveWindow(hwnd));
    static_cast<void>(SetFocus(hwnd));

    return WaitForDxUiThreadFocus(hwnd, timeoutMs);
}

[[nodiscard]] inline bool TryActivateDxUiTestWindow(HWND hwnd, DWORD timeoutMs = 800u) noexcept
{
    if (! TryFocusDxUiTestWindow(hwnd, timeoutMs))
    {
        return false;
    }

    if (GetForegroundWindow() == hwnd)
    {
        return true;
    }

    if (SetForegroundWindow(hwnd) == FALSE && GetForegroundWindow() != hwnd)
    {
        return false;
    }

    const ULONGLONG deadline = GetTickCount64() + timeoutMs;
    do
    {
        if (GetForegroundWindow() == hwnd)
        {
            return true;
        }

        Sleep(10);
    } while (GetTickCount64() < deadline);

    return GetForegroundWindow() == hwnd;
}

inline void RequireColorNear(const D2D1_COLOR_F& actual, const D2D1_COLOR_F& expected, const char* message)
{
    const auto nearlyEqual = [](float a, float b) noexcept { return std::fabs(a - b) <= 0.0001f; };

    if (! nearlyEqual(actual.r, expected.r) || ! nearlyEqual(actual.g, expected.g) || ! nearlyEqual(actual.b, expected.b) ||
        ! nearlyEqual(actual.a, expected.a))
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline void RequireColorDifferent(const D2D1_COLOR_F& lhs, const D2D1_COLOR_F& rhs, const char* message)
{
    const auto nearlyEqual = [](float a, float b) noexcept { return std::fabs(a - b) <= 0.0001f; };

    if (nearlyEqual(lhs.r, rhs.r) && nearlyEqual(lhs.g, rhs.g) && nearlyEqual(lhs.b, rhs.b) && nearlyEqual(lhs.a, rhs.a))
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline void RequirePointNear(const POINT& actual, const POINT& expected, const char* message)
{
    const auto nearlyEqual = [](LONG a, LONG b) noexcept { return std::abs(a - b) <= 1; };

    if (! nearlyEqual(actual.x, expected.x) || ! nearlyEqual(actual.y, expected.y))
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

[[maybe_unused]] inline void RequireSucceeded(HRESULT hr, const char* message)
{
    if (FAILED(hr))
    {
        std::cerr << "FAILED: " << message << " hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << '\n';
        std::exit(1);
    }
}

[[nodiscard]] inline std::wstring ReadTextRangeText(ITextRangeProvider& range, int maxLength, const char* context)
{
    BSTR text = nullptr;
    RequireSucceeded(range.GetText(maxLength, &text), context);
    const auto freeText = wil::scope_exit([&] { SysFreeString(text); });
    return std::wstring(text ? text : L"");
}

inline void RequireRectNear(const RECT& actual, const RECT& expected, const char* message)
{
    const auto nearlyEqual = [](LONG a, LONG b) noexcept { return std::abs(a - b) <= 1; };

    if (! nearlyEqual(actual.left, expected.left) || ! nearlyEqual(actual.top, expected.top) || ! nearlyEqual(actual.right, expected.right) ||
        ! nearlyEqual(actual.bottom, expected.bottom))
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline void RequireRectHasArea(const D2D1_RECT_F& rect, const char* message)
{
    if (rect.right <= rect.left || rect.bottom <= rect.top)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline void RequireFloatNear(float actual, float expected, float epsilon, const char* message)
{
    if (std::fabs(actual - expected) > epsilon)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

inline bool& DxUiWriteBaselinesFlag() noexcept
{
    static bool value = false;
    return value;
}

inline void SetDxUiWriteBaselines(bool value) noexcept
{
    DxUiWriteBaselinesFlag() = value;
}

inline bool ShouldWriteDxUiBaselines() noexcept
{
    return DxUiWriteBaselinesFlag();
}

inline bool& DxUiTestWindowsCanActivateFlag() noexcept
{
    static bool value = false;
    return value;
}

inline void SetDxUiTestWindowsCanActivate(bool value) noexcept
{
    DxUiTestWindowsCanActivateFlag() = value;
}

// Test windows created while it lives cannot be activated, whatever the suite allows. A test that simulates another
// application taking the foreground stays deterministic this way: no application can really take it from such a window.
class ScopedNonActivatingTestWindows final
{
public:
    ScopedNonActivatingTestWindows() noexcept : _previous(DxUiTestWindowsCanActivateFlag())
    {
        SetDxUiTestWindowsCanActivate(false);
    }

    ~ScopedNonActivatingTestWindows()
    {
        SetDxUiTestWindowsCanActivate(_previous);
    }

    ScopedNonActivatingTestWindows(const ScopedNonActivatingTestWindows&)            = delete;
    ScopedNonActivatingTestWindows& operator=(const ScopedNonActivatingTestWindows&) = delete;
    ScopedNonActivatingTestWindows(ScopedNonActivatingTestWindows&&)                 = delete;
    ScopedNonActivatingTestWindows& operator=(ScopedNonActivatingTestWindows&&)      = delete;

private:
    bool _previous;
};

inline constexpr std::wstring_view kDxUiHarnessArtifactSegment{L"dxui"};

[[nodiscard]] inline std::filesystem::path GetDxUiTestArtifactDirectory(std::error_code& ec) noexcept
{
    auto directory = std::filesystem::current_path() / L".build" / L"test-artifacts" / std::to_wstring(GetCurrentProcessId());
    std::filesystem::create_directories(directory, ec);
    return directory;
}

[[nodiscard]] inline std::filesystem::path GetDxUiTestArtifactDirectory()
{
    std::error_code ec;
    const std::filesystem::path directory = GetDxUiTestArtifactDirectory(ec);
    Require(! ec && ! directory.empty(), "DxUiTests artifact TestSandbox root is available");
    return directory;
}

[[nodiscard]] inline std::filesystem::path GetDxUiTestArtifactPath(std::wstring_view fileName)
{
    return GetDxUiTestArtifactDirectory() / std::filesystem::path(fileName);
}

inline std::filesystem::path FindRepoRootForDxUiTests()
{
    std::filesystem::path current = std::filesystem::current_path();
    for (size_t depth = 0u; depth < 8u; ++depth)
    {
        if (std::filesystem::exists(current / L"DxUi.sln") && std::filesystem::exists(current / L"Tests" / L"Controls"))
        {
            return current;
        }

        if (! current.has_parent_path())
        {
            break;
        }
        current = current.parent_path();
    }

    std::cerr << "FAILED: unable to locate repo root for DxUi baseline tests\n";
    std::exit(1);
}

inline std::filesystem::path GetDxUiBaselineDirectory()
{
    return FindRepoRootForDxUiTests() / L"Tests" / L"Controls" / L"Baselines";
}

inline std::filesystem::path GetDxUiBaselinePath(std::wstring_view fileName)
{
    return GetDxUiBaselineDirectory() / std::filesystem::path(fileName);
}

inline wil::com_ptr<IWICImagingFactory> CreateWicFactoryForTest()
{
    static thread_local const HRESULT hrCoInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    Require(hrCoInit == S_OK || hrCoInit == S_FALSE || hrCoInit == RPC_E_CHANGED_MODE, "WIC factory COM initialization succeeds");

    wil::com_ptr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.addressof()));
    if (FAILED(hr) || ! factory)
    {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.addressof()));
    }
    RequireSucceeded(hr, "WIC imaging factory created for baseline tests");
    Require(factory != nullptr, "WIC imaging factory instance created for baseline tests");
    return factory;
}

inline bool SaveWindowHostBitmapCaptureAsPngForTest(const std::filesystem::path& path, const DxUi::WindowHostBitmapCapture& capture)
{
    if (capture.widthPx == 0u || capture.heightPx == 0u || capture.bgraPixels.empty())
    {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        return false;
    }

    const auto factory = CreateWicFactoryForTest();

    wil::com_ptr<IWICStream> stream;
    if (FAILED(factory->CreateStream(stream.addressof())) || ! stream)
    {
        return false;
    }
    if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)))
    {
        return false;
    }

    wil::com_ptr<IWICBitmapEncoder> encoder;
    if (FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.addressof())) || ! encoder)
    {
        return false;
    }
    if (FAILED(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache)))
    {
        return false;
    }

    wil::com_ptr<IWICBitmapFrameEncode> frame;
    wil::com_ptr<IPropertyBag2> properties;
    if (FAILED(encoder->CreateNewFrame(frame.addressof(), properties.addressof())) || ! frame)
    {
        return false;
    }
    if (FAILED(frame->Initialize(properties.get())) || FAILED(frame->SetSize(capture.widthPx, capture.heightPx)))
    {
        return false;
    }

    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&pixelFormat)))
    {
        return false;
    }
    if (pixelFormat != GUID_WICPixelFormat32bppBGRA)
    {
        return false;
    }

    const UINT stride = capture.widthPx * 4u;
    if (FAILED(frame->WritePixels(capture.heightPx,
                                  stride,
                                  static_cast<UINT>(capture.bgraPixels.size()),
                                  const_cast<BYTE*>(reinterpret_cast<const BYTE*>(capture.bgraPixels.data())))))
    {
        return false;
    }

    return SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
}

inline bool LoadWindowHostBitmapCaptureFromPngForTest(const std::filesystem::path& path, DxUi::WindowHostBitmapCapture& capture)
{
    capture = {};
    if (! std::filesystem::exists(path))
    {
        return false;
    }

    const auto factory = CreateWicFactoryForTest();

    wil::com_ptr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.addressof())) || ! decoder)
    {
        return false;
    }

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0u, frame.addressof())) || ! frame)
    {
        return false;
    }

    UINT width  = 0u;
    UINT height = 0u;
    if (FAILED(frame->GetSize(&width, &height)) || width == 0u || height == 0u)
    {
        return false;
    }

    wil::com_ptr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.addressof())) || ! converter)
    {
        return false;
    }
    if (FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom)))
    {
        return false;
    }

    capture.widthPx  = width;
    capture.heightPx = height;
    capture.bgraPixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    const UINT stride = width * 4u;
    return SUCCEEDED(converter->CopyPixels(nullptr, stride, static_cast<UINT>(capture.bgraPixels.size()), reinterpret_cast<BYTE*>(capture.bgraPixels.data())));
}

struct BitmapComparisonStats
{
    size_t differingPixels  = 0u;
    size_t totalPixels      = 0u;
    uint8_t maxChannelDelta = 0u;

    [[nodiscard]] double DifferenceRatio() const noexcept
    {
        return totalPixels == 0u ? 1.0 : static_cast<double>(differingPixels) / static_cast<double>(totalPixels);
    }
};

inline BitmapComparisonStats CompareWindowHostBitmapCapturesForTest(const DxUi::WindowHostBitmapCapture& actual,
                                                                    const DxUi::WindowHostBitmapCapture& expected,
                                                                    uint8_t perChannelTolerance = 8u) noexcept
{
    BitmapComparisonStats stats{};
    if (actual.widthPx != expected.widthPx || actual.heightPx != expected.heightPx)
    {
        return stats;
    }

    stats.totalPixels = static_cast<size_t>(actual.widthPx) * static_cast<size_t>(actual.heightPx);
    for (size_t pixelIndex = 0u; pixelIndex < stats.totalPixels; ++pixelIndex)
    {
        const size_t base     = pixelIndex * 4u;
        uint8_t pixelMaxDelta = 0u;
        for (size_t channelIndex = 0u; channelIndex < 4u; ++channelIndex)
        {
            const uint8_t actualValue   = actual.bgraPixels[base + channelIndex];
            const uint8_t expectedValue = expected.bgraPixels[base + channelIndex];
            const uint8_t delta =
                actualValue > expectedValue ? static_cast<uint8_t>(actualValue - expectedValue) : static_cast<uint8_t>(expectedValue - actualValue);
            pixelMaxDelta = (std::max)(pixelMaxDelta, delta);
        }

        stats.maxChannelDelta = (std::max)(stats.maxChannelDelta, pixelMaxDelta);
        if (pixelMaxDelta > perChannelTolerance)
        {
            ++stats.differingPixels;
        }
    }

    return stats;
}

inline void VerifyOrUpdateBaselineForTest(const char* context,
                                          std::wstring_view fileName,
                                          const DxUi::WindowHostBitmapCapture& actual,
                                          double maxDifferenceRatio   = 0.02,
                                          uint8_t perChannelTolerance = 8u)
{
    const std::filesystem::path baselinePath = GetDxUiBaselinePath(fileName);
    if (ShouldWriteDxUiBaselines())
    {
        Require(SaveWindowHostBitmapCaptureAsPngForTest(baselinePath, actual), context);
        return;
    }

    DxUi::WindowHostBitmapCapture expected;
    Require(LoadWindowHostBitmapCaptureFromPngForTest(baselinePath, expected), context);
    Require(expected.widthPx == actual.widthPx && expected.heightPx == actual.heightPx, context);

    const BitmapComparisonStats stats = CompareWindowHostBitmapCapturesForTest(actual, expected, perChannelTolerance);
    if (stats.DifferenceRatio() > maxDifferenceRatio)
    {
        const std::filesystem::path actualPath = FindRepoRootForDxUiTests() / L".build" / L"test-artifacts" / L"baseline-actual" / baselinePath.filename();
        static_cast<void>(SaveWindowHostBitmapCaptureAsPngForTest(actualPath, actual));
        std::cerr << "FAILED: " << context << " diffRatio=" << stats.DifferenceRatio() << " maxChannelDelta=" << static_cast<int>(stats.maxChannelDelta)
                  << " actual=" << actualPath.string() << '\n';
        std::exit(1);
    }
}

// Clipboard listeners and sync providers can momentarily race the first WM_PASTE
// observation on desktop test runs, so clipboard-driven tests retry the
// full interaction a few times instead of failing on the first transient miss.
template <typename TAction> inline bool RetryClipboardSensitiveAction(TAction action)
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        if (action())
        {
            return true;
        }

        Sleep(20);
    }

    return false;
}

inline std::optional<std::wstring> ReadClipboardUnicodeTextForTest(HWND ownerWindow);

inline bool DispatchQueuedMessageForTest(const MSG& msg)
{
    if (msg.hwnd != nullptr && IsWindow(msg.hwnd) == FALSE)
    {
        std::cerr << "    [TRACE] dropping queued message for destroyed hwnd: msg=" << msg.message << " hwnd=" << msg.hwnd << " wp=" << msg.wParam
                  << " lp=" << msg.lParam << '\n'
                  << std::flush;
        return false;
    }

    TranslateMessage(&msg);
    DispatchMessageW(&msg);
    return true;
}

inline wil::unique_hwnd CreateClipboardOwnerWindowForTest()
{
    HWND hwnd =
        CreateWindowExW(0, L"STATIC", L"DxUiTestsClipboardOwner", WS_OVERLAPPED, -32000, -32000, 16, 16, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (! hwnd)
    {
        const DWORD error = GetLastError();
        std::cerr << "    [WINDOW] create clipboard owner failed: Win32 error " << error << '\n';
    }
    Require(hwnd != nullptr, "clipboard owner window created");
    return wil::unique_hwnd(hwnd);
}

inline bool SetClipboardUnicodeTextForTest(HWND ownerWindow, std::wstring_view text)
{
    const HRESULT hr = DxUi::ActiveTextClipboard().Write(ownerWindow, text);
    if (hr == S_OK)
        static_cast<void>(DxUi::DebugSetClipboardFallbackText(text));
    return hr == S_OK;
}
inline std::optional<std::wstring> ReadClipboardUnicodeTextForTest(HWND ownerWindow)
{
    std::wstring text;
    if (DxUi::ActiveTextClipboard().Read(ownerWindow, text) != S_OK)
        return std::nullopt;
    static_cast<void>(DxUi::DebugSetClipboardFallbackText(text));
    return text;
}

inline D2D1_COLOR_F BlendForTest(const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) noexcept
{
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return D2D1::ColorF(a.r + ((b.r - a.r) * clamped), a.g + ((b.g - a.g) * clamped), a.b + ((b.b - a.b) * clamped), a.a + ((b.a - a.a) * clamped));
}

inline D2D1_COLOR_F ChooseContrastingTextColorForTest(const D2D1_COLOR_F& background) noexcept
{
    const double luminance = DxUi::Detail::RelativeLuminanceFromSrgb(
        std::clamp(background.r, 0.0f, 1.0f), std::clamp(background.g, 0.0f, 1.0f), std::clamp(background.b, 0.0f, 1.0f));
    const double blackContrast = DxUi::Detail::ContrastRatioFromRelativeLuminance(luminance, 0.0);
    const double whiteContrast = DxUi::Detail::ContrastRatioFromRelativeLuminance(1.0, luminance);
    return whiteContrast >= blackContrast ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
}

[[maybe_unused]] inline uint32_t PackColorForTest(const D2D1_COLOR_F& color) noexcept
{
    const auto packChannel = [](float component) noexcept { return static_cast<uint32_t>(std::lround(std::clamp(component, 0.0f, 1.0f) * 255.0f)); };

    return (packChannel(color.a) << 24u) | (packChannel(color.r) << 16u) | (packChannel(color.g) << 8u) | packChannel(color.b);
}

#if defined(ENABLE_TESTS)
[[nodiscard]] inline std::wstring ReadProviderStringProperty(IRawElementProviderSimple& provider, PROPERTYID propertyId, const char* context)
{
    VARIANT value{};
    VariantInit(&value);
    const auto clearValue = wil::scope_exit([&] { VariantClear(&value); });
    RequireSucceeded(provider.GetPropertyValue(propertyId, &value), context);
    Require(value.vt == VT_BSTR, context);
    return value.bstrVal ? std::wstring(value.bstrVal, SysStringLen(value.bstrVal)) : std::wstring{};
}

[[nodiscard]] inline LONG ReadProviderLongProperty(IRawElementProviderSimple& provider, PROPERTYID propertyId, const char* context)
{
    VARIANT value{};
    VariantInit(&value);
    const auto clearValue = wil::scope_exit([&] { VariantClear(&value); });
    RequireSucceeded(provider.GetPropertyValue(propertyId, &value), context);
    Require(value.vt == VT_I4, context);
    return value.lVal;
}

// The name of the element a fragment root's GetFocus reports (the call that answers the system's focus event for a window
// host), or an empty string when it reports none. The call counts like any other UI Automation makes.
[[nodiscard]] inline std::wstring ReadFocusedElementName(IRawElementProviderFragmentRoot& root)
{
    wil::com_ptr_nothrow<IRawElementProviderFragment> focused;
    RequireSucceeded(root.GetFocus(focused.put()), "the fragment root reports its focused element");
    if (! focused)
        return {};
    wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
    RequireSucceeded(focused.query_to(simple.put()), "the focused element is a UI Automation provider");
    return ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, "the focused element has a name");
}

[[nodiscard]] inline bool ReadProviderBoolProperty(IRawElementProviderSimple& provider, PROPERTYID propertyId, const char* context)
{
    VARIANT value{};
    VariantInit(&value);
    const auto clearValue = wil::scope_exit([&] { VariantClear(&value); });
    RequireSucceeded(provider.GetPropertyValue(propertyId, &value), context);
    Require(value.vt == VT_BOOL, context);
    return value.boolVal == VARIANT_TRUE;
}

[[nodiscard]] inline std::vector<std::wstring> ReadSelectionProviderNames(ISelectionProvider& selectionProvider, const char* context)
{
    SAFEARRAY* selectionArray = nullptr;
    RequireSucceeded(selectionProvider.GetSelection(&selectionArray), context);
    Require(selectionArray != nullptr, context);
    const auto destroyArray = wil::scope_exit([&] { SafeArrayDestroy(selectionArray); });

    LONG lowerBound = 0;
    LONG upperBound = -1;
    RequireSucceeded(SafeArrayGetLBound(selectionArray, 1, &lowerBound), context);
    RequireSucceeded(SafeArrayGetUBound(selectionArray, 1, &upperBound), context);

    std::vector<std::wstring> names;
    if (upperBound < lowerBound)
    {
        return names;
    }

    names.reserve(static_cast<size_t>(upperBound - lowerBound + 1));
    for (LONG index = lowerBound; index <= upperBound; ++index)
    {
        wil::com_ptr_nothrow<IUnknown> unknown;
        RequireSucceeded(SafeArrayGetElement(selectionArray, &index, unknown.put_void()), context);
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(unknown.query_to(simple.put()), context);
        names.push_back(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, context));
    }

    return names;
}

[[nodiscard]] inline std::vector<std::wstring> ReadProviderArrayNames(SAFEARRAY* providerArray, const char* context)
{
    Require(providerArray != nullptr, context);

    LONG lowerBound = 0;
    LONG upperBound = -1;
    RequireSucceeded(SafeArrayGetLBound(providerArray, 1, &lowerBound), context);
    RequireSucceeded(SafeArrayGetUBound(providerArray, 1, &upperBound), context);

    std::vector<std::wstring> names;
    if (upperBound < lowerBound)
    {
        return names;
    }

    names.reserve(static_cast<size_t>(upperBound - lowerBound + 1));
    for (LONG index = lowerBound; index <= upperBound; ++index)
    {
        wil::com_ptr_nothrow<IUnknown> unknown;
        RequireSucceeded(SafeArrayGetElement(providerArray, &index, unknown.put_void()), context);
        wil::com_ptr_nothrow<IRawElementProviderSimple> simple;
        RequireSucceeded(unknown.query_to(simple.put()), context);
        names.push_back(ReadProviderStringProperty(*simple.get(), UIA_NamePropertyId, context));
    }

    return names;
}

[[nodiscard]] inline wil::com_ptr_nothrow<IRawElementProviderFragment> GetProviderAtDipPoint(
    HWND hostHwnd, DxUi::WindowHost& host, IRawElementProviderFragmentRoot& rootProvider, float xDip, float yDip, const char* context)
{
    POINT pointPx{
        static_cast<LONG>(std::lround(host.DipsToPixels(xDip))),
        static_cast<LONG>(std::lround(host.DipsToPixels(yDip))),
    };
    Require(ClientToScreen(hostHwnd, &pointPx) != FALSE, context);

    wil::com_ptr_nothrow<IRawElementProviderFragment> provider;
    RequireSucceeded(rootProvider.ElementProviderFromPoint(static_cast<double>(pointPx.x), static_cast<double>(pointPx.y), provider.put()), context);
    Require(provider != nullptr, context);
    return provider;
}
#endif

class AttachedHostWindow final
{
public:
    AttachedHostWindow(const AttachedHostWindow&)            = delete;
    AttachedHostWindow& operator=(const AttachedHostWindow&) = delete;
    AttachedHostWindow(AttachedHostWindow&&)                 = delete;
    AttachedHostWindow& operator=(AttachedHostWindow&&)      = delete;

    explicit AttachedHostWindow(DxUi::WindowHost::PresentationMode presentationMode = DxUi::WindowHost::PresentationMode::HwndSwapChain)
    {
        EnableMotionForTest(_host);
        const ATOM windowClass = EnsureWindowClass();
        if (windowClass == 0)
        {
            const DWORD error = GetLastError();
            std::cerr << "    [WINDOW] register attached host class failed: Win32 error " << error << '\n';
        }
        Require(windowClass != 0, "attached host window class registered");
        const DWORD exStyle = DxUiTestWindowsCanActivateFlag() ? 0u : WS_EX_NOACTIVATE;
        HWND hwnd           = CreateWindowExW(
            exStyle, kWindowClassName, L"DxUiTestsHost", WS_OVERLAPPED, -32000, -32000, 320, 200, nullptr, nullptr, GetModuleHandleW(nullptr), this);
        if (! hwnd)
        {
            const DWORD error = GetLastError();
            std::cerr << "    [WINDOW] create attached host failed: Win32 error " << error << '\n';
        }
        Require(hwnd != nullptr, "attached host window created");
        _hwnd.reset(hwnd);
        Require(_host.Attach(_hwnd.get(), DxUi::WindowHost::AttachOptions{.presentationMode = presentationMode}), "attached host window host attached");
    }

    ~AttachedHostWindow()
    {
        std::cerr << "    [TRACE] attached host dtor: begin\n" << std::flush;
        _applicationMessageHandler = nullptr; // What it captured may already be gone; teardown messages go to DefWindowProc.
        _host.Detach();
        std::cerr << "    [TRACE] attached host dtor: host detached\n" << std::flush;
        _hwnd.reset();
        std::cerr << "    [TRACE] attached host dtor: hwnd reset\n" << std::flush;
    }

    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return _host;
    }

    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return _hwnd.get();
    }

    // Documentation captures size this offscreen window beyond small desktops. The
    // system otherwise clamps top-level windows to its default maximum tracking size.
    void AllowOuterSizeBeyondDesktop(SIZE outerSizePx) noexcept
    {
        _maximumTrackSizePx = outerSizePx;
    }

    // The application's own handling of a message HandleMessage left unhandled, as an application's window procedure does
    // after it: the handler returns true with the message's result, or false to leave the message to DefWindowProc.
    using ApplicationMessageHandler = std::function<bool(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)>;
    void SetApplicationMessageHandler(ApplicationMessageHandler handler)
    {
        _applicationMessageHandler = std::move(handler);
    }

    // How often another application took the foreground from this window: Windows then sends WM_ACTIVATEAPP with FALSE and
    // the thread of the window it activated. The host reacts as designed and releases its native text session, so state a
    // test observes across a pump is gone; see RunWhileForegroundHeld.
    [[nodiscard]] uint32_t ForegroundLossCount() const noexcept
    {
        return _foregroundLossCount;
    }

    // The thread of the window that took the foreground in the latest such loss, 0 when Windows named none.
    [[nodiscard]] DWORD LastForegroundThiefThreadId() const noexcept
    {
        return _lastForegroundThiefThreadId;
    }

    void PumpMessages(DWORD maximumDurationMs = INFINITE) const
    {
        const ULONGLONG started = GetTickCount64();
        MSG msg{};
        size_t processedCount = 0u;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
        {
            static_cast<void>(DispatchQueuedMessageForTest(msg));
            ++processedCount;
            // Keep a transient animation/timer burst from turning a test helper
            // pump into an unbounded loop.
            if (processedCount >= 4096u || (maximumDurationMs != INFINITE && GetTickCount64() - started >= maximumDurationMs))
            {
                break;
            }
        }
    }

private:
    static constexpr PCWSTR kWindowClassName = L"DxUiTests.AttachedHostWindow";

    static ATOM EnsureWindowClass()
    {
        static const ATOM atom = []() noexcept
        {
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc   = &AttachedHostWindow::WndProc;
            windowClass.hInstance     = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = kWindowClassName;
            return RegisterClassW(&windowClass);
        }();
        return atom;
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_NCCREATE)
        {
            const auto* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lp);
            auto* self               = static_cast<AttachedHostWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            return TRUE;
        }

        auto* self = reinterpret_cast<AttachedHostWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self && msg == WM_GETMINMAXINFO && self->_maximumTrackSizePx.cx > 0 && self->_maximumTrackSizePx.cy > 0)
        {
            auto* minMaxInfo             = reinterpret_cast<MINMAXINFO*>(lp);
            minMaxInfo->ptMaxTrackSize.x = (std::max)(minMaxInfo->ptMaxTrackSize.x, self->_maximumTrackSizePx.cx);
            minMaxInfo->ptMaxTrackSize.y = (std::max)(minMaxInfo->ptMaxTrackSize.y, self->_maximumTrackSizePx.cy);
            return 0;
        }
        if (self)
        {
            if (msg == WM_ACTIVATEAPP && wp == FALSE)
            {
                ++self->_foregroundLossCount;
                self->_lastForegroundThiefThreadId = static_cast<DWORD>(lp);
            }
            bool handled         = false;
            const LRESULT result = self->_host.HandleMessage(hwnd, msg, wp, lp, handled);
            if (msg == WM_NCDESTROY)
            {
                self->_host.Detach();
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            if (handled)
            {
                return result;
            }
            LRESULT applicationResult = 0;
            if (self->_applicationMessageHandler && self->_applicationMessageHandler(msg, wp, lp, applicationResult))
            {
                return applicationResult;
            }
        }

        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    wil::unique_hwnd _hwnd;
    DxUi::WindowHost _host;
    ApplicationMessageHandler _applicationMessageHandler;
    SIZE _maximumTrackSizePx{};
    uint32_t _foregroundLossCount      = 0u;
    DWORD _lastForegroundThiefThreadId = 0u;
};

// Names the process that owns a thread, for messages that say who took the foreground: "Claude.exe (process 8900)".
[[nodiscard]] inline std::string DescribeThreadProcessForTest(DWORD threadId)
{
    DWORD processId = 0u;
    if (const wil::unique_handle thread(OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId)); thread)
        processId = GetProcessIdOfThread(thread.get());
    if (processId == 0u)
        return "another application";
    if (const wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId)); process)
    {
        wchar_t path[MAX_PATH]{};
        DWORD length = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(process.get(), 0, path, &length) != FALSE)
        {
            const std::wstring fileName = std::filesystem::path(std::wstring_view(path, length)).filename().wstring();
            std::string name;
            for (const wchar_t ch : fileName)
                name.push_back(ch < 0x80 ? static_cast<char>(ch) : '?');
            return std::format("{} (process {})", name, processId);
        }
    }
    return std::format("process {}", processId);
}

// What became of a sequence run by RunWhileForegroundHeld.
struct ForegroundRunResult
{
    bool held           = false; // A run ended without another application having taken the foreground from its window.
    int runs            = 0;
    DWORD thiefThreadId = 0u; // The thread of the window that took the foreground in the latest lost run.

    // Why the sequence could not be observed, for the capability skip.
    [[nodiscard]] std::string SkipReason(const char* what) const
    {
        // Windows names the thread it activated; when it names none, the foreground window says who holds it now.
        DWORD thread = thiefThreadId;
        if (thread == 0u)
        {
            if (const HWND foreground = GetForegroundWindow())
                thread = GetWindowThreadProcessId(foreground, nullptr);
        }
        return std::format("{} needs its window to keep the foreground, but {} took it in each of {} runs", what, DescribeThreadProcessForTest(thread), runs);
    }
};

// What one run of a sequence found about the foreground: whether another application took it from one of the run's
// windows, and the thread of the window that did.
struct ForegroundAttempt
{
    bool lost           = false;
    DWORD thiefThreadId = 0u;
};

// Runs `attempt` until a run reports that no other application took the foreground from its windows, at most
// `maximumRuns` times. A takeover makes Windows deactivate a test window mid-sequence, and the host reacts as designed
// (it releases its native text session, stops announcing focus changes), so a desktop application retaking the
// foreground a few tens of milliseconds after a test window activated failed assertions that it says nothing about. The
// caller asserts on what the run that kept the foreground recorded, so its assertions are exactly those of a run without
// a thief, and a regression still fails: a run nobody took the foreground from is never repeated. `attempt` plays the
// whole sequence, with windows of its own when a later run must not inherit what an earlier one did to them, records
// what the caller asserts on, and returns what it found about the foreground. When another application takes the
// foreground in every run, the result says who, and ForegroundHeldOrSkip records the skip.
template <typename Attempt> [[nodiscard]] ForegroundRunResult RunUntilForegroundHeld(Attempt&& attempt, int maximumRuns = 5)
{
    ForegroundRunResult result{};
    while (result.runs < maximumRuns)
    {
        ++result.runs;
        const ForegroundAttempt run = attempt();
        if (! run.lost)
        {
            result.held = true;
            return result;
        }
        result.thiefThreadId = run.thiefThreadId;
        std::cerr << "    [FOREGROUND] " << DescribeThreadProcessForTest(result.thiefThreadId) << " took the foreground in run " << result.runs << " of "
                  << maximumRuns << '\n'
                  << std::flush;
    }
    return result;
}

// RunUntilForegroundHeld for `sequence`, the steps that move focus and pump messages in `window`: a run is lost when
// another application took the foreground from `window` during it, and after a lost run the window is activated again
// through the harness's activation helper (best effort). `sequence` must be safe to run again.
template <typename Sequence> [[nodiscard]] ForegroundRunResult RunWhileForegroundHeld(AttachedHostWindow& window, Sequence&& sequence, int maximumRuns = 5)
{
    int runs = 0;
    return RunUntilForegroundHeld(
        [&]
    {
        if (runs++ > 0)
            static_cast<void>(TryActivateDxUiTestWindow(window.Hwnd()));
        const uint32_t lossesBefore = window.ForegroundLossCount();
        sequence();
        return ForegroundAttempt{window.ForegroundLossCount() != lossesBefore, window.LastForegroundThiefThreadId()};
    },
        maximumRuns);
}

// True when a run kept the foreground; otherwise records the capability skip naming who took it and returns false.
[[nodiscard]] inline bool ForegroundHeldOrSkip(const ForegroundRunResult& run, const char* what)
{
    if (! run.held)
        SkipDxUiTest(run.SkipReason(what).c_str());
    return run.held;
}

// Delivers to `window` what Windows sends when another application's window takes the foreground from it, in the order
// Windows sends it, so a test reproduces the takeover without a second application. `thiefThreadId` is the thread of
// the window that took the foreground.
inline void SimulateForegroundTheftForTest(HWND window, DWORD thiefThreadId)
{
    static_cast<void>(SendMessageW(window, WM_NCACTIVATE, FALSE, 0));
    static_cast<void>(SendMessageW(window, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, 0), 0));
    static_cast<void>(SendMessageW(window, WM_ACTIVATEAPP, FALSE, static_cast<LPARAM>(thiefThreadId)));
    static_cast<void>(SendMessageW(window, WM_KILLFOCUS, 0, 0));
}

class ClipboardHostWindow final
{
public:
    ClipboardHostWindow(const ClipboardHostWindow&)            = delete;
    ClipboardHostWindow& operator=(const ClipboardHostWindow&) = delete;
    ClipboardHostWindow(ClipboardHostWindow&&)                 = delete;
    ClipboardHostWindow& operator=(ClipboardHostWindow&&)      = delete;

    ClipboardHostWindow() : _hwnd(CreateClipboardOwnerWindowForTest())
    {
        Require(_host.Attach(_hwnd.get()), "clipboard host attached");
    }

    ~ClipboardHostWindow()
    {
        _host.Detach();
    }

    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return _host;
    }

    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return _hwnd.get();
    }

private:
    wil::unique_hwnd _hwnd;
    DxUi::WindowHost _host;
};

class ExposedTextField final : public DxUi::TextField
{
public:
    using DxUi::TextField::ExportTextInputState;
    using DxUi::TextField::ImportTextInputState;
    using DxUi::TextField::TextField;
};

class ExposedButton final : public DxUi::Button
{
public:
    using DxUi::Button::Button;
    using DxUi::Button::IsPressed;
    using DxUi::Button::OnCaptureLost;
    using DxUi::Button::OnFocusChanged;
    using DxUi::Button::OnHoverChanged;
    using DxUi::Button::SetPressed;
    using DxUi::Button::Tick;
};

struct RecordingContextMenuInvocation
{
    size_t count = 0u;
    POINT lastPoint{};
    bool lastKeyboardInvocation = false;

    void Record(POINT screenPoint, bool keyboardInvocation) noexcept
    {
        ++count;
        lastPoint              = screenPoint;
        lastKeyboardInvocation = keyboardInvocation;
    }
};

[[nodiscard]] inline HWND FindTextInputBridgeEdit(HWND hostHwnd)
{
    if (HWND bridge = FindWindowExW(hostHwnd, nullptr, L"DxUiTextInputBridgeWindow", nullptr))
    {
        return bridge;
    }
    if (HWND richEdit = FindWindowExW(hostHwnd, nullptr, MSFTEDIT_CLASS, nullptr))
    {
        return richEdit;
    }
    return FindWindowExW(hostHwnd, nullptr, L"EDIT", nullptr);
}

constexpr std::wstring_view kLogicalNewlineClipboardTextForTest           = L"alpha\nbeta";
constexpr size_t kLogicalNewlineClipboardSelectionStartForTest            = 2u;
constexpr size_t kLogicalNewlineClipboardSelectionEndForTest              = 7u;
constexpr std::wstring_view kLogicalNewlineClipboardSelectedTextForTest   = L"pha\nb";
constexpr std::wstring_view kLogicalNewlineClipboardCutResultForTest      = L"aleta";
constexpr std::wstring_view kLogicalNewlinePasteClipboardTextForTest      = L"\r\nZ";
constexpr std::wstring_view kLogicalNewlinePasteInsertedTextForTest       = L"\nZ";
constexpr std::wstring_view kLogicalNewlinePasteResultForTest             = L"al\nZeta";
constexpr std::wstring_view kWrappedMultilineClipboardTextForTest         = L"alpha bravo charlie delta echo foxtrot golf hotel";
constexpr size_t kWrappedMultilineClipboardSelectionStartForTest          = 6u;
constexpr size_t kWrappedMultilineClipboardSelectionEndForTest            = 19u;
constexpr std::wstring_view kWrappedMultilineClipboardSelectedTextForTest = L"bravo charlie";
constexpr std::wstring_view kWrappedMultilinePasteClipboardTextForTest    = L"XYZ";
constexpr std::wstring_view kWrappedMultilinePasteResultForTest           = L"alpha XYZ delta echo foxtrot golf hotel";

[[nodiscard]] inline std::wstring RemoveSelectionForTest(std::wstring_view text, size_t selectionStart, size_t selectionEnd)
{
    std::wstring result(text.substr(0u, selectionStart));
    result.append(text.substr(selectionEnd));
    return result;
}

inline void ImportLogicalNewlineClipboardSelectionForTest(DxUi::WindowHost& host, ExposedTextField& field, const char* message)
{
    DxUi::TextInputState state;
    state.text                 = field.GetText();
    state.caretIndex           = kLogicalNewlineClipboardSelectionEndForTest;
    state.selectionAnchorIndex = kLogicalNewlineClipboardSelectionStartForTest;
    state.firstVisibleLine     = 0u;
    state.multiline            = true;
    Require(field.ImportTextInputState(host, state, false), message);
}

inline void RequireLogicalNewlineClipboardVisibleSelectionForTest(const DxUi::TextInputState& state, const char* context)
{
    Require(state.selectionAnchorIndex.has_value(), context);
    const size_t visibleSelectionStart = (std::min)(state.selectionAnchorIndex.value(), state.caretIndex);
    const size_t visibleSelectionEnd   = (std::max)(state.selectionAnchorIndex.value(), state.caretIndex);
    Require(visibleSelectionStart == kLogicalNewlineClipboardSelectionStartForTest && visibleSelectionEnd == kLogicalNewlineClipboardSelectionEndForTest,
            context);
}

inline void ImportWrappedMultilineClipboardSelectionForTest(DxUi::WindowHost& host, ExposedTextField& field, const char* message)
{
    DxUi::TextInputState state;
    state.text                 = field.GetText();
    state.caretIndex           = kWrappedMultilineClipboardSelectionEndForTest;
    state.selectionAnchorIndex = kWrappedMultilineClipboardSelectionStartForTest;
    state.firstVisibleLine     = 0u;
    state.multiline            = true;
    Require(field.ImportTextInputState(host, state, false), message);
}

inline void RequireWrappedMultilineClipboardVisibleSelectionForTest(const DxUi::TextInputState& state, const char* context)
{
    Require(state.selectionAnchorIndex.has_value(), context);
    const size_t visibleSelectionStart = (std::min)(state.selectionAnchorIndex.value(), state.caretIndex);
    const size_t visibleSelectionEnd   = (std::max)(state.selectionAnchorIndex.value(), state.caretIndex);
    Require(visibleSelectionStart == kWrappedMultilineClipboardSelectionStartForTest && visibleSelectionEnd == kWrappedMultilineClipboardSelectionEndForTest,
            context);
}

[[nodiscard]] inline POINT ClientPointToScreenForTest(HWND hwnd, POINT point, const char* context)
{
    Require(ClientToScreen(hwnd, &point) != FALSE, context);
    return point;
}

[[nodiscard]] inline std::optional<COMPOSITIONFORM> ReadTextInputCompositionFormForTest(HWND textInputHwnd)
{
    if (! textInputHwnd)
    {
        return std::nullopt;
    }

    HIMC inputContext = ImmGetContext(textInputHwnd);
    if (! inputContext)
    {
        return std::nullopt;
    }
    const auto releaseContext = wil::scope_exit([&] { ImmReleaseContext(textInputHwnd, inputContext); });

    COMPOSITIONFORM compositionForm{};
    if (ImmGetCompositionWindow(inputContext, &compositionForm) == FALSE)
    {
        return std::nullopt;
    }

    return compositionForm;
}

[[nodiscard]] inline std::optional<CANDIDATEFORM> ReadTextInputCandidateFormForTest(HWND textInputHwnd, DWORD index)
{
    if (! textInputHwnd)
    {
        return std::nullopt;
    }

    HIMC inputContext = ImmGetContext(textInputHwnd);
    if (! inputContext)
    {
        return std::nullopt;
    }
    const auto releaseContext = wil::scope_exit([&] { ImmReleaseContext(textInputHwnd, inputContext); });

    CANDIDATEFORM candidateForm{};
    if (ImmGetCandidateWindow(inputContext, index, &candidateForm) == FALSE)
    {
        return std::nullopt;
    }

    return candidateForm;
}

class SingleCellGridModel final : public DxUi::IGridModel
{
public:
    explicit SingleCellGridModel(DxUi::GridCellData cellData) : _cellData(std::move(cellData))
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"status";
        column.title    = L"Status";
        column.widthDip = 160.0f;
        return column;
    }

    void GetCellData(size_t /*rowIndex*/, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell = _cellData;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId == 0u ? std::optional<size_t>(0u) : std::nullopt;
    }

private:
    DxUi::GridCellData _cellData;
};

class MultiRowGridModel final : public DxUi::IGridModel
{
public:
    explicit MultiRowGridModel(size_t rowCount) : _rowCount(rowCount)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Name";
        column.widthDip = 180.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = std::format(L"Row {:02}", rowIndex);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId);
    }

private:
    size_t _rowCount = 0u;
};

class GroupedGridModel final : public DxUi::IGridModel
{
public:
    struct Group
    {
        uint64_t stableId = 0u;
        std::wstring title;
        size_t startRowIndex = 0u;
        size_t rowCount      = 0u;
        bool collapsed       = false;
    };

    explicit GroupedGridModel(size_t rowCount) : _rowCount(rowCount)
    {
    }

    void SetGroups(std::vector<Group> groups)
    {
        _groups = std::move(groups);
    }

    bool SetGroupCollapsed(uint64_t stableId, bool collapsed)
    {
        for (Group& group : _groups)
        {
            if (group.stableId == stableId)
            {
                group.collapsed = collapsed;
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool IsGroupCollapsed(uint64_t stableId) const
    {
        for (const Group& group : _groups)
        {
            if (group.stableId == stableId)
            {
                return group.collapsed;
            }
        }

        return false;
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Name";
        column.widthDip = 180.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = std::format(L"Row {:02}", rowIndex);
    }

    [[nodiscard]] size_t GetGroupCount() const noexcept override
    {
        return _groups.size();
    }

    [[nodiscard]] DxUi::GridGroupDesc GetGroup(size_t groupIndex) const override
    {
        const Group& group = _groups.at(groupIndex);
        return DxUi::GridGroupDesc{
            .stableId      = group.stableId,
            .title         = group.title,
            .startRowIndex = group.startRowIndex,
            .rowCount      = group.rowCount,
            .collapsed     = group.collapsed,
        };
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId);
    }

private:
    size_t _rowCount = 0u;
    std::vector<Group> _groups;
};

class LargeGridModel final : public DxUi::IGridModel
{
public:
    LargeGridModel(size_t rowCount, size_t columnCount, float columnWidthDip) : _rowCount(rowCount), _columnCount(columnCount), _columnWidthDip(columnWidthDip)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnCount;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = std::format(L"col-{}", columnIndex);
        column.title    = std::format(L"Column {}", columnIndex);
        column.widthDip = _columnWidthDip;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = std::format(L"R{}C{}", rowIndex, columnIndex);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId);
    }

private:
    size_t _rowCount      = 0u;
    size_t _columnCount   = 0u;
    float _columnWidthDip = 120.0f;
};

class MutableRowGridModel final : public DxUi::IGridModel
{
public:
    void SetRowIds(std::vector<uint64_t> rowIds)
    {
        _rowIds = std::move(rowIds);
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowIds.size();
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"name";
        column.title    = L"Name";
        column.widthDip = 180.0f;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t /*columnIndex*/, DxUi::GridCellData& outCell) const override
    {
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = std::format(L"Row {}", _rowIds.at(rowIndex));
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return _rowIds.at(rowIndex);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        for (size_t rowIndex = 0; rowIndex < _rowIds.size(); ++rowIndex)
        {
            if (_rowIds[rowIndex] == rowId)
            {
                return rowIndex;
            }
        }

        return std::nullopt;
    }

private:
    std::vector<uint64_t> _rowIds;
};

class ColumnLayoutGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 3u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        using namespace DxUi;

        switch (columnIndex)
        {
            case 0u:
                return GridColumnDesc{
                    .id            = L"name",
                    .title         = L"Name",
                    .widthDip      = 160.0f,
                    .minWidthDip   = 80.0f,
                    .kind          = GridColumnKind::Text,
                    .sortable      = true,
                    .multiline     = false,
                    .textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                };
            case 1u:
                return GridColumnDesc{
                    .id            = L"path",
                    .title         = L"Path",
                    .widthDip      = 220.0f,
                    .minWidthDip   = 100.0f,
                    .kind          = GridColumnKind::Text,
                    .sortable      = true,
                    .multiline     = false,
                    .textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                };
            default:
                return GridColumnDesc{
                    .id            = L"modified",
                    .title         = L"Modified",
                    .widthDip      = 180.0f,
                    .minWidthDip   = 96.0f,
                    .kind          = GridColumnKind::Text,
                    .sortable      = true,
                    .multiline     = false,
                    .textAlignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                };
        }
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        Require(rowIndex == 0u, "column-layout model uses one row");
        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.multiline = false;

        switch (columnIndex)
        {
            case 0u: outCell.text = L"alpha.txt"; return;
            case 1u: outCell.text = L"C:\\Data"; return;
            default: outCell.text = L"2026-03-15"; return;
        }
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId == 0u || rowId > GetRowCount())
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId - 1u);
    }
};

class RecordingGridDelegate : public DxUi::IGridDelegate
{
public:
    using DxUi::IGridDelegate::OnGridCheckboxToggled;
    using DxUi::IGridDelegate::OnGridContextMenu;
    using DxUi::IGridDelegate::OnGridGroupToggled;
    using DxUi::IGridDelegate::OnGridRowActivated;
    using DxUi::IGridDelegate::OnGridSelectionChanged;

    void OnGridSortRequested(const DxUi::GridSortSpec& sortSpec) override
    {
        ++sortRequestedCount;
        lastSortSpec = sortSpec;
    }

    void OnGridSelectionChanged(DxUi::Grid& sender) override
    {
        lastSelectionSender = &sender;
        ++selectionChangedCount;
    }

    void OnGridRowActivated(DxUi::Grid& sender, size_t rowIndex) override
    {
        lastActivatedSender = &sender;
        ++rowActivatedCount;
        lastActivatedRow = rowIndex;
    }

    void OnGridContextMenu(size_t rowIndex, POINT screenPoint) override
    {
        ++contextMenuCount;
        lastContextMenuRow   = rowIndex;
        lastContextMenuPoint = screenPoint;
    }

    void OnGridGroupToggled(uint64_t groupStableId, bool collapsed) override
    {
        ++groupToggleCount;
        lastGroupStableId  = groupStableId;
        lastGroupCollapsed = collapsed;
    }

    size_t sortRequestedCount = 0u;
    DxUi::GridSortSpec lastSortSpec{};
    DxUi::Grid* lastSelectionSender = nullptr;
    size_t selectionChangedCount    = 0u;
    DxUi::Grid* lastActivatedSender = nullptr;
    size_t rowActivatedCount        = 0u;
    size_t lastActivatedRow         = 0u;
    size_t contextMenuCount         = 0u;
    size_t lastContextMenuRow       = 0u;
    POINT lastContextMenuPoint{};
    size_t groupToggleCount    = 0u;
    uint64_t lastGroupStableId = 0u;
    bool lastGroupCollapsed    = false;
};

class ModelSwappingGridDelegate final : public RecordingGridDelegate
{
public:
    using RecordingGridDelegate::OnGridSelectionChanged;

    void OnGridSelectionChanged(DxUi::Grid& sender) override
    {
        RecordingGridDelegate::OnGridSelectionChanged(sender);
        sender.SetModel(nullptr);
    }
};

class CollapsibleGroupedGridDelegate : public RecordingGridDelegate
{
public:
    using RecordingGridDelegate::OnGridGroupToggled;

    explicit CollapsibleGroupedGridDelegate(GroupedGridModel& model) : _model(&model)
    {
    }

    void OnGridGroupToggled(uint64_t groupStableId, bool collapsed) override
    {
        RecordingGridDelegate::OnGridGroupToggled(groupStableId, collapsed);
        Require(_model->SetGroupCollapsed(groupStableId, collapsed), "grouped grid delegate updates the requested group collapse state");
    }

private:
    GroupedGridModel* _model = nullptr;
};

class CheckboxGridModel final : public DxUi::IGridModel
{
public:
    struct Row
    {
        std::wstring label;
        bool checked = false;
        bool enabled = true;
    };

    explicit CheckboxGridModel(size_t checkboxColumnIndex) : _checkboxColumnIndex(checkboxColumnIndex)
    {
    }

    void SetRows(std::vector<Row> rows)
    {
        _rows = std::move(rows);
    }

    [[nodiscard]] bool SetChecked(size_t rowIndex, size_t columnIndex, bool checked)
    {
        if (rowIndex >= _rows.size() || columnIndex != _checkboxColumnIndex)
        {
            return false;
        }
        _rows[rowIndex].checked = checked;
        return true;
    }

    [[nodiscard]] bool IsChecked(size_t rowIndex) const
    {
        return _rows.at(rowIndex).checked;
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows.size();
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        if (columnIndex == _checkboxColumnIndex)
        {
            column.id        = L"enabled";
            column.title     = L"Enabled";
            column.widthDip  = 120.0f;
            column.multiline = false;
            return column;
        }

        column.id        = L"name";
        column.title     = L"Name";
        column.widthDip  = 180.0f;
        column.multiline = false;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        const Row& row = _rows.at(rowIndex);
        if (columnIndex == _checkboxColumnIndex)
        {
            outCell.kind      = DxUi::GridCellKind::Checkbox;
            outCell.text      = L"Enabled";
            outCell.checked   = row.checked;
            outCell.enabled   = row.enabled;
            outCell.multiline = false;
            return;
        }

        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.text      = row.label;
        outCell.multiline = false;
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId == 0u || rowId > _rows.size())
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId - 1u);
    }

private:
    std::vector<Row> _rows;
    size_t _checkboxColumnIndex = 0u;
};

class LargeCheckboxGridModel final : public DxUi::IGridModel
{
public:
    LargeCheckboxGridModel(size_t rowCount, size_t checkboxColumnIndex) : _rowCount(rowCount), _checkboxColumnIndex(checkboxColumnIndex)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        if (columnIndex == _checkboxColumnIndex)
        {
            column.id        = L"enabled";
            column.title     = L"Enabled";
            column.widthDip  = 120.0f;
            column.multiline = false;
            return column;
        }

        column.id        = L"name";
        column.title     = L"Name";
        column.widthDip  = 180.0f;
        column.multiline = false;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        if (columnIndex == _checkboxColumnIndex)
        {
            outCell.kind      = DxUi::GridCellKind::Checkbox;
            outCell.text      = L"Enabled";
            outCell.checked   = (rowIndex % 2u) == 0u;
            outCell.enabled   = true;
            outCell.multiline = false;
            return;
        }

        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.text      = std::format(L"Rule {:05}", rowIndex);
        outCell.multiline = false;
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId == 0u || rowId > _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId - 1u);
    }

private:
    size_t _rowCount            = 0u;
    size_t _checkboxColumnIndex = 0u;
};

class RecordingCheckboxGridDelegate final : public RecordingGridDelegate
{
public:
    using RecordingGridDelegate::OnGridCheckboxToggled;

    explicit RecordingCheckboxGridDelegate(CheckboxGridModel& model) : _model(&model)
    {
    }

    void OnGridCheckboxToggled(size_t rowIndex, size_t columnIndex, bool checked) override
    {
        ++toggleCount;
        lastToggleRow     = rowIndex;
        lastToggleColumn  = columnIndex;
        lastToggleChecked = checked;
        Require(_model->SetChecked(rowIndex, columnIndex, checked), "checkbox grid delegate updates the requested checkbox cell");
    }

    size_t toggleCount      = 0u;
    size_t lastToggleRow    = 0u;
    size_t lastToggleColumn = 0u;
    bool lastToggleChecked  = false;

private:
    CheckboxGridModel* _model = nullptr;
};

class DedicatedCheckboxColumnGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        if (columnIndex == 0u)
        {
            column.id            = L"enabled";
            column.title         = L"";
            column.kind          = DxUi::GridColumnKind::Checkbox;
            column.widthDip      = 36.0f;
            column.minWidthDip   = 36.0f;
            column.sortable      = false;
            column.multiline     = false;
            column.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
            return column;
        }

        column.id        = L"name";
        column.title     = L"Name";
        column.widthDip  = 180.0f;
        column.multiline = false;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        Require(rowIndex == 0u, "dedicated checkbox model uses one row");
        if (columnIndex == 0u)
        {
            outCell.kind = DxUi::GridCellKind::Checkbox;
            outCell.text.clear();
            outCell.checked   = _checked;
            outCell.enabled   = true;
            outCell.multiline = false;
            return;
        }

        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.text      = L"Alpha";
        outCell.multiline = false;
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId == 1u ? std::optional<size_t>(0u) : std::nullopt;
    }

    void SetChecked(bool checked) noexcept
    {
        _checked = checked;
    }

    [[nodiscard]] bool IsChecked() const noexcept
    {
        return _checked;
    }

private:
    bool _checked = false;
};

class DedicatedCheckboxGridDelegate final : public RecordingGridDelegate
{
public:
    using RecordingGridDelegate::OnGridCheckboxToggled;

    explicit DedicatedCheckboxGridDelegate(DedicatedCheckboxColumnGridModel& model) : _model(&model)
    {
    }

    void OnGridCheckboxToggled(size_t rowIndex, size_t columnIndex, bool checked) override
    {
        ++toggleCount;
        lastToggleRow     = rowIndex;
        lastToggleColumn  = columnIndex;
        lastToggleChecked = checked;
        Require(rowIndex == 0u && columnIndex == 0u, "dedicated checkbox toggle targets the dedicated column");
        _model->SetChecked(checked);
    }

    size_t toggleCount      = 0u;
    size_t lastToggleRow    = 0u;
    size_t lastToggleColumn = 0u;
    bool lastToggleChecked  = false;

private:
    DedicatedCheckboxColumnGridModel* _model = nullptr;
};

class StateImageColumnGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 2u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        if (columnIndex == 0u)
        {
            column.id            = L"state";
            column.title         = L"";
            column.kind          = DxUi::GridColumnKind::StateImage;
            column.widthDip      = 40.0f;
            column.minWidthDip   = 40.0f;
            column.sortable      = false;
            column.multiline     = false;
            column.textAlignment = DWRITE_TEXT_ALIGNMENT_CENTER;
            return column;
        }

        column.id        = L"name";
        column.title     = L"Name";
        column.widthDip  = 180.0f;
        column.multiline = false;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        Require(rowIndex == 0u, "state-image model uses one row");
        if (columnIndex == 0u)
        {
            outCell.kind = DxUi::GridCellKind::IconText;
            outCell.text.clear();
            outCell.iconText  = L"!";
            outCell.multiline = false;
            return;
        }

        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.text      = L"Warning";
        outCell.multiline = false;
    }

    [[nodiscard]] uint64_t GetStableRowId(size_t rowIndex) const noexcept override
    {
        return static_cast<uint64_t>(rowIndex + 1u);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId == 1u ? std::optional<size_t>(0u) : std::nullopt;
    }
};

class MutableTreeModel final : public DxUi::ITreeModel
{
public:
    void SetVisibleItems(std::vector<DxUi::TreeItemData> items)
    {
        _items = std::move(items);
    }

    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return _items.size();
    }

    void GetVisibleItem(size_t visibleIndex, DxUi::TreeItemData& outItem) const override
    {
        outItem = _items.at(visibleIndex);
    }

private:
    std::vector<DxUi::TreeItemData> _items;
};

class RecordingTreeDelegate final : public DxUi::ITreeDelegate
{
public:
    void OnTreeFocusedItemChanged(DxUi::Tree&, std::optional<uint64_t> itemId) override
    {
        ++focusChangedCount;
        lastFocusedItemId = itemId;
    }

    void OnTreeSelectionChanged(uint64_t itemId) override
    {
        ++selectionChangedCount;
        lastSelectedItemId = itemId;
        callOrder += 'P';
        if (observedTree)
            idsSeenByPrimaryCallback = observedTree->GetSelectedItemIds();
    }

    void OnTreeSelectionSetChanged(std::span<const uint64_t> selectedItemIds) override
    {
        ++selectionSetChangedCount;
        lastSelectionSet.assign(selectedItemIds.begin(), selectedItemIds.end());
        callOrder += 'S';
        if (observedTree)
            idsSeenBySetCallback = observedTree->GetSelectedItemIds();
    }

    void OnTreeItemInvoked(uint64_t itemId) override
    {
        ++invokedCount;
        lastInvokedItemId = itemId;
    }

    void OnTreeToggleExpanded(uint64_t itemId, bool expanded) override
    {
        ++toggleCount;
        lastToggledItemId = itemId;
        lastExpandedState = expanded;
    }

    void OnTreeContextMenu(uint64_t itemId, POINT screenPoint) override
    {
        ++contextMenuCount;
        lastContextMenuItemId = itemId;
        lastContextMenuPoint  = screenPoint;
    }

    void OnTreeReorder(const DxUi::TreeDrop& drop) override
    {
        ++reorderCount;
        lastDrop = drop;
        if (observedTree)
            idsSeenByReorderCallback = observedTree->GetSelectedItemIds();
    }

    size_t selectionChangedCount    = 0u;
    size_t selectionSetChangedCount = 0u;
    size_t focusChangedCount        = 0u;
    size_t invokedCount             = 0u;
    size_t toggleCount              = 0u;
    size_t contextMenuCount         = 0u;
    size_t reorderCount             = 0u;
    // The set of the latest OnTreeSelectionSetChanged, and what the tree itself reported while the callbacks ran when
    // `observedTree` is set: a delegate reads a finished selection, never a half-applied one. `callOrder` is P for each
    // OnTreeSelectionChanged and S for each OnTreeSelectionSetChanged, in call order.
    std::vector<uint64_t> lastSelectionSet;
    const DxUi::Tree* observedTree = nullptr;
    std::vector<uint64_t> idsSeenByPrimaryCallback;
    std::vector<uint64_t> idsSeenBySetCallback;
    std::vector<uint64_t> idsSeenByReorderCallback;
    std::string callOrder;
    DxUi::TreeDrop lastDrop{};
    uint64_t lastSelectedItemId = 0u;
    std::optional<uint64_t> lastFocusedItemId;
    uint64_t lastInvokedItemId     = 0u;
    uint64_t lastToggledItemId     = 0u;
    uint64_t lastContextMenuItemId = 0u;
    bool lastExpandedState         = false;
    POINT lastContextMenuPoint{};
};

using TreeIds = std::vector<uint64_t>;

[[nodiscard]] inline std::string DescribeTreeIds(const TreeIds& ids)
{
    std::string text = "{";
    for (size_t index = 0u; index < ids.size(); ++index)
    {
        text += (index == 0u ? "" : ",") + std::to_string(ids[index]);
    }
    return text + "}";
}

// Fails naming both selections, so a broken gesture shows what it selected.
inline void RequireTreeIds(const TreeIds& actual, const TreeIds& expected, const char* context)
{
    if (actual != expected)
    {
        const std::string message = std::string(context) + ": expected " + DescribeTreeIds(expected) + " but the tree holds " + DescribeTreeIds(actual);
        Require(false, message.c_str());
    }
}

// Rows "Row N" at the top level, for ids firstId to firstId + count - 1.
[[nodiscard]] inline std::vector<DxUi::TreeItemData> FlatTreeItems(uint64_t count, uint64_t firstId = 1u)
{
    std::vector<DxUi::TreeItemData> items;
    for (uint64_t id = firstId; id < firstId + count; ++id)
    {
        items.push_back(DxUi::TreeItemData{.id = id, .text = L"Row " + std::to_wstring(id)});
    }
    return items;
}

class PaintTraceControl final : public DxUi::Control
{
public:
    PaintTraceControl(std::vector<std::string>& events, std::string paintTag, std::string overlayTag = {})
        : _events(&events),
          _paintTag(std::move(paintTag)),
          _overlayTag(std::move(overlayTag))
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
        _events->push_back(_paintTag);
    }

    void PaintOverlay(DxUi::WindowHost& /*host*/) const override
    {
        if (! _overlayTag.empty())
        {
            _events->push_back(_overlayTag);
        }
    }

private:
    std::vector<std::string>* _events = nullptr;
    std::string _paintTag;
    std::string _overlayTag;
};

class PaintTraceComboBox final : public DxUi::ComboBox
{
public:
    explicit PaintTraceComboBox(std::vector<std::string>& events) : _events(&events)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
        _events->push_back("combo-base");
    }

    void PaintOverlay(DxUi::WindowHost& /*host*/) const override
    {
        if (GetHitBounds().bottom > GetBounds().bottom)
        {
            _events->push_back("combo-popup");
        }
    }

private:
    std::vector<std::string>* _events = nullptr;
};

class EmptyGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 0u;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 1u;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        DxUi::GridColumnDesc column;
        column.id       = L"empty";
        column.title    = L"Empty";
        column.widthDip = 160.0f;
        return column;
    }

    void GetCellData(size_t /*rowIndex*/, size_t /*columnIndex*/, DxUi::GridCellData& /*outCell*/) const override
    {
        ++cellAccessCount;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t /*rowId*/) const noexcept override
    {
        return std::nullopt;
    }

    mutable size_t cellAccessCount = 0u;
};

struct TrackingControlState
{
    size_t focusGainCount  = 0u;
    size_t focusLossCount  = 0u;
    size_t hoverEnterCount = 0u;
    size_t hoverLeaveCount = 0u;
    size_t mouseDownCount  = 0u;
    size_t mouseMoveCount  = 0u;
    size_t mouseLeaveCount = 0u;
    size_t mouseUpCount    = 0u;
    size_t mouseWheelCount = 0u;
};

class TrackingControl final : public DxUi::Control
{
public:
    explicit TrackingControl(TrackingControlState& state) : _state(&state)
    {
        SetFocusable(true);
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseMove(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, UINT /*modifiers*/) override
    {
        ++_state->mouseMoveCount;
        return true;
    }

    bool OnMouseLeave(DxUi::WindowHost& host) override
    {
        ++_state->mouseLeaveCount;
        return Control::OnMouseLeave(host);
    }

    bool OnMouseDown(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        ++_state->mouseDownCount;
        return ! rightButton;
    }

    bool OnMouseUp(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, bool /*rightButton*/, UINT /*modifiers*/) override
    {
        ++_state->mouseUpCount;
        return true;
    }

    bool OnMouseWheel(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, float /*wheelDelta*/, UINT /*modifiers*/) override
    {
        ++_state->mouseWheelCount;
        return true;
    }

protected:
    void OnFocusChanged(DxUi::WindowHost& host, bool focused) override
    {
        if (focused)
        {
            ++_state->focusGainCount;
        }
        else
        {
            ++_state->focusLossCount;
        }
        Control::OnFocusChanged(host, focused);
    }

    void OnHoverChanged(DxUi::WindowHost& host, bool hovered) override
    {
        if (hovered)
        {
            ++_state->hoverEnterCount;
        }
        else
        {
            ++_state->hoverLeaveCount;
        }
        Control::OnHoverChanged(host, hovered);
    }

private:
    TrackingControlState* _state = nullptr;
};

// A control that focuses itself in an input handler leaves itself alone once the focus callbacks destroyed it. `make`
// adds the control under a root panel beside a focused button and returns it; the host's focus-changed callback replaces
// the whole tree the first time that control gains the focus, and `input` then runs the handler under test against it.
// A handler that touched the destroyed control is caught by AddressSanitizer; every build checks that the replacement
// happened and left nothing focused.
template <typename Make, typename Input> void RequireFocusReplacementLeavesControlAlone(std::string_view name, Make&& make, Input&& input)
{
    using namespace DxUi;
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* before = root->AddChild<Button>(L"Focused before");
    before->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 24.0f));
    Control* const target = make(*root);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 480.0f, 360.0f));
    host.SetFocusControl(before);
    Require(host.GetFocusControl() == before, std::format("{}: the button is focused first", name).c_str());
    bool replaced = false;
    host.SetOnFocusChanged([&](Control* focused)
    {
        if (focused == target && ! replaced)
        {
            replaced = true;
            host.SetRoot(std::make_unique<Panel>());
        }
    });
    input(host, *target);
    host.SetOnFocusChanged({});
    Require(replaced, std::format("{}: focusing the control ran the callback that replaced the controls", name).c_str());
    Require(host.GetFocusControl() == nullptr, std::format("{}: nothing of the replaced controls stays focused", name).c_str());
}
