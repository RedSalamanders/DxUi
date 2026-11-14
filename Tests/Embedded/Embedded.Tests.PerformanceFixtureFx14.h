#pragma once
// FX14 Tooltip path (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx14
{
inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G2")};

inline constexpr Definition kDefinition{
    .id       = "FX14",
    .title    = "Tooltip path",
    .scenario = "The pointer moving inside one tooltip-bearing control during the hover delay and while shown. Proves G1, G2.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx14
