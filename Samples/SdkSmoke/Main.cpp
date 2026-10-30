#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>
#include <DxUi/FrameRuntime.h>

int main()
{
    DxUi::FrameClock clock;
    const auto now = clock.Now();
    if (clock.ElapsedUs(now, now) != 0 || clock.SmoothDeltaUs(16667, DxUi::FrameBudget{}) == 0)
    {
        return 1;
    }
    auto stage = DxUi::FrameStage::Idle;
    {
        DxUi::FrameStageScope scope(stage, DxUi::FrameStage::Layout);
        if (stage != DxUi::FrameStage::Layout)
        {
            return 2;
        }
    }
    return stage == DxUi::FrameStage::Idle ? 0 : 3;
}
