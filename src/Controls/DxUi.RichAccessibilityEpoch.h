#pragma once
#include <atomic>
#include <cstdint>

namespace DxUi::RichText::Detail
{
// Data-only liveness for immutable semantic accessibility snapshots. The owner
// increments it before source/presentation/binding retirement; foreign UIA reads
// never dereference the control. No control lifetime, host or SDK object is held.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
struct AccessibilityEpoch
{
    std::atomic<std::uint64_t> revision{1u};
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
