#include "Controls.Tests.IncrementalParity.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.DxUiTestMovedControls.h"
#include "Controls/DxUi.RetainedCaches.h"
#include "Controls/DxUi.RichLayoutOptions.h"
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Support.Tests.ComboBoxPopupLayout.h"
#include "Support.Tests.IncrementalParity.h"
#include "Support.Tests.LayoutScopes.h"
#include "Support.Tests.PaintRegions.h"
#include "Support.Tests.TextFieldPaint.h"
#include "Support.Tests.UnseenMotion.h"
#include "Support/LayoutScopes.Tests.Hooks.h"
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
    static constexpr uint32_t kKnownDifferences = kNoKnownDifference;
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
        // A capture pumps both scenes' windows: their dispatcher wakes must tick at the scene's clock, never at real time, or a
        // script with motion on (unseen-motion) would see each scene advanced by whatever the pumps happened to deliver.
        Host().DebugSetAnimationClockOwnsWakesForTest(true);
    }
    ~NativeParityScene()
    {
        Host().DebugSetAnimationClockOwnsWakesForTest(false);
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
        DispatchSystemLeave();
    }
    // A move makes the host call TrackMouseEvent; the real cursor is never over this off-screen window, so Windows posts a
    // WM_MOUSELEAVE. It belongs to the step that moved: dispatched now, not by whichever later pump reaches it first (a
    // capture's pump stops at its time bound, so under load one scene would show a hover the other had lost).
    void DispatchSystemLeave()
    {
        MSG leave{};
        while (PeekMessageW(&leave, window.Hwnd(), WM_MOUSELEAVE, WM_MOUSELEAVE, PM_REMOVE) != FALSE)
            static_cast<void>(DispatchQueuedMessageForTest(leave));
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
    // W3.4's script ticks motion, which a hidden host holds: its window is shown, off screen and without activation.
    void ShowWithoutActivation()
    {
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages(50u);
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
        // The pump delivers the host's own wake, which ticks motion that became seen: a negative control without the catch-up
        // before paint (W3.4) reads the frame without it, or the wake would stand in for the catch-up.
        if (! Detail::IsMotionCatchUpDisabledForTest())
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

// W7.6: line number and badge layouts carried across edits against a fresh preparation that builds every layout: pixels equal
// at 96, 120 and 144 DPI, with theme, font size, width and scroll steps.
void TestNativeIncrementalParityRichCodeBlocks()
{
    RunParityScript(RichCodeBlocksScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// W2.3: formulas, diagrams, raster images and SVG rasters carried across edits against a fresh preparation of every object.
// W7.3: position-independent v0 chunks against a fresh preparation with the same chunking, Source and multiplier.
void TestNativeIncrementalParityRichChunks()
{
    DxUi::RichText::Detail::testRichRecordChunkPlan = true;
    const auto stopRecording                        = wil::scope_exit([]() noexcept { DxUi::RichText::Detail::testRichRecordChunkPlan = false; });
    RunParityScript(RichChunksScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    RunParityScript(RichChunksMultiplierScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
}

// W7.3 negative control: every chunk after the first placed one DIP off in the incremental scene must differ.
void TestNativeIncrementalParityRichChunksDetectsAWrongOffset()
{
    DxUi::RichText::Detail::testRichRecordChunkPlan  = true;
    DxUi::RichText::Detail::testRichChunkOffsetError = 1.0f;
    const auto restore                               = wil::scope_exit([]() noexcept
    {
        DxUi::RichText::Detail::testRichRecordChunkPlan  = false;
        DxUi::RichText::Detail::testRichChunkOffsetError = 0.0f;
    });
    const std::string difference                     = RunParityScriptOnce(RichChunksScript<NativeParityScene>(), kRichChunksSeeds[0], MakeNativeScene);
    std::cout << "  native wrong-offset negative control: " << difference << '\n';
    Require(difference.find("pixels differ") != std::string::npos, "a chunk placed one DIP off differs from a fresh preparation of the same chunking");
}

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

// W4.10: the regions scripts on the window host, which still paints whole frames but culls children outside the window and
// the ScrollPanel viewports; a frame of the system's damage alone (DebugRenderDirtyRectForTest) culls against it.
void TestNativeIncrementalParityPaintRegions()
{
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::RegionScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    RunParityScript(PaintRegionScript::RegionDpiScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    // W4.7: blink flips between edits (the window host's flips still paint whole frames until W4.9 presents the region).
    RunParityScript(PaintRegionScript::CaretFlipScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  native regions oracle: " << work.frame.paintCulled << " children culled, " << work.frame.inkEscapes << " ink escapes\n";
    Require(work.frame.paintCulled > 0u, "W4.10: the native regions oracle compares frames that culled children");
}

// W6.3: layout invalidation scopes on the window host. The negative control stops every queued measure invalidation at its
// control unchecked, as a wrong cutoff would: a Label that grew must then differ from the fresh layout.
void TestNativeIncrementalParityLayoutScopes()
{
    // Debug builds also compare every pruned preparation pass of both scenes with a whole layout.
    const bool previous  = DxUi::Detail::LayoutScopeVerificationEnabled();
    const bool verifying = DxUi::Detail::SetLayoutScopeVerificationForTest(true);
    const auto restore   = wil::scope_exit([previous] { static_cast<void>(DxUi::Detail::SetLayoutScopeVerificationForTest(previous)); });
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(LayoutScopes::LayoutScopesScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    const auto work = scope.Delta();
    std::cout << "  native layout-scopes verification " << (verifying ? "on" : "not in this build") << ": " << work.layout.scopeVerifyMismatches
              << " mismatches\n";
    Require(work.layout.scopeVerifyMismatches == 0u, "W6.3: every pruned pass of the script equals a whole layout");
    std::cout << "  native layout-scopes: " << work.layout.measureCutoffs << " cutoffs, " << work.frame.treeVisitsPreparation << " preparation visits\n";
    Require(work.layout.measureCutoffs > 0u, "W6.3: the layout-scopes script compares frames after a cutoff");
}

void TestNativeIncrementalParityLayoutScopesDetectsAWrongCutoff()
{
    constexpr std::array<uint64_t, 1u> seed{0x06030F01u};
    const ParityScript<NativeParityScene> script{"wrong-cutoff",
                                                 seed,
                                                 1u,
                                                 &LayoutScopes::BuildLayoutScopesScene<NativeParityScene>,
                                                 [](NativeParityScene& scene, Rng&, size_t)
    {
        static_cast<Label*>(scene.slots[LayoutScopes::kLabelFirst + 1u])
            ->SetText(L"Row 1 with a caption long enough to push the button and the field to the trailing side");
    }};
    Require(RunParityScriptOnce(script, seed[0], MakeNativeScene).empty(), "W6.3: without the injection the growing Label renders identically");
    // The verification would lay the stale bound out again before the oracle reads it: off for the injected run.
    const bool verifying = DxUi::Detail::LayoutScopeVerificationEnabled();
    static_cast<void>(DxUi::Detail::SetLayoutScopeVerificationForTest(false));
    DxUi::Detail::SetLayoutCutoffUncheckedForTest(true);
    const auto restore           = wil::scope_exit([verifying]
    {
        DxUi::Detail::SetLayoutCutoffUncheckedForTest(false);
        static_cast<void>(DxUi::Detail::SetLayoutScopeVerificationForTest(verifying));
    });
    const std::string difference = RunParityScriptOnce(script, seed[0], MakeNativeScene);
    std::cout << "  native wrong-cutoff negative control: " << difference << '\n';
    Require(difference.starts_with("step 1 ") && ! difference.empty(), "W6.3: an unchecked cutoff leaves a stale bound the oracle sees");
}

// W4.10 round 3 (X1): a PageHost's pages follow the culling rules of a panel's children on the window host, whose whole frames
// cull against the window: nothing below an undeclared page is culled, a declared page's child whose reach meets the window is
// painted, and a page transition in flight stays right. The fresh scene paints with culling off.
void TestNativeIncrementalParityPageHostCulling()
{
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(PaintRegionScript::PageHostCullingScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    Require(scope.Delta().frame.paintCulled > 0u, "W4.10: the native page-host script compares frames that culled a child");
}

// W3.4: motion held while unseen (scrolled out, an unselected tab) against a fresh scene that ticked it every frame on the same
// clock: the first frame that shows it again is at its clock-correct state. Negative control: without the catch-up before
// paint, the incremental scene shows it one frame late and the oracle must see a difference.
void TestNativeIncrementalParityUnseenMotion()
{
    const DxUi::Detail::WorkCounterScope scope;
    RunParityScript(UnseenMotionScript::UnseenMotionScript<NativeParityScene>(), MakeNativeScene, "native", kReplay);
    const DxUi::Detail::WorkCounters work = scope.Delta();
    std::cout << "  native unseen-motion oracle: " << work.frame.tickHeldUnseen << " held, " << work.frame.tickCatchUps << " caught up\n";
    Require(work.frame.tickHeldUnseen > 0u && work.frame.tickCatchUps > 0u, "W3.4: the native oracle compares frames that held and caught up motion");
    DxUi::Detail::SetMotionCatchUpDisabledForTest(true);
    const auto restore = wil::scope_exit([]() noexcept { DxUi::Detail::SetMotionCatchUpDisabledForTest(false); });
    const std::string difference =
        RunParityScriptOnce(UnseenMotionScript::UnseenMotionScript<NativeParityScene>(), UnseenMotionScript::kUnseenMotionSeeds[0], MakeNativeScene);
    std::cout << "  native unseen-motion negative control: " << difference << '\n';
    Require(difference.find("pixels differ") != std::string::npos, "W3.4: motion shown one frame late differs from a scene that ticked it all along");
}
// The scene's pointer input is complete when Pointer returns. A synthesized WM_MOUSEMOVE makes the host call TrackMouseEvent
// (TME_LEAVE); the real cursor is never over the off-screen parity window, so Windows posts WM_MOUSELEAVE to the thread's
// queue. Left there, it was dispatched by whichever capture pumped first, and under load a capture's pump can stop at its
// 50 ms bound before reaching it: one scene then showed the hover the other had already lost (the interaction script, seed
// 0x5EED00B1, step 28, a Tree row on an 87 % loaded machine). After Pointer, no leave of the window may still be queued.
void TestNativeParityPointerInputCompletesInItsStep()
{
    NativeParityScene scene;
    scene.Host().SetTheme(ParityTheme(false));
    auto root   = std::make_unique<Panel>();
    auto* field = root->AddChild<Button>(L"Hover target");
    field->SetBounds(D2D1::RectF(20, 20, 200, 52));
    scene.SetRoot(std::move(root));
    static_cast<void>(scene.Capture(false));
    const HWND hwnd = scene.window.Hwnd();
    scene.Pointer(ParityPointer::Move, CenterOf(*field));
    MSG pending{};
    const bool leaveQueued = PeekMessageW(&pending, hwnd, WM_MOUSELEAVE, WM_MOUSELEAVE, PM_NOREMOVE) != FALSE;
    std::cout << "  pointer move: WM_MOUSELEAVE " << (leaveQueued ? "still queued" : "dispatched") << " when Pointer returns, hovered "
              << (scene.Host().DebugGetHoveredControl() != nullptr) << '\n';
    Require(! leaveQueued, "W4.10 round 3: a parity scene's pointer input, the system's leave included, is dispatched within its step");
    scene.window.PumpMessages(50u);
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
        : scene_(scene),
          definition_(definition),
          scenario_(definition.create())
    {
        context_.emplace(PerformanceFixtures::Context{PerformanceFixtures::Host::Window, variant, scene.Host(), *this});
    }
    FixtureParityPort(const FixtureParityPort&)            = delete;
    FixtureParityPort& operator=(const FixtureParityPort&) = delete;
    FixtureParityPort(FixtureParityPort&&)                 = delete;
    FixtureParityPort& operator=(FixtureParityPort&&)      = delete;
    ~FixtureParityPort() override                          = default;
    void Build()
    {
        scene_.window.AllowOuterSizeBeyondDesktop(SIZE{8192, 8192});
        Resize(context_->variant.width, context_->variant.height, context_->variant.dpi);
        RequireSucceeded(scenario_->Build(*context_), "the fixture builds on the oracle scene");
    }
    void Step(size_t index)
    {
        const auto at = PerformanceFixtures::StepAt(definition_, index);
        Require(at.has_value(), "the oracle step is a fixture step");
        NoteStep(definition_.phases[at->phase].name);
        scenario_->Step(*context_, at->phase, at->step);
    }
    bool Pointer(const PointerEvent& event) override
    {
        const LPARAM at = MAKELPARAM(static_cast<short>(std::lround(event.xPixels)), static_cast<short>(std::lround(event.yPixels)));
        const HWND hwnd = scene_.window.Hwnd();
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
        scene_.DispatchSystemLeave();
        return true;
    }
    bool Key(UINT virtualKey, bool down, UINT) override
    {
        SendMessageW(scene_.window.Hwnd(), down ? WM_KEYDOWN : WM_KEYUP, virtualKey, down ? 1 : static_cast<LPARAM>(0xC0000001u));
        return true;
    }
    bool Character(wchar_t character, UINT) override
    {
        scene_.Character(character);
        return true;
    }
    void AdvanceTime(uint64_t milliseconds) override
    {
        scene_.Advance(milliseconds);
    }
    void Resize(UINT width, UINT height, float dpi) override
    {
        const float scale = dpi / 96.0f;
        DxUiTestMoved::ConfigureHostPlace(scene_.window,
                                          static_cast<UINT>(std::lround(dpi)),
                                          true,
                                          Density::Standard,
                                          D2D1::SizeF(static_cast<float>(width) / scale, static_cast<float>(height) / scale));
        scene_.Host().SetTheme(ParityTheme(true));
    }
    [[nodiscard]] uint64_t NowMs() const noexcept override
    {
        return scene_.clock;
    }

private:
    NativeParityScene& scene_;
    const PerformanceFixtures::Definition& definition_;
    std::unique_ptr<PerformanceFixtures::Scenario> scenario_;
    std::optional<PerformanceFixtures::Context> context_;
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
    for (auto build : {&BuildControlsScene<NativeParityScene>, &BuildRichScene<NativeParityScene>, &BuildRichCodeBlocksScene<NativeParityScene>})
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
    DXUI_RUN_TEST(TestNativeIncrementalParityRichCodeBlocks);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichObjects);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichObjectsDetectsAWrongCarry);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichChunks);
    DXUI_RUN_TEST(TestNativeIncrementalParityRichChunksDetectsAWrongOffset);
    DXUI_RUN_TEST(TestNativeIncrementalParityComboPopup);
    DXUI_RUN_TEST(TestFx10LiveResizeArrangesEveryBodyChild);
    DXUI_RUN_TEST(TestNativeIncrementalParityFx10LiveResize);
    DXUI_RUN_TEST(TestNativeIncrementalParityFx12DenseScreen);
    DXUI_RUN_TEST(TestNativeIncrementalParityTextField);
    DXUI_RUN_TEST(TestNativeIncrementalParityPaintRegions);
    DXUI_RUN_TEST(TestNativeIncrementalParityPageHostCulling);
    DXUI_RUN_TEST(TestNativeIncrementalParityLayoutScopes);
    DXUI_RUN_TEST(TestNativeIncrementalParityLayoutScopesDetectsAWrongCutoff);
    DXUI_RUN_TEST(TestNativeIncrementalParityUnseenMotion);
    DXUI_RUN_TEST(TestNativeParityPointerInputCompletesInItsStep);
}
