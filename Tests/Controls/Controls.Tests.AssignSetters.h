#pragma once
// One way to assign a property (plan W2.10) on a window host: the rows of Support.Tests.AssignSetterRows.h on a WARP WindowHost
// of a non-activating test window, where invalidation and accessibility publication are separate (an embedded view also
// invalidates to publish), so a setter that stopped repainting or stopped publishing fails here. The embedded run is FX5's
// (Embedded.Tests.AssignSetters.h).
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support.Tests.AssignSetterRows.h"

inline void TestAssignSettersOnWindowHost()
{
    AssignSetterGate::g_require = &Require;
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto theme          = DxUi::MakeDefaultThemePalette(true);
    theme.reducedMotion = true;
    window.Host().SetTheme(theme);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    ComplexUiModel model;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* panel = root.get();
    AssignSetterGate::Scene rows;
    rows.root  = panel;
    rows.model = &model;
    for (DxUi::Slider*& slider : rows.sliders)
    {
        slider = panel->AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(8, 8, 296, 36));
    }
    rows.progress = panel->AddChild<DxUi::ProgressBar>();
    rows.progress->SetBounds(D2D1::RectF(8, 40, 296, 52));
    rows.group = panel->AddChild<DxUi::RadioButtons>();
    window.Host().SetRoot(std::move(root));
    Require(window.Host().GetRoot() == panel, "the gate's root is the window host's root");
    static_cast<void>(AssignSetterGate::RunRows(rows, panel, false, "window host"));
    window.Host().SetRoot(nullptr);
}

inline void TestAssignEqualityOnControlSuite()
{
    AssignSetterGate::g_require = &Require;
    AssignSetterGate::TestAssignEquality();
}

// F1 of the W2.10 review: a ScrollPanel re-clamps its offset when its viewport grows, whoever sets the content height after it.
// The documented usage (DxUi.h, ScrollPanel) sets the bounds, lays out, then sets the content height, which can be unchanged.
inline void TestScrollPanelResizeReclampsItsOffset()
{
    DxUi::ScrollPanel scroll;
    float reported = -1.0f;
    size_t reports = 0u;
    scroll.SetOnScrollChanged([&](float offset)
    {
        reported = offset;
        ++reports;
    });
    scroll.SetBounds(D2D1::RectF(0, 0, 200, 100));
    scroll.SetContentHeight(300.0f);
    scroll.SetScrollOffset(200.0f);
    Require(scroll.GetScrollOffset() == 200.0f && reports == 1u, "the offset scrolls to the end of a 300 DIP content in a 100 DIP viewport");
    scroll.SetBounds(D2D1::RectF(0, 0, 200, 250));
    Require(scroll.GetScrollOffset() == 50.0f, "a taller viewport re-clamps the offset to the new extent by itself");
    Require(reports == 2u && reported == 50.0f, "the re-clamped offset is reported");
    DxUi::Detail::WorkCounterScope work;
    scroll.SetContentHeight(300.0f);
    Require(scroll.GetScrollOffset() == 50.0f && reports == 2u && work.Delta().frame.invalidations == 0u, "an unchanged content height is no change");
    scroll.SetBounds(D2D1::RectF(0, 0, 200, 400));
    Require(scroll.GetScrollOffset() == 0.0f && reports == 3u, "a viewport taller than the content scrolls to the top");
    scroll.SetBounds(D2D1::RectF(0, 0, 300, 400));
    Require(reports == 3u, "a resize that leaves the offset in range reports nothing");
}
