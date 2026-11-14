#pragma once
// FX8 Rich typing (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx8
{
inline constexpr std::array kBudgets{std::string_view("G12")};

inline constexpr Definition kDefinition{
    .id       = "FX8",
    .title    = "Rich typing",
    .scenario = "A 64 Ki-unit document with 3 math blocks, 2 diagrams, 2 raster images, 1 SVG, a 200x5 table and a numbered code fence of 2,000 lines; 100 "
                "one-character inserts, 10 Enter presses, 50 arrow keys, a 30-step width resize. Proves G12.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx8
