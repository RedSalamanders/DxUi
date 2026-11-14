// Pointer-move cost on the WindowHost (plan W2.9). The rule: Specs/UI/UI_InputAndAccessibility.md, "Pointer hit result".
#include "Controls.Tests.PointerHit.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.PerformanceFixtures.h"
#include "Controls/DxUi.Internal.h"
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // scenarios are never copied
#include "Embedded.Tests.PerformanceFixtureFx10.h"
#include "Embedded.Tests.PerformanceFixtureFx14.h"
#pragma warning(pop)
#include "Support.Tests.PointerHitDifferential.h"
#include "Support/WorkCounters.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Scenes and probes are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4623 4625 4626 5026 5027)
namespace
{
using DxUi::Detail::WorkCounterScope;
using PointerHitDifferential::Log;
using PointerHitDifferential::Recorder;

// A WARP WindowHost on a non-activating test window sized for the differential scene. Moves are announced by the cursor
// query a WM_SETCURSOR runs (the system sends it before the mouse message, at the same point); the queued WM_MOUSELEAVE the
// off-screen window receives after each move is dropped, as the fixture runner drops it.
class NativeHitScene final
{
public:
    static constexpr bool kNativePopups = true;

    explicit NativeHitScene(bool hitResultEnabled) : _window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true)
    {
        _window.Host().DebugSetPointerHitResultEnabled(hitResultEnabled);
        auto theme          = DxUi::MakeDefaultThemePalette(false);
        theme.reducedMotion = true;
        _window.Host().SetTheme(theme);
        const float scale = _window.Host().GetDpi() / 96.0f;
        RECT outer{};
        RECT client{};
        GetWindowRect(_window.Hwnd(), &outer);
        GetClientRect(_window.Hwnd(), &client);
        SetWindowPos(_window.Hwnd(),
                     nullptr,
                     0,
                     0,
                     (outer.right - outer.left) - client.right + static_cast<LONG>(std::lround(440.0f * scale)),
                     (outer.bottom - outer.top) - client.bottom + static_cast<LONG>(std::lround(340.0f * scale)),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(_window.Hwnd(), SW_SHOWNOACTIVATE);
    }
    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return _window.Host();
    }
    [[nodiscard]] HWND Hwnd() const noexcept
    {
        return _window.Hwnd();
    }
    void SetRoot(std::unique_ptr<DxUi::Control> root)
    {
        _window.Host().SetRoot(std::move(root));
    }
    void Frame()
    {
        constexpr UINT kSystemTimer = 0x0118u;
        MSG message{};
        for (size_t count = 0u; count < 4096u; ++count)
        {
            if (! PeekMessageW(&message, nullptr, 0u, WM_TIMER - 1u, PM_REMOVE) && ! PeekMessageW(&message, nullptr, kSystemTimer + 1u, 0xFFFFu, PM_REMOVE))
                break;
            if (message.message == WM_MOUSELEAVE && message.hwnd == Hwnd())
                continue;
            static_cast<void>(DispatchQueuedMessageForTest(message));
        }
        static_cast<void>(UpdateWindow(Hwnd()));
    }
    [[nodiscard]] POINT Pixel(D2D1_POINT_2F dip) const noexcept
    {
        auto& host = const_cast<NativeHitScene*>(this)->Host();
        return POINT{static_cast<LONG>(std::lround(host.DipsToPixels(dip.x))), static_cast<LONG>(std::lround(host.DipsToPixels(dip.y)))};
    }
    // The point a mouse message at the pixel nearest `dip` carries, as the host computes it.
    [[nodiscard]] D2D1_POINT_2F Snap(D2D1_POINT_2F dip)
    {
        const POINT pixel = Pixel(dip);
        return D2D1::Point2F(Host().PixelsToDip(static_cast<float>(pixel.x)), Host().PixelsToDip(static_cast<float>(pixel.y)));
    }
    DxUi::WindowHostCursorKind CursorQuery(D2D1_POINT_2F dip)
    {
        return Host().DebugResolveCursorKindForPoint(Snap(dip));
    }
    void Move(D2D1_POINT_2F dip)
    {
        Send(WM_MOUSEMOVE, 0u, dip);
    }
    void Down(D2D1_POINT_2F dip)
    {
        Send(WM_LBUTTONDOWN, MK_LBUTTON, dip);
    }
    void Up(D2D1_POINT_2F dip)
    {
        Send(WM_LBUTTONUP, 0u, dip);
    }
    void MiddleDown(D2D1_POINT_2F dip)
    {
        Send(WM_MBUTTONDOWN, MK_MBUTTON, dip);
    }
    void MiddleUp(D2D1_POINT_2F dip)
    {
        Send(WM_MBUTTONUP, 0u, dip);
    }
    void Wheel(D2D1_POINT_2F dip, float delta)
    {
        POINT screen = Pixel(dip);
        static_cast<void>(ClientToScreen(Hwnd(), &screen));
        SendMessageW(Hwnd(),
                     WM_MOUSEWHEEL,
                     MAKEWPARAM(0u, static_cast<short>(std::lround(delta))),
                     MAKELPARAM(static_cast<short>(screen.x), static_cast<short>(screen.y)));
    }
    void Leave()
    {
        SendMessageW(Hwnd(), WM_MOUSELEAVE, 0u, 0);
    }
    void Key(UINT virtualKey)
    {
        SendMessageW(Hwnd(), WM_KEYDOWN, virtualKey, 1);
        SendMessageW(Hwnd(), WM_KEYUP, virtualKey, static_cast<LPARAM>(0xC0000001u));
    }

private:
    void Send(UINT message, WPARAM keys, D2D1_POINT_2F dip)
    {
        const POINT pixel = Pixel(dip);
        SendMessageW(Hwnd(), message, keys, MAKELPARAM(static_cast<short>(pixel.x), static_cast<short>(pixel.y)));
    }
    AttachedHostWindow _window;
};

// A recorder that removes and destroys itself from inside its own move callback.
class SelfRemover final : public Recorder
{
public:
    using Recorder::Recorder;
    bool OnMouseMove(DxUi::ControlHost& host, D2D1_POINT_2F point, UINT modifiers) override
    {
        static_cast<void>(Recorder::OnMouseMove(host, point, modifiers));
        DxUi::Panel* const parent = owner;
        for (size_t index = 0u; parent && index < parent->GetLogicalChildCount(); ++index)
            if (parent->GetLogicalChild(index) == this)
            {
                std::unique_ptr<DxUi::Control> self = parent->TakeChild(index);
                static_cast<void>(parent->CompactChildren());
                self.reset(); // `this` is gone: nothing below touches a member
                return true;
            }
        return true;
    }
    DxUi::Panel* owner = nullptr;
};

// A Button subclass that overrides nothing about hit testing and declares nothing: the host cannot tell what its hit test
// depends on, so it must never reuse a result while one is in the tree.
class UndeclaredButton final : public DxUi::Button
{
public:
    using DxUi::Button::Button;
};

class ExposedLabel final : public DxUi::Label
{
public:
    using DxUi::Label::Label;
    using DxUi::Label::PrepareVisualContent;
};

[[nodiscard]] D2D1_POINT_2F CenterOf(const DxUi::Control& control) noexcept
{
    const D2D1_RECT_F bounds = control.GetBounds();
    return D2D1::Point2F((bounds.left + bounds.right) * 0.5f, (bounds.top + bounds.bottom) * 0.5f);
}
} // namespace
#pragma warning(pop)

// FX14 on the window host: each move, announced by its cursor query, walks the tree once; the move reuses that walk.
void TestPointerHitFx14WindowHitTestsOncePerMove()
{
    using namespace PerformanceFixtures;
    const RunRecord run = RunWindowFixture(Fx14::kDefinition, Fx14::kVariants[0]);
    for (const std::string_view phase : {std::string_view("enter"), std::string_view("pending"), std::string_view("shown"), std::string_view("leave")})
    {
        const CounterSummary walks  = Summarize(run.Phase(phase), "frame.hitTests");
        const CounterSummary reused = Summarize(run.Phase(phase), "frame.hitTestReuses");
        std::cout << "    FX14 window " << phase << ": hit tests per move " << walks.minimum << '-' << walks.maximum << ", reused " << reused.minimum << '-'
                  << reused.maximum << '\n';
        Require(walks.minimum == 1u && walks.maximum == 1u, "FX14 on the window host: one hit test per pointer move");
        Require(reused.minimum == 1u && reused.maximum == 1u, "the move reuses the hit result of its cursor query");
    }
}

// FX10 on the window host: a live resize runs no IsControlInTree search during preparation and prepares no plain Label.
void TestPointerHitFx10WindowPreparesNoPlainLabelAndSearchesNoTree()
{
    using namespace PerformanceFixtures;
    const RunRecord run = RunWindowFixture(Fx10::kDefinition, Fx10::kVariants[0]);
    for (const std::string_view phase : {std::string_view("sweep"), std::string_view("rest")})
    {
        Require(Summarize(run.Phase(phase), "frame.preparationMembershipSearches").maximum == 0u, "FX10 window: no membership search during preparation");
        Require(Summarize(run.Phase(phase), "frame.plainLabelPreparations").maximum == 0u, "FX10 window: no plain Label is prepared");
    }
}

// The seeded differential: the same scripts on a host that reuses hit results and one that walks for every hit test record
// the same callbacks, in the same order, and leave the same hover, capture, focus and cursor.
void TestPointerHitWindowDifferentialMatchesTheUncachedHitTest()
{
    using namespace PointerHitDifferential;
    constexpr std::array<uint64_t, 4u> kSeeds{0x5EED2901u, 0x5EED2902u, 0x5EED2903u, 0x5EED2904u};
    for (const uint64_t seed : kSeeds)
    {
        for (const bool reportedHole : {true, false})
        {
            // The logs outlive the scenes: detaching a scene still reports hover changes.
            Log cachedLog;
            Log walkedLog;
            NativeHitScene cached(true);
            NativeHitScene walked(false);
            SceneSlots cachedSlots;
            SceneSlots walkedSlots;
            BuildScene(cached, cachedSlots, cachedLog, reportedHole);
            BuildScene(walked, walkedSlots, walkedLog, reportedHole);
            Rng cachedRng{seed};
            Rng walkedRng{seed};
            const WorkCounterScope scope;
            uint64_t reuses = 0u;
            for (size_t step = 0u; step < 160u; ++step)
            {
                const WorkCounterScope stepScope;
                const StepRecord a = Step(cached, cachedSlots, cachedLog, cachedRng);
                reuses += stepScope.Delta().frame.hitTestReuses;
                const StepRecord b = Step(walked, walkedSlots, walkedLog, walkedRng);
                if (a != b)
                {
                    std::cerr << "W2.9 differential (window) seed " << seed << " hole reported " << reportedHole << " step " << step
                              << ":\n  cached: " << a.state << '\n';
                    for (const auto& line : a.lines)
                        std::cerr << "    " << line << '\n';
                    std::cerr << "  walked: " << b.state << '\n';
                    for (const auto& line : b.lines)
                        std::cerr << "    " << line << '\n';
                }
                Require(a == b, "a host that reuses hit results delivers what the uncached hit test delivers");
            }
            std::cout << "    W2.9 differential window seed " << seed << (reportedHole ? " (reported hole)" : " (unreported hole)") << ": 160 steps, " << reuses
                      << " reused hit results, 0 differences\n";
            if (reportedHole)
                Require(reuses > 0u, "the cached scene reused hit results");
            else
                Require(reuses == 0u, "an unreported control in the tree turns reuse off");
        }
    }
}

// The hit result never outlives its control: removal between the cursor query and its move, removal from inside the
// control's own move callback, root replacement, device loss and detach. Run under ASan Debug.
void TestPointerHitWindowResultNeverOutlivesItsControl()
{
    Log log; // outlives the scene, whose detach reports hover changes
    NativeHitScene scene(true);
    auto root          = std::make_unique<DxUi::Panel>();
    DxUi::Panel* panel = root.get();
    auto* victim       = root->AddChild<Recorder>(L"victim", log);
    victim->SetBounds(D2D1::RectF(20.0f, 20.0f, 200.0f, 80.0f));
    auto* remover  = root->AddChild<SelfRemover>(L"remover", log);
    remover->owner = panel;
    remover->SetBounds(D2D1::RectF(20.0f, 100.0f, 200.0f, 160.0f));
    scene.SetRoot(std::move(root));
    scene.Frame();
    auto& host = scene.Host();

    // Removed and destroyed between the cursor query and the move it announces.
    const D2D1_POINT_2F point = scene.Snap(CenterOf(*victim));
    static_cast<void>(scene.CursorQuery(point));
    Require(host.DebugHasPointerHitResult(), "the cursor query leaves its hit result for the move");
    for (size_t index = 0u; index < panel->GetLogicalChildCount(); ++index)
        if (panel->GetLogicalChild(index) == victim)
        {
            std::unique_ptr<DxUi::Control> taken = panel->TakeChild(index);
            static_cast<void>(panel->CompactChildren());
            taken.reset();
            break;
        }
    scene.Move(point);
    Require(! host.DebugHasPointerHitResult(), "the move's message ends the pointer event");
    Require(host.DebugGetHoveredControl() == panel, "the move hovers what is under the point now, not the destroyed control");

    // Removed and destroyed by itself, inside its own move callback; then a second move at the same point.
    const D2D1_POINT_2F second = scene.Snap(CenterOf(*remover));
    static_cast<void>(scene.CursorQuery(second));
    scene.Move(second);
    Require(log.lines.size() >= 2u && log.lines.back() == "remover move", "the remover received its move and removed itself");
    static_cast<void>(scene.CursorQuery(second));
    scene.Move(second);
    Require(host.DebugGetHoveredControl() == panel, "the second move at the same point finds the panel");

    // Root replacement, device loss and detach drop a pending result.
    static_cast<void>(scene.CursorQuery(second));
    Require(host.DebugHasPointerHitResult(), "a cursor query leaves a result");
    host.SetRoot(std::make_unique<DxUi::Panel>());
    Require(! host.DebugHasPointerHitResult(), "replacing the root drops it");
    static_cast<void>(scene.CursorQuery(second));
    host.DebugSimulateDeviceLoss();
    Require(! host.DebugHasPointerHitResult(), "device loss drops it");
    static_cast<void>(scene.CursorQuery(second));
    host.Detach();
    Require(! host.DebugHasPointerHitResult(), "detach drops it");
}

// A control whose hit test the host cannot see turns reuse off while it is in the tree; library controls and a control that
// declares its hit test reported keep it.
void TestPointerHitWindowUnreportedControlIsNeverReused()
{
    Log log; // outlives the scene, whose detach reports hover changes
    NativeHitScene scene(true);
    auto root          = std::make_unique<DxUi::Panel>();
    DxUi::Panel* panel = root.get();
    auto* library      = root->AddChild<DxUi::Button>(L"library");
    library->SetBounds(D2D1::RectF(20.0f, 20.0f, 200.0f, 80.0f));
    auto* declared = root->AddChild<Recorder>(L"declared", log);
    declared->SetBounds(D2D1::RectF(20.0f, 100.0f, 200.0f, 160.0f));
    scene.SetRoot(std::move(root));
    scene.Frame();
    Require(library->DebugIsHitTestReported() && declared->DebugIsHitTestReported(), "library and declared controls are reported");

    const auto moveCounts = [&](const DxUi::Control& over)
    {
        const D2D1_POINT_2F point = scene.Snap(CenterOf(over));
        const WorkCounterScope scope;
        static_cast<void>(scene.CursorQuery(point));
        scene.Move(point);
        return scope.Delta().frame;
    };
    auto counts = moveCounts(*library);
    Require(counts.hitTests == 1u && counts.hitTestReuses == 1u, "a library tree walks once per announced move");

    auto* undeclared = panel->AddChild<UndeclaredButton>(L"undeclared");
    undeclared->SetBounds(D2D1::RectF(220.0f, 20.0f, 400.0f, 80.0f));
    scene.Frame();
    Require(! undeclared->DebugIsHitTestReported(), "a consumer subclass of a library control is not reported unless it declares so");
    counts = moveCounts(*library);
    Require(counts.hitTests == 2u && counts.hitTestReuses == 0u, "with an unreported control in the tree every hit test walks");

    for (size_t index = 0u; index < panel->GetLogicalChildCount(); ++index)
        if (panel->GetLogicalChild(index) == undeclared)
        {
            static_cast<void>(panel->TakeChild(index));
            static_cast<void>(panel->CompactChildren());
            break;
        }
    scene.Frame();
    counts = moveCounts(*declared);
    Require(counts.hitTests == 1u && counts.hitTestReuses == 1u, "removing it turns reuse back on");

    // InvalidateHitTest between the cursor query and the move: the move walks again.
    const D2D1_POINT_2F point = scene.Snap(CenterOf(*declared));
    const WorkCounterScope scope;
    static_cast<void>(scene.CursorQuery(point));
    declared->InvalidateHitTest();
    scene.Move(point);
    Require(scope.Delta().frame.hitTests == 2u && scope.Delta().frame.hitTestReuses == 0u, "InvalidateHitTest moves the hit revision");
}

// Plain labels are not prepared; a mnemonic or a typography override set later enables preparation. A label asked to
// prepare for a host it is not attached to is refused (by its GetHost() check: the tree walk is gone).
void TestPointerHitPlainLabelsAreNotPrepared()
{
    NativeHitScene scene(true);
    auto root     = std::make_unique<DxUi::Panel>();
    auto* plain   = root->AddChild<DxUi::Label>(L"Plain");
    auto* marked  = root->AddChild<DxUi::Label>(L"Name");
    auto* styled  = root->AddChild<DxUi::Label>(L"Styled");
    auto* wrapped = root->AddChild<DxUi::Label>(L"Wrapped and aligned text");
    plain->SetBounds(D2D1::RectF(10.0f, 10.0f, 200.0f, 30.0f));
    marked->SetBounds(D2D1::RectF(10.0f, 40.0f, 200.0f, 60.0f));
    styled->SetBounds(D2D1::RectF(10.0f, 70.0f, 200.0f, 100.0f));
    wrapped->SetBounds(D2D1::RectF(10.0f, 110.0f, 120.0f, 160.0f));
    wrapped->SetMultiline(true);
    wrapped->SetAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    wrapped->SetTextColor(D2D1::ColorF(0.8f, 0.1f, 0.1f));
    wrapped->SetFontRole(DxUi::FontRole::Small);
    scene.SetRoot(std::move(root));
    scene.Frame();
    Require(! plain->DebugVisualPreparationEnabled() && ! wrapped->DebugVisualPreparationEnabled(),
            "plain labels (wrapping, alignment, color and font role included) are not prepared");

    const auto layoutsBefore = DxUi::testMnemonicLayoutCreations;
    marked->SetMnemonic(L'N');
    Require(marked->DebugVisualPreparationEnabled(), "a mnemonic set later enables preparation");
    Require(SUCCEEDED(styled->SetTypographyOverride(DxUi::LabelTypography{18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, 0.0f})), "override accepted");
    Require(styled->DebugVisualPreparationEnabled(), "a typography override enables preparation");
    const WorkCounterScope scope;
    scene.Frame();
    Require(DxUi::testMnemonicLayoutCreations == layoutsBefore + 2u, "the next frame prepares exactly the two labels that stopped being plain");
    Require(scope.Delta().frame.plainLabelPreparations == 0u, "and no plain label");

    // Detached: refused without creating a layout.
    ExposedLabel detached(L"Detached");
    detached.SetMnemonic(L'D');
    detached.SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 20.0f));
    const auto beforeDetached = DxUi::testMnemonicLayoutCreations;
    detached.PrepareVisualContent(scene.Host());
    Require(DxUi::testMnemonicLayoutCreations == beforeDetached, "a label not attached to the host is refused");
}
