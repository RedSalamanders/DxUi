#pragma once

// Performance fixture FX4 (plan W0.4; Tests/README.md, "Performance fixtures"): the runner of the interactive-only menu
// fixture defined in Tests/Embedded/Embedded.Tests.PerformanceFixtureFx4.h. It takes the foreground, so both entry points
// run only under the interactive desktop lease: the determinism gate in the Menu suite (test.ps1 -Interactive) and the
// receipt (performance-fixtures.ps1 -Interactive, which passes --interactive to --fixture-receipt).
#include <filesystem>

namespace PerformanceFixtures
{
struct Definition;
struct Variant;
} // namespace PerformanceFixtures

// Two consecutive runs of FX4 record equal counters in every measured phase. Registered in the Menu suite only.
void TestPerformanceFixtureFx4IsDeterministic();
// DxUi.ControlTests.exe --fixture-receipt <FX<N>[@variant]> <output.json> --interactive: one run of an interactive-only
// fixture, its receipt written only when the run was valid. 0 written, 2 no such interactive fixture, 3 the run was
// invalid (a blocked activation or the person's input reached the menu); the reason is on stderr and nothing is written.
int RunInteractiveFixtureReceipt(const PerformanceFixtures::Definition& definition,
                                 const PerformanceFixtures::Variant& variant,
                                 const std::filesystem::path& output);
