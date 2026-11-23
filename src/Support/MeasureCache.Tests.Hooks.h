#pragma once
#include <DxUi/DxUi.h>

namespace DxUi::Detail
{
// Private test seams for the measure cache (Specs/UI/UI_ControlsAndLayout.md, "Measure cache"). Declared only here.

// While enabled on the calling thread, a Debug build re-runs MeasureOverride on every measure-cache hit and counts a
// different answer in layout.measureCacheVerifyMismatches, catching a control whose measured inputs change without
// InvalidateMeasure. Off by default. Returns whether verification exists in this build (Debug and ASan Debug only).
bool SetMeasureCacheVerificationForTest(bool enabled) noexcept;

// One Control::Measure call with valid input: the control, its complete cache key and whether MeasureOverride ran.
struct MeasureProbe final
{
    const Control* control  = nullptr;
    const ControlHost* host = nullptr;
    uint64_t revision       = 0u;
    uint64_t typography     = 0u;
    float dpi               = 0.0f;
    FlowDirection flow      = FlowDirection::LeftToRight;
    Density density         = Density::Standard;
    LayoutConstraints constraints;
    bool executed = false;
};
using MeasureProbeObserver = void (*)(const MeasureProbe& probe, void* context) noexcept;
// Installs an observer of every measure probe on the calling thread (nullptr removes it) and returns the previous one.
// Tests use it to count repeated against unique keys per control; the observer must not touch any control.
MeasureProbeObserver SetMeasureProbeObserverForTest(MeasureProbeObserver observer, void* context, void** previousContext) noexcept;
} // namespace DxUi::Detail
