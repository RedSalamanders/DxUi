#include "Controls.Tests.PopupShow.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support/WorkCounters.h"

#include <format>
#include <iostream>

// W2.4's "one render per menu open" on WARP: a popup-like host (composition swap chain, as MenuPopup attaches) renders its
// first frame with RenderInitialFrameForShow while hidden, then is shown. Every wait is on the condition the test means; the
// test watchdog is the only limit.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;

#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027) // never copied
class PaintCountProbe final : public DxUi::Control
{
public:
    void Paint(DxUi::ControlHost& host) const override
    {
        ++paints;
        if (auto* const brush = host.GetSolidBrush(D2D1::ColorF(0.2f, 0.4f, 0.8f, 1.0f)))
            host.GetDeviceContext()->FillRectangle(GetBounds(), brush);
    }
    void Repaint(DxUi::ControlHost& host) const
    {
        Invalidate(host);
    }
    mutable size_t paints = 0u;
};
#pragma warning(pop)

template <class Condition> void PumpUntilReached(Condition&& reached)
{
    while (! reached())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
            static_cast<void>(DispatchQueuedMessageForTest(message));
        if (reached())
            return;
        NoteDxUiTestProgress();
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, 50u, QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
}

// Every message queued now, then until the pacer holds nothing that would render later (no deferred paint, no armed wait).
void Settle(DxUi::WindowHost& host)
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
        static_cast<void>(DispatchQueuedMessageForTest(message));
    PumpUntilReached([&host]
    {
        const auto state = host.DebugGetFramePacerStateForTest();
        return ! state.deferred && ! state.waitArmed;
    });
}

// A hidden popup-like window with its first frame rendered for the show.
struct PopupLikeWindow final
{
    PopupLikeWindow() : window(DxUi::WindowHost::PresentationMode::CompositionSwapChain, true)
    {
        auto root = std::make_unique<DxUi::Panel>();
        probe     = root->AddChild<PaintCountProbe>();
        probe->SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 160.0f));
        window.Host().SetRoot(std::move(root));
        SetWindowPos(window.Hwnd(), nullptr, 0, 0, 200, 160, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        window.PumpMessages();
        Require(IsWindowVisible(window.Hwnd()) == FALSE, "the popup-like window starts hidden");
    }
    PopupLikeWindow(const PopupLikeWindow&)            = delete;
    PopupLikeWindow& operator=(const PopupLikeWindow&) = delete;
    PopupLikeWindow(PopupLikeWindow&&)                 = delete;
    PopupLikeWindow& operator=(PopupLikeWindow&&)      = delete;
    ~PopupLikeWindow()                                 = default;

    [[nodiscard]] DxUi::WindowHost& Host() noexcept
    {
        return window.Host();
    }
    void Show()
    {
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        currentAfterShowWindow = Host().DebugIsFrameReadyForShowForTest();
        // The window lies off screen, where a show exposes nothing: invalidate it as the system does for a window shown on
        // screen, so the show's WM_PAINT comes.
        RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE);
        Settle(Host());
    }

    AttachedHostWindow window;
    PaintCountProbe* probe      = nullptr;
    bool currentAfterShowWindow = false;
};

void RequirePacingKept(const WorkCounters& work, const char* context)
{
    if (work.frame.presentsWithoutCredit != 0u || work.frame.pacerInvariantViolations != 0u)
        std::cerr << std::format("  {}: presents without credit {}, pacer invariant violations {}\n",
                                 context,
                                 work.frame.presentsWithoutCredit,
                                 work.frame.pacerInvariantViolations);
    Require(work.frame.presentsWithoutCredit == 0u && work.frame.pacerInvariantViolations == 0u, context);
}
} // namespace

void TestPopupShowRendersTheFirstFrameOnce()
{
    PopupLikeWindow popup;
    const WorkCounterScope scope;
    Require(popup.Host().RenderInitialFrameForShow(), "the pre-show render presents the popup's first frame");
    Require(popup.probe->paints == 1u, "the pre-show render paints the popup once");
    Require(popup.Host().DebugIsFrameReadyForShowForTest(), "the presented first frame is current after the pre-show render");
    const auto beforeShow = popup.Host().DebugGetFramePacerStateForTest();
    Require(beforeShow.paced, "the WARP composition chain is paced");
    popup.Show();
    Require(popup.currentAfterShowWindow, "the show itself (WM_SHOWWINDOW, the first WM_SIZE) keeps the first frame current");
    const WorkCounters work = scope.Delta();
    const auto after        = popup.Host().DebugGetFramePacerStateForTest();
    std::cout << std::format("  popup show: {} renders, {} paints, {} presents, {} show paints validated; pacer after the open: credit {}, wait armed {}, "
                             "deferred {}\n",
                             work.frame.renders,
                             popup.probe->paints,
                             work.frame.presents,
                             work.frame.showPaintsValidated,
                             after.credit,
                             after.waitArmed,
                             after.deferred);
    Require(work.frame.renders == 1u && popup.probe->paints == 1u && work.frame.presents == 1u,
            "a popup whose first frame was presented before the show renders once per open");
    Require(work.frame.showPaintsValidated == 1u, "the show's WM_PAINT is validated without a render");
    Require(! after.deferred && ! after.waitArmed, "after the open the pacer holds no deferred paint and no armed wait");
    Require(work.frame.pacerDeferredPaints == 0u, "the show defers no paint");
    RequirePacingKept(work, "the open presents only with a credit and leaves no deferred paint without a wake");

    // The popup's next change renders as any paced frame does.
    const WorkCounterScope next;
    popup.probe->Repaint(popup.Host());
    PumpUntilReached([&] { return popup.probe->paints == 2u; });
    Settle(popup.Host());
    Require(popup.probe->paints == 2u && next.Delta().frame.renders == 1u, "the next change after the open renders once");
    RequirePacingKept(next.Delta(), "the frame after the open");
}

void TestPopupShowRendersAgainAfterAChangeBeforeTheShow()
{
    enum class Change
    {
        Invalidation,
        Size,
        Dpi,
    };
    for (const Change change : {Change::Invalidation, Change::Size, Change::Dpi})
    {
        const char* const name = change == Change::Invalidation ? "an invalidation" : (change == Change::Size ? "a size change" : "a DPI change");
        PopupLikeWindow popup;
        const WorkCounterScope scope;
        Require(popup.Host().RenderInitialFrameForShow(), "the pre-show render presents");
        switch (change)
        {
            case Change::Invalidation: popup.probe->Repaint(popup.Host()); break;
            case Change::Size: SetWindowPos(popup.window.Hwnd(), nullptr, 0, 0, 240, 180, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE); break;
            case Change::Dpi:
            {
                RECT outer{};
                GetWindowRect(popup.window.Hwnd(), &outer);
                bool handled = false;
                static_cast<void>(
                    popup.Host().HandleMessage(popup.window.Hwnd(), WM_DPICHANGED, MAKEWPARAM(144, 144), reinterpret_cast<LPARAM>(&outer), handled));
                Require(popup.Host().GetDpi() == 144.0f, "the popup-like host takes the new DPI");
                break;
            }
        }
        popup.Show();
        PumpUntilReached([&] { return popup.probe->paints >= 2u; });
        Settle(popup.Host());
        const WorkCounters work = scope.Delta();
        std::cout << std::format("  popup show after {}: {} renders, {} paints, {} show paints validated\n",
                                 name,
                                 work.frame.renders,
                                 popup.probe->paints,
                                 work.frame.showPaintsValidated);
        Require(popup.probe->paints == 2u && work.frame.renders == 2u, "a change between the pre-show render and the show renders exactly once more");
        Require(work.frame.showPaintsValidated == 0u, "a changed popup's show paint is not skipped");
        RequirePacingKept(work, "the changed popup presents only with a credit");
    }
}

void TestPopupShowAgainAfterHideRendersAsBefore()
{
    PopupLikeWindow popup;
    Require(popup.Host().RenderInitialFrameForShow(), "the pre-show render presents");
    popup.Show();
    Require(popup.probe->paints == 1u, "the first show renders once");
    ShowWindow(popup.window.Hwnd(), SW_HIDE);
    Settle(popup.Host());
    const WorkCounterScope scope;
    popup.Show();
    PumpUntilReached([&] { return popup.probe->paints >= 2u; });
    Settle(popup.Host());
    const WorkCounters work = scope.Delta();
    Require(popup.probe->paints == 2u && work.frame.renders == 1u, "a window shown again later renders once, as before W2.4");
    Require(work.frame.showPaintsValidated == 0u, "only a first show of the frame rendered for it skips its paint");
    RequirePacingKept(work, "the second show");
}
