#pragma once
#include "DxUi.NativeColorPalette.h"
#include "DxUi.NativeFlowchartLayout.h"
#include <DxUi/RichTextLayout.h>
#include <memory>

namespace DxUi::RichText::Detail
{
struct FlowchartResourceEpoch
{
    std::uint64_t typography{}, resources{};
    bool operator==(const FlowchartResourceEpoch&) const = default;
};
struct DiagramColorContext
{
    D2D1_COLOR_F text{}, stroke{}, fill{};
};
// Preparation-only choice of an opaque arrow-label surface. Keep the canvas
// color at readable contrast; otherwise use the smallest neutral adjustment.
D2D1_COLOR_F ResolveDiagramLabelBackground(D2D1_COLOR_F ink, D2D1_COLOR_F surface) noexcept;
// Target-bound resources are staged independently from the reusable paths and
// layouts. A palette is published only after every requested brush is ready.
struct DiagramPalette
{
    DiagramPalette(std::shared_ptr<const FlowchartModel> graph, ID2D1RenderTarget* target);
    DiagramPalette(const DiagramPalette&)                = delete;
    DiagramPalette& operator=(const DiagramPalette&)     = delete;
    DiagramPalette(DiagramPalette&&) noexcept            = default;
    DiagramPalette& operator=(DiagramPalette&&) noexcept = default;
    bool Advance();
    HRESULT Result() const noexcept
    {
        return palette ? palette->Result() : E_INVALIDARG;
    }
    bool Ready() const noexcept
    {
        return ready;
    }
    bool Matches(const FlowchartModel* value, ID2D1RenderTarget* raster) const noexcept
    {
        return graph.get() == value && target.get() == raster;
    }
    static bool Needed(const FlowchartModel& graph) noexcept;
    std::shared_ptr<const FlowchartModel> graph;
    wil::com_ptr<ID2D1RenderTarget> target;
    std::vector<wil::com_ptr<ID2D1SolidColorBrush>> fills, strokes, texts, edgeStrokes;
    std::unique_ptr<NativeColorPalette> palette;
    std::size_t cursor{};
    bool ready{};
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
    HRESULT Result() const noexcept
    {
        return result_;
    }
    std::uint64_t Generation() const noexcept;
    std::uint64_t Revision() const noexcept;
    std::shared_ptr<const FlowchartLayoutPlan> Geometry() const noexcept;
    // Raster preparation only; the caller owns BeginDraw/EndDraw, clipping and
    // brushes; fill and optional labelFill must be opaque. labelFill matches
    // the canvas; notes and labels inside filled groups retain their own fill.
    // Submission restores the caller's transform and creates no C++/SDK
    // resources. A true result still requires the caller's successful EndDraw.
    bool Draw(ID2D1RenderTarget* target,
              ID2D1Brush* text,
              ID2D1Brush* stroke,
              ID2D1Brush* fill,
              D2D1_POINT_2F origin,
              const DiagramPalette* palette     = nullptr,
              const DiagramColorContext* colors = nullptr,
              ID2D1Brush* labelFill             = nullptr) const noexcept;

private:
    struct State;
    struct Preparation;
    std::unique_ptr<State> state_;
    std::unique_ptr<Preparation> next_;
    std::uint64_t generation_{};
    bool valid_{};
    HRESULT result_ = S_OK;
};
} // namespace DxUi::RichText::Detail
