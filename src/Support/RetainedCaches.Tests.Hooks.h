#pragma once
#include <cstdint>
namespace DxUi
{
class Control;
}
namespace DxUi::Detail
{
// Called with every control whose Paint a Panel dispatches, while set (W4.10's gate names the controls a frame paints);
// nullptr clears it. UI thread only; it must not change the tree.
using PaintObserverForTest = void (*)(const Control& control, void* context) noexcept;
void SetPaintObserverForTest(PaintObserverForTest observer, void* context) noexcept;
[[nodiscard]] PaintObserverForTest GetPaintObserverForTest(void*& context) noexcept;
// Called each time a host's paint region becomes the whole view, while set (W4.10 diagnostics: a test can say why).
void SetFullRegionObserverForTest(void (*observer)() noexcept) noexcept;
// Private deterministic test seam for the incremental-versus-fresh oracle's negative control: while enabled on the calling
// thread, Label::SetText keeps the label's retained layout current, so an incremental frame paints the old text and a
// fresh one (after DebugDropRetainedStateForTest) the new text. Declared only here, never in include/DxUi.
void SetStaleLabelLayoutInjectionForTest(bool enabled) noexcept;
// While enabled on the calling thread, preparation that would reuse a previous publication (block-incremental semantic
// projection) builds from scratch: the oracle's fresh scene sets it around its own steps and captures.
void SetFreshPreparationForTest(bool enabled) noexcept;
// Paint regions and culling (W4.10), on by default, process-wide and present in every configuration: off, every invalidation
// repaints the whole view and no child is culled, as before W4.10. The same-binary comparison of performance-timing.ps1
// (-CandidateEnvironment DXUI_PAINT_REGIONS=0, read by the test executables) and the oracle's reference use it. Set it on the UI
// thread before attaching hosts, not while a frame paints.
void SetPaintRegionsEnabledForTest(bool enabled) noexcept;
// The frame verifier of paint regions and culling (W4.10), dormant until a test program turns it on (the Debug test programs
// do, for every suite). While on, every embedded preparation that painted a region, culled a child or skipped a clip paints
// the same scene again, at the same time, whole and with regions and culling off, into a scratch surface it creates and
// releases on the spot, and compares the two exactly. The second paint changes nothing a test can see: work counters, the
// host's retained caches and their ages, the paint observer, ink escape, the paint region and allocation counts (through
// verificationScopeObserver) are as the first left them. A mismatch is counted, described (count, bounding box, first pixel)
// and passed to the observer, which a test program uses to fail the running test.
using PaintRegionMismatchObserver = void (*)(const char* description) noexcept;
void SetPaintRegionVerificationForTest(bool verify) noexcept;
[[nodiscard]] bool GetPaintRegionVerificationForTest() noexcept;
// Totals since the process started, never reset: what a test program verified and how many differed.
void GetPaintRegionVerificationTotalsForTest(uint64_t& verified, uint64_t& mismatches) noexcept;
void SetPaintRegionMismatchObserverForTest(PaintRegionMismatchObserver observer) noexcept;
} // namespace DxUi::Detail
