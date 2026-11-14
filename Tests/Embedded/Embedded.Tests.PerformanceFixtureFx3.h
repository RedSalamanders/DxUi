#pragma once
// FX3 Grid scrolling, resizing and keys (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx3
{
inline constexpr std::array kBudgets{std::string_view("G3"), std::string_view("G8"), std::string_view("G13")};

inline constexpr Definition kDefinition{
    .id       = "FX3",
    .title    = "Grid scrolling, resizing and keys",
    .scenario = "A Grid of 2,048 rows: 100 wheel notches, 60 fractional touchpad deltas, 30 column-resize steps, 200 arrow keys; plus a grouped variant with "
                "10,000 groups and a 200,000-row select-all. Proves G3, G8, G13 (cells).",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx3
