#pragma once
// FX9 Large-list scroll (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx9
{
inline constexpr std::array kBudgets{std::string_view("G6"), std::string_view("G15"), std::string_view("G13")};

inline constexpr Definition kDefinition{
    .id    = "FX9",
    .title = "Large-list scroll",
    .scenario =
        "100,000-row and 1,000,000-row Grid, and a 10,000-row Tree, scrolled continuously for 10 s. Proves G6, G15 and row-count-independent memory (G13).",
    .budgets = kBudgets,
};
} // namespace PerformanceFixtures::Fx9
