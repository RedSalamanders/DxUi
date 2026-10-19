#pragma once
#include <atomic>
#include <cstdint>

namespace DxUi::RichText::Detail
{
// Data-only liveness for immutable semantic accessibility snapshots. The owner
// increments it before source/presentation/binding retirement; foreign UIA reads
// never dereference the control. No control lifetime, host or SDK object is held.
struct AccessibilityEpoch
{
    std::atomic<std::uint64_t> revision{1u};
};
} // namespace DxUi::RichText::Detail
