#pragma once
// FX1 Idle caret (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx1
{
inline constexpr std::array kBudgets{std::string_view("G1"), std::string_view("G4")};

inline constexpr Definition kDefinition{
    .id       = "FX1",
    .title    = "Idle caret",
    .scenario = "A focused single-line TextField, editable and read-only, for 10 s; plus a focused 64 Ki-unit multiline TextField. Proves G1; with FX5, G4.",
    .budgets  = kBudgets,
};
} // namespace PerformanceFixtures::Fx1
