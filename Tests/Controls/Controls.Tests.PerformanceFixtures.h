#pragma once

#include <DxUi/DxUi.h>

#include <memory>

// Performance fixtures on the WindowHost (plan W0.4; Tests/README.md, "Performance fixtures"): the framework's own fixture,
// the determinism gate for every catalog fixture whose hosts include the window host, and the window receipt command.
void TestPerformanceFixtureFrameworkOnWindowHost();
void TestPerformanceFixturesAreDeterministicOnWindowHost();
void TestFrameScheduleGatesOnWindowFixtures();
// W2.8 (Specs/Core/Core_PerformanceAndResources.md, "Format and brush caches") on the window host.
void TestFormatBrushCacheGatesOnWindowFixtures();
void TestFormatBrushCachesAcrossDeviceLossAndDpiOnWindowHost();
// DxUi.ControlTests.exe --fixture-frame-hashes <FX<N>[@variant]> <output>: a hash of a full frame per step on the window host.
int RunWindowFixtureFrameHashCommand(int argc, wchar_t** argv);
void TestTooltipDeadlineGatesOnWindowFx14();
// DxUi.ControlTests.exe --fixture-receipt <FX<N>[@variant]> <output.json>: a WARP receipt of one fixture on the window host.
int RunWindowFixtureReceiptCommand(int argc, wchar_t** argv);
// DxUi.ControlTests.exe --present-attribution <output.json> <FX<N>[@variant]> Hardware|Warp <minFrames> <minMilliseconds>: the
// window-host present pass of the hardware attribution method (performance-attribution.ps1, plan W0.7).
int RunWindowPresentAttributionCommand(int argc, wchar_t** argv);

namespace PerformanceFixtures
{
struct Definition;
struct Variant;
struct RunRecord;
} // namespace PerformanceFixtures
// One WARP run of a fixture on the window host (counters per step), for an item's gate test (Tests/README.md, "A gate assertion").
[[nodiscard]] PerformanceFixtures::RunRecord RunWindowFixture(const PerformanceFixtures::Definition& definition, const PerformanceFixtures::Variant& variant);

// A fixture live on a WindowHost in a chosen swap-chain mode, WARP or the default hardware adapter, for a measurement that
// is not a fixture run (the presentation-mode comparison, W0.9): the same instance the runners above use (non-activating,
// off-screen window; its frame is the host's own WM_PAINT after a frame-latency credit), driven step by step.
class WindowFixtureSession
{
public:
    WindowFixtureSession()                                       = default;
    WindowFixtureSession(const WindowFixtureSession&)            = delete;
    WindowFixtureSession& operator=(const WindowFixtureSession&) = delete;
    WindowFixtureSession(WindowFixtureSession&&)                 = delete;
    WindowFixtureSession& operator=(WindowFixtureSession&&)      = delete;
    virtual ~WindowFixtureSession()                              = default;
    [[nodiscard]] virtual DxUi::WindowHost& Host() noexcept      = 0;
    [[nodiscard]] virtual HWND Hwnd() const noexcept             = 0;
    virtual void Step(size_t phase, size_t step)                 = 0; // takes a frame credit, then applies the step's input
    virtual void Frame()                                         = 0; // pumps the queue and paints what the host invalidated
    virtual void Resize(UINT width, UINT height, float dpi)      = 0;
};
[[nodiscard]] std::unique_ptr<WindowFixtureSession> OpenWindowFixtureSession(const PerformanceFixtures::Definition& definition,
                                                                             const PerformanceFixtures::Variant& variant,
                                                                             bool forceWarp,
                                                                             DxUi::WindowHost::PresentationMode mode);
