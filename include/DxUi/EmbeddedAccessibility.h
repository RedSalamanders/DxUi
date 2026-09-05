#pragma once
#include <Windows.h>

#include <UIAutomationCore.h>
#include <memory>

namespace DxUi
{
// Application adapter within one module. A plugin adapts its own COM site; no HWND or C++ ownership crosses an ABI.
// Callbacks run on the attaching COM STA, must not destroy/re-enter the control tree, and return owned COM references.
class EmbeddedAccessibilitySite
{
public:
    virtual ~EmbeddedAccessibilitySite()                                                                 = default;
    virtual HRESULT Navigate(NavigateDirection direction, IRawElementProviderFragment** result) noexcept = 0;
    virtual HRESULT FragmentRoot(IRawElementProviderFragmentRoot** result) noexcept                      = 0;
    // Request application/OS focus without changing the tree. Logical control focus is set by DxUi afterward.
    virtual HRESULT RequestFocus() noexcept = 0;
    // Called after a UIA edit/activation. Post/coalesce application work (frame, focus, committed navigation).
    // This callback must not synchronously mutate the tree or call UIA again.
    virtual void ActionCompleted() noexcept
    {
    }
};
struct EmbeddedAccessibilityPlacement
{
    // Physical screen pixels. The displayed viewport must have the same dimensions as the prepared view.
    UiaRect viewport{};
    bool hasKeyboardFocus = false;
};
} // namespace DxUi
