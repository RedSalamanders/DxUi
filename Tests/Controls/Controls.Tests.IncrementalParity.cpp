#include "Controls.Tests.IncrementalParity.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.DxUiTestMovedControls.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "Support.Tests.IncrementalParity.h"
#include "Support/RetainedCaches.Tests.Hooks.h"

#include <windowsx.h>

// The incremental-versus-fresh oracle on the native window host, WARP, HwndSwapChain, hidden window (no activation).
//
// What the native side reads today: the back buffer of a full frame (DebugCaptureBitmap renders the whole window and
// copies the back buffer before Present). Every native frame is a full frame today, because every control invalidation is
// full-window, so this covers what the host presents now. It does NOT read a presented image: a partial frame (W4.6/W4.9)
// leaves pixels outside its dirty rectangle in the back buffer that are two frames old until Present1 copies them, so a
// back-buffer read after a partial frame proves nothing. ReadNativeFrame is the seam: W0.6 replaces it with a non-activating
// capture of the presented, composed window, and partial-frame scripts must use that reader.
namespace
{
using namespace DxUi;
using namespace IncrementalParity;

constexpr D2D1_SIZE_F kSceneDip{660.0f, 480.0f};
constexpr uint64_t kClockStartMs = 1'000'000u;

// W0.6 seam: the native frame reader. Today: a full-frame back-buffer read (see above).
[[nodiscard]] bool ReadNativeFrame(WindowHost& host, WindowHostBitmapCapture& capture)
{
    return host.DebugCaptureBitmap(capture, true);
}

struct NativeParityScene final
{
    static constexpr uint32_t kKnownDifferences = kNativeUiaBoundsAfterSetBounds;
    static constexpr bool kCrossStepCapture     = false;
    AttachedHostWindow window{WindowHost::PresentationMode::HwndSwapChain, true};
    std::array<Control*, kParitySlots> slots{};
    std::vector<std::shared_ptr<void>> models;
    bool dragging  = false;
    uint64_t clock = kClockStartMs;

    NativeParityScene()
    {
        DxUiTestMoved::ConfigureHostPlace(window, 96u, false, Density::Standard, kSceneDip);
        Host().DebugSetAnimationClockForTest(clock);
    }
    ~NativeParityScene()
    {
        Host().DebugSetAnimationClockForTest(std::nullopt);
    }
    NativeParityScene(const NativeParityScene&)            = delete;
    NativeParityScene& operator=(const NativeParityScene&) = delete;

    [[nodiscard]] ControlHost& Host() noexcept
    {
        return window.Host();
    }
    void SetRoot(std::unique_ptr<Control> root)
    {
        Host().SetRoot(std::move(root));
    }
    [[nodiscard]] POINT ToPixels(D2D1_POINT_2F dip)
    {
        return {static_cast<LONG>(std::lround(Host().DipsToPixels(dip.x))), static_cast<LONG>(std::lround(Host().DipsToPixels(dip.y)))};
    }
    void Pointer(ParityPointer kind, D2D1_POINT_2F dip, float wheelDelta = 0.0f)
    {
        POINT px         = ToPixels(dip);
        const LPARAM at  = MAKELPARAM(px.x, px.y);
        const HWND hwnd  = window.Hwnd();
        const WPARAM btn = dragging ? MK_LBUTTON : 0u;
        switch (kind)
        {
            case ParityPointer::Move: SendMessageW(hwnd, WM_MOUSEMOVE, btn, at); break;
            case ParityPointer::Down: SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, at); break;
            case ParityPointer::Up: SendMessageW(hwnd, WM_LBUTTONUP, 0u, at); break;
            case ParityPointer::Wheel:
                ClientToScreen(hwnd, &px);
                SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0u, static_cast<short>(wheelDelta)), MAKELPARAM(px.x, px.y));
                break;
            case ParityPointer::Leave: SendMessageW(hwnd, WM_MOUSELEAVE, 0u, 0); break;
        }
    }
    void Key(UINT virtualKey)
    {
        SendMessageW(window.Hwnd(), WM_KEYDOWN, virtualKey, 1);
        SendMessageW(window.Hwnd(), WM_KEYUP, virtualKey, static_cast<LPARAM>(0xC0000001u));
    }
    void Character(wchar_t character)
    {
        SendMessageW(window.Hwnd(), WM_CHAR, character, 1);
    }
    void SetDpi(UINT dpi)
    {
        DxUiTestMoved::ConfigureHostPlace(window, dpi, Host().GetTheme().dark, Density::Standard, kSceneDip);
        // ConfigureHostPlace applies its own theme; restore the scene's pinned one.
        Host().SetTheme(ParityTheme(Host().GetTheme().dark));
    }
    void Advance(uint64_t milliseconds)
    {
        clock += milliseconds;
        Host().DebugSetAnimationClockForTest(clock);
        static_cast<void>(Host().DebugAnimationTickForTest(clock));
    }
    [[nodiscard]] ParityFrame Capture(bool fresh)
    {
        if (fresh)
        {
            Host().DebugDropRetainedStateForTest();
            std::vector<RetainedCacheCount> counts;
            RetainedCacheAccess::Count(Host(), counts);
            RequireRetainedCachesEmpty(counts, "native drop");
        }
        window.PumpMessages(50u);
        WindowHostBitmapCapture capture;
        bool read = false;
        for (int attempt = 0; attempt < 64 && ! read; ++attempt)
        {
            read = ReadNativeFrame(Host(), capture);
            if (! read)
                window.PumpMessages(10u);
        }
        Require(read, "the native parity frame is read");
        ParityFrame frame;
        frame.widthPx  = capture.widthPx;
        frame.heightPx = capture.heightPx;
        frame.dpi      = Host().GetDpi();
        frame.bgra     = std::move(capture.bgraPixels);
        frame.controls = CollectControls(Host());
        wil::com_ptr_nothrow<IRawElementProviderFragmentRoot> provider;
        provider.attach(Host().DebugCreateAccessibilityProvider());
        Require(provider != nullptr, "the parity window exposes its accessibility provider");
        frame.uia = DescribeUia(provider.get());
        return frame;
    }
};

[[nodiscard]] std::unique_ptr<NativeParityScene> MakeNativeScene()
{
    return std::make_unique<NativeParityScene>();
}

constexpr std::string_view kReplay = "DxUi.ControlTests.exe --suite=Rendering --test=<the test named above>";

void TestNativeIncrementalParityLayout()
{
    RunParityScript(LayoutScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

void TestNativeIncrementalParityInteraction()
{
    RunParityScript(InteractionScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

void TestNativeIncrementalParityRich()
{
    RunParityScript(RichScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// The gate's negative control: a cache that forgets one invalidation must show as a difference, at that control.
void TestNativeIncrementalParityDetectsAStaleCache()
{
    constexpr std::array<uint64_t, 1u> seed{0x5EED00D1u};
    const ParityScript<NativeParityScene> script{"stale-label",
                                                 seed,
                                                 1u,
                                                 &BuildControlsScene<NativeParityScene>,
                                                 [](NativeParityScene& scene, Rng&, size_t)
    { static_cast<Label*>(scene.slots[kMnemonicLabel])->SetText(L"Renamed with mnemonic"); }};
    Require(RunParityScriptOnce(script, seed[0], MakeNativeScene).empty(), "without the injection the label edit renders identically");
    Detail::SetStaleLabelLayoutInjectionForTest(true);
    const auto restore           = wil::scope_exit([] { Detail::SetStaleLabelLayoutInjectionForTest(false); });
    const std::string difference = RunParityScriptOnce(script, seed[0], MakeNativeScene);
    std::cout << "  native stale-cache negative control: " << difference << '\n';
    Require(difference.starts_with("step 1 ") && difference.find("pixels differ") != std::string::npos && difference.find("root/1 (") != std::string::npos,
            "an injected stale Label layout yields a nonzero BGRA difference at the label");
}

// After the drop every registered cache reads 0, and the scenes populate the rows they exercise beforehand.
void TestNativeRetainedCachesDropToZero()
{
    std::vector<RetainedCacheCount> populated;
    for (auto build : {&BuildControlsScene<NativeParityScene>, &BuildRichScene<NativeParityScene>})
    {
        NativeParityScene scene;
        build(scene);
        if (scene.slots[kTree])
        {
            scene.Host().SetFocusControl(scene.slots[kField], false);
            scene.Pointer(ParityPointer::Move, CenterOf(*scene.slots[kButton]));
        }
        static_cast<void>(scene.Capture(false));
        std::vector<RetainedCacheCount> counts;
        RetainedCacheAccess::Count(scene.Host(), counts);
        if (populated.empty())
            populated = counts;
        else
            for (size_t index = 0u; index < counts.size(); ++index)
                populated[index].entries += counts[index].entries;
        static_cast<void>(scene.Capture(true)); // Requires every count to read 0 right after the drop.
    }
    std::string unpopulated;
    for (const auto& count : populated)
    {
        std::cout << "  native retained cache " << count.name << ": " << count.entries << " before the drop\n";
        const bool exempt = std::ranges::find(kRowsNotPopulatedByScenes, count.name) != kRowsNotPopulatedByScenes.end();
        if (! exempt && count.entries == 0u)
            unpopulated += std::string(count.name) + ' ';
    }
    if (! unpopulated.empty())
        std::cerr << "  unpopulated rows: " << unpopulated << '\n';
    Require(unpopulated.empty(), "every registered cache the parity scenes exercise is populated before the drop");
}
} // namespace

void RunIncrementalParityTests()
{
    DXUI_RUN_TEST(TestNativeRetainedCachesDropToZero);
    DXUI_RUN_TEST(TestNativeIncrementalParityDetectsAStaleCache);
    DXUI_RUN_TEST(TestNativeIncrementalParityLayout);
    DXUI_RUN_TEST(TestNativeIncrementalParityInteraction);
    DXUI_RUN_TEST(TestNativeIncrementalParityRich);
}
