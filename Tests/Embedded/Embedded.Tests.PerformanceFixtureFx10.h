#pragma once
// FX10 Live resize (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx10
{
inline constexpr Definition kDefinition{
    .id       = "FX10",
    .title    = "Live resize",
    .scenario = "A WindowHost swept from 800x600 to 3840x2160 in 30 steps. Proves the W6 gates.",
};
} // namespace PerformanceFixtures::Fx10
