#pragma once
#include <DxUi/Configuration.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>
namespace DxUi::Detail
{
// What the diagnostics hooks report as live, so a test can assert that a closed window returned everything it held,
// whatever the renderer and the allocator keep.
enum class LiveResource : uint8_t
{
    MenuPopup,               // A context-menu popup: the root of a menu or one of its submenus.
    MenuRowLayout,           // A text layout held by a row of a described menu.
    MenuAccessibilityRecord, // A control record of a UI Automation snapshot published for a menu popup.
    Count,
};
#if DXUI_ENABLE_DIAGNOSTICS
// Owns units of one resource for as long as it lives: constructing or assigning it counts them, and destroying it or
// assigning another value returns them. Kept beside what it counts, the total cannot drift from the owners still alive.
class LiveResourceCount final
{
public:
    LiveResourceCount() noexcept = default;
    LiveResourceCount(LiveResource resource, size_t units) noexcept : _resource(resource), _units(units)
    {
        Counter(resource).fetch_add(units, std::memory_order_relaxed);
    }
    LiveResourceCount(const LiveResourceCount&)            = delete;
    LiveResourceCount& operator=(const LiveResourceCount&) = delete;
    LiveResourceCount(LiveResourceCount&& other) noexcept : _resource(other._resource), _units(std::exchange(other._units, 0u))
    {
    }
    LiveResourceCount& operator=(LiveResourceCount&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            _resource = other._resource;
            _units    = std::exchange(other._units, 0u);
        }
        return *this;
    }
    ~LiveResourceCount()
    {
        Release();
    }
    // The units of one resource that live owners hold now, in the whole process.
    [[nodiscard]] static size_t Live(LiveResource resource) noexcept
    {
        return Counter(resource).load(std::memory_order_relaxed);
    }

private:
    static std::atomic<size_t>& Counter(LiveResource resource) noexcept
    {
        static std::array<std::atomic<size_t>, static_cast<size_t>(LiveResource::Count)> counters{};
        return counters[static_cast<size_t>(resource)];
    }
    void Release() noexcept
    {
        if (_units != 0u)
            Counter(_resource).fetch_sub(std::exchange(_units, 0u), std::memory_order_relaxed);
    }
    LiveResource _resource = LiveResource::MenuPopup;
    size_t _units          = 0u;
};
#else
class LiveResourceCount final
{
public:
    LiveResourceCount() noexcept = default;
    LiveResourceCount(LiveResource, size_t) noexcept
    {
    }
    [[nodiscard]] static size_t Live(LiveResource) noexcept
    {
        return 0u;
    }
};
#endif
} // namespace DxUi::Detail
