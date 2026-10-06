#pragma once

// Something that runs while a host raises UI Automation selection events, as a test plays it, shared by the control and the
// embedded suites. In an application something can (an outgoing call of UI Automation's in a single-threaded apartment dispatches
// messages, and a message can hide or remove the control, or detach its host); here the library's diagnostics hook runs an action
// after the first event and counts every event raised while it is set, so a test asserts that the raising ended there.

#include "../../src/Controls/DxUi.Internal.h"

#include <functional>
#include <utility>

namespace UiaTest
{
class SelectionEventInterruption final
{
public:
    explicit SelectionEventInterruption(std::function<void()> action) : _action(std::move(action))
    {
        DxUi::DebugSetAccessibilitySelectionEventHookForTest(&SelectionEventInterruption::OnEvent, this);
    }

    SelectionEventInterruption(const SelectionEventInterruption&)            = delete;
    SelectionEventInterruption& operator=(const SelectionEventInterruption&) = delete;

    ~SelectionEventInterruption()
    {
        DxUi::DebugSetAccessibilitySelectionEventHookForTest(nullptr, nullptr);
    }

    // The events raised since the hook was set.
    [[nodiscard]] size_t Events() const noexcept
    {
        return _events;
    }

private:
    static void OnEvent(void* context) noexcept
    {
        auto* const self = static_cast<SelectionEventInterruption*>(context);
        if (++self->_events == 1u && self->_action)
            self->_action();
    }

    std::function<void()> _action;
    size_t _events = 0u;
};
} // namespace UiaTest
