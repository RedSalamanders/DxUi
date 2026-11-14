#pragma once
// FX4 Menus (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
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
