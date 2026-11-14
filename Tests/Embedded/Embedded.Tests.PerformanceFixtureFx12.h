#pragma once
// FX12 Dense screen (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx12
{
inline constexpr std::array kBudgets{std::string_view("G5"), std::string_view("G15"), std::string_view("G13"), std::string_view("G2")};

inline constexpr Definition kDefinition{
    .id       = "FX12",
    .title    = "Dense screen",
    .scenario = "2,000 controls at 3840x2160 and 192 DPI; a 10,000-control form; and a variant using 120 distinct text-format keys. Proves G5, G15, G13 (per "
                "control), G2.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx12
