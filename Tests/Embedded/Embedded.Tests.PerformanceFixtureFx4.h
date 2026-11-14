#pragma once
// FX4 Menus (plan W0.4): a 4,096-row MenuPopup opened, stepped 100 times with the keyboard and closed, an open/close cycle,
// and ten menu sessions, all in an embedded-only application: a thread whose only DxUi host is an EmbeddedHost on the
// application's device, with a plain Win32 owner window and no native WindowHost. Proves G10 (devices created per menu
// session) and records G3's per-step work on a menu.
//
// Interactive only (Definition::interactiveOnly; developer's decision of 14 November 2026). A MenuPopup session activates
// and focuses its root popup, takes mouse capture, is clamped onto a visible monitor and blurs real screen pixels; the
// testing contract ties menu measurement to real foreground interaction, and no non-activating seam exists. So this
// definition has no Scenario: its runner is Tests/Controls/Controls.Tests.PerformanceFixtureFx4.cpp in DxUi.ControlTests.exe,
// reached only through the Menu suite under test.ps1 -Interactive (the determinism gate) and performance-fixtures.ps1
// -Interactive (the receipt), both under the desktop lease. Every other runner, gate, receipt command and the timing harness
// skips it with "FX4: interactive, not run". The run sheet is Measurements/PerformanceAndMotion_2026-11-13/FX4/README.md.
//
// What each phase gives the gates that name FX4 (counters are those of every step; see the runner for how a step works):
// - open: popups.menuPopupPaints (renders per open; W2.4 "one render per menu open"), popups.backdropCaptures (W5.12),
//   resources.*.created and text layouts of an open;
// - keyboard: one VK_DOWN per step, a steady popup frame each: resources.effect / commandList / layer / geometry /
//   gradientStops / bitmap .created, popups.backdropBlurPasses and frame.shadowPasses (W2.4, W4.3, W4.4: 0 per steady
//   frame), popups.menuRowsPainted (W4.2: rows re-recorded per step), text layouts (G11), popups.backdropCaptures (W5.12);
// - close and cycle: what a close and a second open/close of the same menu cost;
// - sessions: ten whole sessions (open a 12-row menu, render, close), each after the first session of the run (the warm
//   phase): resources.d3d11Device.created, resources.deviceContext.created, frame.d2dDevices, frame.compositionDevices and
//   frame.swapChains per session (G10, W2.4: today 1 device per session; target 0).
// W10.1's clause ("FX4 menu runs on the same primitive") is structural: it names this scene, not a counter.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx4
{
inline constexpr size_t kRows          = 4'096u; // rows of the large menu
inline constexpr size_t kKeyboardSteps = 100u;
inline constexpr size_t kSessions      = 10u;
inline constexpr size_t kSessionRows   = 12u;    // rows of each session's menu
inline constexpr float kMaxHeightDip   = 360.0f; // the large menu's viewport: it scrolls
inline constexpr float kMinWidthDip    = 280.0f;

inline constexpr std::array kBudgets{std::string_view("G10"), std::string_view("G3")};
// The application's EmbeddedHost view. The popups take the DPI of the monitor they open on, which the receipt's run records.
inline constexpr std::array kVariants{Variant{"default", 640u, 360u, 96.0f}};

inline constexpr size_t kWarm     = 0u;
inline constexpr size_t kOpen     = 1u;
inline constexpr size_t kKeyboard = 2u;
inline constexpr size_t kClose    = 3u;
inline constexpr size_t kCycle    = 4u;
inline constexpr size_t kSession  = 5u;
inline constexpr std::array kPhases{
    Phase{"warm", 2u, PhaseKind::Warmup, "the application's first menu session: open the 4,096-row menu, then close it; first-use state settles"},
    Phase{"open", 1u, PhaseKind::Measured, "open the 4,096-row menu with its first row selected and pump until it has rendered"},
    Phase{"keyboard", kKeyboardSteps, PhaseKind::Measured, "one Down key to the open popup per step, then its render: a steady popup frame"},
    Phase{"close", 1u, PhaseKind::Measured, "Escape to the popup; pump until the session has ended and the popup is destroyed"},
    Phase{"cycle", 2u, PhaseKind::Measured, "the open/close cycle again: step 0 opens the 4,096-row menu, step 1 closes it"},
    Phase{"sessions",
          kSessions,
          PhaseKind::Measured,
          "ten whole menu sessions of the embedded-only application, one per step: open a 12-row menu, render, Escape, destroyed"},
};
inline constexpr std::array kSources{std::string_view("Tests/Embedded/Embedded.Tests.PerformanceFixtureFx4.h"),
                                     std::string_view("Tests/Controls/Controls.Tests.PerformanceFixtureFx4.cpp")};

inline constexpr Definition kDefinition{
    .id         = "FX4",
    .title      = "Menus",
    .scenario   = "A 4,096-row MenuPopup with 100 keyboard steps and an open/close cycle; ten menu sessions in an embedded-only application. Proves G10, G3.",
    .budgets    = kBudgets,
    .hosts      = kWindow,
    .variants   = kVariants,
    .phases     = kPhases,
    .timedPhase = "keyboard",
    .sources    = kSources,
    .interactiveOnly = true,
};
} // namespace PerformanceFixtures::Fx4
