#pragma once
// FX6 Accessibility (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx6
{
inline constexpr std::array kBudgets{std::string_view("G9")};

inline constexpr Definition kDefinition{
    .id       = "FX6",
    .title    = "Accessibility",
    .scenario = "100 mutations with 0 and with 1 UI Automation client; a word walk over a 64 Ki-unit document; a Splitter drag of 200 px; 100 Slider steps. "
                "Proves G9 and the W8 gates.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx6
