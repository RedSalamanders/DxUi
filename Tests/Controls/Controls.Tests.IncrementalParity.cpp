#include "Controls.Tests.IncrementalParity.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.DxUiTestMovedControls.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Support.Tests.ComboBoxPopupLayout.h"
#include "Support.Tests.IncrementalParity.h"
#include "Support.Tests.TextFieldPaint.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
// Scenarios are never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
#include "Embedded.Tests.PerformanceFixtureFx10.h"
#include "Embedded.Tests.PerformanceFixtureFx12.h"
#pragma warning(pop)

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

void TestNativeIncrementalParityRichBlocks()
{
    RunParityScript(RichBlocksScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// W2.3: formulas, diagrams, raster images and SVG rasters carried across edits against a fresh preparation of every object.
void TestNativeIncrementalParityRichObjects()
{
    RunParityScript(RichObjectsScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// W2.3 negative control: a deliberately wrong carry key (the formula source ignored) must show as a difference when the
// script edits inside a formula.
void TestNativeIncrementalParityRichObjectsDetectsAWrongCarry()
{
    DxUi::RichText::Detail::testRichMathCarryIgnoresSource = true;
    const auto restore                                     = wil::scope_exit([] { DxUi::RichText::Detail::testRichMathCarryIgnoresSource = false; });
    const std::string difference                           = RunParityScriptOnce(RichObjectsScript<NativeParityScene>(), kRichObjectsSeeds[0], MakeNativeScene);
    std::cout << "  native wrong-carry negative control: " << difference << '\n';
    Require(difference.starts_with("step 4 ") && difference.find("inside the first formula") != std::string::npos,
            "a formula carried under a wrong key differs from a fresh preparation at the edit inside it");
}

// W2.2: the ComboBox popup's retained widths and placement against a fresh measurement, on the window host (where the
// popup is painted into the host window as an overlay).
void TestNativeIncrementalParityComboPopup()
{
    RunParityScript(ComboBoxPopupLayoutTests::ComboPopupScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// W2.7: the TextField's retained multiline layout, line metrics and single-line layout against a fresh layout, on the window
// host.
void TestNativeIncrementalParityTextField()
{
    RunParityScript(TextFieldPaintTests::TextFieldScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
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

// A performance fixture replayed by the oracle: its Scenario builds on the scene's host and each oracle step k applies the
// fixture's step k - 1 through this port (Tests/README.md, "Performance fixtures"). Pointer and key input arrive as the
// window messages the fixture's window runner sends; Resize sets the scene's client area and DPI.
class FixtureParityPort final : public PerformanceFixtures::HostPort
{
public:
    FixtureParityPort(NativeParityScene& scene, const PerformanceFixtures::Definition& definition, const PerformanceFixtures::Variant& variant)
        : _scene(scene),
          _definition(definition),
          _scenario(definition.create())
    {
        _context.emplace(PerformanceFixtures::Context{PerformanceFixtures::Host::Window, variant, scene.Host(), *this});
    }
    FixtureParityPort(const FixtureParityPort&)            = delete;
    FixtureParityPort& operator=(const FixtureParityPort&) = delete;
    FixtureParityPort(FixtureParityPort&&)                 = delete;
    FixtureParityPort& operator=(FixtureParityPort&&)      = delete;
    ~FixtureParityPort() override                          = default;
    void Build()
    {
        _scene.window.AllowOuterSizeBeyondDesktop(SIZE{8192, 8192});
        Resize(_context->variant.width, _context->variant.height, _context->variant.dpi);
        RequireSucceeded(_scenario->Build(*_context), "the fixture builds on the oracle scene");
    }
    void Step(size_t index)
    {
        const auto at = PerformanceFixtures::StepAt(_definition, index);
        Require(at.has_value(), "the oracle step is a fixture step");
        NoteStep(_definition.phases[at->phase].name);
        _scenario->Step(*_context, at->phase, at->step);
    }
    bool Pointer(const PointerEvent& event) override
    {
        const LPARAM at = MAKELPARAM(static_cast<short>(std::lround(event.xPixels)), static_cast<short>(std::lround(event.yPixels)));
        const HWND hwnd = _scene.window.Hwnd();
        switch (event.action)
        {
            case PointerAction::Move: SendMessageW(hwnd, WM_MOUSEMOVE, 0u, at); break;
            case PointerAction::Down: SendMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, at); break;
            case PointerAction::Up: SendMessageW(hwnd, WM_LBUTTONUP, 0u, at); break;
            case PointerAction::MiddleDown: SendMessageW(hwnd, WM_MBUTTONDOWN, MK_MBUTTON, at); break;
            case PointerAction::MiddleUp: SendMessageW(hwnd, WM_MBUTTONUP, 0u, at); break;
            case PointerAction::Cancel: SendMessageW(hwnd, WM_CANCELMODE, 0u, 0); break;
            case PointerAction::Leave: SendMessageW(hwnd, WM_MOUSELEAVE, 0u, 0); break;
            case PointerAction::Wheel:
            {
                POINT screen{GET_X_LPARAM(at), GET_Y_LPARAM(at)};
                ClientToScreen(hwnd, &screen);
                SendMessageW(hwnd,
                             WM_MOUSEWHEEL,
                             MAKEWPARAM(0u, static_cast<short>(std::lround(event.wheelDelta))),
                             MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y)));
                break;
            }
        }
        return true;
    }
    bool Key(UINT virtualKey, bool down, UINT) override
    {
        SendMessageW(_scene.window.Hwnd(), down ? WM_KEYDOWN : WM_KEYUP, virtualKey, down ? 1 : static_cast<LPARAM>(0xC0000001u));
        return true;
    }
    bool Character(wchar_t character, UINT) override
    {
        _scene.Character(character);
        return true;
    }
    void AdvanceTime(uint64_t milliseconds) override
    {
        _scene.Advance(milliseconds);
    }
    void Resize(UINT width, UINT height, float dpi) override
    {
        const float scale = dpi / 96.0f;
        DxUiTestMoved::ConfigureHostPlace(_scene.window,
                                          static_cast<UINT>(std::lround(dpi)),
                                          true,
                                          Density::Standard,
                                          D2D1::SizeF(static_cast<float>(width) / scale, static_cast<float>(height) / scale));
        _scene.Host().SetTheme(ParityTheme(true));
    }
    [[nodiscard]] uint64_t NowMs() const noexcept override
    {
        return _scene.clock;
    }

private:
    NativeParityScene& _scene;
    const PerformanceFixtures::Definition& _definition;
    std::unique_ptr<PerformanceFixtures::Scenario> _scenario;
    std::optional<PerformanceFixtures::Context> _context;
};

// Runs a fixture variant through the oracle: after every step the incremental scene (warm measure caches and every other
// retained cache) and the fresh scene (all of them dropped) must agree on the bounds and state of every control (the
// bounds oracle), then on every pixel and UIA element.
template <const PerformanceFixtures::Definition& Fixture> void RunFixtureParity(std::string_view variantName)
{
    const PerformanceFixtures::Variant* variant = Fixture.FindVariant(variantName);
    Require(variant != nullptr, "the fixture variant exists");
    const auto build = [&](NativeParityScene& scene)
    {
        auto port = std::make_shared<FixtureParityPort>(scene, Fixture, *variant);
        scene.models.push_back(port);
        port->Build();
        return port;
    };
    const auto incremental     = MakeNativeScene();
    const auto fresh           = MakeNativeScene();
    const auto incrementalPort = build(*incremental);
    const auto freshPort       = build(*fresh);
    const size_t steps         = PerformanceFixtures::TotalSteps(Fixture);
    for (size_t step = 0u; step <= steps; ++step)
    {
        if (step != 0u)
        {
            incrementalPort->Step(step - 1u);
            freshPort->Step(step - 1u);
        }
        const ParityFrame a = incremental->Capture(false);
        const ParityFrame b = fresh->Capture(true);
        if (a.controls != b.controls)
        {
            ParityFrame boundsOnlyA = a;
            ParityFrame boundsOnlyB = b;
            boundsOnlyA.bgra        = boundsOnlyB.bgra;
            boundsOnlyA.uia         = boundsOnlyB.uia;
            Fail(std::format("bounds oracle, {} {}, step {} ({}): {}", Fixture.id, variantName, step, g_lastStep, FirstDifference(boundsOnlyA, boundsOnlyB)));
        }
        if (std::string difference = FirstDifference(a, b, 0u, NativeParityScene::kKnownDifferences); ! difference.empty())
        {
            Fail(std::format("incremental-versus-fresh oracle, {} {}, step {} ({}): {}", Fixture.id, variantName, step, g_lastStep, difference));
        }
    }
    std::cout << "  parity native " << Fixture.id << ' ' << variantName << ": " << steps << " steps, 0 differences in bounds, pixels or UIA\n";
}

// W6.2's FX10 finding, on the window host outside the oracle: the body row's RichTextEditor follows a Grid, whose
// OnBoundsChanged clamps its scroll through InvalidateGeometry. The arranging FlexPanel took that for a foreign geometry
// change, aborted with E_ABORT, and the host kept the half-arranged geometry: the editor stayed at empty bounds from the first
// frame and through every later frame. Every body child is arranged after the build, after a resize and on later frames.
void TestFx10LiveResizeArrangesEveryBodyChild()
{
    const auto scene = MakeNativeScene();
    auto port        = std::make_shared<FixtureParityPort>(*scene, PerformanceFixtures::Fx10::kDefinition, PerformanceFixtures::Fx10::kVariants[0]);
    scene->models.push_back(port);
    port->Build();
    const auto requireArranged = [&](const char* when)
    {
        static_cast<void>(scene->Capture(false));
        const Control* const body = scene->Host().GetRoot()->GetLogicalChild(2u);
        Require(body && body->GetLogicalChildCount() == 3u, "the FX10 body row holds the form, the list and the notes");
        for (size_t index = 0u; index < body->GetLogicalChildCount(); ++index)
        {
            const auto bounds = body->GetLogicalChild(index)->GetBounds();
            if (! (bounds.right > bounds.left && bounds.bottom > bounds.top))
                Fail(std::format("FX10 body child {} has empty bounds {}", index, when));
        }
    };
    requireArranged("after the build");
    port->Step(0u); // resized to 800x600
    requireArranged("after the first resize");
    requireArranged("on the next frame");
}

// W6.2: FX10's live resize and FX12's dense screen lay out and paint exactly as when every retained cache, the measure caches
// included, is dropped.
void TestNativeIncrementalParityFx10LiveResize()
{
    RunFixtureParity<PerformanceFixtures::Fx10::kDefinition>("default");
}
void TestNativeIncrementalParityFx12DenseScreen()
{
    RunFixtureParity<PerformanceFixtures::Fx12::kDefinition>("default");
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
    DXUI_RUN_TEST(TestNativeIncrementalParityRichBlocks);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichObjects);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichObjectsDetectsAWrongCarry);
    DXUI_RUN_TEST(TestNativeIncrementalParityComboPopup);
    DXUI_RUN_TEST(TestFx10LiveResizeArrangesEveryBodyChild);
    DXUI_RUN_TEST(TestNativeIncrementalParityFx10LiveResize);
    DXUI_RUN_TEST(TestNativeIncrementalParityFx12DenseScreen);
    DXUI_RUN_TEST(TestNativeIncrementalParityTextField);
}
