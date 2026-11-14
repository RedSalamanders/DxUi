#pragma once
// FX2 ComboBox popup and typing (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx2
{
inline constexpr std::array kBudgets{std::string_view("G3"), std::string_view("G11")};

inline constexpr Definition kDefinition{
    .id       = "FX2",
    .title    = "ComboBox popup and typing",
    .scenario = "A ComboBox with 500 items, open with 60 hover moves and closed with 20 typed characters; plus a 100-item popup in which every row fits. "
                "Proves G3, G11.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx2
