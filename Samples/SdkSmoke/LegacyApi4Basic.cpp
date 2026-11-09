#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>
#include <DxUi/FrameRuntime.h>

// The 29 October API-4 source predates RichText and its source-owned SDK sample.
// This noninteractive recipe links the original frame runtime and checks its public state contract.
int main()
{
    DxUi::FrameClock clock;
    const auto now = clock.Now();
    if (now.qpc <= 0 || clock.ElapsedUs(now, now) != 0 || clock.SmoothDeltaUs(16667, DxUi::FrameBudget{}) == 0)
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
