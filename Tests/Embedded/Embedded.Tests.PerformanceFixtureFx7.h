#pragma once
// FX7 IME composition (plan W0.4): declared from the plan, not built yet. Whoever builds it edits only this file: give the
// Definition its hosts, variants, phases, timed phase, sources and create function, and the Scenario that builds and
// steps the scene (Tests/README.md, "Performance fixtures"). The catalog already lists it.
#include "Embedded.Tests.PerformanceFixture.h"

namespace PerformanceFixtures::Fx7
{
inline constexpr Definition kDefinition{
    .id       = "FX7",
    .title    = "IME composition",
    .scenario = "A 10-syllable composition and commit in a 64 Ki-unit TextField and RichTextEditor. Proves the W8 gates.",
};
} // namespace PerformanceFixtures::Fx7
