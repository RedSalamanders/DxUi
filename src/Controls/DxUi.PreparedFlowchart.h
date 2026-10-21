#pragma once
#include "DxUi.NativeFlowchartLayout.h"
#include <DxUi/RichTextLayout.h>

namespace DxUi::RichText::Detail
{
struct FlowchartResourceEpoch
{
    std::uint64_t typography{}, resources{};
    bool operator==(const FlowchartResourceEpoch&) const = default;
};
// Owner-serialized native diagram preparation. Factories are borrowed at Begin
// and retained by COM ownership; no target, host, device, HWND or worker is owned.
class PreparedFlowchart
{
public:
    PreparedFlowchart() noexcept;
    ~PreparedFlowchart() noexcept;
    PreparedFlowchart(const PreparedFlowchart&)            = delete;
    PreparedFlowchart& operator=(const PreparedFlowchart&) = delete;
    // Intrinsic diagram labels use fontFamily/fontSizeDip/locale/readingDirection.
    // Width, paragraph alignment and code font do not alter plain diagram labels.
    // Keep the owner's typography and resource epochs separate; combining them
    // arithmetically could let different invalidation pairs compare equal.
    LayoutPreparationStatus Begin(IDWriteFactory* write,
                                  ID2D1Factory* drawing,
                                  std::shared_ptr<const FlowchartModel> graph,
                                  const LayoutOptions& typography,
                                  FlowchartResourceEpoch resourceEpoch);
    // Every real CreateTextLayout consumes maximumLayouts, including empty node
    // labels. Geometry events and one bounded path creation consume block steps.
    // Zero allowances make no progress. Failure retires only staged work.
    LayoutAdvance Advance(LayoutWorkBudget budget = {});
    void Cancel() noexcept;
    void Invalidate() noexcept;
    void Clear() noexcept;
    bool Pending() const noexcept;
    bool Ready() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    std::shared_ptr<const FlowchartLayoutPlan> Geometry() const noexcept;
    // Raster preparation only; the caller owns BeginDraw/EndDraw, clipping and
    // brushes; fill must be opaque for node and edge-label backgrounds.
    // Submission restores the caller's transform and creates no C++/SDK
    // resources. A true result still requires the caller's successful EndDraw.
    bool Draw(ID2D1RenderTarget* target, ID2D1Brush* text, ID2D1Brush* stroke, ID2D1Brush* fill, D2D1_POINT_2F origin) const noexcept;

private:
    struct State;
    struct Preparation;
    std::unique_ptr<State> state_;
    std::unique_ptr<Preparation> next_;
    std::uint64_t generation_{};
    bool valid_{};
};
} // namespace DxUi::RichText::Detail
