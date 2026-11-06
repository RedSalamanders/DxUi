#include "Support/ControlAllocation.h"
#include <DxUi/ControlCatalog.h>
#include <DxUi/Diagnostics.h>
#include <DxUi/Failure.h>
#include <cstdint>
#include <memory>
#include <type_traits>

static_assert(std::is_nothrow_default_constructible_v<DxUi::Separator>);

bool TestCheckedControlFactory()
{
    std::unique_ptr<DxUi::Control> control;
    if (FAILED(DxUi::CreateControl(DxUi::ControlKind::Separator, control)) || ! control)
        return false;
    auto* const retained = control.get();
    for (const auto& descriptor : DxUi::GetControlCatalog())
    {
        // This is a null process-heap result at the actual factory storage seam,
        // rather than a caught bad_alloc from an STL container.
        DxUi::Detail::ControlAllocationFailureScope fail(0);
        if (DxUi::CreateControl(descriptor.kind, control) != E_OUTOFMEMORY || control.get() != retained)
            return false;
    }
    {
        DxUi::Detail::ControlAllocationFailureScope fail(0);
        std::unique_ptr<DxUi::Separator> refused(new (std::nothrow) DxUi::Separator);
        if (refused)
            return false;
    }
    if (FAILED(DxUi::CreateControl(DxUi::ControlKind::Separator, control)) || ! control || control.get() == retained)
        return false;

    // The explicit padding is the subject of this over-aligned allocation test.
#pragma warning(push)
#pragma warning(disable : 4324)
    class alignas(64) AlignedControl final : public DxUi::Control
    {
    public:
        AlignedControl()                                 = default;
        AlignedControl(const AlignedControl&)            = delete;
        AlignedControl& operator=(const AlignedControl&) = delete;
        AlignedControl(AlignedControl&&)                 = delete;
        AlignedControl& operator=(AlignedControl&&)      = delete;

    private:
        void Paint(DxUi::ControlHost&) const override
        {
        }
    };
#pragma warning(pop)
    const DxUi::Failure::Scope construction;
    auto aligned = std::make_unique<AlignedControl>();
    if (reinterpret_cast<uintptr_t>(aligned.get()) % alignof(AlignedControl))
        return false;
    std::unique_ptr<AlignedControl> nothrowAligned(new (std::nothrow) AlignedControl);
    return nothrowAligned && reinterpret_cast<uintptr_t>(nothrowAligned.get()) % alignof(AlignedControl) == 0;
}
