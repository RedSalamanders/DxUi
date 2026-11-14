#pragma once
// FX13 Whole application (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx13
{
inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G13")};

inline constexpr Definition kDefinition{
    .id       = "FX13",
    .title    = "Whole application",
    .scenario = "60 s idle; cold start to first presented frame. Proves G1 (nothing armed) and G13 (idle and minimized); records cold start.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx13
