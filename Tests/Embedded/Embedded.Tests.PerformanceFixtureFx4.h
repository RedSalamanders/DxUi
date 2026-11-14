#pragma once
// FX4 Menus (plan W0.4): declared, not built in the ordinary suites. A MenuPopup session opens native popup windows that
// take the foreground and capture the pointer (the Menu, MenuResources and MenuResourceScaling suites run only under
// test.ps1 -Interactive), and an embedded-only application's menu session creates those windows too. Building this fixture
// needs an interactive receipt path: a 4,096-row MenuPopup with 100 keyboard steps and an open/close cycle, and ten menu
// sessions on a thread with no native host (G10: devices created per session). Whoever builds it edits only this file.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx4
{
inline constexpr std::array kBudgets{std::string_view("G10"), std::string_view("G3")};

inline constexpr Definition kDefinition{
    .id       = "FX4",
    .title    = "Menus",
    .scenario = "A 4,096-row MenuPopup with 100 keyboard steps and an open/close cycle; ten menu sessions in an embedded-only application. Proves G10, G3.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx4
