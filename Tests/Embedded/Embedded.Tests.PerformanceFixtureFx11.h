#pragma once
// FX11 Motion (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx11
{
inline constexpr std::array kBudgets{std::string_view("G14"), std::string_view("G7")};

inline constexpr Definition kDefinition{
    .id       = "FX11",
    .title    = "Motion",
    .scenario = "PageHost slide, Tree expand and collapse, toggle/checkbox/slider state changes, popup open and close, all with motion enabled, sampled at "
                "30/60/144/240 Hz. Proves G14, G7.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx11
