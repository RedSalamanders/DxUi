#pragma once

// Performance fixtures on the WindowHost (plan W0.4; Tests/README.md, "Performance fixtures"): the framework's own fixture,
// the determinism gate for every catalog fixture whose hosts include the window host, and the window receipt command.
void TestPerformanceFixtureFrameworkOnWindowHost();
void TestPerformanceFixturesAreDeterministicOnWindowHost();
void TestFrameScheduleGatesOnWindowFixtures();
// DxUi.ControlTests.exe --fixture-receipt <FX<N>[@variant]> <output.json>: a WARP receipt of one fixture on the window host.
int RunWindowFixtureReceiptCommand(int argc, wchar_t** argv);

namespace PerformanceFixtures
{
struct Definition;
struct Variant;
struct RunRecord;
} // namespace PerformanceFixtures
// One WARP run of a fixture on the window host (counters per step), for an item's gate test (Tests/README.md, "A gate assertion").
[[nodiscard]] PerformanceFixtures::RunRecord RunWindowFixture(const PerformanceFixtures::Definition& definition, const PerformanceFixtures::Variant& variant);
