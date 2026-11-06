#pragma once
#include <cstddef>
#include <limits>

// Private deterministic seam for the real process-heap allocation used by the
// control factory. It never makes a throwing STL allocator return null.
namespace DxUi::Detail
{
[[nodiscard]] size_t SetControlAllocationFailureCountdown(size_t count) noexcept;
class ControlAllocationFailureScope final
{
public:
    explicit ControlAllocationFailureScope(size_t count) noexcept : previous_(SetControlAllocationFailureCountdown(count))
    {
    }
    ~ControlAllocationFailureScope()
    {
        static_cast<void>(SetControlAllocationFailureCountdown(previous_));
    }
    ControlAllocationFailureScope(const ControlAllocationFailureScope&)            = delete;
    ControlAllocationFailureScope& operator=(const ControlAllocationFailureScope&) = delete;

private:
    size_t previous_;
};
} // namespace DxUi::Detail
